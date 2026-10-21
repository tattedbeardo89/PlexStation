# PlexStation

Native PS5 homebrew Plex client under development, derived from
[ZiZc3/VLC](https://github.com/ZiZc3/VLC). This repository contains real client
and native UI code, but **is not yet a complete or verified PS5 application**.
A real native PS5 build and complete installation artifact are available from
[successful GitHub Actions run 39633799498](https://github.com/tattedbeardo89/PlexStation/actions/runs/39633799498).
Console launch and playback remain untested.

Target: firmware **13.40**, **kstuff Lite**, **ShadowMountPlus 1.7 Beta 5 Fix 1**.
This exact runtime combination is awaiting hardware verification. PlexStation
uses independent development identity **PPSA85301**; VLC's **PPSA85300** is
preserved. No replacement jailbreak or browser runtime is introduced.

## Build the native title in GitHub Actions

Windows users can trigger **Build PlexStation PS5** in the repository Actions tab.
See [exact workflow, artifact and Windows/WSL2 instructions](docs/GITHUB-ACTIONS.md).
The workflow compiled and validated the actual native title on 2026-10-21.
Prior host-test results remain separate from native build and device acceptance.

## Start development

```sh
cd /workspace/PlexStation
bash host/build-core.sh
```

This builds the C++17 Plex client tests and `build/plexctl`, then exercises the
real libcurl transport against a local HTTP fixture. It does not test native
video playback. Local full-build downloads remain proxy-blocked; the native
PS5 build succeeds on GitHub Actions. See the build guide.

## Current implementation

- Device-link request/polling code and persistent random device identity.
- Credential files with mode 0600, atomic writes and local sign-out.
- Resource parsing and endpoint fallback with selected-server identity checks.
- Asynchronous native ImGui library browsing, show/season children, details,
  controller keyboard search/manual entry, paging and version selection.
- Bounded asynchronous poster fetch/decode and a 12-texture memory cache.
- Adapter for single-part H.264/HEVC MKV/MP4 Original-quality playback.
- Existing VLC decode, Vulkan presentation and timestamped audio foundation.
- Native pause, seek, embedded audio/subtitle selection and live audio delay
  from -500 to +500 ms in 25 ms steps; persistent global delay preference.
- Coalesced timeline requests and manual watched/unwatched requests.
- Original PlexStation icon/loading assets and independent title metadata.

These are implemented source paths. Native compilation, linking and package
checks passed in Actions. Live Plex integration, PS5 launch, codec support
and audio sync have **not** been verified.

## Incomplete features

Transcoding/Direct Stream negotiation, automatic bandwidth quality, watchlist,
full Plex-style home carousels and filtering, external subtitles, next episode,
per-item offsets, completion thresholds, offline progress retries and final
visual polish remain incomplete. The current PIN flow also needs verification
against accessible current official documentation and a real Plex account.

## Documentation

- [Full requested project specification](docs/PROJECT-SPEC.md)
- [Cloud setup state](docs/CLOUD-SETUP.md)
- [Architecture and limits](docs/ARCHITECTURE.md)
- [Build, configuration and ShadowMountPlus deployment](docs/BUILD-AND-DEPLOY.md)
- [Actual validation results and device checklist](docs/VALIDATION.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)
- [Licensing and attribution](THIRD_PARTY.md)
- [Original upstream documentation](docs/UPSTREAM-README.md)

GPL-3.0-or-later for new code; imported components retain their own notices.
See LICENSE and THIRD_PARTY.md. Not affiliated with Plex, Sony or VideoLAN.
