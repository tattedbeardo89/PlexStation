# Architecture and implementation boundaries

## Upstream inspected

Imported ZiZc3/VLC commit a91113b9731241dbfc159fd671290ff42afb2db3.
`src/main.cc` owns the frame loop, `gfx.cc` the Vulkan render pass and uploads,
`platform_ps5.c` DualSense, display, audio and runtime imports. Linux uses
`platform_host.c`: an offscreen Vulkan renderer and scripted pad, with audio
output discarded. It is not a desktop TV-player preview.

`player.cc` owns a single libVLC player. Video callbacks write frames into
host-visible Vulkan buffers. Audio callbacks use VLC presentation timestamps
and a paced platform audio thread. `ps5/modules/ps5vdec.c` implements console
hardware decoding. `ps5http.c` implements native HTTP access, with VLC's own
HTTP backend as fallback. `patches/` modifies VLC and dependencies. The build
uses static plugin tables from `ps5/plugins.txt`. Existing decoding, rendering,
audio timing, track selection, and libVLC delay APIs are preserved.

Original `ui.cc` is retained for reference but no longer linked. PlexStation
builds `plex_ui.cc`, a separate controller-first ImGui UI. Application identity
is PPSA85301, distinct from VLC PPSA85300. This is a development title ID;
check for local collisions before installing. Application data is within its
own /app0, with /data/plexstation as its optional media folder.

## New layers

- `plex/client.*`: typed media/resource parsing, device PIN requests, server
  endpoint verification against machineIdentifier, page requests, progress,
  watched state, conservative direct-play URL generation, credential files.
- `plex/transport_curl.cc`: Linux transport, proxy-aware libcurl, verified TLS,
  no automatic redirects, 5 s connect / 15 s whole-request timeout, 8 MiB limit.
- `plex/transport_ps5.cc`: GnuTLS/socket transport, verified app CA store and
  hostname, bounded connect/record waits and response size. Compiled and linked
  in successful native PS5 builds; owner-confirmed authentication on build 2458e45; new profile flows still need acceptance. DNS resolution
  runs on a worker but OS resolver duration remains platform-dependent.
- `plex/artwork.*`: single asynchronous poster request/decode, server-sized
  images, max 640x960 decode, max 12 cached GPU textures, LRU eviction. GPU
  upload/free stays on the render thread. Obsolete account generations are
  discarded. This is an in-memory cache, not a persistent disk cache.
- `plex_ui.cc`: asynchronous metadata tasks, device linking, server selection,
  libraries, show/season children, details, title search, manual endpoint
  keyboard, paging, original-quality playback request, pause/seek/track
  controls and live +/-500 ms audio offsets in 25 ms steps. Global audio
  defaults and HTTP preference use the existing prefs layer.

## Security and current limits

Credentials use a stable random device identifier, atomic writes and mode
0600, with ownership and symlink checks on read. This is filesystem protection,
not hardware-backed encryption. PS5 filesystem permission behavior and random
source availability require device verification. Sign-out clears local tokens;
it does not revoke the account's device authorization remotely. Do not include
account files in backups or releases.

Requests send Plex tokens as headers. VLC playback currently uses a tokenized
media-part URL, so unstructured VLC logging is suppressed and player-open
messages redact URLs. No token is printed by the new client. Raw HTTP is
explicitly opt-in; automatic discovery accepts it only for local-marked
connections. Manual HTTP is the user's explicit choice and sends credentials
unencrypted. Redirects are refused to avoid forwarding authorization to an
unexpected host. Remote Plex endpoints need environment network access to
that actual hostname; do not add a global wildcard speculatively.

Only Original-quality single-part H.264/HEVC MKV/MP4 direct-play is implemented.
It is a requested mode, not proof of active decoding or working playback.
Direct Stream, transcoding decisions, bandwidth adaptation, external subtitle
retrieval and subtitle appearance UI are not implemented. Existing libVLC
embedded track controls are exposed. No JWT flow is claimed: device PIN
requests now use non-strong TV PINs, corrected browser-auth QR URLs and a tested
asynchronous lifecycle. See AUTHENTICATION.md for accessible references and
official-site access limits; owner-confirmed primary-account authentication is distinct from pending Plex Home acceptance.

The native TV UI includes actual Home carousels, cast/genre presentation,
basic sorting/filtering and account/profile-scoped automatic server reconnection.
The full requested Plex experience remains incomplete: cloud Watchlist, advanced
filters, automatic next episode, per-item offsets and full localization are not
implemented. Progress updates are coalesced at 10-second intervals and on state
changes; a failed update is reported, but an offline retry queue is not
implemented. New Home/profile flows have fixture coverage, with live PMS and
physical acceptance still pending. See TV-CLIENT-STATUS.md for the full matrix.


Plex Home now keeps the primary linked token in the existing private credential
file and switched tokens in memory. Fresh validated profile identity scopes
resource discovery, server preferences and timeline/playback authorization.
PIN entry suppresses raw/focus input diagnostics. Home feeds use actual
Continue Watching/server hubs and bounded supplemental metadata queries.
There are 12 cached textures, one asynchronous fetch/decode at a time, no
bulk library image preload, and generation checks on screen/profile changes.
See PLEX-HOME.md and CRASH-DIAGNOSTICS.md for protocol references, matching debug
artifacts and the still-unconfirmed physical abort diagnosis.
