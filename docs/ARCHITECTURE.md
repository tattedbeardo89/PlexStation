# Native architecture and evidence boundaries

Current feature inventory and corrections: [full implementation audit](FULL-IMPLEMENTATION-AUDIT.md).
Historical implementation snapshots in other documents describe their named
commits, not the current tree. The owner's accepted visual direction is dark
navy/charcoal with indigo/blue accents, superseding the original gold spec.

## Preserved playback and platform

Imported ZiZc3/VLC foundation `a91113b9731241dbfc159fd671290ff42afb2db3`.
`main.cc` owns the frame loop; `gfx.cc` owns Vulkan rendering and uploads.
`platform_ps5.c` provides DualSense, display and timestamped audio integration;
`ps5/modules/ps5vdec.c` provides hardware decode. `player.cc` owns one native
libVLC player with video/audio callbacks, track APIs, seek, rate and delay.
The audio callback requests 48 kHz signed 16-bit PCM; lossless source playback
does not establish bit-perfect output. `platform_host.c` is a test backend,
not a real desktop media-player acceptance environment.

The title remains PPSA85301, separate from upstream VLC PPSA85300. Existing
codec/decoder, Vulkan and audio-clock paths are retained. No browser runtime.
`ui.cc` is historical upstream UI; PlexStation links `plex_ui.cc` instead.

## Application layers

- `plex/client.*`: typed metadata, numeric bounds, JSON/XML account/profile
  responses, PIN auth, Plex resources, endpoint identity and authenticated
  library checks, paging, timeline/watched requests and direct-play adapter.
- `plex/transport_curl.cc` and `transport_ps5.cc`: bounded HTTP, verified TLS,
  hostname/CA checks and no general redirect following. The native backend
  uses GnuTLS/socket I/O. DNS resolution duration remains OS-dependent.
- `plex_ui.cc`: async job dispatch with screen/profile generation rejection,
  profile/PIN controller navigation, verified cached endpoint startup, scoped
  warm Home data, shared sidebar, poster grid, carousels, tabs and details.
- `plex_video_ui.h`, `subtitles.h`, `subtitle_latch.h`: session/default subtitle
  selection, unique language/title mapping to actual VLC IDs, external text
  attachment with cancellation and no reopen, verified active selection, Off
  across seek/pause. Plex stream IDs are never passed as VLC IDs.
- `streaming.h`: original single-part H.264/HEVC MKV/MP4, PMS copy/transcode
  decisions and HLS output with known codecs; separate local/remote quality
  caps. Automatic is a deterministic policy, not measured bandwidth adaptation.
  Burned subtitles and unknown decisions are rejected. Native runtime streaming
  and session cleanup remain live/device acceptance requirements.
- `audio.h`, `plex_audio_ui.h`, `music_feed.h`, `listening.h`: audio-only native
  playback, artists/albums/tracks/playlists, bounded session queue, shuffle and
  repeat. Audiobooks are explicitly designated music libraries, with separate
  local book progress/bookmarks, chapter/rate/timer controls and server timestamp
  reconciliation. No invented server audiobook type or normalized narrator API.
- `home_feed.h`, `navigation.h`, `search.h`: actual permitted server hubs,
  multi-library supplemental rows, default section IDs, categories and search.
- `cloud.h`, `plex_cloud_ui.h`: real Plex Cloud Watchlist read/write endpoints,
  filter/page/sort and GUID-to-local resolution. Managed-user cloud access is
  disabled; no owner's cloud token is substituted. Provider acceptance pending.
- `live_tv.h`: authorized personal DVR/provider channel/guide/tuning adapter.
  Free cloud TV has a separate accurate unavailable state; its supported provider
  adapter is unestablished. Rentals routes/UI are removed.

## Image storage and GPU lifetime

`artwork.*` runs bounded image downloads/decodes on two bounded media lanes plus an independent avatar lane;
GPU uploads/frees stay on the render thread. Poster and landscape requests use
PMS image sizing. Up to 64 entries and 64 MiB retained RGBA are allowed; recently
visible images and avatars receive eviction protection. `library_free_image`
waits for submitted GPU work. Same-frame clears defer destruction to update.

`cache.h` persists validated compressed images with hashed scope/keys, owned
private directories/files, bounded reads, atomic writes and LRU byte/entry
limits. Scopes include account/profile/server; avatars have a separate primary
account cache. PS5 prefers `/data/plexstation-client`, falling back to the app
path when needed. Disk persistence/permissions still require console testing.
Corrupt cache entries are discarded and repaired. Absolute approved image URLs
resolve directly, with cross-origin credentials withheld; parent/series fallback
fields survive parsing and Home serialization. Automatic attempts are capped at three. Valid disk copies are read
without redownloading. PNG/JPEG avatars are decoded and bounded to 256 square;
WebP/SVG are unsupported. Avatar redirects are limited to permitted Plex HTTPS
origins, at most three hops, with credentials withheld from image CDNs.

## Persistence, privacy and validation

Account credentials are mode 0600, ownership/symlink checked and atomically
written. Primary token persists only in the private account file; switched
profile tokens stay in memory. PINs are masked and cleared; PIN input logging
is suppressed. Profile restore never bypasses a required PIN. Cached Home is
presented only after current identity and authenticated library access checks.
Tokens/playback URLs are absent from preferences, cache keys and public logs.
Native player logging redacts authenticated sources. HTTP is explicit local
opt-in; no TLS downgrade or certificate bypass.

`plex_progress_ui.h` and `progress_outbox.h` coalesce timeline requests and keep
a token-free scoped durable retry queue with freshness reconciliation. Local
book progress is distinct from music and per-track server progress. Live TV
must not generate library timeline/scrobble updates.

Host fixtures validate application behavior using fake player/GPU/services;
transport suites use real loopback HTTP/TLS. ASan/UBSan catch host memory errors.
GitHub Actions `build-ps5.yml` performs real compilation, SELF conversion,
package validation and matching debug collection. Only the owner's device tests
establish firmware/controller/codec/display/avatar/synchronization acceptance.
See [validation](VALIDATION.md) and the audit for exact evidence and remaining
limits, including bandwidth adaptation, localization and external service APIs.
