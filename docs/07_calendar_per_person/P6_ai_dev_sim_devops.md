# P6 — Yanis, AI Dev — Simulateur & DevOps — Calendrier 20 jours

**Mission rappel** : pybind11 binding `libzappy_sim`, binaire `zappy_ai` C++ libtorch, infra CI/CD, Docker, devcontainer, GitHub Pages docs.

**Zones owned** : `ai_cpp/**`, `sim_python/**`, `docker/**`, `.github/workflows/**`, `.pre-commit-config.yaml`, `tools/**` (co-owner), `docs/01_architecture/05_simulator.md` (co-owner).

---

## S1 — Foundations

### D1 — lun 25/05
- `docker/Dockerfile` base Ubuntu 22.04 (Vulkan SDK 1.3, gcc-13, clang-17, Python 3.11, CUDA runtime)
- `docker/Dockerfile.training` extension (PyTorch + ray + RLlib)
- `docker/.devcontainer/devcontainer.json` VS Code
- `Makefile` root wrapper Epitech (`make`, `make re`, `make clean`, `make fclean`, `make tests_run`)
- `CMakeLists.txt` root, options `ZAPPY_BUILD_TESTS`, `ZAPPY_BUILD_SIM`, `ZAPPY_HAS_RT`
- **DoD** : devcontainer ouvre, autre dev clone+docker compose+build OK
- Dépendances : aucune
- Bloque : tous les devs (env)

### D2 — mar 26/05
- `sim_python/CMakeLists.txt` avec pybind11
- `sim_python/src/zappy_sim_pybind.cpp` stub binding `Sim`
- `tools/format_all.sh` + `tools/clang_tidy_runner.sh`
- `.pre-commit-config.yaml` complet + run sur tout repo
- **DoD** : `import zappy_sim; sim = zappy_sim.Sim()` marche, pre-commit OK
- Bloque : aucun

### D3 — mer 27/05 — M1
- Sync protocole 10h-11h
- Refactor `server/` en `server/core/` + `server/runtime/` (avec P1)
- `libzappy_core` static lib + `zappy_server` link
- ADR-003 (pybind11 strategy)
- Bouchon binding `zappy_sim` qui appelle réellement `libzappy_core` (état vide)
- **DoD** : `libzappy_core.a` + `zappy_sim.so` + Python import OK
- Bloque : P5 (env utilise binding)

### D4 — jeu 28/05
- `tests/conformance_sim_vs_runtime/` skeleton, 1 scénario simple
- ADR-008 (broadcast codec) avec P2
- **DoD** : `make conformance` PASS sur scénario stub
- Bloque : aucun

### D5 — ven 29/05 — M2
- MkDocs Material setup, `mkdocs serve` local OK
- Doxygen Doxyfile intégration MkDocs
- Vérification CI nightly stub (workflow vide vert)
- Déploiement GitHub Pages
- Demo Friday + retro
- **DoD** : site MkDocs déployé public, M2 PASS

---

## S2 — Core MVP

### D6 — lun 1/06
- Sprint planning
- Compléter binding pybind11 `Sim::reset/step` qui appellent vraiment `WorldState`
- Setup release GIL
- Stress test 8 threads × 1000 steps
- **DoD** : `zappy_sim` produit états cohérents, conformance test "10 forwards" PASS
- Dépendances : ops de P1
- Bloque : P5 (env utilise vrai sim)

### D7 — mar 2/06
- Implémenter rule-based FSM dans `ai_cpp/src/policy_rule_based.cpp`
- Survie + élévation + coordination simple broadcasts
- Tests `test_fsm_survival.cpp`
- **DoD** : `zappy_ai` rule-based survit > 5 min
- Bloque : aucun

### D8 — mer 3/06
- Conformance test : ajouter 5 scenarios YAML (move, look, take, set, incantation echec)
- CI rouge si conformance fail
- ASan/UBSan en CI debug
- **DoD** : 5 scénarios PASS, CI bloque PRs qui break

### D9 — jeu 4/06
- Broadcast codec (ADR-008) : encode/decode payload + magic XOR team
- Tests `test_broadcast_codec.cpp` exhaustifs
- `zappy_ai` peut envoyer + recevoir broadcasts codés (debug log)
- **DoD** : 2 AIs same team comm OK, other team ignorent
- Bloque : aucun

### D10 — ven 5/06 — M3
- Buffer + fix CI flaky
- Vérifier `zappy_ai` libtorch charge .pt + forward + action
- Doc `04_ai_rl.md` mis à jour
- Demo Friday + retro
- **DoD** : inférence libtorch E2E, M3 PASS

---

## S3 — Bonus + intégration

### D11 — lun 8/06
- Sprint planning
- Compléter `zappy_ai` : intégration broadcast codé team
- Perf inférence libtorch : profile, viser < 2ms forward
- **DoD** : `zappy_ai` complet, 2 AIs coordonnent

### D12 — mar 9/06
- Grafana dashboard JSON `grafana/zappy_overview.json` (8 panels métriques)
- `docker-compose.yml` ajoute Prometheus + Grafana pour dev local
- **DoD** : `docker compose up grafana` montre dashboard live

### D13 — mer 10/06
- Audio engine miniaudio : load OGG music + WAV SFX
- API `audio.play_sfx("incantation")`, `audio.play_music("ambient.ogg")`
- Volumes configurables
- **DoD** : SFX joue sur events GUI, musique boucle

### D14 — jeu 11/06
- Performance tuning libtorch : batch inference si possible
- ASan 30 min sur `zappy_ai` → 0 leaks
- **DoD** : forward < 2 ms median, ASan clean

### D15 — ven 12/06 — M4
- Release internal `v0.3.0-rc1` packagée tar.gz
- Buffer
- Demo Friday + retro
- **DoD** : externe peut run le tar.gz, M4 PASS

---

## S4 — Polish + soutenance

### D16 — lun 15/06
- Sprint planning S4
- Release prep : version bump, CHANGELOG draft
- Validation `make` normes Epitech
- Trial Docker image release `ghcr.io/.../zappy:1.0.0-rc1`
- **DoD** : packaging prêt, externe peut docker pull

### D17 — mar 16/06
- Préparation slides soutenance (plan voir `08_deliverables/03_soutenance_slides_plan.md`)
- Script démo vidéo
- **DoD** : slides draft + storyboard reviewed

### D18 — mer 17/06 — Code freeze
- Matin : finalize slides + repérer salle soutenance + matos check
- 14h-17h : Répétition complète (rôle coordinateur)
- 18h : Code freeze, tag `v1.0.0-rc1`
- **DoD** : tag créé, branche release/1.0.0

### D19 — jeu 18/06
- MkDocs final, GitHub Pages déployé
- `CHANGELOG.md` final
- Tag `v1.0.0` officiel
- Release GitHub avec tar.gz
- Slides global compilées
- Liste matériel soutenance
- 2ème répétition (rôle présentateur principal)
- **DoD** : v1.0.0 publié, matos vérifié

### D20 — ven 19/06 — Soutenance
- 8h30 arrivée
- 9h-11h : préparation finale (lead)
- Soutenance : présentateur principal (transitions, Q&A infra/DevOps)
- Backup tech (fallback vidéo si crash)
- **DoD** : soutenance livrée 🎉

---

## Sync points clés

| Quand | Avec qui | Sujet |
|-------|----------|-------|
| D1 | Tous | Devcontainer / env setup |
| D3 | P1 | Refactor core/runtime |
| D3 | Tech-leads | M1 protocole |
| D5 | Tous | MkDocs setup |
| D6-D7 | P5 | Sim binding ready, FSM |
| D9 | P2 | Broadcast codec dans server |
| D12 | P2 | Métriques Prometheus dans Grafana |
| D16-D20 | Tous | Release + soutenance coord |

## Outils / setup

- Docker + docker-compose
- GitHub CLI (`gh`)
- VS Code + Dev Containers extension
- LibTorch 2.x (C++)
- ffmpeg (montage vidéo si besoin)
- Linux desktop pour démo soutenance

## Auto-évaluation

| Sprint | Score (1-5) | Notes |
|--------|-------------|-------|
| S1 | __ | |
| S2 | __ | |
| S3 | __ | |
| S4 | __ | |
