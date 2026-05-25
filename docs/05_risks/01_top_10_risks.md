# 01 — Top 10 risques projet

Échelle :
- **Probabilité** : Low (L) / Medium (M) / High (H)
- **Impact** : Low (L) / Medium (M) / High (H) / Critical (C)
- **Score = P × I** (en gardant `C=4, H=3, M=2, L=1`)

| # | Risque | Prob. | Impact | Score | Pôle | Owner |
|---|--------|:-----:|:------:|:-----:|------|-------|
| R01 | Ray tracing Vulkan : peu de devs ont RTX, bloque test/dev | M | H | 6 | GUI | P3 |
| R02 | RL multi-agent ne converge pas en 4 semaines | H | H | 9 | AI | P5 |
| R03 | Couche réseau poll/asio bug obscur sous Fedora | L | C | 4 | Server | P2 |
| R04 | Vulkan 1.3 + dynamic rendering + bindless : courbe apprentissage | M | H | 6 | GUI | P3 |
| R05 | Pybind11 GIL / multi-thread RLlib : leaks / segfaults sim | M | H | 6 | AI/Sim | P6 |
| R06 | Conformance sim vs runtime drift inattendu | M | C | 8 | Server/AI | P1, P6 |
| R07 | Conflit dans contrat protocole impacte 3 pôles → blocage | M | H | 6 | Cross | Leads |
| R08 | Machine training (64 GB + RTX) down ou indisponible | L | H | 3 | AI | P6 |
| R09 | Dépendances vcpkg / pip qui breakent en CI Fedora | M | M | 4 | DevOps | P6 |
| R10 | Soutenance jour-J : crash live demo, GPU pilote, etc. | M | C | 8 | All | Leads |

Voir [`02_mitigation_plan.md`](02_mitigation_plan.md) pour les plans de mitigation détaillés.

## Risques bonus (sous le top 10)

| # | Risque | Prob. | Impact | Note |
|---|--------|:-----:|:------:|------|
| R11 | Dépassement scope (feature creep), tout le monde veut un bonus | M | M | Discipliner via DoD et sprints fermés |
| R12 | Tech-lead absent / malade | L | H | Backup tech-lead défini par pôle |
| R13 | Données git-lfs trop volumineuses / quota dépassé | L | M | Tracker `models/` + `gui/assets/` |
| R14 | Réseau interne lent pour push gros models | L | L | Compresser models, mirror local |
| R15 | Sujet Epitech v3.2.4 contient un ambiguïté ou modif tardive | L | M | Question écrite à `pierre.robert@epitech.eu` si découvert |
| R16 | Frustration / fatigue équipe à mi-parcours | M | M | Demo Friday + retro + flexibilité |
| R17 | Doxygen ou MkDocs casse au build → blocage CI | L | L | Job non bloquant en S1, bloquant à partir de S2 |
| R18 | Asset libre choisi viole une license non vue | L | L | Vérification rapide en S1 + log dans `gui/assets/LICENSES.md` |
| R19 | Tarball ref-server obsolète vs protocole sujet | L | M | Comparer manuellement les outputs en S1 |
| R20 | Audio : driver Linux PulseAudio capricieux | L | L | Fallback ALSA, mute si erreur |

## Heatmap risques

```
       Impact →   L      M      H      C
Prob ↓
H              -     -    R02   -
M              -    R09   R01, R04, R05, R07, R11, R16   R06, R10
L              -    R13, R14, R15, R17-R20   R08, R12   R03
```

Les risques en zone rouge (M-H, M-C, H-H, H-C) sont la priorité : R01, R02, R04, R05, R06, R07, R10.

## Revue mensuelle (en interne)

Revue des risques à chaque retro vendredi :
- Nouveaux risques apparus
- Risques mitigés (à retirer)
- Mise à jour scores
- Plan d'action si score augmente
