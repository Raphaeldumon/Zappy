# 01 — Livrables finaux (J+25)

## Checklist projet à 100%

À J+25, on considère le projet **terminé** si TOUTES les cases sont cochées :

### Code et binaires

- [ ] `make` à la racine produit `zappy_server`, `zappy_gui`, `zappy_ai` sans warning
- [ ] `make clean`, `make fclean`, `make re`, `make tests_run` fonctionnent
- [ ] Le `Makefile` est aux normes Epitech (`.PHONY`, variables CFLAGS/LDFLAGS visibles)
- [ ] CI verte sur le tag `v1.0.0` (matrix Ubuntu+Fedora × gcc+clang)
- [ ] Tag `v1.0.0` annoté + push
- [ ] GitHub Release publiée avec `zappy-v1.0.0.tar.gz`

### Fonctionnel

- [ ] `./zappy_server -p 4242 -x 50 -y 50 -n red blue -c 6 -f 100` + `./zappy_gui -p 4242 -h localhost` + 4×`./zappy_ai -p 4242 -n red -h localhost` → partie complète sans crash jusqu'à game over (ou stop manuel après >5 min)
- [ ] Toutes les commandes AI du sujet implémentées et conformes
- [ ] Tous les events GUI du protocole implémentés et conformes
- [ ] Handshake AI + GRAPHIC conforme sujet
- [ ] Vision cone correcte aux 8 niveaux + wrap toroidal
- [ ] Élévation table sujet exacte (1→2 .. 7→8)
- [ ] Fork (42/f) → egg
- [ ] Eject pousse joueurs + détruit eggs
- [ ] Broadcast directionality sphérique shortest path
- [ ] Death cycle (food=0 → meurt → libère slot)
- [ ] Egg hatching aléatoire

### GUI

- [ ] Rendu 3D **planète torus dans l'espace** avec atmosphère scattering
- [ ] Skybox étoiles + nebula procedurals
- [ ] Particles compute shader actives (incantation, mort)
- [ ] Post-FX : SSAO + TAA + Bloom + Tonemap ACES
- [ ] Ray tracing OK ou drop documenté (ADR-007)
- [ ] Rendu 2D **planisphère** (overlay rectangulaire torus déroulé)
- [ ] Bascule de vue : 2D / 3D / Split / Top-down
- [ ] 3 caméras : free orbit / follow player / top-down
- [ ] HUD ImGui : Teams list, Player Info, Timeline, Speed control
- [ ] Debug panel F3 : frametime, GPU mem, packets, draw calls
- [ ] Menu principal : Connect / Replay / Settings / Quit
- [ ] Audio : musique ambient + SFX (incantation, mort, broadcast, fork)
- [ ] Replay reader : load `.zrec` + timeline scrub + speed control 0.25x..16x

### IA

- [ ] Mode rule-based FSM solide (survit + élève)
- [ ] Mode inférence libtorch C++ avec `model.pt`
- [ ] Broadcast codé team (magic byte XOR + payload b64)
- [ ] Fallback automatique inférence → rule-based si modèle absent
- [ ] Reconnexion auto si serveur down (resilience)

### Simulateur / Training

- [ ] `libzappy_sim` lib statique (refactor core/runtime fait)
- [ ] Binding pybind11 `zappy_sim` Python utilisable
- [ ] Conformance test sim vs runtime PASS sur 5+ scénarios
- [ ] Env PettingZoo `ParallelEnv` wrappant `zappy_sim`
- [ ] Curriculum learning 4 stages traversés
- [ ] Self-play tournament + ELO tracking opérationnels
- [ ] Training final exporté en TorchScript `model.pt`
- [ ] **Eval : win rate ≥ 70% vs `zappy_ref` sur 100 matches** (ou drop documenté si pas atteint)
- [ ] `eval_report.md` dans `models/current/`

### Bonus serveur

- [ ] Admin socket (port `-p+1000`) + token auth
- [ ] Spectator mode (`TEAM_NAME=SPECTATOR`)
- [ ] Hot-reload config CLI (admin command)
- [ ] Persistance snapshot disque (SIGUSR1 + admin)
- [ ] Recorder `.zrec` (option `--record`)
- [ ] Prometheus exporter `/metrics:9090`
- [ ] Grafana dashboard JSON fourni

### Qualité

- [ ] Couverture tests unit C++ core ≥ 70%
- [ ] Couverture tests Python ≥ 80%
- [ ] 20+ scénarios YAML d'intégration AI↔server PASS
- [ ] Tests conformance sim vs runtime PASS
- [ ] Benchmarks documentés : FPS GUI, ticks/sec, steps/sec, samples/sec
- [ ] CI verte
- [ ] Pre-commit hooks fonctionnels

### Documentation

- [ ] MkDocs site déployé sur GitHub Pages (en français)
- [ ] Toutes les pages du plan rendues
- [ ] Doxygen API C++ généré et intégré
- [ ] README projet à jour (overview, install, usage)
- [ ] README par sous-projet (`server/`, `gui/`, `ai_cpp/`, `ai_python/`)
- [ ] CHANGELOG.md à jour
- [ ] ADRs complètes pour décisions structurantes (20+)
- [ ] Diagrammes (mermaid ou ASCII) dans `docs/01_architecture/`

### Livrables soutenance

- [ ] Slides finaux (PDF + source)
- [ ] Vidéo démo 5 min montée (mp4 1080p+)
- [ ] Script démo live (`docs/08_deliverables/02_demo_script.md`)
- [ ] Backup plan : replay `.zrec` + tar.gz sur USB
- [ ] 2 répétitions complètes effectuées (J-2 et J-1)

### Performance (cibles validées)

- [ ] GUI ≥ 60 FPS @ 4K, map 200x200, 24 players
- [ ] Server ≥ 500 ticks/sec, map 200x200
- [ ] Sim ≥ 100 000 samples/sec aggregate sur machine training
- [ ] Inférence libtorch < 2 ms forward
- [ ] Démarrage GUI < 3 sec, server < 1 sec

## Livrables détaillés

### Code

- Repo `G-YEP-400-RUN-4-1-zappy-3` sur GitHub
- Branche `main` à jour, tag `v1.0.0`
- Tarball release `zappy-v1.0.0.tar.gz` (~50-200 MB selon assets)

### Documentation

- Site MkDocs : `https://<org>.github.io/G-YEP-400-RUN-4-1-zappy-3/`
- Doxygen API : `/api_reference/` sous-section
- README principal en français
- Tous les ADRs dans `docs/adrs/`

### Modèle IA

- `models/current/model.pt` (TorchScript)
- `models/current/eval_report.md` (statistiques win rate)
- `models/current/training_config.yaml` (config reproductible)
- `models/manifest.json` (hashes SHA256, versions)

### Démo

- Vidéo `assets/demo_v1.0.0.mp4` (5 min, 1080p+)
- Slides `docs/08_deliverables/slides_soutenance_v1.0.0.pdf` (+source)
- Replay sample `assets/sample_replay_v1.0.0.zrec` (3 min de gameplay)

### Logs / artefacts

- Run W&B référencé dans `eval_report.md`
- Tensorboard logs `models/runs/<run_id>/`
- Captures Grafana dans `docs/grafana_screenshots/`

## Validation finale (jeudi 18 juin)

Procédure de validation finale, exécutée par 2 personnes (binôme P6 + 1 autre) :

1. `git clone` du tag `v1.0.0` dans un dossier vierge
2. `docker compose up devcontainer` ou install manuel selon doc
3. `make` à la racine
4. Vérification présence des 3 binaires + permissions
5. Lance partie complète selon procedure de `docs/09_appendix/01_useful_commands.md`
6. Démarrage GUI, vérification rendu
7. Lance script eval `python -m zappy_train.training.eval --opponent ref --matches 10`
8. Lecture replay
9. Mode admin (pause, set f, kill, spawn)
10. Pendant tout ce temps, monitoring via Grafana

Si **toutes** les étapes passent → 🎉 projet à 100%.
Si une étape échoue → ouvrir issue P0 critique, vendredi matin résolution prioritaire.
