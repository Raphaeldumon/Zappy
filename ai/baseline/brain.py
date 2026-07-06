"""BrainMixin: brain responsibilities of the AI.

Auto-split from the original monolith; bodies are byte-identical.
The build bundler (_bundle.py) re-stitches these into a single zappy_ai."""

import select
import signal
import time
from .models import State


class BrainMixin:
    def run(self) -> int:
        # Auto-reap forked children so spawned bots don't pile up as zombies.
        if hasattr(signal, "SIGCHLD"):
            signal.signal(signal.SIGCHLD, signal.SIG_IGN)

        self.connect()
        self.handshake()

        while self.running:
            # Block until the socket has data instead of busy-polling on a fixed
            # 2ms sleep. A server reply wakes us instantly (sub-ms), so the
            # reactive feed cycle (Look -> Take) no longer eats a 2ms latency tax
            # per command — that tax is what capped survival at high frequency.
            # The 50ms cap is just a floor so time-based logic (rebroadcast
            # intervals, fork cooldowns, gather timers) still ticks when idle.
            if self.sock is not None:
                try:
                    select.select([self.sock], [], [], 0.05)
                except (OSError, ValueError):
                    pass

            for line in self.read_available():
                if line:
                    self.handle_line(line)

            self.tick()

        if self.sock:
            self.sock.close()

        return 0

    def tick(self) -> None:
        if not self.running or self.state == State.DEAD:
            return

        # Drop a wedged front command (no reply within its timeout) so the
        # pipeline can't stall forever on a lost response.
        if self.pending:
            cmd, at, _ = self.pending[0]
            if time.time() - at > self.cmd_timeout(cmd):
                self.log(f"[AI] command timeout: {cmd}")
                self.pending.pop(0)
                # The lost reply may still arrive and shift onto the wrong entry;
                # stop trusting latency samples until the queue resyncs (empties).
                self.pipeline_desynced = True
        elif self.pipeline_desynced:
            # Queue drained with no outstanding commands -> FIFO is back in sync.
            self.pipeline_desynced = False

        # Opening team census: announce presence so everyone can size the team.
        self.census_maintain()

        # Keep the pipeline topped up with committed plan steps. Safe even while
        # other replies are outstanding — it's a deterministic path, not a
        # state-dependent decision.
        if self.send_next_plan_cmd():
            return

        # Everything below is a reactive decision that must see fresh world state,
        # so only act once the pipeline has drained (preserves single-pending
        # semantics for Look/Inventory/Take/Incantation/etc.).
        if self.has_pending():
            return

        self.prune_answered_reqs()

        if self.send_pending_ack():
            return

        if self.memory.force_look:
            self.memory.force_look = False
            self.send_cmd("Look")
            return

        if self.pending_follow_direction is not None:
            direction = self.pending_follow_direction
            self.pending_follow_direction = None
            self.set_follow_plan(direction)

            if self.send_next_plan_cmd():
                return

        if self.should_refresh_inventory():
            self.send_cmd("Inventory")
            return

        self.choose_state()
        self.run_state()

    def handle_line(self, line: str) -> None:
        self.log(f"[SERVER -> AI] {line}")

        if line == "dead":
            print("[AI] DEAD", file=sys.stderr)  # always: survival metric
            self.state = State.DEAD
            self.running = False
            return

        msg = self.parse_broadcast(line)
        if msg:
            direction, text = msg
            self.handle_team_message(direction, text)
            return

        if line.startswith("eject:"):
            self.memory.was_ejected = True
            self.plan.clear()
            self.memory.force_look = True
            return

        if line.startswith("Current level:"):
            try:
                self.memory.level = int(line.replace("Current level:", "").strip())
            except ValueError:
                pass

            print(
                f"[AI] LEVELUP {self.memory.level}", file=sys.stderr
            )  # always: metric
            self.pop_pending()

            # Manifest blitz: the stones for the NEXT level are already on the tile
            # and the 6 frozen players are still here -> fire the next Incantation
            # immediately, no Set, no re-gather. Ride straight to L8.
            if self.blitzing and self.memory.level < 8:
                self.broadcast_hold()  # keep the frozen mates frozen
                self.send_cmd("Incantation")
                self.waiting_incantation = True
                return

            was_frozen = time.time() < self.arrived_hold_until
            self.blitzing = False
            self.after_incantation_done()
            self.must_fork_after_gather_fail = False
            self.abort_gather("level changed", False)
            self.clear_following()
            # A frozen participant in someone's blitz: keep holding so we stay on
            # the tile for the next ritual instead of wandering after leveling.
            if was_frozen:
                self.arrived_hold_until = time.time() + self.incant_hold_window()
            return

        if line == "Elevation underway":
            return

        if line.lstrip("-").isdigit() and self.front_pending() == "Connect_nbr":
            self.memory.free_slots = int(line)
            self.pop_pending()
            return

        if line in ("ok", "ko"):
            cmd = self.pop_pending() or ""
            self.count_action(cmd)

            if line == "ok":
                self.mark_dirty(cmd)

            if cmd == "Fork" and line == "ok":
                self.memory.forks_done += 1
                self.memory.last_fork_at = time.time()
                self.must_fork_after_gather_fail = False
                # Hold the next census fork until this child announces (HELLO
                # clears it), or until the deadline if it never boots.
                if self.census_active():
                    self.census_fork_deadline = time.time() + self.census_child_timeout
                # Egg laid: launch the client that will hatch from it.
                self.spawn_child_for_egg()
                print(
                    f"[AI] fork ok -> egg laid, spawned client "
                    f"(forks_done={self.memory.forks_done})",
                    file=sys.stderr,
                )

            if cmd == "Incantation":
                # A failed ritual (e.g. a blitz mate left) ends the blitz; the
                # bot drops back to normal manifest decisions next tick.
                self.blitzing = False
                self.after_incantation_done()

            if line == "ko":
                if cmd.startswith("Take "):
                    self.failed_take(cmd.replace("Take ", "", 1))
                    return

                self.memory.inventory_dirty = True
                self.memory.force_look = True

            return

        if line.startswith("[") and line.endswith("]"):
            if self.is_inventory_line(line):
                self.memory.inventory = self.parse_inventory(line)
                self.memory.inventory_dirty = False
                self.memory.last_inventory_at = time.time()
                self.memory.actions_since_inventory = 0
                self.pop_pending()

                self.log(
                    "[AI] inventory updated: "
                    + " ".join(
                        f"{r}={self.memory.inventory.get(r, 0)}" for r in RESOURCES
                    )
                )
            else:
                self.memory.visible_tiles = self.parse_look(line)
                self.memory.last_look_at = time.time()
                self.memory.moves_since_look = 0
                self.memory.force_look = False
                self.pop_pending()

            return

        self.log(f"[AI] ignored: {line}")

    def handle_team_message(self, direction: int, text: str) -> None:
        if not text.startswith(f"{self.team}:"):
            return

        kind, fields = self.parse_team_payload(text)
        sender = self.get_int(fields, "from")

        if sender is None or sender == self.bot_id:
            return

        if kind == "HELLO":
            self.handle_hello(sender)
        elif kind == "GATHER":
            self.handle_gather(direction, fields, sender)
        elif kind == "GATHER_ACK":
            self.handle_ack(fields, sender)
        elif kind == "HOLD":
            self.handle_hold(fields, sender)
        elif kind == "ARRIVED":
            self.handle_arrived(fields, sender)
        elif kind == "MFOOD":
            self.handle_mfood(fields, sender)
        elif kind == "MRDY":
            self.handle_mrdy(fields, sender)
        elif kind == "MRDY2":
            self.handle_mrdy2(fields, sender)
        elif kind == "MCOME":
            self.handle_mcome(fields, sender)

    def choose_state(self) -> None:
        food = self.food()

        if food <= self.survive_min():
            self.abort_gather("low food", False)
            self.clear_following()
            self.state = State.SURVIVE
            return

        # Frozen for a teammate's incantation ritual (HOLD): hold position. Survival
        # above still overrides, so we never freeze into starvation.
        if time.time() < self.arrived_hold_until and not self.preparing_incantation:
            self.plan.clear()
            self.state = State.LOOK  # LOOK doesn't move us; we just wait on-tile
            return

        if self.preparing_incantation:
            self.state = (
                State.PREPARE_INCANTATION if self.stones_to_drop else State.INCANT
            )
            return

        if self.needs_look():
            self.state = State.LOOK
            return

        # Opening census: the elected forker builds the team to the blitz quorum
        # before anyone settles into the manifest collect (which never forks).
        # Bounded to the census window, so it stops the moment quorum is met.
        if self.census_active() and self.should_fork():
            self.state = State.REPRODUCE
            return

        # The blitz kicks in at L2: do the quick solo L1->L2 normally first, then
        # collect the L2->L8 manifest and blitz.
        if not self.manifest_gave_up and self.memory.level >= 2:
            self.manifest_choose()
            return

        if self.has_required_stones():
            players = REQ[self.memory.level]["players"]

            if players <= 1:
                self.state = State.PREPARE_INCANTATION
                return

            if self.active_req:
                self.state = State.CALL_TEAMMATES
                return

            if self.is_following():
                self.state = State.LOOK
                return

            if self.must_fork_after_gather_fail:
                self.state = State.REPRODUCE if self.should_fork() else State.FARM_FOOD
                return

            if self.in_gather_cooldown():
                self.state = State.REPRODUCE if self.should_fork() else State.FARM_FOOD
                return

            if food < self.gather_start_min():
                self.state = State.REPRODUCE if self.should_fork() else State.FARM_FOOD
                return

            self.state = State.CALL_TEAMMATES
            return

        self.abort_gather("stones not ready", False)
        self.clear_following()

        if self.should_fork():
            self.state = State.REPRODUCE
        elif self.visible_useful_resource():
            self.state = State.COLLECT
        else:
            self.state = State.EXPLORE

    def run_state(self) -> None:
        self.log(
            f"[AI] state={self.state.value} level={self.memory.level} food={self.food()} "
            f"actions_since_inventory={self.memory.actions_since_inventory} "
            f"moves_since_look={self.memory.moves_since_look}"
        )

        if self.state == State.SURVIVE:
            self.survive()
        elif self.state == State.LOOK:
            self.send_cmd("Look")
        elif self.state == State.COLLECT:
            self.collect()
        elif self.state == State.EXPLORE:
            self.explore()
        elif self.state == State.FARM_FOOD:
            self.farm_food()
        elif self.state == State.CALL_TEAMMATES:
            self.call_teammates()
        elif self.state == State.PREPARE_INCANTATION:
            self.prepare_incantation()
        elif self.state == State.INCANT:
            self.send_cmd("Incantation")
            self.waiting_incantation = True
        elif self.state == State.REPRODUCE:
            self.reproduce()
