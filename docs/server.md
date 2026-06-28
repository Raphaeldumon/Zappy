# Le serveur Zappy

Document de référence du binaire `zappy_server`. Décrit ce que le serveur
fait réellement, à partir du code source (`server/`).

## 1. Rôle

Le serveur tient l'unique source de vérité du jeu : la carte, les drones, les
œufs, les équipes et le temps. Les IA et la GUI s'y connectent par TCP ; le
serveur arbitre, fait avancer l'horloge et pousse les évènements.

- Langage : **C++20**, un seul processus, un seul thread.
- Multiplexage par **`poll(2)`** uniquement — pas d'attente active, pas de lib
  réseau tierce.
- Lancement :

  ```
  ./zappy_server -p port -x width -y height -n team1 team2 ... -c clientsNb -f freq
  ```

  `-f` est la **fréquence** : l'inverse de l'unité de temps. Plus `f` est grand,
  plus le jeu va vite.

## 2. Découpage en couches

Le code est rangé par dépendance, du pur vers l'I/O. `core/` ne connaît ni les
sockets ni le protocole : il reste testable en isolation.

| Couche | Dossier | Responsabilité |
|--------|---------|----------------|
| Logique | `server/core/` | État du monde, règles, horloge évènementielle. Zéro I/O. |
| Réseau | `server/net/` | `poll(2)`, accept, buffers recv/send par client. |
| Protocole | `server/protocol/` | Décodage/encodage du fil (handshake, AI, GUI). |
| Orchestration | `server/runtime/` | `main`, parsing args, boucle, handshake, pipeline de commandes. |
| Tests | `server/tests/` | Tests unitaires `core/` + `protocol/`. |

Le format exact des messages AI et GUI vit dans [`protocols.md`](protocols.md).
Ce document-ci traite l'architecture, pas la grammaire du fil.

## 3. Cycle de vie

`main.cpp` parse les arguments puis construit un `Server` et appelle `run()`.
La boucle (`server/runtime/server.cpp`) est volontairement courte :

```cpp
while (running_) {
    scheduler_.advance_to(now_ticks());   // exécuter ce qui est dû
    net_.poll_once(ms_until_next_event()); // dormir jusqu'au prochain paquet OU event
    announce_win_if_over();
}
```

Trois temps par tour :

1. **Rattraper le temps** — `advance_to` exécute tous les évènements échus.
2. **Dormir intelligemment** — `poll()` se réveille soit sur un paquet réseau,
   soit à l'échéance du prochain évènement (`ms_until_next_event()`). Aucun
   busy-wait. Si l'agenda est vide, timeout de secours de 20 ms.
3. **Vérifier la victoire** — une équipe avec 6 joueurs niveau 8 déclenche `seg`
   et stoppe la boucle.

`now_ticks()` convertit le temps écoulé (`steady_clock`) en ticks via `f` :
`ticks = nanosecondes × f / 1e9`. Tout coût d'action s'exprime donc en `cost/f`
secondes.

## 4. Le temps : `EventScheduler`

`server/core/event_scheduler.hpp` — file à priorité ordonnée par `(tick, seq)`.

- Le `seq` (ordre d'insertion) casse les égalités de tick → exécution **FIFO
  déterministe**, donc rejouable.
- `schedule(at_tick, cb)` renvoie un id ; `cancel(id)` annule un évènement encore
  en attente (utile à la mort d'un drone ou à une déconnexion).
- `advance_to(now)` dépile tout ce dont `tick <= now`.
- `next_event_tick()` alimente le timeout de `poll()`.

Tout passe par là : consommation de nourriture, fin d'incantation, éclosion d'un
fork, respawn des ressources. Le serveur ne « tourne » jamais à vide — il dort
entre deux échéances.

### Coûts des actions (en `1/f`)

Définis dans `protocol/.../ai_protocol.hpp::time_cost` :

| Action | Coût |
|--------|------|
| `Inventory` | 1 |
| Forward, Right, Left, Look, Broadcast, Eject, Take, Set | 7 |
| `Fork` | 42 |
| `Incantation` | 300 |
| `Connect_nbr` | 0 |

Respawn des ressources : évènement récurrent toutes les **20** unités de temps
qui se re-planifie lui-même.

## 5. Le monde : `WorldState`

`server/core/world_state.{hpp,cpp}` — l'état pur, sans I/O.

- Carte = tableau 1D de `Tile`, indexé `idx = y*W + x`, avec **wrap toroïdal**
  sur les deux axes.
- Conteneurs : `unordered_map<PlayerId, Player>`, `vector<Egg>`, `vector<Team>`.
- Un `Player` porte position, orientation (`North/East/South/West`), niveau,
  inventaire (`ResourceSet` de 7 entiers), points de vie, et les id d'évènements
  à annuler (`food_event_id`, `cmd_event_id`).
- Actions : `move_forward`, `turn_left/right`, `take_object`, `set_object`,
  `eject`, `add_egg`, `look`, `respawn_resources`, `check_win`.
- `look` renvoie un cône en pyramide : niveau de vision `n` voit `2n+1` cases de
  large à la rangée `n`.

### Règles : `game_rules.hpp`

Constantes et tables hors de la logique d'état :

- `MAX_LEVEL = 8`, `STARTING_FOOD = 10`, `LIFE_UNITS_PER_FOOD = 126` (chaque
  ration de nourriture = 126 unités de temps de vie).
- `ELEVATION_TABLE` : pour chaque palier 1→8, nombre de joueurs requis et coût en
  pierres (linemate…thystame).
- `can_elevate(world, x, y, level)` vérifie *à la fois* le nombre de joueurs du
  bon niveau sur la case **et** les pierres présentes au sol.
- `consume_elevation_stones(...)` retire les pierres, à n'appeler qu'après un
  `can_elevate` positif.

## 6. Réseau : `NetworkLayer`

`server/net/network_layer.{hpp,cpp}` — un seul `poll(2)`, un seul socket
d'écoute partagé AI + GUI.

- API par callbacks : `on_connect`, `on_line`, `on_disconnect`. Le `Server` les
  branche dans son constructeur ; `NetworkLayer` ne connaît rien du jeu.
- `on_line` est appelé **ligne par ligne** (terminateur `\n`) : le buffering du
  recv est géré dans `client.cpp`.
- Émission : `send_to(fd, …)`, `broadcast_gui(…)`, `broadcast_all(…)`. Tout est
  bufferisé par client ; `POLLOUT` n'est armé que s'il reste des données à
  écrire.
- `pollfds_` reconstruit paresseusement via `pollfds_dirty_`.
- `close_client(fd)` marque mort ; le balayage (`sweep_dead_clients`) se fait en
  fin de `poll_once`, jamais en plein milieu d'une itération.

### Plafonds (`runtime/limits.hpp`)

Garde-fous contre l'épuisement de descripteurs de fichiers (1 client = 1 fd) :

```
MAX_CLIENTS_PER_TEAM = 100
MAX_TEAMS            = 8
MAX_GUI_CLIENTS      = 6
MAX_TOTAL_CLIENTS    = 100*8 + 6 = 806
```

Au-delà du total, le fd fraîchement accepté est refermé aussitôt.

## 7. Connexion : handshake et routage

Tout client arrive par le même flux. Le serveur envoie d'abord `WELCOME`, puis le
client annonce son nom d'équipe. `protocol/handshake.cpp::handle_handshake`
décide :

- nom `== "GRAPHIC"` → **client GUI** (graphique, authentifié).
- nom correspondant à une équipe enregistrée → **client AI** (drone).
- sinon → connexion invalide.

`GRAPHIC` est réservé : `parse_args` refuse une équipe ainsi nommée, comme il
refuse les doublons et les noms vides.

À l'ouverture d'une GUI, le serveur pousse un **snapshot complet** (taille de
carte, temps, noms d'équipes, contenu de chaque case, chaque joueur, chaque œuf).
Ensuite il n'envoie plus que des évènements granulaires à chaque changement —
pas de re-broadcast global.

## 8. Pipeline des commandes AI

Suivi dans `server/runtime/server_commands.cpp` :

1. `enqueue_ai_command` empile la ligne dans la file du client.
   File **bornée à 10** (`MAX_COMMAND_QUEUE`) ; au-delà la commande est jetée.
2. `execute_next_command` dépile la tête.
   - Commande invalide → réponse `ko`, **sans coût en temps**, on enchaîne tout
     de suite la suivante.
   - Commande valide → planifiée à `now_ticks() + time_cost(cmd)`.
3. À l'échéance, le callback applique l'effet sur `WorldState`, répond à l'IA,
   pousse l'évènement GUI correspondant, puis relance `execute_next_command`.

L'incantation est un cas à part : démarrée, elle gèle le drone, planifie une fin
à +300/f, puis **réévalue** les conditions à l'expiration (les pierres ou les
joueurs ont pu disparaître entre-temps).

## 9. Mort, fork, victoire

- **Faim** : chaque drone planifie une consommation à +126 unités. À l'échéance,
  il décrémente sa nourriture et re-planifie ; à zéro, `kill_player`.
- **Fork** : à +42/f, dépose un `Egg` sur la case et libère un slot d'équipe.
- **Victoire** : `check_win` détecte 6 joueurs niveau 8 d'une même équipe →
  `seg <team>` en GUI, `running_ = false`.

## 10. Tests

Unitaires via CTest (`make tests_run`) — chaque binaire `core/`/`protocol/` est
un `main()` autonome :

- `test_world_state` — wrap, mouvements, turns, take/set, eject, look, victoire.
- `test_event_scheduler` — ordering, cancel, précision des ticks.
- `test_game_rules` — table d'élévation, direction de broadcast.
- `test_protocol_ai` / `test_protocol_gui` — parsing et émission.
- `test_handshake` — routage GRAPHIC / équipe connue / inconnue.
- `test_network` — buffering des lignes.
- `test_parse_args` — options requises, refus GRAPHIC/doublon/vide, plafonds.

Le pipeline réseau + commandes (`server.cpp`, `server_commands.cpp`,
`network_layer.cpp`), hors de portée des unitaires, est couvert par les tests
d'intégration (`tools/run_integration.py`, label ctest `integration`) : vrai
serveur + faux drones + capture GUI.

## 11. Signaux

`SIGINT` / `SIGTERM` basculent `running_` à false ; la boucle sort et ferme les
sockets proprement. `SA_RESTART` est désactivé pour que `poll()` rende `EINTR`
immédiatement au signal plutôt que de reprendre son attente.
