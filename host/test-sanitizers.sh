#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
headers=${VULKAN_HEADERS:-/workspace/deps/Vulkan-Headers/include}
flags=(-std=c++17 -fno-exceptions -fno-rtti -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -pthread
  -I"$repo/src" -I"$repo/third_party" -I"$repo/third_party/imgui" -I"$repo/third_party/stb"
  -I"$repo/third_party/volk" -I"$headers")
mkdir -p "$repo/build"
# Compile shared instrumented sources once; every suite links the same ASan/UBSan objects.
common=()
for source in "$repo/src/plex/client.cc" "$repo/third_party/tinyxml2/tinyxml2.cpp"; do
  object="$repo/build/asan-$(basename "$source").o"
  g++ "${flags[@]}" -c "$source" -o "$object"
  common+=("$object")
done
for kind in controller plex home home_feed audio listening streaming discovery artwork subtitle cache; do
  extra=()
  if [[ $kind == artwork ]]; then extra+=("$repo/src/plex/artwork.cc"); fi
  if [[ $kind == controller ]]; then extra+=("$repo/third_party/qrcodegen/qrcodegen.c" "$repo"/third_party/imgui/imgui{,_draw,_tables,_widgets}.cpp); fi
  g++ "${flags[@]}" "$repo/tests/${kind}_tests.cc" "${common[@]}" "${extra[@]}" -o "$repo/build/${kind}-tests-asan" -lgnutls
  ASAN_OPTIONS=detect_leaks=1 "$repo/build/${kind}-tests-asan"
done

for mode in normal empty; do
  extra=()
  if [[ $mode == empty ]]; then extra+=(-DEMPTY_DECIMAL -Wl,--wrap=localeconv); fi
  g++ "${flags[@]}" "${extra[@]}" "$repo/tests/json_numeric_tests.cc" -o "$repo/build/json-numeric-$mode-asan"
  ASAN_OPTIONS=detect_leaks=1 "$repo/build/json-numeric-$mode-asan"
done
