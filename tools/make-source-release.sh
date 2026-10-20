#!/usr/bin/env bash
# Application sources only; binary redistribution additionally requires corresponding dependency sources.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$repo/dist"
tar -C "$repo" -czf "$repo/dist/plexstation-app-source.tar.gz" \
    LICENSE THIRD_PARTY.md README.md AGENTS.md .gitignore .clang-format docs host patches ps5 shaders src tests third_party tools assets
printf 'Application source archive created; include corresponding built dependency sources before binary redistribution.\n'
