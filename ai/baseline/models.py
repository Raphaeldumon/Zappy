"""State machine enum + per-bot Memory model."""

import enum
from dataclasses import dataclass, field
from typing import Dict, List

from .constants import RESOURCES


class State(enum.Enum):
    SURVIVE = "SURVIVE"
    LOOK = "LOOK"
    COLLECT = "COLLECT"
    EXPLORE = "EXPLORE"
    FARM_FOOD = "FARM_FOOD"
    CALL_TEAMMATES = "CALL_TEAMMATES"
    PREPARE_INCANTATION = "PREPARE_INCANTATION"
    INCANT = "INCANT"
    REPRODUCE = "REPRODUCE"
    DEAD = "DEAD"


@dataclass
class Memory:
    width: int = 0
    height: int = 0
    free_slots: int = 0
    level: int = 1
    inventory: Dict[str, int] = field(default_factory=lambda: {r: 0 for r in RESOURCES})
    visible_tiles: List[List[str]] = field(default_factory=list)
    inventory_dirty: bool = True
    force_look: bool = False
    was_ejected: bool = False
    actions_since_inventory: int = 0
    moves_since_look: int = 0
    last_inventory_at: float = 0.0
    last_look_at: float = 0.0
    last_fork_at: float = 0.0
    forks_done: int = 0
    last_gather_at: float = 0.0
