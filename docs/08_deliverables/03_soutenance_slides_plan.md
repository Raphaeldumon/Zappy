# 03 — Plan slides soutenance

**Format** : 22 slides, durée ~12-15 min de slides + 7-10 min démo + Q&A.

## Slide 1 — Titre
- Logo Epitech
- **ZAPPY**
- Sous-titre : "G-YEP-400 · Promotion <année>"
- Noms équipe (6 personnes)
- Date soutenance 19 juin 2026

## Slide 2 — Le projet
- 3 binaires : server / GUI / AI
- Stack : C++17/20, Vulkan 1.3, Python+RLlib+libtorch
- Objectif : monde torus, ressources, élévation, victoire = 6 joueurs lvl 8

## Slide 3 — L'équipe et rôles
- 6 photos / 6 rôles
- 2 server / 2 GUI / 2 AI
- Briefly mentionner Lead par pôle

## Slide 4 — Architecture globale
- Diagramme : `zappy_server` <-> `zappy_gui` <-> `zappy_ai` (TCP)
- Offline : `libzappy_sim` → pybind11 → RLlib → `model.pt` → libtorch dans `zappy_ai`
- Repo monorepo

## Slide 5 — Serveur (1/2)
- C++17/20, single thread, asio + poll, conforme sujet
- State machine déterministe, time loop event-driven
- 100% protocole AI + GUI conformes

## Slide 6 — Serveur (2/2) — Bonus
- Admin/spectator socket avec token auth
- Persistance snapshot JSON + hot-reload config
- Recorder `.zrec` (replay binaire)
- Prometheus exporter + Grafana dashboard

## Slide 7 — GUI Vulkan (1/3) — Architecture
- Vulkan 1.3 dynamic rendering (no legacy render pass)
- Bindless descriptor indexing (1 set géant)
- Compute particles GPU
- Frame graph orchestrant les passes

## Slide 8 — GUI Vulkan (2/3) — Visuels
- Skybox étoiles + nebula procedurals (FBM)
- Atmosphere scattering Rayleigh+Mie
- Post-FX : SSAO + TAA + Bloom + Tonemap ACES
- (Si applicable) Ray tracing pour reflets

## Slide 9 — GUI Vulkan (3/3) — UX
- 4 vues : 3D / 2D planisphère / Split / Top-down
- 3 caméras : free / follow / top-down
- HUD ImGui complet, debug F3
- Replay reader avec timeline scrub
- Audio (musique + SFX miniaudio)

## Slide 10 — IA (1/3) — Approche
- Pas une seule IA mais 2 : RL trained (libtorch C++ inference) + rule-based FSM fallback
- Pourquoi ? Robustesse — si modèle KO, rule-based prend le relais

## Slide 11 — IA (2/3) — Training pipeline
- Simulateur turbo : `libzappy_sim` C++ exposé via pybind11
- `PettingZoo ParallelEnv` multi-agent
- RLlib PPO + curriculum (4 stages) + self-play + ELO
- W&B + Streamlit dashboards
- 100k samples/sec sur machine 64GB+RTX

## Slide 12 — IA (3/3) — Résultats
- Best checkpoint exporté en TorchScript
- **Win rate vs zappy_ref : XX% sur 100 matches** (chiffre réel à insérer)
- Broadcast codé team (magic XOR + payload b64) → coordination intra-team
- Démo : `python -m zappy_train.training.eval --opponent ref`

## Slide 13 — Simulateur
- Refactor server `core/` + `runtime/`
- `libzappy_core` shared entre runtime et sim
- **Conformance test bit-à-bit** : impossible que sim et runtime divergent
- Performance : 1M+ steps/sec single env

## Slide 14 — Qualité et tests
- CI matrix Ubuntu+Fedora × gcc+clang
- Pre-commit : clang-format, clang-tidy, cppcheck, ruff, mypy, glslangValidator
- Coverage : 70% C++ core, 80% Python
- 20+ scénarios YAML d'intégration AI↔server
- Conformance sim/runtime + smoke GUI

## Slide 15 — DevOps
- GitFlow strict (main/develop/feature/release/hotfix)
- Branch protection + 2 reviews + CODEOWNERS
- Conventional Commits + commitlint
- ADRs (~20) : chaque décision tracée
- Docker + devcontainer pour 0-friction onboarding
- MkDocs déployé GitHub Pages (en français)

## Slide 16 — Métriques de perf atteintes
Tableau :
| Cible | Atteint |
|-------|---------|
| FPS GUI 4K | 60+ |
| Server ticks/sec | 500+ |
| Sim samples/sec | 100k+ |
| Inférence libtorch | <2 ms |
| Coverage C++ core | 70%+ |
| Coverage Python | 80%+ |

## Slide 17 — Quelques choix techniques notables
- Dynamic rendering vs render pass (ADR-002)
- pybind11 pour bridger sim↔training (ADR-003)
- RLlib multi-agent vs SB3 (ADR-004)
- libtorch C++ inference vs ONNX (ADR-006)
- Broadcast codé team (ADR-008)

## Slide 18 — Challenges rencontrés
- Vulkan RT fallback strategy (ADR-007)
- Convergence RL multi-agent → curriculum + dense reward
- Sim/runtime drift → conformance test bit-à-bit
- (Autre challenge réel rencontré)

## Slide 19 — Démo live (transition)
- Logo / image
- "Place à la démo !"

## Slide 20 — Métriques équipe (post-démo)
- ~XX PRs mergées
- ~XX commits
- ~XX ADRs
- ~XX tests
- Coverage finale
- Lignes de code par pôle

## Slide 21 — Et ensuite ?
- Améliorations possibles : RT full pipeline, plus de curriculum, support Windows/Mac
- Open source ? (si décidé après projet)
- Apprentissages clés équipe

## Slide 22 — Merci + Q&A
- Photo équipe
- "Merci, questions ?"
- Liens : repo GitHub, docs MkDocs, vidéo démo

---

## Notes de présentation

### Qui parle quand

| Slide | Qui présente |
|-------|--------------|
| 1-3 | P6 (intro générale) |
| 4 | P1 (architecture overview) |
| 5-6 | P1 + P2 (serveur) |
| 7-9 | P3 + P4 (GUI) |
| 10-12 | P5 (IA) |
| 13 | P1 + P6 (simulateur) |
| 14 | P6 (qualité) |
| 15 | P6 (DevOps) |
| 16 | P6 (perf) |
| 17 | Lead pôle concerné par ADR |
| 18 | Lead pôle concerné |
| 19 | P6 (transition démo) |
| 20-21 | P6 (clôture) |
| 22 | P6 (merci) |

### Style visuel
- Thème dark moderne (cohérent avec Vulkan/tech)
- Police monospace pour code
- Couleurs : noir / bleu Epitech / accent vert
- Captures d'écran de la GUI + Grafana + W&B dashboards (slides 8, 9, 12)
- Diagrammes architecture (slides 4, 7, 11, 13) — Mermaid exporté en SVG

### Outils
- Slides : Slidev (Markdown → HTML+JS) ou Reveal.js ou Keynote
- Export PDF pour backup

### Préparation
- Slides finalisés mardi 16 juin
- Review collective mercredi 17 juin matin
- Répétition speech mercredi 17 juin AM + jeudi 18 juin AM
- Chacun memorize sa portion mais slides drivent

### Backup
- Slides PDF sur 2 USB
- Slides cloud (Google Drive partagé)
- Vidéo démo locale ET cloud
