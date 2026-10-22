#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
headers=${VULKAN_HEADERS:-/workspace/deps/Vulkan-Headers/include}
flags=(-std=c++17 -fno-exceptions -fno-rtti -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -pthread
  -I"$repo/src" -I"$repo/third_party" -I"$repo/third_party/imgui" -I"$repo/third_party/stb"
  -I"$repo/third_party/volk" -I"$headers")
mkdir -p "$repo/build"
common=("$repo/src/plex/client.cc" "$repo/third_party/tinyxml2/tinyxml2.cpp")
for kind in plex home home_feed audio listening artwork controller; do
  extra=()
  if [[ $kind == artwork ]]; then extra+=("$repo/src/plex/artwork.cc"); fi
  if [[ $kind == controller ]]; then extra+=("$repo/third_party/qrcodegen/qrcodegen.c" "$repo"/third_party/imgui/imgui{,_draw,_tables,_widgets}.cpp); fi
  g++ "${flags[@]}" "$repo/tests/${kind}_tests.cc" "${common[@]}" "${extra[@]}" -o "$repo/build/${kind}-tests-asan"
  ASAN_OPTIONS=detect_leaks=1 "$repo/build/${kind}-tests-asan"
done
