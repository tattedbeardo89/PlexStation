#!/usr/bin/env bash
# Host-side syntax validation of new source only. NOT a PS5 build or playback test.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
headers=${VULKAN_HEADERS:-/workspace/deps/Vulkan-Headers/include}
[[ -f $headers/vulkan/vulkan.h ]] || { echo 'Set VULKAN_HEADERS to the Vulkan-Headers include directory' >&2; exit 2; }
g++ -std=c++17 -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
    -I"$repo/src" -I"$repo/third_party" -I"$repo/third_party/imgui" \
    -I"$repo/third_party/stb" -I"$repo/third_party/volk" -I"$headers" \
    -fsyntax-only "$repo/src/plex_ui.cc" "$repo/src/plex/artwork.cc" \
    "$repo/src/plex/client.cc" "$repo/src/plex/transport_ps5.cc"
echo 'New native source passed host syntax checks; cross-linking and hardware behavior remain unverified'
