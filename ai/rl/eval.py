#!/usr/bin/env python3
"""
eval.py — Évaluation des performances de zappy_ai.

Pendant logique de train.py : train.py PRODUIT des parties (data), eval.py
les JUGE. Aucune notion de ML ici non plus — on agrège des métriques
mesurables sur l'IA FSM :

  * taux de victoire par équipe (via "seg") ;
  * niveau max atteint (distribution) ;
  * nombre de joueurs distincts ayant atteint chaque niveau ;
  * morts par partie ;
  * proportion de parties allant jusqu'à un vainqueur vs timeout ;
  * durée moyenne / médiane des parties.

La condition de victoire du sujet (>= 6 joueurs au niveau max 8) sert de
critère de réussite "objectif", en plus du seg renvoyé par le serveur.

Deux usages :
  1. eval.py results.json              -> lit un JSON déjà produit par train.py
  2. eval.py --run [options train]     -> lance d'abord un lot via train.run_batch
"""

import argparse
import json
import statistics
import sys
from typing import Dict, List, Optional


MAX_LEVEL = 8
WIN_PLAYERS = 6


# --------------------------------------------------------------------------- #
# Chargement des données
# --------------------------------------------------------------------------- #

def load_results(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as fh:
        return json.load(fh)


def run_then_load(args: argparse.Namespace) -> dict:
    """Réutilise le moteur de train.py pour générer un lot frais, sans
    dupliquer la logique d'orchestration."""
    try:
        import ai.rl.train as train
    except ImportError:
        print("[eval] train.py introuvable (doit être dans le même dossier)",
              file=sys.stderr)
        raise SystemExit(84)

    cfg = train.BatchConfig(
        server_bin=args.server,
        ai_script=args.ai,
        teams=args.teams,
        width=args.width,
        height=args.height,
        clients=args.clients,
        frequency=args.frequency,
        games=args.games,
        max_duration=args.max_duration,
        host=args.host,
        use_gui=not args.no_gui,
    )
    results = train.run_batch(cfg)
    return {"config": train.asdict(cfg), "results": results}


# --------------------------------------------------------------------------- #
# Calcul des métriques
# --------------------------------------------------------------------------- #

def all_teams(results: List[dict]) -> List[str]:
    names = []
    for game in results:
        for team in game.get("teams", {}):
            if team not in names:
                names.append(team)
    return names


def team_metrics(results: List[dict], team: str) -> dict:
    """Agrège les métriques d'une équipe sur l'ensemble des parties."""
    games_played = 0
    wins = 0
    objective_wins = 0          # >= 6 joueurs au niveau max (critère sujet)
    max_levels: List[int] = []
    deaths: List[int] = []
    players_seen: List[int] = []

    for game in results:
        stats = game.get("teams", {}).get(team)
        if stats is None:
            continue
        games_played += 1

        if game.get("winner") == team:
            wins += 1

        max_levels.append(stats.get("max_level", 1))
        deaths.append(stats.get("deaths", 0))
        players_seen.append(stats.get("players_seen", 0))

        # level_counts peut avoir des clés str (JSON) ou int.
        lvl_counts = stats.get("level_counts", {})
        at_max = int(lvl_counts.get(str(MAX_LEVEL), lvl_counts.get(MAX_LEVEL, 0)))
        if at_max >= WIN_PLAYERS:
            objective_wins += 1

    return {
        "games_played": games_played,
        "wins_seg": wins,
        "win_rate_seg": _ratio(wins, games_played),
        "objective_wins": objective_wins,
        "objective_win_rate": _ratio(objective_wins, games_played),
        "avg_max_level": _avg(max_levels),
        "best_max_level": max(max_levels) if max_levels else 0,
        "avg_deaths": _avg(deaths),
        "avg_players_seen": _avg(players_seen),
    }


def global_metrics(results: List[dict]) -> dict:
    durations = [g.get("duration_s", 0.0) for g in results]
    reasons: Dict[str, int] = {}
    for g in results:
        reasons[g.get("reason", "?")] = reasons.get(g.get("reason", "?"), 0) + 1
    return {
        "games": len(results),
        "avg_duration_s": _avg(durations),
        "median_duration_s": round(statistics.median(durations), 2) if durations else 0,
        "reasons": reasons,
        "decided_games": sum(1 for g in results if g.get("winner")),
    }


def _avg(xs: List[float]) -> float:
    return round(sum(xs) / len(xs), 2) if xs else 0.0


def _ratio(num: int, den: int) -> float:
    return round(num / den, 3) if den else 0.0


# --------------------------------------------------------------------------- #
# Rapport
# --------------------------------------------------------------------------- #

def print_report(payload: dict) -> dict:
    results = payload.get("results", [])
    if not results:
        print("[eval] aucun résultat à évaluer.", file=sys.stderr)
        return {}

    gm = global_metrics(results)
    report = {"global": gm, "teams": {}}

    print("=" * 56)
    print("  ÉVALUATION ZAPPY_AI  (FSM, pas de ML)")
    print("=" * 56)
    print(f"Parties jouées        : {gm['games']}")
    print(f"Parties décidées (seg): {gm['decided_games']}")
    print(f"Durée moyenne         : {gm['avg_duration_s']} s")
    print(f"Durée médiane         : {gm['median_duration_s']} s")
    print(f"Fins                  : {gm['reasons']}")
    print("-" * 56)

    for team in all_teams(results):
        m = team_metrics(results, team)
        report["teams"][team] = m
        print(f"[{team}]")
        print(f"  victoires (seg)        : {m['wins_seg']}/{m['games_played']} "
              f"(taux {m['win_rate_seg']})")
        print(f"  victoires objectif >=6@8: {m['objective_wins']}/{m['games_played']} "
              f"(taux {m['objective_win_rate']})")
        print(f"  niveau max moyen        : {m['avg_max_level']} "
              f"(meilleur {m['best_max_level']}/{MAX_LEVEL})")
        print(f"  morts / partie (moy)    : {m['avg_deaths']}")
        print(f"  joueurs vus / partie    : {m['avg_players_seen']}")
        print("-" * 56)

    return report


# --------------------------------------------------------------------------- #
# CLI
# --------------------------------------------------------------------------- #

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Évaluation des résultats de train.py (pas de ML)."
    )
    p.add_argument("results", nargs="?", default="results.json",
                   help="fichier JSON produit par train.py")
    p.add_argument("--out", default=None,
                   help="écrire le rapport agrégé dans ce JSON")

    # --run : générer un lot frais via train.py avant d'évaluer
    p.add_argument("--run", action="store_true",
                   help="lancer d'abord un lot via train.run_batch")
    p.add_argument("--server", default="./zappy_server")
    p.add_argument("--ai", default="./zappy_ai.py")
    p.add_argument("--teams", nargs="+", default=["team1"])
    p.add_argument("-x", "--width", type=int, default=10)
    p.add_argument("-y", "--height", type=int, default=10)
    p.add_argument("-c", "--clients", type=int, default=6)
    p.add_argument("-f", "--frequency", type=int, default=100)
    p.add_argument("--games", type=int, default=5)
    p.add_argument("--max-duration", type=float, default=120.0)
    p.add_argument("--host", default="localhost")
    p.add_argument("--no-gui", action="store_true")
    return p.parse_args()


def main() -> int:
    args = parse_args()

    if args.run:
        payload = run_then_load(args)
    else:
        try:
            payload = load_results(args.results)
        except FileNotFoundError:
            print(f"[eval] fichier introuvable: {args.results} "
                  f"(lance d'abord train.py ou utilise --run)", file=sys.stderr)
            return 84

    report = print_report(payload)

    if args.out and report:
        with open(args.out, "w", encoding="utf-8") as fh:
            json.dump(report, fh, indent=2)
        print(f"[eval] rapport écrit dans {args.out}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
