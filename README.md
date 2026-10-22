# PlexStation

Native PS5 homebrew Plex client under development, derived from
[ZiZc3/VLC](https://github.com/ZiZc3/VLC). This repository contains real client
and native UI code, but **is not yet a complete or verified PS5 application**.
A real native PS5 build and complete installation artifact are available from
[native stability, Plex Home and TV UI build 39702538057](https://github.com/tattedbeardo89/PlexStation/actions/runs/39702538057),
built from `d52e5cd39ad6689f1f917179a6d1761132adde03`.
This is the preserved physical-testing baseline, tagged
`baseline/ps5-physical-test-d52e5cd`. New development does not replace its artifacts.
See [artifact names, implemented scope and device test checklist](docs/TV-CLIENT-STATUS.md).
The owner confirms launch, Plex authentication, PMS connection and available
library retrieval on connectivity build 2458e45. Entering Movies then aborts on
that build. Separate ShadowMountPlus SIGSYS evidence is not yet correlated to
a build/launch. No physical crash resolution or playback acceptance is claimed.

Target: firmware **13.40**, **kstuff Lite**, **ShadowMountPlus 1.7 Beta 5 Fix 1**.
Launch/UI rendering is owner-confirmed; input and playback need verification. PlexStation
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
- Original single-part H.264/HEVC MKV/MP4 playback, plus asynchronous PMS
  copy/transcode decisions, local/remote quality caps and HLS fallback paths.
- Native Music artists/albums/tracks/playlists, content shelves, audio-only
  Now Playing, bounded session queues, shuffle/repeat and a mini-player.
- Profile/server-scoped audiobook library designation, local resume/bookmarks,
  chapters/track fallback, requested speeds and a session sleep timer.
- Actual grouped PMS search, multiple-library Home aggregation and genre/year
  filter values; conditional real Plex Cloud Watchlist with local GUID resolution.
- Existing VLC decode, Vulkan presentation and timestamped audio foundation.
- Native pause, seek, embedded audio/subtitle selection and live audio delay
  from -500 to +500 ms in 25 ms steps; persistent global delay preference.
- Coalesced timelines, token-free durable retry/reconciliation, configurable
  completion thresholds and cancellable next-episode autoplay.
- Original PlexStation icon/loading assets and independent title metadata.

These are implemented source paths. Native compilation, linking and package
checks passed in Actions, and the owner confirmed PS5 launch/UI rendering.
Plex linking, PMS connection and library discovery are owner-confirmed on the
older build. Full browsing, revised profile/Home flows, playback and audio sync
still require acceptance.

## Incomplete features

New media features are source implementations with fixture coverage; live PMS
stream negotiation, cloud provider access/writes, managed-user permissions,
physical audio/video/subtitle/seek/speed behavior and native stability require
acceptance. No codec is claimed audible/bit-perfect on PS5 from compilation.
The audio callback uses 48 kHz S16 output. Bandwidth measurement/adaptation,
normalized audiobook narrator/series support, managed-user cloud Watchlist,
full localization and final visual polish remain incomplete. Bookmarks and
book-level completion are explicitly PlexStation-local. See the comprehensive
[media checkpoint status, codec matrix and limits](docs/MEDIA-EXPANSION.md).

## Documentation

- [Music, Audiobooks, streaming and discovery checkpoints](docs/MEDIA-EXPANSION.md)
- [Full requested project specification](docs/PROJECT-SPEC.md)
- [Cloud setup state](docs/CLOUD-SETUP.md)
- [Architecture and limits](docs/ARCHITECTURE.md)
- [Build, configuration and ShadowMountPlus deployment](docs/BUILD-AND-DEPLOY.md)
- [DualSense investigation, diagnostics and Windows FTP deployment](docs/CONTROLLER-TESTING.md)
- [TV linking implementation, references and account test procedure](docs/AUTHENTICATION.md)
- [Server connection diagnostics, manual endpoints and PS5 testing](docs/SERVER-CONNECTIVITY.md)
- [Actual validation results and device checklist](docs/VALIDATION.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)
- [Licensing and attribution](THIRD_PARTY.md)
- [Original upstream documentation](docs/UPSTREAM-README.md)

GPL-3.0-or-later for new code; imported components retain their own notices.
See LICENSE and THIRD_PARTY.md. Not affiliated with Plex, Sony or VideoLAN.

Account-scoped automatic PMS reconnection, Change Server and optional controller
overlay settings are documented in [server reconnection](docs/SERVER-RECONNECTION.md),
including the movie-library crash investigation and physical acceptance checklist.

Plex Home users/PINs, isolated profile sessions, personalized Home carousels,
Continue Watching, movie sort/filter controls and native stability isolation
are implemented in the current source. See [delivery status and physical
checklist](docs/TV-CLIENT-STATUS.md), [Plex Home API/isolation](docs/PLEX-HOME.md)
and [crash diagnosis and matching debug symbols](docs/CRASH-DIAGNOSTICS.md).
These source implementations do not establish live Plex or PS5 acceptance.

The f511344 first-fault investigation, numeric-locale parser correction,
Plex Home avatar pipeline and native navy/indigo theme are documented in
[the stability and theme testing guide](docs/SIGILL-AVATARS-THEME.md).
Host/native build validation and physical PS5 acceptance are separate;
see [validation](docs/VALIDATION.md) before interpreting compatibility claims.
