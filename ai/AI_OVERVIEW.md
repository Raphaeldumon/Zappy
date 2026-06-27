# Zappy AI — Full Workflow & Design

A start-to-finish walkthrough of the Zappy AI bot
([`ai/baseline/zappy_ai_baseline.py`](baseline/zappy_ai_baseline.py), mirrored to
the gitignored `ai/zappy_ai`). It is a **100% rule-based state machine** — no
training, no neural net. One process drives one drone; teams are many such
processes.

---

## 1. The goal & the win condition

Each drone climbs **elevation levels 1 → 8**. The game ends the instant **6
players of one team reach level 8** (`world_state.cpp` `check_win`:
`count >= 6`) — the server emits `seg`, sets
`running_ = false`, and the process exits. So winning is a **race**: get six
drones to level 8 before anyone else.

Each elevation (`Incantation`) needs **N players of the same level + a specific
set of stones on the tile** ([`REQ`](baseline/zappy_ai_baseline.py#L19)):

| Level → | players | linemate | deraumere | sibur | mendiane | phiras | thystame |
|---------|---------|----------|-----------|-------|----------|--------|----------|
| 1→2 | 1 | 1 | – | – | – | – | – |
| 2→3 | 2 | 1 | 1 | 1 | – | – | – |
| 3→4 | 2 | 2 | – | 1 | – | 2 | – |
| 4→5 | 4 | 1 | 1 | 2 | – | 1 | – |
| 5→6 | 4 | 1 | 2 | 1 | 3 | – | – |
| 6→7 | 6 | 1 | 2 | 3 | – | 1 | – |
| 7→8 | 6 | 2 | 2 | 2 | 2 | 2 | 1 |

Two server facts the whole strategy leans on:
- An incantation elevates **every same-level player on the tile** (the player
  count is a *minimum*, not a cap).
- A successful ritual **consumes only that level's stones** from the tile,
  leaving the rest. (Verified server-side.)

---

## 2. Talking to the server (the protocol)

Plain-text, line-based, one TCP socket. Commands the bot sends and what it
expects back:

| Command | Server reply | Meaning |
|---------|--------------|---------|
| `Forward` / `Left` / `Right` | `ok` | move / turn |
| `Look` | `[tile0, tile1, ...]` | vision cone (see §6) |
| `Inventory` | `[food n, linemate n, ...]` | own stock |
| `Take <obj>` / `Set <obj>` | `ok` / `ko` | pick up / drop one item |
| `Broadcast <text>` | `ok` | sound to **all** players (§7) |
| `Incantation` | `Elevation underway` then `Current level: N` / `ko` | ritual |
| `Fork` | `ok` | lay an egg + free a team slot |
| `Connect_nbr` | `<int>` | **free** team slots (not active count) |
| `Eject` | `ok`/`ko` | shove players off the tile |

Unprompted server pushes: `message K, <text>` (a broadcast was heard),
`eject: K` (we got shoved), `dead` (we starved). Reply parsing lives in
[`handle_line`](baseline/zappy_ai_baseline.py#L495).

**Key blind spots the bot must work around:**
- **No command returns position.** The bot never knows its (x, y). All
  navigation toward teammates is done purely by broadcast **sound bearing**.
- **`Connect_nbr` returns free slots, not team size** — a bot cannot directly
  ask "how many of us are alive?".
- A received broadcast carries **no sender or team** — only `message K, <text>`.
  Team origin is by *convention* (every message is prefixed `<team>:`), and is
  spoofable. `K` is the only trustworthy bit (see §6).

---

## 3. Connection & handshake

[`connect`](baseline/zappy_ai_baseline.py#L181) →
[`handshake`](baseline/zappy_ai_baseline.py#L273):

1. Read `WELCOME`.
2. Send the team name.
3. Read free-slot count, then `X Y` map dimensions.
4. If `-f` was not given, **measure** the server frequency empirically
   ([`estimate_frequency`](baseline/zappy_ai_baseline.py#L311)): time
   `Connect_nbr` vs `Forward` round-trips; the delta is one action's wall-cost,
   and `freq ≈ 7 / delta` (an action costs `7/f` seconds of game time).

The frequency drives every timeout in the bot, because the server's clock — not
the bot's CPU — paces the game.

---

## 4. The main loop

[`run`](baseline/zappy_ai_baseline.py#L414) blocks on `select` (woken instantly
by a server reply, 50 ms idle cap so timers still tick), drains all available
lines into [`handle_line`](baseline/zappy_ai_baseline.py#L495), then calls
[`tick`](baseline/zappy_ai_baseline.py#L446).

### Pipelining (the `pending` FIFO)
The server buffers up to **`MAX_COMMAND_QUEUE = 10`** commands per client. The
bot keeps a FIFO [`self.pending`](baseline/zappy_ai_baseline.py#L83) (capped at
8) of commands awaiting a reply (the server answers **in order**). Two regimes:

- **Deterministic movement plans** (`Forward Forward Right …`) are pushed
  several-at-once via [`send_next_plan_cmd`](baseline/zappy_ai_baseline.py#L1590)
  so the server's action queue never idles between steps.
- **Reactive decisions** (Look→Take, Inventory, Incantation) wait for the
  pipeline to drain (`if self.has_pending(): return`) so they always see fresh
  world state.

A wedged front command is dropped after a per-command timeout
([`cmd_timeout`](baseline/zappy_ai_baseline.py#L264)) so a lost reply can't stall
the bot forever.

### `tick` order of priority
1. timeout-drop a stale front command,
2. top up the movement pipeline,
3. (drain) — bail if anything still pending,
4. send a queued `GATHER_ACK` if owed,
5. forced `Look` (vision invalidated),
6. follow a broadcast bearing,
7. refresh `Inventory` if stale,
8. **`choose_state()` then `run_state()`** — the brain.

---

## 5. The world model

[`Memory`](baseline/zappy_ai_baseline.py#L43) is the bot's entire belief state:
level, inventory, last `Look` grid, free slots, plus freshness counters
(`actions_since_inventory`, `moves_since_look`) and timestamps used to decide
when vision/inventory have gone stale and must be re-queried. Vision and
inventory are **cached** and only refreshed when counters/timers say they're
stale — re-`Look`ing every action would burn the action budget.

---

## 6. Vision & navigation

`Look` returns a flattened cone of tiles, index 0 = current tile, widening
forward. [`tile_pos`](baseline/zappy_ai_baseline.py#L1604) converts a flat index
into `(distance_forward, lateral_offset)`;
[`plan_to_tile`](baseline/zappy_ai_baseline.py#L1618) turns that into a
`Left/Right/Forward` sequence. [`move_to_tile`](baseline/zappy_ai_baseline.py#L1567)
folds the terminal action (e.g. `Take food`) into the **same** pipelined burst so
arriving doesn't cost an extra Look+Take round-trip.

### Sound bearing (the only way to find teammates)
A heard broadcast comes with `K` (0–8), the direction of the source **in the
listener's own frame**: `0` = same tile, `1` = straight ahead, then **clockwise**
(`3` = right, `5` = behind, `7` = left).
[`broadcast_plan`](baseline/zappy_ai_baseline.py#L1657) steps so the source moves
toward the front; the leader keeps rebroadcasting and the follower re-aims each
time. Because homing is coarse (8 directions) and the leader may itself move,
convergence is **slow and oscillating** — bearing 0 (exact co-location) is rare.
This single fact shaped the entire coordination design.

**Bearing 0 is the one server-authoritative signal of physical co-location** —
it's the backbone of the ARRIVED handshake below.

---

## 7. The state machine

[`choose_state`](baseline/zappy_ai_baseline.py#L612) picks a state by strict
priority each tick; [`run_state`](baseline/zappy_ai_baseline.py#L682) executes it.

| State | When | Does |
|-------|------|------|
| `SURVIVE` | food ≤ `survive_min` | drop everything, find & eat food |
| (HOLD) | frozen for a mate's ritual | stay on tile (LOOK, no move) |
| `PREPARE_INCANTATION` / `INCANT` | have stones, ready | `Set` each stone, then `Incantation` |
| `LOOK` | vision stale | refresh the cone |
| `CALL_TEAMMATES` | have stones, need ≥2 players | run the gather protocol (§8) |
| `COLLECT` | a needed stone is visible | walk to it, `Take` |
| `FARM_FOOD` | need food cushion | walk to food, `Take` |
| `REPRODUCE` | low-level & fed | `Fork` + spawn a child (§9) |
| `EXPLORE` | nothing useful visible | wander (biased forward) |
| `DEAD` | got `dead` | stop |

**Survival always preempts everything** — the bot never freezes or coordinates
itself into starvation.

### Food economy (the binding constraint)
`STARTING_FOOD = 10`, and food drains continuously (~8 food/sec at f=1000). A
stationary bot starves in roughly `food × 0.126` seconds. Survival is flat
(`survive_food = 8`, `survive_per_level = 0`): level-scaled survival was tried
and *regressed* — raising the floor just makes valuable high-level bots farm
forever and never coordinate. Food, not stones, is what's scarce.

---

## 8. Coordination handshakes (how bots find each other)

Getting N same-level bots onto one tile, position-blind, is the hard part. Four
broadcast handshakes solve it. They are the convergence primitives the blitz
(§10) drives, and also back the last-resort per-level fallback (§10):

- **GATHER / GATHER_ACK** — a leader with the stones opens a request
  (`start_gather`), broadcasting `GATHER:req=…:level=…:need=…`. Same-level,
  fed teammates reply `GATHER_ACK` and start homing on the bearing. Lowest
  `bot_id` wins if two leaders collide
  ([`handle_gather`](baseline/zappy_ai_baseline.py#L1044)).
- **ARRIVED (the big unlock)** — `Look` shows `"player"` with **no level**, so a
  leader can't tell a real same-level teammate from a passing level-1 baby, and
  firing on the wrong count guarantees a `ko`. Instead, when the leader's
  broadcast reaches a follower as **bearing 0**, the follower *knows* it's on the
  exact tile and announces `ARRIVED`. The leader counts **distinct same-level
  ARRIVED senders** and incants only on that count
  ([`present = 1 + len(self.arrived_at_leader)`](baseline/zappy_ai_baseline.py#L886)).
  This took ritual success from ~14% to ~92%.
- **HOLD** — before its multi-step `Set…Set…Incantation`, the leader broadcasts
  `HOLD` so assembled teammates **freeze on-tile** instead of wandering off and
  dropping the count below `need`. Only bots actually following *this* leader
  freeze ([`handle_hold`](baseline/zappy_ai_baseline.py#L1010)), else a map-wide
  broadcast would freeze and starve non-participants.

---

## 9. Reproduction, forking & eggs

Server `Fork` only **lays an egg and frees a slot** — nothing hatches until a
*new client connects* with the team name. So on every `Fork ok` the bot
**spawns a fresh process**
([`spawn_child_for_egg`](baseline/zappy_ai_baseline.py#L388), detached
`subprocess.Popen`) to occupy that egg; otherwise eggs rot and the population
never grows. `SIGCHLD` is ignored so children don't become zombies.

Forking is **disciplined** ([`should_fork`](baseline/zappy_ai_baseline.py#L1701)):
only while **level ≤ 2**, only when well-fed, and rate-limited per bot. A
level-3+ bot is valuable and food-bound — forking costs it food *and* adds a
competitor for scarce food, while the baby won't grow up in time to help. Lean
forking kept attrition low, which is what let a high-level cohort persist and
climb to level 8.

### The opening census — `HELLO` and the fork election
The blitz (§10) needs **≥ `census_target` (default 6) reachable mates**, but a
bot can't ask the server its team size — `Connect_nbr` returns *free slots*, not
the active count (§2). So the team **self-counts by broadcast**:

- On boot every bot broadcasts **`HELLO:from=<bot_id>`**
  ([`broadcast_hello`](baseline/zappy_ai_baseline.py#L1661)) and re-emits it every
  `hello_interval` (`60/freq`, min 1 s) while the census is open. Everyone unions
  the sender ids into [`team_members`](baseline/zappy_ai_baseline.py#L209). On the
  **first** sighting of a new id the bot echoes one HELLO back
  ([`handle_hello`](baseline/zappy_ai_baseline.py#L1665)) so the newcomer learns
  about the bots already present — bounded to first-sighting so it can't storm.

- **Fork election:** the bot with the **lowest known `bot_id`** is the sole
  forker ([`is_census_forker`](baseline/zappy_ai_baseline.py#L1684)). Only it
  forks — *past the normal per-bot cap* — until the union reaches
  `census_target`, then **nobody forks**
  ([`should_fork` census branch](baseline/zappy_ai_baseline.py#L1721)). The
  observed member count is the global limiter, so total forks ≈
  `quorum − initial_bots`. Forked children and late siblings HELLO on boot and
  join the union; their ids are **time-seeded** (always larger) so they never
  unseat the elected forker — the election is stable without a handshake.

Two pacing guards keep the count honest
([`census_maintain`](baseline/zappy_ai_baseline.py#L1689) /
[`should_fork`](baseline/zappy_ai_baseline.py#L1701)):
- a **settle window** (`census_settle`, 3 s) before a bot will act as forker, so
  a just-booted bot doesn't fork while it still (wrongly) thinks it's alone — a
  singleton is always its own minimum id;
- after each `Fork ok` the next fork **holds** until the child announces via
  HELLO ([`census_fork_deadline`](baseline/zappy_ai_baseline.py#L663)), or until
  `census_child_timeout` (4 s) if the child never boots — so one slow egg can't
  inflate the population, and a deadbeat egg can't stall the census.

The whole census is bounded by `census_timeout` (30 s); if quorum never forms it
closes and `should_fork` reverts to the **original per-bot cap** (`max_forks`),
the same fallback the manifest blitz uses on a degenerate map (§10).

---

## 10. The Manifest Blitz — the strategy

**The one and only strategy** (no on/off switch). Measured: **~30 s to level 8,
4/4 win rate** at f=1000 / 20×20 / 8 bots; robust across 10×10–40×40 maps. It
exploits the two server facts from §1: drop *all* stones once, then incant
repeatedly — each ritual eats only its level's share, and all six players level
every time.

After the quick solo **L1→L2**, every bot enters
[`manifest_choose`](baseline/zappy_ai_baseline.py#L1376), a phased rendezvous:

1. **`collect`** — every bot races to gather the **full L2→L8 stone manifest**
   (`manifest_target` = the sum of every level's requirement: 8 linemate, 8
   deraumere, 10 sibur, 5 mendiane, 6 phiras, 1 thystame = **38 stones**). The
   **first** to complete elects itself **anchor** and broadcasts `MFOOD`.
2. **`bank`** — on hearing `MFOOD`, everyone stops collecting and fills food to
   **`food_reserve` (default 200)**.
3. **`ready`** — banked bots broadcast `MRDY`/`MRDY2`. The anchor counts ready
   mates; when `len(ready_too) + 1 ≥ 6`
   ([the `+1` is the anchor itself](baseline/zappy_ai_baseline.py#L1420)) it
   fires `MCOME` and opens a 6-player gather.
4. **`converge`** — the cohort walks to the anchor **on full food tanks**,
   homing on the anchor's gather bearing; arrival is tracked by **ARRIVED**
   (§8), not the level-blind player count.
5. **`blitz`** — with 6 ARRIVED, the anchor
   ([`start_manifest_blitz`](baseline/zappy_ai_baseline.py#L1461)) drops all 38
   stones and fires `Incantation`. On each `Current level: N` the handler
   immediately fires the next `Incantation`
   ([blitz driver](baseline/zappy_ai_baseline.py#L528)) — L2→L8 runs in ~3 s.

### Why `food_reserve` is the load-bearing knob
Convergence is slow and oscillating (§6), so the anchor must **hold the rally
for ~25 s** while six bodies trickle onto its tile. At ~8 food/sec, hold time ≈
`reserve / 8`, so **200 ≈ 25 s** — just enough. Below ~140 the rally starves
before six assemble; above 200 just wastes foraging time. It's **map-size
independent**. Two supporting fixes make it work: the anchor's gather is
**persistent** (never resets the ARRIVED count — resetting was the old
"997-abort thrash"), and the anchor **does not bail** at the normal food-abort
floor, riding down to `survive_min` while eating tile food
([`call_teammates`](baseline/zappy_ai_baseline.py#L862)).

Anchor election is deterministic under contention: any bot hearing an
`MFOOD`/`MRDY` from a **lower `bot_id`** demotes itself and rallies to that one
([`manifest_demote_to`](baseline/zappy_ai_baseline.py#L739)). If no anchor ever
emerges (e.g. the map lacks a thystame, or fewer than 6 mates are reachable)
within `manifest_timeout`, the bot sets `manifest_gave_up` and plays the simple
per-level gather (§8) as an automatic last-resort safety — not a selectable mode,
just so a degenerate map doesn't brick the team forever.

### Broadcast vocabulary
| Message | From → meaning |
|---------|----------------|
| `HELLO` | any bot on boot: census ping — union the `from` id, elect the forker (§9) |
| `GATHER` / `GATHER_ACK` | leader opens rally / teammate accepts |
| `ARRIVED` | follower is physically on the leader's tile (bearing 0) |
| `HOLD` | leader: freeze, ritual starting |
| `MFOOD` | anchor: stop collecting, bank food |
| `MRDY` / `MRDY2` | anchor fed+ready / member fed+ready |
| `MCOME` | anchor: launch, converge now |

All carry `:level=…:from=<bot_id>` and are filtered by the `<team>:` prefix.

---

## 11. Tuning knobs & environment

| Var / field | Default | Effect |
|-------------|---------|--------|
| `ZAPPY_RESERVE` | 200 | banked food before converging (the main dial) |
| `ZAPPY_TEAM_TARGET` | 6 | census quorum — the elected forker forks until the team reaches this (§9) |
| `ZAPPY_CENSUS_TIMEOUT` | 30 | seconds the census stays open before falling back to `max_forks` |
| `max_forks` | 3 | per-bot fork cap (census fallback / `manifest_gave_up`) |
| `survive_food` | 8 | flat survival floor |
| `-f <freq>` | measured | server frequency; practical ceiling ~1000–1500 |

**Frequency ceiling:** the bot is **I/O-wait-bound** (blocked on the server's
per-action delay), not CPU-bound. Past ~f=1500 a single
action's wall-clock exceeds the `7/f` budget and the bot starves. Stick to
f ≤ 1000.

---

## 12. Observability

The bot runs **quiet by default** (per-action logging costs ms/action and
starves at high f). `-v` enables the full `[AI → SERVER]` / `[SERVER → AI]`
trace. Always-on lifecycle markers regardless of `-v`: **`LEVELUP <n>`**,
**`[AI] DEAD`**, **`spawned child`** — grep these to score quiet runs.

---

## 13. One-line summary

Solo-climb to L2 → every bot hoards the full 38-stone L2→L8 manifest → first done
becomes the anchor → the team banks a big food reserve → six converge on full
tanks via sound-homing + the ARRIVED handshake → the anchor drops everything and
incants six times back-to-back → **6 drones hit level 8 in ~30 s and the game is
won.**
