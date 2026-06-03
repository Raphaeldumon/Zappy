# P1 — Léa, Server Lead — Calendrier 20 jours

**Mission rappel** : cœur de la logique serveur — `WorldState`, `EventScheduler`, game rules, persistance, hot-reload.

**Zones owned** : `server/core/**`, `tests/integration_yaml/**` (co-owner), `docs/01_architecture/02_server.md`, `docs/01_architecture/05_simulator.md`.

---

## S1 — Foundations

### D1 — lun 25/05
- Initialiser `server/CMakeLists.txt`, dossiers `core/` et `runtime/`
- `server/runtime/main.cpp` parse `--help` via CLI11
- Headers vides `core/{world_state,tile,player,egg,resource,event_scheduler,game_rules}.hpp`
- **DoD** : `./zappy_server --help` affiche usage, `make` produit le binaire
- Dépendances : aucune
- Bloque : P6 (a besoin du squelette pour split core/runtime), P2 (idem)

### D2 — mar 26/05
- Définir POD `Tile`, `Player`, `Egg`, `Resource`
- `WorldState::WorldState(int w, int h)` + `at(x, y)` avec wrap toroidal
- 1er test Catch2 `test_world_state_wrap.cpp`
- Draft ADR-001 (langage C++17/20)
- **DoD** : `ctest -R world_state` PASS
- Dépendances : aucune
- Bloque : P6 (binding pybind)

### D3 — mer 27/05 — M1 contrats
- Sync cross-team protocole 10h-11h
- Interface `EventScheduler` + 5 tests Catch2 ordering
- Stubs `WorldState::move/turn/take/set` (mutent l'état)
- ADR-005 (.zrec format) coordination avec P2
- **DoD** : `EventScheduler` interface stable, ADRs 001 + 005 Accepted
- Dépendances : aucune
- Bloque : P2 (dépend des stubs ops pour parser dispatch)

### D4 — jeu 28/05
- `WorldState::look(player_id)` complet avec vision cone + wrap
- 8 tests vision (level 1..8)
- Documenter algo dans `02_server.md`
- **DoD** : tests vision PASS, code reviewed
- Dépendances : `Tile`, `Player`, orientation
- Bloque : P6 (conformance sim sur look)

### D5 — ven 29/05 — M2 socle
- Coverage `server/core/` ≥ 70%
- Doxygen `WorldState` API publique
- Demo Friday : sync sur le serveur skeleton
- Retro
- **DoD** : M2 PASS, ADRs Accepted
- Dépendances : tous tests CI verts

---

## S2 — Core MVP

### D6 — lun 1/06
- Sprint planning
- Implémenter `take_object`, `set_object`, `move_forward`, `turn_left/right`
- Implémenter `respawn_resources` avec densités sujet
- Tests par op (`test_world_basic_ops.cpp`)
- **DoD** : 5 ops PASS, conformance sim PASS sur scénarios basiques
- Dépendances : `Tile`, `Player`
- Bloque : P2 (protocol dispatch sur ces ops), P6 (sim test conformance)

### D7 — mar 2/06
- Implémenter `Incantation` (300/f, vérif debut+fin, table sujet)
- Implémenter `Fork` (42/f, ajoute egg)
- Implémenter `Eject` (push from tile + destroy eggs)
- Tests Catch2 par op, scenarios des 7 niveaux d'élévation
- **DoD** : `test_incantation.cpp` couvre les 7 niveaux
- Bloque : P2 (protocole dispatch), P5 (env Python)

### D8 — mer 3/06
- Persistance JSON `nlohmann/json` : snapshot + reload
- SIGUSR1 → dump `snapshot-*.json`
- SIGUSR2 → reload config
- Tests `test_persistence.cpp` (snapshot → reload → état identique)
- **DoD** : `kill -USR1` produit fichier, reload OK
- Bloque : P2 (admin reload utilise cette logique)

### D9 — jeu 4/06
- `game_rules::check_elevation_eligibility()` table sujet
- Hot-reload config (densités, f, lifespan) via SIGUSR2
- Coverage push vers 70%
- **DoD** : table 7 niveaux PASS, hot-reload OK
- Bloque : P2 (utilise les rules)

### D10 — ven 5/06 — M3 MVP
- Buffer / fix
- Test integration : 5 min run sans crash, élévations qui réussissent
- Coverage report
- Demo Friday + retro
- **DoD** : M3 PASS, serveur stable

---

## S3 — Bonus + intégration

### D11 — lun 8/06
- Sprint planning
- Hot-reload via commande admin `reload-config` (côté server, P2 fait socket)
- Refactor découpler config statique / runtime
- **DoD** : reload densité applique au prochain respawn
- Bloque : P2 (admin protocol)

### D12 — mar 9/06
- Stress test server (4t × 6p, 50x50, 10 min)
- Profile CPU + RSS, fix leaks
- **DoD** : RSS < 300 MB, 0 crash 10 min
- Dépendances : prom exporter de P2 pour visualiser

### D13 — mer 10/06
- Death lifecycle (player meurt → `pdi` + libère slot)
- Egg hatching : connexion sur slot libre → choose random egg
- **DoD** : lifecycle complet, GUI cohérent

### D14 — jeu 11/06
- Doxygen complet `server/core/`
- Buffer / bug fixing
- **DoD** : `make docs` clean

### D15 — ven 12/06 — M4
- Coverage > 75%
- Demo Friday + retro
- **DoD** : M4 PASS

---

## S4 — Polish + soutenance

### D16 — lun 15/06
- Sprint planning S4
- Stress test final (200x200, 4 teams × 6, f=500, 30 min)
- Profile + optimize hot paths
- **DoD** : 500 ticks/sec atteint, RSS < 300 MB

### D17 — mar 16/06
- Bug fixing prio P0/P1
- Coverage > 75%
- **DoD** : 0 P0/P1 ouverts

### D18 — mer 17/06 — Code freeze 18h
- Matin : derniers bug fixes
- 14h-17h : Répétition complète E2E
- Démo serveur + AI rôle dans soutenance
- **DoD** : tag `v1.0.0-rc1`

### D19 — jeu 18/06
- Doc finale `02_server.md`, `06_protocols.md`
- README server + exemples CLI
- Slides "Architecture serveur" avec P6
- 2ème répétition
- **DoD** : doc et slides reviewed

### D20 — ven 19/06 — Soutenance
- 9h-11h : préparation finale
- Soutenance : rôle démo serveur + AI live + Q&A protocole/réseau cross avec P2
- **DoD** : soutenance livrée 🎉

---

## Sync points clés avec autres pôles

| Quand | Avec qui | Sujet |
|-------|----------|-------|
| D3 | P2, P3, P4, P5, P6 | Sync protocole (M1) |
| D3 | P6 | Refactor core/runtime split |
| D4-D5 | P6 | Conformance sim test setup |
| D8 | P2 | Persistance / reload coordination |
| Chaque PR `core/` | P6 | Co-review obligatoire (conformance) |
| Chaque PR `protocol_*` | P2 | Coordination |

## Outils / setup spécifiques

- `gcovr` installé pour coverage
- `valgrind` pour debug rare
- `perf` pour profiling
- IDE recommandé : VS Code + clangd
- Notebook personnel pour notes design (pas committé)

## Auto-évaluation fin de chaque sprint

| Sprint | Score perso (1-5) | Notes |
|--------|--------------------|-------|
| S1 | __/5 | |
| S2 | __/5 | |
| S3 | __/5 | |
| S4 | __/5 | |
