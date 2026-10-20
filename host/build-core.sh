#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$repo/build"
flags=(-std=c++17 -O2 -g -Wall -Wextra -Werror -I"$repo/src" -I"$repo/third_party")
g++ "${flags[@]}" "$repo/src/plex/client.cc" "$repo/tests/plex_tests.cc" -o "$repo/build/plex-tests"
"$repo/build/plex-tests"
g++ "${flags[@]}" "$repo/src/plex/client.cc" "$repo/src/plex/transport_curl.cc" \
    "$repo/tools/plexctl.cc" $(pkg-config --cflags --libs libcurl) -o "$repo/build/plexctl"
g++ "${flags[@]}" "$repo/src/plex/client.cc" "$repo/src/plex/transport_curl.cc" \
    "$repo/tests/transport_probe.cc" $(pkg-config --cflags --libs libcurl) -o "$repo/build/transport-probe"
python3 "$repo/tests/test_transport.py"
