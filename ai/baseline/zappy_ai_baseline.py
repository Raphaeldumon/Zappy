#!/usr/bin/env python3

import argparse
import enum
import os
import random
import select
import signal
import socket
import subprocess
import sys
import time
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

RESOURCES = ["food", "linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]
STONES = ["linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]

REQ = {
    1: {"players": 1, "linemate": 1, "deraumere": 0, "sibur": 0, "mendiane": 0, "phiras": 0, "thystame": 0},
    2: {"players": 2, "linemate": 1, "deraumere": 1, "sibur": 1, "mendiane": 0, "phiras": 0, "thystame": 0},
    3: {"players": 2, "linemate": 2, "deraumere": 0, "sibur": 1, "mendiane": 0, "phiras": 2, "thystame": 0},
    4: {"players": 4, "linemate": 1, "deraumere": 1, "sibur": 2, "mendiane": 0, "phiras": 1, "thystame": 0},
    5: {"players": 4, "linemate": 1, "deraumere": 2, "sibur": 1, "mendiane": 3, "phiras": 0, "thystame": 0},
    6: {"players": 6, "linemate": 1, "deraumere": 2, "sibur": 3, "mendiane": 0, "phiras": 1, "thystame": 0},
    7: {"players": 6, "linemate": 2, "deraumere": 2, "sibur": 2, "mendiane": 2, "phiras": 2, "thystame": 1},
}


class State(enum.Enum):
    SURVIVE = "SURVIVE"
    LOOK = "LOOK"
    COLLECT = "COLLECT"
    EXPLORE = "EXPLORE"
    FARM_FOOD = "FARM_FOOD"
    CALL_TEAMMATES = "CALL_TEAMMATES"
    FOLLOW = "FOLLOW"
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


class AI:
    def __init__(self, host: str, port: int, team: str, frequency: Optional[int]):
        self.host = host
        self.port = port
        self.team = team
        self.frequency_override = frequency
        self.frequency = max(1, frequency or 100)
        self.sock: Optional[socket.socket] = None
        self.recv_buffer = ""
        self.blocking_buffer: List[str] = []
        self.pending_cmd: Optional[str] = None
        self.pending_at = 0.0
        self.memory = Memory()
        self.state = State.LOOK
        self.running = True
        self.bot_id = (int(time.time() * 1000000) + random.randint(0, 99999)) % 1000000000
        self.plan: List[str] = []
        self.stones_to_drop: List[str] = []
        self.preparing_incantation = False
        self.waiting_incantation = False

        self.survive_food = 8
        self.gather_start_food = 24
        self.gather_abort_food = 10
        self.fork_food = 25
        self.max_forks = 6
        self.max_plan_length = 2 if self.frequency >= 100 else 3

        self.must_fork_after_gather_fail = False

        self.active_req: Optional[str] = None
        self.active_level = 0
        self.active_need = 0
        self.active_acks: Dict[int, float] = {}
        self.gather_counter = 0
        self.gather_attempts = 0
        self.gather_started_at = 0.0
        self.gather_last_broadcast_at = 0.0
        self.gather_arrival_started_at = 0.0
        self.last_gather_giveup_at = 0.0

        self.answered_reqs: Dict[str, float] = {}
        self.pending_ack_req: Optional[str] = None
        self.pending_ack_level = 0
        self.pending_ack_leader: Optional[int] = None

        self.following_req: Optional[str] = None
        self.following_leader: Optional[int] = None
        self.following_until = 0.0
        self.pending_follow_direction: Optional[int] = None

    def connect(self) -> None:
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))
        self.sock.setblocking(False)

    def send_raw(self, line: str) -> None:
        if not self.sock:
            raise RuntimeError("socket not connected")
        if not line.endswith("\n"):
            line += "\n"
        self.sock.sendall(line.encode())

    def send_cmd(self, cmd: str) -> bool:
        if self.pending_cmd is not None:
            return False
        self.send_raw(cmd)
        self.pending_cmd = cmd
        self.pending_at = time.time()
        print(f"[AI -> SERVER] {cmd}", file=sys.stderr)
        return True

    def read_available(self) -> List[str]:
        if not self.sock:
            return []

        out: List[str] = []

        while True:
            readable, _, _ = select.select([self.sock], [], [], 0)
            if not readable:
                break

            try:
                data = self.sock.recv(4096)
            except BlockingIOError:
                break
            except ConnectionResetError:
                out.append("dead")
                break

            if not data:
                out.append("dead")
                break

            self.recv_buffer += data.decode(errors="ignore")

            while "\n" in self.recv_buffer:
                line, self.recv_buffer = self.recv_buffer.split("\n", 1)
                out.append(line.strip())

        return out

    def read_blocking(self, timeout: float = 5.0) -> str:
        if self.blocking_buffer:
            return self.blocking_buffer.pop(0)

        start = time.time()

        while time.time() - start < timeout:
            lines = self.read_available()
            if lines:
                self.blocking_buffer.extend(lines)
                return self.blocking_buffer.pop(0)
            time.sleep(0.005)

        raise TimeoutError("timeout waiting server")

    def cmd_timeout(self, cmd: str) -> float:
        if cmd in ("Inventory", "Connect_nbr"):
            return max(1.0, 5.0 / self.frequency)
        if cmd == "Fork":
            return max(2.0, 70.0 / self.frequency)
        if cmd == "Incantation":
            return max(5.0, 380.0 / self.frequency)
        return max(1.0, 25.0 / self.frequency)

    def handshake(self) -> None:
        welcome = self.read_blocking()
        if welcome != "WELCOME":
            raise RuntimeError(f"expected WELCOME, got {welcome}")

        self.send_raw(self.team)

        slots = self.read_blocking()
        dims = self.read_blocking()

        try:
            self.memory.free_slots = int(slots)
        except ValueError:
            self.memory.free_slots = 0

        width, height = dims.split()
        self.memory.width = int(width)
        self.memory.height = int(height)

        for line in self.blocking_buffer:
            self.handle_line(line)
        self.blocking_buffer.clear()

        if self.frequency_override is None:
            self.frequency = self.estimate_frequency()
        else:
            self.frequency = self.frequency_override

        self.max_plan_length = 2 if self.frequency >= 100 else 3

        print(
            f"[AI] Connected team={self.team} bot_id={self.bot_id} "
            f"slots={self.memory.free_slots} map={self.memory.width}x{self.memory.height} "
            f"freq={self.frequency}",
            file=sys.stderr,
        )

        self.send_cmd("Inventory")

    def estimate_frequency(self) -> int:
        def round_trip(cmd: str, ok) -> Optional[float]:
            start = time.perf_counter()
            self.send_raw(cmd)

            while time.perf_counter() - start < 60:
                for line in self.read_available():
                    if ok(line):
                        return time.perf_counter() - start
                    self.blocking_buffer.append(line)
                time.sleep(0.0005)

            return None

        connect_times = []
        forward_times = []

        for _ in range(4):
            dt = round_trip("Connect_nbr", lambda l: l.lstrip("-").isdigit())
            if dt is None:
                return 100
            connect_times.append(dt)

        for _ in range(4):
            dt = round_trip("Forward", lambda l: l == "ok")
            if dt is None:
                return 100
            forward_times.append(dt)

        delta = min(forward_times) - min(connect_times)
        if delta <= 0:
            delta = min(forward_times)

        return max(1, round(7 / delta))

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

    def run(self) -> int:
        self.connect()
        self.handshake()

        try:
            return int(line.replace("Current level:", "").strip())
        except ValueError:
            return None


class Brain:
    def __init__(
        self,
        team_name: str,
        memory: WorldMemory,
        queue: CommandQueue,
        frequency: int,
    ):
        self.team_name = team_name
        self.memory = memory
        self.queue = queue
        self.frequency = max(1, frequency)

        self.state = State.INIT
        self.stones_to_drop: List[str] = []

        self.bot_id = random.randint(1000, 9999)

        self.preparing_incantation = False
        self.waiting_incantation_result = False

        self.movement_plan: List[str] = []

        if self.frequency >= 100:
            self.max_plan_length = 2
        else:
            self.max_plan_length = 3

        self.max_forks = 6
        self.fork_food_threshold = 25
        self.fork_max_level = 6

        self.survive_food_threshold = 8

        # On ne démarre pas un gather à 18 food : trop bas.
        self.gather_start_food_threshold = 24

        # Une fois le gather lancé, on ne l'abandonne pas juste parce que
        # la food descend à 23/22.
        self.gather_abort_food_threshold = 10

        self.eject_food_threshold = 20

        # --- GATHER / ACK ---
        self.gather_request_counter = 0
        self.active_gather_req: Optional[str] = None
        self.active_gather_level: int = 0
        self.active_gather_need: int = 0
        self.active_gather_acks: Dict[int, float] = {}
        self.gather_started_at: float = 0.0
        self.gather_last_broadcast_at: float = 0.0
        self.gather_attempts: int = 0
        self.gather_arrival_started_at: float = 0.0
        self.last_gather_giveup_at: float = 0.0

        # Après un gather raté, objectif forcé :
        # FARM_FOOD -> FORK, au lieu de retenter CALL trop tôt.
        self.must_fork_after_gather_fail: bool = False

        self.answered_gather_requests: Dict[str, float] = {}
        self.pending_gather_ack_req: Optional[str] = None
        self.pending_gather_ack_level: int = 0
        self.pending_gather_ack_leader: Optional[int] = None

        # Leader election : plus petit bot_id gagne.
        self.following_gather_req: Optional[str] = None
        self.following_gather_leader: Optional[int] = None
        self.following_gather_until: float = 0.0
        # Last sound bearing (0-8) heard from the leader we're walking to. Updated
        # on every gather broadcast; drives FOLLOW state movement between messages.
        self.following_gather_direction: int = 0
        # Freeze-on-tile handshake for the incantation ritual.
        self.arrived_hold_until: float = 0.0
        self.incant_hold_broadcasts: int = 0

    def tick(self) -> None:
        if self.state == State.DEAD:
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

            self.pending_cmd = None
            self.after_incantation_done()
            self.must_fork_after_gather_fail = False
            self.abort_gather("level changed", False)
            self.clear_following()
            return

        if line == "Elevation underway":
            return

        if line.lstrip("-").isdigit() and self.pending_cmd == "Connect_nbr":
            self.memory.free_slots = int(line)
            self.pending_cmd = None
            return

        if line in ("ok", "ko"):
            cmd = self.pending_cmd or ""
            self.pending_cmd = None
            self.count_action(cmd)

            if line == "ok":
                self.mark_dirty(cmd)

            if cmd == "Fork" and line == "ok":
                self.memory.forks_done += 1
                self.memory.last_fork_at = time.time()
                self.must_fork_after_gather_fail = False
                print(
                    f"[AI] fork ok -> egg laid, waiting for external client "
                    f"(forks_done={self.memory.forks_done})",
                    file=sys.stderr,
                )

            if cmd == "Incantation":
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
                self.pending_cmd = None

                print(
                    "[AI] inventory updated: "
                    + " ".join(f"{r}={self.memory.inventory.get(r, 0)}" for r in RESOURCES),
                    file=sys.stderr,
                )
            else:
                self.memory.visible_tiles = self.parse_look(line)
                self.memory.last_look_at = time.time()
                self.memory.moves_since_look = 0
                self.memory.force_look = False
                self.pending_cmd = None

            return

        print(f"[AI] ignored: {line}", file=sys.stderr)

    def choose_state(self) -> None:
        food = self.food()

        if food <= self.survive_food:
            self.abort_gather("low food", False)
            self.clear_following()
            self.state = State.SURVIVE
            return

        # Frozen for a teammate's incantation ritual (HOLD received): stay put.
        # Survival above still overrides this, so we never freeze into death.
        if time.time() < self.arrived_hold_until and not self.preparing_incantation:
            self.state = State.FOLLOW
            return

        if self.preparing_incantation:
            self.state = State.PREPARE_INCANTATION if self.stones_to_drop else State.INCANT
            return

        # Following a leader takes priority over gathering our own stones: the
        # incantation only needs N bodies on the tile, the leader supplies stones.
        # (food survival above already preempts this.)
        if self.is_following_gather():
            self.state = State.FOLLOW
            return

        if self.needs_look():
            self.state = State.LOOK
            return

        if self.has_required_stones():
            players = REQ[self.memory.level]["players"]

            if players <= 1:
                self.state = State.PREPARE_INCANTATION
                return

            if self.active_req:
                self.state = State.CALL_TEAMMATES
                return

            # Si je suis en train de suivre un leader, je marche vers lui.
            if self.is_following_gather():
                self.state = State.FOLLOW
                return

            if self.must_fork_after_gather_fail:
                self.state = State.REPRODUCE if self.should_fork() else State.FARM_FOOD
                return

            if self.in_gather_cooldown():
                self.state = State.REPRODUCE if self.should_fork() else State.FARM_FOOD
                return

            if food < self.gather_start_food:
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
        print(
            f"[AI] state={self.state.value} level={self.memory.level} food={self.food()} "
            f"actions_since_inventory={self.memory.actions_since_inventory} "
            f"moves_since_look={self.memory.moves_since_look}",
            file=sys.stderr,
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
        elif self.state == State.FOLLOW:
            self.follow_leader()
        elif self.state == State.PREPARE_INCANTATION:
            self.prepare_incantation()
        elif self.state == State.INCANT:
            self.send_cmd("Incantation")
            self.waiting_incantation = True
        elif self.state == State.REPRODUCE:
            self.reproduce()

    def gather_ack_window(self) -> float:
        return max(2.0, 80.0 / self.frequency)

    def gather_rebroadcast_interval(self) -> float:
        return max(0.6, 35.0 / self.frequency)

    def gather_arrival_timeout(self) -> float:
        return max(8.0, 300.0 / self.frequency)

    def incant_hold_window(self) -> float:
        # How long a teammate freezes on the tile after a HOLD, covering the
        # leader's Set+Incantation cycles so the player count doesn't drop.
        return max(3.0, 150.0 / self.frequency)

    def gather_cooldown(self) -> float:
        return max(4.0, 180.0 / self.frequency)

    def in_gather_cooldown(self) -> bool:
        return self.last_gather_giveup_at > 0 and time.time() - self.last_gather_giveup_at < self.gather_cooldown()

    def confirmed_players(self) -> int:
        return 1 + len(self.active_acks) if self.active_req else 1

    def locked_leader(self) -> bool:
        return bool(self.active_req and self.active_need > 0 and self.confirmed_players() >= self.active_need)

    def start_gather(self, need: int) -> None:
        self.gather_counter += 1
        self.gather_attempts += 1
        self.active_req = f"{self.bot_id}-{self.memory.level}-{self.gather_counter}"
        self.active_level = self.memory.level
        self.active_need = need
        self.active_acks.clear()
        self.gather_started_at = time.time()
        self.gather_last_broadcast_at = 0
        self.gather_arrival_started_at = 0
        self.clear_following()

        print(
            f"[AI] new gather req={self.active_req} need={need} attempt={self.gather_attempts}",
            file=sys.stderr,
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

    def broadcast_incant_hold(self) -> bool:
        # "Everyone of my level on my tile: freeze, I'm about to incant."
        msg = (
            f"{self.team_name}:HOLD:"
            f"level={self.memory.level}:"
            f"from={self.bot_id}"
        )
        return self.queue.send(f"Broadcast {msg}")

    def abort_gather(self, reason: str, giveup: bool) -> None:
        if not self.active_req:
            return

        print(f"[AI] abort gather req={self.active_req}: {reason}", file=sys.stderr)

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
                print("[AI] gather failed -> switch objective to FARM_FOOD/FORK", file=sys.stderr)

    def call_teammates(self) -> None:
        now = time.time()
        need = REQ[self.memory.level]["players"]

        if self.food() <= self.gather_abort_food:
            self.abort_gather("food too low while waiting teammates", True)
            self.state = State.SURVIVE
            self.survive()
            return

        if self.memory.visible_tiles and "food" in self.memory.visible_tiles[0]:
            self.send_cmd("Take food")
            return

        if players_on_tile < required_players:
            self.incant_hold_broadcasts = 0

        if self.players_on_tile() >= need:
            # Freeze the teammates here BEFORE the multi-step Set/Incantation
            # ritual, otherwise they wander off mid-ritual and the count drops
            # below the requirement -> server returns ko. Broadcast HOLD a couple
            # times so everyone on the tile latches a freeze timer, then incant.
            if self.incant_hold_broadcasts < 2:
                if self.broadcast_incant_hold():
                    self.incant_hold_broadcasts += 1
                return
            self.incant_hold_broadcasts = 0
            self.abort_gather("enough players on tile", False)
            self.prepare_incantation()
            return

        if not self.active_req:
            self.gather_attempts = 0
            self.start_gather(need)
            return

        confirmed = self.confirmed_players()

        if confirmed >= need:
            if self.gather_arrival_started_at <= 0:
                self.gather_arrival_started_at = now
                print(
                    f"[AI] enough ACK for req={self.active_req} confirmed={confirmed}/{need}; "
                    f"locked leader, waiting on tile",
                    file=sys.stderr,
                )

            if now - self.gather_last_broadcast_at > self.gather_rebroadcast_interval():
                self.broadcast_gather()
                return

            if now - self.gather_arrival_started_at > self.gather_arrival_timeout():
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

    def handle_team_message(self, direction: int, text: str) -> None:
        if not text.startswith(f"{self.team}:"):
            return

        kind, fields = self.parse_team_payload(text)
        sender = self.get_int(fields, "from")

        if sender is None or sender == self.bot_id:
            return

        if kind == "GATHER":
            self.handle_gather(direction, fields, sender)
        elif kind == "GATHER_ACK":
            self.handle_ack(fields, sender)

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

    def handle_team_message(self, direction: int, text: str) -> None:
        if not text.startswith(f"{self.team_name}:"):
            return

        kind, fields = self.parse_team_payload(text)
        if not kind:
            return

        sender_id = self.int_field(fields, "from")
        if sender_id == self.bot_id:
            return

        if sender_id is not None:
            self.memory.known_teammates[sender_id] = time.time()

        self.memory.team_messages.append((direction, text))

        if kind == "GATHER":
            self.handle_gather_message(direction, fields, sender_id)
            return

        if kind == "GATHER_ACK":
            self.handle_gather_ack(fields, sender_id)
            return

        if kind == "HOLD":
            self.handle_hold_message(fields, sender_id)
            return

    def handle_gather_message(
        self,
        direction: int,
        fields: Dict[str, str],
        sender_id: Optional[int],
    ) -> None:
        req_id = fields.get("req", "")
        level = self.get_int(fields, "level")

        if not req_id or level is None:
            return

        self.memory.last_gather_at = time.time()

        if self.locked_leader():
            print(
                f"[AI] locked gather leader req={self.active_req} "
                f"confirmed={self.confirmed_players()}/{self.active_need}; "
                f"ignore gather from={sender}",
                file=sys.stderr,
            )
            return

        if self.is_following() and self.following_leader is not None and sender > self.following_leader:
            return

        self.following_gather_req = req_id
        self.following_gather_leader = sender_id
        self.following_gather_until = time.time() + self.gather_arrival_timeout()
        # Remember the bearing; FOLLOW state walks toward it from the main loop
        # (doing it here is useless — a command is almost always already pending).
        self.following_gather_direction = direction

        self.queue_gather_ack(req_id, level, sender_id)

    def handle_hold_message(
        self,
        fields: Dict[str, str],
        sender_id: Optional[int],
    ) -> None:
        if sender_id == self.bot_id:
            return
        level = self.int_field(fields, "level")
        if level != self.memory.level:
            return
        if self.preparing_incantation or self.waiting_incantation_result:
            return
        if self.memory.inventory.get("food", 0) <= self.survive_food_threshold:
            return  # too hungry to wait around; survival wins
        # A leader of my level is about to incant on a nearby/this tile. Freeze so
        # the player count stays put through its ritual. Cheap if I'm not actually
        # on the tile — at worst I idle a few hundred ms.
        self.arrived_hold_until = time.time() + self.incant_hold_window()
        print(
            f"[AI] HOLD from {sender_id}: freezing {self.incant_hold_window():.1f}s",
            file=sys.stderr,
        )

    def handle_gather_ack(
        self,
        fields: Dict[str, str],
        sender_id: Optional[int],
    ) -> None:
        if sender_id is None:
            return

        if not self.can_answer_gather(level, sender):
            return

        self.following_req = req_id
        self.following_leader = sender
        self.following_until = time.time() + self.gather_arrival_timeout()
        self.queue_ack(req_id, level, sender)
        self.pending_follow_direction = direction

    def handle_ack(self, fields: Dict[str, str], sender: int) -> None:
        req_id = fields.get("req", "")
        level = self.get_int(fields, "level")

        if self.active_req and req_id == self.active_req and level == self.memory.level:
            if sender not in self.active_acks:
                print(
                    f"[AI] GATHER_ACK req={req_id} from={sender} "
                    f"confirmed={1 + len(self.active_acks) + 1}/{self.active_need}",
                    file=sys.stderr,
                )

            self.active_acks[sender] = time.time()

    def can_answer_gather(self, level: int, leader: int) -> bool:
        return (
            leader != self.bot_id
            and level == self.memory.level
            and not self.preparing_incantation
            and not self.waiting_incantation
            and self.food() > self.survive_food
        )

    def queue_ack(self, req_id: str, level: int, leader: int) -> None:
        if req_id in self.answered_reqs or self.pending_ack_req == req_id:
            return

        food_tile = self.find_tile_containing("food")

        if food_tile == 0:
            self.queue.send("Take food")
            return

        if food_tile is not None:
            self.move_towards_tile(food_tile, full_plan=True)
            return

        if self.memory.moves_since_look >= 1:
            self.queue.send("Look")
            return

        self.queue.send("Forward")

    def look(self) -> None:
        self.queue.send("Look")

    def follow_leader(self) -> None:
        # Walk toward the leader using the last bearing it broadcast. Driven from
        # the main loop (queue empty, no movement plan), so unlike the old reactive
        # path it actually fires. The leader rebroadcasts, refreshing the bearing,
        # so we re-aim each message and converge over several steps.
        if self.memory.inventory.get("food", 0) <= self.survive_food_threshold:
            self.following_gather_req = None
            self.following_gather_leader = None
            self.state = State.SURVIVE
            self.survive()
            return

        # Frozen for a ritual (HOLD): hold position, do not move.
        if time.time() < self.arrived_hold_until:
            self.queue.send("Look")
            return

        direction = self.following_gather_direction

        # Bearing 0 = we're on the leader's tile. Latch a freeze and Look so the
        # leader keeps counting us present; it drives the incantation from here.
        if direction == 0:
            self.arrived_hold_until = max(
                self.arrived_hold_until, time.time() + self.incant_hold_window()
            )
            self.queue.send("Look")
            return

        plan = self.broadcast_direction_to_plan(direction)[: self.max_plan_length]
        if not plan:
            self.queue.send("Look")
            return

        first = plan.pop(0)
        self.movement_plan = plan
        self.queue.send(first)
        if not self.movement_plan:
            self.memory.force_look = True

    def collect(self) -> None:
        resource = self.best_resource_on_current_tile()

        if resource:
            self.queue.send(f"Take {resource}")
            return

        target = self.find_best_resource_tile()

        if target is not None:
            self.move_towards_tile(target)
            return

        self.state = State.EXPLORE
        self.explore()

    def prepare_incantation(self) -> None:
        if not self.preparing_incantation:
            self.preparing_incantation = True
            self.stones_to_drop = self.required_stones_list()
            self.movement_plan.clear()
            self.abort_gather("start incantation preparation")

        if self.stones_to_drop:
            stone = self.stones_to_drop.pop(0)
            self.queue.send(f"Set {stone}")
            return

        self.state = State.INCANT
        self.incant()

    def send_pending_ack(self) -> bool:
        if not self.pending_ack_req:
            return False

        if self.food() <= self.survive_food or self.preparing_incantation or self.waiting_incantation:
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
        self.answered_reqs = {req: t for req, t in self.answered_reqs.items() if now - t <= ttl}

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
            self.move_to_tile(tile, True)
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
            self.move_to_tile(tile, True)
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

    def food(self) -> int:
        return self.memory.inventory.get("food", 0)

    def should_refresh_inventory(self) -> bool:
        if self.preparing_incantation:
            return False

        if self.memory.last_inventory_at == 0 or self.memory.inventory_dirty:
            return True

        if time.time() - self.memory.last_inventory_at > max(1.0, 120.0 / self.frequency):
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
        if cmd.startswith("Take ") or cmd.startswith("Set ") or cmd in ("Fork", "Eject", "Incantation"):
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

    def has_required_stones(self) -> bool:
        requirements = REQ.get(self.memory.level)
        if not requirements:
            return False

        for stone in STONES:
            if self.memory.inventory.get(stone, 0) < requirements.get(stone, 0):
                return False

        print(f"[AI] has stones for lvl {self.memory.level}, ready to incant", file=sys.stderr)
        return True

    def need_stone(self, stone: str) -> bool:
        return self.memory.inventory.get(stone, 0) < REQ.get(self.memory.level, {}).get(stone, 0)

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

    def find_tile(self, item: str) -> Optional[int]:
        for idx, tile in enumerate(self.memory.visible_tiles):
            if item in tile:
                return idx

        return None

    def players_on_tile(self) -> int:
        if not self.memory.visible_tiles:
            return 1

        return self.memory.visible_tiles[0].count("player")

    def should_fork(self) -> bool:
        if self.active_req or self.is_following() or self.preparing_incantation or self.waiting_incantation:
            return False

        if self.memory.forks_done >= self.max_forks:
            return False

        if self.food() < self.fork_food:
            return False

        if self.memory.level > 3 and not self.need_more_mates():
            return False

        return time.time() - self.memory.last_fork_at >= max(3.0, 150.0 / self.frequency)

    def need_more_mates(self) -> bool:
        return REQ.get(self.memory.level, {}).get("players", 1) > 1 and self.has_required_stones()

    def move_to_tile(self, idx: int, full: bool) -> None:
        plan = self.plan_to_tile(idx)

        if not full:
            plan = plan[: self.max_plan_length]

        if not plan:
            return

        first = plan.pop(0)
        self.plan = plan

        print(f"[AI] moving toward tile={idx} plan={[first] + plan}", file=sys.stderr)
        self.send_cmd(first)

    def send_next_plan_cmd(self) -> bool:
        if not self.plan:
            return False

        cmd = self.plan.pop(0)
        self.send_cmd(cmd)

        if not self.plan:
            self.memory.force_look = True

        return True

    def tile_pos(self, idx: int) -> Tuple[int, int]:
        dist = int(idx ** 0.5)

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
        if self.waiting_incantation or self.preparing_incantation or self.food() <= self.survive_food:
            return

        if self.plan:
            return

        plan = self.broadcast_plan(direction)[: self.max_plan_length]

        if plan:
            print(f"[AI] following broadcast direction={direction} plan={plan}", file=sys.stderr)
            self.plan = plan

    def broadcast_plan(self, direction: int) -> List[str]:
        # K is the server's sound bearing in THIS bot's own frame: 0 = same tile,
        # 1 = straight ahead, then CLOCKWISE to 8 (see server broadcast_direction /
        # test_game_rules). So 3 = our right, 5 = behind, 7 = our left. Step so the
        # source moves toward our front; the leader rebroadcasts and we re-aim.
        if direction == 0:
            return ["Look"]
        if direction in (1, 2, 8):       # ahead or ahead-diagonal -> just advance
            return ["Forward"]

        if direction in (3, 4):           # on our right -> face right, advance
            return ["Right", "Forward"]

        if direction == 5:                # directly behind -> turn around
            return ["Right", "Right", "Forward"]
        if direction in (6, 7):           # on our left -> face left, advance
            return ["Left", "Forward"]
        return []


class FrequencyEstimator:
    FORWARD_COST = 7

    def __init__(self, network: NetworkClient):
        self.network = network
        self.deferred_lines: List[str] = []
        self.last_connect_value: Optional[int] = None

    def _round_trip(self, command: str, is_response, guard: float = 60.0):
        start = time.perf_counter()
        self.network.send_line(command)

        while time.perf_counter() - start < guard:
            for line in self.network.read_available_lines():
                if line == "dead":
                    self.deferred_lines.append(line)
                    return None
                if is_response(line):
                    return time.perf_counter() - start
                self.deferred_lines.append(line)
            time.sleep(0.0005)

        return None

    def estimate(self, samples: int = 6, default: int = 100) -> int:
        def is_int(line: str) -> bool:
            if line.lstrip("-").isdigit():
                self.last_connect_value = int(line)
                return True
            return False

        def is_ok(line: str) -> bool:
            return line == "ok"

        connect_times: List[float] = []
        for _ in range(samples):
            dt = self._round_trip("Connect_nbr", is_int)
            if dt is None:
                return default
            connect_times.append(dt)

        forward_times: List[float] = []
        for _ in range(samples):
            dt = self._round_trip("Forward", is_ok)
            if dt is None:
                return default
            forward_times.append(dt)

        delta = min(forward_times) - min(connect_times)
        if delta <= 0:
            delta = min(forward_times)

        freq = round(self.FORWARD_COST / delta)
        return max(1, freq)


class ZappyAI:
    def __init__(
        self,
        host: str,
        port: int,
        team_name: str,
        frequency_override: Optional[int] = None,
    ):
        self.frequency_override = frequency_override
        self.frequency = max(1, frequency_override or 100)
        self.network = NetworkClient(host, port, team_name)
        self.memory = WorldMemory()
        self.queue = CommandQueue(self.network, self.frequency)
        self.brain = Brain(team_name, self.memory, self.queue, self.frequency)
        self.running = True

    def run(self) -> None:
        # Auto-reap forked children so spawned bots don't pile up as zombies.
        if hasattr(signal, "SIGCHLD"):
            signal.signal(signal.SIGCHLD, signal.SIG_IGN)

        self.network.connect()
        self.handshake()

        while self.running:
            self.read_server_messages()
            self.brain.tick()
            time.sleep(0.002)

        self.network.close()

    def spawn_child_for_egg(self) -> None:
        """Launch a fresh AI process to occupy the egg this player just laid.

        The server's Fork only creates an egg and frees one team connection
        slot; a player hatches from it only when a new client connects with the
        team name. So every successful Fork must be matched by a new process,
        otherwise eggs rot and the team population can never grow.
        """
        argv = [sys.executable, os.path.abspath(sys.argv[0]),
                "-p", str(self.network.port),
                "-n", self.network.team_name,
                "-h", self.network.host]
        if self.frequency_override is not None:
            argv += ["-f", str(self.frequency_override)]
        try:
            subprocess.Popen(
                argv,
                stdin=subprocess.DEVNULL,
                start_new_session=True,  # detach: our signals/exit don't kill it
                close_fds=True,          # don't leak our socket into the child
            )
            print(f"[AI] spawned child to claim egg ({' '.join(argv)})",
                  file=sys.stderr)
        except OSError as error:
            print(f"[AI] failed to spawn child for egg: {error}", file=sys.stderr)

    def apply_frequency(self, freq: int) -> None:
        freq = max(1, int(freq))
        self.frequency = freq
        self.queue.frequency = freq
        self.brain.frequency = freq
        self.brain.max_plan_length = 2 if freq >= 100 else 3

    def handshake(self) -> None:
        welcome = self.network.read_blocking_line()
        if welcome != "WELCOME":
            raise RuntimeError(f"Invalid handshake, expected WELCOME, got: {welcome}")

        self.network.send_line(self.network.team_name)

        client_num = self.network.read_blocking_line()
        dimensions = self.network.read_blocking_line()

        try:
            self.memory.client_num = int(client_num)
        except ValueError:
            self.memory.client_num = 0

        self.memory.free_slots = self.memory.client_num
        self.memory.last_connect_at = time.time()

        try:
            width, height = dimensions.split()
            self.memory.width = int(width)
            self.memory.height = int(height)
        except ValueError:
            raise RuntimeError(f"Invalid map dimensions: {dimensions}")

        for line in self.network.drain_blocking_buffer():
            self.handle_line(line)

        if self.frequency_override is not None:
            measured = self.frequency_override
            print(
                f"[AI] frequency override (-f) f={measured} (no measurement)",
                file=sys.stderr,
            )
        elif self.running:
            estimator = FrequencyEstimator(self.network)
            measured = estimator.estimate(samples=6, default=100)
            print(f"[AI] measured server frequency f={measured}", file=sys.stderr)

            if estimator.last_connect_value is not None:
                self.memory.client_num = estimator.last_connect_value
                self.memory.free_slots = estimator.last_connect_value
                self.memory.last_connect_at = time.time()

            for line in estimator.deferred_lines:
                self.handle_line(line)
        else:
            measured = self.frequency

        self.apply_frequency(measured)

        print(
            f"[AI] Connected as team={self.network.team_name}, "
            f"bot_id={self.brain.bot_id}, "
            f"slots={self.memory.client_num}, "
            f"map={self.memory.width}x{self.memory.height}, "
            f"freq={self.frequency}",
            file=sys.stderr,
        )

        if self.running:
            self.queue.send("Inventory")

    def read_server_messages(self) -> None:
        lines = self.network.read_available_lines()

        for line in lines:
            if not line:
                continue

            self.handle_line(line)

    def update_action_counters(self, command: str) -> None:
        if not command:
            return

        if command != "Inventory":
            self.memory.actions_since_inventory += 1

        if command in ("Forward", "Left", "Right"):
            self.memory.moves_since_look += 1

    def mark_inventory_dirty_after_command(self, command: str) -> None:
        if command in ("Inventory", "Look"):
            return

        if command in ("Forward", "Left", "Right"):
            return

        if command.startswith("Take "):
            self.memory.inventory_dirty = True
            return

        if command.startswith("Set "):
            if not self.brain.preparing_incantation:
                self.memory.inventory_dirty = True
            return

        if command == "Incantation":
            self.memory.inventory_dirty = True
            return

        if command.startswith("Broadcast"):
            return

        if command in ("Fork", "Eject"):
            self.memory.inventory_dirty = True
            return

    def force_look_after_failed_take(self, command: str) -> None:
        failed_resource = command.replace("Take ", "", 1)

        if self.memory.visible_tiles:
            try:
                self.memory.visible_tiles[0].remove(failed_resource)
            except ValueError:
                pass

        self.brain.movement_plan.clear()
        self.memory.force_look = True
        self.memory.inventory_dirty = False
        self.memory.moves_since_look = 999

    def handle_line(self, line: str) -> None:
        print(f"[SERVER -> AI] {line}", file=sys.stderr)

        if line == "dead":
            self.memory.level = 0
            self.brain.state = State.DEAD
            self.brain.waiting_incantation_result = False
            self.running = False
            return

        broadcast = Parser.parse_broadcast(line)
        if broadcast:
            direction, text = broadcast
            self.brain.handle_team_message(direction, text)
            return

        if line.startswith("eject:"):
            self.memory.was_ejected = True
            self.brain.movement_plan.clear()
            self.memory.force_look = True
            return

        new_level = Parser.parse_current_level(line)
        if new_level is not None:
            self.memory.level = new_level
            self.memory.inventory_dirty = True
            self.memory.force_look = True
            self.brain.stones_to_drop.clear()
            self.brain.preparing_incantation = False
            self.brain.waiting_incantation_result = False
            self.brain.movement_plan.clear()
            self.brain.must_fork_after_gather_fail = False
            self.brain.following_gather_req = None
            self.brain.following_gather_leader = None
            self.brain.following_gather_until = 0.0
            self.brain.abort_gather("level changed")
            self.queue.pop_expected_response()
            return

        if line == "Elevation underway":
            return

        if line.lstrip("-").isdigit() and self.queue.current_pending() == "Connect_nbr":
            try:
                self.memory.free_slots = int(line)
            except ValueError:
                self.memory.free_slots = 0

            self.memory.last_connect_at = time.time()
            self.queue.pop_expected_response()

            print(
                f"[AI] connect_nbr -> free_slots={self.memory.free_slots}",
                file=sys.stderr,
            )
            return

        if line in ("ok", "ko"):
            pending = self.queue.pop_expected_response()
            command = pending.command if pending else ""

            self.update_action_counters(command)

            if line == "ok":
                self.mark_inventory_dirty_after_command(command)

            if command == "Incantation":
                self.brain.preparing_incantation = False
                self.brain.waiting_incantation_result = False
                self.brain.stones_to_drop.clear()
                self.brain.movement_plan.clear()
                self.memory.inventory_dirty = True
                self.memory.force_look = True
                self.brain.abort_gather("incantation response")

            if line == "ok" and command == "Fork":
                self.memory.forks_done += 1
                self.memory.last_fork_at = time.time()
                self.brain.must_fork_after_gather_fail = False
                # Egg laid: launch the client that will hatch from it.
                self.spawn_child_for_egg()
                print(
                    f"[AI] fork ok -> egg laid, spawned client "
                    f"(forks_done={self.memory.forks_done})",
                    file=sys.stderr,
                )

            if line == "ko":
                if command.startswith("Take "):
                    self.force_look_after_failed_take(command)
                    return

                if command.startswith("Set "):
                    self.memory.inventory_dirty = True
                    self.memory.force_look = True
                    return

                if command == "Incantation":
                    self.memory.inventory_dirty = True
                    self.memory.force_look = True
                    return

                self.memory.inventory_dirty = True

            return

        if Parser.is_inventory_response(line):
            self.memory.inventory = Parser.parse_inventory(line)
            self.memory.last_inventory_at = time.time()
            self.memory.inventory_dirty = False
            self.memory.actions_since_inventory = 0
            self.queue.pop_expected_response()

            print(
                f"[AI] inventory updated: "
                f"food={self.memory.inventory.get('food', 0)} "
                f"linemate={self.memory.inventory.get('linemate', 0)} "
                f"deraumere={self.memory.inventory.get('deraumere', 0)} "
                f"sibur={self.memory.inventory.get('sibur', 0)} "
                f"mendiane={self.memory.inventory.get('mendiane', 0)} "
                f"phiras={self.memory.inventory.get('phiras', 0)} "
                f"thystame={self.memory.inventory.get('thystame', 0)}",
                file=sys.stderr,
            )
            return

        if Parser.is_look_response(line):
            self.memory.visible_tiles = Parser.parse_look(line)
            self.memory.last_look_at = time.time()
            self.memory.moves_since_look = 0
            self.memory.force_look = False
            self.queue.pop_expected_response()
            return

        print(f"[AI] Ignored unknown line: {line}", file=sys.stderr)


def print_usage(program_name: str) -> None:
    print(
        f"USAGE: {program_name} -p port -n name -h machine [-f freq]\n"
        "\n"
        "option description\n"
        "-p port      port number\n"
        "-n name      name of the team\n"
        "-h machine   name of the machine; localhost by default\n"
        "-f freq      optional: force server frequency\n",
        file=sys.stderr,
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(add_help=False)

    parser.add_argument("-p", dest="port", type=int)
    parser.add_argument("-n", dest="name")
    parser.add_argument("-h", dest="host", default="localhost")
    parser.add_argument("-f", dest="frequency", type=int, default=None)
    parser.add_argument("--help", action="store_true", dest="help")

    try:
        args, unknown = parser.parse_known_args()
    except SystemExit:
        print_usage(sys.argv[0])
        raise SystemExit(84)

    if args.help:
        print_usage(sys.argv[0])
        raise SystemExit(0)

    if unknown:
        print_usage(sys.argv[0])
        raise SystemExit(84)

    if args.port is None or args.name is None:
        print_usage(sys.argv[0])
        raise SystemExit(84)

    if args.port <= 0 or args.port > 65535:
        print_usage(sys.argv[0])
        raise SystemExit(84)

    if args.frequency is not None and args.frequency <= 0:
        print_usage(sys.argv[0])
        raise SystemExit(84)

    return args


def main() -> int:
    try:
        args = parse_args()
    except SystemExit as exit_code:
        return int(exit_code.code)

    ai = ZappyAI(
        host=args.host,
        port=args.port,
        team_name=args.name,
        frequency_override=args.frequency,
    )

    try:
        ai.run()
    except KeyboardInterrupt:
        print("[AI] Interrupted", file=sys.stderr)
        return 130
    except Exception as error:
        print(f"[AI] Error: {error}", file=sys.stderr)
        return 84

    return 0


if __name__ == "__main__":
    raise SystemExit(main())