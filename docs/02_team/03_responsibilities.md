# 03 — Responsabilités opérationnelles

## Ownership zones de code (CODEOWNERS)

Fichier `.github/CODEOWNERS` à créer :

```
# Format: <path-glob>  <@reviewer1> <@reviewer2>...

# Server
/server/core/                @P1
/server/runtime/network_*    @P2
/server/runtime/protocol_*   @P2 @P1
/server/runtime/recorder_*   @P2
/server/runtime/metrics_*    @P2
/server/tests/               @P1 @P2

# GUI
/gui/include/zappy/gui/renderer/   @P3
/gui/src/renderer/                 @P3
/gui/shaders/                      @P3 @P4
/gui/include/zappy/gui/scene/      @P4 @P3
/gui/include/zappy/gui/ui/         @P4
/gui/include/zappy/gui/audio/      @P4
/gui/include/zappy/gui/input/      @P4
/gui/include/zappy/gui/net/        @P4
/gui/assets/                       @P4
/gui/tests/                        @P4 @P3

# AI / sim
/ai_cpp/                     @P6 @P5
/ai_python/                  @P5 @P6
/sim_python/                 @P6 @P1
/tests/conformance_*         @P6 @P1

# Infra / shared
/.github/                    @P6
/docker/                     @P6
/cmake/                      @P6 @P1
/tools/                      @P6 @P4
/Makefile                    @P6
/CMakeLists.txt              @P6 @P1
/.pre-commit-config.yaml     @P6

# Docs
/docs/01_architecture/02_server.md    @P1
/docs/01_architecture/03_gui_vulkan.md @P3 @P4
/docs/01_architecture/04_ai_rl.md     @P5
/docs/01_architecture/05_simulator.md @P6 @P1
/docs/01_architecture/06_protocols.md @P2 @P1 @P3 @P5 @P6
/docs/                                 @P6

# Top-level
/PLAN.md                     @P1 @P2 @P3 @P4 @P5 @P6
```

## On-call rotation (CI / red builds)

Astreinte tournante 1 semaine :

| Semaine | On-call principal | Backup |
|---------|-------------------|--------|
| S1 (25-29 mai) | P6 | P1 |
| S2 (1-5 juin) | P2 | P6 |
| S3 (8-12 juin) | P3 | P5 |
| S4 (15-19 juin) | P6 | P1 |

**Devoirs on-call** :
- Surveiller channel Discord `#ci-alerts`
- Si CI rouge sur `develop` ou `main` : intervention dans les 30 min
- Si bloquant > 2h : escalader vers tech-leads concernés

## Escalation paths

| Type problème | Premier contact | Si bloquant > 4h |
|---------------|-----------------|-------------------|
| Bug build/CI | On-call | P6 + autres on-call |
| Bug serveur core | P1 | P1 + P6 sync |
| Bug GUI render | P3 | P3 + P4 sync |
| Bug AI training | P5 | P5 + P6 sync |
| Conflit contrat protocole | P1 + P2 + P3 + P5 | Vote 6 personnes |
| Hardware (training machine down) | P6 | P5 + P6 |
| Question scope / livrable | Tech-leads (P1, P3, P5) | Vote 6 personnes |

## Daily ownership

| Jour | Animateur standup | Note-taker | Retro animator (vendredi) |
|------|-------------------|------------|---------------------------|
| Lundi | P1 | P2 | — |
| Mardi | P3 | P4 | — |
| Mercredi | P5 | P6 | — |
| Jeudi | P1 | P3 | — |
| Vendredi | P6 | P5 | P6 |

Rotation simple, tout le monde anime au moins 1x/semaine.

## Communication rules

- **Pas de @here / @everyone** sauf urgence (CI rouge sur main, hardware down, blocker équipe entière)
- **Threads Discord** pour discussions techniques approfondies (ne pas polluer le channel principal)
- **Lien PR** : toujours partagé dans le channel correspondant (`#server` / `#gui` / `#ai`)
- **Décision technique** : si > 15 min de discussion → propose une ADR
- **Question bloquante** : si tu es bloqué depuis 30 min, demande à l'équipe (pas de fierté)

## Definition of "available"

Heures cœur : **10h → 18h** (heure de Paris). Pendant ces heures, tu réponds aux mentions dans <30 min.
Hors heures cœur : best effort.
Standup obligatoire à 9h30 (15 min). Si retard : préviens sur Discord avant 9h25 + envoie un message standup async.

## Holidays / off

Vu la durée courte (4 semaines), **aucun congé planifié** dans la fenêtre 25 mai → 19 juin. Si imprévu : prévenir l'équipe 48h avant, redistribuer les tickets en cours.
