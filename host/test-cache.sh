#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$repo/build"
g++ -std=c++17 -O1 -Wall -Wextra -Werror -pthread -I"$repo/src" "$repo/tests/cache_tests.cc" -o "$repo/build/cache-tests" -lgnutls
"$repo/build/cache-tests"
