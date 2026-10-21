#!/usr/bin/env bash
# Exercise the PS5 GnuTLS/socket source on Linux. This does not validate PS5 ABI/runtime behavior.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
headers=${VULKAN_HEADERS:-/workspace/deps/Vulkan-Headers/include}
mkdir -p "$repo/build"
g++ -std=c++17 -O2 -Wall -Wextra -Werror -I"$repo/src" -I"$repo/third_party" \
    -I"$repo/third_party/volk" -I"$headers" -DTEST_PS5_TRANSPORT \
    "$repo/third_party/tinyxml2/tinyxml2.cpp" "$repo/src/plex/client.cc" "$repo/src/plex/transport_ps5.cc" \
    "$repo/tests/transport_probe.cc" $(pkg-config --cflags --libs gnutls) \
    -o "$repo/build/transport-gnutls-probe"
python3 "$repo/tests/test_transport.py" transport-gnutls-probe
