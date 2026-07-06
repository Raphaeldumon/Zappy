"""The AI class: assembles all mixins and holds construction (__init__)."""

import os
import random
import socket
import time
from typing import Dict, List, Optional, Tuple

from .models import Memory, State
from .net import NetMixin
from .protocol import ProtocolMixin
from .sensing import SensingMixin
from .movement import MovementMixin
from .behaviors import BehaviorMixin
from .gather import GatherMixin
from .manifest import ManifestMixin
from .census import CensusMixin
from .brain import BrainMixin


class AI(
    NetMixin,
    ProtocolMixin,
    SensingMixin,
    MovementMixin,
    BehaviorMixin,
    GatherMixin,
    ManifestMixin,
    CensusMixin,
    BrainMixin,
):
    def __init__(
        self,
        host: str,
        port: int,
        team: str,
        frequency: Optional[int],
        verbose: bool = False,
    ):
        # Per-action stderr logging costs milliseconds/action (file-write
        # syscalls), which at high f exceeds the 7/f s budget per action and
        # starves the bot. Off by default; -v re-enables for debugging.
        self.verbose = verbose
        self.host = host
        self.port = port
        self.team = team
        self.frequency_override = frequency
        self.frequency = max(1, frequency or 100)
        self.sock: Optional[socket.socket] = None
        self.recv_buffer = ""
        self.blocking_buffer: List[str] = []
        # Outstanding commands awaiting a reply, FIFO (server answers in order).
        # The server buffers up to MAX_COMMAND_QUEUE=10 per client and silently
        # drops the overflow, so we cap below that. Pipelining deterministic
        # command sequences (movement plans) keeps the server's action queue full
        # instead of going idle for a round-trip between every action.
        # (cmd, sent_at, queue_depth_at_send). queue_depth lets us discard
        # samples taken behind a backed-up pipeline (their latency includes the
        # wait for earlier commands, not just their own execution).
        self.pending: List[Tuple[str, float, int]] = []
        self.max_pending = 8

        # Passive frequency tracking. The server time unit can change mid-game
        # (the GUI lets an operator retune it), and every timeout/cooldown here
        # is scaled by self.frequency -- a stale value silently breaks command
        # wedge detection and gather/incant timers. Instead of re-running the
        # active probe (8 round trips + real Forwards that move the bot), we
        # infer freq from the latency of commands we already send: freq in
        # ticks/s ~= tick_cost / real_elapsed. EMA-smoothed, override wins.
        self.cmd_tick_cost = {
            "Forward": 7,
            "Right": 7,
            "Left": 7,
            "Look": 7,
            "Inventory": 1,
            "Connect_nbr": 0,
            "Eject": 7,
            "Fork": 42,
            "Incantation": 300,
        }
        self.freq_ema: Optional[float] = None
        # Set when a command is wedge-dropped (reply<->command FIFO broken),
        # cleared once the pipeline drains empty. Pauses freq sampling meanwhile.
        self.pipeline_desynced = False
        self.memory = Memory()
        self.state = State.LOOK
        self.running = True
        self.bot_id = (
            int(time.time() * 1000000) + random.randint(0, 99999)
        ) % 1000000000
        self.plan: List[str] = []
        self.stones_to_drop: List[str] = []
        self.preparing_incantation = False
        self.waiting_incantation = False

        # NOTE: level-scaled survival was tried (survive_per_level=6) and REGRESSED
        # (0 L4, more deaths): the food economy is the binding constraint, so a
        # higher floor just makes high-level bots farm endlessly and never reach
        # the food to start a gather. Kept flat. The scaling hook stays at 0 in
        # case a richer map / food strategy later makes it viable.
        self.survive_food = 8
        self.survive_per_level = 0
        self.fork_food = 25
        # Fork discipline: fewer mouths on a food-scarce map. Each bot forks at
        # most this many times, and only while low-level (see should_fork) — a
        # swarm of starving babies drains the shared food the valuable high-level
        # bots need. This lean setting (3 forks, level<=2 only) is the LOW-death
        # config that reached level 8 (game win) in ~16 min at f=1000: low
        # attrition lets the cohort persist and climb. Looser settings churned
        # more bots and starved the high-level pool.
        self.max_forks = 3
        self.max_plan_length = 2 if self.frequency >= 100 else 3

        self.must_fork_after_gather_fail = False

        self.active_req: Optional[str] = None
        self.active_level = 0
        self.active_need = 0
        self.active_acks: Dict[int, float] = {}
        self.gather_counter = 0
        self.gather_attempts = 0
        self.gather_started_at = 0.0
        self.gather_last_broadcast_at = 0.0
        self.gather_arrival_started_at = 0.0
        self.last_gather_giveup_at = 0.0

        self.answered_reqs: Dict[str, float] = {}
        self.pending_ack_req: Optional[str] = None
        self.pending_ack_level = 0
        self.pending_ack_leader: Optional[int] = None

        # Level-verified physical arrival. Look shows "player" with NO level, so a
        # leader can't tell a same-level teammate from a passing level-1 baby. A
        # follower instead knows it's on the leader's exact tile when the leader's
        # broadcast arrives as bearing 0 (server-authoritative), and announces
        # ARRIVED. The leader counts distinct same-level ARRIVED senders and only
        # incants on THAT count — never the level-blind on-tile player count.
        self.arrived_at_leader: set = set()

        self.following_req: Optional[str] = None
        self.following_leader: Optional[int] = None
        self.following_until = 0.0
        self.pending_follow_direction: Optional[int] = None

        # Freeze-on-tile handshake: a leader with enough players broadcasts HOLD,
        # teammates freeze in place through its Set+Incantation ritual instead of
        # wandering off (which drops the count below the requirement -> ko).
        self.arrived_hold_until = 0.0
        self.incant_hold_broadcasts = 0

        # Manifest blitz strategy — the ONLY strategy. Every bot races to collect
        # the FULL stone set for L2->L8 (sum of all levels' requirements). The first
        # to complete becomes the anchor, the team banks food, synchronises,
        # converges on full tanks, drops all stones at once, and incants straight up
        # L2->L8 (each ritual consumes only its level's stones; all 6 players level
        # each time). Measured: ~30s to L8 and 4/4 win rate, robust across
        # 10x10..40x40 maps at reserve~200. Needs ~6 reachable mates; with fewer it
        # waits out manifest_timeout then sets manifest_gave_up and plays the simple
        # per-level gather as a last-resort safety (NOT a user-selectable mode).
        self.blitzing = False  # anchor mid-blitz (incant repeatedly)
        self.manifest_gave_up = False  # fell back to normal play (no anchor emerged)
        self.manifest_started_at = time.time()
        # Phased rendezvous: collect -> bank -> ready -> converge -> blitz. Bots
        # bank food to a reserve BEFORE converging and only launch once all are
        # ready, so the whole cohort makes the trek on full tanks (the fix for the
        # food-vs-convergence wall).
        self.manifest_phase = "collect"  # collect|bank|ready|converge (anchor: +blitz)
        self.manifest_is_anchor = False
        self.manifest_anchor = None  # id of the anchor a member is rallying to
        self.ready_too: set = set()  # anchor: members who signalled READY_TOO
        # Bank a big food reserve before converging so the cohort can outlast the
        # slow oscillating rally — this is the load-bearing tuning. At ~8 food/sec
        # drain, reserve/8 ≈ seconds of hold time, so 200 ≈ 25s, enough for 6 to
        # assemble. 200 is near-optimal across map sizes; higher just wastes
        # foraging time (40x40 @400 was slower for no gain). Tune via ZAPPY_RESERVE.
        self.food_reserve = int(os.environ.get("ZAPPY_RESERVE", "200"))
        self.food_abort = max(12, self.food_reserve // 3)  # re-bank below this
        self.manifest_food_floor = max(20, self.food_reserve // 2)  # collector top-up
        self.last_focus_bcast = 0.0
        self.last_ready_bcast = 0.0

        # ---- Opening census + fork election --------------------------------
        # The blitz needs >= census_target reachable mates, but a bot can't ask
        # the server its team size. So each bot announces presence once it boots
        # (HELLO:from=id) and everyone unions the ids into team_members. The
        # lowest known id is the elected forker: ONLY it forks, past the normal
        # per-bot cap, until the union reaches the quorum. The observed member
        # count is the global limiter -> total forks ~= quorum - initial bots.
        # Forked children (and late siblings) HELLO on boot and join the union;
        # their ids are time-seeded and larger, so they never win the election.
        # HELLO doubles as a permanent heartbeat: members silent past
        # member_ttl() are evicted (death sends no broadcast), which breaks the
        # quorum again and re-arms the census so the team refills to target.
        self.team_members = {self.bot_id}
        self.team_last_seen: Dict[int, float] = {}
        self.census_target = int(os.environ.get("ZAPPY_TEAM_TARGET", "6"))
        # Floor for the census window; the effective window is census_window(),
        # which scales with frequency (3000/f) so slow games get time to fork.
        self.census_timeout = float(os.environ.get("ZAPPY_CENSUS_TIMEOUT", "30"))
        # Settle window: wait for existing members' HELLOs to land before acting
        # as forker, so a freshly booted bot doesn't fork while it still thinks
        # it's alone (a singleton is always its own min).
        self.census_settle = float(os.environ.get("ZAPPY_CENSUS_SETTLE", "3"))
        # How long to wait for a forked child to announce before forking again
        # (a deadbeat child shouldn't stall the census).
        self.census_child_timeout = float(os.environ.get("ZAPPY_CENSUS_CHILD", "4"))
        self.census_deadline: Optional[float] = None
        self.census_settle_until: Optional[float] = None
        self.census_fork_deadline = 0.0
        self.last_hello_at = 0.0
