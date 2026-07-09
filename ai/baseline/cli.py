"""Command-line entry point: arg parsing + main()."""

import argparse
import sys

from .ai import AI


def _usage() -> str:
    return (
        f"UTILISATION : {sys.argv[0]} -p port -n nom -h machine [-f freq] [-v]\n"
        "  -p port     numéro de port du serveur (obligatoire)\n"
        "  -n nom      nom de l'équipe (obligatoire)\n"
        "  -h machine  adresse du serveur (par défaut : localhost)\n"
        "  -f freq     fréquence : unités de temps par seconde (strictement positive)\n"
        "  -v          journalise chaque commande/réponse (lent ; désactivé par défaut)\n"
    )


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

    if args.help:
        print(_usage())
        raise SystemExit(0)

    errors = []
    if unknown:
        errors.append("arguments inconnus : " + " ".join(unknown))
    missing = [flag for flag, value in (("-p", args.port), ("-n", args.name)) if value is None]
    if missing:
        errors.append("argument(s) requis manquant(s) : " + " ".join(missing))
    if args.port is not None and (args.port <= 0 or args.port > 65535):
        errors.append("port invalide : doit être entre 1 et 65535")
    if args.frequency is not None and args.frequency <= 0:
        errors.append("fréquence invalide : doit être strictement positive")

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        print(_usage(), file=sys.stderr, end="")
        raise SystemExit(84)

    return args


def main() -> int:
    try:
        args = parse_args()
        return AI(args.host, args.port, args.name, args.frequency, args.verbose).run()
    except KeyboardInterrupt:
        print("[IA] Interrompu", file=sys.stderr)
        return 130
    except Exception as err:
        print(f"[IA] Erreur : {err}", file=sys.stderr)
        return 84


if __name__ == "__main__":
    raise SystemExit(main())
