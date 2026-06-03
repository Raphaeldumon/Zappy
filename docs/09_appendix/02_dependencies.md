# 02 — Dépendances

## Système (à installer via apt/dnf)

### Ubuntu 22.04
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    gcc-13 g++-13 \
    clang-17 clang-tools-17 clang-format-17 clang-tidy-17 \
    cmake ninja-build \
    pkg-config \
    git git-lfs \
    python3.11 python3.11-venv python3-pip \
    libvulkan-dev vulkan-validationlayers-dev spirv-tools \
    libwayland-dev libxkbcommon-dev xorg-dev \
    libpulse-dev libalsa-ocaml-dev \
    libssl-dev \
    cppcheck \
    doxygen graphviz \
    xvfb \
    curl wget \
    docker.io docker-compose
```

Vulkan SDK 1.3.x (via LunarG repo) :
```bash
wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list https://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list
sudo apt update
sudo apt install -y vulkan-sdk
```

### Fedora 39
```bash
sudo dnf install -y \
    @development-tools \
    gcc gcc-c++ clang clang-tools-extra \
    cmake ninja-build \
    pkg-config \
    git git-lfs \
    python3.11 python3-pip python3-virtualenv \
    vulkan-loader-devel vulkan-validation-layers spirv-tools \
    wayland-devel libxkbcommon-devel libX11-devel \
    pulseaudio-libs-devel alsa-lib-devel \
    openssl-devel \
    cppcheck \
    doxygen graphviz \
    xorg-x11-server-Xvfb \
    curl wget \
    docker docker-compose
```

## C++ libs (via vcpkg manifest)

`vcpkg.json` :
```json
{
  "name": "zappy",
  "version": "1.0.0",
  "dependencies": [
    { "name": "asio",                "version>=": "1.30.0"   },
    { "name": "spdlog",              "version>=": "1.13.0"   },
    { "name": "nlohmann-json",       "version>=": "3.11.3"   },
    { "name": "cli11",               "version>=": "2.4.0"    },
    { "name": "catch2",              "version>=": "3.5.0"    },
    { "name": "prometheus-cpp",      "version>=": "1.2.0"    },
    { "name": "vulkan-headers",      "version>=": "1.3.275"  },
    { "name": "vulkan-loader",       "version>=": "1.3.275"  },
    { "name": "vulkan-memory-allocator", "version>=": "3.0.1" },
    { "name": "vk-bootstrap",        "version>=": "1.3.279"  },
    { "name": "glslang",             "version>=": "1.3.275"  },
    { "name": "glm",                 "version>=": "0.9.9.8"  },
    { "name": "glfw3",               "version>=": "3.3.10"   },
    { "name": "imgui",               "version>=": "1.90.0", "features": ["docking-experimental", "glfw-binding", "vulkan-binding"] },
    { "name": "miniaudio",           "version>=": "0.11.21"  },
    { "name": "libtorch",            "version>=": "2.2.0"    },
    { "name": "pybind11",            "version>=": "2.11.1"   },
    { "name": "benchmark",           "version>=": "1.8.3"    },
    { "name": "tomlplusplus",        "version>=": "3.4.0"    }
  ],
  "builtin-baseline": "<commit-hash-stable>"
}
```

## Python deps (via pyproject.toml)

`ai_python/pyproject.toml` (extrait `[project.dependencies]`) :
```toml
[project]
name = "zappy-train"
version = "1.0.0"
requires-python = ">=3.11,<3.13"
dependencies = [
    "torch==2.2.*",
    "numpy>=1.26,<2",
    "ray[rllib]==2.10.*",
    "gymnasium==0.29.*",
    "pettingzoo==1.24.*",
    "supersuit==3.9.*",
    "wandb==0.16.*",
    "tensorboard==2.15.*",
    "tensorboardX>=2.6",
    "pydantic==2.6.*",
    "pyyaml>=6.0,<7",
    "typer>=0.9,<1",
    "rich>=13,<14",
    "streamlit==1.31.*",
    "plotly>=5,<6",
    "pandas>=2.1,<3",
]

[project.optional-dependencies]
dev = [
    "pytest>=8,<9",
    "pytest-cov>=4.1,<5",
    "pytest-benchmark>=4,<5",
    "pytest-xdist>=3.5,<4",
    "mutmut>=2.4,<3",
    "ruff>=0.5,<1",
    "black>=24,<25",
    "mypy==1.10.*",
    "types-PyYAML",
    "types-requests",
    "pre-commit>=3.7,<4",
]
notebooks = [
    "jupyterlab>=4.1,<5",
    "matplotlib>=3.8,<4",
    "seaborn>=0.13,<1",
]
```

`sim_python` exposed via `pybind11`, installé via `pip install -e .` après build CMake.

## CI / Docker

Dockerfile principal pour CI :
```dockerfile
FROM ubuntu:22.04 AS base
ARG GCC_VERSION=13
ARG CLANG_VERSION=17

RUN apt-get update && apt-get install -y \
    build-essential gcc-${GCC_VERSION} g++-${GCC_VERSION} \
    clang-${CLANG_VERSION} clang-tools-${CLANG_VERSION} \
    clang-format-${CLANG_VERSION} clang-tidy-${CLANG_VERSION} \
    cmake ninja-build pkg-config git git-lfs \
    python3.11 python3.11-venv python3-pip \
    libwayland-dev libxkbcommon-dev xorg-dev \
    libpulse-dev libssl-dev cppcheck doxygen graphviz \
    xvfb curl wget ca-certificates gnupg \
    && rm -rf /var/lib/apt/lists/*

# Vulkan SDK
RUN wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | apt-key add - \
    && wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list https://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list \
    && apt-get update && apt-get install -y vulkan-sdk \
    && rm -rf /var/lib/apt/lists/*

# vcpkg
ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git $VCPKG_ROOT \
    && $VCPKG_ROOT/bootstrap-vcpkg.sh
ENV PATH=$VCPKG_ROOT:$PATH

# Python deps
COPY ai_python/pyproject.toml /tmp/pyproject.toml
RUN python3.11 -m venv /opt/venv && \
    /opt/venv/bin/pip install --no-cache-dir -e /tmp/.[dev,notebooks]

WORKDIR /workspace
```

Dockerfile training (avec CUDA) :
```dockerfile
FROM nvidia/cuda:12.3.0-runtime-ubuntu22.04 AS base
# ... reprend base + ajoute torch CUDA build
RUN /opt/venv/bin/pip install --no-cache-dir --index-url https://download.pytorch.org/whl/cu121 torch==2.2.*
```

## Versions épinglées critiques

Tableau récap (à mettre à jour si bump) :

| Lib | Version épinglée | Raison du pin |
|-----|------------------|---------------|
| Vulkan SDK | 1.3.275+ | Features dynamic_rendering + RT stables |
| asio | 1.30+ | C++20 coroutines support |
| imgui | 1.90+ | Docking branch stabilisée |
| vk-bootstrap | 1.3.279+ | Vulkan 1.3 features |
| libtorch | 2.2.x | Compat CUDA 12.x + TorchScript stable |
| torch (Python) | 2.2.x | Match libtorch C++ pour ScriptModule cross-load |
| ray[rllib] | 2.10.x | API multi-agent stable |
| pettingzoo | 1.24.x | ParallelEnv API |
| pybind11 | 2.11+ | C++17 support complet |

## Mise à jour des deps

- **Dependabot** : weekly PR (config `.github/dependabot.yml`)
- Bump major → ADR obligatoire + test régression complet en CI
- Bump minor/patch → merge auto si CI verte + label `auto-merge`
- Tests stress recommandés après bump `torch` ou `vulkan-sdk`

## License / compliance

License du projet : **privé/propriétaire** (pas de fichier LICENSE).
Pour les deps :
- Toutes les deps utilisées sont sous license permissive (MIT, BSD, Apache 2.0, zlib)
- Vérifier `gui/assets/LICENSES.md` pour les assets libres (Kenney CC0, Quaternius CC0, etc.)
- Aucune dep GPL/AGPL dans la stack runtime
