#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Zappy AI test harness — runs N independent games in parallel and writes a
# study report (time-to-each-level, deaths, forks, population-over-time, ritual
# stats) to ai/test_logs/run_<timestamp>/.
#
# Run N independent games to beat the per-game variance: more samples per
# wall-window without needing a higher frequency (the Python bot can't keep up
# much past f=1000 — it's request-reply latency bound).
#
# Usage (all overridable as env vars):
#   N=3 BOTS=8 DUR=900 FREQ=1000 MAPW=20 MAPH=20 ./ai/run_ai_test.sh
#   make -C gui3d test-ai                 # via the Makefile rule
# ---------------------------------------------------------------------------
set -u

# --- config -----------------------------------------------------------------
N=${N:-3}            # parallel games
BOTS=${BOTS:-8}      # starting bots per game
DUR=${DUR:-900}      # seconds per run (15 min; L8 takes ~8-16 min at f=1000)
FREQ=${FREQ:-1000}   # server frequency (>~1500 starves this Python bot)
MAPW=${MAPW:-20}
MAPH=${MAPH:-20}
TEAM=${TEAM:-t1}
CLIENTS=${CLIENTS:-40}   # -c slots per team (room for forks)
SAMPLE=${SAMPLE:-30}     # population sampling interval, seconds
BASEPORT=${BASEPORT:-4600}
# Generic A/B: the first AB_GAMES games run with env var $AB_ENV=1 set (arm A),
# the rest without it (arm B / default). E.g. AB_ENV=ZAPPY_NO_OVERSTACK AB_GAMES=2
# on a 4-game run => 2 no-overstack vs 2 overstack, identical contention.
AB_ENV=${AB_ENV:-}
AB_GAMES=${AB_GAMES:-0}

export LC_ALL=C

# This Python bot is real-time-sensitive: too many parallel processes cause CPU
# contention that slows every bot below f viability and skews the climb. Past
# ~4 games on a typical box the results stop being comparable.
if [ "$N" -gt 4 ]; then
  echo "WARNING: N=$N games may overload the CPU (each game's population grows to"
  echo "         ~30 bots). Results across runs become incomparable. Prefer N<=4"
  echo "         and repeat runs for more samples. Continuing anyway in 3s..."
  sleep 3
fi

# --- resolve paths (script lives in ai/, repo root is its parent) -----------
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SERVER="$ROOT/zappy_server"
BOT="$ROOT/ai/baseline/zappy_ai_baseline.py"
STAMP="$(date +%Y%m%d_%H%M%S)"
OUT="$ROOT/ai/test_logs/run_$STAMP"
mkdir -p "$OUT"
REPORT="$OUT/report.txt"

[ -x "$SERVER" ] || { echo "ERROR: server binary not found/built at $SERVER (run: make -C $ROOT build)"; exit 1; }
[ -f "$BOT" ]    || { echo "ERROR: bot not found at $BOT"; exit 1; }

echo "config: N=$N bots=$BOTS dur=${DUR}s freq=$FREQ map=${MAPW}x${MAPH} -> $OUT"

# --- clean any stale processes on our port range ----------------------------
for g in $(seq 0 $((N-1))); do pkill -f "zappy_server -p $((BASEPORT+g))" 2>/dev/null; done
pkill -f "zappy_ai_baseline.py -p 46" 2>/dev/null
sleep 1

# --- launch servers ---------------------------------------------------------
SRVS=()
for g in $(seq 0 $((N-1))); do
  PORT=$((BASEPORT + g))
  "$SERVER" -p "$PORT" -x "$MAPW" -y "$MAPH" -n "$TEAM" -c "$CLIENTS" -f "$FREQ" \
    >"$OUT/server$g.log" 2>&1 &
  SRVS+=($!)
done
sleep 1

# --- launch bots; every line prefixed with elapsed seconds (locale-safe) ----
T0=$(date +%s)
for g in $(seq 0 $((N-1))); do
  PORT=$((BASEPORT + g))
  ABENV=""
  if [ -n "$AB_ENV" ] && [ "$g" -lt "$AB_GAMES" ]; then ABENV="$AB_ENV=1"; fi
  for i in $(seq 1 "$BOTS"); do
    ( env $ABENV python3 "$BOT" -p "$PORT" -n "$TEAM" -h localhost -f "$FREQ" 2>&1 \
        | awk -v t0="$T0" '{print (systime()-t0), $0; fflush()}' >> "$OUT/game$g.log" ) &
  done
done

# --- population sampler (background): alive bots per game over time ----------
POP="$OUT/population.csv"
{ printf "t"; for g in $(seq 0 $((N-1))); do printf ",g%d" "$g"; done; printf ",total\n"; } > "$POP"
(
  while :; do
    sleep "$SAMPLE"
    t=$(( $(date +%s) - T0 ))
    [ "$t" -ge "$DUR" ] && break
    line="$t"; total=0
    for g in $(seq 0 $((N-1))); do
      c=$(pgrep -f "zappy_ai_baseline.py -p $((BASEPORT+g))" | wc -l)
      line="$line,$c"; total=$((total+c))
    done
    echo "$line,$total" >> "$POP"
  done
) &
SAMPLER=$!

# --- wait, then stop everything ---------------------------------------------
sleep "$DUR"
kill "$SAMPLER" 2>/dev/null
pkill -f "zappy_ai_baseline.py -p 46" 2>/dev/null
for s in "${SRVS[@]}"; do kill "$s" 2>/dev/null; done
sleep 1

# --- analyse + write report -------------------------------------------------
{
  echo "===================================================================="
  echo "Zappy AI test report  $STAMP"
  echo "config: games=$N  bots/game=$BOTS  dur=${DUR}s  freq=$FREQ  map=${MAPW}x${MAPH}"
  [ -n "$AB_ENV" ] && [ "$AB_GAMES" -gt 0 ] && echo "A/B: games 0..$((AB_GAMES-1)) run with $AB_ENV=1, rest default"
  echo "===================================================================="
  echo
  reached8=0
  for g in $(seq 0 $((N-1))); do
    f="$OUT/game$g.log"
    [ -f "$f" ] || continue
    deaths=$(grep -cF '[AI] DEAD' "$f")
    forks=$(grep -cF 'spawned child' "$f")
    mx=$(grep -oE 'LEVELUP [0-9]+' "$f" | grep -oE '[0-9]+' | sort -n | tail -1)
    arm=""
    if [ -n "$AB_ENV" ]; then
      if [ "$g" -lt "$AB_GAMES" ]; then arm="[$AB_ENV] "; else arm="[default] "; fi
    fi
    echo "--- game $g $arm: maxlevel=${mx:-1}  deaths=$deaths  forks=$forks ---"
    for lvl in 2 3 4 5 6 7 8; do
      t=$(awk -v L="LEVELUP $lvl" '$0 ~ L {print $1; exit}' "$f")
      n=$(grep -cF "LEVELUP $lvl" "$f")
      if [ -n "$t" ]; then
        printf "    L%d  first @ %4ds (%2dm%02ds)   total reaches: %d\n" "$lvl" "$t" "$((t/60))" "$((t%60))" "$n"
      fi
    done
    [ "${mx:-1}" -ge 8 ] && reached8=$((reached8+1))
    echo
  done
  echo "--- summary ---"
  echo "games reaching L8: $reached8 / $N"
  # fastest L8 across games
  best=$(for g in $(seq 0 $((N-1))); do awk '/LEVELUP 8/{print $1; exit}' "$OUT/game$g.log" 2>/dev/null; done | sort -n | head -1)
  [ -n "$best" ] && echo "fastest time-to-L8: ${best}s ($((best/60))m$((best%60))s)"
  echo
  echo "population over time (alive bots per game): see population.csv"
  column -t -s, "$POP" 2>/dev/null || cat "$POP"
} > "$REPORT"

echo "DONE -> $REPORT"
cat "$REPORT"
