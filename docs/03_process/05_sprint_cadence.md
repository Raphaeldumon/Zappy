# 05 — Cadence sprint, standup, retro

## Sprint = 1 semaine

| Sprint | Dates | Thème |
|--------|-------|-------|
| S1 | lun 25 → ven 29 mai 2026 | Foundations |
| S2 | lun 1 → ven 5 juin 2026 | Core MVP |
| S3 | lun 8 → ven 12 juin 2026 | Bonus + intégration |
| S4 | lun 15 → ven 19 juin 2026 | Polish + soutenance |

## Cadence quotidienne

### Standup — chaque jour 9h30, 15 min, en visio Discord

Format (chacun parle ~2 min) :
1. Ce que j'ai fait hier
2. Ce que je fais aujourd'hui
3. Blockers ?

Animateur du jour : rotation (voir [`docs/02_team/03_responsibilities.md`](../02_team/03_responsibilities.md)).
Note-taker : poste résumé dans `#standup` channel Discord.

**Règles** :
- Démarrage à 9h30 pile, même si tout le monde n'est pas là (les retardataires lisent les notes)
- Pas de débat technique dans le standup → "let's take this offline" ; planifier un break-out sync ensuite
- Si quelqu'un n'est pas là physiquement : standup async sur Discord avant 9h30

### Travail focus

- 10h → 12h30 : matin focus (no meeting sauf urgence)
- 12h30 → 14h : pause déjeuner
- 14h → 16h : afternoon focus
- 16h → 17h : pair-programming / reviews / breakouts au besoin
- 17h → 18h : flush PRs reviews + commit du jour
- 18h+ : optionnel selon les besoins

## Cadence hebdomadaire

### Planning lundi 9h30 → 10h30 (1h)

À la place du standup standard du lundi.
Animateur : tech-lead du sprint (rotation : S1=P6, S2=P1, S3=P3, S4=P5).

Agenda :
1. Recap retro vendredi précédent (10 min)
2. Goals du sprint (15 min) — référence le doc sprint dans `docs/06_calendar/sprints/`
3. Repartir les tickets du sprint dans le Kanban (20 min)
4. Risques connus / aides nécessaires (10 min)
5. AOB (5 min)

Output : board GitHub Projects à jour, tout le monde sait sur quoi il bosse cette semaine.

### Retro vendredi 17h → 17h45 (45 min)

Format Stop / Start / Continue + actions :

1. **Stop** (15 min) : ce qui ne marche pas, que faut-il arrêter
2. **Start** (10 min) : ce qu'on devrait commencer la semaine prochaine
3. **Continue** (5 min) : ce qui marche bien
4. **Actions** (10 min) : transformer les Stop/Start en tickets actionnables
5. **Mood check** (5 min) : 1-5 chacun, anonyme via menti.com ou similaire

Note-taker (= retro animator) poste le résumé dans Discord `#retro` + crée les tickets actions.

### Demo Friday — vendredi 16h → 17h (1h)

Avant la retro, on fait une **demo interne** : chaque pôle montre ce qu'il a livré dans la semaine.
But : alignement, fierté, et catch d'incohérences cross-team plus tôt.

## Cadence ad-hoc

### Pair-programming sessions

Encouragé sur tâches complexes :
- Vulkan RT setup (P3 + P4)
- pybind11 binding (P6 + P1 ou P5)
- Conformance sim vs runtime (P1 + P6)
- RLlib config + curriculum (P5 + P6)

Format : visio Discord screen share, durée 1-3h, à planifier 1 jour à l'avance.

### Breakout sync techniques

Quand un thread Discord devient trop long ou qu'il y a désaccord :
1. Crée un event Discord "Sync <topic>" dans 30 min
2. Max 4 personnes
3. Max 30 min
4. Output : décision actée OU ADR à écrire

### Crisis sync

Si CI rouge sur main > 1h, ou blocker critique :
1. Le on-call sonne le rassemblement dans `#general`
2. Visio Discord immédiate
3. War room jusqu'à résolution

## Définition d'un sprint réussi

À la fin du sprint, l'équipe vote (à main levée en retro) :
- ✅ Tous les goals du sprint atteints + qualité
- 🟡 La plupart des goals atteints, qq glissements minimes
- ❌ Goals majeurs ratés ou qualité dégradée

Cible : 100% en ✅ ou 🟡 sur les 4 sprints. Si ❌ : re-plan immédiat avec le tech-lead du sprint suivant.

## Outils

| Outil | Usage |
|-------|-------|
| Discord | Chat, visio standup, screen share |
| GitHub Projects | Kanban board (Backlog / Todo Sprint / In Progress / Review / Done) |
| GitHub Issues | Tickets unitaires |
| GitHub PRs | Code reviews |
| Menti / sli.do | Mood check anonyme retro |
| Google Meet (backup) | Si Discord lag |

## Sprint board template

Colonnes Kanban :
1. **Backlog** : tickets non priorisés / sprints futurs
2. **Todo Sprint** : tickets engagés sur le sprint courant
3. **In Progress** : tickets en cours (max 2 par dev)
4. **Review** : PRs en attente review
5. **Done** : tickets fermés sur le sprint courant

Limite WIP par dev : **2 tickets max en "In Progress"**.
Si tu as 2 tickets en `In Progress` et tu en débloques un troisième → tu helps reviewer d'abord, sinon tu split la complexité.
