# PlexStation

Current stabilization candidate: [PS5 UI/artwork/startup changes, measurements and acceptance evidence](docs/PS5-STABILIZATION.md).

Native PS5 Plex client built on [ZiZc3/VLC](https://github.com/ZiZc3/VLC),
using the existing native VLC decoder, Vulkan renderer and timestamped audio.
The owner confirms launch, Plex authentication, Home profile selection, server
connection, native video and subtitles initially Off at `5d8adb9`. The earlier
working baseline `1ad4f6c4ab562762bd4c2ffa958cc098e234528a` is preserved.

Current source audit: [full feature matrix, defects and acceptance evidence](docs/FULL-IMPLEMENTATION-AUDIT.md).
[Native build 40063743047](https://github.com/tattedbeardo89/PlexStation/actions/runs/40063743047)
passed for `1078e74cb636cf852ec17dfc221cb551cbbe508b`, with full host/sanitizer
regressions, native compilation, package validation, installation and matching
symbols. [Exact artifact links and checksums](docs/FULL-IMPLEMENTATION-AUDIT.md#final-native-build-and-artifacts--passed).
New behavior still requires the owner's physical PS5 tests.
Historical crash reports remain in the investigation documents; they do not
supersede the owner's newer confirmed working video baseline.

Target: firmware **13.40**, **kstuff Lite**, **ShadowMountPlus 1.7 Beta 5 Fix 1**,
DualSense and independent title **PPSA85301**. Keep VLC **PPSA85300** separate.
No replacement jailbreak, browser player or new rendering framework is used.

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
- Bounded asynchronous poster fetch/decode and a bounded 64-entry / 64 MiB GPU cache and scoped persistent disk cache.
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
- Scoped cached-endpoint fast reconnect, warm Home presentation, library tabs,
  automatic video defaults, Home Movies, seasonal rows and sidebar controls.
- Personal Live TV channel/guide/tune adapter, scheduled recording/rule lists
  and confirmed authorized whole-rule cancellation; a separate accurate free cloud
  TV availability page. Rentals is removed from navigation.
- Original angular SVG/PNG/DXT1 branding and independent PPSA85301 metadata.

These are implemented source paths, with host fixtures and successful native
builds. Only the baseline behaviors described above have owner acceptance.
Subtitle changes, avatars, artwork retention, new Home/navigation, music,
audiobooks, streaming and external-service behavior need device/live testing.

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

- [Full implementation audit, corrections and limits](docs/FULL-IMPLEMENTATION-AUDIT.md)
- [Current physical acceptance and safe diagnostics](docs/HARDWARE-ACCEPTANCE.md)

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
