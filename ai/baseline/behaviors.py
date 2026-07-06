"""BehaviorMixin: behaviors responsibilities of the AI.

Auto-split from the original monolith; bodies are byte-identical.
The build bundler (_bundle.py) re-stitches these into a single zappy_ai."""

import os
import random
import subprocess
import sys
from .constants import REQ, STONES


class BehaviorMixin:
    def survive(self) -> None:
        if not self.memory.visible_tiles:
            self.send_cmd("Look")
            return

        if self.memory.moves_since_look > 0:
            self.send_cmd("Look")
            return

        tile = self.find_tile("food")

        if tile == 0:
            self.send_cmd("Take food")
        elif tile is not None:
            self.move_to_tile(tile, True, "Take food")
        else:
            self.send_cmd("Forward")

    def collect(self) -> None:
        item = self.best_current_item()

        if item:
            self.send_cmd(f"Take {item}")
            return

        tile = self.best_resource_tile()

        if tile is not None:
            self.move_to_tile(tile, False)
        else:
            self.explore()

    def farm_food(self) -> None:
        if not self.memory.visible_tiles:
            self.send_cmd("Look")
            return

        if "food" in self.memory.visible_tiles[0]:
            self.send_cmd("Take food")
            return

        tile = self.best_food_tile()

        if tile is not None:
            self.move_to_tile(tile, True, "Take food")
        elif self.memory.moves_since_look >= 1:
            self.send_cmd("Look")
        else:
            self.send_cmd("Forward")

    def explore(self) -> None:
        if self.memory.was_ejected:
            self.memory.was_ejected = False
            self.send_cmd("Look")
            return

        if self.is_following():
            self.send_cmd("Look")
            return

        if self.memory.moves_since_look >= 2:
            self.send_cmd("Look")
            return

        r = random.random()

        if r < 0.65:
            self.send_cmd("Forward")
        elif r < 0.825:
            self.send_cmd("Left")
        else:
            self.send_cmd("Right")

    def prepare_incantation(self) -> None:
        if not self.preparing_incantation:
            self.preparing_incantation = True
            self.plan.clear()
            self.clear_following()
            self.stones_to_drop = []

            for stone in STONES:
                self.stones_to_drop += [stone] * REQ[self.memory.level].get(stone, 0)

        if self.stones_to_drop:
            self.send_cmd(f"Set {self.stones_to_drop.pop(0)}")
        else:
            self.send_cmd("Incantation")
            self.waiting_incantation = True

    def reproduce(self) -> None:
        self.clear_following()
        self.abort_gather("reproduce", False)
        self.send_cmd("Fork")

    def after_incantation_done(self) -> None:
        self.preparing_incantation = False
        self.waiting_incantation = False
        self.stones_to_drop.clear()
        self.plan.clear()
        self.clear_following()
        self.memory.inventory_dirty = True
        self.memory.force_look = True

    def spawn_child_for_egg(self) -> None:
        """Launch a fresh AI process to occupy the egg this player just laid.

        Server Fork only creates an egg + frees one team slot; a player hatches
        from it only when a new client connects with the team name. So every
        successful Fork must spawn a new process, else eggs rot and the team
        population can never grow past the initial bots.
        """
        argv = [
            sys.executable,
            os.path.abspath(sys.argv[0]),
            "-p",
            str(self.port),
            "-n",
            self.team,
            "-h",
            self.host,
        ]
        if self.frequency_override is not None:
            argv += ["-f", str(self.frequency_override)]
        try:
            subprocess.Popen(
                argv,
                stdin=subprocess.DEVNULL,
                start_new_session=True,  # detach: our signals/exit don't kill it
                close_fds=True,  # don't leak our socket into the child
            )
            print(
                f"[AI] spawned child to claim egg ({' '.join(argv)})", file=sys.stderr
            )
        except OSError as error:
            print(f"[AI] failed to spawn child for egg: {error}", file=sys.stderr)

    def survive_min(self) -> int:
        # Level-scaled survival floor: bail to eat well before starving, more so
        # the higher (more valuable) the bot is.
        return self.survive_food + (self.memory.level - 1) * self.survive_per_level

    def gather_abort_min(self) -> int:
        # Abort a gather to forage a couple food above the pure-survival floor,
        # so coordinating never tips a high-level bot into starvation.
        return self.survive_min() + 2

    def gather_start_min(self) -> int:
        # Only START a gather with a healthy cushion above the survival floor.
        # (+16 over the flat floor of 8 = 24, the original tuned value.)
        return self.survive_min() + 16

    def has_required_stones(self) -> bool:
        requirements = REQ.get(self.memory.level)
        if not requirements:
            return False

        for stone in STONES:
            if self.memory.inventory.get(stone, 0) < requirements.get(stone, 0):
                return False

        self.log(f"[AI] has stones for lvl {self.memory.level}, ready to incant")
        return True

    def need_stone(self, stone: str) -> bool:
        if not self.manifest_gave_up:
            return self.memory.inventory.get(stone, 0) < self.manifest_target(stone)
        return self.memory.inventory.get(stone, 0) < REQ.get(self.memory.level, {}).get(
            stone, 0
        )
