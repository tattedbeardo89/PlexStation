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

No reference screenshots were attached with this request; written layout and
palette requirements guide the implementation. No physical behavior is inferred
from fixtures. New runtime logs must remain bounded and redact private data.
