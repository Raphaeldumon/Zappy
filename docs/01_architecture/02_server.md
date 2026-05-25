# 02 — Architecture serveur

## Contraintes sujet (rappel)

- Binary `zappy_server`, **C/C++/Rust** → choisi : **C++17/20**
- **Single process, single thread**
- **poll(2)** obligatoire (le sujet précise "must use poll" et déconseille le busy-waiting)
- Usage : `./zappy_server -p port -x width -y height -n team1 team2 ... -c clientsNb -f freq`
- Densités ressources fixées (`food=0.5, linemate=0.3, deraumere=0.15, sibur=0.1, mendiane=0.1, phiras=0.08, thystame=0.05`)
- Respawn ressources toutes les 20 unités de temps
- Client `GRAPHIC` = GUI authentifié

## Vue d'ensemble

```
                      ┌─────────────────────────────────────┐
                      │     main(int argc, char **argv)     │
                      │   - parse args (CLI11)              │
                      │   - load config                     │
                      │   - signal handler (SIGINT/USR1)    │
                      │   - Server::run()                   │
                      └──────────────┬──────────────────────┘
                                     │
                  ┌──────────────────▼──────────────────┐
                  │           class Server              │
                  │  ┌──────────────┐ ┌───────────────┐ │
                  │  │ NetworkLayer │ │   WorldState  │ │
                  │  │  - asio/poll │ │  - tiles      │ │
                  │  │  - clients   │ │  - players    │ │
                  │  │  - admin sock│ │  - eggs       │ │
                  │  └──────┬───────┘ │  - resources  │ │
                  │         │         └───────┬───────┘ │
                  │  ┌──────▼─────────────────▼───────┐ │
                  │  │       EventScheduler           │ │
                  │  │  - priority queue actions      │ │
                  │  │  - tick = 1/f sec              │ │
                  │  │  - resource respawn every 20   │ │
                  │  │  - incantation timer 300/f     │ │
                  │  └──────────────┬─────────────────┘ │
                  │                 │                   │
                  │  ┌──────────────▼─────────────────┐ │
                  │  │      ProtocolDispatcher        │ │
                  │  │   - AI protocol parser         │ │
                  │  │   - GUI protocol emitter       │ │
                  │  │   - Admin protocol             │ │
                  │  └──────────────┬─────────────────┘ │
                  │                 │                   │
                  │  ┌──────────────▼─────────────────┐ │
                  │  │       Metrics + Recorder       │ │
                  │  │   - prometheus exporter        │ │
                  │  │   - .zrec writer (optionnel)   │ │
                  │  └────────────────────────────────┘ │
                  └─────────────────────────────────────┘
```

## Layout des sources

```
server/
├── CMakeLists.txt
├── include/zappy/server/
│   ├── server.hpp
│   ├── world_state.hpp
│   ├── tile.hpp
│   ├── player.hpp
│   ├── egg.hpp
│   ├── resource.hpp
│   ├── event_scheduler.hpp
│   ├── network_layer.hpp
│   ├── client.hpp
│   ├── protocol_ai.hpp
│   ├── protocol_gui.hpp
│   ├── protocol_admin.hpp
│   ├── config.hpp
│   ├── recorder.hpp
│   └── metrics.hpp
├── src/
│   ├── main.cpp
│   ├── server.cpp
│   ├── world_state.cpp
│   ├── ...
│   └── ...
└── tests/
    ├── test_world_state.cpp
    ├── test_event_scheduler.cpp
    ├── test_protocol_ai.cpp
    ├── test_protocol_gui.cpp
    └── ...
```

## Modules en détail

### `world_state`
- Tableau 2D de `Tile`, indexation `(x,y) -> idx = y*W + x` avec wrap toroidal
- `std::vector<Player>` (id auto-incrément), `std::vector<Egg>`, `std::vector<Team>`
- Méthodes : `move_forward`, `turn_left/right`, `take_object`, `set_object`, `start_incantation`, `eject`, `fork`, `look(range)` retournant le tableau de tiles dans le cône de vision
- Vision = pyramide en avant : niveau 1 → 1+3 cases, niveau 2 → 1+3+5, etc.
- Wrap toroidal géré dans `Tile& at(int x, int y)` avec modulo positif

### `event_scheduler`
- `priority_queue<TimedEvent, vector, greater>` ordonné par `tick_target`
- Chaque action met un event dans la queue avec `tick_target = current_tick + (cost * f_inv)`
- À chaque iteration du run loop, on consomme tous les events dont `tick_target <= now`
- Respawn ressources : event récurrent toutes les 20 unités
- Incantation : event spécial qui réévalue les conditions à l'expiration (300/f sec)
- `fork` : event 42/f sec → ajoute un egg sur la tile

### `network_layer`
- `asio::io_context` en single-thread, basé sur `poll_reactor`
- Boucle principale :
  ```cpp
  while (running) {
      auto next_event_time = scheduler.next_event_time();
      auto poll_timeout = next_event_time - now;
      io_ctx.run_for(poll_timeout);
      scheduler.tick(now);
  }
  ```
- Active polling **interdit** : le serveur dort jusqu'au prochain event ou paquet réseau
- Sockets : 1 listening pour AI/GUI (port `-p`), 1 listening pour admin (port `-p + 1000`)
- Buffer commande par client : max 10 commandes en file (conformité sujet)

### `protocol_ai`
- Parser ligne par ligne (`\n` terminator)
- Mapping table : `{"Forward", &handle_forward}, {"Right", &handle_right}, ...`
- Bad command → réponse `"ko\n"`
- File de commandes par player (max 10), serveur dépile au rythme du `event_scheduler`
- Handshake :
  ```
  S -> "WELCOME\n"
  C -> "TEAM_NAME\n"
  S -> "CLIENT_NUM\n"  (nb slots libres)
  S -> "X Y\n"         (dimensions)
  ```
- Cas spécial `TEAM_NAME == "GRAPHIC"` → bascule sur `protocol_gui`

### `protocol_gui`
- Voir [`06_protocols.md`](06_protocols.md) pour la table exhaustive
- Stratégie push-only : le serveur émet l'événement granulaire à chaque changement
- Pas de re-broadcast complet de l'état, uniquement diffs
- À la connexion GUI : envoi du snapshot complet (`msz`, `mct`, `tna`, `pnw` pour chaque player, `enw` pour chaque egg, `sgt`)

### `protocol_admin` (bonus)
- Socket séparé `-p + 1000`
- Commandes texte : `pause`, `resume`, `set f <value>`, `kill <player_id>`, `spawn <res> <x> <y> <n>`, `snapshot`, `reload-config`
- Auth simple : token CLI `--admin-token <token>`

### `recorder` (bonus)
- Si flag `--record <path>`, ouvre un `.zrec` et écrit chaque event GUI émis
- Format : header magic `ZREC` + version u32 + frames `{u64 timestamp_ms; u32 len; char payload[]}`
- Voir [`docs/01_architecture/06_protocols.md#format-zrec`](06_protocols.md#format-zrec)

### `metrics`
- HTTP server intégré via `prometheus-cpp` sur port `9090`
- Métriques exposées :
  - `zappy_tick_total{}` (counter)
  - `zappy_players_alive{team}` (gauge)
  - `zappy_actions_processed_total{action}` (counter)
  - `zappy_action_latency_seconds{action}` (histogram)
  - `zappy_bytes_sent_total{client_type}` (counter)
  - `zappy_active_clients{team}` (gauge)
  - `zappy_incantations_total{level,result}` (counter)

## Mode simulation (training)

Compilation `-DZAPPY_SIMULATION_MODE=ON` → :
- `NetworkLayer` désactivé (stub)
- `EventScheduler` avance immédiatement au prochain event (pas de sleep)
- Sortie : lib statique `libzappy_sim.a` consommable depuis pybind11

## Performance cibles

| KPI | Cible |
|-----|-------|
| Ticks/sec normal mode | 500 (avec `f=500`) |
| Ticks/sec simulation mode | 1M+ (single env) |
| Mémoire RSS | <300 MB pour map 200x200 + 24 players + 4 teams |
| Latence action AI → réponse | <2 ms (hors temps d'attente du cost de l'action) |

## Tests

- `test_world_state.cpp` : tile wrap, vision cone, take/set object, elevation eligibility
- `test_event_scheduler.cpp` : ordering, cancel, tick precision
- `test_protocol_ai.cpp` : parsing, bad commands, max 10 in queue
- `test_protocol_gui.cpp` : event emission on state change, format conformity
- `test_recorder.cpp` : roundtrip write→read, version compatibility

## Signaux

- `SIGINT` : graceful shutdown (close sockets, flush logs, close recorder)
- `SIGUSR1` : dump snapshot vers `./snapshot-<timestamp>.json`
- `SIGUSR2` : reload config (mêmes valeurs que via socket admin)
