"""ManifestMixin: manifest responsibilities of the AI.

Auto-split from the original monolith; bodies are byte-identical.
The build bundler (_bundle.py) re-stitches these into a single zappy_ai."""

import time
from typing import Dict, List
from .constants import REQ, STONES
from .models import State


class ManifestMixin:
    def broadcast_focus_food(self) -> None:
        self.last_focus_bcast = time.time()
        self.send_cmd(
            f"Broadcast {self.team}:MFOOD:level={self.memory.level}:from={self.bot_id}"
        )

    def broadcast_ready(self) -> None:
        self.last_ready_bcast = time.time()
        self.send_cmd(
            f"Broadcast {self.team}:MRDY:level={self.memory.level}:from={self.bot_id}"
        )

    def broadcast_ready_too(self) -> None:
        self.last_ready_bcast = time.time()
        self.send_cmd(
            f"Broadcast {self.team}:MRDY2:level={self.memory.level}:from={self.bot_id}"
        )

    def broadcast_come_now(self) -> None:
        self.send_cmd(
            f"Broadcast {self.team}:MCOME:level={self.memory.level}:from={self.bot_id}"
        )

    def manifest_demote_to(self, anchor_id: int) -> None:
        # A lower-id anchor exists -> stop being an anchor, rally to it instead.
        self.manifest_is_anchor = False
        self.manifest_anchor = anchor_id
        self.ready_too.clear()
        self.abort_gather("defer to lower-id manifest anchor", False)
        if self.manifest_phase in ("collect", "bank", "ready", "converge"):
            if self.manifest_phase == "collect":
                self.manifest_phase = "bank"

    def handle_mfood(self, fields: Dict[str, str], sender: int) -> None:
        if self.get_int(fields, "level") != self.memory.level:
            return
        if self.manifest_is_anchor:
            if sender < self.bot_id:
                self.manifest_demote_to(sender)
            return
        # Member: stop collecting, rally to this anchor, start banking food.
        self.manifest_anchor = sender
        if self.manifest_phase == "collect":
            self.manifest_phase = "bank"

    def handle_mrdy(self, fields: Dict[str, str], sender: int) -> None:
        # Anchor announcing it's fed + ready (also gives us its bearing).
        if self.get_int(fields, "level") != self.memory.level:
            return
        if self.manifest_is_anchor:
            if sender < self.bot_id:
                self.manifest_demote_to(sender)
            return
        self.manifest_anchor = sender

    def handle_mrdy2(self, fields: Dict[str, str], sender: int) -> None:
        # A member reports it's fed + ready.
        if (
            self.manifest_is_anchor
            and self.get_int(fields, "level") == self.memory.level
        ):
            self.ready_too.add(sender)

    def handle_mcome(self, fields: Dict[str, str], sender: int) -> None:
        # Launch: converge on the anchor (its GATHER drives the actual follow).
        if self.manifest_is_anchor:
            return
        if self.get_int(fields, "level") != self.memory.level:
            return
        self.manifest_anchor = sender
        self.manifest_phase = "converge"

    def manifest_target(self, stone: str) -> int:
        # Total of this stone needed to ritual from our current level all the way
        # to L8 (one ritual per level; each elevates all 6 players).
        return sum(REQ[lvl].get(stone, 0) for lvl in range(self.memory.level, 8))

    def has_full_manifest(self) -> bool:
        return all(
            self.memory.inventory.get(s, 0) >= self.manifest_target(s) for s in STONES
        )

    def manifest_drop_list(self) -> List[str]:
        out: List[str] = []
        for s in STONES:
            out += [s] * self.manifest_target(s)
        return out

    def manifest_visible(self) -> bool:
        return any(
            item in STONES and self.need_stone(item)
            for tile in self.memory.visible_tiles
            for item in tile
        )

    def manifest_timeout(self) -> float:
        # If no anchor emerges (e.g. no thystame on the map) within this, give up
        # the strategy and play normally so the team isn't stuck forever.
        return max(300.0, 200000.0 / self.frequency)

    def manifest_choose(self) -> None:
        # Phased rendezvous. choose_state already handled survival/HOLD/preparing.
        now = time.time()
        phase = self.manifest_phase

        # ---- COLLECT: race to complete the full L2->L8 stone set -------------
        if phase == "collect":
            if self.has_full_manifest():
                # First to complete becomes the anchor: tell the team to stop
                # collecting and bank food, then bank ourselves.
                self.manifest_is_anchor = True
                self.manifest_phase = "bank"
                self.broadcast_focus_food()
                self.state = State.FARM_FOOD
                return
            # keep a fat food buffer while collecting (collection is cheap)
            if self.food() < self.manifest_food_floor:
                self.state = State.FARM_FOOD
                return
            if time.time() - self.manifest_started_at > self.manifest_timeout():
                self.manifest_gave_up = True  # no anchor ever emerged -> normal play
                self.state = State.EXPLORE
                return
            self.state = State.COLLECT if self.manifest_visible() else State.EXPLORE
            return

        # ---- BANK: everyone fills food to a reserve before converging --------
        if phase == "bank":
            if self.food() < self.food_reserve:
                self.state = State.FARM_FOOD
                return
            self.manifest_phase = "ready"  # banked -> signal readiness
            self.state = State.LOOK
            return

        # ---- READY: fed and waiting; anchor synchronises the launch ----------
        if phase == "ready":
            if self.manifest_is_anchor:
                # Re-bank if our reserve ran down while waiting for stragglers.
                if self.food() < self.food_abort:
                    self.manifest_phase = "bank"
                    self.ready_too.clear()
                    self.state = State.FARM_FOOD
                    return
                # Count only members still heartbeating (census liveness): a
                # READY_TOO from a bot that has since starved must not let the
                # launch fire with 5 bodies.
                if len(self.ready_too & self.team_members) + 1 >= 6:
                    # All 6 fed and ready -> launch: COME_NOW + open the gather.
                    self.broadcast_come_now()
                    self.manifest_phase = "converge"
                    self.start_gather(6)
                    self.state = State.CALL_TEAMMATES
                    return
                # Keep broadcasting READY (gives members our bearing) + eat in
                # place: FARM_FOOD only Takes food already under us (no move) when
                # the tile has it, so we stay put as the rendezvous beacon.
                if now - self.last_ready_bcast > self.gather_rebroadcast_interval():
                    self.broadcast_ready()
                tile0 = (
                    self.memory.visible_tiles[0] if self.memory.visible_tiles else []
                )
                self.state = State.FARM_FOOD if "food" in tile0 else State.LOOK
                return
            # Member: announce READY_TOO, then hold fed until COME_NOW arrives.
            if now - self.last_ready_bcast > self.gather_rebroadcast_interval():
                self.broadcast_ready_too()
            if self.food() < self.manifest_food_floor:
                self.state = State.FARM_FOOD
                return
            self.state = State.LOOK
            return

        # ---- CONVERGE: full tanks, walk to the anchor, then blitz ------------
        # Anchor drives the gather (ARRIVED count -> drop+blitz). Members follow
        # the anchor's gather bearing (full food survives the trek).
        if self.manifest_is_anchor:
            if not self.active_req:
                self.start_gather(6)
            self.state = State.CALL_TEAMMATES
            return
        if self.is_following():
            self.state = State.LOOK
            return
        # member told to come but no bearing yet (anchor not heard) -> wait fed
        if self.food() < 12:
            self.state = State.FARM_FOOD
            return
        self.state = State.LOOK

    def start_manifest_blitz(self) -> None:
        # Anchor has 6 mates on its tile: drop the whole stone pile, then incant
        # straight up. The Set sequence runs via PREPARE_INCANTATION; on each
        # level-up the Current-level handler fires the next Incantation (stones for
        # the next level are already on the tile).
        self.abort_gather("manifest blitz", False)
        self.preparing_incantation = True
        self.blitzing = True
        self.stones_to_drop = self.manifest_drop_list()
