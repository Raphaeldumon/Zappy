# IA Zappy — Workflow complet & conception

Parcours de bout en bout du bot IA Zappy
([`ai/baseline/zappy_ai_baseline.py`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py), recopié vers
le `ai/zappy_ai` gitignoré). C'est une **machine à états 100 % à base de règles** —
aucun entraînement, aucun réseau de neurones. Un processus pilote un drone ; une
équipe, c'est plusieurs de ces processus.

---

## 1. L'objectif & la condition de victoire

Chaque drone gravit les **niveaux d'élévation 1 → 8**. La partie s'arrête dès que
**6 joueurs d'une même équipe atteignent le niveau 8** (`world_state.cpp`
`check_win` : `count >= 6`) — le serveur émet `seg`, passe `running_ = false`, et
le processus se termine. Gagner est donc une **course** : amener six drones au
niveau 8 avant tout le monde.

Chaque élévation (`Incantation`) exige **N joueurs du même niveau + un jeu précis
de pierres sur la tuile** ([`REQ`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L19)) :

| Niveau → | joueurs | linemate | deraumere | sibur | mendiane | phiras | thystame |
|----------|---------|----------|-----------|-------|----------|--------|----------|
| 1→2 | 1 | 1 | – | – | – | – | – |
| 2→3 | 2 | 1 | 1 | 1 | – | – | – |
| 3→4 | 2 | 2 | – | 1 | – | 2 | – |
| 4→5 | 4 | 1 | 1 | 2 | – | 1 | – |
| 5→6 | 4 | 1 | 2 | 1 | 3 | – | – |
| 6→7 | 6 | 1 | 2 | 3 | – | 1 | – |
| 7→8 | 6 | 2 | 2 | 2 | 2 | 2 | 1 |

Deux faits serveur sur lesquels repose toute la stratégie :
- Une incantation élève **tous les joueurs de même niveau sur la tuile** (le
  nombre de joueurs est un *minimum*, pas un plafond).
- Un rituel réussi **ne consomme que les pierres de ce niveau** sur la tuile, et
  laisse le reste. (Vérifié côté serveur.)

---

## 2. Parler au serveur (le protocole)

Texte brut, ligne par ligne, une seule socket TCP. Commandes envoyées par le bot
et ce qu'il attend en retour :

| Commande | Réponse serveur | Sens |
|----------|-----------------|------|
| `Forward` / `Left` / `Right` | `ok` | avancer / tourner |
| `Look` | `[tile0, tile1, ...]` | cône de vision (voir §6) |
| `Inventory` | `[food n, linemate n, ...]` | stock propre |
| `Take <obj>` / `Set <obj>` | `ok` / `ko` | ramasser / poser un objet |
| `Broadcast <text>` | `ok` | son vers **tous** les joueurs (§7) |
| `Incantation` | `Elevation underway` puis `Current level: N` / `ko` | rituel |
| `Fork` | `ok` | pond un œuf + libère un slot d'équipe |
| `Connect_nbr` | `<int>` | slots d'équipe **libres** (pas le nombre actif) |
| `Eject` | `ok`/`ko` | pousse les joueurs hors de la tuile |

Messages serveur non sollicités : `message K, <text>` (un broadcast a été
entendu), `eject: K` (on s'est fait pousser), `dead` (on est mort de faim).
Le parsing des réponses vit dans
[`handle_line`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L495).

**Angles morts clés que le bot doit contourner :**
- **Aucune commande ne renvoie la position.** Le bot ne connaît jamais son
  (x, y). Toute la navigation vers les coéquipiers se fait uniquement par
  **relèvement sonore** des broadcasts.
- **`Connect_nbr` renvoie les slots libres, pas la taille de l'équipe** — un bot
  ne peut pas demander directement « combien sommes-nous en vie ? ».
- Un broadcast reçu ne porte **ni émetteur ni équipe** — juste `message K,
  <text>`. L'origine d'équipe est par *convention* (chaque message est préfixé
  `<team>:`), et est falsifiable. `K` est le seul bit fiable (voir §6).

---

## 3. Connexion & handshake

[`connect`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L181) →
[`handshake`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L273) :

1. Lire `WELCOME`.
2. Envoyer le nom d'équipe.
3. Lire le nombre de slots libres, puis les dimensions `X Y` de la carte.
4. Si `-f` n'a pas été fourni, **mesurer** la fréquence serveur empiriquement
   ([`estimate_frequency`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L311)) : chronométrer les
   aller-retours `Connect_nbr` vs `Forward` ; le delta est le coût mural d'une
   action, et `freq ≈ 7 / delta` (une action coûte `7/f` secondes de temps de
   jeu).

La fréquence pilote chaque timeout du bot, car c'est l'horloge du serveur — pas
le CPU du bot — qui cadence la partie.

---

## 4. La boucle principale

[`run`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L414) bloque sur `select` (réveillé
instantanément par une réponse serveur, plafond d'inactivité de 50 ms pour que
les timers tournent quand même), draine toutes les lignes disponibles dans
[`handle_line`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L495), puis appelle
[`tick`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L446).

### Pipelining (la FIFO `pending`)
Le serveur bufferise jusqu'à **`MAX_COMMAND_QUEUE = 10`** commandes par client. Le
bot tient une FIFO [`self.pending`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L83) (plafonnée
à 8) des commandes en attente de réponse (le serveur répond **dans l'ordre**).
Deux régimes :

- **Plans de déplacement déterministes** (`Forward Forward Right …`) poussés
  plusieurs d'un coup via
  [`send_next_plan_cmd`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1590), pour que la file
  d'actions du serveur ne reste jamais oisive entre deux pas.
- **Décisions réactives** (Look→Take, Inventory, Incantation) attendent que le
  pipeline se vide (`if self.has_pending(): return`) pour toujours voir un état du
  monde frais.

Une commande de tête coincée est jetée après un timeout par commande
([`cmd_timeout`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L264)) : une réponse perdue ne peut
pas bloquer le bot pour toujours.

### Ordre de priorité de `tick`
1. jeter par timeout une commande de tête périmée,
2. réalimenter le pipeline de déplacement,
3. (drain) — abandonner s'il reste quoi que ce soit en attente,
4. envoyer un `GATHER_ACK` en file si dû,
5. `Look` forcé (vision invalidée),
6. suivre un relèvement de broadcast,
7. rafraîchir l'`Inventory` si périmé,
8. **`choose_state()` puis `run_state()`** — le cerveau.

---

## 5. Le modèle du monde

[`Memory`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L43) est tout l'état de croyance du bot :
niveau, inventaire, dernière grille `Look`, slots libres, plus des compteurs de
fraîcheur (`actions_since_inventory`, `moves_since_look`) et des horodatages pour
décider quand vision/inventaire sont périmés et doivent être re-interrogés. Vision
et inventaire sont **mis en cache** et rafraîchis seulement quand
compteurs/timers les déclarent périmés — re-`Look`er à chaque action brûlerait le
budget d'actions.

---

## 6. Vision & navigation

`Look` renvoie un cône de tuiles aplati, indice 0 = tuile courante, s'élargissant
vers l'avant. [`tile_pos`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1604) convertit un
indice plat en `(distance_avant, décalage_latéral)` ;
[`plan_to_tile`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1618) en fait une séquence
`Left/Right/Forward`. [`move_to_tile`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1567) replie
l'action terminale (p. ex. `Take food`) dans la **même** rafale pipelinée, pour
que l'arrivée ne coûte pas un aller-retour Look+Take supplémentaire.

### Relèvement sonore (la seule façon de trouver les coéquipiers)
Un broadcast entendu vient avec `K` (0–8), la direction de la source **dans le
repère propre de l'auditeur** : `0` = même tuile, `1` = droit devant, puis **dans
le sens horaire** (`3` = droite, `5` = derrière, `7` = gauche).
[`broadcast_plan`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1657) avance pour que la source
remonte vers l'avant ; le leader rediffuse en continu et le suiveur se réoriente à
chaque fois. Le homing étant grossier (8 directions) et le leader pouvant lui-même
bouger, la convergence est **lente et oscillante** — le relèvement 0 (co-location
exacte) est rare. Ce seul fait a façonné toute la conception de la coordination.

**Le relèvement 0 est le seul signal serveur faisant autorité sur la
co-location physique** — c'est l'épine dorsale du handshake ARRIVED ci-dessous.

---

## 7. La machine à états

[`choose_state`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L612) choisit un état par priorité
stricte à chaque tick ; [`run_state`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L682)
l'exécute.

| État | Quand | Fait |
|------|-------|------|
| `SURVIVE` | food ≤ `survive_min` | tout lâcher, trouver & manger de la nourriture |
| (HOLD) | gelé pour le rituel d'un coéquipier | rester sur la tuile (LOOK, pas de move) |
| `PREPARE_INCANTATION` / `INCANT` | pierres en main, prêt | `Set` chaque pierre, puis `Incantation` |
| `LOOK` | vision périmée | rafraîchir le cône |
| `CALL_TEAMMATES` | pierres en main, besoin de ≥2 joueurs | lancer le protocole de rassemblement (§8) |
| `COLLECT` | une pierre nécessaire est visible | marcher dessus, `Take` |
| `FARM_FOOD` | besoin de réserve de food | marcher vers la food, `Take` |
| `REPRODUCE` | bas niveau & nourri | `Fork` + engendrer un enfant (§9) |
| `EXPLORE` | rien d'utile en vue | errer (biaisé vers l'avant) |
| `DEAD` | a reçu `dead` | s'arrêter |

**La survie préempte toujours tout** — le bot ne se fige et ne se coordonne jamais
jusqu'à la famine.

### Économie de nourriture (la contrainte liante)
`STARTING_FOOD = 10`, et la food se vide en continu (~8 food/s à f=1000). Un bot
immobile meurt de faim en gros en `food × 0,126` secondes. La survie est plate
(`survive_food = 8`, `survive_per_level = 0`) : une survie échelonnée par niveau a
été essayée et a *régressé* — relever le plancher pousse juste les bots haut
niveau (précieux) à farmer pour toujours sans jamais se coordonner. C'est la
nourriture, pas les pierres, qui est rare.

---

## 8. Handshakes de coordination (comment les bots se trouvent)

Amener N bots de même niveau sur une tuile, sans connaître les positions, est la
partie difficile. Quatre handshakes par broadcast le résolvent. Ce sont les
primitives de convergence que le blitz (§10) pilote, et elles soutiennent aussi le
repli par niveau de dernier recours (§10) :

- **GATHER / GATHER_ACK** — un leader avec les pierres ouvre une requête
  (`start_gather`), en diffusant `GATHER:req=…:level=…:need=…`. Les coéquipiers de
  même niveau et nourris répondent `GATHER_ACK` et commencent à homer sur le
  relèvement. Le plus petit `bot_id` gagne si deux leaders entrent en collision
  ([`handle_gather`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1044)).
- **ARRIVED (le grand déblocage)** — `Look` montre `"player"` **sans niveau**,
  donc un leader ne peut pas distinguer un vrai coéquipier de même niveau d'un
  bébé niveau 1 de passage, et tirer sur le mauvais compte garantit un `ko`. À la
  place, quand le broadcast du leader atteint un suiveur en **relèvement 0**, le
  suiveur *sait* qu'il est sur la tuile exacte et annonce `ARRIVED`. Le leader
  compte les **émetteurs ARRIVED distincts de même niveau** et n'incante que sur
  ce compte
  ([`present = 1 + len(self.arrived_at_leader)`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L886)).
  Cela a fait passer le taux de réussite des rituels de ~14 % à ~92 %.
- **HOLD** — avant son `Set…Set…Incantation` en plusieurs étapes, le leader
  diffuse `HOLD` pour que les coéquipiers assemblés **se figent sur la tuile** au
  lieu de partir errer et de faire passer le compte sous `need`. Seuls les bots
  suivant réellement *ce* leader se figent
  ([`handle_hold`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1010)), sinon un broadcast à
  l'échelle de la carte figerait et affamerait les non-participants.

---

## 9. Reproduction, fork & œufs

Le `Fork` serveur ne fait que **pondre un œuf et libérer un slot** — rien n'éclôt
tant qu'un *nouveau client ne se connecte pas* avec le nom d'équipe. Donc à chaque
`Fork ok` le bot **engendre un nouveau processus**
([`spawn_child_for_egg`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L388), `subprocess.Popen`
détaché) pour occuper cet œuf ; sinon les œufs pourrissent et la population ne
croît jamais. `SIGCHLD` est ignoré pour que les enfants ne deviennent pas des
zombies.

Le fork est **discipliné**
([`should_fork`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1701)) : seulement tant que
**niveau ≤ 2**, seulement bien nourri, et limité en cadence par bot. Un bot
niveau 3+ est précieux et contraint par la food — forker lui coûte de la food
*et* ajoute un concurrent pour une food rare, alors que le bébé ne grandira pas à
temps pour aider. Un fork sobre a maintenu l'attrition basse, ce qui a permis à
une cohorte de haut niveau de persister et grimper au niveau 8.

### Le recensement d'ouverture — `HELLO` et l'élection du forker
Le blitz (§10) a besoin de **≥ `census_target` (par défaut 6) coéquipiers
joignables**, mais un bot ne peut pas demander au serveur la taille de son équipe
— `Connect_nbr` renvoie les *slots libres*, pas le compte actif (§2). Donc
l'équipe **s'auto-compte par broadcast** :

- Au démarrage chaque bot diffuse **`HELLO:from=<bot_id>`**
  ([`broadcast_hello`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1661)) et le ré-émet tous
  les `hello_interval` (`60/freq`, min 1 s) tant que le recensement est ouvert.
  Chacun fait l'union des ids émetteurs dans
  [`team_members`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L209). À la **première** détection
  d'un nouvel id, le bot renvoie un HELLO en écho
  ([`handle_hello`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1665)) pour que le nouveau
  venu apprenne les bots déjà présents — borné à la première détection pour ne pas
  saturer.

- **Élection du forker :** le bot avec le **plus petit `bot_id` connu** est le
  seul forker ([`is_census_forker`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1684)). Lui
  seul forke — *au-delà du plafond normal par bot* — jusqu'à ce que l'union
  atteigne `census_target`, puis **personne ne forke**
  ([branche census de `should_fork`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1721)). Le
  compte de membres observé est le limiteur global, donc le total de forks ≈
  `quorum − bots_initiaux`. Les enfants forkés et les frères tardifs font HELLO au
  démarrage et rejoignent l'union ; leurs ids sont **seedés par le temps**
  (toujours plus grands) pour ne jamais détrôner le forker élu — l'élection est
  stable sans handshake.

Deux garde-fous de cadence gardent le compte honnête
([`census_maintain`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1689) /
[`should_fork`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1701)) :
- une **fenêtre de stabilisation** (`census_settle`, 3 s) avant qu'un bot
  n'agisse comme forker, pour qu'un bot tout juste démarré ne forke pas alors
  qu'il pense (à tort) être seul — un singleton est toujours son propre id minimal;
- après chaque `Fork ok` le fork suivant **patiente** jusqu'à ce que l'enfant
  s'annonce via HELLO
  ([`census_fork_deadline`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L663)), ou jusqu'à
  `census_child_timeout` (4 s) si l'enfant ne démarre jamais — pour qu'un œuf lent
  ne gonfle pas la population, et qu'un œuf mort-né ne bloque pas le recensement.

Tout le recensement est borné par `census_timeout` (30 s) ; si le quorum ne se
forme jamais, il se ferme et `should_fork` revient au **plafond original par bot**
(`max_forks`), le même repli que le blitz manifeste utilise sur une carte
dégénérée (§10).

---

## 10. Le Blitz — la stratégie

**La seule et unique stratégie** (pas d'interrupteur). Mesuré : **~30 s pour le
niveau 8, 4/4 de taux de victoire** à f=1000 / 20×20 / 8 bots ; robuste sur des
cartes 10×10–40×40. Elle exploite les deux faits serveur du §1 : lâcher *toutes*
les pierres une fois, puis incanter en boucle — chaque rituel ne mange que la part
de son niveau, et les six joueurs montent à chaque fois.

Après le rapide **L1→L2** en solo, chaque bot entre dans
[`manifest_choose`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1376), un rendez-vous en
phases :

1. **`collect`** — chaque bot fonce ramasser le **manifeste complet de pierres
   L2→L8** (`manifest_target` = la somme de chaque exigence de niveau : 8
   linemate, 8 deraumere, 10 sibur, 5 mendiane, 6 phiras, 1 thystame = **38
   pierres**). Le **premier** à terminer s'auto-élit **anchor** et diffuse
   `MFOOD`.
2. **`bank`** — en entendant `MFOOD`, tout le monde arrête de collecter et remplit
   la food jusqu'à **`food_reserve` (par défaut 200)**.
3. **`ready`** — les bots remplis diffusent `MRDY`/`MRDY2`. L'anchor compte les
   coéquipiers prêts ; quand `len(ready_too) + 1 ≥ 6`
   ([le `+1` est l'anchor lui-même](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1420)) il tire
   `MCOME` et ouvre un rassemblement à 6 joueurs.
4. **`converge`** — la cohorte marche vers l'anchor **réservoirs de food pleins**,
   en homing sur le relèvement de rassemblement de l'anchor ; l'arrivée est
   suivie par **ARRIVED** (§8), pas par le compte de joueurs aveugle au niveau.
5. **`blitz`** — avec 6 ARRIVED, l'anchor
   ([`start_manifest_blitz`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L1461)) lâche les 38
   pierres et tire `Incantation`. À chaque `Current level: N`, le handler tire
   immédiatement l'`Incantation` suivante
   ([driver du blitz](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L528)) — L2→L8 en ~3 s.

### Pourquoi `food_reserve` est le bouton porteur
La convergence est lente et oscillante (§6), donc l'anchor doit **tenir le
ralliement ~25 s** pendant que six corps affluent vers sa tuile. À ~8 food/s, le
temps de maintien ≈ `réserve / 8`, donc **200 ≈ 25 s** — juste assez. Sous ~140 le
ralliement meurt de faim avant que six s'assemblent ; au-dessus de 200 on gaspille
juste du temps de fourrage. C'est **indépendant de la taille de carte**. Deux
correctifs de soutien le font marcher : le rassemblement de l'anchor est
**persistant** (ne réinitialise jamais le compte ARRIVED — réinitialiser était
l'ancien « thrash d'abandon à 997 »), et l'anchor **n'abandonne pas** au plancher
normal d'abandon-food, descendant jusqu'à `survive_min` en mangeant la food de la
tuile ([`call_teammates`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L862)).

L'élection de l'anchor est déterministe en cas de contention : tout bot entendant
un `MFOOD`/`MRDY` d'un **`bot_id` plus petit** se rétrograde et se rallie à
celui-là ([`manifest_demote_to`](https://github.com/EpitechPGE2-2025/G-YEP-400-RUN-4-1-zappy-3/blob/main/ai/baseline/zappy_ai_baseline.py#L739)). Si aucun
anchor n'émerge jamais (p. ex. la carte manque de thystame, ou moins de 6
coéquipiers sont joignables) dans `manifest_timeout`, le bot pose
`manifest_gave_up` et joue le simple rassemblement par niveau (§8) en filet de
sécurité automatique de dernier recours — pas un mode sélectionnable, juste pour
qu'une carte dégénérée ne bloque pas l'équipe à jamais.

### Vocabulaire des broadcasts
| Message | De → sens |
|---------|-----------|
| `HELLO` | tout bot au démarrage : ping de recensement — union de l'id `from`, élit le forker (§9) |
| `GATHER` / `GATHER_ACK` | leader ouvre un ralliement / coéquipier accepte |
| `ARRIVED` | suiveur physiquement sur la tuile du leader (relèvement 0) |
| `HOLD` | leader : figez-vous, rituel en cours de démarrage |
| `MFOOD` | anchor : arrêtez de collecter, banquez la food |
| `MRDY` / `MRDY2` | anchor nourri+prêt / membre nourri+prêt |
| `MCOME` | anchor : lancement, convergez maintenant |

Tous portent `:level=…:from=<bot_id>` et sont filtrés par le préfixe `<team>:`.

---

## 11. Boutons de réglage & environnement

| Var / champ | Défaut | Effet |
|-------------|--------|-------|
| `ZAPPY_RESERVE` | 200 | food banquée avant convergence (le cadran principal) |
| `ZAPPY_TEAM_TARGET` | 6 | quorum de recensement — le forker élu forke jusqu'à ce que l'équipe l'atteigne (§9) |
| `ZAPPY_CENSUS_TIMEOUT` | 30 | secondes pendant lesquelles le recensement reste ouvert avant repli sur `max_forks` |
| `max_forks` | 3 | plafond de fork par bot (repli census / `manifest_gave_up`) |
| `survive_food` | 8 | plancher de survie plat |
| `-f <freq>` | mesurée | fréquence serveur ; plafond pratique ~1000–1500 |

**Plafond de fréquence :** le bot est **limité par l'attente I/O** (bloqué sur le
délai serveur par action), pas par le CPU. Au-delà de ~f=1500, le temps mural
d'une seule action dépasse le budget `7/f` et le bot meurt de faim. Rester à
f ≤ 1000.

---

## 12. Observabilité

Le bot tourne **silencieux par défaut** (le log par action coûte des ms/action et
affame à haute f). `-v` active la trace complète `[AI → SERVER]` / `[SERVER →
AI]`. Marqueurs de cycle de vie toujours actifs quel que soit `-v` :
**`LEVELUP <n>`**, **`[AI] DEAD`**, **`spawned child`** — à grep pour scorer les
runs silencieux.

---

## 13. Résumé en une ligne

Montée solo jusqu'à L2 → chaque bot thésaurise le manifeste complet de 38 pierres
L2→L8 → le premier terminé devient l'anchor → l'équipe banque une grosse réserve
de food → six convergent réservoirs pleins via homing sonore + le handshake
ARRIVED → l'anchor lâche tout et incante six fois d'affilée → **6 drones touchent
le niveau 8 en ~30 s et la partie est gagnée.**
