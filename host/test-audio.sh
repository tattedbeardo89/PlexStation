#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$repo/build"
g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -pthread -I"$repo/src" -I"$repo/third_party" \
 "$repo/tests/audio_tests.cc" "$repo/src/plex/client.cc" "$repo/third_party/tinyxml2/tinyxml2.cpp" -o "$repo/build/audio-tests"
"$repo/build/audio-tests"
