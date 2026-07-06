"""SensingMixin: sensing responsibilities of the AI.

Auto-split from the original monolith; bodies are byte-identical.
The build bundler (_bundle.py) re-stitches these into a single zappy_ai."""

import time
from typing import Optional
from .constants import STONES


class SensingMixin:
    def should_refresh_inventory(self) -> bool:
        if self.preparing_incantation:
            return False

        if self.memory.last_inventory_at == 0 or self.memory.inventory_dirty:
            return True

        if time.time() - self.memory.last_inventory_at > max(
            1.0, 120.0 / self.frequency
        ):
            return True

        if self.food() <= 5:
            return self.memory.actions_since_inventory >= 1

        if self.food() <= 10:
            return self.memory.actions_since_inventory >= 2

        return self.memory.actions_since_inventory >= 4

    def needs_look(self) -> bool:
        if self.memory.force_look or not self.memory.visible_tiles:
            return True

        return self.memory.moves_since_look >= (1 if self.food() <= 5 else 2)

    def count_action(self, cmd: str) -> None:
        if cmd and cmd != "Inventory":
            self.memory.actions_since_inventory += 1

        if cmd in ("Forward", "Left", "Right"):
            self.memory.moves_since_look += 1

    def mark_dirty(self, cmd: str) -> None:
        if (
            cmd.startswith("Take ")
            or cmd.startswith("Set ")
            or cmd in ("Fork", "Eject", "Incantation")
        ):
            self.memory.inventory_dirty = True

    def failed_take(self, item: str) -> None:
        if self.memory.visible_tiles:
            try:
                self.memory.visible_tiles[0].remove(item)
            except ValueError:
                pass

        self.plan.clear()
        self.memory.force_look = True
        self.memory.inventory_dirty = False
        self.memory.moves_since_look = 999

    def food(self) -> int:
        return self.memory.inventory.get("food", 0)

    def find_tile(self, item: str) -> Optional[int]:
        for idx, tile in enumerate(self.memory.visible_tiles):
            if item in tile:
                return idx

        return None

    def players_on_tile(self) -> int:
        if not self.memory.visible_tiles:
            return 1

        return self.memory.visible_tiles[0].count("player")

    def visible_useful_resource(self) -> bool:
        return any(
            item == "food" or (item in STONES and self.need_stone(item))
            for tile in self.memory.visible_tiles
            for item in tile
        )

    def best_current_item(self) -> Optional[str]:
        if not self.memory.visible_tiles:
            return None

        tile = self.memory.visible_tiles[0]

        if "food" in tile and self.food() <= 10:
            return "food"

        for stone in STONES:
            if stone in tile and self.need_stone(stone):
                return stone

        if "food" in tile:
            return "food"

        return None

    def best_resource_tile(self) -> Optional[int]:
        best = None
        score = 0

        for idx, tile in enumerate(self.memory.visible_tiles):
            s = 0

            for item in tile:
                if item == "food":
                    s += 10 if self.food() <= 10 else 2
                elif item in STONES and self.need_stone(item):
                    s += 5

            if s > score:
                score = s
                best = idx

        return best

    def best_food_tile(self) -> Optional[int]:
        best = None
        best_score = -999

        for idx, tile in enumerate(self.memory.visible_tiles):
            count = tile.count("food")
            if count <= 0:
                continue

            dist, off = self.tile_pos(idx)
            score = count * 12 - dist * 2 - abs(off)

            if score > best_score:
                best_score = score
                best = idx

        return best
