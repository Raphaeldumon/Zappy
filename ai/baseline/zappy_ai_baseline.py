#!/usr/bin/env python3

import argparse
import enum
import random
import select
import socket
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

        while self.running:
            for line in self.read_available():
                if line:
                    self.handle_line(line)

            self.tick()
            time.sleep(0.002)

        if self.sock:
            self.sock.close()

        return 0

    def tick(self) -> None:
        if not self.running or self.state == State.DEAD:
            return

        if self.pending_cmd is not None:
            if time.time() - self.pending_at > self.cmd_timeout(self.pending_cmd):
                print(f"[AI] command timeout: {self.pending_cmd}", file=sys.stderr)
                self.pending_cmd = None
            else:
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

        if self.send_next_plan_cmd():
            return

        if self.should_refresh_inventory():
            self.send_cmd("Inventory")
            return

        self.choose_state()
        self.run_state()

    def handle_line(self, line: str) -> None:
        print(f"[SERVER -> AI] {line}", file=sys.stderr)

        if line == "dead":
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

        if self.preparing_incantation:
            self.state = State.PREPARE_INCANTATION if self.stones_to_drop else State.INCANT
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

            if self.is_following():
                self.state = State.LOOK
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

        if self.players_on_tile() >= need:
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

    def handle_gather(self, direction: int, fields: Dict[str, str], sender: int) -> None:
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

        if self.active_req:
            if sender < self.bot_id:
                self.abort_gather(f"lower bot_id leader wins before lock: {sender} < {self.bot_id}", False)
            else:
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

        self.pending_ack_req = req_id
        self.pending_ack_level = level
        self.pending_ack_leader = leader

        print(f"[AI] queued GATHER_ACK req={req_id} leader={leader}", file=sys.stderr)

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
        if direction == 0:
            return ["Look"]
        if direction == 1:
            return ["Forward"]
        if direction in (2, 3):
            return ["Left", "Forward"]
        if direction in (4, 5):
            return ["Left", "Left", "Forward"]
        if direction == 6:
            return ["Right", "Right", "Forward"]
        if direction in (7, 8):
            return ["Right", "Forward"]
        return []


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("-p", dest="port", type=int)
    parser.add_argument("-n", dest="name")
    parser.add_argument("-h", dest="host", default="localhost")
    parser.add_argument("-f", dest="frequency", type=int, default=None)
    parser.add_argument("--help", action="store_true")

    args, unknown = parser.parse_known_args()

    if args.help or unknown or args.port is None or args.name is None:
        print(f"USAGE: {sys.argv[0]} -p port -n name -h machine [-f freq]", file=sys.stderr)
        raise SystemExit(0 if args.help else 84)

    if args.port <= 0 or args.port > 65535 or (args.frequency is not None and args.frequency <= 0):
        print(f"USAGE: {sys.argv[0]} -p port -n name -h machine [-f freq]", file=sys.stderr)
        raise SystemExit(84)

    return args


def main() -> int:
    try:
        args = parse_args()
        return AI(args.host, args.port, args.name, args.frequency).run()
    except KeyboardInterrupt:
        print("[AI] Interrupted", file=sys.stderr)
        return 130
    except Exception as err:
        print(f"[AI] Error: {err}", file=sys.stderr)
        return 84


if __name__ == "__main__":
    raise SystemExit(main())




# --------------------------------------------------------------------------- #
# Compatibility layer for train_ai / train_rl.py
# --------------------------------------------------------------------------- #
# train_ai importe NetworkClient, Parser et INCANTATION_REQUIREMENTS.
# Cette baseline expose AI + REQ, donc on ajoute juste les wrappers attendus.
# Ça ne change PAS la logique de jeu de l'IA baseline.

INCANTATION_REQUIREMENTS = REQ


class NetworkClient:
    def __init__(self, host: str, port: int, team_name: str):
        self.host = host
        self.port = port
        self.team_name = team_name
        self.sock: Optional[socket.socket] = None
        self.recv_buffer = ""
        self.blocking_buffer: List[str] = []

    def connect(self) -> None:
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))
        self.sock.setblocking(False)

    def close(self) -> None:
        if self.sock is not None:
            try:
                self.sock.close()
            finally:
                self.sock = None

    def send_line(self, line: str) -> None:
        if self.sock is None:
            raise RuntimeError("socket not connected")

        if not line.endswith("\n"):
            line += "\n"

        self.sock.sendall(line.encode())

    def read_available_lines(self) -> List[str]:
        if self.sock is None:
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

    def read_blocking_line(self, timeout: float = 5.0) -> str:
        if self.blocking_buffer:
            return self.blocking_buffer.pop(0)

        start = time.time()

        while time.time() - start < timeout:
            lines = self.read_available_lines()

            if lines:
                self.blocking_buffer.extend(lines)
                return self.blocking_buffer.pop(0)

            time.sleep(0.005)

        raise TimeoutError("timeout waiting server")

    def drain_blocking_buffer(self) -> List[str]:
        lines = self.blocking_buffer[:]
        self.blocking_buffer.clear()
        return lines


class Parser:
    @staticmethod
    def parse_inventory(line: str) -> Dict[str, int]:
        inv = {r: 0 for r in RESOURCES}
        clean = line.strip()

        if clean.startswith("["):
            clean = clean[1:]

        if clean.endswith("]"):
            clean = clean[:-1]

        for part in clean.split(","):
            toks = part.strip().split()

            if len(toks) == 2 and toks[0] in inv:
                try:
                    inv[toks[0]] = int(toks[1])
                except ValueError:
                    inv[toks[0]] = 0

        return inv

    @staticmethod
    def parse_look(line: str) -> List[List[str]]:
        clean = line.strip()

        if clean.startswith("["):
            clean = clean[1:]

        if clean.endswith("]"):
            clean = clean[:-1]

        return [tile.strip().split() for tile in clean.split(",")]

    @staticmethod
    def is_inventory_response(line: str) -> bool:
        if not (line.startswith("[") and line.endswith("]")):
            return False

        clean = line[1:-1].strip()

        if not clean:
            return False

        for part in clean.split(","):
            toks = part.strip().split()

            if len(toks) != 2:
                return False

            if toks[0] not in RESOURCES:
                return False

            if not toks[1].isdigit():
                return False

        return True

    @staticmethod
    def is_look_response(line: str) -> bool:
        return (
            line.startswith("[")
            and line.endswith("]")
            and not Parser.is_inventory_response(line)
        )

    @staticmethod
    def parse_broadcast(line: str) -> Optional[Tuple[int, str]]:
        if not line.startswith("message "):
            return None

        try:
            prefix, text = line.split(",", 1)
            return int(prefix.replace("message", "").strip()), text.strip()
        except ValueError:
            return None

    @staticmethod
    def parse_current_level(line: str) -> Optional[int]:
        if not line.startswith("Current level:"):
            return None

        try:
            return int(line.replace("Current level:", "", 1).strip())
        except ValueError:
            return None