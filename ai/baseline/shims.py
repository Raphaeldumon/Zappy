"""Compatibility shims for train_ai/train_rl: NetworkClient, Parser.

Unchanged from the original; not used by the live bot."""

import select
import socket
import time
from typing import Dict, List, Optional, Tuple

from .constants import RESOURCES


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
