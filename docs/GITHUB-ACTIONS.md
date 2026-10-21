# Native PS5 builds on GitHub Actions and Windows 11

## Latest controller test build

Use [run 39644828464](https://github.com/tattedbeardo89/PlexStation/actions/runs/39644828464),
artifact `PlexStation-PPSA85301-26c5116044c2936364ce76cf94ab6054eb83e87f`.
Native compilation, package validation, Unix ZIP permissions and uploads passed.
See [DualSense testing and Windows FTP instructions](CONTROLLER-TESTING.md).
Device navigation and the new permission deployment require hardware testing.

## Host choice and exact build

PS5_Vulkan's pinned README explicitly supports **Arch/CachyOS**. Its existing
Ubuntu workflow validates its converter/packaging tool, not the complete native
RADV + mesa_clc + libclc chain. This project therefore does **not** assume full
Ubuntu compatibility: an Ubuntu 24.04 GitHub-hosted runner runs Docker with an
**Arch Linux container**. No PS5, jailbreak or local Linux workstation is
required to compile. Console launch/playback still require device testing.

`ci/build-lock.json` pins the amd64 container digest, Arch signed package
snapshot (2026/10/20), PS5_Vulkan, payload SDK and Mesa fork commits, and VLC
3.0.24. FFmpeg and the remaining media-library versions stay pinned by the
existing `ps5/deps/build-deps.sh` recipes. The SDK release checksum, Mesa wrap
checksums and clean-room libc.prx checksum remain enforced. Arch package
signature checking is retained; there is no insecure fallback mirror mode.

The entire package environment, including LLVM/Clang, libclc, translator,
Meson, generators, assembler and build tools, comes from the fixed Arch
snapshot. `packages.txt` records the exact resolved versions. Source archive
hashes are recorded in `source-SHA256SUMS.txt`, and reused downloads are checked
against that manifest. Media archive URLs are version-pinned HTTPS upstream
releases; most inherited recipes do not have independently pinned archive
checksums. This is version-reproducible configuration, not a claim of proven
bit-identical rebuilds or a complete independently authenticated source lock.

The workflow invokes the actual production sequence:

1. Fetch the three exact Git commits into the build cache.
2. Bootstrap the pinned PS5 payload SDK and host zlib.
3. Reproduce the clean-room runtime twice, requiring byte identity and its
   recorded checksum; build the native executable converter.
4. Build RADV release, including native mesa_clc/vtn_bindgen2; apply the
   existing VLC HDR/refresh patches and rebuild RADV.
5. Acquire VLC 3.0.24; cross-compile **all** existing dependency recipes,
   including FFmpeg, GnuTLS, subtitle, audio/container and network libraries.
6. Build static libVLC and its complete configured plugin list.
7. Compile/link PlexStation's real platform/player/UI/client source, convert
   the resulting x86-64 ELF, sign eboot.bin, and create dist/PPSA85301.
8. Check title identity, required files, actual ELF/SELF formats, nontrivial
   executable size, native converter inspection, linked video-output plugin,
   runtime imports, absence of runtime credentials and self-contained files.
9. ZIP the **whole** PPSA85301 installation directory and write its SHA-256.

No host-test executable is substituted for eboot.bin. A failed compile, link,
signature check or package check fails the job. The link script now captures
linker exit status directly; a quiet successful link cannot fail because grep
found no warnings. Native scripts use the installed LLVM utility names rather
than assuming Ubuntu's `-18` suffix. Application code and prior test results
are unchanged by this build task.

## First successful native artifact

[Run 39633799498](https://github.com/tattedbeardo89/PlexStation/actions/runs/39633799498)
completed successfully in **28m 14s** on 2026-10-21 at commit
`dcf18ddaf5d98b0e941ee0106977cf9b1e833b03`. Both installation and diagnostic
uploads succeeded. Its installation artifact is
`PlexStation-PPSA85301-dcf18ddaf5d98b0e941ee0106977cf9b1e833b03` (**37.2 MB**).
The GitHub outer artifact digest is
`139826a6202163f2c7661849e62cb1c83c0c806b9b95795cbeb7bb98652ad5f5`;
use the included SHA256SUMS for the inner installation ZIP.

The workflow's native gates passed; this result was confirmed from GitHub's
successful job/step states and artifact listing. Downloading artifacts requires
GitHub sign-in, so the artifact was not independently downloaded in the cloud
environment. No hardware launch or playback test has been performed.

A second successful [run 39634054358](https://github.com/tattedbeardo89/PlexStation/actions/runs/39634054358)
at `f4a9666fa071572e808c6d5ab1ed43d29362e05d` exercised the refined cache.
Its job took **2m 44s** (the run's longer total includes time queued).
Its complete installation artifact is
`PlexStation-PPSA85301-f4a9666fa071572e808c6d5ab1ed43d29362e05d` (37.2 MB),
with outer artifact SHA-256
`b5a4586d63658659d29883e24f1ecc04cc72eb6056370b9bcc488f89701e27d9`.
This is the newer validated artifact; the application source is identical.

## Trigger and download — Windows browser

1. Open <https://github.com/tattedbeardo89/PlexStation/actions>.
2. Select **Build PlexStation PS5**.
3. Click **Run workflow**, select the branch containing the workflow (currently
   `feature/plexstation-initial-implementation`), then **Run workflow**.
4. Open the run and inspect **Native PS5 title (Arch container)**. A clean build
   may be lengthy; the configured job limit is 330 minutes. The first successful
   cold build completed in 28m 14s on 2026-10-21.
5. Only after the job succeeds, under **Artifacts**, download
   `PlexStation-PPSA85301-<commit SHA>`. GitHub requires sign-in to download.
6. Extract the GitHub artifact ZIP, then extract the contained
   `PlexStation-PPSA85301.zip`. The result must contain the **PPSA85301** folder
   with `eboot.bin`, `sce_sys`, `sce_module`, fonts, certificates and imports.
7. Inspect `validation.json`, `SHA256SUMS`, source hashes and package/provenance
   records. Download the separate `PlexStation-build-diagnostics-<SHA>` artifact
   for full logs; it is also produced on failed builds where Actions permits.

The workflow also runs automatically for pushes changing native-build inputs.
`workflow_dispatch` must exist on the repository's default branch to appear in
GitHub's manual-run UI; the feature branch is currently that default. If you
later change the default branch, merge this workflow there to retain the button.

The job uses read-only repository permissions and no Plex credentials. GitHub
Actions must be enabled in repository Settings > Actions > General. Private
repository minutes, artifact storage quotas or organization policies may apply.
Artifacts expire after 14 days. Logs are not installable applications. Failed
runs never upload the success-only installation artifact.

## Cache behavior

The dependency cache is keyed by the full content hash of the image, lock,
build orchestration, cross-build recipes, compatibility code and patches.
Restoration is restricted to the exact dependency fingerprint. A per-run
checkpoint suffix lets later successful builds replace partial checkpoints
without attempting to overwrite an immutable GitHub cache. Caches also retain interrupted build checkpoints under the exact fingerprint.
An unfinished media prefix is rebuilt because its completion stamp is absent;
the upstream SDK and RADV scripts verify their own pinned provenance before reuse. PlexStation's own title is rebuilt on every run,
and a previous dist/PPSA85301 is deleted before compilation. Source caches
and generated binaries are ignored by Git. Ninja/Make parallelism is limited
to two to reduce memory pressure; build logs remain available on failure.

## Optional Windows 11 / WSL2 alternative

Use the **same** Arch container if hosted runner time, memory, quota or policy
prevents practical builds. This is a provided fallback, not evidence that
GitHub-hosted runners cannot build the application.

1. Enable/install WSL2 from an administrator PowerShell:
   `wsl --install -d Ubuntu`, then restart if requested.
2. Install Docker Desktop, enable its **WSL2 backend**, and choose Linux
   containers. Allocate at least 12 GiB memory and 40 GiB free disk for the
   complete toolchain; actual requirements must be measured. Two build jobs
   are used by default. A laptop with insufficient RAM/disk may still fail.
3. Install Git for Windows. In a normal PowerShell:

   ```powershell
   git -c core.autocrlf=false clone --branch feature/plexstation-initial-implementation https://github.com/tattedbeardo89/PlexStation.git
   cd PlexStation
   powershell -ExecutionPolicy Bypass -File ci/build-windows.ps1
   ```

   The process-policy option applies to that script process, not a permanent
   system policy change. Alternatively run the following Docker commands
   directly if local script policy forbids PowerShell scripts:

   ```powershell
   docker build --progress plain -f ci/Dockerfile.ps5 -t plexstation-ps5:locked .
   docker run --rm --init -e BUILD_JOBS=2 --mount "type=bind,source=$($PWD.Path),target=/src" --mount type=volume,source=plexstation-ps5-cache,target=/build plexstation-ps5:locked
   ```

4. Retrieve `artifacts/PlexStation-PPSA85301.zip` and the report/logs from your
   Windows checkout. Linux build state remains in a Docker named volume.

Docker Desktop/WSL2 must have HTTPS access to the fixed Arch archive, GitHub
releases and each official source URL in the dependency recipes. Never disable
TLS/signature checks to work around blocked downloads. The Windows path has
not been executed on a Windows machine during this task.

## Deployment and compatibility

Follow docs/BUILD-AND-DEPLOY.md. Copy the complete **PPSA85301** folder to an
unused `/data/homebrew/PPSA85301` via your existing ShadowMountPlus method.
Do not overwrite VLC PPSA85300 or replace kstuff Lite. Target: firmware 13.40,
kstuff Lite, ShadowMountPlus 1.7 Beta 5 Fix 1. Successful compilation and package
inspection do not establish launch, decoder, A/V-sync or firmware compatibility.
The owner has confirmed launch/UI rendering of run 39634054358 after a 777
permission workaround; controller input failed. See
[controller and Windows FTP permission instructions](CONTROLLER-TESTING.md) for
the corrected packaging and pending navigation/playback checks.
