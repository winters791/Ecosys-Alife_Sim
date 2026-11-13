#!/usr/bin/env bash
# Build the Godot extension (only what changed) and open the viewer.
#
#   ./view.sh                      -> 128x128 world, seed 42
#   ./view.sh --size=256 --seed=42 --pop=500
#
# Needs Godot 4.7+ and godot-cpp:
#   git clone --depth 1 https://github.com/godotengine/godot-cpp thirdparty/godot-cpp
# Set GODOT=/path/to/godot if it isn't in /Applications or on your PATH.

set -euo pipefail
cd "$(dirname "$0")"

godot="${GODOT:-}"
if [[ -z "$godot" ]]; then
  if [[ -x /Applications/Godot.app/Contents/MacOS/Godot ]]; then
    godot=/Applications/Godot.app/Contents/MacOS/Godot
  else
    godot="$(command -v godot || command -v godot4 || true)"
  fi
fi
if [[ -z "$godot" ]]; then
  echo "Godot not found. Set GODOT=/path/to/godot" >&2
  exit 1
fi
if [[ ! -d thirdparty/godot-cpp ]]; then
  echo "Missing thirdparty/godot-cpp. Run:" >&2
  echo "  git clone --depth 1 https://github.com/godotengine/godot-cpp thirdparty/godot-cpp" >&2
  exit 1
fi

# The first build compiles godot-cpp and takes a few minutes; after that it's seconds.
if [[ ! -f build-godot/CMakeCache.txt ]]; then
  cmake -S . -B build-godot -DCMAKE_BUILD_TYPE=Release -DLUCA_BUILD_GODOT=ON
fi
cmake --build build-godot --target luca_godot -j

# First run only: let Godot register the extension.
if [[ ! -f viewer/.godot/extension_list.cfg ]]; then
  "$godot" --path viewer --headless --import >/dev/null 2>&1 || true
fi

exec "$godot" --path viewer -- "$@"
