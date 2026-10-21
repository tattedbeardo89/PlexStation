#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
headers=${VULKAN_HEADERS:-/workspace/deps/Vulkan-Headers/include}
mkdir -p "$repo/build"
flags=(-std=c++17 -O1 -g -Wall -Wextra -Werror -pthread
    -I"$repo/src" -I"$repo/third_party" -I"$repo/third_party/imgui"
    -I"$repo/third_party/volk" -I"$headers")
g++ "${flags[@]}" "$repo/third_party/tinyxml2/tinyxml2.cpp" "$repo/tests/controller_tests.cc" "$repo/src/plex/client.cc" "$repo/third_party/qrcodegen/qrcodegen.c" \
    "$repo"/third_party/imgui/imgui{,_draw,_tables,_widgets}.cpp -o "$repo/build/controller-tests"
"$repo/build/controller-tests"
g++ "${flags[@]}" "$repo/third_party/tinyxml2/tinyxml2.cpp" "$repo/tests/pad_backend_tests.cc" -o "$repo/build/pad-backend-tests"
"$repo/build/pad-backend-tests"
python3 "$repo/tests/test_package_permissions.py"
