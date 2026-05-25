# 04 — Architecture Decision Records (ADRs)

## Pourquoi des ADRs

Une **ADR** capture une décision technique structurante : contexte, options envisagées, décision, conséquences. Elle versionne le raisonnement, pas seulement le résultat.

Objectif : qu'un futur dev (ou ton futur toi dans 3 mois) puisse comprendre **pourquoi** un choix a été fait, et donc juger s'il faut le réviser ou pas.

## Où sont-elles ?

```
docs/adrs/
├── 000-template.md
├── 001-server-language-cpp17.md
├── 002-vulkan-dynamic-rendering.md
├── 003-pybind11-strategy.md
├── ...
```

## Template

`docs/adrs/000-template.md` :

```markdown
# ADR-XXX : <Titre court à l'impératif>

- **Statut** : Proposed | Accepted | Deprecated | Superseded by ADR-YYY
- **Date** : YYYY-MM-DD
- **Auteur(s)** : @P1, @P3
- **Reviewers** : @P5 @P6
- **Tags** : server, gui, ai, protocol, build, ...

## Contexte

Quel est le problème, la contrainte, le besoin qui motivent cette décision ?
Quelles forces sont en présence (technique, équipe, calendrier, sujet) ?

## Options considérées

### Option A — <nom>
- Description courte
- ✅ Avantages
- ❌ Inconvénients
- 💰 Coût (temps, complexité)

### Option B — <nom>
- ...

### Option C — <nom>
- ...

## Décision

Nous choisissons l'**option X** parce que :
- Raison 1
- Raison 2
- ...

## Conséquences

### Positives
- ...

### Négatives / coûts
- ...

### Suivi nécessaire
- [ ] Action 1
- [ ] Action 2

## Références

- Issue #
- PR #
- Lien externe / RFC / doc tierce
```

## Workflow ADR

1. **Identifier le besoin** : pendant un sprint planning, une PR, ou une discussion qui dure > 15 min
2. **Créer l'ADR en `Proposed`** : numéro suivant disponible
3. **Ouvrir une PR ADR-only** : sur `develop`, scope `docs/adrs/`
4. **Discussion** : commentaires sur la PR (pas dans des channels éphémères)
5. **Vote tech-leads** : pour les ADRs `Accepted`, il faut OK des 3 tech-leads concernés
6. **Merge en `Accepted`** : la décision est officielle
7. **Si réfutée** : marquer `Deprecated` avec lien vers la nouvelle ADR `Superseded by`

## ADRs déjà identifiées (à écrire S1)

| # | Titre | Owner |
|---|-------|-------|
| 001 | Server language C++17/20 (vs C, Rust) | P1 |
| 002 | Vulkan 1.3 dynamic rendering vs render pass legacy | P3 |
| 003 | pybind11 pour exposer libzappy_sim au training Python | P6 |
| 004 | RLlib multi-agent vs SB3 single-agent | P5 |
| 005 | Format .zrec binaire custom (vs JSON Lines) | P2 |
| 006 | libtorch C++ inference (vs ONNX Runtime) | P5 / P6 |
| 007 | Stratégie fallback ray tracing pour non-RTX | P3 |
| 008 | Broadcast codé entre AIs same team | P6 |
| 009 | asio standalone (vs Boost.Asio, vs raw poll) | P2 |
| 010 | Format métriques Prometheus + labels | P2 |
| 011 | Frame graph design (push vs pull, ressources lifetime) | P3 |
| 012 | Shader hot-reload via glslang runtime | P3 |
| 013 | ImGui docking layout par défaut | P4 |
| 014 | Asset pipeline glTF 2.0 + KTX2 + script build_assets | P4 |
| 015 | Replay timeline UX (scrub, bookmark, speed) | P4 |
| 016 | Curriculum design (stages YAML) | P5 |
| 017 | Reward shaping multi-objectif | P5 |
| 018 | Export TorchScript vs ONNX vs raw weights | P5 |
| 019 | CI strategy : matrix Ubuntu+Fedora x gcc+clang | P6 |
| 020 | vcpkg manifest mode vs Conan vs system | P6 |

## ADRs prévues en S2 / S3 / S4

- Découvertes au fur et à mesure des PRs
- Numéros assignés à la création (incrémentaux, jamais réutilisés)

## ADR life-cycle

```
   ┌──────────┐
   │ Proposed │
   └────┬─────┘
        │ tech-leads OK
        ▼
   ┌──────────┐         ┌────────────┐
   │ Accepted │ ───────►│ Superseded │
   └────┬─────┘         └────────────┘
        │
        │ devient obsolète sans remplacement
        ▼
   ┌────────────┐
   │ Deprecated │
   └────────────┘
```

## Anti-patterns à éviter

- ❌ ADR écrite **après** l'implémentation (sauf en rare retrofit)
- ❌ ADR de 3 lignes "on choisit X" sans contexte ni options
- ❌ ADR qui ne sera jamais relue : un titre clair + une section décision concise est mieux qu'un essai exhaustif
- ❌ Modifier une ADR `Accepted` : créer une nouvelle qui supersede
- ❌ Une ADR dans une PR de code : toujours dans une PR séparée pour la traçabilité
