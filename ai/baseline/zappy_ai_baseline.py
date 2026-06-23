#!/usr/bin/env python3

import argparse
import enum
import os
import random
import select
import socket
import subprocess
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

    # --- Reproduction (Fork) ---
    free_slots: int = 0          # slots libres connus (via Connect_nbr)
    last_connect_at: float = 0.0 # dernière mesure de Connect_nbr
    forks_done: int = 0          # nb de Fork réussis par CE bot
    spawned_children: int = 0     # nb de clients enfants lancés par CE process
    last_fork_at: float = 0.0
    want_spawn_child: bool = False  # demande au process de lancer un client

    # --- Ejection / rassemblement ---
    last_eject_at: float = 0.0
    last_gather_at: float = 0.0  # dernier GATHER reçu (on n'éjecte pas alors)


class NetworkClient:
    def __init__(self, host: str, port: int, team_name: str):
        self.host = host
        self.port = port
        self.team_name = team_name
        self.sock: Optional[socket.socket] = None
        self.recv_buffer = ""
        # Lignes déjà extraites du socket mais pas encore consommées par
        # read_blocking_line (évite de les perdre si plusieurs lignes
        # arrivent dans le même segment TCP, ex: pendant le handshake).
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
        # On sert d'abord les lignes déjà bufferisées.
        if self._blocking_buffer:
            return self._blocking_buffer.pop(0)

        start = time.time()

        while time.time() - start < timeout:
            lines = self.read_available_lines()
            if lines:
                # On bufferise TOUTES les lignes reçues et on en rend une.
                # Avant, lines[1:] était silencieusement perdu.
                self._blocking_buffer.extend(lines)
                return self._blocking_buffer.pop(0)
            time.sleep(0.005)

        raise TimeoutError("Timeout while waiting for server line")

    def drain_blocking_buffer(self) -> List[str]:
        """Récupère les lignes bufferisées non consommées (à ré-injecter
        dans le traitement normal avant de démarrer la boucle principale)."""
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

        # --- Politique de reproduction (Fork) ---
        # On reproduit uniquement quand on est bien nourri et de bas niveau :
        # les bots de haut niveau se concentrent sur l'élévation. Cela borne
        # naturellement la croissance (les bots qui montent arrêtent de forker).
        self.max_forks = 6            # plafond de Fork par bot (anti fork-bomb)
        self.fork_food_threshold = 25 # food mini pour se permettre un Fork
        self.fork_max_level = 3       # reproduction "normale" jusqu'au lvl 3

        # --- Seuils de survie / rassemblement ---
        # Avec peu de nourriture, il ne faut pas appeler l'équipe :
        # on sécurise d'abord assez de food pour attendre / pondre.
        self.survive_food_threshold = 8
        self.gather_food_threshold = 18

        # --- Politique d'éjection ---
        # L'éjection est offensive : comme les joueurs sont anonymes, on évite
        # d'éjecter pendant/juste après un rassemblement d'équipe.
        self.eject_food_threshold = 20

    def tick(self) -> None:
        if self.state == State.DEAD:
            return

        # Garde anti-blocage : si on attend un résultat d'incantation mais
        # que plus aucune commande n'est en vol (ex: timeout de la file),
        # on débloque pour ne pas geler le bot indéfiniment.
        if self.waiting_incantation_result and not self.queue.has_pending():
            self.waiting_incantation_result = False

        if self.waiting_incantation_result:
            return

        self.queue.clear_old_commands()

        if self.queue.has_pending():
            return

        food = self.memory.inventory.get("food", 0)

        if self.memory.force_look:
            self.memory.force_look = False
            self.queue.send("Look")
            return

        if self.movement_plan :
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

            # Important : ne pas spam GATHER si l'équipe connue est trop petite.
            # Dans ce cas, l'objectif devient : trouver assez de food puis pondre
            # ou remplir les slots libres existants.
            if not self.has_enough_known_teammates(required_players):
                if self.should_reproduce():
                    self.state = State.REPRODUCE
                else:
                    self.state = State.FARM_FOOD
                return

            # Même si les coéquipiers existent, appeler avec peu de food est
            # suicidaire : on farm d'abord un matelas pour attendre le groupe.
            if food < self.gather_food_threshold:
                self.state = State.FARM_FOOD
                return

            self.state = State.CALL_TEAMMATES
            return

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

        if self.stones_to_drop:
            stone = self.stones_to_drop.pop(0)
            self.queue.send(f"Set {stone}")
            return

        self.state = State.INCANT
        self.incant()

    def call_teammates(self) -> None:
        now = time.time()
        food = self.memory.inventory.get("food", 0)
        required_players = INCANTATION_REQUIREMENTS[self.memory.level]["players"]

        # Double sécurité : même si choose_state a déjà filtré, on évite
        # d'appeler si on sait que l'équipe est trop petite.
        if not self.has_enough_known_teammates(required_players):
            if self.should_reproduce():
                self.state = State.REPRODUCE
                self.reproduce()
            else:
                self.state = State.FARM_FOOD
                self.farm_food()
            return

        if food < self.gather_food_threshold:
            self.state = State.FARM_FOOD
            self.farm_food()
            return

        if self.memory.visible_tiles and "food" in self.memory.visible_tiles[0]:
            self.queue.send("Take food")
            return

        players_on_tile = self.count_players_on_current_tile()

        if players_on_tile >= required_players:
            self.state = State.PREPARE_INCANTATION
            self.prepare_incantation()
            return

        if now - self.memory.last_broadcast_at > max(0.4, 40.0 / self.frequency):
            msg = (
                f"{self.team_name}:GATHER:"
                f"level={self.memory.level}:"
                f"from={self.bot_id}"
            )
            self.queue.send(f"Broadcast {msg}")
            self.memory.last_broadcast_at = now
            return

        # Le leader ne doit pas partir en exploration pendant le rassemblement.
        # Il garde la tile comme point de rendez-vous et rafraîchit juste la vue.
        self.queue.send("Look")

    def incant(self) -> None:
        if self.waiting_incantation_result:
            return

        if self.queue.send("Incantation"):
            self.waiting_incantation_result = True

    # ------------------------------------------------------------------
    # Reproduction (Fork)
    # ------------------------------------------------------------------
    def should_reproduce(self) -> bool:
        if self.preparing_incantation or self.waiting_incantation_result:
            return False

        if self.memory.forks_done >= self.max_forks:
            return False

        needs_team = self.needs_more_teammates_for_incantation()

        # En temps normal, les hauts niveaux évitent de forker. Mais si un
        # niveau 4+ a les pierres et pas assez d'équipe, il DOIT pouvoir pondre.
        if self.memory.level > self.fork_max_level and not needs_team:
            return False

        # D'après le sujet, Fork est l'action qui produit un œuf et ajoute
        # ensuite un slot. Donc on ne spawn pas un enfant juste parce que
        # Connect_nbr indique un slot libre : pour cette stratégie, une
        # reproduction = assez de food -> Fork -> spawn child après ok.
        if self.memory.inventory.get("food", 0) < self.fork_food_threshold:
            return False

        # Cooldown dépendant de la fréquence (Fork coûte 42/f).
        now = time.time()
        cooldown = max(3.0, 150.0 / self.frequency)
        if now - self.memory.last_fork_at < cooldown:
            return False

        return True

    def reproduce(self) -> None:
        # Reproduction stricte : on pond toujours un œuf d'abord. Le nouveau
        # client ne sera lancé qu'après le "ok" du Fork, ce qui garantit qu'un
        # slot vient bien d'être créé par CE joueur.
        self.queue.send("Fork")

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

    def teammate_memory_ttl(self) -> float:
        return max(20.0, 800.0 / self.frequency)

    def prune_known_teammates(self) -> None:
        now = time.time()
        ttl = self.teammate_memory_ttl()
        self.memory.known_teammates = {
            bot_id: seen_at
            for bot_id, seen_at in self.memory.known_teammates.items()
            if now - seen_at <= ttl
        }

    def estimated_team_size(self) -> int:
        self.prune_known_teammates()

        # On prend le max pour éviter de compter deux fois un enfant qu'on a
        # lancé puis entendu par Broadcast. C'est volontairement conservateur.
        heard_team_size = 1 + len(self.memory.known_teammates)
        spawned_team_size = 1 + self.memory.spawned_children
        return max(heard_team_size, spawned_team_size)

    def has_enough_known_teammates(self, required_players: int) -> bool:
        known = self.estimated_team_size()

        if known >= required_players:
            return True

        print(
            f"[AI] not enough known teammates for lvl {self.memory.level}: "
            f"known={known}/{required_players}; farming/forking instead of GATHER",
            file=sys.stderr,
        )
        return False

    def needs_more_teammates_for_incantation(self) -> bool:
        requirements = INCANTATION_REQUIREMENTS.get(self.memory.level)
        if not requirements:
            return False

        required_players = requirements["players"]
        if required_players <= 1:
            return False

        if not self.has_required_stones_for_level():
            return False

        return not self.has_enough_known_teammates(required_players)

    # ------------------------------------------------------------------
    # Ejection
    # ------------------------------------------------------------------
    def should_eject(self) -> bool:
        # Offensif et risqué (joueurs anonymes) : on reste très prudent.
        if self.preparing_incantation or self.waiting_incantation_result:
            return False

        if self.memory.inventory.get("food", 0) < self.eject_food_threshold:
            return False

        # Jamais juste après un rassemblement d'équipe : on ne disperse pas
        # nos propres coéquipiers.
        if time.time() - self.memory.last_gather_at < max(5.0, 200.0 / self.frequency):
            return False

        # Cooldown d'éjection.
        if time.time() - self.memory.last_eject_at < max(3.0, 100.0 / self.frequency):
            return False

        # Seulement si d'autres joueurs partagent notre tuile.
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
            # Les axes sont indépendants : on se décale de |offset| sur le côté,
            # PUIS on avance de `distance` tuiles (et non distance - |offset|).
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

    def handle_team_message(self, direction: int, text: str) -> None:
        if not text.startswith(f"{self.team_name}:"):
            return

        sender_id = self.extract_sender_id(text)
        if sender_id == self.bot_id:
            return

        if sender_id is not None:
            self.memory.known_teammates[sender_id] = time.time()

        self.memory.team_messages.append((direction, text))

        if ":GATHER:" in text:
            self.memory.last_gather_at = time.time()
            self.follow_broadcast_direction(direction)

    def extract_sender_id(self, text: str) -> Optional[int]:
        marker = "from="
        if marker not in text:
            return None

        raw = text.split(marker, 1)[1].split(":", 1)[0].strip()
        try:
            return int(raw)
        except ValueError:
            return None

    def follow_broadcast_direction(self, direction: int) -> None:
        if self.queue.has_pending() or self.waiting_incantation_result:
            return

        if self.preparing_incantation:
            return

        food = self.memory.inventory.get("food", 0)
        if food <= self.survive_food_threshold:
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
        # Numérotation du son autour du joueur (face en haut) :
        #   2 1 8
        #   3 . 7
        #   4 5 6
        # On se tourne vers la moitié d'où vient le son, puis on avance.
        if direction == 0:
            return ["Look"]

        if direction == 1:
            return ["Forward"]

        if direction in (2, 3):          # gauche / avant-gauche
            return ["Left", "Forward"]

        if direction in (4, 5):          # arrière(-gauche) : demi-tour
            return ["Left", "Left", "Forward"]

        if direction == 6:               # arrière-droite : demi-tour
            return ["Right", "Right", "Forward"]

        if direction in (7, 8):          # droite / avant-droite
            return ["Right", "Forward"]

        return []


class FrequencyEstimator:
    """Mesure la fréquence RÉELLE du serveur pour s'y synchroniser.

    Principe (cf. sujet: temps_réel = coût_action / f) :
      - Une commande de coût NUL (Connect_nbr, temps "-") sert de référence
        pour la latence réseau (RTT) : T_connect ~= RTT + bruit.
      - Une commande de coût CONNU (Forward = 7) : T_forward ~= 7/f + RTT + bruit.
      - En soustrayant, le RTT s'annule :  f = 7 / (T_forward - T_connect).

    On prend le MINIMUM sur plusieurs échantillons : le bruit (réseau, GC,
    ordonnancement du serveur) est toujours POSITIF (il ajoute du temps,
    jamais n'en retire), donc min(T) tend vers la vraie valeur.
    f étant un entier (sujet), on arrondit : on retombe exactement sur f.
    """

    FORWARD_COST = 7  # unités de temps d'un Forward (sujet)

    def __init__(self, network: "NetworkClient"):
        self.network = network
        # Lignes asynchrones reçues pendant la mesure (broadcast, eject, dead)
        # à ré-injecter ensuite dans le traitement normal.
        self.deferred_lines: List[str] = []
        # Valeur renvoyée par Connect_nbr (slots libres) : bonus utile.
        self.last_connect_value: Optional[int] = None

    def _round_trip(self, command: str, is_response, guard: float = 60.0):
        """Envoie une commande et mesure le temps jusqu'à SA réponse.
        Renvoie le temps (float) ou None si le joueur meurt / timeout."""
        start = time.perf_counter()
        self.network.send_line(command)

        while time.perf_counter() - start < guard:
            for line in self.network.read_available_lines():
                if line == "dead":
                    self.deferred_lines.append(line)
                    return None
                if is_response(line):
                    return time.perf_counter() - start
                # Ligne asynchrone (message K,..., eject:...) : on la diffère.
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

        # Soustraction du RTT (annulé) + minimum (filtre le bruit positif).
        delta = min(forward_times) - min(connect_times)
        if delta <= 0:
            # Jitter pathologique : repli sans soustraction du RTT.
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
        # Si -f est fourni, on le croit ; sinon on mesurera f au handshake.
        self.frequency_override = frequency_override
        self.frequency = max(1, frequency_override or 100)  # placeholder
        self.network = NetworkClient(host, port, team_name)
        self.memory = WorldMemory()
        self.queue = CommandQueue(self.network, self.frequency)
        self.brain = Brain(team_name, self.memory, self.queue, self.frequency)
        self.running = True

        # Clients enfants lancés après un Fork (pour faire éclore les œufs).
        self.children: List[subprocess.Popen] = []
        self.max_children = 4  # plafond par process (anti fork-bomb)

    def run(self) -> None:
        self.network.connect()
        self.handshake()

        while self.running:
            self.read_server_messages()
            self.brain.tick()

            if self.memory.want_spawn_child:
                self.memory.want_spawn_child = False
                self.spawn_child()

            time.sleep(0.002)

        self.network.close()

    def spawn_child(self) -> None:
        """Lance un nouveau client zappy_ai pour occuper un slot libre
        (un œuf de l'équipe éclot alors). Plafonné pour éviter un fork-bomb."""
        self.children = [c for c in self.children if c.poll() is None]
        if len(self.children) >= self.max_children:
            return

        cmd = [
            sys.executable,
            os.path.abspath(sys.argv[0]),
            "-p", str(self.network.port),
            "-n", self.network.team_name,
            "-h", self.network.host,
        ]

        try:
            child = subprocess.Popen(
                cmd,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            self.children.append(child)
            self.memory.spawned_children += 1
            # L'enfant occupe le slot créé par le Fork précédent.
            self.memory.free_slots = max(0, self.memory.free_slots - 1)
            self.memory.last_connect_at = 0.0
            print(f"[AI] spawned child client (pid={child.pid})", file=sys.stderr)
        except Exception as error:
            print(f"[AI] failed to spawn child: {error}", file=sys.stderr)

    def apply_frequency(self, freq: int) -> None:
        """Propage la fréquence (mesurée ou override) à tous les composants
        qui en dépendent pour leurs calculs de temps."""
        freq = max(1, int(freq))
        self.frequency = freq
        self.queue.frequency = freq
        self.brain.frequency = freq
        # Plan de déplacement plus court à haute fréquence (actions rapides),
        # plus long à basse fréquence (chaque Look coûte cher).
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

        # Lignes éventuellement bufferisées pendant le handshake : on les
        # traite avant toute autre chose pour préserver l'ordre serveur.
        for line in self.network.drain_blocking_buffer():
            self.handle_line(line)

        # --- Synchronisation de la fréquence sur le serveur ---
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
        if command == "Inventory":
            return

        if command == "Look":
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

        if command in ("Fork", "Broadcast", "Eject"):
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
            self.queue.pop_expected_response()
            return

        if line == "Elevation underway":
            return

        # Réponse de Connect_nbr : un entier nu = nombre de slots libres.
        # Sans ça, le pending Connect_nbr ne serait jamais dépilé (file bloquée).
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

            if line == "ok" and command == "Fork":
                self.memory.forks_done += 1
                self.memory.last_fork_at = time.time()
                # L'œuf est pondu -> un slot s'est libéré : on le remplit.
                self.memory.want_spawn_child = True
                print(
                    f"[AI] fork ok -> egg laid "
                    f"(forks_done={self.memory.forks_done})",
                    file=sys.stderr,
                )

            if line == "ko":
                self.brain.stones_to_drop.clear()
                self.brain.preparing_incantation = False
                self.brain.waiting_incantation_result = False
                self.brain.movement_plan.clear()

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
        "-f freq      optional: force server frequency (measured if omitted)\n",
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