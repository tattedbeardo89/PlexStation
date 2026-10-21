#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# The same native build is used by Actions and Windows Docker Desktop/WSL2.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo"
# Mounted checkout/cache ownership belongs to the host runner, not container root.
git config --global --add safe.directory "$repo"
mkdir -p artifacts/logs /build
rm -f artifacts/PlexStation-PPSA85301.zip artifacts/SHA256SUMS
report=$repo/artifacts/validation.json
stage=initialization
finish() {
    result=$?
    trap - EXIT
    export BUILD_EXIT=$result BUILD_STAGE=$stage
    python3 - "$report" <<'PY'
import json, os, pathlib, subprocess, sys
report = pathlib.Path(sys.argv[1])
report.write_text(json.dumps({
    'success': os.environ['BUILD_EXIT'] == '0',
    'exit_code': int(os.environ['BUILD_EXIT']),
    'last_stage': os.environ['BUILD_STAGE'],
    'application_commit': subprocess.check_output(['git','rev-parse','HEAD'], text=True).strip(),
    'pins': json.load(open('ci/build-lock.json')),
    'native_build': 'completed' if os.environ['BUILD_EXIT']=='0' else 'failed',
    'host_tests': 'not run by this native build; prior results remain in docs/VALIDATION.md',
    'hardware_target': 'PS5 13.40, kstuff Lite, ShadowMountPlus 1.7 Beta 5 Fix 1',
    'hardware_verification': 'NOT TESTED; compilation does not establish compatibility',
}, indent=2)+'\n')
PY
    # Diagnostics are separate from the success-only installation artifact.
    if [[ -d /build/vlc/logs ]]; then cp -a /build/vlc/logs/. artifacts/logs/; fi
    if [[ -f /build/vlc/build-title/link.log ]]; then cp /build/vlc/build-title/link.log artifacts/logs/; fi
    find /build/PS5_Vulkan/.deps/work -maxdepth 1 -name '*.log' -type f -exec cp {} artifacts/logs/ \; 2>/dev/null || true
    exit "$result"
}
trap finish EXIT
exec > >(tee artifacts/logs/native-build.log) 2>&1
pin() { python3 -c 'import json,sys; print(json.load(open("ci/build-lock.json"))[sys.argv[1]])' "$1"; }
checkout() {
    local name=$1 sha=$2 root=/build/$1
    if [[ ! -d $root ]]; then
        mkdir -p "$root"
        git -C "$root" init -q
        git -C "$root" remote add origin "https://github.com/mihawk-99/$name.git"
        git -C "$root" fetch --depth 1 origin "$sha"
        git -C "$root" checkout --detach "$sha"
    fi
    git config --global --add safe.directory "$root"
    [[ $(git -C "$root" rev-parse HEAD) == "$sha" ]] || { echo "Wrong cached $name revision" >&2; exit 2; }
    git -C "$root" diff --exit-code --quiet
}
export VLC_PS5_WORK=/build/vlc PS5_VULKAN=/build/PS5_Vulkan
export PS5_PAYLOAD_SDK=$PS5_VULKAN/.deps/native/ps5-payload-sdk
export PS5_PAYLOAD_SDK_FORK=/build/PS5_PayloadSDK PS5_MESA_FORK=/build/PS5_Mesa
export PS5_MESA_REVISION
PS5_MESA_REVISION=$(pin PS5_Mesa)
export BUILD_JOBS=${BUILD_JOBS:-2}
export PATH="$VLC_PS5_WORK/tools/bin:$PATH"
export PS5_CLANG=/usr/bin/clang LLVM_READELF=/usr/bin/llvm-readelf LLVM_OBJCOPY=/usr/bin/llvm-objcopy
cp /opt/plexstation-packages.txt artifacts/packages.txt
clang --version
llvm-config --version
stage=source-checkouts
checkout PS5_Vulkan "$(pin PS5_Vulkan)"
checkout PS5_PayloadSDK "$(pin PS5_PayloadSDK)"
checkout PS5_Mesa "$PS5_MESA_REVISION"
[[ $(pin arch_snapshot) == $(sed -n 's|.*repos/\([0-9/]\{10\}\)/.*|\1|p' /etc/pacman.d/mirrorlist) ]] || { echo 'Snapshot configuration mismatch'; exit 2; }
# Upstream SDK bootstrap checks the v0.42 release SHA-256; RADV uses the pinned
# fork and its Meson wrap hashes. The clean-room runtime is checked unchanged.
stage=sdk
bash "$PS5_VULKAN/tools/setup-native-dependencies.sh"
stage=runtime
bash "$PS5_VULKAN/tools/rebuild-libc.sh"
(cd "$PS5_VULKAN/runtime" && sha256sum --check --strict libc.prx.sha256)
stage=converter
zlib=$(find "$PS5_VULKAN/.deps/native/zlib/root" -name libz.a -type f -print -quit)
[[ -n $zlib ]] || { echo 'SDK bootstrap did not build host zlib'; exit 2; }
mkdir -p "$PS5_VULKAN/build/host"
clang++ -std=c++20 -O2 -Wall -Wextra -Werror \
    -I"$PS5_VULKAN/.deps/native/zlib/root/usr/include" \
    "$PS5_VULKAN/tooling/native/native_app_builder.cpp" \
    "$PS5_VULKAN/tooling/native/self_container.cpp" \
    "$PS5_VULKAN/tooling/native/elf_object.cpp" \
    "$PS5_VULKAN/tooling/native/sce_module_writer.cpp" \
    "$zlib" -o "$PS5_VULKAN/build/host/ps5-native-tool"
stage=radv
bash "$PS5_VULKAN/tools/build-radv.sh" release
# Required by the existing VLC renderer's HDR and display-refresh APIs.
bash ps5/radv-patches.sh
stage=vlc-source
mkdir -p "$VLC_PS5_WORK/dl" "$VLC_PS5_WORK/src"
archive=$VLC_PS5_WORK/dl/vlc-$(pin vlc_version).tar.xz
if [[ ! -s $archive ]]; then
    curl --fail --location --retry 3 --proto '=https' "$(pin vlc_url)" -o "$archive.part"
    mv "$archive.part" "$archive"
fi
# Cache corruption must never be silently accepted. First acquisition uses
# verified HTTPS release URLs; subsequent reuse is checked against its manifest.
if [[ -f $VLC_PS5_WORK/dl/SHA256SUMS ]]; then
    (cd "$VLC_PS5_WORK/dl" && sha256sum --check --strict SHA256SUMS)
fi
if [[ ! -d $VLC_PS5_WORK/src/vlc-$(pin vlc_version) ]]; then
    tar -xJf "$archive" -C "$VLC_PS5_WORK/src"
fi
# A stamp covers every build recipe, pin, container and dependency patch. No
# prefix from a different configuration is restored under a fallback key.
fingerprint=$(python3 ci/fingerprint.py)
if [[ ! -f /build/native-dependencies.complete || $(cat /build/native-dependencies.complete) != "$fingerprint" ]]; then
    stage=dependencies
    TARGET=ps5 bash ps5/deps/build-deps.sh
    stage=libvlc
    TARGET=ps5 bash ps5/configure-vlc.sh
    printf '%s\n' "$fingerprint" > /build/native-dependencies.complete
fi
stage=title
# Do not reuse a prior package if the new compilation fails.
rm -rf dist/PPSA85301
WITH_DEMO=0 bash ps5/build-title.sh
stage=package-validation
python3 tools/check-package.py dist/PPSA85301
python3 ci/validate-native.py dist/PPSA85301 "$VLC_PS5_WORK/build-title/llvm-pie.elf" "$PS5_VULKAN/build/host/ps5-native-tool"
(cd "$VLC_PS5_WORK/dl" && find . -maxdepth 1 -type f ! -name '*.part' ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS)
cp "$VLC_PS5_WORK/dl/SHA256SUMS" artifacts/source-SHA256SUMS.txt
cp "$PS5_VULKAN/.deps/native/radv-release/PROVENANCE.txt" artifacts/radv-provenance.txt
stage=archive
# ZIP retains PPSA85301 as the top-level directory when extracted on Windows.
(cd dist && python3 -m zipfile -c ../artifacts/PlexStation-PPSA85301.zip PPSA85301)
(cd artifacts && sha256sum PlexStation-PPSA85301.zip > SHA256SUMS)
stage=complete
