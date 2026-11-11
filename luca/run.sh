#!/usr/bin/env bash
# Build (only what changed), run the sim, and plot — in one command.
#
#   ./run.sh                       -> out/latest, default settings
#   ./run.sh kleiber_k03           -> out/kleiber_k03
#   ./run.sh test1 --light uniform --ticks 10000 --seed 3
#
# The first argument is the output folder name (optional). Everything after
# it is passed straight to luca_headless (see ./build/luca_headless --help).

set -euo pipefail
cd "$(dirname "$0")"

name="latest"
if [[ $# -gt 0 && "$1" != --* ]]; then
  name="$1"
  shift
fi
out="out/$name"

# Configure once; after that, only rebuild changed files.
if [[ ! -f build/CMakeCache.txt ]]; then
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
fi
cmake --build build -j

./build/luca_headless --ticks 20000 --out "$out" "$@"
.venv/bin/python tools/plot.py "$out"
echo "plots: $out/run.png  $out/map.png"
