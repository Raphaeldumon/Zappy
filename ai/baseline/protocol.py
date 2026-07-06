"""ProtocolMixin: protocol responsibilities of the AI.

Auto-split from the original monolith; bodies are byte-identical.
The build bundler (_bundle.py) re-stitches these into a single zappy_ai."""

from typing import Dict, List, Optional, Tuple
from .constants import RESOURCES


class ProtocolMixin:
    def parse_inventory(self, line: str) -> Dict[str, int]:
        inv = {r: 0 for r in RESOURCES}
        clean = line.strip()[1:-1]

        for part in clean.split(","):
            toks = part.strip().split()
            if len(toks) == 2 and toks[0] in inv:
                try:
                    inv[toks[0]] = int(toks[1])
                except ValueError:
                    pass

        return inv

    def parse_look(self, line: str) -> List[List[str]]:
        return [tile.strip().split() for tile in line.strip()[1:-1].split(",")]

    def is_inventory_line(self, line: str) -> bool:
        if not (line.startswith("[") and line.endswith("]")):
            return False

        clean = line[1:-1].strip()
        if not clean:
            return False

        for part in clean.split(","):
            toks = part.strip().split()
            if len(toks) != 2 or toks[0] not in RESOURCES or not toks[1].isdigit():
                return False

        return True

    def parse_broadcast(self, line: str) -> Optional[Tuple[int, str]]:
        if not line.startswith("message "):
            return None

        try:
            prefix, text = line.split(",", 1)
            return int(prefix.replace("message", "").strip()), text.strip()
        except ValueError:
            return None

    def parse_team_payload(self, text: str) -> Tuple[str, Dict[str, str]]:
        parts = text.split(":")
        if len(parts) < 2 or parts[0] != self.team:
            return "", {}

        fields = {}

        for part in parts[2:]:
            if "=" in part:
                k, v = part.split("=", 1)
                fields[k] = v

        return parts[1], fields

    def get_int(self, fields: Dict[str, str], key: str) -> Optional[int]:
        try:
            return int(fields[key])
        except (KeyError, ValueError):
            return None
