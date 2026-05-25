"""PPO training entry point (P5; ADR-004).

Runs against the stub env in S1 so the pipeline is exercised before the real
simulator exists. RLlib is imported lazily inside main() so the module imports
without the RL stack installed.

Usage:
    python -m zappy_train.training.train_ppo --debug --max-steps 1000
"""

from __future__ import annotations

import argparse

from zappy_train.env import ZappyParallelEnv


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Train a Zappy PPO policy")
    p.add_argument("--debug", action="store_true", help="tiny run, no W&B")
    p.add_argument("--max-steps", type=int, default=100_000)
    p.add_argument("--n-agents", type=int, default=4)
    p.add_argument("--seed", type=int, default=42)
    return p.parse_args(argv)


def _smoke_loop(args: argparse.Namespace) -> int:
    """Dependency-free rollout used by --debug and tests."""
    env = ZappyParallelEnv(n_agents=args.n_agents, max_steps=args.max_steps, seed=args.seed)
    obs = env.reset(seed=args.seed)
    steps = 0
    rng_actions = {a: int(env.num_actions) - 1 for a in env.agents}  # NOOP
    while steps < args.max_steps:
        result = env.step(rng_actions)
        steps += 1
        if all(result.truncations.values()):
            break
    print(f"smoke run OK: {steps} steps, obs_dim={env.obs_dim}, actions={env.num_actions}")
    return 0


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.debug:
        return _smoke_loop(args)

    # TODO(P5): real RLlib PPO. Imported lazily so --debug works without the stack.
    try:
        import ray  # noqa: F401
        from ray.rllib.algorithms.ppo import PPOConfig  # noqa: F401
    except ImportError:
        print("RLlib not installed. Run: pip install -e 'ai_python[rl]' (or use --debug)")
        return 1
    raise NotImplementedError("real PPO training lands in S2 (P5)")


if __name__ == "__main__":
    raise SystemExit(main())
