# 02 — Architecture serveur

## Contraintes sujet (rappel)

- Binary `zappy_server`, **C/C++/Rust** → choisi : **C++20**
- **Single process, single thread**
- **poll(2)** obligatoire (le sujet précise "must use poll" et déconseille le busy-waiting)
- Usage : `./zappy_server -p port -x width -y height -n team1 team2 ... -c clientsNb -f freq`
- Densités ressources fixées (`food=0.5, linemate=0.3, deraumere=0.15, sibur=0.1, mendiane=0.1, phiras=0.08, thystame=0.05`)
- Respawn ressources toutes les 20 unités de temps
- Client `GRAPHIC` = GUI authentifié

## Vue d'ensemble

```
                      ┌─────────────────────────────────────┐
                      │     main(int argc, char **argv)     │
                      │   - parse_args (parse_args.cpp)     │
                      │   - signal handler (SIGINT/SIGTERM) │
                      │   - Server::run()                   │
                      └──────────────┬──────────────────────┘
                                     │
                  ┌──────────────────▼──────────────────┐
                  │           class Server              │
                  │  ┌──────────────┐ ┌───────────────┐ │
                  │  │ NetworkLayer │ │   WorldState  │ │
                  │  │  - poll(2)   │ │  - tiles      │ │
                  │  │  - clients   │ │  - players    │ │
                  │  │              │ │  - eggs/teams │ │
                  │  └──────┬───────┘ └───────┬───────┘ │
                  │  ┌──────▼─────────────────▼───────┐ │
                  │  │       EventScheduler           │ │
                  │  │  - priority queue (tick, seq)  │ │
                  │  │  - tick dérivé de 1/f          │ │
                  │  │  - resource respawn every 20   │ │
                  │  │  - incantation timer 300/f     │ │
                  │  └──────────────┬─────────────────┘ │
                  │  ┌──────────────▼─────────────────┐ │
                  │  │   protocol/ (parse + emit)      │ │
                  │  │   - handshake                   │ │
                  │  │   - ai_handler (AI parser)      │ │
                  │  │   - gui_handler (GUI requests)  │ │
                  │  │   - gui_emitter (GUI messages)  │ │
                  │  └────────────────────────────────┘ │
                  └─────────────────────────────────────┘
```

## Layout des sources

```
server/
├── CMakeLists.txt
├── core/                       ← logique pure, zéro I/O (unit-testable)
│   ├── types.hpp               ← Player, Tile, Egg, Team, Orientation, ResourceSet
│   ├── world_state.{hpp,cpp}   ← état du monde + règles de jeu
│   ├── world_state_events.hpp
│   ├── game_rules.hpp          ← table d'élévation, coûts, constantes
│   └── event_scheduler.{hpp,cpp}
├── net/                        ← I/O réseau
│   ├── network_layer.{hpp,cpp} ← poll(2), accept, buffers
│   └── client.{hpp,cpp}        ← état + buffers recv/send par client
├── protocol/                   ← encodage/décodage du fil
│   ├── handshake.{hpp,cpp}
│   ├── ai_handler.{hpp,cpp}    ← parse des commandes AI
│   ├── gui_handler.{hpp,cpp}   ← parse des requêtes GUI
│   └── gui_emitter.{hpp,cpp}   ← fabrique des messages GUI
├── runtime/                    ← orchestration
│   ├── main.cpp
│   ├── parse_args.{hpp,cpp}
│   ├── limits.hpp              ← plafonds (teams, clients, GUIs)
│   ├── server.{hpp,cpp}        ← boucle + handshake + cycle de vie
│   └── server_commands.cpp     ← pipeline de commandes AI + dispatch GUI
└── tests/
    ├── test_world_state.cpp
    ├── test_event_scheduler.cpp
    ├── test_game_rules.cpp
    ├── test_protocol_ai.cpp
    ├── test_protocol_gui.cpp
    ├── test_handshake.cpp
    ├── test_network.cpp
    └── test_parse_args.cpp
```

Le découpage `core/` (logique pure, sans réseau) garde le jeu unit-testable et
réutilisable par un futur binding simulateur.

## Modules en détail

### `core/world_state`
- Tableau 1D de `Tile`, indexation `(x,y) -> idx = y*W + x` avec wrap toroïdal
- `std::unordered_map<PlayerId, Player>`, `std::vector<Egg>`, `std::vector<Team>`
- Méthodes : `move_forward`, `turn_left/right`, `take_object`, `set_object`, `eject`,
  `add_egg`/`hatch_egg`, `look` (cône de vision), `respawn_resources`, `check_win`
- Vision = pyramide en avant : niveau 1 → 1+3 cases, niveau 2 → 1+3+5, etc.
- Wrap toroïdal via `wrap_x`/`wrap_y` (modulo positif), exposé par `Tile& at(x, y)`
- Élévation : `game_rules.hpp::can_elevate` + `consume_elevation_stones`

### `core/event_scheduler`
- `priority_queue<Event>` ordonné par `(tick, seq)` — le `seq` casse les égalités
  pour un ordre FIFO déterministe (rejouabilité)
- Chaque action planifie un event à `now_ticks() + cost`
- `advance_to(now)` consomme tous les events dont `tick <= now`
- Annulation paresseuse via une liste de tombstones (`cancel(id)`)
- Respawn ressources : event récurrent toutes les 20 unités (se re-planifie)
- Incantation : event à 300/f qui réévalue les conditions à l'expiration
- `fork` : event 42/f → ajoute un egg sur la tile

### `net/network_layer`
- **poll(2)** direct (pas de lib réseau externe), single-thread
- Boucle principale (`Server::run`) :
  ```cpp
  while (running_) {
      scheduler_.advance_to(now_ticks());
      net_.poll_once(ms_until_next_event());
      announce_win_if_over();
  }
  ```
- Active polling **interdit** : `poll()` dort jusqu'au prochain paquet **ou** le
  prochain event dû (timeout = `ms_until_next_event()`)
- 1 socket d'écoute pour AI/GUI (port `-p`)
- `pollfds` reconstruits paresseusement (flag `pollfds_dirty_`)
- Écritures bufferisées par client, POLLOUT armé seulement si données en attente
- Plafonds dans `runtime/limits.hpp` (teams, clients par équipe, GUIs, total)

### `protocol/ai_handler`
- Parser ligne par ligne (terminateur `\n`)
- Table de mapping `{"Forward", Command::Forward}, ...`
- Commande inconnue/malformée → `nullopt` → le serveur répond `"ko\n"`
- File de commandes par client (max 10, conformité sujet) ; le serveur dépile au
  rythme du `event_scheduler`
- Handshake (`handshake.cpp`) :
  ```
  S -> "WELCOME\n"
  C -> "TEAM_NAME\n"
  S -> "CLIENT_NUM\n"  (nb slots libres)
  S -> "X Y\n"         (dimensions)
  ```
- Cas spécial `TEAM_NAME == "GRAPHIC"` → bascule sur le protocole GUI
- `GRAPHIC` est réservé : `parse_args` refuse une équipe nommée `GRAPHIC`
  (ainsi que les doublons et les noms vides)

### `protocol/gui_handler` + `gui_emitter`
- Voir [`06_protocols.md`](06_protocols.md) pour la table exhaustive
- `gui_emitter` : fabriques statiques des messages (`msz`, `bct`, `pnw`, `ppo`, …)
- `gui_handler` : parse les requêtes GUI entrantes (`msz`, `mct`, `bct`, `ppo`,
  `plv`, `pin`, `sgt`, `sst`) ; tag inconnu → `suc`, paramètre invalide → `sbp`
- Stratégie push : le serveur émet l'événement granulaire à chaque changement,
  pas de re-broadcast complet de l'état
- À la connexion GUI : snapshot complet (`msz`, `sgt`, `tna`, `bct` × W·H,
  `pnw` par player, `enw` par egg)

## Tests

Lancés via CTest (`make tests_run`). Tous les binaires `core/`/`protocol/` sont
des `main()` autonomes avec `assert` (migration Catch2 prévue, ADR-006).

- `test_world_state.cpp` : wrap, mouvement (4 orientations + wrap), turns,
  take/set, eject, look, slots d'équipe, `check_win`
- `test_event_scheduler.cpp` : ordering, cancel, précision des ticks
- `test_game_rules.cpp` : table d'élévation, `broadcast_direction`
- `test_protocol_ai.cpp` : parsing, commandes invalides, file max 10
- `test_protocol_gui.cpp` : émission GUI, conformité du format
- `test_handshake.cpp` : routage GRAPHIC→GUI / équipe connue→AI / inconnue→Invalid
- `test_network.cpp` : buffering des lignes côté client
- `test_parse_args.cpp` : options requises, GRAPHIC/doublon/vide refusés, plafonds

Tests d'intégration (`tools/run_integration.py`, label ctest `integration`) :
lancent un vrai serveur + drones fake-AI + capture GUI, et vérifient
no_crashes / handshake GUI / acks des commandes / spawns / absence de drift
protocole. Couvre `server_commands.cpp` / `server.cpp` / `network_layer.cpp`
que les tests unitaires ne peuvent pas atteindre.

## Couverture

`make coverage` (gcovr) : résumé propre, tests exclus du %. La logique pure
(`core/` + `protocol/`) est bien couverte ; le pipeline réseau/commandes l'est
via les tests d'intégration.

## Signaux

- `SIGINT` / `SIGTERM` : arrêt propre — le handler bascule `running_` à false,
  la boucle sort et ferme les sockets. `SA_RESTART` désactivé pour que `poll()`
  retourne `EINTR` immédiatement à la réception du signal.
