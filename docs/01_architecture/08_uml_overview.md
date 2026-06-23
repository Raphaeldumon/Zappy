# 08 — UML overview (vue d'ensemble)

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

Le GUI est développé séparément (sur `main`, `gui/` n'est qu'un placeholder).
La version active est **`gui3d/`** sur la branche `gui3Dtest` : un rendu 3D
raylib (caméra `Camera3D`, modèles `.glb` par ressource). Le rendu et la carte
existent ; le branchement réseau (handshake `GRAPHIC` + parse du protocole GUI)
reste à câbler — `main.cpp` peuple aujourd'hui la carte avec des ressources de
test.

```mermaid
classDiagram
    class Interface {
        run() : boucle 60 FPS
        handleInput / update / render
        Camera3D + modèle food (.glb)
    }
    class RaylibEngine {
        InitWindow / beginDrawing / endDrawing
        load3DModel / drawShape (Shape)
        wrapper raylib
    }
    class GameMap {
        getTile(x, y) / addResource
        tiles row-major (MapTile)
    }
    class MapTile {
        resources[7]
        player_ids
    }

    Interface --> RaylibEngine : rendu
    Interface --> GameMap : état du monde
    GameMap --> MapTile : cases
```

Le serveur reste l'unique source de vérité : à terme le GUI ne fera que
refléter les événements poussés (cf. section 1), via le même contrat de
protocole côté serveur, indépendamment de l'implémentation du rendu.

## 5. Règles clefs (rappel)

| Mécanisme | Valeur |
|---|---|
| Vie | 1 food = 126/f secondes, famine → mort (`pdi`) |
| Incantation | gel des participants, 300 ticks, re-vérification à la fin |
| Respawn ressources | toutes les 20 ticks, complément vers densités cibles |
| Victoire | 6 joueurs niveau 8 dans une équipe → `seg` |
