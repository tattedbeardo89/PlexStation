# Remaining-features implementation and acceptance record

> Historical October 23 checkpoint. The [October 24 full implementation audit](FULL-IMPLEMENTATION-AUDIT.md)
> supersedes its current-state statements, including seasonal selection, stable
> library-pin reordering, category routing and supported DVR recording controls.


The owner physically accepted launch, primary authentication, server connection,
native video, subtitles initially Off, and normal JSON numeric parsing at
`5d8adb9ca74d051b8154e01dafb0ad0848c372f7`. Those are owner observations about that
build, not evidence for the changes below. The current cloud checkout initially
contained `0675991`; the exact accepted tree and commit were recovered and
verified before implementation. No playback engine was replaced.

## Source candidate

| Requested checkpoint | Implemented source paths | Remaining acceptance or limits |
| --- | --- | --- |
| A2 subtitles | Details selector before playback; Options / Tracks / Subtitles during playback; Off, Automatic, preferred language, metadata labels, forced/hearing-impaired flags, native tracks, supported external tracks; visible selection; profile defaults separated from media/session choices | Native runtime and physical playback verification pending. Embedded mapping requires a unique native language/title match; ambiguous mappings fail to Off with a message. Native playback menu permits choosing actual VLC IDs. Transcoded embedded switching is unavailable; choose Original. No stream IDs are assumed interchangeable. |
| A2 lifecycle | Apply after discovery, verify native active ID, preserve Off across seek/pause, cancel obsolete external loads, disable external-load automatic reopen for Plex selections, bounded failure feedback | External download/attach and subtitle renderer still need physical acceptance. Transcode requests disable server subtitles; server decisions that burn subtitles are rejected. |
| A2 preferences | Settings / Playback / Subtitles: default Off, Automatic, preferred language, forced metadata preference, optional per-media memory, size/delay and reset | New v3 schema deliberately starts Off. Selection memory is scoped by account/profile/server/media/version, using stream identity rather than a global track number. Forced preference needs one matching track. |
| A2 avatars | Preserve XML avatar/avatarUrl fields, per-image pending/failure UI, safe HTTP/TLS/redirect/format/byte/decode/GPU diagnostics with opaque request numbers, explicit Retry Avatar, independent disk scope and stronger GPU residency | **Physical root cause is not established.** No owner's failing device log was supplied. WebP/SVG remain unsupported by stb. Real PNG/JPEG decode, redirect isolation and fake GPU tests do not prove the PS5 displays an avatar. |
| A2 artwork | Existing bounded disk/GPU cache retained; corrupt full-decode validation and repair, avatar cache separate from posters, disk/GPU/download/decode/eviction/write/upload counters, protected visible textures and deferred destruction | Disk cache uses private `/data/plexstation-client` on PS5 when available, otherwise the title directory. An unwritable path is counted as a write failure. Persistence and GPU fences require device retest. |
| B startup | After profile/PIN authentication, try the previously verified endpoint with 800 ms connect / 1400 ms request limits; verify server identity and libraries using current profile authorization, fall back to discovery; scoped cached Home; idle background resource refresh; Clear Cached Connection | DNS resolution can still exceed socket deadlines. Actual cold/warm PS5 timings are unavailable. Host warm fixture: two bounded requests, rather than full discovery first; one observed completion was 0.075 ms after entering the fixture wait, which is not network or device latency. |
| C Home | Existing native carousel layout retained and extended: released movies, added movies, released episodes, added shows; series/season/episode metadata retained during aggregation; focused item survives matching background row refresh | Reference screenshots were not available in this session. No screenshot-equivalence or physical visual acceptance claimed. |
| C Watchlist | Actual Plex cloud provider, grid, account/profile restrictions, All/Movies/TV filters, added-date/title sorting, pagination and real response count; supported add/remove/local resolution retained | Cloud provider filters and count behavior need live validation; unavailable cloud access is explicit. No fabricated local Watchlist or cloud Recommended page. Official metadata-static/provider-static image URLs can use the authorized PMS image transcoder; other cloud image origins still use placeholders. |
| C libraries | Default Movies/TV IDs scoped per server/profile, deterministic accessible fallback, Recommended/Library/Categories, paged library browse, real genre categories and server collection rows | Recommendations use actual selected-library queries/hubs; not every server exposes every row. Categories currently emphasize genres. |
| D Home Movies | Dedicated sidebar section found by real library ID/name, remembered routing, own tabs; personal videos excluded from professional Movies defaults and release recommendations | Thumbnail/date/duration presentation and all controller paths need device review. |
| D Music/Audiobooks | Shared top tabs, remembered library selection, real feed and category paths; existing queues/shuffle/repeat, bookmarks/resume/speed/chapters/timer and explicit audiobook designation retained | UI remains less complete than video; some routes remain lists. First selection can still show a library chooser. No semantic conversion of ordinary music to audiobooks. |
| E seasonal/sidebar | Halloween/Horror and Christmas/Holiday from real permitted genre tags on library Recommended pages; seasonal toggle; profile-specific optional visibility, ordering, reset, and accessible video-library pins | Home seasonal rows now query up to four permitted nonpersonal libraries using actual genre/collection matches. Winter/summer rows need matching server tags or collections; arbitrary season definition editing is not implemented. Library pin reorder beyond built-in entries is not implemented. |
| E Live TV/Rentals | Personal DVR and EPG provider discovery, up to 200 authorized channels, today's actual current/upcoming programs, channel logos, explicit tune and supported session HLS via the original player; protected/unknown codecs rejected; separate free-TV and Rentals limitations | Personal Live TV is source/fixture tested, not tuner/device verified. No schedule editing/recording, multi-day guide paging, verified idle-tuner cleanup or free cloud Live TV. Rental catalog/entitlement integration and licensed DRM remain unavailable. |
| F branding | Original SVG master and 512-square PNG icon, 1920x1080 PNG background masters, reproducible DXT1 DDS packaging, original in-app symbol | Actual installed icon and PS5 shell cache refresh require device verification. PPSA85301 and saved data are retained. |

## Service boundaries and evidence

Checked the official [PMS API](https://developer.plex.tv/pms/) on October 23, 2026.
Its OpenAPI 1.2.3 documents `/media/providers`, `/livetv/dvrs`, provider EPG channel
and grid queries, channel tuning, and session playlists. These API surfaces are used by the personal Live TV source adapter. A generic
unavailable screen is not treated as completion of personal Live TV. Provider/grid endpoints require the advertised provider and
current server/profile authorization; do not assume free cloud Live TV shares
the same authorization or playback flow.

The official [rental FAQ](https://support.plex.tv/articles/faq-rentals-purchases-on-plex/)
states rentals use DRM and require compatible DRM/HDCP support. This client has
no licensed DRM path or verified supported rental catalog/entitlement API.
Use an authorized Plex application for protected rental playback. No prices,
entitlements, or purchase controls are fabricated. Personal tuning closes the
native HTTP consumer on stop; an explicit server-side idle tuner release has not
been validated. No DVR configuration, guide reload, recording or channel mapping
mutation is introduced.

## Build and host validation

Run the existing host scripts, including `test-subtitles`, `test-artwork`,
`test-controller`, `test-home`, `test-discovery`, both transports, connections,
core/auth/audio/streaming/cache/crash/numeric checks, strict syntax, and
`test-sanitizers`. Logs are kept separately from public source. Host fixtures
use synthetic accounts and fake player/GPU boundaries, with real image decode,
filesystem caches, HTTP/TLS transports and ImGui draw lists where applicable.

The first cloud native attempt failed before compilation because
`/workspace/deps/PS5_Vulkan/.deps/native/radv-release/lib/libvulkan_radeon.ps5.a`
is absent. GitHub Actions uses the established pinned native build and dependency
cache. An Actions link is not a passing build; installation and debug artifacts
must both exist for the exact commit before reporting native success.

Branding: `assets/plexstation.svg`, `ps5/sce_sys/icon0.png`, PNG background masters,
`tools/make-brand.py` (authoring, Pillow, including the existing DXT1 DDS format). Build packaging validates the icon/background dimensions.
The install artifact is `PlexStation-PPSA85301-<sha>` and matching symbols are
`PlexStation-native-debug-<sha>`. Do not install diagnostics as an application.

## Physical PS5 checklist — all new behavior awaiting owner verification

Record build SHA, media version/codecs, Direct Play or server streaming, and profile.

- [ ] Install complete PPSA85301 folder without overwriting PPSA85300 or deleting account data.
- [ ] Confirm launch, primary auth, Home/PIN switch, server connection and native playback remain working.
- [ ] Fresh unrelated movie/episode starts Off; resume starts at the correct position.
- [ ] Before playback: Off, Automatic, each language, forced/hearing-impaired labels, media version change.
- [ ] During playback: enable, disable, switch language; confirm no restart and continuous audio/position.
- [ ] Off stays Off after seek, pause/resume, late external download, stop/replay and another movie.
- [ ] Embedded/external, absent/ambiguous track, failed external download and server-streamed media report correctly.
- [ ] Profiles keep separate defaults/media memory and cannot view another profile's cached Home.
- [ ] Who's Watching, sidebar, profile/settings and Switch User show real avatars; capture safe request-number diagnostics if not.
- [ ] Retry Avatar works after outage; PNG/JPEG succeeds or identifies decode/upload failure. WebP/SVG limitation is explicit.
- [ ] Scroll, focus, leave/return and relaunch with network disconnected: valid posters persist; cache usage/hits change.
- [ ] Memory pressure never destroys an image referenced by submitted GPU work.
- [ ] Measure profile-submit → authorized Home for cold and warm startup (five runs each); repeat server offline/route change.
- [ ] Home background refresh keeps row/card focus and real active-profile progress.
- [ ] Movies/TV default libraries and Change Library persist; Home Movies routes correctly.
- [ ] Watchlist counts/filter/sort/pages and add/remove match Plex's authorized account.
- [ ] Music queues and audiobook bookmarks/chapters/speed/timer retain their distinct behavior.
- [ ] Sidebar hide/order/reset/pins, seasonal toggle, 1080p/4K layout and rapid controller navigation.
- [ ] Personal Live TV: authorized DVRs/channels, logos, current/upcoming guide times, tune H.264/HEVC, pause/stop/return, tuner release, no library-progress writes; test a denied profile and unsupported/protected channel.
- [ ] Free cloud Live TV and protected Rentals show accurate native-client limitations.
- [ ] Icon/splash use angular branding. If the PS5 shell shows its cached old icon, fully close the title and restart the console/launcher normally; do not delete saved data or system databases.

No A2–F checkpoint is physically accepted by host tests. Remaining gaps above
must stay visible in release notes.

## Resume audit and A2 control-path follow-up

The resumed environment contained a clean, older `work` branch at `0675991`.
Fetched the requested feature branch and continued its existing history at
`fe9c0c4a348c0c02fe79e2d45e7c3abda16269a8`; both `5d8adb9` and `1ad4f6c`
remain ancestors. The source/acceptance inventory above remains applicable.

Verified GitHub metadata for [native run 40039598122](https://github.com/tattedbeardo89/PlexStation/actions/runs/40039598122):
actual native compile, matching debug collection, packaging and uploads passed.
Its installation artifact is `PlexStation-PPSA85301-fe9c0c4a348c0c02fe79e2d45e7c3abda16269a8`
and symbols are `PlexStation-native-debug-fe9c0c4a348c0c02fe79e2d45e7c3abda16269a8`.
These existing artifacts predate the following fix.

Playback Information's legacy external-subtitle action bypassed the shared
subtitle session policy and allowed the player's reopen fallback. It now opens
the same embedded/external subtitle menu used by Options / Tracks. The actual
controller fixture checks that entering this menu preserves the active track,
playback URL and playback position. External choices therefore use the shared
bounded, cancellable attachment path with reopening disabled.

Physical acceptance remains pending: test both menu entry points, choose an
external track, select Off while it loads, seek/pause, and verify uninterrupted
video/audio and correct captions. Avatar diagnosis and persistent artwork
retention still require device observations; no physical root cause or fix is
claimed from host fixtures.

Resume follow-up local checks passed: 74 core assertions, ten libcurl transport
scenarios, subtitle policy, artwork decode/cache/lifecycle, persistent cache,
production controller/UI including the alternate subtitle entry, PS5 pad mocks,
ZIP permissions/theme boundary, ten native GnuTLS transport scenarios and strict
native-source host syntax. Loopback fixtures require the execution sandbox's
network permission. Hosted native and sanitizer gates are reported separately.

### Resume follow-up native result

[Run 40060131391](https://github.com/tattedbeardo89/PlexStation/actions/runs/40060131391)
completed successfully for `208b5be1ec73f6fa11797dbd3ae2b21262eafce3`.
Full hosted host/sanitizer regressions, actual native compilation/validation,
matching debug collection, package permissions and artifact uploads passed.

- [Installation artifact](https://github.com/tattedbeardo89/PlexStation/actions/runs/40060131391/artifacts/12546553507):
  `PlexStation-PPSA85301-208b5be1ec73f6fa11797dbd3ae2b21262eafce3`, 39,460,749 bytes;
  outer artifact SHA-256 `a04c326412d6b72090b460569675919c857eb12816aae3f39187e42ba6b14405`.
- [Matching symbols](https://github.com/tattedbeardo89/PlexStation/actions/runs/40060131391/artifacts/12546688166):
  `PlexStation-native-debug-208b5be1ec73f6fa11797dbd3ae2b21262eafce3`, 92,656,101 bytes;
  outer artifact SHA-256 `ed40a1615f5803caa92c6c3ff5ecc864e1027f4cd4f9878c1f1f3409d4206bc9`.

Evidence is GitHub run/job states and artifact metadata; these archives were
not downloaded or independently binary-inspected during this follow-up.
Physical PS5 acceptance of this build remains pending. Preserve runtime account,
preferences, listening and progress files when installing the complete title.
The documentation-only commit recording this result does not change the built
source or start another native build.
