# PS5 UI, artwork and startup stabilization

Integration baseline: `e79d5bd3a882cb6c841de4d0a8cd084bd1eb840a`, the repository's
only remote integration/default branch, `feature/plexstation-initial-implementation`.
Its latest passing native source is `1078e74cb636cf852ec17dfc221cb551cbbe508b`,
[run 40063743047](https://github.com/tattedbeardo89/PlexStation/actions/runs/40063743047).
No installed PS5 build SHA, private live account response, device screenshot or
installed/registered icon file was supplied with this request. The owner's
reported symptoms are physical evidence; host fixtures are not replacements for
console testing. Existing launch/auth/server/native video references `5d8adb9`
and `1ad4f6c` remain ancestors and rollback points.

Changes are isolated on `feature/ps5-ui-artwork-stabilization`. No integration
merge, release publication, PS5 deployment, uninstall, title-ID change or user-data
deletion is authorized/performed by this change.

## Findings and corrections

- `poster` fitted portrait images inside fixed 240×245 rail and 250×270 grid
  regions. A 2:3 poster therefore occupied only part of a visibly wider focus
  container. `presentation.h` now defines exact poster/book 2:3, episode/category
  16:9 and music 1:1 geometry. Shared flow/rail offsets use actual widths, gaps
  and focused-item bounds. `poster` uses proportional cover cropping inside
  those regions, including intentional media-specific placeholders. Mixed rails
  and grids wrap and navigate by measured card positions rather than five slots.
- Native `home_frame` displayed only five fixed-width cards and scrolled by
  `column-4`, which was tied to the wrong widths. Pixel-based shared rail offsets
  now make the first, middle and final focused item fully visible. Controller
  fixtures record actual scaled image/focus geometry at 1080p and 4K.
- `Item` parsing/serialization lost parent/series artwork fields. It now retains
  `parentThumb`, `grandparentThumb`, `parentArt` and `grandparentArt`, including
  across Home aggregation/cache. Episode imagery tries its still, supplied
  backdrops, season/series posters, then an intentional generic episode graphic.
  Category/collection backgrounds take precedence over portrait collection art.
- Many absolute discovery/Watchlist image URLs were rejected or delegated to
  the selected PMS image transcoder. `image_source.h` now resolves approved
  absolute Plex/public metadata-image origins directly, preserves paths/query
  identity, sizes relative PMS images through that PMS, strips authorization
  query parameters and sends server tokens only to the selected origin. Image
  redirects are bounded and revalidated; cross-origin tokens are withheld.
  Unknown external origins are intentionally rejected rather than given tokens.
- Artwork used one asynchronous future for posters, backdrops, avatars and cache
  maintenance. A slow request blocked unrelated visible art. Two media lanes
  plus an independent avatar lane now allow bounded concurrent requests and
  coalesce duplicate in-flight keys. GPU work stays on the render thread;
  visible-texture protection and deferred destruction remain intact. Retries
  stop after three automatic attempts. Valid scoped disk/GPU copies are reused.
  PMS-sized replies retain strict dimensions; approved CDN originals are bounded
  before decode/downsample. PNG/JPEG and existing stb formats are supported;
  WebP/SVG network images still need a supported decoder and produce explicit
  format errors/fallbacks. No physical avatar root cause is claimed without logs.
- Profiles use their actual parsed avatar fields and primary-account image
  authorization. All members have stable initials or the original neutral
  `avatar-placeholder.svg` native vector equivalent while loading/absent/failed.
  Missing source, pending work and failed image stages remain distinct. Protected
  PIN authorization and profile/server cache generations are unchanged.
- Home/Movies/TV combined generated recent rows with differently identified
  server hubs. Shared semantic IDs now distinguish recently added movies,
  added series and released episodes/movies. Explicit distinct library scopes
  remain separate; equivalent aliases and repeated items do not duplicate rails.
  Recently Added TV queries real series (`type=2`, `addedAt:desc`). Released
  episodes use actual original air dates, excluding missing/invalid/future dates;
  import time is never used as a release date. Air dates appear on release cards.
- Home used to wait for all supplemental requests before publishing any data.
  Incremental snapshots now expose usable authorized rows as soon as they are
  available, merge into warm presentation without clearing it, preserve focused
  hub/item identities and reconcile at completion. Sidebar/item actions can
  cancel remaining Home work. Metadata requests use 1500 ms connect / 2500 ms
  request limits; OS DNS duration remains platform-dependent. The verified cached
  endpoint path still precedes broad discovery and preserves TLS, identity,
  profile/PIN and current library-access checks. Obsolete generations cannot
  publish or overwrite a later user's Home cache. Cache schema v3 avoids old
  duplicate presentation snapshots without deleting account/artwork data.
- Rentals route/sidebar/UI is removed. Free Live TV and Personal DVR are separate
  destinations. Music and Audiobooks remain distinct and visible; Audiobooks
  occupies the former Rentals position after Live TV in the default order.
  Versioned sidebar migration preserves stable IDs, maps the old Rentals slot to
  Audiobooks, avoids duplicate audiobook entries and does not apply old Rentals
  visibility to Personal DVR. Library designation remains explicit, persistent
  and profile/server scoped. Ordinary music is not inferred to be books from
  names or paths. Book covers/author/book terminology and filtered chapter search
  retain separate local listening semantics.

No changes replace `player.cc`, `platform_ps5.c`, decoder modules, timestamped
A/V handling, subtitle lifecycle APIs, server failover, DVR tune/recording APIs,
search authorization or numeric-locale/crash instrumentation.

## Startup measurements and reproduction

`main.cc` now logs monotonic native phases: platform, display, ImGui, VLC,
local-library initialization, account/UI initialization and first frame submitted.
Existing profile-submit → authorized-server and populated-Home metrics remain.
These show actual console initialization bottlenecks without logging secrets.
VLC/GPU initialization order remains intact because changing thread-affinity
without device evidence would risk working playback. Independent metadata and
image work runs asynchronously; drawing never waits for every rail's artwork.

[Raw samples and method](STABILIZATION-MEASUREMENTS.json) use the baseline feed
header from e79d5bd and a common host client/harness with a controlled 10 ms delay
per request. Five runs per variant:

| Controlled host metric | Baseline | Incremental |
|---|---:|---:|
| Median first usable metadata | 152.083 ms | 10.243 ms |
| Range first metadata | 152.043–152.135 ms | 10.231–10.265 ms |
| Median all metadata complete | 152.083 ms | 152.377 ms |
| Requests per run | 15 | 15 |

This measures publication responsiveness, **not total PS5 boot speed or live
network throughput**. It does not claim a physical speedup. Reproduce with
`bash host/benchmark-startup.sh` after fetching the baseline object. The production
controller warm-path fixture also checks two bounded authorized endpoint requests;
its CPU timing is not network/device latency. Record five cold and five warm
console runs using `startup:native`, `startup: authorized-server-ms` and
`startup: first-home-ms`, including cache/route flags. Do not include PIN entry
time in network comparisons. Include offline/changed routes, expired tokens,
multiple servers, slow network and empty Home cases.

## Category artwork and PS5 icon diagnosis

Sixteen original SVG masters in `assets/categories/` cover Action, Adventure,
Animation, Comedy, Crime, Documentary, Drama, Family, Fantasy, Horror, Mystery,
Romance, Science Fiction, Thriller, Western and Other. The stdlib-only
`tools/make-category-art.py` rasterizes committed masters to native 320×180 PNGs
at packaging time. Output is reproducible within the pinned build environment;
no third-party poster art or additional rendering framework is used. The native
UI always keeps the same artwork dimensions and can render a generic vector
fallback even before assets load or after an image failure.

The PS5 icon is `ps5/sce_sys/icon0.png`, independently checked against the source
bytes during packaging. It is not the in-app ImGui logo. The expected icon SHA-256 is
`d6b3eaac084816c97312bdc36a29b31493efb37fad43616b43d7a4dc9a1d14ec`,
identical to the baseline source. The existing angular 512×512 art is retained; redesigning it would not diagnose an installation/cache
problem. `artwork-manifest.json` records expected and packaged icon/background/
category hashes. Developer Options → Read-only installed icon audit compares the
installed title resource to that manifest on a worker, logging hashes and match
state only. It explicitly does not inspect or alter registered shell artwork.

On a PC with a staged package or read-only FTP mirror:

```sh
python3 tools/artwork-diagnostics.py --package /path/to/PPSA85301
python3 tools/artwork-diagnostics.py --installed /path/to/read-only-title-mirror \
  --registered-icon /path/to/explicitly-copied-registered-icon.png
```

Default mode writes nothing. It checks PPSA85301 and PlexStation title metadata,
reads only the requested artwork/metadata, and does not scan account/save files
or system databases. If installed icon bytes differ, the operator can separately
choose a **staging-only** refresh:

```sh
python3 tools/artwork-diagnostics.py --refresh-staged-icon /path/to/staged/PPSA85301 \
  --confirm-title PPSA85301
```

This requires explicit confirmation, refuses a different title or linked staging
root, saves an icon backup and atomically replaces only staged `sce_sys/icon0.png`.
It does not deploy to the console or mutate registration/caches. Validate the
stage, then follow normal full-title installation after closing the app. Preserve
`prefs.txt`, `plexstation-account.json`, listening/progress files and
`/data/plexstation-client`. Keep 755 directory/executable, 644 resource and 600
private-file modes with correct ownership. No reinstall/uninstall is assumed
necessary. If all title hashes match but shell art remains old, registration or
caching needs further read-only device evidence; a normal launcher/console restart
may refresh it, but no title-database reset is authorized or performed.

## Free Live TV research and limitations

Inspected the current official [PMS API 1.2.3](https://developer.plex.tv/pms/) and
[Plex Free Live TV page](https://www.plex.tv/watch-free-tv/). The documented PMS
DVR/EPG contracts (`/livetv`, tuner/provider channel/grid/session endpoints) are
personal-server interfaces. They do not establish a usable third-party contract
for Plex's free ad-supported cloud channels. No authorized supported free-channel
catalog/playback contract was established here; no official-client session
secrets were scraped and no advertising, geo, authentication or DRM restriction
was bypassed. The native free-TV page accurately states that no personal DVR is
required and that its authorized catalog/playback integration is unavailable.
It links separately to retained Personal DVR functionality. **No free channel
listing or playback is claimed working.** This is the maximum verified compliant
subset available in this environment, not a claim that Plex has no internal API.

## Acceptance evidence

Host tests use synthetic services/player/GPU boundaries, except actual image
codecs/filesystem caches and loopback HTTP/TLS fixtures. Native compilation is
separate from both host validation and physical acceptance.

| Criterion | Implemented source/test evidence | Automatic verification | External/device acceptance |
|---|---|---|---|
| 1 Startup/discovery | verified endpoint first; bounded Home; incremental/cancelled snapshots; phase logs | Core/transports/controller/benchmark | Five cold/warm PS5 runs pending |
| 2 Home avatars | primary auth/parsed fields, independent lane, initials/neutral SVG, safe errors | Home/profile/artwork fixtures incl admin/managed/PIN/guest/absent | Actual avatar response/TLS/decoder diagnosis pending |
| 3 Home movie cards | shared 2:3 cover/flow/focus | Presentation and scaled controller measures | Physical display/retention pending |
| 4 TV show cards | series metadata, 2:3 cover | Presentation/controller | Physical display pending |
| 5 On Deck | 16:9 episode sources/backdrops/poster fallback/progress | Metadata and actual decode/fallback fixtures | Live media/rendering pending |
| 6 Home movie rail | canonical semantic/scope identity | Duplicate/alias/scoped-hub tests | Actual server response pending |
| 7 TV added vs released | type=2 addition time; type=4 valid air dates; original dates displayed | Older-import/new-air/missing/future fixtures | Actual library data pending |
| 8 Watchlist cards | actual item types, direct approved absolute imagery, fallback | Discovery/URL/artwork/controller | Authorized provider data pending |
| 9 Movies duplicate rows | same semantic composition shared with Home | Generated + server-hub tests | Physical page pending |
| 10 Movie categories | original SVG/PNG/static vector fallback | Reproducibility/dimensions/geometry/package gates | Physical category page pending |
| 11 TV duplicate rows | series vs episode identities and selected-library rows | Composition/metadata tests | Actual server/physical page pending |
| 12 TV categories | same shared assets/geometry | Same asset/card tests | Physical page pending |
| 13 Free TV vs DVR | explicit truthful free-TV state; separate Personal DVR | Controller route and no-DVR-request test | Free cloud listing/playback unavailable; DVR live test pending |
| 14 Rentals removal | no rental route/sidebar/UI | Registry/route checks | Not a physical dependency |
| 15 Audiobooks position | v2 registry migration, own visible entry after Live TV | Fresh/legacy order/controller tests | Device navigation pending |
| 16 Music/books | persistent designation, book covers/terms/local progress vs music | Audio/listening/controller/profile tests | Actual audio/chapters/resume pending |
| 17 Shell icon | source/package hash check, native read-only audit, staged backup refresh | Hash/mode/title/reproducibility tests | Installed/registered PS5 files not supplied |
| 18 Existing functionality | decoder/audio/player kept, all prior tests retained | Full workflow gates recorded below | Owner's full regression pending |

### Physical comparison checklist

Record the installed SHA and keep its matching symbols. Preserve the known
working package and runtime data. Verify launch, account restore, PIN, remembered
server/failover, native video, subtitles/Off, audio selection/sync, search,
Continue Watching, watched/resume, TV hierarchy and DVR operations first.
Then test every criterion above at 1080p and 4K, large/empty libraries, first/
middle/last rail items, missing/failed images, repeated refresh/relaunch and profile
switching. Collect only safe avatar/art stage/request IDs and startup/hash lines;
never share account files, PINs, full authenticated URLs or private raw responses.

Existing test scenarios are retained. The grid navigation expectation changes
from five to six actual portrait columns. The fixture uploader now checks the
real media bounds (640×960), with an explicit retained 256×256 avatar assertion
and the original oversized-PMS-response rejection; a new larger-CDN-original
case verifies downsampling. These are updated contracts, not removed safeguards.

Native build/source/artifact/PR evidence is appended after the hosted workflow
actually completes. No physical pass is inferred from compilation.
