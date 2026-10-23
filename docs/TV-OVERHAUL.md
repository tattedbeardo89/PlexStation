# Native TV overhaul: evidence and checkpoints

The owner confirms build 1ad4f6c4ab562762bd4c2ffa958cc098e234528a launches,
authenticates and plays real video on PS5 13.40 / kstuff Lite /
ShadowMountPlus 1.7 Beta 5 Fix 1. This is the rollback baseline, run 39876566427.
The source tag and separate verified artifact archive preserve it without a
rebuild. Later changes require new physical acceptance.

## Initial source audit

Existing: native VLC playback/audio timing, JSON locale fix/self-test, Home/PIN
profiles, TLS/identity/library verification, preferred server ID, scoped local
progress, Home carousels, typed audio queues/audiobooks, basic grids/filters,
cloud Watchlist adapters and shared navy/indigo theme.

Partial: subtitle Off is a one-shot numeric selection and startup uses default
language; late track restoration can override it. Artwork is a single-worker,
12-texture LRU with no visible-frame protection and no disk cache. More visible
variants than cache slots cause eviction/redownload churn. Avatar handling has
limited URL/field coverage and incomplete final-stage diagnostics. Server ID is
remembered but profile activation always performs discovery before connection.

Missing: persistent image/metadata/verified-route caches, default media-library
associations, shared section tabs, Home Movies/pinned libraries, customizable
sidebar, Live TV guide and Rentals capability UI, updated angular branding.
Some desired cloud/DVR playback paths require provider capability and DRM
verification; unsupported paths must remain accurately labelled.

## Delivery order

A: subtitle policy and persistent artwork/visible residency/avatar diagnostics.
B: verified endpoint fast path and display-only metadata caches.
C: shared tabs/carousels/grids/default libraries/Home Movies/seasonal browsing.
D: angular native icon, Music/Audiobooks and profile sidebar preferences.
E: researched personal DVR/cloud TV/Rentals capabilities with honest states.
F: full regression/sanitizers, actual native builds, complete package/matching
symbols, evidence and physical acceptance checklist.

Reference screenshots could not be accessed reliably in this environment; written
layout and palette requirements guide the implementation. No physical behavior is inferred
from fixtures. New runtime logs must remain bounded and redact private data.

## Checkpoint A implementation and acceptance

The changes retain baseline `1ad4f6c`'s VLC codecs, decoder, audio timing, title
identity and transport verification. No PS5 acceptance is inferred from host tests.

### Subtitles

New profiles default to **Off**. Off uses VLC's actual disabled track identifier,
keeps late track discovery/restoration from re-enabling captions, and is enforced
while playing after seek/resume/reopen. Playback Options → Subtitles provides
Off, Automatic, Preferred Language, actual VLC tracks, validated external PMS
subtitles, delay and size. Settings → Playback → Subtitles controls defaults and
whether to remember them for this account/profile. Changing audio language no
longer changes the subtitle policy. Explicit track choices apply to the current
file; the next file follows the saved default.

Automatic uses the available VLC subtitle selection; preferred language accepts
common ISO aliases and regional language tags. If a preferred language is not
found during its bounded discovery window, captions remain Off. This VLC API
has no authoritative forced-only flag: a forced-only filter is explicitly
unavailable rather than inferred from titles. Off disables forced captions too.

### Artwork retention and persistence

The old 12-texture LRU could churn when the screen requested more than 12 poster,
landscape and avatar variants. The new 64-entry / 64 MiB GPU cache protects
textures used in the previous/current frame and waits for a safe eviction
instead of deleting visible images. Upload/free remain on the render thread;
decode, network and disk maintenance stay on one asynchronous worker.

Validated compressed images persist in `plexstation-artwork-cache` beneath the
existing application data directory. Default disk budget: 256 MiB, adjustable
64–1024 MiB under Settings → Appearance → Artwork cache. That page exposes
usage, clearing the active scope, and nearby prefetch. Opaque SHA256 filenames
scope keys to account/profile/server, survive endpoint changes and app updates,
and contain no authentication headers or plaintext URLs. Files are 600, directory
700; unsafe ownership/modes and symlinks are rejected. No packaged fonts/assets
are modified. Storage is evicted within the shared byte/count budget; disk hits
are validated again before decode. Keep this data folder when updating the title.

### Plex Home avatars

Profile metadata accepts thumb/avatar/avatarUrl, relative and protocol-relative
Plex image paths and upgrades known Plex HTTP origins to HTTPS. Only known Plex
HTTPS image origins and bounded redirects are accepted; account tokens are never
forwarded to CDN origins. Only the matching owner profile can inherit the
validated owner avatar. PNG/JPEG decode is tested, large avatars are cropped and
bounded to 256 square; unsupported formats retain the existing safe fallback.
The focused profile is fetched first. Bounded diagnostics distinguish missing
metadata, rejected origin, HTTP/MIME/redirect, decode, disk, GPU and display
stages without names, URLs, tokens or response bodies. Physical avatar root cause
and acceptance remain to be established on the owner's server.

### PS5 acceptance checklist

1. Confirm build SHA and normal launch/authentication/video playback first.
2. Start a captioned film: default Off; explicitly enable a track, choose Off,
   seek, pause/resume, reopen and start the next film. Captions must stay Off
   when requested. Test Automatic, preferred language and external subtitles.
3. Browse Home and a large movie library, scroll back, enter details/playback and
   return. Previously visible posters must not disappear under cache pressure.
4. Relaunch without deleting the data folder; revisit the same titles. Check
   disk usage, clear the active cache and verify a subsequent network refetch.
   Switch profiles/servers and verify images never cross those identity scopes.
5. Enter Plex Home profiles; verify owner/managed avatars and focus. If a
   placeholder remains, share only redacted `avatar: phase=` lines.

Host fixtures cover the production policy/cache/decoder and controller UI.
Native workflow compilation/package/debug-manifest gates must also pass before
an installation artifact is reported. Checkpoints B–F above remain separate work;
reference images were not needed for these behavioral changes.
