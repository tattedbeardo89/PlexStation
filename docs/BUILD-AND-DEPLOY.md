# Build, run and deploy

For a Windows-only machine, see [GitHub Actions and the WSL2 container fallback](GITHUB-ACTIONS.md).
The committed workflow builds the actual native title; it does not substitute host tests.

## Verified host core workflow

Requires Linux, g++ with C++17, pkg-config, libcurl headers/library, Python 3 and OpenSSL (local test certificate generation).

```sh
cd /workspace/PlexStation
bash host/build-core.sh
```

Additional checks in the prepared cloud cache:

```sh
bash host/check-native-source.sh
bash host/test-native-transport.sh
```

The latter also needs GnuTLS development headers/library and the pinned Vulkan headers.

The core command builds and runs core tests, builds `build/plexctl`, and runs real local
HTTP transport tests. It does not build or validate the VLC player.
A private test account can be linked interactively, once plex.tv is reachable:

```sh
./build/plexctl /workspace/plexstation-private login
./build/plexctl /workspace/plexstation-private servers
./build/plexctl /workspace/plexstation-private signout
```

The directory is mode 0700 and the account file 0600. Never paste tokens into
commands, source files, logs, chat or releases. Device-link instructions are
shown by the program. Live sign-in has not been tested in this environment.

## Native player prerequisites

The actual upstream recipe is static VLC 3.0.24 plus the libraries in
`ps5/deps/build-deps.sh`. Do not substitute stock system VLC: the player uses
private headers, patched modules, and static plugin registration.

Set these consistently in the shell used for all commands:

```sh
export VLC_PS5_WORK=/workspace/plexstation-work
export PS5_VULKAN=/workspace/deps/PS5_Vulkan
export PATH=/workspace/deps/tools/bin:$PATH
```

PS5_Vulkan inspected revision: f410e67a0241b3e8edcdbcd2a9b285dfc4a412ed.
Its SDK bootstrap expects PS5_PayloadSDK at
`b83202be73e930050e00de0f1b4d9ec46c0391bf`, with the upstream v0.42 SDK
archive verified against the hash in its setup script. A current SDK checkout
alone is not the installed toolchain. Follow PS5_Vulkan's own dependency,
LLVM/Clang, platform-library and release RADV build instructions, including
its AGENTS.md when modifying that dependency. Do not disable checksum checks.
The local cloud checkout lacks that installed SDK/RADV toolchain. The pinned
Arch container successfully builds it on GitHub Actions; see
[hosted build instructions and artifact](GITHUB-ACTIONS.md).

The shader compiler was built locally from glslang 15.2.0 commit
99ee11b0106b48f06fb9e2a324ba928b8316cc33. Vulkan headers are pinned at
409c16be502e39fe70dd6fe2d9ad4842ef2c9a53 (v1.4.313). Those checkout/build caches
are outside application sources under /workspace/deps.

Download VLC using the upstream README's official URL; verify its upstream
signature/checksum before extracting. Downloads are currently proxy-blocked:
`https://download.videolan.org/pub/videolan/vlc/3.0.24/vlc-3.0.24.tar.xz`.
Dependency scripts also require their documented source hosts; see the
script's fetch calls. Expand network access only for the needed hosts.

After prerequisites are available, the upstream commands are:

```sh
# VLC source must exist at $VLC_PS5_WORK/src/vlc-3.0.24
TARGET=host bash ps5/deps/build-deps.sh
TARGET=host bash ps5/configure-vlc.sh
bash host/build-app.sh
# Result: $VLC_PS5_WORK/out/plexstation-host

TARGET=ps5 bash ps5/deps/build-deps.sh
TARGET=ps5 bash ps5/configure-vlc.sh
WITH_DEMO=0 bash ps5/build-title.sh
python3 tools/check-package.py dist/PPSA85301
```

The complete PS5 sequence, including these dependency recipes, succeeded in
[Actions run 39633799498](https://github.com/tattedbeardo89/PlexStation/actions/runs/39633799498).
The full host UI build has not succeeded locally.
Upstream dependency downloads have no complete checksum manifest: create and
verify an authoritative source/signature manifest before publishing binaries.
Do not treat a successful source download as a reproducible verified release.

The host UI build is offscreen Vulkan, with scripted input and silent audio:
prepare a separate writable app directory with `fonts/` and `certs/`, then set
`VLCPS5_DATA` to it. `VLCPS5_SCRIPT` selects the upstream pad-script input.
Do not publish loopback previews. A display-only screenshot would not establish
playback or audio synchronization.

## PS5 deployment target

User's target: firmware 13.40, kstuff Lite, ShadowMountPlus 1.7 Beta 5 Fix 1.
Upstream documents kstuff plus ShadowMountPlus; it does not establish that
this exact Lite/firmware combination works. No replacement jailbreak is part
of this project. Optional upstream sandbox lifting mentions etaHEN, OnionHEN
or a Helper; it is not required by the new Plex client design. No helper is
automatically deployed.

When a real build passes package checks, copy the **whole** `PPSA85301` folder
into `/data/homebrew/`, following your existing ShadowMountPlus workflow.
Check that `/data/homebrew/PPSA85301` is unused first. Do not overwrite an
existing title, copy over `PPSA85300`, change system jailbreak files, or install
VLC's original metadata. Keep `sce_module`, fonts, certificates and imports
with the executable. This follows upstream's signed `eboot.bin` folder format,
not an assumed retail PKG installation path.

Await device verification before labeling a package compatible. Validate launch,
controller input, TLS, authentication persistence, real libraries, movie and
episode playback, seek/pause/resume, tracks, progress synchronization, return
to details, and audio offset behavior. Keep VLC installed separately.

## Windows FTP modes and controller follow-up

Use [the exact FTP permission and DualSense test procedure](CONTROLLER-TESTING.md).
The package now supplies 755 directories/executables and 644 read-only files,
with ZIP metadata checked in Actions. Windows extraction/FTP may discard Unix
modes; set and verify them remotely. Keep account files 600 and ensure app data
ownership allows runtime writes. The confirmed 777 launch workaround is not
the permanent deployment procedure.

## Angular branding assets

`assets/plexstation.svg` is the original vector master. `python3 tools/make-brand.py`
regenerates the PNG masters (authoring requires Pillow). The authoring script also writes matching 1920x1080 DXT1 DDS backgrounds,
preserving the established PS5 splash format. These rasters are committed and
copied into the installation folder; Pillow is not needed in the native container. `check-package.py`
checks the installed 512x512 PNG icon and both full-sized backgrounds. The in-app
header draws the same geometry using ImGui. Keep PPSA85301 and its saved account
files; a stale shell icon should be checked after fully closing/restarting the
existing launcher/console, without deleting application data or system caches.
