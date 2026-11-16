#!/usr/bin/env bash
# Configure, build and test Arena Evolve. Run on Linux (WSL2 Ubuntu).
#   bash scripts/check.sh              normal Debug build
#   bash scripts/check.sh thread       Debug build with a sanitizer (address, undefined or thread)
set -euo pipefail

sanitizer="${1:-}"
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Build output goes into the Linux home folder, not the repo: it is much faster than /mnt/c
# and keeps the repo clean. One folder per sanitizer so the builds do not overwrite each other.
build="$HOME/arena-build${sanitizer:+-$sanitizer}"

cmake -S "$repo" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Debug ${sanitizer:+-DSANITIZE=$sanitizer} -Wno-dev
cmake --build "$build"
ctest --test-dir "$build" --output-on-failure
