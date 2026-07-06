"""MovementMixin: movement responsibilities of the AI.

Auto-split from the original monolith; bodies are byte-identical.
The build bundler (_bundle.py) re-stitches these into a single zappy_ai."""

import time
from typing import List, Optional, Tuple


class MovementMixin:
    def move_to_tile(
        self, idx: int, full: bool, terminal: Optional[str] = None
    ) -> None:
        plan = self.plan_to_tile(idx)

        if not full:
            plan = plan[: self.max_plan_length]

        if not plan:
            return

        # Fold the action at the destination (e.g. "Take food") into the SAME
        # pipelined burst, so we don't stall on a separate Look+Take round-trip
        # after arriving. We only append it when the whole path fits (full plan);
        # a truncated path doesn't reach the tile yet. If the item moved/was taken
        # meanwhile the Take just ko's — cheap.
        if terminal and full:
            plan.append(terminal)

        first = plan.pop(0)
        self.plan = plan

        self.log(f"[AI] moving toward tile={idx} plan={[first] + plan}")
        self.send_cmd(first)

    def send_next_plan_cmd(self) -> bool:
        # A movement plan is a committed, deterministic sequence, so it is safe to
        # pipeline: push as many steps as the pipeline allows in one tick instead
        # of one per round-trip. The server runs them back-to-back with no idle gap.
        sent = False
        while self.plan and len(self.pending) < self.max_pending:
            cmd = self.plan.pop(0)
            if not self.send_cmd(cmd):
                break
            sent = True
            if not self.plan:
                self.memory.force_look = True
        return sent

    def tile_pos(self, idx: int) -> Tuple[int, int]:
        dist = int(idx**0.5)

        while (dist + 1) * (dist + 1) <= idx:
            dist += 1

        while dist * dist > idx:
            dist -= 1

        start = dist * dist
        center = start + dist

        return dist, idx - center

    def plan_to_tile(self, idx: int) -> List[str]:
        if idx <= 0:
            return []

        dist, off = self.tile_pos(idx)

        if off == 0:
            return ["Forward"] * dist

        if off < 0:
            return ["Left"] + ["Forward"] * abs(off) + ["Right"] + ["Forward"] * dist

        return ["Right"] + ["Forward"] * abs(off) + ["Left"] + ["Forward"] * dist

    def set_follow_plan(self, direction: int) -> None:
        if (
            self.waiting_incantation
            or self.preparing_incantation
            or self.food() <= self.survive_min()
        ):
            return

        # Frozen for a ritual: stay put, don't chase.
        if time.time() < self.arrived_hold_until:
            return

        # Bearing 0 = we're on the leader's tile. Latch a freeze so we hold here
        # for the ritual even after the leader stops broadcasting to run it.
        if direction == 0:
            self.arrived_hold_until = max(
                self.arrived_hold_until, time.time() + self.incant_hold_window()
            )
            return

        if self.plan:
            return

        plan = self.broadcast_plan(direction)[: self.max_plan_length]

        if plan:
            self.log(f"[AI] following broadcast direction={direction} plan={plan}")
            self.plan = plan

    def broadcast_plan(self, direction: int) -> List[str]:
        # K is the server's sound bearing in our own frame: 0 = same tile, 1 =
        # straight ahead, then CLOCKWISE to 8 (server broadcast_direction /
        # test_game_rules). So 3 = our right, 5 = behind, 7 = our left. Step so the
        # source moves toward our front; the leader rebroadcasts and we re-aim.
        if direction == 0:
            return ["Look"]
        if direction in (1, 2, 8):  # ahead or ahead-diagonal -> advance
            return ["Forward"]
        if direction in (3, 4):  # on our right -> face right, advance
            return ["Right", "Forward"]
        if direction == 5:  # directly behind -> turn around
            return ["Right", "Right", "Forward"]
        if direction in (6, 7):  # on our left -> face left, advance
            return ["Left", "Forward"]
        return []
