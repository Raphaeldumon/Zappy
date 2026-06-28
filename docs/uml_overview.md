# UML overview (vue d'ensemble)

Diagrammes volontairement simples — les grandes idées seulement.
Rendu automatique sur GitHub et sur le site MkDocs (plugin mermaid2).

## 1. Composants

```mermaid
flowchart LR
    subgraph clients
        AI["zappy_ai<br/>(client IA, Python — dossier ai/)"]
        GUI["zappy_gui<br/>(visualiseur 3D raylib — branche gui3Dtest)"]
    end

    SRV["zappy_server<br/>(simulation, autoritaire)"]

    AI -- "protocole IA<br/>Forward / Look / Take... -> ok/ko" --> SRV
    GUI -- "handshake GRAPHIC<br/>requetes msz / pin / sst" --> SRV
    SRV -- "events temps reel<br/>pnw ppo bct pic pie pdi seg..." --> GUI

    PROTO["zappy_protocol<br/>(contrat partage, header-only)"]
    PROTO -.-> AI
    PROTO -.-> GUI
    PROTO -.-> SRV
```

Une seule source de vérité : le serveur. Les GUIs sont des miroirs passifs
(événements poussés), les IA n'ont que leur vision locale (`Look`).

## 2. Serveur — classes principales

```mermaid
classDiagram
    class Server {
        run() : boucle principale
        orchestration handshake / commandes
    }
    class NetworkLayer {
        poll_once(timeout)
        send_to(fd, line)
        broadcast_gui(line)
        sockets non bloquants
    }
    class EventScheduler {
        schedule(tick, callback)
        advance_to(now)
        file de priorité déterministe
    }
    class WorldState {
        tiles / players / eggs / teams
        move, take, set, eject, elevate
        état pur, zéro I/O
    }
    class Protocol {
        parse_ai_command()
        GuiEmitter (msz, bct, pnw...)
        handshake
    }

    Server --> NetworkLayer : I/O réseau
    Server --> EventScheduler : temps virtuel (ticks = temps réel x f)
    Server --> WorldState : mutations du monde
    Server --> Protocol : parse / format
```

Mono-thread, événementiel : tout (coût des actions, faim, incantations,
respawn) est un callback planifié à un tick. `poll()` dort jusqu'au prochain
paquet **ou** prochain événement.

## 3. Cycle de vie d'une commande IA

```mermaid
sequenceDiagram
    participant IA as zappy_ai
    participant S as Server
    participant W as WorldState
    participant G as zappy_gui

    IA->>S: "Forward\n"
    Note over S: file ≤ 10 commandes / drone
    S->>S: schedule(now + 7/f ticks)
    Note over S: le drone attend,<br/>le serveur ne bloque jamais
    S->>W: move_forward(player)
    S-->>IA: "ok\n"
    S-->>G: "ppo #id x y o\n"
```

## 4. GUI 3D (branche `gui3Dtest`, dossier `gui3d/`)

La version active est **`gui3d/`** sur la branche `gui3Dtest` : un rendu 3D
raylib (caméra libre, modèles `.glb` par ressource + drone animé, skybox 360,
audio). Le réseau **est câblé** : `main.cpp` se connecte, fait le handshake
`GRAPHIC` (`msz`/dimensions), puis `Interface` lit le flux via `NetClient` et
applique chaque événement poussé (`pnw ppo bct pic pie pdi seg…`) au monde via
`ProtocolParser`. Une timeline enregistre tout (record + scrub + pause).

raylib est **entièrement encapsulé** : seule `RaylibEngine` (TU
`raylibWrapper.cpp`) touche raylib ; tout le reste ne manipule que des types
neutres `gfx::` (Vec3, Color, Camera, handles opaques) — `interface.cpp` ne
contient aucun symbole raylib.

```mermaid
classDiagram
    class Interface {
        run() : boucle 60 FPS
        handleInput / update / render
        gfx Camera libre + modèles .glb animés + skybox
        timeline (record / scrub)
    }
    class RaylibEngine {
        façade graphique (seul point de contact raylib)
        fenêtre / input / audio
        modèles / textures / animations / skybox
        dessin 2D + 3D ; n'expose que des types gfx
    }
    class NetClient {
        connect / handshake GRAPHIC
        poll() : lignes serveur
    }
    class ProtocolParser {
        apply(line, map, state)
        pnw ppo bct pic pie pdi seg...
    }
    class GameMap {
        getTile(x, y) / setResource
        tiles row-major (MapTile)
    }
    class MapTile {
        resources[7]
        players (aiPlayer)
    }

    Interface --> RaylibEngine : rendu (via gfx)
    Interface --> NetClient : flux serveur
    Interface --> ProtocolParser : applique les events
    Interface --> GameMap : état du monde
    ProtocolParser --> GameMap : mutations
    GameMap --> MapTile : cases
```

Le serveur reste l'unique source de vérité : le GUI ne fait que refléter les
événements poussés (cf. section 1), via le même contrat de protocole, quelle
que soit l'implémentation du rendu.

## 5. Règles clefs (rappel)

| Mécanisme | Valeur |
|---|---|
| Vie | 1 food = 126/f secondes, famine → mort (`pdi`) |
| Incantation | gel des participants, 300 ticks, re-vérification à la fin |
| Respawn ressources | toutes les 20 ticks, complément vers densités cibles |
| Victoire | 6 joueurs niveau 8 dans une équipe → `seg` |

## 6. Client IA (dossier `ai/`) — structure du code

Un seul module fait tout le bot : [`ai/baseline/zappy_ai_baseline.py`]. `make`
le **copie** en `ai/zappy_ai` (l'exécutable lancé). Comportement/stratégie
détaillés dans [`ai/AI_OVERVIEW.md`](../../ai/AI_OVERVIEW.md).

```mermaid
flowchart TB
    MAIN["main() / parse_args()"] --> AI["classe AI<br/>(toute la logique)"]
    BASE["ai/baseline/zappy_ai_baseline.py"] -->|"make : cp"| MIR["ai/zappy_ai<br/>(copie exécutée)"]
```

Le cœur est la classe **`AI`** : un automate qui possède un `Memory` (l'état du
monde perçu) et un `State` courant. Une seule socket non bloquante, un pipeline
FIFO de commandes (`pending`, ≤ 8). Les méthodes se regroupent par
responsabilité.

```mermaid
classDiagram
    class AI {
        run() : boucle principale
        tick() : pipeline puis decision
        handle_line() : tri des lignes serveur
        choose_state() : choix d etat
        run_state() : execution d etat
        plan_to_tile() : navigation geometrique
        broadcast_plan() : ralliement sonore
        start_gather() : coordination
        start_manifest_blitz() : blitz
        spawn_child_for_egg() : fork
    }
    class Memory {
        dataclass etat percu
        level / inventory / visible_tiles
        free_slots / compteurs / timers
    }
    class State {
        enum 10 etats
        SURVIVE / LOOK / COLLECT
        CALL_TEAMMATES / INCANT
        REPRODUCE / DEAD
    }
    class REQ {
        const table d elevation
        niveau vers joueurs et pierres
    }

    AI *-- Memory : possede
    AI --> State : etat courant
    AI ..> REQ : table d elevation
```

Boucle de contrôle : `run()` (`select` sur la socket) → `handle_line()` classe
chaque ligne serveur (réponse `ok/ko`, `Current level`, broadcast, `dead`…) →
`tick()` vide le pipeline puis appelle `choose_state()` / `run_state()`. La
navigation est purement géométrique (`Look` → indices de tuiles → plan
`Left/Right/Forward`), le ralliement purement sonore (`broadcast_plan` sur le cap
`K`). Le serveur reste l'unique source de vérité : l'IA n'a que sa vision locale
et l'audio des broadcasts.
