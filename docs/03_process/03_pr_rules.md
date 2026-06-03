# 03 — Règles de PR

## PR Template

Fichier `.github/PULL_REQUEST_TEMPLATE.md` :

```markdown
## 📝 Description

<!-- Décris brièvement ce que cette PR change et pourquoi. -->

## 🎯 Type de changement

- [ ] feat (nouvelle feature)
- [ ] fix (bug fix)
- [ ] perf (optimisation perf)
- [ ] refactor (refactoring sans changement comportemental)
- [ ] docs (documentation seulement)
- [ ] test (ajout/modif tests)
- [ ] chore (build, deps, CI)

## 🔗 Lien issue / ADR

- Closes #
- Related ADR : docs/adrs/00X-...

## ✅ Checklist obligatoire

- [ ] **Conventional Commits** : tous mes commits respectent le format
- [ ] **CI verte** : tous les checks passent
- [ ] **Tests** : j'ai ajouté/modifié les tests pour cette modification
- [ ] **Coverage** : la couverture ne baisse pas sous 70% C++ / 80% Python
- [ ] **Doc** : si API publique modifiée, doxygen / docstring à jour
- [ ] **Pas de TODO/FIXME laissés sans issue associée**
- [ ] **Pas de fichiers commentés / code mort**
- [ ] **Si bump version deps** : justification dans le body
- [ ] **Si modif protocole** : ADR créé et lien ci-dessus
- [ ] **Si modif perf critique** : benchmark ajouté/maj
- [ ] **CODEOWNERS** ont été assignés en reviewers (auto)
- [ ] **2 reviews** demandées (au moins 1 du domaine impacté)

## 📷 Screenshots (si visuel)

<!-- Avant / après, ou screenshots du résultat -->

## 🧪 Comment tester

```bash
# Étape 1 :
make re && ./zappy_server -p 4242 -x 20 -y 20 -n red -c 4 -f 100 &

# Étape 2 :
./zappy_gui -p 4242 -h localhost

# Vérification attendue :
# - ...
```

## ⚠️ Breaking changes

<!-- Si applicable, lister les changements cassants -->

## 🤔 Risques / inconnues

<!-- Ce qui pourrait mal tourner, ou que tu n'as pas testé -->
```

## Règles

### Avant d'ouvrir la PR

1. **Rebase sur develop** : `git fetch origin && git rebase origin/develop`
2. **Tests locaux verts** : `make test` (alias pour `ctest` + `pytest`)
3. **Lint local vert** : `pre-commit run --all-files`
4. **Conventional Commits respectés**
5. **Brancher : feature/...** (pas direct sur develop / main)
6. **PR opened** vers `develop` (jamais directement vers `main`, sauf release/hotfix)

### Pendant le review

- **2 approvals required**, dont au moins 1 du CODEOWNERS du domaine
- **CI verte obligatoire** pour merge
- **Discussions résolues** : toutes les conversations review marquées resolved
- **Pas de force push** après le premier review (sauf rebase clean documenté en commentaire)
- **Suggestions GitHub** : à apply via UI quand mineur ; sinon discuter en thread

### Délai de review

- **<=4h en heures cœur** (10h-18h Paris)
- **<=24h sinon**
- Si tu es bloqué en attente review > 4h : ping le reviewer + l'on-call dans `#general`

### Auto-merge

Activé pour les PR labelisées `auto-merge` (par P6 généralement) :
- bumps deps via dependabot
- doc-only changes
- ci-only changes
À condition : 1 approval + CI verte.

### Squash merge par défaut

GitHub configuré pour proposer **squash and merge** par défaut sur `develop`.
Le commit de squash reprend le titre de la PR (qui doit respecter Conventional Commits).

## Labels

| Label | Usage |
|-------|-------|
| `type:feat` / `type:fix` / etc. | type changement |
| `scope:server` / `scope:gui` / etc. | pôle impacté |
| `priority:high` / `priority:medium` / `priority:low` | priorité |
| `status:blocked` | bloqué par dépendance externe |
| `status:needs-rebase` | conflit avec develop |
| `status:ready-for-review` | prêt review |
| `wip` | work in progress, pas reviewable |
| `auto-merge` | éligible auto-merge |
| `breaking-change` | introduit rupture |
| `adr-required` | doit avoir une ADR avant merge |

## ADR — quand en écrire une

Tu **dois** écrire une ADR avant de merger si ton PR :
- Modifie une API publique entre deux composants (server↔ai, server↔gui, sim↔py)
- Change un choix de techno / lib structurante
- Modifie le format `.zrec`, la config serveur, ou un protocole
- Change la stratégie de build/CMake/CI
- Introduit ou retire une dépendance majeure

Workflow ADR : voir [`04_adrs.md`](04_adrs.md).

## CODEOWNERS

Voir [`docs/02_team/03_responsibilities.md#ownership-zones-de-code-codeowners`](../02_team/03_responsibilities.md#ownership-zones-de-code-codeowners) pour le contenu du fichier `.github/CODEOWNERS`.

GitHub assignera automatiquement les CODEOWNERS comme reviewers requis.

## Backlog / Triage

Toutes les issues nouvelles passent par `Backlog` du board Projects, triées au standup lundi matin (priorisation par tech-leads).
