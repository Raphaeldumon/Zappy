"""Observation / action encoding contract.

This MUST stay in lockstep with the C++ side (ai_cpp/ and server vision encoding).
Any change is a protocol-adjacent decision -> coordinate with P5/P6 and record an ADR.
"""

from __future__ import annotations

from enum import IntEnum


# Discrete action space exposed to the policy. Order is stable (it indexes the
# policy output head); do not reorder without an ADR.
class Action(IntEnum):
    FORWARD = 0
    RIGHT = 1
    LEFT = 2
    LOOK = 3
    INVENTORY = 4
    CONNECT_NBR = 5
    FORK = 6
    EJECT = 7
    INCANTATION = 8
    # Take/Set are parameterized by the 7 resources -> 14 actions.
    TAKE_FOOD = 9
    TAKE_LINEMATE = 10
    TAKE_DERAUMERE = 11
    TAKE_SIBUR = 12
    TAKE_MENDIANE = 13
    TAKE_PHIRAS = 14
    TAKE_THYSTAME = 15
    SET_FOOD = 16
    SET_LINEMATE = 17
    SET_DERAUMERE = 18
    SET_SIBUR = 19
    SET_MENDIANE = 20
    SET_PHIRAS = 21
    SET_THYSTAME = 22
    # Coded team broadcasts (bonus).
    BROADCAST_HELP = 23
    BROADCAST_HERE = 24
    BROADCAST_READY = 25
    BROADCAST_GATHER = 26
    BROADCAST_PROBE = 27
    NOOP = 28


NUM_ACTIONS: int = len(Action)

# Placeholder flat observation size. P5 finalizes the real layout in S1 D3
# (inventory[7] + level + vision-cone encoding + team signals + ...).
OBS_DIM: int = 64

RESOURCES: tuple[str, ...] = (
    "food",
    "linemate",
    "deraumere",
    "sibur",
    "mendiane",
    "phiras",
    "thystame",
)
