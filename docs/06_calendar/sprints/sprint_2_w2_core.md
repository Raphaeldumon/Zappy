# Sprint 2 (W2) — Core MVP

**Dates** : lundi 1 juin → vendredi 5 juin 2026
**Thème** : MVP fonctionnel de chaque pôle. À la fin de S2, **les 3 binaires jouent une vraie partie** (AI rule-based, GUI 2D+3D basique, server complet).

## Goals du sprint

1. **Server v1** : toute la logique de gameplay implémentée (`Forward`, `Right`, `Left`, `Look`, `Inventory`, `Broadcast`, `Connect_nbr`, `Fork`, `Eject`, `Take/Set object`, `Incantation`)
2. **GUI v1** : scène 3D base (torus dans l'espace, skybox étoiles procedurales, trantorians, ressources), HUD partiellement fonctionnel
3. **AI rule-based v1** : FSM solide qui survit + cherche pierres + initie incantations
4. **Simulateur turbo v1** : `Sim::step` reproduit le runtime (conformance test PASS)
5. **Training PPO démarré** : pipeline E2E fonctionne, première baseline trained
6. **Tests d'intégration AI↔server** : 5 scénarios YAML PASS
7. **Recorder `.zrec`** : capture une partie complète, replay readable
8. **Vue 2D planisphère** opérationnelle (rendu rectangulaire torus déroulé)

## Goals NON dans S2

- Bonus serveur (admin, hot-reload config, Prometheus exporter)
- Ray tracing, atmosphere, post-FX
- Self-play tournament + ELO
- Audio
- Replay UI (timeline scrub)

---

## D6 — Lundi 1 juin

### 9h30-10h30 : Sprint planning S2
- Recap retro S1
- Goals du sprint, tickets répartis
- Identifier les dépendances cross-team (notamment : P1 doit livrer `WorldState` complet pour que P6 puisse compléter `zappy_sim`)

### P1
- **Tâche** : Implémenter `WorldState::take_object()` + `set_object()` + `move_forward()` + `turn_left()` + `turn_right()`
- Implémenter `respawn_resources()` (densités sujet)
- Tests Catch2 par opération
- **Inputs** : skeleton de S1
- **Outputs** : opérations basiques OK
- **Critère** : tests `test_world_basic_ops.cpp` PASS

### P2
- **Tâche** : Compléter `protocol_ai::dispatch` (Forward, Right, Left, Look, Inventory)
- Connect command queue par client (max 10)
- Tests `test_protocol_ai_dispatch.cpp`
- **Inputs** : `WorldState` ops de P1
- **Outputs** : 5 commandes AI fonctionnelles bout-en-bout
- **Critère** : intégration `nc localhost 4242` → handshake + 5 cmds OK

### P3
- **Tâche** : Pipeline complet G-buffer + Lighting pass deferred basique
- Sun light directionnel
- Multi-mesh draw : torus + cubes (placeholders trantorian)
- **Outputs** : scène 3D avec lighting basique
- **Critère** : 60 FPS @ 1080p sur scène simple

### P4
- **Tâche** : Torus mesh procedural (généré CPU, big radius R, small radius r)
- Skybox cube procedural (couleurs gradient placeholder, étoiles à venir)
- Placement caméra initiale qui montre torus en orbite
- **Outputs** : torus visible orbiting + skybox
- **Critère** : peut tourner autour, 60 FPS

### P5
- **Tâche** : Connecter `pettingzoo_env` au vrai `zappy_sim` (qui sera complet en milieu de semaine — pour l'instant : stub avec actions discrètes)
- Reward shaping V0 testé avec mock environment
- Lancer 1st run PPO 30 min sur env stub → baseline win rate
- **Outputs** : training tourne, W&B logs visibles
- **Critère** : 1k samples/sec en debug mode

### P6
- **Tâche** : Compléter binding pybind11 `zappy_sim::Sim` avec `reset`/`step` qui appellent vraiment `WorldState`
- Setup release GIL (`py::call_guard<py::gil_scoped_release>`)
- Stress test 8 threads × 1000 steps sans crash
- **Inputs** : WorldState ops P1
- **Outputs** : `zappy_sim` produit états cohérents
- **Critère** : conformance test scénario "10 forwards" PASS

---

## D7 — Mardi 2 juin

### Standup

### P1
- **Tâche** : Implémenter `Incantation` (300/f ticks, vérif conditions debut + fin, table sujet)
- Implémenter `Fork` (42/f, créer egg)
- Implémenter `Eject` (push players from tile + destroy eggs)
- Tests par opération
- **Outputs** : 4 ops gameplay restantes OK
- **Critère** : tests `test_incantation.cpp` couvrent les 7 niveaux d'élévation

### P2
- **Tâche** : Compléter `protocol_ai::dispatch` (Broadcast, Connect_nbr, Fork, Eject, Take, Set, Incantation)
- Implémenter `protocol_gui::emit_*` pour pnw, ppo, plv, pin, pgt, pdr, pic, pie, pfk, pdi, enw, ebo, edi, pex, pbc
- Tests emit GUI conformes au protocole
- **Outputs** : protocole AI 100% + protocole GUI émission complète
- **Critère** : `tools/replay_protocol_dump.sh` capture une session → diff avec ref-server protocole

### P3
- **Tâche** : Ajouter pass `LightingPass` + premier shader procedural skybox étoiles (FBM noise hash)
- Hot-reload shader pour itérer rapidement
- **Outputs** : skybox étoiles brillantes, scintille
- **Critère** : ressemble à un ciel étoilé

### P4
- **Tâche** : Trantorian mesh (cube + orientation indicator), instancing pour N trantorians
- Resource mesh (gem-like, couleurs par type)
- Layout 2D planisphère via overlay (texture quad fullscreen avec carte rendue offscreen)
- **Outputs** : N trantorians visibles, N ressources visibles, 2D bottom overlay
- **Critère** : 60 FPS avec 50 trantorians + 200 ressources

### P5
- **Tâche** : Lancement training PPO sur `zappy_sim` réel (map 10x10, 4 agents, rule-based opponents bouchon)
- Eval pipeline : tournament mini sur 10 matches
- **Outputs** : training E2E fonctionne, W&B chart loss
- **Critère** : `samples_per_sec ≥ 10k`

### P6
- **Tâche** : Implémenter rule-based FSM dans `ai_cpp/src/policy_rule_based.cpp`
- Survie : recherche food si food < threshold
- Élévation : si niveau N + ressources sur tile → init incantation
- Coordination simple : `Broadcast HERE` si trouve ressource utile, `Broadcast HELP` si food critique
- Tests `test_fsm_survival.cpp`
- **Outputs** : `zappy_ai` rule-based qui survit > 5 min
- **Critère** : tests scenarios FSM PASS

---

## D8 — Mercredi 3 juin

### Standup

### P1
- **Tâche** : Persistance — snapshot disque JSON via `nlohmann/json`
- SIGUSR1 dump, SIGUSR2 reload
- Implémenter `lifespan_per_food` (player meurt si food = 0)
- Tests `test_persistence.cpp`
- **Outputs** : `kill -USR1 $(pgrep zappy_server)` produit `snapshot-*.json`
- **Critère** : snapshot reloadable produit même état

### P2
- **Tâche** : Recorder `.zrec` : write header + frames pendant la partie
- `tools/zrec_inspect.cpp` outil CLI lecture
- Tests `test_recorder_roundtrip.cpp`
- **Outputs** : `./zappy_server --record game.zrec` produit fichier valide
- **Critère** : `./zrec_inspect game.zrec --dump` retourne lignes protocole

### P3
- **Tâche** : Pass `ShadowPass` (cascaded shadow maps du soleil)
- Pass `OverlayPass` 2D pour HUD ImGui (compose ImGui par dessus)
- **Outputs** : ombres soft sous trantorians, HUD ImGui visible
- **Critère** : shadow visible sur torus surface

### P4
- **Tâche** : HUD ImGui v1 : Team list (lit `tna`), Player Info panel (`pin`, `plv`, `ppo`), Timeline events
- Speed control widget (slider) émet `sst T` au serveur
- **Outputs** : HUD réactif aux events serveur
- **Critère** : changer speed via slider impacte rythme du serveur

### P5
- **Tâche** : Reward shaping V1 (potential-based pour shaping)
- Curriculum stage 0 (tiny map 10x10) lance training nightly
- ELO bootstrap (init tous agents à 1500)
- **Outputs** : curriculum opérationnel
- **Critère** : 1ère policy stage 0 a Win rate > 50% contre random

### P6
- **Tâche** : Conformance test : ajouter 5 scenarios YAML (move, look, take, set, incantation echec)
- CI rouge si conformance fail
- ASan / UBSan en CI debug
- **Outputs** : conformance test 5 scénarios PASS
- **Critère** : CI bloque toute PR qui breakent la sim

---

## D9 — Jeudi 4 juin

### Standup

### P1
- **Tâche** : `game_rules::check_elevation_eligibility()` table sujet complète
- Hot-reload config JSON (densités, f, lifespan) via signal SIGUSR2
- Coverage push vers 70%
- **Outputs** : élévation conforme sujet
- **Critère** : `test_elevation_eligibility.cpp` PASS

### P2
- **Tâche** : Broadcast direction algorithm (sphérique, shortest path, 8 directions)
- `pbc` event GUI émit avec direction
- Tests `test_broadcast_direction.cpp` (cas: same tile = 0, voisinage = 1..8, opposite torus = shortest path)
- **Outputs** : broadcast directionality conforme sujet
- **Critère** : tests directions 8 cardinales + torus wrap PASS

### P3
- **Tâche** : `Particles` compute shader : démarrer un système de particules GPU (étoiles dans le ciel scintillent, fumée à la mort)
- Émetteur CPU → GPU buffer → compute simulation → vertex shader rendering
- **Outputs** : particules à la mort d'un trantorian
- **Critère** : ~10k particules @ 60 FPS

### P4
- **Tâche** : Caméra follow player (clic sur trantorien → caméra suit smoothly)
- Top-down camera (touche `1/2/3` pour switch modes)
- **Outputs** : 3 modes caméra fonctionnels
- **Critère** : transitions fluides entre modes

### P5
- **Tâche** : Continuer training, monitor W&B charts
- Ajouter eval contre `zappy_ref` : démarrer ref-server + N AI rule-based, mesurer win rate des policies sortie de PPO
- **Outputs** : 1er eval vs ref publié
- **Critère** : nombre concret (probablement bas, mais on a la baseline)

### P6
- **Tâche** : Broadcast codec (ADR-008) — encode/decode payload binaire + magic byte XOR team
- Tests `test_broadcast_codec.cpp` exhaustifs
- `zappy_ai` peut envoyer + recevoir broadcasts codés (mode debug log les décodes)
- **Outputs** : protocole intra-team prêt
- **Critère** : 2 AIs same team → comm OK, AIs other team → ignorent

---

## D10 — Vendredi 5 juin (M3 : MVP partie complète)

### Standup

### P1
- **Tâche** : Buffer / fix tests core
- Test integration : `./zappy_server -p 4242 -x 20 -y 20 -n red blue -c 4 -f 100` tourne 5 min sans crash, élévations qui réussissent
- **Outputs** : serveur stable
- **Critère** : 5 min sans crash, players élèvent

### P2
- **Tâche** : Buffer / fix protocol bugs
- Tests d'intégration AI↔server : `tests/integration_yaml/scenario_basic_game.yaml` PASS
- **Outputs** : 5 scénarios YAML PASS dans CI
- **Critère** : `tools/run_integration.sh` PASS sur 5 scénarios

### P3
- **Tâche** : Buffer / fix rendu bugs
- Smoke test CI (xvfb headless capture screenshot, compare reference)
- **Outputs** : smoke test CI vert
- **Critère** : reference image établie, diff < 5%

### P4
- **Tâche** : Buffer / HUD polish
- Connexion live au serveur depuis le GUI (`./zappy_gui -p 4242 -h localhost`)
- Vérifier que le rendu reflète le state serveur en temps réel
- **Outputs** : GUI live connecté
- **Critère** : démo manuelle live

### P5
- **Tâche** : 2nd training run avec reward fixes + obs encoder validation
- Vérifier que `zappy_ai` (compilé par P6) charge un model.pt et joue sur serveur (même mauvais)
- **Outputs** : E2E AI RL pipeline → inférence C++ marche
- **Critère** : `./zappy_ai -p 4242 -n red --model models/run_smoke/ckpt_latest.pt` connecte et bouge

### P6
- **Tâche** : Buffer / fix CI flaky
- Vérifier que `zappy_ai` libtorch charge un .pt → forward → action
- Doc dans `docs/01_architecture/04_ai_rl.md` mis à jour
- **Outputs** : inférence libtorch end-to-end
- **Critère** : `./zappy_ai --model demo.pt` joue (mal probablement, mais joue)

### 16h-17h : Demo Friday S2 — **M3 : démo partie complète**

Scénario démo :
1. P6 démarre `./zappy_server -p 4242 -x 30 -y 30 -n red blue -c 6 -f 500 --record demo_s2.zrec`
2. P4 démarre `./zappy_gui -p 4242 -h localhost`
3. P5 démarre 6× `./zappy_ai -p 4242 -n red -h localhost` (rule-based) + 6× pour blue
4. On observe pendant 5 min : élévations qui se déclenchent, broadcasts, morts/naissances
5. P2 montre Recorder : on charge le `.zrec` après dans le GUI (replay sera plus complet en S3 mais lecture basique OK)

### 17h-17h45 : Retro S2

### Checkpoint M3 — Definition of Done sprint
- [ ] Server v1 : toutes les commandes AI fonctionnelles **DONE**
- [ ] Server émet 100% des events GUI conformes
- [ ] Persistance snapshot + reload + hot-reload config **DONE**
- [ ] Recorder `.zrec` capture une partie **DONE**
- [ ] GUI v1 : scène 3D base, skybox étoiles, torus, trantorians, ressources, shadows, ImGui HUD partiellement réactif **DONE**
- [ ] 3 caméras (free, follow, top-down) **DONE**
- [ ] 2D planisphère overlay opérationnelle **DONE**
- [ ] AI rule-based v1 : survit > 5 min, fait élévations **DONE**
- [ ] Simulateur conformance 5 scénarios PASS **DONE**
- [ ] Training pipeline E2E : `train_ppo --max-time-min 30` PASS, eval vs ref produit chiffre **DONE**
- [ ] zappy_ai libtorch charge .pt, joue sur serveur **DONE**
- [ ] CI verte sur tous les jobs (matrix Ubuntu+Fedora × gcc+clang) **DONE**

Si tout ✅ → **M3 PASSÉ**, S3 peut démarrer les bonus.
