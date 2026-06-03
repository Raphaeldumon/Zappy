# 00 — Calendrier overview

## Période

- **Démarrage** : lundi 25 mai 2026
- **Soutenance** : vendredi 19 juin 2026
- **Rythme** : 5 jours / semaine (lundi → vendredi), weekends OFF
- **Total** : 20 jours ouvrés répartis en 4 sprints d'une semaine

## Gantt textuel des sprints

```
                     S1                 S2                 S3                 S4
                 25→29 mai           1→5 juin           8→12 juin          15→19 juin
                ━━━━━━━━━━━         ━━━━━━━━━━━         ━━━━━━━━━━━         ━━━━━━━━━━━

P1 Server Lead   ┃REFACTOR ┃        ┃WORLD V1 ┃        ┃BONUS PERS┃        ┃PERF + DEMO┃
                 ┃CORE/RT  ┃        ┃EVT SCHED┃        ┃HOTRELOAD ┃        ┃           ┃

P2 Server Net    ┃CI/REPO  ┃        ┃POLL/PRO ┃        ┃ADMIN/RECO┃        ┃PROMETHEUS ┃
                 ┃PROTO STB┃        ┃PARSER   ┃        ┃ZREC      ┃        ┃FINAL      ┃

P3 GUI Lead      ┃VK TRIANG┃        ┃FRAME GR ┃        ┃PARTICLES ┃        ┃RT + POST  ┃
                 ┃VMA BASE ┃        ┃G-BUFFER ┃        ┃ATMOS     ┃        ┃FX FINAL   ┃

P4 GUI Dev       ┃SOCLE+UI ┃        ┃SCENE 2D ┃        ┃HUD/REPLAY┃        ┃AUDIO+DEMO ┃
                 ┃MOCK NET ┃        ┃3D BASE  ┃        ┃CAMERAS   ┃        ┃CINEMATIC  ┃

P5 AI Lead       ┃ENV STUB ┃        ┃PPO BOOT ┃        ┃CURRIC.   ┃        ┃EXPORT     ┃
                 ┃REWARD V0┃        ┃TRAIN V1 ┃        ┃SELFPLAY  ┃        ┃EVAL FINAL ┃

P6 AI/DevOps     ┃INFRA CI ┃        ┃SIM CORE ┃        ┃LIBTORCH  ┃        ┃REL + DEMO ┃
                 ┃DOCKER   ┃        ┃PYBIND   ┃        ┃BROADCAST ┃        ┃PIPELINE   ┃

Milestones         M0    M1   M2      M3                  M4               M5    M6
                 ↑D1   ↑D3  ↑D5  ↑D6→D10                ↑D11→D15        ↑D17  ↑D20
                 KO  PROTO  SOC  CORE                   BONUS           FRZ   SOUT
                 FF  FIGÉ   LE   MVP                    DONE            CODE
```

## Jalons (milestones)

| Code | Date | Description |
|------|------|-------------|
| **M0** | lun 25 mai (D1) | Kickoff, repo prêt, devcontainer fonctionnel chez les 6 |
| **M1** | mer 27 mai (D3) | Contrats protocole figés (struct C++ + sérialisation), ADRs 001-008 Accepted |
| **M2** | ven 29 mai (D5) | Socle livré : 3 binaires "hello", CI verte, sim core squelette, parallélisation possible |
| **M3** | ven 5 juin (D10) | MVP : 3 binaires fonctionnels jouent une partie complète AI rule-based, GUI base 2D+3D, server complet |
| **M4** | ven 12 juin (D15) | Bonus terminés : training RL convergent, GUI shaders custom, broadcast codé, admin/spectator, replay |
| **M5** | mer 17 juin (D18) | **Code freeze** : plus de feature, uniquement bugfix + polish + répétition |
| **M6** | ven 19 juin (D20) | **Soutenance** : démo live + Q&A jury |

## Calendrier des sprints — liens

- [Sprint 1 (W1) — Foundations](sprints/sprint_1_w1_foundations.md)
- [Sprint 2 (W2) — Core MVP](sprints/sprint_2_w2_core.md)
- [Sprint 3 (W3) — Bonus + intégration](sprints/sprint_3_w3_features.md)
- [Sprint 4 (W4) — Polish + soutenance](sprints/sprint_4_w4_polish.md)

## Calendrier par personne — liens

- [P1 — Léa](../07_calendar_per_person/P1_server_lead.md)
- [P2 — Marc](../07_calendar_per_person/P2_server_dev.md)
- [P3 — Sami](../07_calendar_per_person/P3_gui_lead_vulkan.md)
- [P4 — Inès](../07_calendar_per_person/P4_gui_dev_ux.md)
- [P5 — Théo](../07_calendar_per_person/P5_ai_lead_rl.md)
- [P6 — Yanis](../07_calendar_per_person/P6_ai_dev_sim_devops.md)

## Cadence rappel

| Quand | Évènement |
|-------|-----------|
| Lundi 9h30-10h30 | Sprint planning |
| Mardi → vendredi 9h30 | Standup 15 min |
| Vendredi 16h00-17h00 | Demo Friday |
| Vendredi 17h00-17h45 | Retro |

## Conventions des fiches sprint et per-person

Chaque fiche sprint contient :
- Objectifs du sprint
- Jours `D1..D5` détaillés avec, par personne :
  - **Tâche du jour**
  - **Inputs** nécessaires (livré par qui)
  - **Outputs** (livré pour qui)
  - **Critère d'acceptance**
  - **Risques / blockers**
- Fin de sprint : checkpoint + DoD vérification

Chaque fiche per-person contient :
- Synthèse de mission rappel
- 20 jours détaillés, focus tâches **propres** au dev
- Dépendances avec les autres
- Auto-évaluation de fin de sprint
