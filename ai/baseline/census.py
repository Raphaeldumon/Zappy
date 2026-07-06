"""CensusMixin: census responsibilities of the AI.

Auto-split from the original monolith; bodies are byte-identical.
The build bundler (_bundle.py) re-stitches these into a single zappy_ai."""

import time
from .constants import REQ


class CensusMixin:
    def hello_interval(self) -> float:
        return max(1.0, 60.0 / self.frequency)

    def member_ttl(self) -> float:
        # Silence longer than this means the member is dead (death sends no
        # broadcast, so absence of heartbeats is the only signal). Must exceed
        # the longest legitimate silence: an Incantation freezes a bot for
        # 300/f s, and the quorum-met heartbeat runs at 2x hello_interval.
        return max(6.0 * self.hello_interval(), 320.0 / self.frequency, 10.0)

    def census_window(self) -> float:
        # Window scaled to game speed: each fork needs a 42/f-t.u. hatch plus
        # client boot plus food farming, so a fixed wall-clock 30s starves the
        # census at low f. 3000/f == the env default 30s at f=100.
        return max(self.census_timeout, 3000.0 / self.frequency)

    def broadcast_hello(self) -> None:
        self.last_hello_at = time.time()
        self.send_cmd(f"Broadcast {self.team}:HELLO:from={self.bot_id}")

    def handle_hello(self, sender: int) -> None:
        known = sender in self.team_members
        self.team_members.add(sender)
        self.team_last_seen[sender] = time.time()
        if known:
            return  # heartbeat from a known member: last_seen refreshed above
        # A new body showed up -> the fork we were waiting on (or a sibling)
        # landed; release the in-flight hold so the forker can continue.
        self.census_fork_deadline = 0.0
        self.log(
            f"[AI] census +{sender} -> {len(self.team_members)}/{self.census_target}"
        )
        # Echo once so the newcomer learns about us too. Bounded: only on the
        # first sighting of a given id, never on repeat HELLOs.
        self.broadcast_hello()

    def evict_silent_members(self) -> None:
        # Prune presumed-dead members. This is what re-breaks the quorum after
        # a death (so the census re-arms) and re-elects the forker when the
        # corpse held the lowest id.
        cutoff = time.time() - self.member_ttl()
        for mid in [m for m in self.team_members if m != self.bot_id]:
            if self.team_last_seen.get(mid, 0.0) < cutoff:
                self.team_members.discard(mid)
                self.team_last_seen.pop(mid, None)
                self.log(
                    f"[AI] census -{mid} (silent) -> "
                    f"{len(self.team_members)}/{self.census_target}"
                )

    def census_active(self) -> bool:
        if self.manifest_gave_up or self.census_deadline is None:
            return False
        if len(self.team_members) >= self.census_target:
            return False
        return time.time() < self.census_deadline

    def is_census_forker(self) -> bool:
        # Lowest known id is elected. Recomputed each call, so a lower id that
        # appears late takes over automatically.
        return self.bot_id == min(self.team_members)

    def census_maintain(self) -> None:
        now = time.time()
        if self.census_deadline is None:  # first tick: start clocks
            self.census_deadline = now + self.census_window()
            self.census_settle_until = now + self.census_settle
            self.broadcast_hello()
            return
        if self.manifest_gave_up:
            return  # fallback play: no quorum bookkeeping

        self.evict_silent_members()

        # Heartbeat forever, not just during the opening window: continuous
        # liveness is what lets survivors detect a death and re-fork back to
        # quorum. Halved rate once the quorum is met (pure liveness traffic).
        interval = self.hello_interval()
        if len(self.team_members) >= self.census_target:
            interval *= 2.0
        if now - self.last_hello_at >= interval:
            self.broadcast_hello()

        # Quorum broken with the window closed (a member died, or the opening
        # window was too short to finish forking): re-open it so the elected
        # forker repopulates. Short settle: one heartbeat round, enough to
        # hear a member the eviction wrongly presumed dead.
        if (
            len(self.team_members) < self.census_target
            and now >= self.census_deadline
        ):
            self.census_deadline = now + self.census_window()
            self.census_settle_until = now + self.hello_interval()
            self.log(
                f"[AI] census re-armed: {len(self.team_members)}/"
                f"{self.census_target}"
            )

    def should_fork(self) -> bool:
        if (
            self.active_req
            or self.is_following()
            or self.preparing_incantation
            or self.waiting_incantation
        ):
            return False

        if self.food() < self.fork_food:
            return False

        if time.time() - self.memory.last_fork_at < max(3.0, 150.0 / self.frequency):
            return False

        # Fallback (no quorum ever formed): original per-bot cap behaviour.
        # Only low-level bots reproduce here — the cap exists to limit mouths
        # on a scarce map, not to restore a quorum, and a level-3+ bot is too
        # valuable and food-bound to pay ~6 food for a level-1 baby.
        if self.manifest_gave_up:
            if self.memory.level > 2:
                return False
            return self.memory.forks_done < self.max_forks

        # Census mode: the team is sized to the blitz quorum, not a per-bot cap.
        # No level gate here: after deaths the survivors may all be level 3+,
        # and the elected forker forking is the only path back to quorum —
        # gating it stalls the blitz forever at 5 bots.
        if len(self.team_members) >= self.census_target:
            return False  # quorum met -> nobody forks
        if not self.census_active():
            return False  # window closed; census_maintain re-arms it next tick
        if self.census_settle_until and time.time() < self.census_settle_until:
            return False  # let initial HELLOs land first
        if time.time() < self.census_fork_deadline:
            return False  # a forked child hasn't announced yet
        return self.is_census_forker()

    def need_more_mates(self) -> bool:
        return (
            REQ.get(self.memory.level, {}).get("players", 1) > 1
            and self.has_required_stones()
        )
