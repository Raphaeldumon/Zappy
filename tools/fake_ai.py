#!/usr/bin/env python3
"""Throwaway fake AI client — drives world state so the debug GUI shows motion.

Not a real agent. Connects as a normal AI (team member), then random-walks and
grabs resources so the server emits ppo / pgt / bct events. Use it next to
zappy_gui2d to eyeball that the server + GUI pipeline reacts to player actions.

Usage:
    tools/fake_ai.py -p 4242 -h localhost -n red [--bots 3] [--delay 0.4]

Each bot is one socket = one drone. The server caps drones at clients_per_team
(-c), so asking for more bots than free slots just connects fewer.
"""

from __future__ import annotations

import argparse
import random
import socket
import sys
import threading
import time

# Commands that move/turn or change tiles -> visible GUI events. Weighted toward
# Forward so drones actually roam instead of spinning in place.
MOVES = ["Forward"] * 5 + ["Right", "Left", "Look", "Inventory"]
RESOURCES = ["food", "linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]


def run_bot(host: str, port: int, team: str, delay: float, stop: threading.Event, idx: int) -> None:
    try:
        sock = socket.create_connection((host, port), timeout=3)
    except OSError as exc:
        print(f"[bot {idx}] connect failed: {exc}", file=sys.stderr)
        return

    sock.sendall(team.encode() + b"\n")
    sock.settimeout(0.2)

    # Drain the handshake (WELCOME, client-num, "X Y"). We don't parse it.
    time.sleep(0.2)
    try:
        sock.recv(4096)
    except OSError:
        pass

    print(f"[bot {idx}] joined team {team!r}")
    rng = random.Random(idx * 7919 + 1)

    while not stop.is_set():
        if rng.random() < 0.25:
            cmd = f"Take {rng.choice(RESOURCES)}"
        else:
            cmd = rng.choice(MOVES)
        try:
            sock.sendall(cmd.encode() + b"\n")
            sock.recv(4096)  # consume reply / async lines, ignore content
        except socket.timeout:
            pass
        except OSError as exc:
            print(f"[bot {idx}] disconnected: {exc}", file=sys.stderr)
            return
        stop.wait(delay)

    sock.close()


def main() -> int:
    # add_help=False so we can use -h for host (matches zappy_server / zappy_gui2d).
    ap = argparse.ArgumentParser(description="Fake AI driver for the Zappy debug GUI.", add_help=False)
    ap.add_argument("--help", action="help", help="show this help and exit")
    ap.add_argument("-p", "--port", type=int, default=4242)
    ap.add_argument("-h", "--host", default="localhost")
    ap.add_argument("-n", "--name", default="red", help="team name to join")
    ap.add_argument("--bots", type=int, default=1, help="number of drones to spawn")
    ap.add_argument("--delay", type=float, default=0.4, help="seconds between commands per bot")
    args = ap.parse_args()

    stop = threading.Event()
    threads = [
        threading.Thread(target=run_bot, args=(args.host, args.port, args.name, args.delay, stop, i), daemon=True)
        for i in range(args.bots)
    ]
    for t in threads:
        t.start()

    print(f"running {args.bots} bot(s) on {args.host}:{args.port} team={args.name!r} — Ctrl-C to stop")
    try:
        while any(t.is_alive() for t in threads):
            time.sleep(0.3)
    except KeyboardInterrupt:
        print("\nstopping…")
        stop.set()
        for t in threads:
            t.join(timeout=1.0)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
