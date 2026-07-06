"""NetMixin: net responsibilities of the AI.

Auto-split from the original monolith; bodies are byte-identical.
The build bundler (_bundle.py) re-stitches these into a single zappy_ai."""

import select
import socket
import sys
import time
from typing import List, Optional


class NetMixin:
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
        if len(self.pending) >= self.max_pending:
            return False
        qdepth = len(self.pending)
        self.send_raw(cmd)
        self.pending.append((cmd, time.time(), qdepth))
        self.log(f"[AI -> SERVER] {cmd}")
        return True

    def log(self, msg: str) -> None:
        # Hot-path logging, gated: silent unless -v. Lifecycle markers
        # (level-ups, death, fork) use print() directly so they always show.
        if self.verbose:
            print(msg, file=sys.stderr)

    def has_pending(self) -> bool:
        return len(self.pending) > 0

    def front_pending(self) -> Optional[str]:
        # The command the next server reply will answer (oldest unanswered).
        return self.pending[0][0] if self.pending else None

    def pop_pending(self) -> Optional[str]:
        # Consume the oldest outstanding command (a reply just arrived for it).
        if not self.pending:
            return None
        cmd, sent_at, qdepth = self.pending.pop(0)
        self.observe_frequency(cmd, sent_at, qdepth)
        return cmd

    def observe_frequency(self, cmd: str, sent_at: float, qdepth: int) -> None:
        # Refine self.frequency from this reply's real latency. Free: no extra
        # commands, no movement -- piggybacks on traffic we already generate.
        if self.frequency_override is not None:
            return
        # After a wedge-drop the FIFO reply<->command mapping is broken: the
        # server's late reply now lands on the wrong pending entry, so its
        # measured latency is garbage (often near zero -> a huge bogus sample
        # that would yank the estimate up and trigger MORE false timeouts).
        # Refuse to sample until the pipeline drains and resyncs.
        if self.pipeline_desynced:
            return
        # Behind a queue -> latency includes earlier commands' execution, so the
        # sample is inflated. Skip; plenty of clean (qdepth==0) samples arrive.
        if qdepth != 0:
            return
        cost = self.cmd_tick_cost.get(cmd, 0)
        if cost <= 0:  # instant or unknown -> no signal
            return
        elapsed = time.time() - sent_at
        if elapsed <= 0:
            return
        sample = cost / elapsed  # ticks per second
        # Discard (don't smooth) implausible samples instead of clamping them in:
        # a clamped extreme still poisons the EMA. The server caps well under 1e4.
        if not (1.0 <= sample <= 10000.0):
            return
        if self.freq_ema is None:
            self.freq_ema = sample
        # A sustained >3x gap means the operator retuned the time unit; crawling
        # there via EMA strands the bot on stale timeouts for dozens of samples.
        # Snap straight to the new sample instead.
        elif sample > 3 * self.freq_ema or sample * 3 < self.freq_ema:
            self.freq_ema = sample
        else:
            self.freq_ema = 0.3 * sample + 0.7 * self.freq_ema
        new_freq = max(1, round(self.freq_ema))
        if new_freq != self.frequency:
            self.frequency = new_freq
            self.max_plan_length = 2 if self.frequency >= 100 else 3

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

        self.log(
            f"[AI] Connected team={self.team} bot_id={self.bot_id} "
            f"slots={self.memory.free_slots} map={self.memory.width}x{self.memory.height} "
            f"freq={self.frequency}",
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
            dt = round_trip("Connect_nbr", lambda lamb: lamb.lstrip("-").isdigit())
            if dt is None:
                return 100
            connect_times.append(dt)

        for _ in range(4):
            dt = round_trip("Forward", lambda lamb: lamb == "ok")
            if dt is None:
                return 100
            forward_times.append(dt)

        delta = min(forward_times) - min(connect_times)
        if delta <= 0:
            delta = min(forward_times)

        return max(1, round(7 / delta))
