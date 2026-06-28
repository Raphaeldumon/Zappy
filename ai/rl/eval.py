#!/usr/bin/env python3
"""
eval_rl.py — Évaluation d'un agent Zappy entraîné (Double DQN).

Évalue un checkpoint produit par train_rl.py en le faisant jouer, en GREEDY
(epsilon=0 par défaut), contre le VRAI serveur Zappy déjà lancé. Comme train_rl,
ce script NE recrée PAS de monde : il se CONNECTE à un serveur existant
(--host/--port) ; un épisode = une vie de drone. --launch-server reste une
commodité optionnelle qui démarre UNE instance du vrai binaire fourni.

Il réutilise l'environnement, l'agent et l'espace d'actions de train_rl
(ZappyEnv, DQNAgent, ACTIONS, OBS_DIM, setup_server) : l'évaluation passe donc
exactement par le même protocole et le même encodage d'observation que
l'entraînement (sinon les métriques seraient mensongères).

Métriques produites :
  * taux de réussite : épisodes ayant atteint --target-level ;
  * niveau atteint (moyenne / médiane / distribution) ;
  * taux de mort vs survie ;
  * pas par épisode (efficacité), nourriture finale ;
  * retour cumulé (récompense shaping) — indicatif ;
  * distribution des actions choisies (détection de comportements dégénérés).

Une baseline aléatoire optionnelle (--baseline) donne un point de comparaison :
un agent utile doit faire nettement mieux que des actions tirées au hasard.

Exemples
--------
  # serveur déjà lancé :
  #   ./zappy_server -p 4242 -x 10 -y 10 -n RL -c 64 -f 1000
  python3 eval_rl.py --load runs/zappy_dqn.pt --host localhost --port 4242 \
                     --frequency 1000 --episodes 30 --baseline

  # commodité : laisser eval_rl lancer le vrai binaire fourni
  python3 eval_rl.py --load runs/zappy_dqn.pt \
                     --launch-server ../../build/bin/zappy_server \
                     --port 4242 --frequency 1000 --episodes 30
"""

from __future__ import annotations

import argparse
import json
import os
import statistics
import sys
from collections import Counter
from dataclasses import dataclass, asdict, field
from typing import Dict, List, Optional

import numpy as np

# On réutilise tout le moteur de train_rl (même dossier).
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)

import train as T  # ZappyEnv, DQNAgent, ACTIONS, OBS_DIM, setup_server


# --------------------------------------------------------------------------- #
# Résultats.
# --------------------------------------------------------------------------- #

@dataclass
class EpisodeResult:
    index: int
    max_level: int
    reached_target: bool
    died: bool
    truncated: bool
    steps: int
    final_food: int
    ret: float
    terminal: str            # 'level' / 'dead' / 'truncated'
    action_counts: Dict[str, int] = field(default_factory=dict)


@dataclass
class Summary:
    policy: str
    episodes: int
    target_level: int
    success_rate: float          # fraction ayant atteint target_level
    death_rate: float
    mean_level: float
    median_level: float
    mean_steps: float
    mean_final_food: float
    mean_return: float
    level_distribution: Dict[int, int]
    action_distribution: Dict[str, int]


# --------------------------------------------------------------------------- #
# Boucle d'évaluation (générique : agent greedy OU politique aléatoire).
# --------------------------------------------------------------------------- #

def run_episode(env: T.ZappyEnv, choose_action, idx: int) -> EpisodeResult:
    obs, _ = env.reset()
    done = False
    max_level = env.conn.state.level if env.conn else 1
    ret = 0.0
    counts: Counter = Counter()
    info: Dict = {"level": max_level, "food": 0, "steps": 0, "outcome": "?"}
    terminated = truncated = False

    while not done:
        action = choose_action(obs)
        counts[T.ACTIONS[int(action)]] += 1
        obs, reward, terminated, truncated, info = env.step(action)
        done = terminated or truncated
        ret += reward
        max_level = max(max_level, info["level"])

    died = (info.get("outcome") == "dead")
    reached = max_level >= env.target_level
    if reached and not died:
        terminal = "level"
    elif died:
        terminal = "dead"
    else:
        terminal = "truncated"

    return EpisodeResult(
        index=idx,
        max_level=max_level,
        reached_target=reached,
        died=died,
        truncated=bool(truncated and not died),
        steps=info.get("steps", 0),
        final_food=info.get("food", 0),
        ret=round(ret, 3),
        terminal=terminal,
        action_counts=dict(counts),
    )


def summarize(policy: str, target_level: int,
              results: List[EpisodeResult]) -> Summary:
    n = len(results)
    levels = [r.max_level for r in results]
    level_dist: Dict[int, int] = {}
    for l in levels:
        level_dist[l] = level_dist.get(l, 0) + 1
    action_dist: Counter = Counter()
    for r in results:
        action_dist.update(r.action_counts)

    return Summary(
        policy=policy,
        episodes=n,
        target_level=target_level,
        success_rate=round(sum(r.reached_target for r in results) / n, 4),
        death_rate=round(sum(r.died for r in results) / n, 4),
        mean_level=round(statistics.mean(levels), 3),
        median_level=float(statistics.median(levels)),
        mean_steps=round(statistics.mean(r.steps for r in results), 2),
        mean_final_food=round(statistics.mean(r.final_food for r in results), 2),
        mean_return=round(statistics.mean(r.ret for r in results), 3),
        level_distribution=dict(sorted(level_dist.items())),
        action_distribution=dict(action_dist.most_common()),
    )


def print_summary(s: Summary) -> None:
    bar = "-" * 56
    print(bar, file=sys.stderr)
    print(f"  Politique         : {s.policy}", file=sys.stderr)
    print(f"  Épisodes          : {s.episodes}", file=sys.stderr)
    print(f"  Niveau cible      : {s.target_level}", file=sys.stderr)
    print(f"  Taux de réussite  : {s.success_rate:.1%} "
          f"(atteint le niveau cible)", file=sys.stderr)
    print(f"  Taux de mort      : {s.death_rate:.1%}", file=sys.stderr)
    print(f"  Niveau atteint    : moy {s.mean_level} | méd {s.median_level}",
          file=sys.stderr)
    print(f"  Pas / épisode     : {s.mean_steps}", file=sys.stderr)
    print(f"  Nourriture finale : {s.mean_final_food}", file=sys.stderr)
    print(f"  Retour moyen      : {s.mean_return}", file=sys.stderr)
    print(f"  Distribution niv. : {s.level_distribution}", file=sys.stderr)
    top = list(s.action_distribution.items())[:6]
    print(f"  Actions (top 6)   : {top}", file=sys.stderr)
    print(bar, file=sys.stderr)


# --------------------------------------------------------------------------- #
# Programme principal.
# --------------------------------------------------------------------------- #

def evaluate(args: argparse.Namespace) -> int:
    if not args.load:
        print("[eval_rl] --load <model.pt> requis", file=sys.stderr)
        return 84

    launcher, host, port = T.setup_server(args)
    report: Dict = {}
    try:
        env = T.ZappyEnv(
            host=host, port=port, frequency=args.frequency, team=args.team,
            target_level=args.target_level, max_steps=args.max_steps,
        )

        # --- agent entraîné ----------------------------------------------- #
        agent = T.DQNAgent(obs_dim=T.OBS_DIM, n_actions=len(T.ACTIONS),
                           device=args.device, hidden=args.hidden)
        ckpt = agent.load(args.load)
        if ckpt.get("obs_dim") not in (None, T.OBS_DIM) or \
           ckpt.get("n_actions") not in (None, len(T.ACTIONS)):
            print(
                f"[eval_rl] incompatibilité de checkpoint: "
                f"obs_dim={ckpt.get('obs_dim')} n_actions={ckpt.get('n_actions')} "
                f"(attendu {T.OBS_DIM}/{len(T.ACTIONS)})",
                file=sys.stderr,
            )
            return 84
        print(f"[eval_rl] modèle chargé: {args.load} "
              f"(meta={ckpt.get('meta', {})})", file=sys.stderr)

        def greedy(obs):
            return agent.act(obs, epsilon=args.epsilon)

        agent_results: List[EpisodeResult] = []
        for i in range(1, args.episodes + 1):
            r = run_episode(env, greedy, i)
            agent_results.append(r)
            print(f"[eval_rl ep {i:3d}] niv={r.max_level} cible={'oui' if r.reached_target else 'non':3} "
                  f"fin={r.terminal:9} pas={r.steps:3d} food={r.final_food:3d} "
                  f"ret={r.ret:7.2f}", file=sys.stderr)

        agent_summary = summarize("dqn_greedy", args.target_level, agent_results)
        print_summary(agent_summary)
        report["agent"] = {
            "summary": asdict(agent_summary),
            "episodes": [asdict(r) for r in agent_results],
            "checkpoint": args.load,
            "checkpoint_meta": ckpt.get("meta", {}),
        }

        # --- baseline aléatoire (optionnelle) ----------------------------- #
        if args.baseline:
            rng = np.random.default_rng(args.seed)

            def random_policy(obs):
                return int(rng.integers(0, len(T.ACTIONS)))

            base_results: List[EpisodeResult] = []
            for i in range(1, args.episodes + 1):
                base_results.append(run_episode(env, random_policy, i))
            base_summary = summarize("random", args.target_level, base_results)
            print_summary(base_summary)
            report["baseline_random"] = {
                "summary": asdict(base_summary),
                "episodes": [asdict(r) for r in base_results],
            }
            # verdict simple
            delta = agent_summary.success_rate - base_summary.success_rate
            verdict = "MIEUX que l'aléatoire" if delta > 0 else \
                      ("ÉGAL à l'aléatoire" if delta == 0 else "PIRE que l'aléatoire")
            print(f"[eval_rl] verdict: l'agent fait {verdict} "
                  f"(Δ réussite = {delta:+.1%})", file=sys.stderr)
            report["verdict"] = {"success_rate_delta": round(delta, 4),
                                 "label": verdict}
    finally:
        try:
            env.close()
        except Exception:
            pass
        if launcher is not None:
            launcher.stop()

    if args.out:
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        with open(args.out, "w") as fh:
            json.dump(report, fh, indent=2, ensure_ascii=False)
        print(f"[eval_rl] rapport JSON écrit: {args.out}", file=sys.stderr)

    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Évaluation d'un agent Zappy DQN contre le VRAI serveur."
    )
    p.add_argument("--load", required=True, help="checkpoint .pt à évaluer")
    p.add_argument("--episodes", type=int, default=30,
                   help="nombre d'épisodes d'évaluation")
    p.add_argument("--epsilon", type=float, default=0.0,
                   help="exploration pendant l'éval (0 = greedy)")
    p.add_argument("--baseline", action="store_true",
                   help="ajouter une baseline aléatoire pour comparer")
    p.add_argument("--out", default=os.path.join(_THIS_DIR, "runs", "eval_report.json"),
                   help="fichier JSON de sortie (vide pour ne rien écrire)")

    # serveur : par défaut on se connecte à un serveur DÉJÀ lancé.
    p.add_argument("--host", default="localhost")
    p.add_argument("--port", type=int, default=4242)
    p.add_argument("--launch-server", default=None, metavar="BIN",
                   help="OPTIONNEL: lance UNE instance du VRAI zappy_server fourni")
    p.add_argument("--width", type=int, default=10, help="(si --launch-server) -x")
    p.add_argument("--height", type=int, default=10, help="(si --launch-server) -y")
    p.add_argument("--clients", type=int, default=64,
                   help="(si --launch-server) -c : assez de slots pour reconnecter")
    p.add_argument("--frequency", type=int, default=100,
                   help="-f du serveur (timeouts) ; mets-le haut pour aller vite")
    p.add_argument("--team", default="RL")
    p.add_argument("--target-level", type=int, default=2)
    p.add_argument("--max-steps", type=int, default=200)
    p.add_argument("--hidden", type=int, default=128,
                   help="taille cachée (doit matcher l'entraînement)")
    p.add_argument("--seed", type=int, default=0)
    import torch
    p.add_argument("--device",
                   default="cuda" if torch.cuda.is_available() else "cpu")
    return p


def main() -> int:
    args = build_parser().parse_args()
    if not args.out:
        args.out = None
    return evaluate(args)


if __name__ == "__main__":
    raise SystemExit(main())
