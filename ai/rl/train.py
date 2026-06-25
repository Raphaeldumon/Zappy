#!/usr/bin/env python3
from __future__ import annotations

import argparse
import contextlib
import csv
import os
import random
import select
import socket
import subprocess
import sys
import time
from collections import deque
from dataclasses import dataclass, field
from typing import Callable, Deque, Dict, List, Optional, Tuple

import numpy as np

try:
    import gymnasium as gym
    from gymnasium import spaces
except ImportError:
    print("[train_rl] gymnasium manquant: pip install gymnasium", file=sys.stderr)
    raise

try:
    import torch
    import torch.nn as nn
    import torch.nn.functional as F
except ImportError:
    print("[train_rl] pytorch manquant: pip install torch", file=sys.stderr)
    raise


_THIS_DIR = os.path.dirname(os.path.abspath(__file__))

# Compatible avec :
#   ai/train.py
#   ai/rl/train.py
_PARENT_DIR = os.path.dirname(_THIS_DIR)
_BASELINE_DIRS = [
    os.path.join(_THIS_DIR, "baseline"),
    os.path.join(_PARENT_DIR, "baseline"),
]

for path in (_THIS_DIR, _PARENT_DIR):
    if path and path not in sys.path:
        sys.path.insert(0, path)

for path in _BASELINE_DIRS:
    if os.path.isdir(path) and path not in sys.path:
        sys.path.insert(0, path)


# --------------------------------------------------------------------------- #
# Import baseline + fallback compat.
# --------------------------------------------------------------------------- #

try:
    from baseline.zappy_ai_baseline import RESOURCES, STONES  # type: ignore
except ImportError:
    try:
        from zappy_ai_baseline import RESOURCES, STONES  # type: ignore
    except ImportError as exc:
        print(
            "[train_rl] impossible d'importer baseline/zappy_ai_baseline.py",
            file=sys.stderr,
        )
        raise exc


try:
    from baseline.zappy_ai_baseline import INCANTATION_REQUIREMENTS  # type: ignore
except ImportError:
    try:
        from zappy_ai_baseline import INCANTATION_REQUIREMENTS  # type: ignore
    except ImportError:
        try:
            from baseline.zappy_ai_baseline import REQ as INCANTATION_REQUIREMENTS  # type: ignore
        except ImportError:
            from zappy_ai_baseline import REQ as INCANTATION_REQUIREMENTS  # type: ignore


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

        if not clean:
            return [[]]

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
            with contextlib.suppress(Exception):
                self.sock.close()
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
            except (ConnectionResetError, BrokenPipeError, OSError):
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

            time.sleep(0.001)

        raise TimeoutError("timeout waiting server")

    def drain_blocking_buffer(self) -> List[str]:
        lines = self.blocking_buffer[:]
        self.blocking_buffer.clear()
        return lines


MAX_LEVEL = 8


def tile_relative_position(tile_index: int) -> Tuple[int, int]:
    if tile_index <= 0:
        return 0, 0

    distance = int(tile_index ** 0.5)

    while (distance + 1) * (distance + 1) <= tile_index:
        distance += 1

    while distance * distance > tile_index:
        distance -= 1

    row_start = distance * distance
    center_index = row_start + distance
    offset = tile_index - center_index

    return distance, offset


class RealServerLauncher:
    def __init__(
        self,
        server_bin: str,
        port: int,
        width: int,
        height: int,
        teams: List[str],
        clients: int,
        frequency: int,
    ):
        self.server_bin = server_bin
        self.port = port
        self.width = width
        self.height = height
        self.teams = teams
        self.clients = clients
        self.frequency = frequency
        self.proc: Optional[subprocess.Popen] = None

    def start(self) -> None:
        if not os.path.exists(self.server_bin):
            raise FileNotFoundError(f"zappy_server introuvable: {self.server_bin}")

        cmd = [
            self.server_bin,
            "-p",
            str(self.port),
            "-x",
            str(self.width),
            "-y",
            str(self.height),
            "-n",
            *self.teams,
            "-c",
            str(self.clients),
            "-f",
            str(self.frequency),
        ]

        print(f"[train_rl] lancement serveur: {' '.join(cmd)}", file=sys.stderr)

        self.proc = subprocess.Popen(
            cmd,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        deadline = time.time() + 5.0

        while time.time() < deadline:
            with contextlib.suppress(OSError):
                with socket.create_connection(("localhost", self.port), timeout=0.2):
                    return

            if self.proc.poll() is not None:
                raise RuntimeError("le serveur s'est arrêté au démarrage")

            time.sleep(0.05)

        raise TimeoutError("le serveur n'écoute pas après 5 s")

    def stop(self) -> None:
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()

            with contextlib.suppress(Exception):
                self.proc.wait(timeout=2.0)

            if self.proc.poll() is None:
                self.proc.kill()

        self.proc = None


@dataclass
class GameState:
    width: int = 0
    height: int = 0
    level: int = 1
    alive: bool = True
    inventory: Dict[str, int] = field(
        default_factory=lambda: {r: 0 for r in RESOURCES}
    )
    tiles: List[List[str]] = field(default_factory=list)

    def tile0(self) -> List[str]:
        return self.tiles[0] if self.tiles else []

    def players_on_tile0(self) -> int:
        return self.tile0().count("player") + 1

    def ground_count(self, tile: List[str], resource: str) -> int:
        return tile.count(resource)


class SyncConnection:
    def __init__(self, host: str, port: int, team: str, frequency: int):
        self.net = NetworkClient(host, port, team)
        self.frequency = max(1, frequency)
        self.state = GameState()

    def connect(self) -> None:
        self.net.connect()

        welcome = self.net.read_blocking_line(timeout=5.0)
        if welcome != "WELCOME":
            raise RuntimeError(f"handshake invalide, attendu WELCOME, reçu {welcome!r}")

        if not self._safe_send(self.net.team_name):
            raise RuntimeError("connexion morte pendant l'envoi du nom d'équipe")

        _client_num = self.net.read_blocking_line(timeout=5.0)
        dims = self.net.read_blocking_line(timeout=5.0)

        try:
            w, h = dims.split()
            self.state.width = int(w)
            self.state.height = int(h)
        except ValueError:
            raise RuntimeError(f"dimensions invalides: {dims!r}")

        for line in self.net.drain_blocking_buffer():
            self._handle_async(line)

        self.request_inventory()
        self.request_look()

    def close(self) -> None:
        self.net.close()

    def _timeout(self, ticks: float, floor: float = 0.02, extra: float = 0.02) -> float:
        return max(floor, ticks / self.frequency + extra)

    def _safe_send(self, command: str) -> bool:
        if not self.state.alive:
            return False

        try:
            self.net.send_line(command)
            return True
        except (BrokenPipeError, ConnectionResetError, OSError):
            self.state.alive = False
            return False

    def _handle_async(self, line: str) -> None:
        if line == "dead":
            self.state.alive = False
            return

        if line.startswith("eject:"):
            return

        lvl = Parser.parse_current_level(line)
        if lvl is not None:
            self.state.level = lvl
            return

        if Parser.parse_broadcast(line) is not None:
            return

        if line == "Elevation underway":
            return

    def _drain(self, timeout: float, accept: Callable[[str], bool]) -> Optional[str]:
        start = time.time()

        while time.time() - start < timeout:
            try:
                lines = self.net.read_available_lines()
            except (BrokenPipeError, ConnectionResetError, OSError):
                self.state.alive = False
                return "dead"

            for line in lines:
                if not line:
                    continue

                if line == "dead":
                    self.state.alive = False
                    return "dead"

                if accept(line):
                    return line

                self._handle_async(line)

            if not self.state.alive:
                return "dead"

            time.sleep(0.0001)

        return None

    def request_inventory(self) -> None:
        if not self._safe_send("Inventory"):
            return

        line = self._drain(
            timeout=self._timeout(1, floor=0.01, extra=0.05),
            accept=Parser.is_inventory_response,
        )

        if line and line != "dead":
            self.state.inventory = Parser.parse_inventory(line)

    def request_look(self) -> None:
        if not self._safe_send("Look"):
            return

        line = self._drain(
            timeout=self._timeout(7, floor=0.01, extra=0.05),
            accept=Parser.is_look_response,
        )

        if line and line != "dead":
            self.state.tiles = Parser.parse_look(line)

    def execute(self, command: str) -> str:
        if not self._safe_send(command):
            return "dead"

        if command == "Incantation":
            def accept(line: str) -> bool:
                return (
                    line in ("ok", "ko")
                    or Parser.parse_current_level(line) is not None
                )

            timeout = self._timeout(300, floor=0.2, extra=0.2)
        else:
            def accept(line: str) -> bool:
                return line in ("ok", "ko")

            timeout = self._timeout(7, floor=0.01, extra=0.05)

        line = self._drain(timeout=timeout, accept=accept)

        if line is None:
            return "timeout"

        if line == "dead":
            return "dead"

        lvl = Parser.parse_current_level(line)
        if lvl is not None:
            self.state.level = lvl
            return "level"

        return line


ACTIONS: List[str] = (
    ["Forward", "Right", "Left"]
    + [f"Take {r}" for r in RESOURCES]
    + [f"Set {s}" for s in STONES]
    + ["Incantation"]
)

ACTION_FORWARD = 0
TAKE_SLICE = slice(3, 3 + len(RESOURCES))
SET_SLICE = slice(3 + len(RESOURCES), 3 + len(RESOURCES) + len(STONES))
ACTION_INCANT = len(ACTIONS) - 1


def next_level_requirements(level: int) -> Dict[str, int]:
    req = INCANTATION_REQUIREMENTS.get(level)

    if req is None:
        return {s: 0 for s in STONES}

    return {s: req.get(s, 0) for s in STONES}


OBS_DIM = 1 + len(STONES) + 1 + len(STONES) + len(RESOURCES) + 1 + 1


class ZappyEnv(gym.Env):
    metadata = {"render_modes": []}

    def __init__(
        self,
        host: str = "localhost",
        port: int = 4242,
        frequency: int = 100,
        team: str = "RL",
        target_level: int = 2,
        max_steps: int = 200,
        food_target: int = 30,
        reward_level: float = 50.0,
        reward_death: float = 25.0,
        reward_step: float = 0.02,
        coef_stone: float = 10.0,
        coef_food: float = 2.0,
        gamma: float = 0.99,
    ):
        super().__init__()

        self.host = host
        self.port = port
        self.frequency = max(1, frequency)
        self.team = team
        self.target_level = target_level
        self.max_steps = max_steps
        self.food_target = food_target
        self.reward_level = reward_level
        self.reward_death = reward_death
        self.reward_step = reward_step
        self.coef_stone = coef_stone
        self.coef_food = coef_food
        self.gamma = gamma

        self.action_space = spaces.Discrete(len(ACTIONS))
        self.observation_space = spaces.Box(
            low=0.0,
            high=1.0,
            shape=(OBS_DIM,),
            dtype=np.float32,
        )

        self.conn: Optional[SyncConnection] = None
        self.steps = 0
        self._prev_phi_stone = 0.0
        self._prev_phi_food = 0.0
        self._prev_level = 1

    def _phi_stone(self, st: GameState) -> float:
        req = next_level_requirements(st.level)
        needed = [s for s in STONES if req[s] > 0]

        if not needed:
            return 0.0

        tile = st.tile0()
        fracs = [min(1.0, st.ground_count(tile, s) / req[s]) for s in needed]

        return float(np.mean(fracs))

    def _phi_food(self, st: GameState) -> float:
        return min(1.0, st.inventory.get("food", 0) / max(1, self.food_target))

    def _encode(self, st: GameState) -> np.ndarray:
        obs = np.zeros(OBS_DIM, dtype=np.float32)
        i = 0

        obs[i] = min(1.0, st.inventory.get("food", 0) / 50.0)
        i += 1

        for s in STONES:
            obs[i] = min(1.0, st.inventory.get(s, 0) / 5.0)
            i += 1

        obs[i] = (st.level - 1) / (MAX_LEVEL - 1)
        i += 1

        req = next_level_requirements(st.level)
        tile = st.tile0()

        for s in STONES:
            if req[s] > 0:
                deficit = max(0, req[s] - st.ground_count(tile, s))
                obs[i] = deficit / req[s]
            else:
                obs[i] = 0.0
            i += 1

        max_d = max(1, self._vision_depth(st.level))
        nearest = {r: None for r in RESOURCES}

        for idx, t in enumerate(st.tiles):
            d, _ = tile_relative_position(idx)

            for r in RESOURCES:
                if r in t and (nearest[r] is None or d < nearest[r]):
                    nearest[r] = d

        for r in RESOURCES:
            obs[i] = 0.0 if nearest[r] is None else (1.0 - nearest[r] / max_d)
            i += 1

        obs[i] = min(1.0, st.players_on_tile0() / 6.0)
        i += 1

        req_full = INCANTATION_REQUIREMENTS.get(st.level)
        ready = 0.0

        if req_full is not None:
            stones_ok = all(st.ground_count(tile, s) >= req[s] for s in STONES)
            players_ok = st.players_on_tile0() >= req_full["players"]
            ready = 1.0 if stones_ok and players_ok else 0.0

        obs[i] = ready

        return obs

    @staticmethod
    def _vision_depth(level: int) -> int:
        return level

    def reset(self, *, seed: Optional[int] = None, options=None):
        super().reset(seed=seed)
        self.close()

        last_err: Optional[Exception] = None

        for _ in range(20):
            try:
                self.conn = SyncConnection(
                    self.host,
                    self.port,
                    self.team,
                    self.frequency,
                )
                self.conn.connect()
                break
            except Exception as err:
                last_err = err
                self.conn = None
                time.sleep(0.05)

        if self.conn is None:
            raise RuntimeError(
                f"impossible de se connecter au serveur {self.host}:{self.port} "
                f"(team={self.team}). Dernière erreur: {last_err}"
            )

        self.steps = 0

        st = self.conn.state
        self._prev_level = st.level
        self._prev_phi_stone = self._phi_stone(st)
        self._prev_phi_food = self._phi_food(st)

        return self._encode(st), {}

    def step(self, action: int):
        assert self.conn is not None

        command = ACTIONS[int(action)]
        outcome = self.conn.execute(command)
        st = self.conn.state

        if outcome == "dead" or not st.alive:
            self.steps += 1

            info = {
                "outcome": "dead",
                "level": st.level,
                "food": st.inventory.get("food", 0),
                "steps": self.steps,
            }

            return (
                self._encode(st),
                float(-self.reward_step - self.reward_death),
                True,
                False,
                info,
            )

        if command in ("Forward", "Right", "Left"):
            self.conn.request_look()
        elif command.startswith("Take ") or command.startswith("Set ") or command == "Incantation":
            self.conn.request_inventory()

            if self.conn.state.alive:
                self.conn.request_look()

        st = self.conn.state
        self.steps += 1

        if not st.alive:
            info = {
                "outcome": "dead",
                "level": st.level,
                "food": st.inventory.get("food", 0),
                "steps": self.steps,
            }

            return (
                self._encode(st),
                float(-self.reward_step - self.reward_death),
                True,
                False,
                info,
            )

        reward = -self.reward_step
        terminated = False
        truncated = False

        if st.level > self._prev_level:
            reward += self.reward_level * (st.level - self._prev_level)

        phi_stone = self._phi_stone(st)
        phi_food = self._phi_food(st)

        reward += self.coef_stone * (self.gamma * phi_stone - self._prev_phi_stone)
        reward += self.coef_food * (self.gamma * phi_food - self._prev_phi_food)

        self._prev_phi_stone = phi_stone
        self._prev_phi_food = phi_food
        self._prev_level = st.level

        if st.level >= self.target_level:
            terminated = True
        elif self.steps >= self.max_steps:
            truncated = True

        info = {
            "outcome": outcome,
            "level": st.level,
            "food": st.inventory.get("food", 0),
            "steps": self.steps,
        }

        return self._encode(st), float(reward), terminated, truncated, info

    def close(self):
        if self.conn is not None:
            with contextlib.suppress(Exception):
                self.conn.close()

            self.conn = None


class QNetwork(nn.Module):
    def __init__(self, obs_dim: int, n_actions: int, hidden: int = 128):
        super().__init__()

        self.net = nn.Sequential(
            nn.Linear(obs_dim, hidden),
            nn.ReLU(),
            nn.Linear(hidden, hidden),
            nn.ReLU(),
            nn.Linear(hidden, n_actions),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)


@dataclass
class Transition:
    s: np.ndarray
    a: int
    r: float
    s2: np.ndarray
    done: float


class ReplayBuffer:
    def __init__(self, capacity: int):
        self.buf: Deque[Transition] = deque(maxlen=capacity)

    def push(self, *args) -> None:
        self.buf.append(Transition(*args))

    def sample(self, batch: int):
        items = random.sample(self.buf, batch)

        s = torch.as_tensor(np.array([t.s for t in items]), dtype=torch.float32)
        a = torch.as_tensor([t.a for t in items], dtype=torch.int64).unsqueeze(1)
        r = torch.as_tensor([t.r for t in items], dtype=torch.float32).unsqueeze(1)
        s2 = torch.as_tensor(np.array([t.s2 for t in items]), dtype=torch.float32)
        d = torch.as_tensor([t.done for t in items], dtype=torch.float32).unsqueeze(1)

        return s, a, r, s2, d

    def __len__(self) -> int:
        return len(self.buf)


class DQNAgent:
    def __init__(
        self,
        obs_dim: int,
        n_actions: int,
        lr: float = 1e-3,
        gamma: float = 0.99,
        buffer_size: int = 50_000,
        batch_size: int = 64,
        target_sync: int = 500,
        hidden: int = 128,
        device: str = "cpu",
    ):
        self.device = torch.device(device)
        self.obs_dim = obs_dim
        self.n_actions = n_actions
        self.gamma = gamma
        self.batch_size = batch_size
        self.target_sync = target_sync

        self.q = QNetwork(obs_dim, n_actions, hidden).to(self.device)
        self.target = QNetwork(obs_dim, n_actions, hidden).to(self.device)
        self.target.load_state_dict(self.q.state_dict())

        self.opt = torch.optim.Adam(self.q.parameters(), lr=lr)
        self.buffer = ReplayBuffer(buffer_size)
        self.learn_steps = 0

    def act(self, obs: np.ndarray, epsilon: float) -> int:
        if random.random() < epsilon:
            return random.randrange(self.n_actions)

        with torch.no_grad():
            t = torch.as_tensor(
                obs,
                dtype=torch.float32,
                device=self.device,
            ).unsqueeze(0)

            return int(self.q(t).argmax(dim=1).item())

    def learn(self) -> Optional[float]:
        if len(self.buffer) < self.batch_size:
            return None

        s, a, r, s2, d = self.buffer.sample(self.batch_size)

        s = s.to(self.device)
        a = a.to(self.device)
        r = r.to(self.device)
        s2 = s2.to(self.device)
        d = d.to(self.device)

        q_sa = self.q(s).gather(1, a)

        with torch.no_grad():
            next_a = self.q(s2).argmax(dim=1, keepdim=True)
            q_next = self.target(s2).gather(1, next_a)
            target = r + self.gamma * (1.0 - d) * q_next

        loss = F.smooth_l1_loss(q_sa, target)

        self.opt.zero_grad()
        loss.backward()
        nn.utils.clip_grad_norm_(self.q.parameters(), 10.0)
        self.opt.step()

        self.learn_steps += 1

        if self.learn_steps % self.target_sync == 0:
            self.target.load_state_dict(self.q.state_dict())

        return float(loss.item())

    def save(self, path: str, meta: Optional[Dict] = None) -> None:
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)

        torch.save(
            {
                "model": self.q.state_dict(),
                "obs_dim": self.obs_dim,
                "n_actions": self.n_actions,
                "actions": ACTIONS,
                "meta": meta or {},
            },
            path,
        )

    def load(self, path: str) -> Dict:
        ckpt = torch.load(path, map_location=self.device)
        self.q.load_state_dict(ckpt["model"])
        self.target.load_state_dict(self.q.state_dict())
        return ckpt


def setup_server(args: argparse.Namespace) -> Tuple[Optional[RealServerLauncher], str, int]:
    if args.launch_server:
        launcher = RealServerLauncher(
            server_bin=args.launch_server,
            port=args.port,
            width=args.width,
            height=args.height,
            teams=[args.team],
            clients=args.clients,
            frequency=args.frequency,
        )
        launcher.start()
        return launcher, "localhost", args.port

    print(
        f"[train_rl] connexion au serveur existant {args.host}:{args.port} "
        f"(team={args.team}, f={args.frequency}). "
        f"Lance-le, ex.: zappy_server -p {args.port} -x {args.width} "
        f"-y {args.height} -n {args.team} -c {args.clients} -f {args.frequency}",
        file=sys.stderr,
    )

    return None, args.host, args.port


def linear_epsilon(step: int, start: float, end: float, decay_steps: int) -> float:
    if step >= decay_steps:
        return end

    return start + (end - start) * (step / decay_steps)


def train(args: argparse.Namespace) -> None:
    random.seed(args.seed)
    np.random.seed(args.seed)
    torch.manual_seed(args.seed)

    launcher, host, port = setup_server(args)

    env = ZappyEnv(
        host=host,
        port=port,
        frequency=args.frequency,
        team=args.team,
        target_level=args.target_level,
        max_steps=args.max_steps,
        gamma=args.gamma,
    )

    agent = DQNAgent(
        obs_dim=OBS_DIM,
        n_actions=len(ACTIONS),
        lr=args.lr,
        gamma=args.gamma,
        buffer_size=args.buffer,
        batch_size=args.batch,
        target_sync=args.target_sync,
        hidden=args.hidden,
        device=args.device,
    )

    if args.load:
        agent.load(args.load)
        print(f"[train_rl] modèle chargé depuis {args.load}", file=sys.stderr)

    os.makedirs(args.out_dir, exist_ok=True)

    log_path = os.path.join(args.out_dir, "train_log.csv")
    log_file = open(log_path, "w", newline="")
    writer = csv.writer(log_file)

    writer.writerow(
        [
            "episode",
            "return",
            "steps",
            "max_level",
            "final_food",
            "epsilon",
            "loss",
        ]
    )

    global_step = 0
    best_return = -1e9

    try:
        for ep in range(1, args.episodes + 1):
            obs, _ = env.reset()

            done = False
            ep_return = 0.0
            last_loss = 0.0
            max_level = env.conn.state.level if env.conn else 1
            eps = args.eps_start
            info = {
                "steps": 0,
                "level": 1,
                "food": 0,
            }

            while not done:
                eps = linear_epsilon(
                    global_step,
                    args.eps_start,
                    args.eps_end,
                    args.eps_decay,
                )

                action = agent.act(obs, eps)
                obs2, reward, terminated, truncated, info = env.step(action)

                done = terminated or truncated

                agent.buffer.push(
                    obs,
                    action,
                    reward,
                    obs2,
                    1.0 if terminated else 0.0,
                )

                obs = obs2
                ep_return += reward
                max_level = max(max_level, info["level"])
                global_step += 1

                if global_step % args.train_every == 0:
                    loss = agent.learn()

                    if loss is not None:
                        last_loss = loss

            print(
                f"[ep {ep:4d}] return={ep_return:8.2f} "
                f"steps={info['steps']:3d} "
                f"max_lvl={max_level} "
                f"food={info['food']:3d} "
                f"eps={eps:.3f} "
                f"loss={last_loss:.4f}",
                file=sys.stderr,
            )

            writer.writerow(
                [
                    ep,
                    f"{ep_return:.3f}",
                    info["steps"],
                    max_level,
                    info["food"],
                    f"{eps:.4f}",
                    f"{last_loss:.5f}",
                ]
            )
            log_file.flush()

            if ep % args.save_every == 0 or ep_return > best_return:
                best_return = max(best_return, ep_return)

                ckpt_path = os.path.join(args.out_dir, "zappy_dqn.pt")

                agent.save(
                    ckpt_path,
                    meta={
                        "episode": ep,
                        "return": ep_return,
                        "target_level": args.target_level,
                        "frequency": args.frequency,
                    },
                )

    finally:
        env.close()
        log_file.close()

        if launcher is not None:
            launcher.stop()

    final_path = os.path.join(args.out_dir, "zappy_dqn.pt")
    agent.save(final_path, meta={"episodes": args.episodes})

    print(f"[train_rl] modèle final sauvé: {final_path}", file=sys.stderr)
    print(f"[train_rl] logs: {log_path}", file=sys.stderr)


def play(args: argparse.Namespace) -> None:
    launcher, host, port = setup_server(args)

    env = ZappyEnv(
        host=host,
        port=port,
        frequency=args.frequency,
        team=args.team,
        target_level=args.target_level,
        max_steps=args.max_steps,
    )

    agent = DQNAgent(
        obs_dim=OBS_DIM,
        n_actions=len(ACTIONS),
        device=args.device,
        hidden=args.hidden,
    )

    if not args.load:
        print("[play] --load <model.pt> requis", file=sys.stderr)

        if launcher is not None:
            launcher.stop()

        return

    agent.load(args.load)

    n = args.eval_episodes
    levels: List[int] = []

    try:
        for ep in range(1, n + 1):
            obs, _ = env.reset()

            done = False
            max_level = env.conn.state.level if env.conn else 1
            ret = 0.0
            info = {
                "steps": 0,
                "level": 1,
                "food": 0,
            }

            while not done:
                action = agent.act(obs, epsilon=0.0)
                obs, reward, terminated, truncated, info = env.step(action)

                done = terminated or truncated
                ret += reward
                max_level = max(max_level, info["level"])

            levels.append(max_level)

            print(
                f"[play ep {ep}] max_lvl={max_level} "
                f"return={ret:.2f} "
                f"steps={info['steps']}",
                file=sys.stderr,
            )

    finally:
        env.close()

        if launcher is not None:
            launcher.stop()

    if levels:
        reached = sum(1 for level in levels if level >= args.target_level)

        print(
            f"[play] niveau cible atteint {reached}/{n} épisodes "
            f"(moy. niveau {np.mean(levels):.2f})",
            file=sys.stderr,
        )


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Entraînement Double DQN pour Zappy contre le vrai serveur."
    )

    p.add_argument("--play", action="store_true")
    p.add_argument("--load", default=None)
    p.add_argument("--out-dir", default=os.path.join(_THIS_DIR, "runs"))

    p.add_argument("--host", default="localhost")
    p.add_argument("--port", type=int, default=4242)
    p.add_argument("--launch-server", default=None, metavar="BIN")

    p.add_argument("--width", type=int, default=10)
    p.add_argument("--height", type=int, default=10)
    p.add_argument("--clients", type=int, default=64)
    p.add_argument("--frequency", type=int, default=100)
    p.add_argument("--team", default="RL")
    p.add_argument("--target-level", type=int, default=2)
    p.add_argument("--max-steps", type=int, default=200)

    p.add_argument("--episodes", type=int, default=300)
    p.add_argument("--eval-episodes", type=int, default=10)
    p.add_argument("--gamma", type=float, default=0.99)
    p.add_argument("--lr", type=float, default=1e-3)
    p.add_argument("--buffer", type=int, default=50_000)
    p.add_argument("--batch", type=int, default=64)
    p.add_argument("--hidden", type=int, default=128)
    p.add_argument("--target-sync", type=int, default=500)
    p.add_argument("--train-every", type=int, default=1)
    p.add_argument("--save-every", type=int, default=25)
    p.add_argument("--eps-start", type=float, default=1.0)
    p.add_argument("--eps-end", type=float, default=0.05)
    p.add_argument("--eps-decay", type=int, default=10_000)
    p.add_argument("--seed", type=int, default=0)
    p.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")

    return p


def main() -> int:
    args = build_parser().parse_args()

    if args.play:
        play(args)
    else:
        train(args)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())