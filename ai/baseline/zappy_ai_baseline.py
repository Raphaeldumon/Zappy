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


RESOURCES = [
    "food",
    "linemate",
    "deraumere",
    "sibur",
    "mendiane",
    "phiras",
    "thystame",
]

STONES = [
    "linemate",
    "deraumere",
    "sibur",
    "mendiane",
    "phiras",
    "thystame",
]

INCANTATION_REQUIREMENTS = {
    1: {
        "players": 1,
        "linemate": 1,
        "deraumere": 0,
        "sibur": 0,
        "mendiane": 0,
        "phiras": 0,
        "thystame": 0,
    },
    2: {
        "players": 2,
        "linemate": 1,
        "deraumere": 1,
        "sibur": 1,
        "mendiane": 0,
        "phiras": 0,
        "thystame": 0,
    },
    3: {
        "players": 2,
        "linemate": 2,
        "deraumere": 0,
        "sibur": 1,
        "mendiane": 0,
        "phiras": 2,
        "thystame": 0,
    },
    4: {
        "players": 4,
        "linemate": 1,
        "deraumere": 1,
        "sibur": 2,
        "mendiane": 0,
        "phiras": 1,
        "thystame": 0,
    },
    5: {
        "players": 4,
        "linemate": 1,
        "deraumere": 2,
        "sibur": 1,
        "mendiane": 3,
        "phiras": 0,
        "thystame": 0,
    },
    6: {
        "players": 6,
        "linemate": 1,
        "deraumere": 2,
        "sibur": 3,
        "mendiane": 0,
        "phiras": 1,
        "thystame": 0,
    },
    7: {
        "players": 6,
        "linemate": 2,
        "deraumere": 2,
        "sibur": 2,
        "mendiane": 2,
        "phiras": 2,
        "thystame": 1,
    },
}


class State(enum.Enum):
    INIT = "INIT"
    SURVIVE = "SURVIVE"
    LOOK = "LOOK"
    COLLECT = "COLLECT"
    EXPLORE = "EXPLORE"
    PREPARE_INCANTATION = "PREPARE_INCANTATION"
    CALL_TEAMMATES = "CALL_TEAMMATES"
    INCANT = "INCANT"
    REPRODUCE = "REPRODUCE"
    EJECT = "EJECT"
    FARM_FOOD = "FARM_FOOD"
    DEAD = "DEAD"


@dataclass
class PendingCommand:
    command: str
    created_at: float


@dataclass
class WorldMemory:
    width: int = 0
    height: int = 0
    client_num: int = 0
    level: int = 1

    inventory: Dict[str, int] = field(
        default_factory=lambda: {resource: 0 for resource in RESOURCES}
    )

    visible_tiles: List[List[str]] = field(default_factory=list)

    last_look_at: float = 0.0
    last_inventory_at: float = 0.0
    last_broadcast_at: float = 0.0

    team_messages: List[Tuple[int, str]] = field(default_factory=list)
    known_teammates: Dict[int, float] = field(default_factory=dict)

    was_ejected: bool = False
    inventory_dirty: bool = True

    actions_since_inventory: int = 0
    moves_since_look: int = 0

    force_look: bool = False

    free_slots: int = 0
    last_connect_at: float = 0.0

    forks_done: int = 0
    last_fork_at: float = 0.0

    last_eject_at: float = 0.0
    last_gather_at: float = 0.0


class NetworkClient:
    def __init__(self, host: str, port: int, team_name: str):
        self.host = host
        self.port = port
        self.team_name = team_name
        self.sock: Optional[socket.socket] = None
        self.recv_buffer = ""
        self._blocking_buffer: List[str] = []

    def connect(self) -> None:
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))
        self.sock.setblocking(False)

    def close(self) -> None:
        if self.sock:
            self.sock.close()
            self.sock = None

    def send_line(self, line: str) -> None:
        if not self.sock:
            raise RuntimeError("Socket not connected")

        if not line.endswith("\n"):
            line += "\n"

        self.sock.sendall(line.encode("utf-8"))

    def read_available_lines(self) -> List[str]:
        if not self.sock:
            return []

        lines: List[str] = []

        while True:
            readable, _, _ = select.select([self.sock], [], [], 0)
            if not readable:
                break

            try:
                data = self.sock.recv(4096)
            except BlockingIOError:
                break
            except ConnectionResetError:
                lines.append("dead")
                break

            if not data:
                lines.append("dead")
                break

            self.recv_buffer += data.decode("utf-8", errors="ignore")

            while "\n" in self.recv_buffer:
                line, self.recv_buffer = self.recv_buffer.split("\n", 1)
                lines.append(line.strip())

        return lines

    def read_blocking_line(self, timeout: float = 5.0) -> str:
        if self._blocking_buffer:
            return self._blocking_buffer.pop(0)

        start = time.time()

        while time.time() - start < timeout:
            lines = self.read_available_lines()
            if lines:
                self._blocking_buffer.extend(lines)
                return self._blocking_buffer.pop(0)
            time.sleep(0.005)

        raise TimeoutError("Timeout while waiting for server line")

    def drain_blocking_buffer(self) -> List[str]:
        leftover = self._blocking_buffer
        self._blocking_buffer = []
        return leftover


class CommandQueue:
    def __init__(self, network: NetworkClient, frequency: int):
        self.network = network
        self.frequency = max(1, frequency)
        self.pending: List[PendingCommand] = []
        self.max_pending = 1

    def can_send(self) -> bool:
        return len(self.pending) < self.max_pending

    def has_pending(self) -> bool:
        return len(self.pending) > 0

    def current_pending(self) -> Optional[str]:
        if not self.pending:
            return None
        return self.pending[0].command

    def send(self, command: str) -> bool:
        if not self.can_send():
            return False

        self.network.send_line(command)
        self.pending.append(PendingCommand(command=command, created_at=time.time()))

        print(f"[AI -> SERVER] {command}", file=sys.stderr)
        return True

    def pop_expected_response(self) -> Optional[PendingCommand]:
        if not self.pending:
            return None
        return self.pending.pop(0)

    def expected_command_timeout(self, command: str) -> float:
        if command == "Inventory":
            return max(1.0, 5.0 / self.frequency)

        if command == "Connect_nbr":
            return max(1.0, 5.0 / self.frequency)

        if command == "Fork":
            return max(2.0, 70.0 / self.frequency)

        if command == "Incantation":
            return max(5.0, 380.0 / self.frequency)

        return max(1.0, 25.0 / self.frequency)

    def clear_old_commands(self) -> None:
        if not self.pending:
            return

        pending = self.pending[0]
        elapsed = time.time() - pending.created_at
        timeout = self.expected_command_timeout(pending.command)

        if elapsed <= timeout:
            return

        print(
            f"[AI] command timeout: {pending.command} "
            f"elapsed={elapsed:.2f}s timeout={timeout:.2f}s",
            file=sys.stderr,
        )

        self.pending.clear()


class Parser:
    @staticmethod
    def parse_inventory(line: str) -> Dict[str, int]:
        result = {resource: 0 for resource in RESOURCES}

        clean = line.strip()
        if clean.startswith("["):
            clean = clean[1:]
        if clean.endswith("]"):
            clean = clean[:-1]

        parts = clean.split(",")

        for part in parts:
            tokens = part.strip().split()
            if len(tokens) != 2:
                continue

            name, value = tokens

            if name in result:
                try:
                    result[name] = int(value)
                except ValueError:
                    result[name] = 0

        return result

    @staticmethod
    def parse_look(line: str) -> List[List[str]]:
        clean = line.strip()
        if clean.startswith("["):
            clean = clean[1:]
        if clean.endswith("]"):
            clean = clean[:-1]

        tiles = clean.split(",")
        parsed_tiles: List[List[str]] = []

        for tile in tiles:
            objects = tile.strip().split()
            parsed_tiles.append(objects)

        return parsed_tiles

    @staticmethod
    def is_inventory_response(line: str) -> bool:
        if not line.startswith("[") or not line.endswith("]"):
            return False

        clean = line[1:-1].strip()
        if not clean:
            return False

        parts = clean.split(",")
        found_resource = False

        for part in parts:
            tokens = part.strip().split()

            if len(tokens) != 2:
                return False

            name, value = tokens

            if name not in RESOURCES:
                return False

            if not value.isdigit():
                return False

            found_resource = True

        return found_resource

    @staticmethod
    def is_look_response(line: str) -> bool:
        if not line.startswith("[") or not line.endswith("]"):
            return False

        return not Parser.is_inventory_response(line)

    @staticmethod
    def parse_broadcast(line: str) -> Optional[Tuple[int, str]]:
        if not line.startswith("message "):
            return None

        try:
            prefix, text = line.split(",", 1)
            direction = int(prefix.replace("message", "").strip())
            return direction, text.strip()
        except ValueError:
            return None

    @staticmethod
    def parse_current_level(line: str) -> Optional[int]:
        if not line.startswith("Current level:"):
            return None

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
        self.fork_max_level = 3

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

    def tick(self) -> None:
        if self.state == State.DEAD:
            return

        if self.waiting_incantation_result and not self.queue.has_pending():
            self.waiting_incantation_result = False

        if self.waiting_incantation_result:
            return

        self.queue.clear_old_commands()

        if self.queue.has_pending():
            return

        self.prune_answered_gathers()

        if self.send_pending_gather_ack():
            return

        if self.memory.force_look:
            self.memory.force_look = False
            self.queue.send("Look")
            return

        if self.movement_plan:
            command = self.movement_plan.pop(0)
            self.queue.send(command)

            if not self.movement_plan:
                self.memory.force_look = True

            return

        if self.should_refresh_inventory():
            self.queue.send("Inventory")
            return

        self.choose_state()
        self.run_state()

    def should_refresh_inventory(self) -> bool:
        if self.preparing_incantation:
            return False

        if self.memory.last_inventory_at == 0:
            return True

        if self.memory.inventory_dirty:
            return True

        food = self.memory.inventory.get("food", 0)

        if food <= 5:
            return self.memory.actions_since_inventory >= 1

        if food <= 10:
            return self.memory.actions_since_inventory >= 2

        return self.memory.actions_since_inventory >= 4

    def should_force_look(self) -> bool:
        if self.memory.force_look:
            return True

        if not self.memory.visible_tiles:
            return True

        food = self.memory.inventory.get("food", 0)

        if food <= 5:
            return self.memory.moves_since_look >= 1

        return self.memory.moves_since_look >= 2

    def choose_state(self) -> None:
        food = self.memory.inventory.get("food", 0)

        if food <= self.survive_food_threshold and not self.preparing_incantation:
            self.abort_gather("low food / survival")
            self.state = State.SURVIVE
            return

        if self.preparing_incantation:
            if self.stones_to_drop:
                self.state = State.PREPARE_INCANTATION
            else:
                self.state = State.INCANT
            return

        if self.should_force_look():
            self.state = State.LOOK
            return

        stones_ready = self.has_required_stones_for_level()

        if stones_ready:
            required_players = INCANTATION_REQUIREMENTS[self.memory.level]["players"]

            if required_players <= 1:
                self.state = State.PREPARE_INCANTATION
                return

            # Si j'ai déjà lancé un gather, je continue même si la food descend
            # sous gather_start_food_threshold.
            if self.active_gather_req:
                self.state = State.CALL_TEAMMATES
                return

            # Si je suis en train de suivre un leader, je ne redeviens pas leader.
            if self.is_following_gather():
                self.state = State.LOOK
                return

            # Après gather raté : objectif = farm jusqu'au Fork.
            if self.must_fork_after_gather_fail:
                if self.should_reproduce():
                    self.state = State.REPRODUCE
                else:
                    self.state = State.FARM_FOOD
                return

            if self.in_gather_retry_cooldown():
                if self.should_reproduce():
                    self.state = State.REPRODUCE
                else:
                    self.state = State.FARM_FOOD
                return

            # On commence le gather uniquement avec assez de marge.
            if food < self.gather_start_food_threshold:
                if self.should_reproduce():
                    self.state = State.REPRODUCE
                else:
                    self.state = State.FARM_FOOD
                return

            self.state = State.CALL_TEAMMATES
            return

        self.abort_gather("stones not ready")

        if self.should_reproduce():
            self.state = State.REPRODUCE
            return

        if self.visible_useful_resource_exists():
            self.state = State.COLLECT
            return

        if self.should_eject():
            self.state = State.EJECT
            return

        self.state = State.EXPLORE

    def run_state(self) -> None:
        print(
            f"[AI] state={self.state.value} "
            f"level={self.memory.level} "
            f"food={self.memory.inventory.get('food', 0)} "
            f"actions_since_inventory={self.memory.actions_since_inventory} "
            f"moves_since_look={self.memory.moves_since_look}",
            file=sys.stderr,
        )

        if self.state == State.SURVIVE:
            self.survive()
        elif self.state == State.LOOK:
            self.look()
        elif self.state == State.COLLECT:
            self.collect()
        elif self.state == State.PREPARE_INCANTATION:
            self.prepare_incantation()
        elif self.state == State.CALL_TEAMMATES:
            self.call_teammates()
        elif self.state == State.INCANT:
            self.incant()
        elif self.state == State.REPRODUCE:
            self.reproduce()
        elif self.state == State.EJECT:
            self.do_eject()
        elif self.state == State.FARM_FOOD:
            self.farm_food()
        elif self.state == State.EXPLORE:
            self.explore()
        else:
            self.look()

    # ------------------------------------------------------------------
    # GATHER / ACK
    # ------------------------------------------------------------------
    def gather_ack_window(self) -> float:
        return max(2.0, 80.0 / self.frequency)

    def gather_rebroadcast_interval(self) -> float:
        return max(0.6, 35.0 / self.frequency)

    def gather_arrival_timeout(self) -> float:
        return max(8.0, 300.0 / self.frequency)

    def gather_retry_cooldown(self) -> float:
        return max(4.0, 180.0 / self.frequency)

    def gather_answer_ttl(self) -> float:
        return max(30.0, 1000.0 / self.frequency)

    def max_gather_attempts(self) -> int:
        return 3

    def in_gather_retry_cooldown(self) -> bool:
        if self.last_gather_giveup_at <= 0:
            return False
        return time.time() - self.last_gather_giveup_at < self.gather_retry_cooldown()

    def is_following_gather(self) -> bool:
        if not self.following_gather_req:
            return False

        if time.time() > self.following_gather_until:
            self.following_gather_req = None
            self.following_gather_leader = None
            self.following_gather_until = 0.0
            return False

        return True

    def confirmed_gather_players(self) -> int:
        if not self.active_gather_req:
            return 1
        return 1 + len(self.active_gather_acks)

    def start_new_gather_request(self, required_players: int) -> bool:
        self.gather_request_counter += 1
        self.gather_attempts += 1
        self.active_gather_req = (
            f"{self.bot_id}-{self.memory.level}-{self.gather_request_counter}"
        )
        self.active_gather_level = self.memory.level
        self.active_gather_need = required_players
        self.active_gather_acks.clear()
        self.gather_started_at = time.time()
        self.gather_last_broadcast_at = 0.0
        self.gather_arrival_started_at = 0.0

        print(
            f"[AI] new gather req={self.active_gather_req} "
            f"need={required_players} attempt={self.gather_attempts}",
            file=sys.stderr,
        )

        return self.broadcast_active_gather()

    def broadcast_active_gather(self) -> bool:
        if not self.active_gather_req:
            return False

        msg = (
            f"{self.team_name}:GATHER:"
            f"req={self.active_gather_req}:"
            f"level={self.active_gather_level}:"
            f"need={self.active_gather_need}:"
            f"from={self.bot_id}"
        )

        if self.queue.send(f"Broadcast {msg}"):
            self.gather_last_broadcast_at = time.time()
            self.memory.last_broadcast_at = self.gather_last_broadcast_at
            return True

        return False

    def abort_gather(self, reason: str, mark_giveup: bool = False) -> None:
        if not self.active_gather_req:
            return

        print(
            f"[AI] abort gather req={self.active_gather_req}: {reason}",
            file=sys.stderr,
        )

        self.active_gather_req = None
        self.active_gather_level = 0
        self.active_gather_need = 0
        self.active_gather_acks.clear()
        self.gather_started_at = 0.0
        self.gather_last_broadcast_at = 0.0
        self.gather_attempts = 0
        self.gather_arrival_started_at = 0.0

        if mark_giveup:
            self.last_gather_giveup_at = time.time()

            self.following_gather_req = None
            self.following_gather_leader = None
            self.following_gather_until = 0.0

            if self.needs_more_teammates_for_incantation():
                self.must_fork_after_gather_fail = True
                print(
                    "[AI] gather failed -> switch objective to FARM_FOOD/FORK",
                    file=sys.stderr,
                )

    def call_teammates(self) -> None:
        now = time.time()
        food = self.memory.inventory.get("food", 0)
        required_players = INCANTATION_REQUIREMENTS[self.memory.level]["players"]

        if food <= self.gather_abort_food_threshold:
            self.abort_gather("food too low while waiting teammates", mark_giveup=True)
            self.state = State.SURVIVE
            self.survive()
            return

        # Le leader reste un point fixe, mais peut manger ce qui est sur sa case.
        if self.memory.visible_tiles and "food" in self.memory.visible_tiles[0]:
            self.queue.send("Take food")
            return

        players_on_tile = self.count_players_on_current_tile()

        if players_on_tile >= required_players:
            self.abort_gather("enough players on tile")
            self.state = State.PREPARE_INCANTATION
            self.prepare_incantation()
            return

        if not self.active_gather_req:
            self.gather_attempts = 0
            self.start_new_gather_request(required_players)
            return

        confirmed = self.confirmed_gather_players()

        if confirmed >= required_players:
            if self.gather_arrival_started_at <= 0:
                self.gather_arrival_started_at = now
                print(
                    f"[AI] enough ACK for req={self.active_gather_req} "
                    f"confirmed={confirmed}/{required_players}; waiting on tile",
                    file=sys.stderr,
                )

            if now - self.gather_last_broadcast_at > self.gather_rebroadcast_interval():
                self.broadcast_active_gather()
                return

            if now - self.gather_arrival_started_at > self.gather_arrival_timeout():
                self.abort_gather("mates did not arrive on tile", mark_giveup=True)
                if self.should_reproduce():
                    self.state = State.REPRODUCE
                    self.reproduce()
                else:
                    self.state = State.FARM_FOOD
                    self.farm_food()
                return

            self.queue.send("Look")
            return

        # Pas assez d'ACK : on attend un peu, puis nouveau req.
        if now - self.gather_started_at >= self.gather_ack_window():
            if self.gather_attempts < self.max_gather_attempts():
                self.start_new_gather_request(required_players)
                return

            self.abort_gather(
                f"not enough ACK ({confirmed}/{required_players})",
                mark_giveup=True,
            )

            if self.should_reproduce():
                self.state = State.REPRODUCE
                self.reproduce()
            else:
                self.state = State.FARM_FOOD
                self.farm_food()
            return

        if now - self.gather_last_broadcast_at > self.gather_rebroadcast_interval():
            self.broadcast_active_gather()
            return

        self.queue.send("Look")

    def should_answer_gather(self, level: int, leader_id: Optional[int]) -> bool:
        if leader_id == self.bot_id:
            return False

        if level != self.memory.level:
            return False

        if self.preparing_incantation or self.waiting_incantation_result:
            return False

        if self.memory.inventory.get("food", 0) <= self.survive_food_threshold:
            return False

        return True

    def queue_gather_ack(
        self,
        req_id: str,
        level: int,
        leader_id: Optional[int],
    ) -> None:
        if req_id in self.answered_gather_requests:
            return

        if self.pending_gather_ack_req == req_id:
            return

        if not self.should_answer_gather(level, leader_id):
            return

        self.pending_gather_ack_req = req_id
        self.pending_gather_ack_level = level
        self.pending_gather_ack_leader = leader_id

        print(
            f"[AI] queued GATHER_ACK req={req_id} leader={leader_id}",
            file=sys.stderr,
        )

    def send_pending_gather_ack(self) -> bool:
        if not self.pending_gather_ack_req:
            return False

        if self.preparing_incantation or self.waiting_incantation_result:
            self.pending_gather_ack_req = None
            return False

        if self.memory.inventory.get("food", 0) <= self.survive_food_threshold:
            self.pending_gather_ack_req = None
            return False

        req_id = self.pending_gather_ack_req
        level = self.pending_gather_ack_level

        msg = (
            f"{self.team_name}:GATHER_ACK:"
            f"req={req_id}:"
            f"level={level}:"
            f"from={self.bot_id}"
        )

        if not self.queue.send(f"Broadcast {msg}"):
            return False

        self.answered_gather_requests[req_id] = time.time()
        self.pending_gather_ack_req = None
        self.pending_gather_ack_level = 0
        self.pending_gather_ack_leader = None
        return True

    def prune_answered_gathers(self) -> None:
        now = time.time()
        ttl = self.gather_answer_ttl()
        self.answered_gather_requests = {
            req_id: answered_at
            for req_id, answered_at in self.answered_gather_requests.items()
            if now - answered_at <= ttl
        }

    def parse_team_payload(self, text: str) -> Tuple[str, Dict[str, str]]:
        parts = text.split(":")
        if len(parts) < 2 or parts[0] != self.team_name:
            return "", {}

        kind = parts[1]
        fields: Dict[str, str] = {}

        for part in parts[2:]:
            if "=" not in part:
                continue
            key, value = part.split("=", 1)
            fields[key.strip()] = value.strip()

        return kind, fields

    def int_field(self, fields: Dict[str, str], key: str) -> Optional[int]:
        if key not in fields:
            return None
        try:
            return int(fields[key])
        except ValueError:
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

    def handle_gather_message(
        self,
        direction: int,
        fields: Dict[str, str],
        sender_id: Optional[int],
    ) -> None:
        req_id = fields.get("req", "")
        level = self.int_field(fields, "level")

        if not req_id or level is None:
            return

        if sender_id is None or sender_id == self.bot_id:
            return

        self.memory.last_gather_at = time.time()

        # Si je suis déjà en train de suivre un leader, je garde le plus petit id.
        if self.is_following_gather():
            if (
                self.following_gather_leader is not None
                and sender_id > self.following_gather_leader
            ):
                return

        # Élection de leader déterministe :
        # le plus petit bot_id gagne.
        if self.active_gather_req:
            if sender_id < self.bot_id:
                self.abort_gather(
                    f"lower bot_id leader wins: {sender_id} < {self.bot_id}",
                    mark_giveup=False,
                )
            else:
                return

        if not self.should_answer_gather(level, sender_id):
            return

        self.following_gather_req = req_id
        self.following_gather_leader = sender_id
        self.following_gather_until = time.time() + self.gather_arrival_timeout()

        self.queue_gather_ack(req_id, level, sender_id)
        self.follow_broadcast_direction(direction)

    def handle_gather_ack(
        self,
        fields: Dict[str, str],
        sender_id: Optional[int],
    ) -> None:
        if sender_id is None:
            return

        req_id = fields.get("req", "")
        level = self.int_field(fields, "level")

        if not self.active_gather_req:
            return

        if req_id != self.active_gather_req:
            return

        if level != self.memory.level:
            return

        if sender_id not in self.active_gather_acks:
            print(
                f"[AI] GATHER_ACK req={req_id} from={sender_id} "
                f"confirmed={1 + len(self.active_gather_acks) + 1}/"
                f"{self.active_gather_need}",
                file=sys.stderr,
            )

        self.active_gather_acks[sender_id] = time.time()

    # ------------------------------------------------------------------
    # Basic states
    # ------------------------------------------------------------------
    def survive(self) -> None:
        if not self.memory.visible_tiles:
            self.queue.send("Look")
            return

        if self.memory.moves_since_look > 0:
            self.queue.send("Look")
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

    def incant(self) -> None:
        if self.waiting_incantation_result:
            return

        if self.queue.send("Incantation"):
            self.waiting_incantation_result = True

    # ------------------------------------------------------------------
    # Reproduction
    # ------------------------------------------------------------------
    def should_reproduce(self) -> bool:
        if self.preparing_incantation or self.waiting_incantation_result:
            return False

        if self.active_gather_req:
            return False

        if self.memory.forks_done >= self.max_forks:
            return False

        needs_team = self.needs_more_teammates_for_incantation()

        if self.memory.level > self.fork_max_level and not needs_team:
            return False

        if self.memory.inventory.get("food", 0) < self.fork_food_threshold:
            return False

        now = time.time()
        cooldown = max(3.0, 150.0 / self.frequency)
        if now - self.memory.last_fork_at < cooldown:
            return False

        return True

    def reproduce(self) -> None:
        self.abort_gather("reproduce")
        self.queue.send("Fork")

    def needs_more_teammates_for_incantation(self) -> bool:
        requirements = INCANTATION_REQUIREMENTS.get(self.memory.level)
        if not requirements:
            return False

        required_players = requirements["players"]
        if required_players <= 1:
            return False

        if not self.has_required_stones_for_level():
            return False

        return True

    def farm_food(self) -> None:
        if not self.memory.visible_tiles:
            self.queue.send("Look")
            return

        if self.memory.visible_tiles and "food" in self.memory.visible_tiles[0]:
            self.queue.send("Take food")
            return

        food_tile = self.find_best_food_tile()

        if food_tile is not None:
            self.move_towards_tile(food_tile, full_plan=True)
            return

        if self.memory.moves_since_look >= 1:
            self.queue.send("Look")
            return

        self.queue.send("Forward")

    # ------------------------------------------------------------------
    # Ejection
    # ------------------------------------------------------------------
    def should_eject(self) -> bool:
        if self.preparing_incantation or self.waiting_incantation_result:
            return False

        if self.active_gather_req:
            return False

        if self.is_following_gather():
            return False

        if self.memory.inventory.get("food", 0) < self.eject_food_threshold:
            return False

        if time.time() - self.memory.last_gather_at < max(5.0, 200.0 / self.frequency):
            return False

        if time.time() - self.memory.last_eject_at < max(3.0, 100.0 / self.frequency):
            return False

        return self.count_players_on_current_tile() > 1

    def do_eject(self) -> None:
        if self.queue.send("Eject"):
            self.memory.last_eject_at = time.time()
            self.memory.force_look = True

    def explore(self) -> None:
        if self.memory.was_ejected:
            self.memory.was_ejected = False
            self.queue.send("Look")
            return

        if self.is_following_gather():
            self.queue.send("Look")
            return

        if self.memory.moves_since_look >= 2:
            self.queue.send("Look")
            return

        roll = random.random()

        if roll < 0.65:
            self.queue.send("Forward")
        elif roll < 0.825:
            self.queue.send("Left")
        else:
            self.queue.send("Right")

    # ------------------------------------------------------------------
    # Inventory / Look helpers
    # ------------------------------------------------------------------
    def has_required_stones_for_level(self) -> bool:
        requirements = INCANTATION_REQUIREMENTS.get(self.memory.level)
        if not requirements:
            return False

        for stone in STONES:
            have = self.memory.inventory.get(stone, 0)
            need = requirements.get(stone, 0)

            if have < need:
                print(
                    f"[AI] missing for lvl {self.memory.level}: {stone} {have}/{need}",
                    file=sys.stderr,
                )
                return False

        print(
            f"[AI] has stones for lvl {self.memory.level}, ready to incant",
            file=sys.stderr,
        )
        return True

    def required_stones_list(self) -> List[str]:
        requirements = INCANTATION_REQUIREMENTS.get(self.memory.level, {})
        stones: List[str] = []

        for stone in STONES:
            for _ in range(requirements.get(stone, 0)):
                stones.append(stone)

        return stones

    def visible_useful_resource_exists(self) -> bool:
        if not self.memory.visible_tiles:
            return False

        for tile in self.memory.visible_tiles:
            for item in tile:
                if item == "food":
                    return True
                if item in STONES and self.need_stone(item):
                    return True

        return False

    def need_stone(self, stone: str) -> bool:
        requirements = INCANTATION_REQUIREMENTS.get(self.memory.level, {})
        return self.memory.inventory.get(stone, 0) < requirements.get(stone, 0)

    def best_resource_on_current_tile(self) -> Optional[str]:
        if not self.memory.visible_tiles:
            return None

        current_tile = self.memory.visible_tiles[0]

        if "food" in current_tile and self.memory.inventory.get("food", 0) <= 10:
            return "food"

        for stone in STONES:
            if stone in current_tile and self.need_stone(stone):
                return stone

        if "food" in current_tile:
            return "food"

        return None

    def find_best_resource_tile(self) -> Optional[int]:
        if not self.memory.visible_tiles:
            return None

        best_tile = None
        best_score = -999

        for index, tile in enumerate(self.memory.visible_tiles):
            score = 0

            for item in tile:
                if item == "food":
                    score += 10 if self.memory.inventory.get("food", 0) <= 10 else 2
                elif item in STONES and self.need_stone(item):
                    score += 5

            if score > best_score:
                best_score = score
                best_tile = index

        if best_score <= 0:
            return None

        return best_tile

    def find_best_food_tile(self) -> Optional[int]:
        if not self.memory.visible_tiles:
            return None

        best_tile = None
        best_score = -999

        for index, tile in enumerate(self.memory.visible_tiles):
            food_count = tile.count("food")
            if food_count <= 0:
                continue

            distance, offset = self.tile_index_to_relative_position(index)
            score = food_count * 12 - distance * 2 - abs(offset)

            if score > best_score:
                best_score = score
                best_tile = index

        return best_tile

    def find_tile_containing(self, resource: str) -> Optional[int]:
        if not self.memory.visible_tiles:
            print("[AI] no visible tiles in memory", file=sys.stderr)
            return None

        for index, tile in enumerate(self.memory.visible_tiles):
            if resource in tile:
                print(f"[AI] found {resource} on tile {index}", file=sys.stderr)
                return index

        print(f"[AI] {resource} not visible", file=sys.stderr)
        return None

    def count_players_on_current_tile(self) -> int:
        if not self.memory.visible_tiles:
            return 1

        return self.memory.visible_tiles[0].count("player")

    # ------------------------------------------------------------------
    # Movement helpers
    # ------------------------------------------------------------------
    def move_towards_tile(self, tile_index: int, full_plan: bool = False) -> None:
        if tile_index <= 0:
            return

        if self.movement_plan:
            return

        plan = self.tile_index_to_plan(tile_index, full_plan=full_plan)

        if not plan:
            return

        print(
            f"[AI] moving toward tile={tile_index} plan={plan}",
            file=sys.stderr,
        )

        first_command = plan.pop(0)
        self.movement_plan = plan
        self.queue.send(first_command)

    def tile_index_to_plan(self, tile_index: int, full_plan: bool = False) -> List[str]:
        distance, offset = self.tile_index_to_relative_position(tile_index)

        if distance <= 0:
            return []

        plan: List[str] = []

        if offset == 0:
            plan = ["Forward"] * distance
        elif offset < 0:
            plan.append("Left")
            plan.extend(["Forward"] * abs(offset))
            plan.append("Right")
            plan.extend(["Forward"] * distance)
        else:
            plan.append("Right")
            plan.extend(["Forward"] * abs(offset))
            plan.append("Left")
            plan.extend(["Forward"] * distance)

        if full_plan:
            return plan

        return plan[: self.max_plan_length]

    def tile_index_to_relative_position(self, tile_index: int) -> Tuple[int, int]:
        distance = int(tile_index ** 0.5)

        while (distance + 1) * (distance + 1) <= tile_index:
            distance += 1

        while distance * distance > tile_index:
            distance -= 1

        row_start = distance * distance
        center_index = row_start + distance
        offset = tile_index - center_index

        return distance, offset

    def follow_broadcast_direction(self, direction: int) -> None:
        if self.queue.has_pending() or self.waiting_incantation_result:
            return

        if self.preparing_incantation:
            return

        if self.memory.inventory.get("food", 0) <= self.survive_food_threshold:
            return

        if self.movement_plan:
            return

        plan = self.broadcast_direction_to_plan(direction)

        if not plan:
            return

        plan = plan[: self.max_plan_length]

        print(
            f"[AI] following broadcast direction={direction} plan={plan}",
            file=sys.stderr,
        )

        self.movement_plan = plan

    def broadcast_direction_to_plan(self, direction: int) -> List[str]:
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
        self.network.connect()
        self.handshake()

        while self.running:
            self.read_server_messages()
            self.brain.tick()
            time.sleep(0.002)

        self.network.close()

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
                print(
                    f"[AI] fork ok -> egg laid, waiting for external client "
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