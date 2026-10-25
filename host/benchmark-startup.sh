#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$repo/work/stabilization/baseline/plex" "$repo/build"
git -C "$repo" show e79d5bd3a882cb6c841de4d0a8cd084bd1eb840a:src/plex/home_feed.h > "$repo/work/stabilization/baseline/plex/home_feed.h"
for variant in baseline incremental; do
  flags=()
  if [[ $variant == baseline ]]; then flags+=(-DBASELINE_FEED -I"$repo/work/stabilization/baseline" -I"$repo/src/plex"); fi
  g++ -std=c++17 -O1 -pthread "${flags[@]}" -I"$repo/src" -I"$repo/third_party" "$repo/tests/startup_benchmark.cc" "$repo/src/plex/client.cc" "$repo/third_party/tinyxml2/tinyxml2.cpp" -o "$repo/build/startup-$variant"
  for run in 1 2 3 4 5; do "$repo/build/startup-$variant" 2>"$repo/work/stabilization/benchmark-$variant.log"; done
done
