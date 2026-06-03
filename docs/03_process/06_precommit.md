# 06 — Pre-commit hooks

## Pourquoi

Garantir que **toute modification commitée localement** respecte le style, passe le lint, ne contient pas de secrets, avant même d'ouvrir une PR. Réduit drastiquement les allers-retours review pour des virgules.

## Installation (chaque dev, J1 du projet)

```bash
# Installer pre-commit framework
pip install --user pre-commit

# Dans le repo
cd ~/Epitech/G-YEP/G-YEP-400-RUN-4-1-zappy-3
pre-commit install                  # active les hooks sur git commit
pre-commit install --hook-type commit-msg  # active commitlint

# Première run sur tout le repo
pre-commit run --all-files
```

## Fichier `.pre-commit-config.yaml`

```yaml
default_language_version:
  python: python3.11

repos:
  # 1. Sanity / sécurité
  - repo: https://github.com/pre-commit/pre-commit-hooks
    rev: v4.6.0
    hooks:
      - id: trailing-whitespace
      - id: end-of-file-fixer
      - id: check-yaml
      - id: check-json
      - id: check-toml
      - id: check-merge-conflict
      - id: check-added-large-files
        args: ['--maxkb=5000']
      - id: detect-private-key
      - id: mixed-line-ending
        args: ['--fix=lf']

  # 2. Secrets detection
  - repo: https://github.com/Yelp/detect-secrets
    rev: v1.5.0
    hooks:
      - id: detect-secrets
        args: ['--baseline', '.secrets.baseline']

  # 3. C++ formatting
  - repo: https://github.com/pre-commit/mirrors-clang-format
    rev: v17.0.6
    hooks:
      - id: clang-format
        types_or: [c++, c]
        args: ['--style=file', '-i']

  # 4. C++ lint (clang-tidy local + cppcheck)
  - repo: local
    hooks:
      - id: clang-tidy
        name: clang-tidy
        entry: ./tools/clang_tidy_runner.sh
        language: system
        types: [c++]
        require_serial: true
      - id: cppcheck
        name: cppcheck
        entry: cppcheck
        args: ['--enable=warning,style,performance,portability', '--error-exitcode=1', '--inline-suppr', '--std=c++20']
        language: system
        types: [c++]

  # 5. Shaders lint (glslang validator)
  - repo: local
    hooks:
      - id: glslang-validator
        name: glslangValidator
        entry: glslangValidator
        args: ['--target-env', 'vulkan1.3']
        language: system
        types_or: [file]
        files: 'gui/shaders/.*\.(vert|frag|comp|rgen|rmiss|rchit|tesc|tese|geom)$'

  # 6. Python : ruff + black + mypy
  - repo: https://github.com/astral-sh/ruff-pre-commit
    rev: v0.5.0
    hooks:
      - id: ruff
        args: ['--fix']
      - id: ruff-format

  - repo: https://github.com/psf/black
    rev: 24.4.2
    hooks:
      - id: black
        language_version: python3.11

  - repo: https://github.com/pre-commit/mirrors-mypy
    rev: v1.10.0
    hooks:
      - id: mypy
        additional_dependencies:
          - types-PyYAML
          - types-requests
        args: ['--strict', '--ignore-missing-imports']
        files: '^ai_python/zappy_train/'

  # 7. YAML / Markdown
  - repo: https://github.com/adrienverge/yamllint
    rev: v1.35.1
    hooks:
      - id: yamllint
        args: ['-d', '{extends: default, rules: {line-length: {max: 200}}}']

  - repo: https://github.com/igorshubovych/markdownlint-cli
    rev: v0.41.0
    hooks:
      - id: markdownlint
        args: ['--config', '.markdownlint.yaml']

  # 8. Dockerfile lint
  - repo: https://github.com/hadolint/hadolint
    rev: v2.12.0
    hooks:
      - id: hadolint-docker

  # 9. Commit message (Conventional Commits)
  - repo: https://github.com/compilerla/conventional-pre-commit
    rev: v3.4.0
    hooks:
      - id: conventional-pre-commit
        stages: [commit-msg]
        args:
          - feat
          - fix
          - docs
          - style
          - refactor
          - perf
          - test
          - build
          - ci
          - chore
          - revert
```

## `.clang-format`

```yaml
BasedOnStyle: LLVM
Language: Cpp
Standard: c++20
ColumnLimit: 100
IndentWidth: 4
ContinuationIndentWidth: 4
AccessModifierOffset: -4
BreakBeforeBraces: Attach
AllowShortFunctionsOnASingleLine: Inline
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
SpaceAfterCStyleCast: true
PointerAlignment: Left
ReferenceAlignment: Left
SortIncludes: CaseSensitive
IncludeBlocks: Regroup
IncludeCategories:
  - Regex: '^<.*\.h>'      # C system
    Priority: 1
  - Regex: '^<.*>'          # C++ stdlib
    Priority: 2
  - Regex: '^"zappy/.*"'    # project
    Priority: 4
  - Regex: '.*'             # other third-party
    Priority: 3
AlignAfterOpenBracket: Align
AlignConsecutiveAssignments: false
AlignConsecutiveDeclarations: false
NamespaceIndentation: None
FixNamespaceComments: true
```

## `.clang-tidy`

```yaml
Checks: >
  bugprone-*,
  cert-*,
  clang-analyzer-*,
  cppcoreguidelines-*,
  modernize-*,
  performance-*,
  portability-*,
  readability-*,
  -modernize-use-trailing-return-type,
  -readability-magic-numbers,
  -cppcoreguidelines-avoid-magic-numbers,
  -cppcoreguidelines-pro-bounds-array-to-pointer-decay,
  -cppcoreguidelines-pro-type-vararg,
  -bugprone-easily-swappable-parameters

WarningsAsErrors: '*'
HeaderFilterRegex: '(server|gui|ai_cpp|sim_python)/.*\.(hpp|cpp|h)$'
FormatStyle: file
CheckOptions:
  - { key: readability-identifier-naming.ClassCase, value: CamelCase }
  - { key: readability-identifier-naming.StructCase, value: CamelCase }
  - { key: readability-identifier-naming.FunctionCase, value: lower_case }
  - { key: readability-identifier-naming.VariableCase, value: lower_case }
  - { key: readability-identifier-naming.PrivateMemberSuffix, value: _ }
  - { key: readability-identifier-naming.ConstexprVariableCase, value: UPPER_CASE }
  - { key: readability-identifier-naming.EnumConstantCase, value: UPPER_CASE }
  - { key: readability-identifier-naming.MacroDefinitionCase, value: UPPER_CASE }
  - { key: readability-identifier-naming.NamespaceCase, value: lower_case }
```

## Performance des hooks

Pre-commit ne lance les hooks **que sur les fichiers staged** → rapide.
Full run `pre-commit run --all-files` : ~2-5 min.

## CI exécute les mêmes vérifications

Tous les hooks tournent aussi en CI (`ci.yml` job `lint`). Si quelqu'un bypass (`--no-verify`), la CI catchera.

## Bypass autorisé ?

**Non.** Pas de `git commit --no-verify` sauf situation explicitement documentée (ex: commit de merge initial après import git lfs raté). Si tu as besoin de bypass, demande à l'on-call.

## tools/clang_tidy_runner.sh

```bash
#!/usr/bin/env bash
# Run clang-tidy on staged files only, using the project's compile_commands.json
set -euo pipefail

BUILD_DIR=${BUILD_DIR:-build}
COMPILE_COMMANDS="$BUILD_DIR/compile_commands.json"

if [[ ! -f "$COMPILE_COMMANDS" ]]; then
    echo "WARNING: $COMPILE_COMMANDS not found, running 'cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON' first..."
    cmake -S . -B "$BUILD_DIR" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null
fi

# Args from pre-commit are the file paths
clang-tidy --quiet -p "$BUILD_DIR" "$@"
```

## tools/format_all.sh

```bash
#!/usr/bin/env bash
# Run pre-commit on all files
set -euo pipefail

if [[ "${1:-}" == "--check" ]]; then
    pre-commit run --all-files --show-diff-on-failure
else
    pre-commit run --all-files
fi
```
