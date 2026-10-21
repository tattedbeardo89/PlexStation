#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo"
mkdir -p build
g++ -std=c++17 -O2 -Wall -Wextra -Werror -I src -I third_party \
    third_party/tinyxml2/tinyxml2.cpp src/plex/client.cc tests/connection_tests.cc -o build/connection-tests
build/connection-tests
headers=${VULKAN_HEADERS:-/workspace/deps/Vulkan-Headers/include}
g++ -std=c++17 -O2 -Wall -Wextra -Werror -pthread -I src -I third_party \
    -I third_party/volk -I "$headers" third_party/tinyxml2/tinyxml2.cpp \
    src/plex/client.cc src/plex/transport_ps5.cc tests/connection_transport_probe.cc \
    $(pkg-config --cflags --libs gnutls) -Wl,--wrap=getaddrinfo,--wrap=connect,--wrap=poll,--wrap=send,--wrap=recv -o build/connection-transport-probe
python3 tests/test_connections.py

g++ -std=c++17 -O2 -Wall -Wextra -Werror -I src -I third_party/volk -I "$headers" \
    src/prefs.cc tests/prefs_connection_probe.cc -o build/prefs-connection-probe
python3 - <<'PYTEST'
import tempfile, subprocess
with tempfile.TemporaryDirectory(prefix='plex-prefs-connection-') as directory:
    for action in ('write','read'):
        subprocess.run(['build/prefs-connection-probe',directory,action],check=True)
print('Manual endpoint and server identity preferences persisted across separate processes; no credentials stored')
PYTEST
