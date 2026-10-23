#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
headers=${VULKAN_HEADERS:-/workspace/deps/Vulkan-Headers/include}
mkdir -p "$repo/build"
g++ -std=c++17 -O1 -Wall -Wextra -Werror -pthread \
  -I"$repo/src" -I"$repo/third_party" -I"$repo/third_party/imgui" \
  -I"$repo/third_party/stb" -I"$repo/third_party/volk" -I"$headers" \
  "$repo/tests/artwork_tests.cc" "$repo/src/plex/artwork.cc" \
  "$repo/src/plex/client.cc" "$repo/third_party/tinyxml2/tinyxml2.cpp" -o "$repo/build/artwork-tests" -lgnutls
"$repo/build/artwork-tests"
