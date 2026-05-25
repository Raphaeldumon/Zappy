# ZAPPY — PLAN DE PROJET 6 PERSONNES / 4 SEMAINES

**Project:** G-YEP-400 Zappy
**Période:** lundi 25 mai 2026 → vendredi 19 juin 2026 (20 jours ouvrés)
**Équipe:** 6 personnes — 2 server / 2 GUI / 2 AI
**Soutenance:** vendredi 19 juin 2026
**Stack:** C++17/20 (server + GUI) · Vulkan 1.3 (GUI) · Python + PyTorch + RLlib (training AI) · libtorch C++ (inférence AI)

---

## Index de la documentation

```
PLAN.md                            ← Ce fichier (point d'entrée)
docs/
├── 00_executive_summary.md        ← Résumé exécutif : objectifs, stack, équipe, KPIs
├── 01_architecture/
│   ├── 01_overview.md             ← Vue d'ensemble système (4 composants)
│   ├── 02_server.md               ← Architecture serveur C++ (poll, état monde, time loop)
│   ├── 03_gui_vulkan.md           ← Architecture GUI Vulkan 1.3 (frame graph, RT, post-FX)
│   ├── 04_ai_rl.md                ← Architecture IA RL (PettingZoo, RLlib, libtorch)
│   ├── 05_simulator.md            ← Simulateur turbo (server headless pour training)
│   ├── 06_protocols.md            ← Protocoles AI↔server, GUI↔server, broadcast codé team
│   └── 07_monorepo_structure.md   ← Arborescence du repo + conventions de nommage
├── 02_team/
│   ├── 01_roles.md                ← 6 rôles détaillés (P1..P6) : missions, skills, livrables
│   ├── 02_raci.md                 ← Matrice RACI sur toutes les grandes tâches
│   └── 03_responsibilities.md     ← Ownership zones de code, on-call rotation, escalation
├── 03_process/
│   ├── 01_gitflow.md              ← GitFlow strict (main/develop/feature/release/hotfix)
│   ├── 02_ci_cd.md                ← Pipeline GitHub Actions (build, lint, tests, bench)
│   ├── 03_pr_rules.md             ← Template PR, checklist, 2 reviews, CODEOWNERS
│   ├── 04_adrs.md                 ← Architecture Decision Records (template + workflow)
│   ├── 05_sprint_cadence.md       ← Standup quotidien, retro, planning, sprint 1 sem
│   └── 06_precommit.md            ← Hooks pre-commit (clang-format/tidy/cppcheck/ruff)
├── 04_quality/
│   ├── 01_definition_of_done.md   ← DoD par type de tâche (feature, bug, perf, doc)
│   ├── 02_testing_strategy.md     ← Tests unit C++, integration AI↔server, pytest+mutmut, bench
│   ├── 03_code_standards.md       ← C++ style guide, Python style guide, nommage, headers
│   └── 04_performance_targets.md  ← 60 FPS 4K, server 500 ticks/s, training 100k steps/s
├── 05_risks/
│   ├── 01_top_10_risks.md         ← Top 10 risques projet identifiés
│   └── 02_mitigation_plan.md      ← Plan de mitigation détaillé par risque
├── 06_calendar/
│   ├── 00_overview.md             ← Gantt textuel + jalons + sprints
│   └── sprints/
│       ├── sprint_1_w1_foundations.md   ← 25-29 mai : socle commun obligatoire
│       ├── sprint_2_w2_core.md          ← 1-5 juin : MVP de chaque pole
│       ├── sprint_3_w3_features.md      ← 8-12 juin : features bonus + intégration
│       └── sprint_4_w4_polish.md        ← 15-19 juin : polish + soutenance
├── 07_calendar_per_person/
│   ├── P1_server_lead.md          ← Léa — Server Lead (état du monde, time-loop, persistance)
│   ├── P2_server_dev.md           ← Marc — Server Dev (poll/asio, protocole, admin, Prometheus)
│   ├── P3_gui_lead_vulkan.md      ← Sami — GUI Lead (renderer Vulkan, RT, bindless, frame graph)
│   ├── P4_gui_dev_ux.md           ← Inès — GUI Dev (scène 2D/3D, HUD ImGui, replay, audio)
│   ├── P5_ai_lead_rl.md           ← Théo — AI Lead RL (env multi-agent, RLlib, training)
│   └── P6_ai_dev_sim_devops.md    ← Yanis — AI Dev (sim turbo, libtorch C++, DevOps/CI)
├── 08_deliverables/
│   ├── 01_final_deliverables.md   ← Checklist livrable final J+25
│   ├── 02_demo_script.md          ← Script démo live soutenance (10 min)
│   ├── 03_soutenance_slides_plan.md  ← Plan slides (20-25 slides)
│   └── 04_video_demo_plan.md      ← Storyboard vidéo démo (5 min)
└── 09_appendix/
    ├── 01_useful_commands.md      ← Cheatsheet commandes (make, docker, gh, etc.)
    ├── 02_dependencies.md         ← Liste exhaustive deps + versions épinglées
    └── 03_references.md           ← Liens utiles : Vulkan, RLlib, PDFs sujet
```

---

## Calendrier de haut niveau

| Sprint | Semaine | Dates | Thème | Output cible |
|:------:|:-------:|:------|:------|:-------------|
| S1 | W1 | 25-29 mai | **Foundations** : socle technique commun, CI, contrats | Repo prêt, 3 binaires "hello", CI verte, devcontainer, contrats protocole figés |
| S2 | W2 | 1-5 juin  | **Core features** : MVP fonctionnel de chaque pôle | Server v1, GUI v1 (2D + 3D base), AI rule-based v1, simulateur turbo |
| S3 | W3 | 8-12 juin | **Bonus + intégration** : features riches, training RL, intégration cross-team | Training RL en cours, GUI shaders custom, broadcast codé, admin/spectator |
| S4 | W4 | 15-19 juin | **Polish + démo** : ray tracing, post-FX, freeze, répé, soutenance | Mode démo, vidéo, slides, soutenance Ven 19/06 |

Jalons clés (milestones) :

- **M0 — Day 1 (lun 25 mai)** : kickoff, repo prêt, devcontainer fonctionnel chez les 6
- **M1 — Day 3 (mer 27 mai)** : contrats protocole figés (struct C++ + sérialisation)
- **M2 — End of S1 (ven 29 mai)** : socle livré, parallélisation possible
- **M3 — End of S2 (ven 5 juin)** : 3 binaires MVP qui jouent ensemble une partie complète
- **M4 — End of S3 (ven 12 juin)** : training RL convergent, GUI bonus terminés, intégration validée
- **M5 — Code freeze (mer 17 juin)** : plus de feature, uniquement bugfix + polish
- **M6 — Soutenance (ven 19 juin)** : démo live + Q&A

---

## Pilotage de lecture

- **Tu es un dev** → commence par `docs/07_calendar_per_person/PX_*.md` correspondant à ton rôle.
- **Tu es le tech-lead / chef projet** → lis `docs/00_executive_summary.md` puis `docs/06_calendar/00_overview.md`.
- **Tu prépares la soutenance** → `docs/08_deliverables/*`.
- **Tu cherches une décision technique** → `docs/01_architecture/*` + `docs/03_process/04_adrs.md`.
- **Tu vas écrire/reviewer une PR** → `docs/03_process/03_pr_rules.md` + `docs/04_quality/01_definition_of_done.md`.

---

## Principes directeurs (non négociables)

1. **Socle d'abord** : la semaine 1 est consacrée à la création du socle (CI, devcontainer, contrats, mocks). Personne ne commence ses features avant la fin de S1.
2. **Le contrat protocole est sacré** : aucune modification du contrat protocole sans ADR + accord des 3 pôles.
3. **CI verte = condition de merge** : pas de "je merge et je fix après". Si CI rouge, on revert.
4. **2 reviews obligatoires** dont au moins 1 du CODEOWNERS du domaine impacté.
5. **Conventional Commits** strict + commitlint dans la CI.
6. **Chaque modif d'architecture = ADR** dans `docs/03_process/04_adrs/`. Pas d'ADR = pas de merge.
7. **Tests obligatoires** : pas de PR sans tests (unit ou integration) au moins pour la partie modifiée.
8. **Definition of Done** appliquée systématiquement (voir `docs/04_quality/01_definition_of_done.md`).
9. **Standup 9h30 chaque jour** (15 min), **retro vendredi 17h** (45 min), **planning lundi 9h30** (1h).
10. **Documentation en français**, code/commits/PR en anglais.
