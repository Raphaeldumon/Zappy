# 08 — UML overview (vue d'ensemble)

Diagrammes volontairement simples — les grandes idées seulement.
Rendu automatique sur GitHub et sur le site MkDocs (plugin mermaid2).

## 1. Composants

```mermaid
flowchart LR
    subgraph clients
        AI["zappy_ai<br/>(client IA)"]
        GUI["zappy_gui<br/>(visualiseur 3D raylib)"]
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

## 4. GUI sur la branche `main` — `zappy_gui2d`

Sur `main`, le visualiseur fonctionnel est **`zappy_gui2d`** (raylib, 2D) :
un outil de debug volontairement minimal pour *voir* l'état du serveur en
direct. (`zappy_gui` y est encore un stub ; le visualiseur 3D complet —
planète torus, HUD, post-FX — vit sur la branche `feat/raylib-3d-gui`.)

```mermaid
classDiagram
    class App {
        main() : boucle 60 FPS
        pompe réseau -> parse -> draw
        rendu grille + points ressources + HUD
    }
    class NetClient {
        connect(host, port) : handshake GRAPHIC
        poll_lines() : lignes complètes reçues
        send_line(req)
        socket non bloquant
    }
    class Parser {
        apply_line(world, ligne) bool
        dispatch par tag (msz, bct, pnw...)
        false si tag inconnu (drift protocole)
    }
    class World {
        width / height / time_unit
        tiles : ressources[7] par case
        players : pos, orientation, level
        eggs, teams, état pur (POD)
    }

    App --> NetClient : pompe le socket
    App --> Parser : 1 ligne = 1 mise à jour
    Parser --> World : mutations
    App --> World : lit pour dessiner
```

Pipeline en 4 étapes : pomper le socket, parser chaque ligne,
mettre à jour un état POD, le dessiner à 60 FPS. C'est ce même pipeline
(net → parse → état → rendu) que la version 3D réutilise et étend.

## 5. Règles clefs (rappel)

| Mécanisme | Valeur |
|---|---|
| Vie | 1 food = 126/f secondes, famine → mort (`pdi`) |
| Incantation | gel des participants, 300 ticks, re-vérification à la fin |
| Respawn ressources | toutes les 20 ticks, complément vers densités cibles |
| Victoire | 6 joueurs niveau 8 dans une équipe → `seg` |
