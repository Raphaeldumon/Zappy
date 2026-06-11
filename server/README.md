# server/ — `zappy_server`

Owners: **P1 Léa** (core logic) · **P2 Marc** (networking & ops).

## Layout

```
core/          libzappy_core — pure game logic, no I/O
  types.hpp             Tile, Player, Egg, ResourceSet, Orientation
  world_state.*         toroidal map, teams, players, eggs, resources
  event_scheduler.*     deterministic tick-ordered event queue
  game_rules.hpp        elevation table, life constants, vision rules

net/           socket/client transport only
  client.*             connected-client state
  network_layer.*      accept/read/write/disconnect event loop

protocol/      Zappy wire parsing and formatting
  ai_handler.*         AI command parser and AI response formatting
  gui_handler.*        GUI request parser
  gui_emitter.*        GUI event/response formatting
  handshake.*          initial AI/GUI handshake parsing

runtime/       executable orchestration
  args.*              CLI parser (-p -x -y -n -c -f, --help)
  server.*            wires core, net, protocol and scheduler together
  main.cpp            entry point

tests/         CTest units using plain asserts
```

## Build & run

```bash
make                 # produces ./zappy_server
./zappy_server --help
./zappy_server -p 4242 -x 10 -y 10 -n red blue -c 4 -f 100
ctest --test-dir build -L server   # run server tests
```

## Boundaries

- `core/` must stay independent from sockets, file descriptors and protocol text.
- `net/` should not know Zappy game rules; it only moves lines in and out.
- `protocol/` owns parsing and formatting of the wire contract.
- `runtime/` is the glue layer. It is allowed to know about all other server modules.

The wire contract lives in `protocol/` and is **frozen** — change it only via an ADR.
