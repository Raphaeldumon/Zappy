"""GatherMixin: gather responsibilities of the AI.

Auto-split from the original monolith; bodies are byte-identical.
The build bundler (_bundle.py) re-stitches these into a single zappy_ai."""

import time
from typing import Dict
from .constants import REQ
from .models import State


class GatherMixin:
    def gather_ack_window(self) -> float:
        return max(2.0, 80.0 / self.frequency)

    def gather_rebroadcast_interval(self) -> float:
        return max(0.6, 35.0 / self.frequency)

    def gather_arrival_timeout(self) -> float:
        return max(8.0, 300.0 / self.frequency)

    def incant_hold_window(self) -> float:
        # Seconds a teammate freezes after a HOLD, covering the leader's
        # Set+Incantation cycles so the on-tile player count holds.
        return max(3.0, 150.0 / self.frequency)

    def gather_cooldown(self) -> float:
        return max(4.0, 180.0 / self.frequency)

    def in_gather_cooldown(self) -> bool:
        return (
            self.last_gather_giveup_at > 0
            and time.time() - self.last_gather_giveup_at < self.gather_cooldown()
        )

    def confirmed_players(self) -> int:
        return 1 + len(self.active_acks) if self.active_req else 1

    def locked_leader(self) -> bool:
        return bool(
            self.active_req
            and self.active_need > 0
            and self.confirmed_players() >= self.active_need
        )

    def start_gather(self, need: int) -> None:
        self.gather_counter += 1
        self.gather_attempts += 1
        self.active_req = f"{self.bot_id}-{self.memory.level}-{self.gather_counter}"
        self.active_level = self.memory.level
        self.active_need = need
        self.active_acks.clear()
        self.arrived_at_leader.clear()
        self.gather_started_at = time.time()
        self.gather_last_broadcast_at = 0
        self.gather_arrival_started_at = 0
        self.clear_following()

        self.log(
            f"[AI] new gather req={self.active_req} need={need} attempt={self.gather_attempts}",
        )

        self.broadcast_gather()

    def broadcast_gather(self) -> bool:
        if not self.active_req:
            return False

        msg = (
            f"{self.team}:GATHER:"
            f"req={self.active_req}:"
            f"level={self.active_level}:"
            f"need={self.active_need}:"
            f"from={self.bot_id}"
        )

        if self.send_cmd(f"Broadcast {msg}"):
            self.gather_last_broadcast_at = time.time()
            return True

        return False

    def broadcast_hold(self) -> bool:
        # "Everyone of my level on my tile: freeze, I'm about to incant."
        msg = f"{self.team}:HOLD:level={self.memory.level}:from={self.bot_id}"
        return self.send_cmd(f"Broadcast {msg}")

    def abort_gather(self, reason: str, giveup: bool) -> None:
        if not self.active_req:
            return

        self.log(f"[AI] abort gather req={self.active_req}: {reason}")

        self.active_req = None
        self.active_level = 0
        self.active_need = 0
        self.active_acks.clear()
        self.gather_started_at = 0
        self.gather_last_broadcast_at = 0
        self.gather_attempts = 0
        self.gather_arrival_started_at = 0

        if giveup:
            self.last_gather_giveup_at = time.time()
            self.clear_following()

            if self.need_more_mates():
                self.must_fork_after_gather_fail = True
                self.log("[AI] gather failed -> switch objective to FARM_FOOD/FORK")

    def call_teammates(self) -> None:
        now = time.time()
        need = REQ[self.memory.level]["players"]
        # Manifest anchor wants a full 6-stack so one drop+blitz rides L2->L8.
        manifest_anchor = self.has_full_manifest()
        if manifest_anchor:
            need = 6

        # The manifest anchor holds the rally on its banked reserve — it must NOT
        # bail at gather_abort_min (that was the 813-abort thrash). It rides down to
        # survive_min (choose_state's survival preempt), eating any tile food first.
        if self.food() <= self.gather_abort_min() and not manifest_anchor:
            self.abort_gather("food too low while waiting teammates", True)
            self.state = State.SURVIVE
            self.survive()
            return

        if self.memory.visible_tiles and "food" in self.memory.visible_tiles[0]:
            self.send_cmd("Take food")
            return

        # Count only same-level teammates that announced ARRIVED (physically on my
        # tile, server-verified via bearing 0) — NOT players_on_tile(), which is
        # level-blind and fires on passing level-1 babies -> guaranteed ko.
        present = 1 + len(self.arrived_at_leader)

        if present < need:
            self.incant_hold_broadcasts = 0

        if present >= need:
            # Freeze teammates BEFORE the multi-step ritual, else they wander off
            # mid-Set/Incantation and the count drops below need -> ko. Broadcast
            # HOLD a couple times so everyone latches a freeze, then incant.
            if self.incant_hold_broadcasts < 2:
                if self.broadcast_hold():
                    self.incant_hold_broadcasts += 1
                return
            self.incant_hold_broadcasts = 0
            self.abort_gather("enough ARRIVED on tile", False)
            if manifest_anchor:
                self.start_manifest_blitz()
            else:
                self.prepare_incantation()
            return

        if not self.active_req:
            self.gather_attempts = 0
            self.start_gather(need)
            return

        if manifest_anchor:
            # Persistent rally: convergence is tracked by ARRIVED (physical, set in
            # handle_arrived), which only accumulates if we DON'T reset the gather.
            # So NEVER restart/abort on ACK count here (that wiped the count, the
            # 997-abort thrash). Just keep the bearing alive and wait for bodies.
            if now - self.gather_last_broadcast_at > self.gather_rebroadcast_interval():
                self.broadcast_gather()
                return
            if now - self.gather_started_at > max(90.0, 90000.0 / self.frequency):
                self.abort_gather("manifest rally timed out", True)
                self.manifest_gave_up = True
                self.state = State.EXPLORE
                return
            self.send_cmd("Look")
            return

        confirmed = self.confirmed_players()

        if confirmed >= need:
            if self.gather_arrival_started_at <= 0:
                self.gather_arrival_started_at = now
                self.log(
                    f"[AI] enough ACK for req={self.active_req} confirmed={confirmed}/{need}; "
                    f"locked leader, waiting on tile",
                )

            if now - self.gather_last_broadcast_at > self.gather_rebroadcast_interval():
                self.broadcast_gather()
                return

            # The manifest anchor must assemble 6 scattered collectors — give it
            # far more patience than a normal 2-player gather, and keep waiting
            # (eating on tile) rather than scattering to farm/fork.
            arrival_timeout = self.gather_arrival_timeout() * (
                8 if manifest_anchor else 1
            )
            if now - self.gather_arrival_started_at > arrival_timeout:
                self.abort_gather("mates did not arrive on tile", True)
                self.state = State.REPRODUCE if self.should_fork() else State.FARM_FOOD
                self.run_state()
                return

            self.send_cmd("Look")
            return

        if now - self.gather_started_at >= self.gather_ack_window():
            if self.gather_attempts < 3:
                self.start_gather(need)
                return

            self.abort_gather(f"not enough ACK ({confirmed}/{need})", True)
            self.state = State.REPRODUCE if self.should_fork() else State.FARM_FOOD
            self.run_state()
            return

        if now - self.gather_last_broadcast_at > self.gather_rebroadcast_interval():
            self.broadcast_gather()
            return

        self.send_cmd("Look")

    def handle_gather(
        self, direction: int, fields: Dict[str, str], sender: int
    ) -> None:
        req_id = fields.get("req", "")
        level = self.get_int(fields, "level")

        if not req_id or level is None:
            return

        self.memory.last_gather_at = time.time()

        if self.locked_leader():
            self.log(
                f"[AI] locked gather leader req={self.active_req} "
                f"confirmed={self.confirmed_players()}/{self.active_need}; "
                f"ignore gather from={sender}",
            )
            return

        if (
            self.is_following()
            and self.following_leader is not None
            and sender > self.following_leader
        ):
            return

        if self.active_req:
            if sender < self.bot_id:
                self.abort_gather(
                    f"lower bot_id leader wins before lock: {sender} < {self.bot_id}",
                    False,
                )
            else:
                return

        if not self.can_answer_gather(level, sender):
            return

        self.following_req = req_id
        self.following_leader = sender
        self.following_until = time.time() + self.gather_arrival_timeout()
        self.queue_ack(req_id, level, sender)
        self.pending_follow_direction = direction

        # Bearing 0 from our leader = we are physically on its exact tile. Announce
        # ARRIVED so it can count us as a confirmed same-level body. Re-announced on
        # every direction-0 rebroadcast (self-throttled), so a lost one self-heals.
        if direction == 0:
            self.send_cmd(
                f"Broadcast {self.team}:ARRIVED:req={req_id}:level={level}:from={self.bot_id}"
            )

    def handle_ack(self, fields: Dict[str, str], sender: int) -> None:
        req_id = fields.get("req", "")
        level = self.get_int(fields, "level")

        if self.active_req and req_id == self.active_req and level == self.memory.level:
            if sender not in self.active_acks:
                self.log(
                    f"[AI] GATHER_ACK req={req_id} from={sender} "
                    f"confirmed={1 + len(self.active_acks) + 1}/{self.active_need}",
                )

            self.active_acks[sender] = time.time()

    def handle_arrived(self, fields: Dict[str, str], sender: int) -> None:
        # A same-level teammate reports it's physically on my gather tile.
        req_id = fields.get("req", "")
        level = self.get_int(fields, "level")
        if self.active_req and req_id == self.active_req and level == self.memory.level:
            if sender not in self.arrived_at_leader:
                self.log(
                    f"[AI] ARRIVED req={req_id} from={sender} "
                    f"present={1 + len(self.arrived_at_leader) + 1}/{self.active_need}",
                )
            self.arrived_at_leader.add(sender)

    def handle_hold(self, fields: Dict[str, str], sender: int) -> None:
        level = self.get_int(fields, "level")
        if level != self.memory.level:
            return
        if self.preparing_incantation or self.waiting_incantation:
            return
        if self.food() <= self.survive_min():
            return  # too hungry to wait; survival wins
        # Only freeze if we're actually converging on THIS leader — otherwise a
        # map-wide broadcast would freeze distant non-participants and starve them.
        if not (self.is_following() and self.following_leader == sender):
            return
        self.arrived_hold_until = time.time() + self.incant_hold_window()

    def can_answer_gather(self, level: int, leader: int) -> bool:
        return (
            leader != self.bot_id
            and level == self.memory.level
            and not self.preparing_incantation
            and not self.waiting_incantation
            and self.food() > self.survive_min()
        )

    def queue_ack(self, req_id: str, level: int, leader: int) -> None:
        if req_id in self.answered_reqs or self.pending_ack_req == req_id:
            return

        self.pending_ack_req = req_id
        self.pending_ack_level = level
        self.pending_ack_leader = leader

        self.log(f"[AI] queued GATHER_ACK req={req_id} leader={leader}")

    def send_pending_ack(self) -> bool:
        if not self.pending_ack_req:
            return False

        if (
            self.food() <= self.survive_min()
            or self.preparing_incantation
            or self.waiting_incantation
        ):
            self.pending_ack_req = None
            return False

        msg = (
            f"{self.team}:GATHER_ACK:"
            f"req={self.pending_ack_req}:"
            f"level={self.pending_ack_level}:"
            f"from={self.bot_id}"
        )

        if not self.send_cmd(f"Broadcast {msg}"):
            return False

        self.answered_reqs[self.pending_ack_req] = time.time()
        self.pending_ack_req = None
        self.pending_ack_level = 0
        self.pending_ack_leader = None
        return True

    def prune_answered_reqs(self) -> None:
        now = time.time()
        ttl = max(30.0, 1000.0 / self.frequency)
        self.answered_reqs = {
            req: t for req, t in self.answered_reqs.items() if now - t <= ttl
        }

    def clear_following(self) -> None:
        self.following_req = None
        self.following_leader = None
        self.following_until = 0
        self.pending_follow_direction = None

    def is_following(self) -> bool:
        if not self.following_req:
            return False

        if time.time() > self.following_until:
            self.clear_following()
            return False

        return True
