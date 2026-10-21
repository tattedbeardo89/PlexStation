#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo"
mkdir -p build
g++ -std=c++17 -O2 -Wall -Wextra -Werror -I src -I third_party \
    third_party/tinyxml2/tinyxml2.cpp src/plex/client.cc third_party/qrcodegen/qrcodegen.c tests/auth_tests.cc -o build/auth-tests
build/auth-tests
