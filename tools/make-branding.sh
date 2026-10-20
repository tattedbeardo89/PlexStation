#!/usr/bin/env bash
# Reproduce PlexStation's original icon/loading images with ImageMagick.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
convert -size 512x512 xc:'#101217' -fill '#1c1f27' -stroke '#e5ac42' -strokewidth 6 \
    -draw 'roundrectangle 48,48 464,464 64,64' -stroke none -fill '#e5ac42' \
    -draw 'path "M166 350V148h94c68 0 99 34 99 82s-31 81-99 81h-40v39zm54-155v69h37c32 0 48-10 48-34s-16-35-48-35z"' \
    -draw 'circle 345,350 363,350' "$repo/ps5/sce_sys/icon0.png"
convert -size 1920x1080 xc:'#101217' "$repo/ps5/sce_sys/icon0.png" -gravity center \
    -composite "$repo/ps5/sce_sys/pic0.dds"
cp "$repo/ps5/sce_sys/pic0.dds" "$repo/ps5/sce_sys/pic1.dds"
