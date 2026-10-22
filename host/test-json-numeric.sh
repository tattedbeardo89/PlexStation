#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$repo/build"
for mode in normal empty; do
  extra=()
  if [[ $mode == empty ]]; then extra+=(-DEMPTY_DECIMAL -Wl,--wrap=localeconv); fi
  g++ -std=c++17 -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
    -I"$repo/src" -I"$repo/third_party" "${extra[@]}" "$repo/tests/json_numeric_tests.cc" -o "$repo/build/json-numeric-$mode"
  "$repo/build/json-numeric-$mode"
done
