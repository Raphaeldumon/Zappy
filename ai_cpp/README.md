# ai_cpp/ — `zappy_ai`

Owners: **P5 Théo** (policy) · **P6 Yanis** (client plumbing / libtorch).

The runtime AI client. It connects to the server, speaks the AI protocol, and picks
actions — either from a trained policy (`models/current/model.pt`, loaded via libtorch)
or a rule-based fallback. Training itself lives in [`ai_python/`](../ai_python/).

## Layout

```
include/zappy/ai/agent.hpp   Agent interface (decide() -> Command)
src/args.*                   CLI parser (-p -n -h, --help)
src/main.cpp                 wires args -> (TODO) socket client + handshake + loop
tests/                       skeleton CTest units
```

## Build & run

```bash
make                 # produces ./zappy_ai
./zappy_ai --help
./zappy_ai -p 4242 -n red -h localhost
ctest --test-dir build -L ai
```

## Where to start (Sprint 1)

- **P6**: socket client to `host:port`, AI handshake (`WELCOME` -> team -> `CLIENT_NUM`
  -> `X Y`), line reader/writer. Then add libtorch as an optional dep (gated CMake
  option) for inference.
- **P5**: rule-based `Agent` baseline so the client plays without a checkpoint, plus
  the obs/action encoding shared with `ai_python/` (keep them in lockstep).

Action/observation encoding must match `ai_python/zappy_train/env/` exactly.
