#!/usr/bin/env bash
# Rend les slides HTML en PNG.
#   ./render.sh          → toutes les slides présentes
#   ./render.sh 01 04    → slides choisies
# Sorties : out/slideNN.png (3840×2160, fonds finaux, .native masqués)
#           out/preview/slideNN.png (1920×1080, .native visibles)
set -euo pipefail
cd "$(dirname "$0")"
CHROME=${CHROME:-google-chrome}
mkdir -p out/preview

slides=("$@")
if [ ${#slides[@]} -eq 0 ]; then
  for f in slide??.html; do slides+=("${f:5:2}"); done
fi

for nn in "${slides[@]}"; do
  f="slide${nn}.html"
  [ -f "$f" ] || { echo "skip: $f absent" >&2; continue; }
  "$CHROME" --headless=new --disable-gpu --hide-scrollbars \
    --force-device-scale-factor=2 --window-size=1920,1080 \
    --virtual-time-budget=3000 \
    --screenshot="out/slide${nn}.png" "file://$PWD/${f}?final" 2>/dev/null
  "$CHROME" --headless=new --disable-gpu --hide-scrollbars \
    --force-device-scale-factor=1 --window-size=1920,1080 \
    --virtual-time-budget=3000 \
    --screenshot="out/preview/slide${nn}.png" "file://$PWD/${f}" 2>/dev/null
  echo "rendu: slide${nn}"
done
