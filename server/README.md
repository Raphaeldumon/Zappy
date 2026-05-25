# server/ — `zappy_server`

Owners: **P1 Léa** (core logic) · **P2 Marc** (networking & ops).

## Layout

```
core/         libzappy_core — PURE game logic, no I/O (reused by sim_python)
  types.hpp           Tile, Player, Egg, ResourceSet, Orientation
  world_state.*       toroidal grid, players; TODO: move/look/take/set
  event_scheduler.*   deterministic tick-ordered event queue (FIFO ties)
  game_rules.hpp      elevation table, life constants, vision (TODO)
runtime/      the executable
  args.*              CLI parser (-p -x -y -n -c -f, --help)
  main.cpp            wires args -> WorldState -> (TODO) network + tick loop
tests/        skeleton CTest units (plain asserts; migrate to Catch2, ADR-006)
```

## Build & run

```bash
make                 # produces ./zappy_server
./zappy_server --help
./zappy_server -p 4242 -x 10 -y 10 -n red blue -c 4 -f 100
ctest --test-dir build -L server   # run core tests
```

## Where to start (Sprint 1)

- **P1**: flesh out `WorldState` (move_forward/turn/take/set/look with toroidal wrap)
  and `game_rules` elevation/vision. Target ≥70% coverage on `core/`.
- **P2**: add `runtime/network_layer.{hpp,cpp}` (asio, ADR-009), the AI/GUI line
  parsers (`runtime/protocol_*.{hpp,cpp}`), the handshake, then the poll loop in
  `main.cpp`. Recorder/metrics come in S3.

The wire contract lives in `protocol/` and is **frozen** — change it only via an ADR.
```
