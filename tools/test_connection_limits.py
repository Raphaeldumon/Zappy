#!/usr/bin/env python3
"""Connection-limit / stress test for zappy_server.

Real-socket tests that the unit suite cannot reach: they spin up an actual
server and open many concurrent TCP connections to verify the server enforces
its capacity limits *and stays alive* past them.

Two tests:

  1. team_capacity   -- fill one team to its -c slot count, then keep going.
                        The slots'th client must be accepted; every client past
                        it must be refused (team full -> socket closed after the
                        WELCOME, no "X Y" dimensions line).

  2. absolute_limit  -- open MAX_TOTAL_CLIENTS raw connections + several beyond.
                        The ceiling'th socket must still get WELCOME; everything
                        past it is refused at accept time (closed before WELCOME).
                        The server must not crash.

Exit 0 if both PASS, 1 otherwise.

Usage:
    tools/test_connection_limits.py [--server build/bin/zappy_server] [--port 4500]

These limits come from server/runtime/limits.hpp:
    MAX_CLIENTS_PER_TEAM = 100
    MAX_TEAMS            = 8
    MAX_GUI_CLIENTS      = 6
    MAX_TOTAL_CLIENTS    = 100*8 + 6 = 806
Keep them in sync if limits.hpp changes.
"""

from __future__ import annotations

import argparse
import os
import re
import resource
import socket
import subprocess
import sys
import time

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Mirror of server/runtime/limits.hpp.
MAX_CLIENTS_PER_TEAM = 100
MAX_TEAMS = 8
MAX_GUI_CLIENTS = 6
MAX_TOTAL_CLIENTS = MAX_CLIENTS_PER_TEAM * MAX_TEAMS + MAX_GUI_CLIENTS  # 806

DIMS_RE = re.compile(rb"^\d+ \d+$")  # the "X Y" handshake line (accepted AI)

# Connection outcomes.
ACCEPTED = "accepted"  # got WELCOME and the "X Y" dimensions line
REFUSED_TEAM_FULL = "team_full"  # got WELCOME, then closed before dimensions
REFUSED_NO_WELCOME = "no_welcome"  # closed at accept, never sent WELCOME
ERROR = "error"  # connect failed / unexpected


def raise_fd_limit(target: int) -> None:
    """Make sure this process can hold `target` simultaneous sockets."""
    soft, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
    want = target + 64  # headroom for stdio, the listen probe, etc.
    if soft < want:
        new = min(max(want, soft), hard if hard != resource.RLIM_INFINITY else want)
        resource.setrlimit(resource.RLIMIT_NOFILE, (new, hard))


def start_server(
    server_bin: str,
    port: int,
    teams: list[str],
    clients_per_team: int,
    freq: int = 1000,
):
    cmd = [
        server_bin,
        "-p",
        str(port),
        "-x",
        "10",
        "-y",
        "10",
        "-n",
        *teams,
        "-c",
        str(clients_per_team),
        "-f",
        str(freq),
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    time.sleep(0.5)
    if proc.poll() is not None:
        err = proc.stderr.read().decode(errors="replace") if proc.stderr else ""
        raise RuntimeError(f"server exited early ({proc.returncode}): {err}")
    return proc


def stop_server(proc) -> None:
    proc.terminate()
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        proc.kill()


def open_conn(port: int) -> socket.socket | None:
    try:
        s = socket.create_connection(("127.0.0.1", port), timeout=3)
        s.setblocking(True)
        s.settimeout(2.0)
        return s
    except OSError:
        return None


def classify_ai(sock: socket.socket, team: str) -> str:
    """Send a team name, read the reply, decide accepted vs which refusal.

    Accepted AI stream: "WELCOME\\n" then "<free_slots>\\n" then "X Y\\n".
    Team-full: "WELCOME\\n" then EOF.  fd-ceiling: EOF with no WELCOME.
    """
    buf = b""
    welcome = False
    try:
        # Drain whatever is queued (WELCOME arrives on connect).
        try:
            buf += sock.recv(4096)
        except socket.timeout:
            pass
        welcome = b"WELCOME" in buf

        sock.sendall(team.encode() + b"\n")

        deadline = time.time() + 2.0
        while time.time() < deadline:
            try:
                chunk = sock.recv(4096)
            except socket.timeout:
                break
            if not chunk:  # EOF: server closed the socket
                break
            buf += chunk
            welcome = welcome or b"WELCOME" in buf
            if any(DIMS_RE.match(ln) for ln in buf.split(b"\n")):
                return ACCEPTED
    except OSError:
        pass

    if any(DIMS_RE.match(ln) for ln in buf.split(b"\n")):
        return ACCEPTED
    if welcome:
        return REFUSED_TEAM_FULL
    return REFUSED_NO_WELCOME


def got_welcome(sock: socket.socket) -> bool:
    try:
        data = sock.recv(4096)
    except (socket.timeout, OSError):
        return False
    return b"WELCOME" in data


# ---------------------------------------------------------------------------
# Test 1: a single team filled past its -c capacity
# ---------------------------------------------------------------------------
def test_team_capacity(server_bin: str, port: int) -> bool:
    slots = MAX_CLIENTS_PER_TEAM  # -c 100
    overflow = 8
    attempts = slots + overflow
    print(f"\n=== test: team_capacity (-c {slots}, {attempts} clients) ===")

    # f=1: a drone's life is STARTING_FOOD*LIFE_UNITS_PER_FOOD / f = 1260s, so no
    # drone starves during the test. Otherwise dying drones free slots mid-run and
    # late joiners get admitted past the cap (correct server behaviour, but it
    # masks the cap we're trying to assert).
    proc = start_server(server_bin, port, ["overflowers"], slots, freq=1)
    socks: list[socket.socket] = []
    outcomes: dict[str, int] = {}
    try:
        for i in range(attempts):
            s = open_conn(port)
            if s is None:
                outcomes[ERROR] = outcomes.get(ERROR, 0) + 1
                continue
            socks.append(s)  # hold the fd: closing it frees a team slot
            res = classify_ai(s, "overflowers")
            outcomes[res] = outcomes.get(res, 0) + 1

        alive = proc.poll() is None
    finally:
        for s in socks:
            s.close()
        stop_server(proc)

    accepted = outcomes.get(ACCEPTED, 0)
    team_full = outcomes.get(REFUSED_TEAM_FULL, 0)

    print(f"    accepted={accepted} team_full={team_full} other={outcomes}")
    ok = (
        alive
        and accepted == slots
        and team_full == overflow
        and outcomes.get(ERROR, 0) == 0
        and outcomes.get(REFUSED_NO_WELCOME, 0) == 0
    )
    detail = []
    if not alive:
        detail.append("server died")
    if accepted != slots:
        detail.append(f"expected {slots} accepted, got {accepted}")
    if team_full != overflow:
        detail.append(f"expected {overflow} team-full refusals, got {team_full}")
    print(f"    [{'PASS' if ok else 'FAIL'}] {'; '.join(detail) or 'limits enforced'}")
    return ok


# ---------------------------------------------------------------------------
# Test 2: the absolute connection ceiling and beyond
# ---------------------------------------------------------------------------
def test_absolute_limit(server_bin: str, port: int) -> bool:
    teams = [f"t{i}" for i in range(MAX_TEAMS)]
    beyond = 12
    attempts = MAX_TOTAL_CLIENTS + beyond  # 806 + 12 = 818 raw sockets
    print(
        f"\n=== test: absolute_limit ({attempts} raw conns, ceiling {MAX_TOTAL_CLIENTS}) ==="
    )

    raise_fd_limit(attempts)
    proc = start_server(server_bin, port, teams, MAX_CLIENTS_PER_TEAM)
    socks: list[socket.socket] = []
    errors = 0
    try:
        # Open every socket first (fills the listen backlog), then let the
        # single-accept-per-poll server drain it.
        for _ in range(attempts):
            s = open_conn(port)
            if s is None:
                errors += 1
                continue
            socks.append(s)
        time.sleep(2.0)  # give the server time to accept/reject the whole storm

        welcomed = sum(1 for s in socks if got_welcome(s))
        refused = len(socks) - welcomed
        alive = proc.poll() is None
    finally:
        for s in socks:
            s.close()
        stop_server(proc)

    print(
        f"    opened={len(socks)} welcomed={welcomed} refused={refused} errors={errors}"
    )
    ok = alive and errors == 0 and welcomed == MAX_TOTAL_CLIENTS and refused == beyond
    detail = []
    if not alive:
        detail.append("server died under load")
    if welcomed != MAX_TOTAL_CLIENTS:
        detail.append(f"expected {MAX_TOTAL_CLIENTS} welcomed, got {welcomed}")
    if refused != beyond:
        detail.append(f"expected {beyond} refused, got {refused}")
    if errors:
        detail.append(f"{errors} connect errors")
    print(
        f"    [{'PASS' if ok else 'FAIL'}] {'; '.join(detail) or 'ceiling enforced, server alive'}"
    )
    return ok


def main() -> int:
    ap = argparse.ArgumentParser(description="Zappy connection-limit stress test.")
    ap.add_argument(
        "--server",
        default=os.path.join(REPO_ROOT, "build", "bin", "zappy_server"),
        help="path to zappy_server binary",
    )
    ap.add_argument(
        "--port", type=int, default=4500, help="base port (test 2 uses port+1)"
    )
    args = ap.parse_args()

    if not os.path.isfile(args.server):
        print(
            f"test_connection_limits: server binary not found: {args.server}\n"
            f"build it first: cmake --build build --target zappy_server",
            file=sys.stderr,
        )
        return 2

    t1 = test_team_capacity(args.server, args.port)
    t2 = test_absolute_limit(args.server, args.port + 1)

    all_ok = t1 and t2
    print(f"\n{'ALL PASS' if all_ok else 'FAILURES'} (2 tests)")
    return 0 if all_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
