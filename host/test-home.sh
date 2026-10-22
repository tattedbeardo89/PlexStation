#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$repo/build"
g++ -std=c++17 -O2 -Wall -Wextra -Werror -I"$repo/src" -I"$repo/third_party" \
  "$repo/src/plex/client.cc" "$repo/third_party/tinyxml2/tinyxml2.cpp" "$repo/tests/home_tests.cc" -o "$repo/build/home-tests"
"$repo/build/home-tests"
