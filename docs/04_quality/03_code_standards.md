# 03 — Standards de code

## C++ — règles structurantes

### Style
- Formaté par `clang-format` (voir `.clang-format` dans `docs/03_process/06_precommit.md`)
- Indent 4 spaces, no tabs
- Column limit 100
- Brace style : Attach (Allman réservé namespaces)

### Standard C++
- **C++20** activé (`-std=c++20`)
- Concepts autorisés
- `std::format` autorisé (gcc 13+ et clang 17+ OK)
- `std::span` privilégié à pointer+length
- `[[nodiscard]]` sur tout retour non-void utilisé pour valider
- `noexcept` partout où c'est vrai
- `constexpr` agressif sur les fonctions pures

### Header guards
- `#pragma once` (pas de macro guards)

### Nommage
- Classes / structs : `PascalCase` (`WorldState`, `EventScheduler`)
- Fonctions / méthodes : `snake_case` (`move_forward`, `compute_reward`)
- Variables : `snake_case`
- Membres privés : `snake_case_` (suffixe `_`)
- `constexpr` et macros : `SCREAMING_SNAKE_CASE`
- Enum class values : `UPPER_CASE`
- Namespace : `lower_case`, racine `zappy::`

### Includes
- Trois groupes séparés par ligne vide :
  1. C system headers `<sys/...>`
  2. C++ stdlib `<vector>`
  3. Third-party `<asio.hpp>`
  4. Project `"zappy/..."`
- Auto-grouped par clang-format

### Ressources
- **RAII** systématique. Pas de `new`/`delete` nu.
- Smart pointers : `std::unique_ptr` par défaut, `std::shared_ptr` uniquement quand ownership partagé prouvé
- Pas de `using namespace std;` (sauf très local scope dans .cpp)

### Exceptions
- Autorisées sauf dans `server/core/` (hot path) qui utilise `std::expected<T,E>` / `tl::expected`
- GUI/AI peuvent throw (rare events)

### Const correctness
- `const` autant que possible (références const, méthodes const)
- `auto` autorisé mais privilégier types explicites en API publique

### Multithreading
- Server : single thread imposé (sujet), pas de mutex
- GUI : main thread + render thread + audio thread → `std::jthread`, `std::atomic`, `std::mutex` minimaliste
- Sim : single thread per env (parallélisme = N envs)
- Toujours documenter quel thread possède quelle ressource

### Macros
- Évitées au maximum
- Si nécessaire : préfixées `ZAPPY_` (ex `ZAPPY_LIKELY`, `ZAPPY_ASSERT`)
- Pas de macro qui ressemble à une fonction (préférer `[[gnu::always_inline]] static inline`)

### Errors
- `[[nodiscard]] std::expected<T, ErrorKind> foo()` pour les opérations qui peuvent échouer dans le hot path
- `enum class ErrorKind { ... }` plutôt que codes int
- Logging : `spdlog::error("context: {}", e)` jamais d'erreurs silencieuses

## Python — règles

### Style
- Formaté par `black` (line length 100)
- Linter `ruff` (configuration `pyproject.toml`)
- Type checker `mypy --strict`

### Nommage
- snake_case pour fichiers, fonctions, variables
- PascalCase pour classes, type aliases
- UPPER_SNAKE pour constantes
- Préfixe `_` pour privé module-level, `__` pour mangled class-level (rare)

### Type hints
- **Strict** : toutes les signatures publiques annotées
- `from __future__ import annotations` en tête de fichier
- Préférer `list[int]` à `List[int]` (PEP 585)
- Types Numpy : `npt.NDArray[np.float32]`

### Imports
- Auto-organisés par `ruff` (isort intégré)
- Pas de wildcards `from x import *`
- Imports relatifs interdits dans `zappy_train/`, absolus uniquement

### Async / concurrence
- AI training : multi-process via RLlib workers, pas asyncio
- pybind11 release GIL pour Sim::step (`py::call_guard<py::gil_scoped_release>`)

### Docstrings
- Google style
- Obligatoire pour toute fonction publique du package `zappy_train`

### Errors
- Exceptions explicites typées (`class TrainingError(Exception)`)
- Pas de catch-all `except:` (interdit par ruff)

## Shaders GLSL

### Style
- Indent 4 spaces
- Snake_case nommage
- `// =====` séparateurs de sections (PUSH / UBO / VARYINGS / FUNCTIONS / MAIN)

### Layout convention
```glsl
#version 460
#extension GL_EXT_buffer_reference : require

// ============ PUSH CONSTANTS ============
layout(push_constant) uniform PushConstants {
    mat4 view_proj;
    vec3 camera_pos;
    float time;
} pc;

// ============ UBO (set 0) ============
layout(set = 0, binding = 0) uniform Scene { ... } scene;

// ============ BINDLESS (set 1) ============
layout(set = 1, binding = 0) uniform sampler2D textures[];

// ============ INPUTS ============
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;

// ============ OUTPUTS ============
layout(location = 0) out vec4 out_color;

// ============ MAIN ============
void main() { ... }
```

### Includes
- Fichiers `.glsl` dans `gui/shaders/common/`
- `#include "common/utils.glsl"` (preprocessor glslang)

### Compile-time constants
- `#define ZAPPY_HAS_RT 1` injecté depuis CMake selon device features

## Commits / messages / docs

- En anglais pour : code, identifiants, commits, PR titles, PR bodies, ADRs (option), docstrings, comments
- En français pour : documentation utilisateur (`docs/`), README utilisateur, slides soutenance
- ADRs : titre en français possible, body indifférent (équipe FR)

## Auto-formatage et lint — workflow

```bash
# Avant commit : pre-commit lance tout automatiquement
git add server/core/world_state.cpp
git commit -m "feat(server): add wrap toroidal"
# pre-commit corrige formatting, vérifie lint, vérifie message

# Si tu veux re-formater tout (rare)
./tools/format_all.sh

# Si tu veux juste lint check sans modifier
./tools/format_all.sh --check
```

## Tabous absolus

- ❌ `git push --force` sur `main` ou `develop`
- ❌ Bypass `--no-verify` sur les commits
- ❌ Désactiver un lint check sans ADR
- ❌ Code commenté en bloc (utiliser git)
- ❌ Numéro magique sans `constexpr` nommé
- ❌ `goto` (sauf cleanup unique fonction, rare)
- ❌ `printf` direct (utiliser `spdlog`)
- ❌ `std::cout` dans code non-CLI (sauf debug temporaire)
- ❌ Catch all exception sans context (`catch (...) { /* nothing */ }`)
- ❌ Doc en anglais pour utilisateurs francophones
- ❌ Variables 1-char sauf indices de loops (`i`, `j`, `k`)

## Bonus : doxygen / docstrings

- C++ : doxygen-style (`/// @brief`, `/// @param`, `/// @return`) sur les API publiques (`include/zappy/**`)
- Python : Google-style docstrings, types dans signatures (pas dans docstrings)
- Pas obligatoire pour API privées (membres `_` ou `static`)

Exemple C++ :
```cpp
/// @brief Compute the vision cone for a player at level L.
///
/// @param player_id  ID of the player (must be alive)
/// @param level      Current player level (1..8)
/// @return Vector of TileView, indexed left-to-right, front-to-back
[[nodiscard]] std::vector<TileView> compute_vision(PlayerId player_id, int level) const;
```

Exemple Python :
```python
def compute_reward(prev_state: AgentState, new_state: AgentState,
                   action_id: int, alive: bool, just_died: bool) -> float:
    """Compute per-step reward for an agent.

    Args:
        prev_state: Agent state before the action.
        new_state: Agent state after the action.
        action_id: Discrete action id (0..N).
        alive: True if the agent is still alive.
        just_died: True if the agent died this step.

    Returns:
        Scalar reward for this step.
    """
```
