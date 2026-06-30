# Architecture GUI 3D (raylib)

## Contraintes sujet (rappel)

- Binary `zappy_gui`, **C/C++/Rust** → choisi : **C++17** (raylib glue)
- Client **GRAPHIC** : se connecte au serveur, s'authentifie avec `GRAPHIC`, n'observe que (jamais d'action de jeu)
- Usage : `./zappy_gui -p port [-h machine]` (host par défaut `127.0.0.1`, code d'erreur `84`)
- Reçoit toute la vue du monde via le protocole GUI (`bct`, `ppo`, `pnw`, `seg`…), et peut piloter la vitesse serveur (`sst`)
- Affichage **temps réel** d'une partie en cours, mode spectateur

## Vue d'ensemble

Un seul client mono-fenêtre, mono-thread, organisé autour d'une classe « chef
d'orchestre » `Interface` qui possède tous les sous-systèmes. La dépendance à
raylib est **entièrement confinée** dans une façade (`RaylibEngine`) : le reste
du code ne connaît que des types `gfx::` neutres, ce qui rend le renderer
remplaçable et `interface.{hpp,cpp}` testable sans GPU.

```
                  ┌─────────────────────────────────────────┐
                  │   main(argc, argv)  (main.cpp)          │
                  │   - parse -p / -h                       │
                  │   - NetClient::connect + handshake      │
                  │   - Interface(net, w, h).run()          │
                  └──────────────────┬──────────────────────┘
                                     │
                  ┌──────────────────▼──────────────────────┐
                  │            class Interface              │
                  │  boucle: handleInput → update → render  │
                  │ ┌─────────────┐  ┌────────────────────┐ │
                  │ │ RaylibEngine│  │ NetClient (TCP)    │ │
                  │ │  façade gfx │  │  poll() par frame  │ │
                  │ │  (PIMPL)    │  └─────────┬──────────┘ │
                  │ └─────────────┘            │            │
                  │ ┌──────────────────────────▼─────────┐  │
                  │ │ ProtocolParser.apply(line, …)      │  │
                  │ └──────────┬─────────────┬───────────┘  │
                  │      ┌─────▼─────┐  ┌────▼──────────┐    │
                  │      │ GameMap   │  │ GuiState      │    │
                  │      │ tiles +   │  │ players/eggs/ │    │
                  │      │ resources │  │ teams/winner  │    │
                  │      └───────────┘  └───────────────┘    │
                  └─────────────────────────────────────────┘
```

## Layout des sources

```
gui/
├── CMakeLists.txt            ← détecte raylib, sinon skip propre (warning)
├── main.cpp                  ← parse args, connexion + handshake, lance Interface
├── interface.{hpp,cpp}       ← classe Interface : boucle, caméra, rendu, HUD, timeline
├── raylibWrapper.{hpp,cpp}   ← RaylibEngine : façade gfx (PIMPL) isolant raylib
├── gfxTypes.hpp              ← types neutres : Vec2/3, Color, Camera, Ray, Key, handles
├── netClient.{hpp,cpp}       ← NetClient : connexion TCP, handshake GRAPHIC, poll() ligne
├── protocolParser.{hpp,cpp}  ← ProtocolParser : applique une ligne wire à Map + State
├── gameMap.{hpp,cpp}         ← GameMap : grille W×H de MapTile (7 ressources + joueurs)
├── guiState.hpp              ← GuiState : joueurs, œufs, équipes, incantations, vainqueur
├── aiPlayer.hpp              ← aiPlayer : id, équipe, pos, orientation, niveau, vie
└── assets/                   ← skybox (.png + shaders), modèles .glb, textures, musique
```

## Séparation renderer / logique (façade `RaylibEngine`)

`RaylibEngine` (`raylibWrapper.{hpp,cpp}`) est une **façade PIMPL** : tout ce qui
touche raylib — fenêtre, caméra, skybox, sol batché, dessin 2D/3D, audio, modèles,
animations — est une méthode de cette classe. `interface.cpp` reste 100 %
raylib-free et ne manipule que des `gfx::` :

- **Types** (`gfxTypes.hpp`) : `Vec2`, `Vec3`, `Color`, `BBox`, `Ray`, `Camera`
  (free-fly : position/target/up), `enum class Key`, et des handles opaques
  (`ModelHandle`, `TextureHandle`, `MusicHandle`, `AnimSetHandle`, `NoHandle`).
- **API gfx** : `beginDrawing/endDrawing`, `beginMode3D/endMode3D`, `keyDown/keyPressed`,
  `mouseWheel`, `drawText/drawRect/drawModelEx/drawCube/…`, `worldToScreen`,
  `loadModel/loadTexture/loadMusic/loadAnimations` + leurs `unload`.

Conséquence : changer de moteur (p. ex. Vulkan) ne touche que la
façade ; la logique d'affichage et le réseau ne bougent pas.

## Réseau et flux protocole

- **`NetClient`** : `connect(host, port)` bloquant au démarrage, puis `handshake()`
  (lit `WELCOME`, envoie `GRAPHIC`, lit `msz` → taille de carte). Ensuite le socket
  passe non-bloquant : `poll()` est appelé **chaque frame** et rend les lignes
  `'\n'`-terminées complètes (jamais bloquant). `send()` sert les requêtes GUI.
- **`ProtocolParser::apply(line, map, state)`** route chaque ligne wire vers `GameMap`
  et `GuiState`. Tags gérés :
  `bct` (contenu tuile), `tna` (nom d'équipe), `pnw`/`ppo`/`plv`/`pex`/`pdi`
  (joueurs : new/pos/level/éjection/mort), `enw`/`ebo`/`edi` (œufs),
  `pbc` (broadcast), `pic`/`pie` (incantation start/end), `pgt`/`sgt`/`sst`
  (ressources/temps), `seg` (fin de partie → vainqueur).
- **Timeline (record + scrub)** : chaque ligne modifiant l'état est enregistrée avec
  son instant d'arrivée (`_history`). En live, application incrémentale ; en scrub,
  le monde est reconstruit en rejouant l'historique jusqu'à `t` (`rebuildWorldTo`).

## Modèle d'état

- **`GameMap`** : grille `W×H` de `MapTile` ; chaque tuile porte un tableau de 7
  ressources (`MAP_RESOURCE_NAMES`) et la liste des joueurs présents. Accès bornés
  (`getTile` lève `std::out_of_range`).
- **`GuiState`** : `unordered_map<id, aiPlayer>` joueurs, `unordered_map<id, EggInfo>`
  œufs, liste d'équipes, set des tuiles en incantation, `winner` / `hasWinner`.
- **`aiPlayer`** : id, équipe, `x/y`, `Orientation` (N/E/S/O), niveau, unités de vie.

## Boucle de jeu (`Interface::run`)

Chaque frame enchaîne trois étapes :

1. **`handleInput()`** — caméra free-fly (souris = regard, ZQSD = vol, Shift = boost
   vitesse, Space / Ctrl (ou C) = haut/bas, molette = vitesse de vol), sélection de tuile (clic = ray-pick,
   double-clic = focus), suivi de joueur (`F`), contrôle de vitesse (`sst`), pause /
   scrub timeline, `Enter` masque/affiche l'écran de fin.
2. **`update()`** — `recordIncoming()` draine le socket, applique les lignes, met à
   jour la machine d'animation des joueurs (glide cellule-à-cellule synchronisé au
   clip Walk), les death ghosts, la musique.
3. **`render()`** — skybox → sol batché → modèles (ressources, joueurs animés,
   œufs, surbrillance de sélection) en `Mode3D`, puis HUD 2D par-dessus.

## Rendu 3D

- **Skybox** : panorama équirectangulaire 360° échantillonné par direction de vue
  dans un shader (`assets/shaders/skybox.{vs,fs}`), dessiné sur un cube centré
  caméra au far plane → fond infiniment lointain, sans pincement aux pôles ni
  couture. Cube/shader/texture possédés par la façade.
- **Sol** : damier batché (`drawCheckerFloor`) avec deux textures (sol clair / sombre).
- **Ressources** : un `.glb` par type, chargé une fois, instancié sur chaque tuile
  le portant ; bounding box mise en cache pour mettre à l'échelle et poser la base
  sur la surface. Fallback cube si le `.glb` manque.
- **Joueurs** : modèle `.glb` animé. Clips résolus par nom vers des slots fixes
  (`Idle, Walk, Death, Kick, Dance, Pickup, Jump`). Chaque joueur porte son propre
  clip + frame, sa pose étant uploadée juste avant son `drawModelEx` → animations
  indépendantes. La mort (`pdi`) efface le joueur avant l'anim : un **death ghost**
  rejoue le clip Death une fois à la dernière pose connue.

## HUD et écrans 2D (dessin custom, pas de raygui)

Tout le 2D passe par `RaylibEngine::drawText/drawRect/…` — aucune dépendance raygui.

- **HUD compact** permanent (haut-gauche).
- **Stats panel** global (`Tab`).
- **Help overlay** liste des contrôles (`H` / `F1`).
- **End screen** : résumé centré du vainqueur après `seg`, couleurs par équipe
  (`teamColor`), scroll molette sur la liste des joueurs ; `Enter` le masque pour
  continuer à voler dans la partie terminée.
- **Tile info panel** (haut-droite) sur la tuile sélectionnée.
- **Timeline bar** (bas) : position + mode live/pause/scrub.

## Contrôles

| Touche / souris        | Action                                            |
|------------------------|---------------------------------------------------|
| Souris                 | regarder (curseur capturé)                        |
| ZQSD / flèches         | voler (Shift = plus rapide)                       |
| Space / Ctrl (ou C)    | monter / descendre                                |
| Molette                | vitesse de vol (ou scroll de l'écran de fin)      |
| Clic gauche            | sélectionner la tuile visée (crosshair)           |
| Double-clic            | centrer la caméra sur cette tuile                 |
| R                      | revenir à la vue d'ensemble                       |
| F                      | suivre / ne plus suivre le joueur sélectionné     |
| + / -                  | vitesse de simulation (`sst`)                     |
| P                      | pause / reprise                                   |
| PageDown / PageUp      | reculer / avancer d'1 s dans le temps             |
| End                    | revenir au live                                   |
| Tab                    | stats globales                                    |
| M                      | musique on/off                                    |
| Enter                  | masquer / afficher l'écran de fin (après victoire)|
| H / F1                 | aide                                              |

## Build

Construit dans l'arbre CMake normal (pas de Makefile séparé), via le `make` racine.

```cmake
# gui/CMakeLists.txt
find_library(RAYLIB_LIBRARY NAMES raylib)
if(NOT RAYLIB_LIBRARY)
    message(WARNING "raylib not found — skipping zappy_gui. …")
    return()                       # skip propre : le reste du projet build quand même
endif()
add_executable(zappy_gui main.cpp interface.cpp raylibWrapper.cpp
                         gameMap.cpp netClient.cpp protocolParser.cpp)
target_link_libraries(zappy_gui PRIVATE ${RAYLIB_LIBRARY} GL m pthread dl rt X11)
```

- **C++17**, `-Wall -Wextra` (la façade raylib n'est pas soumise aux options strictes
  `-Wconversion`/`-Wpedantic` du reste du projet — ça ne vaut pas la peine sur du glue GUI).
- Sortie dans `build/bin/zappy_gui`, copiée en `./zappy_gui` par le Makefile racine.
- **Sans raylib installé** : la cible est sautée avec un warning, sans casser le build
  (le serveur et l'AI se construisent quand même — utilisé par la CI).
