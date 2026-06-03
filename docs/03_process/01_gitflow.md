# 01 — GitFlow

## Branches permanentes

- **`main`** : production stable. Chaque merge sur main = un release tag (`v0.x.y`).
- **`develop`** : intégration courante. C'est la branche par défaut du repo.

## Branches éphémères

- **`feature/<scope>-<short-desc>`** : nouvelle feature, partira de `develop`, mergera dans `develop`.
- **`release/<version>`** : préparation d'une release (last fixes, version bumps, changelog). Part de `develop`, merge dans `main` + back-merge dans `develop`.
- **`hotfix/<scope>-<short-desc>`** : fix urgent en prod, part de `main`, merge dans `main` + back-merge dans `develop`.
- **`chore/<scope>-<short-desc>`** : refactor, doc, build, deps. Comme feature mais sans changement comportemental.

## Branch protection rules (à activer dans GitHub Settings)

### `main`
- Pull request requise (1 PR minimum)
- 2 approvals required
- Dismiss stale approvals on new push
- Require review from CODEOWNERS
- Status checks required : `ci/build`, `ci/lint`, `ci/test-unit`, `ci/test-integration`, `ci/commitlint`, `ci/docs-build`
- Require branches to be up to date before merge
- Require linear history (squash or rebase merge only)
- Restrict push : tech-leads uniquement (P1, P3, P5, P6)
- Allow force push : **NO**
- Allow deletion : **NO**

### `develop`
- Pull request requise
- 2 approvals required
- Status checks required : `ci/build`, `ci/lint`, `ci/test-unit`, `ci/commitlint`
- Require review from CODEOWNERS
- Require branches to be up to date before merge
- Allow force push : **NO**

### `feature/*`, `chore/*`, `hotfix/*`, `release/*`
- Pas de protection particulière (les devs travaillent dessus librement)
- CI doit passer pour pouvoir merger vers `develop` ou `main`

## Workflow journalier (par dev)

```bash
# Matin : update local
git checkout develop
git pull --rebase

# Créer ta feature branch
git checkout -b feature/server-event-scheduler

# Code, commit, commit (Conventional Commits)
git commit -m "feat(server): add EventScheduler skeleton"
git commit -m "test(server): add unit tests for EventScheduler ordering"

# Pousser
git push -u origin feature/server-event-scheduler

# Ouvrir PR → develop, attendre 2 reviews + CI verte
# Squash-merge (auto via GitHub UI)
```

## Workflow release (fin de sprint)

```bash
# Vendredi soir / fin de sprint
git checkout develop
git pull --rebase
git checkout -b release/0.2.0

# Bump version dans :
#  - CMakeLists.txt (project(zappy VERSION 0.2.0))
#  - ai_python/pyproject.toml
#  - docs/CHANGELOG.md (auto-généré via release-please)

git commit -m "chore(release): bump version to 0.2.0"
git push -u origin release/0.2.0

# PR release/0.2.0 → main
# Après merge :
git tag v0.2.0
git push origin v0.2.0

# Back-merge main → develop pour récupérer le bump de version
git checkout develop
git merge --no-ff main
git push origin develop
```

## Workflow hotfix

```bash
# Bug critique en prod / sur main
git checkout main
git pull --rebase
git checkout -b hotfix/gui-crash-empty-team

# Fix + tests
git commit -m "fix(gui): handle empty teams list without crashing"

# PR hotfix → main, merge fast
# Tag patch v0.2.1
git tag v0.2.1
git push origin v0.2.1

# Back-merge main → develop
git checkout develop
git merge --no-ff main
git push origin develop
```

## Conventional Commits

Tous les commits doivent suivre [Conventional Commits 1.0](https://www.conventionalcommits.org/). Enforced par `commitlint` en CI.

Format :
```
<type>(<scope>): <subject (impératif, présent, <72 char)>

[Optional body : explique le pourquoi]

[Optional footer : Closes #42, BREAKING CHANGE: ...]
```

Types : `feat | fix | docs | style | refactor | perf | test | build | ci | chore | revert`

Scopes whitelist : `server | gui | ai | sim | train | protocol | docs | ci | build | deps | infra`

Exemples valides :
```
feat(server): add admin token authentication
fix(gui): correct memory leak in particle pass
docs(adrs): add ADR-005 for zrec format
perf(sim): batch reset 10x faster
ci(workflows): cache vcpkg dependencies
```

## Squash vs rebase vs merge

- **Squash merge** par défaut pour `feature/*` → `develop` : 1 commit propre dans l'historique
- **Rebase merge** pour `release/*` → `main` : préserve les commits de fix mineurs
- **Merge commit (--no-ff)** uniquement pour back-merge `main` → `develop`
- Le dev doit faire `git rebase develop` localement avant de pusher pour éviter les conflits

## .gitmessage template

Fichier `.gitmessage` à la racine :
```
# <type>(<scope>): <subject>
#
# Body : pourquoi cette modification ? (impératif, présent)
#
# Footer :
#   Closes #issue-number
#   BREAKING CHANGE: description si rupture API
```

Activer pour le repo :
```
git config commit.template .gitmessage
```
