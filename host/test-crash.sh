#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$repo/build"
g++ -std=c++17 -O1 -g -fno-exceptions -fno-rtti -fno-omit-frame-pointer -Wall -Wextra -Werror \
  -I"$repo/src" "$repo/tests/termination_trace_tests.cc" -o "$repo/build/termination-trace-tests"
"$repo/build/termination-trace-tests"
python3 "$repo/tests/test_symbolization.py"
python3 "$repo/tests/test_debug_collection.py"

python3 "$repo/tests/test_baseline_archive.py"
cc -std=c11 -O2 -Wall -Wextra -Werror -I"$repo/src" "$repo/tests/crash_exit_tests.c" -o "$repo/build/crash-exit-tests"
"$repo/build/crash-exit-tests"
