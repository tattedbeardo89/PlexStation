#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$repo/build"
g++ -std=c++17 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -I"$repo/src" "$repo/tests/subtitle_tests.cc" -o "$repo/build/subtitle-tests"
"$repo/build/subtitle-tests"
