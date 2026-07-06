"""Command-line entry point: arg parsing + main()."""

import argparse
import sys

from .ai import AI


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("-p", dest="port", type=int)
    parser.add_argument("-n", dest="name")
    parser.add_argument("-h", dest="host", default="localhost")
    parser.add_argument("-f", dest="frequency", type=int, default=None)
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="log every command/response (slow; off by default)",
    )
    parser.add_argument("--help", action="store_true")

    args, unknown = parser.parse_known_args()

    if args.help or unknown or args.port is None or args.name is None:
        print(
            f"USAGE: {sys.argv[0]} -p port -n name -h machine [-f freq]",
            file=sys.stderr,
        )
        raise SystemExit(0 if args.help else 84)

    if (
        args.port <= 0
        or args.port > 65535
        or (args.frequency is not None and args.frequency <= 0)
    ):
        print(
            f"USAGE: {sys.argv[0]} -p port -n name -h machine [-f freq]",
            file=sys.stderr,
        )
        raise SystemExit(84)

    return args


def main() -> int:
    try:
        args = parse_args()
        return AI(args.host, args.port, args.name, args.frequency, args.verbose).run()
    except KeyboardInterrupt:
        print("[AI] Interrupted", file=sys.stderr)
        return 130
    except Exception as err:
        print(f"[AI] Error: {err}", file=sys.stderr)
        return 84


if __name__ == "__main__":
    raise SystemExit(main())
