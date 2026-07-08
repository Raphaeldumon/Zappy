#!/usr/bin/env python3
"""Integration-test runner for Zappy (AI <-> server <-> GUI).

Launches zappy_server, connects a GUI capture client plus instrumented fake-AI
drones, drives them for the scenario duration, then checks the scenario's
expectations. Exit 0 if all PASS (SKIPs allowed), 1 otherwise.

Scenario schema: docs/04_quality/02_testing_strategy.md
Usage:
    tools/run_integration.py [scenario.yaml ...]
    tools/run_integration.py --server build/bin/zappy_server tests/integration_yaml/*.yaml

With no scenario args it runs every tests/integration_yaml/*.yaml.
"""

from __future__ import annotations

import argparse
import glob
import os
import random
import socket
import subprocess
import sys
import time

try:
    import yaml
except ImportError:
    print("run_integration: PyYAML required (pip install pyyaml)", file=sys.stderr)
    sys.exit(2)

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MOVES = ["Forward"] * 4 + ["Right", "Left", "Look", "Inventory"]
RESOURCES = ["food", "linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]

# GUI tags that signal a protocol error from the server's point of view.
GUI_ERROR_TAGS = {"suc", "sbp"}
# Known GUI tags (server -> GUI). Anything else from the server is "unknown".
GUI_KNOWN_TAGS = {
    "msz",
    "bct",
    "tna",
    "pnw",
    "ppo",
    "plv",
    "pin",
    "pex",
    "pbc",
    "pic",
    "pie",
    "pfk",
    "pdr",
    "pgt",
    "pdi",
    "enw",
    "ebo",
    "edi",
    "sgt",
    "sst",
    "wth",
    "seg",
    "smg",
    "suc",
    "sbp",
}


class AiBot:
    """One AI drone socket with command/ack accounting."""

    def __init__(self, host: str, port: int, team: str, idx: int):
        self.host, self.port, self.team, self.idx = host, port, team, idx
        self.sent = 0
        self.acked = 0
        self.connected = False
        self.error = ""
        self._sock: socket.socket | None = None
        self._rx = ""
        self._rng = random.Random(idx * 6151 + 3)

    def start(self) -> bool:
        try:
            self._sock = socket.create_connection((self.host, self.port), timeout=3)
        except OSError as exc:
            self.error = f"connect: {exc}"
            return False
        self._sock.sendall(self.team.encode() + b"\n")
        # Drain handshake (WELCOME, client-num, "X Y"); not command replies.
        time.sleep(0.2)
        self._sock.setblocking(False)
        try:
            self._sock.recv(4096)
        except (BlockingIOError, OSError):
            pass
        self.connected = True
        return True

    def _drain_acks(self) -> None:
        if self._sock is None:
            return
        try:
            while True:
                chunk = self._sock.recv(4096)
                if not chunk:
                    break
                self._rx += chunk.decode(errors="replace")
        except (BlockingIOError, OSError):
            pass
        # Count ok/ko lines as acks. Async server lines (message/dead/eject) are
        # not acks; ignore them for the ratio.
        lines = self._rx.split("\n")
        self._rx = lines.pop()  # keep partial
        for ln in lines:
            ln = ln.strip()
            if (
                ln in ("ok", "ko")
                or ln.startswith("[")
                or ln.startswith("Current level")
            ):
                self.acked += 1

    def step(self) -> None:
        if self._sock is None:
            return
        cmd = (
            f"Take {self._rng.choice(RESOURCES)}"
            if self._rng.random() < 0.2
            else self._rng.choice(MOVES)
        )
        try:
            self._sock.sendall(cmd.encode() + b"\n")
            self.sent += 1
        except OSError as exc:
            self.error = f"send: {exc}"
        self._drain_acks()

    def finish(self) -> None:
        # Final drain so late acks are counted before we score.
        time.sleep(0.2)
        self._drain_acks()
        if self._sock is not None:
            self._sock.close()


class GuiCapture:
    """GRAPHIC client that records the server's GUI stream."""

    def __init__(self, host: str, port: int):
        self.host, self.port = host, port
        self.tag_counts: dict[str, int] = {}
        self.unknown: list[str] = []
        self.connected = False
        self.error = ""
        self._sock: socket.socket | None = None
        self._rx = ""

    def start(self) -> bool:
        try:
            self._sock = socket.create_connection((self.host, self.port), timeout=3)
        except OSError as exc:
            self.error = f"connect: {exc}"
            return False
        self._sock.sendall(b"GRAPHIC\n")
        self._sock.setblocking(False)
        self.connected = True
        return True

    def pump(self) -> None:
        if self._sock is None:
            return
        try:
            while True:
                chunk = self._sock.recv(8192)
                if not chunk:
                    break
                self._rx += chunk.decode(errors="replace")
        except (BlockingIOError, OSError):
            pass
        lines = self._rx.split("\n")
        self._rx = lines.pop()
        for ln in lines:
            ln = ln.strip()
            if not ln or ln == "WELCOME":
                continue
            tag = ln.split(" ", 1)[0]
            self.tag_counts[tag] = self.tag_counts.get(tag, 0) + 1
            if tag not in GUI_KNOWN_TAGS:
                self.unknown.append(ln)

    def finish(self) -> None:
        time.sleep(0.2)
        self.pump()
        if self._sock is not None:
            self._sock.close()


def probe_admin(host: str, admin_port: int, token: str) -> tuple[bool, str]:
    """Exercise the bonus admin socket: auth + pause/resume/spawn round-trip.

    Returns (ok, detail). ok=False on any unexpected reply.
    """
    try:
        s = socket.create_connection((host, admin_port), timeout=2)
    except OSError as exc:
        return False, f"connect {host}:{admin_port}: {exc}"
    s.settimeout(0.5)

    def cmd(line: str) -> str:
        s.sendall(line.encode() + b"\n")
        time.sleep(0.1)
        try:
            return s.recv(256).decode(errors="replace").strip()
        except socket.timeout:
            return "<no reply>"

    time.sleep(0.15)
    try:
        s.recv(256)  # greeting banner
    except (socket.timeout, OSError):
        pass

    checks = [
        ("pause", lambda r: r.startswith("ko")),  # gated before auth
        (f"auth {token}", lambda r: r == "authenticated"),
        ("pause", lambda r: r == "paused"),
        ("resume", lambda r: r == "resumed"),
        ("spawn food 0 0 1", lambda r: r.startswith("spawned")),
    ]
    for line, want in checks:
        got = cmd(line)
        if not want(got):
            s.close()
            return False, f"{line!r} -> {got!r}"
    s.close()
    return True, "auth+pause+resume+spawn ok"


def run_scenario(path: str, server_bin: str, base_port: int) -> bool:
    with open(path) as fh:
        sc = yaml.safe_load(fh)

    name = sc.get("name", os.path.basename(path))
    srv = sc["server"]
    teams = srv["teams"]
    port = base_port
    duration = float(sc.get("max_duration_sec", 6))

    print(f"\n=== scenario: {name} ({os.path.basename(path)}) ===")
    print(
        f"    server :{port} map {srv['width']}x{srv['height']} teams={teams} f={srv['f']}"
    )

    admin_token = srv.get("admin_token", "")
    cmd = [
        server_bin,
        "-p",
        str(port),
        "-x",
        str(srv["width"]),
        "-y",
        str(srv["height"]),
        "-n",
        *teams,
        "-c",
        str(srv["clients_per_team"]),
        "-f",
        str(srv["f"]),
    ]
    if admin_token:
        cmd += ["--admin-token", admin_token]
    server = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    time.sleep(0.5)

    if server.poll() is not None:
        err = server.stderr.read().decode(errors="replace") if server.stderr else ""
        print(f"    FAIL: server exited early ({server.returncode})\n{err}")
        return False

    gui = GuiCapture("localhost", port)
    gui.start()
    time.sleep(0.2)

    bots: list[AiBot] = []
    idx = 0
    for spec in sc.get("ai", []):
        for _ in range(int(spec["count"])):
            b = AiBot("localhost", port, spec["team"], idx)
            if b.start():
                bots.append(b)
            idx += 1

    # Drive: tick bots + pump GUI until the duration elapses.
    deadline = time.time() + duration
    while time.time() < deadline:
        for b in bots:
            b.step()
        gui.pump()
        time.sleep(0.05)

    server_alive = server.poll() is None

    # Probe the bonus admin socket while the server is still up.
    admin_ok, admin_detail = (False, "no admin_token in scenario")
    if admin_token and server_alive:
        admin_ok, admin_detail = probe_admin("localhost", port + 1000, admin_token)

    for b in bots:
        b.finish()
    gui.finish()

    server.terminate()
    try:
        server.wait(timeout=3)
    except subprocess.TimeoutExpired:
        server.kill()

    # ---- evaluate expectations ----
    total_sent = sum(b.sent for b in bots)
    total_acked = sum(b.acked for b in bots)
    ack_ratio = (total_acked / total_sent) if total_sent else 0.0
    connected_bots = sum(1 for b in bots if b.connected)

    results: list[tuple[str, str, str]] = []  # (assertion, status, detail)

    def check(assertion: str) -> None:
        if assertion == "no_crashes":
            ok = server_alive
            results.append(
                (
                    assertion,
                    "PASS" if ok else "FAIL",
                    "server alive" if ok else "server died mid-run",
                )
            )
        elif assertion == "gui_handshake_ok":
            ok = (
                gui.connected
                and gui.tag_counts.get("msz", 0) >= 1
                and gui.tag_counts.get("bct", 0) >= 1
            )
            results.append(
                (
                    assertion,
                    "PASS" if ok else "FAIL",
                    f"msz={gui.tag_counts.get('msz', 0)} bct={gui.tag_counts.get('bct', 0)}",
                )
            )
        elif assertion == "all_actions_acknowledged_in_time":
            # Allow a small tail of in-flight commands at shutdown.
            ok = total_sent > 0 and ack_ratio >= 0.9
            results.append(
                (
                    assertion,
                    "PASS" if ok else "FAIL",
                    f"acked {total_acked}/{total_sent} ({ack_ratio:.0%})",
                )
            )
        elif assertion == "players_spawned":
            ok = gui.tag_counts.get("pnw", 0) >= connected_bots and connected_bots > 0
            results.append(
                (
                    assertion,
                    "PASS" if ok else "FAIL",
                    f"pnw={gui.tag_counts.get('pnw', 0)} bots={connected_bots}",
                )
            )
        elif assertion == "protocol_clean":
            errs = sum(gui.tag_counts.get(t, 0) for t in GUI_ERROR_TAGS)
            ok = errs == 0 and not gui.unknown
            detail = f"err_tags={errs} unknown={len(gui.unknown)}"
            if gui.unknown:
                detail += f" e.g. {gui.unknown[0]!r}"
            results.append((assertion, "PASS" if ok else "FAIL", detail))
        elif assertion == "admin_socket_ok":
            if not admin_token:
                results.append((assertion, "SKIP", "scenario has no admin_token"))
            else:
                results.append(
                    (assertion, "PASS" if admin_ok else "FAIL", admin_detail)
                )
        elif assertion == "respawn_diffs_only":
            # Respawn must push bct only for changed tiles, never the whole map.
            # Full-map spam would be ~W*H per 20-tick cycle (tens of thousands at
            # f=1000 over the run); the diff-only path stays in the low hundreds.
            # Cap generously at one full map's worth (snapshot) + 5x slack.
            bct = gui.tag_counts.get("bct", 0)
            cap = srv["width"] * srv["height"] * 5
            ok = bct <= cap
            results.append(
                (assertion, "PASS" if ok else "FAIL", f"bct={bct} cap={cap}")
            )
        elif assertion == "any_team_wins":
            won = gui.tag_counts.get("seg", 0) >= 1
            # Fake AI can't elevate to win; treat absence as SKIP, presence as PASS.
            results.append(
                (
                    assertion,
                    "PASS" if won else "SKIP",
                    "seg seen" if won else "needs real AI (fake AI can't win)",
                )
            )
        else:
            results.append((assertion, "SKIP", "unknown assertion in runner"))

    for a in sc.get("expectations", []):
        check(a)

    failed = False
    for assertion, status, detail in results:
        mark = {"PASS": "✓", "FAIL": "✗", "SKIP": "–"}[status]
        print(f"    [{mark}] {status:4} {assertion}  ({detail})")
        if status == "FAIL":
            failed = True

    print(
        f"    bots: {connected_bots}/{idx} connected, {total_sent} cmds, {ack_ratio:.0%} acked; "
        f"GUI tags: {sum(gui.tag_counts.values())}"
    )
    return not failed


def main() -> int:
    ap = argparse.ArgumentParser(description="Zappy integration test runner.")
    ap.add_argument(
        "scenarios",
        nargs="*",
        help="scenario YAML files (default: tests/integration_yaml/*.yaml)",
    )
    ap.add_argument(
        "--server",
        default=os.path.join(REPO_ROOT, "build", "bin", "zappy_server"),
        help="path to zappy_server binary",
    )
    ap.add_argument(
        "--base-port",
        type=int,
        default=4400,
        help="first port (incremented per scenario)",
    )
    args = ap.parse_args()

    if not os.path.isfile(args.server):
        print(
            f"run_integration: server binary not found: {args.server}\n"
            f"build it first: cmake --build build --target zappy_server",
            file=sys.stderr,
        )
        return 2

    scenarios = args.scenarios or sorted(
        glob.glob(os.path.join(REPO_ROOT, "tests", "integration_yaml", "*.yaml"))
    )
    if not scenarios:
        print("run_integration: no scenarios found", file=sys.stderr)
        return 2

    all_ok = True
    for i, path in enumerate(scenarios):
        ok = run_scenario(path, args.server, args.base_port + i)
        all_ok = all_ok and ok

    print(f"\n{'ALL PASS' if all_ok else 'FAILURES'} ({len(scenarios)} scenario(s))")
    return 0 if all_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
