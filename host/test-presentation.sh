#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$repo/build"
g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fno-exceptions -fno-rtti -pthread -I"$repo/src" -I"$repo/third_party" "$repo/tests/presentation_tests.cc" "$repo/src/plex/client.cc" "$repo/third_party/tinyxml2/tinyxml2.cpp" -o "$repo/build/presentation-tests" -lgnutls
"$repo/build/presentation-tests"
python3 "$repo/tests/test_category_art.py"
