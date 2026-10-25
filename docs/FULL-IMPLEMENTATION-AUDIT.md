# Full implementation audit — October 24, 2026

> Historical audit candidate. For this stabilization branch use [the current report](PS5-STABILIZATION.md), including Rentals removal, free TV/DVR separation, shared card geometry and icon diagnostics.

Audit base: `7513c0e307a911b902dc8e5f370a600bb5d13361`, fetched feature branch,
clean working tree. Native source checkpoint `208b5be1ec73f6fa11797dbd3ae2b21262eafce3`
passed [run 40060131391](https://github.com/tattedbeardo89/PlexStation/actions/runs/40060131391).
`5d8adb9ca74d051b8154e01dafb0ad0848c372f7` and `1ad4f6c4ab562762bd4c2ffa958cc098e234528a` remain rollback references.

The matrix records the corrected source state; milestone sections below identify
the original defects and the evidence for each change. Partial and externally
blocked rows remain open.
“Implemented” describes reachable source behavior. Host fixtures use fake
player/GPU/service boundaries; only the transport fixtures make actual local
HTTP/TLS requests. No new physical acceptance is inferred from builds.
The owner confirms native launch, account authentication, profile selection,
server connection, video and subtitles initially Off at the recorded baseline.

The original PROJECT-SPEC orange/gold visual direction is superseded by the
owner's navy/indigo requirement. Older status documents refer to historical
builds; their claims of absent disk cache/Watchlist/music/transcoding/autoplay
must not override current inspected source. Reference screenshots are unavailable;
the current task's layout requirements govern the audit.

| Feature | Implementation status | Evidence | Missing work | Tests | PS5 verification |
|---|---|---|---|---|---|
| Launch/auth/primary token | Implemented | client.cc create_pin/poll_pin/validate_account; auth.h; ui_init/login_screen; 5d8adb9 | Protect existing behavior | auth_tests; plex_tests | Owner confirms launch/auth on 5d8adb9; new changes pending |
| Home users/PIN/profile isolation | Implemented | home.h profiles/profile_scope; choose_profile/ValidateProfile/clear_profile | Real avatar failure still unresolved | home_tests; controller_tests | Owner confirms profile selection on 5d8adb9 |
| Server identity/TLS/endpoint fallback | Implemented | client.cc select/identity/sections; transport_ps5.cc; connect_server | Prefer verified route after discovery too | connection_tests; test_transport | Owner confirms connection on 5d8adb9 |
| Pre-play subtitles | Requires physical PS5 verification | plex_video_ui.h prepare_subtitle_selection/subtitle_picker_frame/apply_video_plan; a5fbca9 | Pre-play policy and external reuse tests added; actual native discovery/rendering acceptance pending | subtitle_tests; controller_tests | Off accepted; switching pending |
| In-player subtitle switching/Off | Requires physical PS5 verification | service_subtitle_selection; player.cc player_set_subtitle_track/player_add_subtitle; 208b5be | Reuse, Off and media-open-count regressions passed; actual late-load cancellation/rendering needs PS5 | controller_tests; subtitle_tests | Pending; never infer from stub player |
| Subtitle defaults/memory/forced/language | Implemented | v3 scoped preferences in plex_video_ui.h; subtitles.h match_subtitle | Ambiguous embedded match intentionally fails closed | subtitle_tests; controller_tests | Pending |
| Subtitle appearance/delay | Implemented | video_tools/subtitle_settings_frame; player_set_sub_style/player_set_subtitle_delay | Renderer acceptance | streaming_tests; controller_tests | Pending |
| Native Direct Play/decoder/audio clock | Implemented | client.cc direct_play; player.cc; ps5/modules/ps5vdec.c; platform_ps5.c | Preserve pipeline; extend codec testing on console | plex_tests; streaming_tests | Owner confirms native video on 5d8adb9 |
| Direct Stream/transcoding | Requires physical PS5 verification | streaming.h negotiate_video/universal_query; request_video; HLS native VLC | Single-part known output only; no burned captions | streaming_tests | Live negotiation/segments/session cleanup pending |
| Automatic quality | Partially implemented | quality_name/universal_query use local Original / remote 1080 cap | No bandwidth measurement or adaptive selection; label accurately | streaming_tests | Pending |
| Version/audio tracks/audio delay | Implemented | detail version; playback_track_frame; player_set_audio_track; ±500 ms | Live formats/sync measurement | controller_tests; streaming_tests | Per-codec and A/V sync pending |
| Resume/seek/progress/watched | Implemented | queue_progress; plex_progress_ui.h; progress_outbox.h; mark_video_completion | Server reconciliation acceptance; protect live exclusion | plex_tests; streaming_tests; controller_tests | Basic video accepted; cross-client progress pending |
| Next Episode/autoplay | Implemented | episodes.h; service_next/next_countdown_frame | Specials/order and permissions acceptance | streaming_tests | Pending |
| Artwork GPU residency/lifetime | Requires physical PS5 verification | artwork.cc update/request_image/clear; gfx/library_free_image | Device disappearing-poster root cause not established | artwork_tests fake GPU + real decode | Disappearing posters reported on older builds |
| Persistent/scoped artwork | Implemented | cache.h DiskCache; configure_artwork_scope; artwork.cc set_scope | No network fetch on valid disk hit; device permissions/path test | cache_tests; artwork_tests | Relaunch/offline retention pending |
| Home avatars | Requires physical PS5 verification | home.h profile_avatar; client XML fields; artwork.cc avatar_url/redirect/decode/upload; profile_screen_frame | Need safe device request-stage logs to establish root cause; WebP/SVG unsupported | home_tests; artwork_tests | Avatar Unavailable reported; no confirmed correction |
| Avatar diagnostics/retry | Implemented | Artwork::avatar_state/diagnostics/retry_avatars; Developer Retry Avatar | Picker/Developer retry and format errors added; device request-stage logs still required | artwork_tests | Pending |
| Fast endpoint startup | Implemented | activate_profile FastConnect 800/1400ms; identity + authenticated sections | Cache route can fail with token needing rediscovery; DNS may exceed timeout | controller_tests; connections | Cold/warm device measurements missing |
| Warm Home presentation | Implemented | restore_presentation/presentation_scope/home_presentation_cache | Validate cached schema; retain scoped permission gate | cache_tests; controller_tests | Pending |
| Background Home refresh/focus | Implemented | service_home_refresh/load_home; HomeFeed ID reconciliation; 568f95f | Idle scheduling added; physical refresh/focus acceptance pending | controller_tests | Pending |
| Startup measurements | Partially implemented | startup: authorized-server-ms; home checkpoints | Route/cache/first-Home metrics added; no measured PS5 cold/warm timings available | controller_tests fast-path fixture | Device five-run cold/warm benchmark pending |
| Home layout/episode cards | Implemented | home_frame/poster shared Theme; home_feed.h | Supplemental personal-video exclusion and collection cards added; screenshot equivalence and device navigation unverified | controller_tests 1080/4K; home_feed_tests | Visual/focus acceptance pending |
| Personalized Collections on Home | Implemented | home_feed collection serialization/aggregation; open_media; f6d853b | Real server collections and images pending live acceptance | discovery_tests; home_feed_tests | Pending |
| Watchlist sync/grid/count/filter/pages | Requires physical PS5 verification | cloud.h cloud_watchlist/cloud_change/resolve_cloud; plex_cloud_ui.h; grid renderer | Circle interception is already implemented; recommended cloud feed unavailable | discovery_tests; controller_tests | Cloud access/write acceptance pending; managed profiles blocked |
| Movies/TV default routing/tabs | Implemented | navigation.h default_library; open_library/activate_library_tab; scoped prefs | Selected-library routing and type=2 TV added query implemented; live server and physical tab acceptance pending | discovery_tests; controller_tests | Pending |
| Categories genres/collections | Requires physical PS5 verification | library_categories and explicit category types; open_item/open_media; f6d853b | Bounded genres/collections and drill-through corrected; live category acceptance pending | controller_tests category→movie; discovery_tests genre/collection/seasonal | Pending |
| Library tools/sorts/search/pages | Implemented | library_tools loop bounded by actual labels; 192707d | Real controller behavior and library filter availability pending | controller_tests bottom-of-tools rendering; ASan/UBSan | Pending |
| Home Movies | Requires physical PS5 verification | personal_library/default_library; persisted section designation; landscape/card_metadata | Stable ID across rename and personal duration/date added; physical validation pending | discovery_tests; controller_tests | Pending |
| Music browsing/queue/audio-only | Implemented | audio.h; plex_audio_ui.h audio_queue_load/audio_start/audio_frame; player :no-video | Genre route handling exists; sole-library defaults absent; direct-item/default-feed race | audio_tests; controller_tests | Audible formats not yet accepted |
| Music Recommended/categories/default | Partially implemented | music_feed.h; audio_home/audio_library_tab | Defaults, direct-item request isolation, artists/authors and real categories implemented; some requested dedicated shelves remain partial | discovery_tests; controller_tests | Pending |
| Audiobooks designated libraries/progress | Implemented | book_section; ListeningStore/book_resume/book_save; AudioQueue | Keep local semantics explicit and library-scope local recommendations | listening_tests; audio_tests | Pending |
| Audiobook chapters/rates/bookmarks/timer | Implemented | player chapters/rate; SleepTimer; audio_frame | Narrator/series unavailable unless actual server metadata; no invented type | listening_tests; controller_tests | Audibility/pitch/timer hardware acceptance pending |
| Seasonal Home/library recommendations | Implemented | seasonal_library/seasonal_tag/append_seasonal_home; recommendation_month | Winter/summer and real collection fallback/custom season selection added; live matches pending | discovery_tests | Pending |
| Sidebar hide/order/pins/theme | Implemented | sidebar_order/library_pin_order/nav_visible; settings 10/11; Theme | Stable pin reorder/reset and requested default order added; device usability pending | controller_tests; test_theme.py | Pending |
| Personal Live TV guide/tune | Requires physical PS5 verification | live_tv.h live_channels/live_guide/live_tune; LiveChannels/Guide/Tune jobs | Current-day bounded guide; schedule/rules and confirmed cancellation added; creation/edit and idle tuner release unverified | discovery_tests; controller_tests | No tuner/device acceptance |
| DVR recordings | Partially implemented | live_recordings/cancel_recording_rule; LiveRecordings/LiveCancel jobs; live-recordings UI | Scheduled/rule lists and confirmed owner rule cancellation added; creation/edit/storage template and tuner cleanup remain incomplete | No existing end-to-end test | Not tested |
| Free cloud Live TV | Blocked by external API/platform | Live TV unavailable state separates personal tuner from cloud | No verified cloud authorization/playback adapter; do not use fake channels | Unsupported UI inspection | Not supported |
| Rentals catalog/entitlements/DRM | Blocked by external API/platform | rentals screen; no licensed DRM path or supported catalog binding | Accurate unavailable state; no pricing/payment/entitlement fabrication | Unsupported UI inspection | Not supported |
| Angular icon/splash/title | Implemented | assets/plexstation.svg; ps5/sce_sys; make-brand.py; check-package.py; 596ea9f/816d7eb | Verify packaged dimensions and small-size asset; safe shell-cache refresh | package validation; test_package_permissions | Installed icon pending |
| Shared UI architecture | Partially implemented | Theme/poster/home_frame/row/sidebar shared; media adapters | Tabs now share rendering; media adapters intentionally retain distinct audio/video controls; no new framework | test_theme.py; controller_tests | 1080/4K fixture only |
| Crash/numeric/TLS/secrets | Implemented | diagnostics.h; patched json.hpp; crash/debug scripts; transports; secure stores | Preserve instrumentation and no URL/token logs | test-json-numeric; test-crash; transports/sanitizers | Parser/video baseline accepted; distinct historical signals need matching logs |
| Native build/symbols/package | Implemented | build-ps5.yml; collect-debug/check-package/package-title; final native run 40063743047 at 1078e74 | Final full hosted and native gates passed; physical acceptance and independent binary inspection are separate | All host suites + Actions native | Build success is not device acceptance |
| README/status accuracy | Implemented | README/ARCHITECTURE rewritten; historical status banners; this audit | Keep each new native result separate from physical acceptance | Documentation/source cross-check | Use owner evidence only |

## Milestone 1 — correctness and truthful evidence

Changes: repeated external subtitle selection reuses the verified attached VLC
ID within the current playback session; a new player-open clears that map.
Off still cancels pending attachment policy and no Plex action enables reopening.
The library-tools loop is now bounded by its actual ten labels. Sidebar focus
validation covers all built-ins and accessible pins. Profile-picker Square
retries failed avatars without removing healthy images; unsupported WebP/SVG
responses expose explicit format errors, never tokens or source URLs.

Local validation passed: subtitle policy, real image decode/cache/lifecycle,
production UI/controller (including repeat external select, Off, no reopen,
bottom-of-tools render and newer sidebar entries), PS5 pad mocks, package modes,
theme boundary and strict native-source syntax. These use fake VLC/GPU boundaries
and do not establish physical subtitle or avatar acceptance. Full sanitizers and
native compilation will run once for the combined milestones using the existing
workflow; intermediate commits remain rollback points without duplicate builds.

README/architecture now reflect native video acceptance, persistent artwork,
streaming, Watchlist and media features. Historical status tables are explicitly
labeled historical. The milestone sections retain the starting defects so reviewers can compare
them against the corrected matrix above.

## Milestone 2 — startup and background metadata

The cached endpoint still requires current profile authorization, TLS, matching
machineIdentifier and authenticated current sections. When discovery is needed,
`Client::select` now prefers the last successful route only if it is present in
the freshly authorized candidates; all HTTP/HTTPS-only policy checks remain.
A missing saved route cannot inject an endpoint into that list.

Idle Home refresh runs at most once per 120 seconds, only after eight seconds
without input and outside details/settings/profile/video/search/library tabs.
It shares the existing worker, does not overlap pending requests, leaves visible
rows intact, and restores focus by hub/item identity. Failed refresh retries are
spaced by 45 seconds. Cached Home remains scoped by profile/server/current IDs.

`startup: first-home-ms` reports profile-submit through first populated Home,
with cached-route/cached-home booleans and row count. The existing authorized
server metric now also includes profile-switch authorization; it previously
started after that work and underreported the user's wait. No endpoint/token or
username is logged. Cold versus warm PS5 latency must still be measured on the
console. Host evidence measures request count (warm: two bounded authorized
endpoint requests); fixture CPU duration is not real network/device latency.

Acceptance: five cold and five warm launches, measure profile-submit to usable
Home, repeat saved-route outage, remote route, changed permissions and protected
PIN profile. Retain focus through background changes and reject another
profile's cached presentation.

Milestone 2 local checks passed: real connection/framing/TLS fixtures and
separate-process preference persistence, production controller startup/idle
refresh/focus coalescing, pad/package/theme checks, strict native-source syntax.

## Milestone 3 — real category routes and media navigation

`library_categories` combines actual selected-section genres and collections,
using explicit category route types and bounded pagination. The old blanket
Categories-tab check treated every later movie/episode as a genre. Genre →
movie now requests metadata/details; collections route to real collection children.
TV Recently Added explicitly requests shows (`type=2`). Supplemental professional
Home rows omit Home Movies; personalized collection cards survive serialization
and open the real collection. Home Movies cards use landscape thumbnails and
personal duration/date labels. Server aggregate hubs can still include any
profile-permitted media; only supplemental selected-library queries are filtered.

Music/Audiobook entry opens an accessible remembered default (or deterministic
first library) and persists it. Opening a track/album directly avoids starting
a competing default-feed request. Audio categories share real genre/collection
routes with video; tabs share rendering. Change Library now supports audio too.
Book saves carry section ID and last-played order so Continue Listening can be
scoped to the selected library. Older records without a section are retained for
resume but need one newly resolved playback/save before appearing in that row.

Seasonal Home and library pages use the same real-genre/collection adapter,
including Winter/Summer. Settings → Libraries supports automatic or explicit
Halloween/Holiday/Winter/Summer selection plus disable. No matching media means
no row. Default sidebar order follows the owner's requested order; optional
Continue Watching remains available at the end. Pins reorder by stable library
ID and Reset clears current-server pins as well as built-in visibility/order.
Watchlist shows persistent filter/sort/count controls and a reachable explicit
cloud-access error/retry view, instead of leaving unrelated content visible
while probing. A separate cloud Recommended feed is still unavailable.

Local checks passed: discovery/search/Watchlist fixtures, seasonal tag and
collection fallback, real selected-section categories, Home Movies supplemental
exclusion, Home/profile fixtures, production controller default routing,
category → movie details, direct audio-item request, 1080p/4K draws, pad mocks,
package/theme boundaries and strict syntax. Final hosted gates cover the combined
source including the Watchlist error/presentation changes.

Icon source/package audit: original SVG master, icon0.png 512×512, pic0.dds and
pic1.dds 1920×1080 DXT1. Existing package validation enforces dimensions and
PPSA85301 identity. Installed icon/splash acceptance remains pending; refresh an
old shell icon by closing the app and restarting the launcher/console normally,
without deleting saved data or system databases.

## Milestone 4 — supported DVR controls and media isolation

Inspected official [PMS OpenAPI 1.2.3](https://developer.plex.tv/pms/) on
October 24, 2026: GET `/media/subscriptions`, GET
`/media/subscriptions/scheduled` and DELETE
`/media/subscriptions/{subscriptionId}`. `live_recordings` parses bounded actual
recording subscriptions/scheduled grabs. Live TV → Square opens recordings;
L2/R2 switches schedule/rules, Square refreshes. Only the owner profile can open
the rule-cancellation prompt. A second Cross confirms the whole-rule operation;
Circle preserves it. The worker rereads current profile-authorized rules before
deleting an exact present ID; 401/403/unsupported/malformed responses never
become success. Profile/server transitions clear lists and confirmation state.
No live server was mutated during development; writes were fixture requests.

The API's DELETE cancels the subscription and all scheduled grabs, not one
program or completed media. New/edit recording-rule setup needs server-provided
template/storage configuration and remains in Plex's supported app. No free
cloud Live TV adapter was established. Rentals remain limited by unavailable
supported catalog/entitlement binding and licensed DRM/HDCP playback; see the
[official Rentals FAQ](https://support.plex.tv/articles/faq-rentals-purchases-on-plex/).
No price, entitlement, rental availability or payment action is fabricated.

Music/Audiobook recommendations now avoid showing global ordinary playlists as
book-specific content. Freshly resolved tracks outside designated audiobook
libraries cannot inherit audiobook playback semantics. Local Continue Listening
is filtered by saved actual library ID. Home Movies' remembered accessible section
ID survives a title rename; permissions remain established by fresh sections.

Targeted checks cover actual adapter reads/deletes, forbidden/malformed/stale
rules, no mutation before confirmation, denied owner controls, book-library
recommendation scoping, pin order across library reorder, reset/profile isolation,
managed-user Watchlist states and strict source syntax. Hosted full regressions,
ASan/UBSan, native compile and package/debug collection are the final gate.

## Remaining boundaries and release assessment

This is an implementation candidate, not full Plex parity or physical acceptance.

- The owner's Avatar Unavailable root cause needs device request-stage logs.
  PNG/JPEG, redirect/credential isolation, decode/cache/upload fixtures and retry
  are implemented; WebP/SVG decoding remains unsupported.
- Native caption rendering, repeated external attachments, HLS segment auth,
  codec/rate/pitch/sync, tuner release and real cloud writes need device/service
  evidence. Source menus and stub-player tests do not establish those results.
- Automatic quality uses local Original / remote 1080p policy; measured/adaptive
  bandwidth quality remains missing. Multipart video and normalized audiobook
  narrator/series metadata are not implemented.
- Watchlist is real supported cloud data, with grid/count/filter/sort/pages and
  local GUID resolution; separate cloud Recommended discovery and managed-user
  cloud access remain unavailable. Availability is checked on selection.
- Music recommendation genres are browsable through Categories; not every
  requested personalized shelf has a dedicated carousel. Audiobook series are
  available only through real existing collections, not an invented schema.
- DVR creation/edit/storage template UI, recording priorities, multi-day guide
  paging and explicit verified idle-tuner release remain incomplete. Scheduled
  list and full-rule cancellation are implemented, with live acceptance pending.
- Free cloud TV and Rentals lack verified supported integration/DRM. No workaround
  or bypass is introduced. Full localization and physical visual polish remain.
- Cold/warm device timings and full regression acceptance are pending the owner.
  Use [the physical test procedure](HARDWARE-ACCEPTANCE.md), including five cold
  and five warm runs and safe avatar/cache diagnostics.

The existing native decoder/audio/renderer, PPSA85301 identity, numeric locale
fix, crash tools, TLS, PIN protections and private credential storage are retained.
The angular packaged icon was visually inspected; its original P/play motif,
navy background, blue/indigo layers and 512-square dimensions match the requested
direction. Shell rendering and small-display legibility still need console review.

Milestone 4 final local checks passed: discovery/seasonal/category/recording and
book-scope fixtures; audio/listening persistence; production controller including
recording confirmation/cancel/owner gate, stable pin ordering/reset, managed cloud
unavailable state; pad backend, package permissions, theme boundary and strict
native-source syntax. A live account, actual tuner or physical PS5 was not used.

## Exact milestone commits

| Milestone | Full source SHA |
|---|---|
| Correctness and audit | `192707d81ec1ce1ba2c9ec7b1ef157e292611a99` |
| Startup and idle Home | `568f95f9e54d7eceb5490b7db863918439e4a161` |
| Navigation and media rows | `f6d853b66bee56a9c5819871d1410a49132073cb` |
| DVR/isolation combined candidate | `fdeec1d28020ac1a362b4386aed3952795dba97f` |

One native workflow builds the combined candidate, avoiding overlapping builds.
Each earlier commit is retained as a source rollback point; it has not been
separately cross-built. The final native artifacts contain all four milestones
plus the shared-transition and subtitle media-open-counter follow-ups.

## Final review follow-up — shared sidebar transition

Review while the first combined native gate ran found another reachable early-
return defect: Home Movies/pinned libraries/Live TV/Rentals activated before
closing Music's browse layer, so that layer could consume the next frame and
hide the destination. Those branches also retained Details/Settings flags.
All non-audio sidebar routes now clear the old browse/details/settings layers
before dispatching the selected destination, preserving active background audio
until actual video playback starts. The production controller test enters each
of the four routes with all three old states active and verifies the destination
owns the screen. Search also clears stale library-tab context; a media item's
actual section identity takes precedence over Home Movies thumbnail styling.

The active first build is allowed to finish. A single subsequent native build
will validate the final follow-up; neither run is represented as physical
acceptance. This extra build is justified by the confirmed navigation defect,
not an overlapping or cosmetic-only rebuild.

Follow-up local production controller/pad/package/theme and strict native-source
checks passed with the final shared transition and search styling changes.

Audit precision correction: the tools renderer's old literal upper bound (13)
did not match its ten labels, but the normal focus clamp also limited the loop
window to valid labels. No out-of-bounds crash was reproduced through normal
focus navigation. The array-sized bound is defensive hardening, not evidence
that this caused a prior physical crash. Category drill-through, repeated
external-track selection, competing audio-feed requests and stale sidebar
layers are separately demonstrated application defects with regression cases.

## Final native build and artifacts — passed

Final built source: `1078e74cb636cf852ec17dfc221cb551cbbe508b`.
[Run 40063743047](https://github.com/tattedbeardo89/PlexStation/actions/runs/40063743047)
completed successfully. Every hosted host-regression gate (including ASan/UBSan
and both numeric locale cases), actual native compile/validation, matching debug
collection, installation ZIP modes and artifact uploads passed.
The workflow also runs the symbolization fixture. This is native build evidence,
not physical PS5 acceptance or live Plex/tuner validation.

| Deliverable | Link | Bytes | Outer artifact SHA-256 |
|---|---|---|---|
| Complete installation | [PPSA85301](https://github.com/tattedbeardo89/PlexStation/actions/runs/40063743047/artifacts/12548449135) | 39,492,485 | `1a0503bc610d6d0b353ee171e991ff3e67b6371243d70faf79e3d97847058f85` |
| Matching native symbols | [native-debug](https://github.com/tattedbeardo89/PlexStation/actions/runs/40063743047/artifacts/12549140245) | 92,894,636 | `73c868fab2e2bdc9959214933c7ccbcb8f9cdf6e044b7235196e6faf8841306f` |
| Build diagnostics | [logs/report](https://github.com/tattedbeardo89/PlexStation/actions/runs/40063743047/artifacts/12547879179) | 198,409 | `a3243aee2c5cd8e71afe886261b698edc0afe88fc5eb29a32c47c14f22c7cadb` |

Artifact names include the full built SHA. Installation and symbols expire per
GitHub retention policy; retain both offline before testing. The earlier combined
source `fdeec1d28020ac1a362b4386aed3952795dba97f` also passed
[run 40063037586](https://github.com/tattedbeardo89/PlexStation/actions/runs/40063037586).
These two executions were sequential. The second is needed for the confirmed
shared navigation follow-up, not an overlapping duplicate build.

Follow-up implementation commit: `ffd45cc9befb3354af8379198ef0c4cd86ed6028`.
Additional regression commit: `1078e74cb636cf852ec17dfc221cb551cbbe508b`.
The latter adds explicit player-open counts across pre-play selection and live
external re-selection, catching a same-URL reopen that URL equality alone misses.
All are included in the final successful hosted tests and native package.

[Machine-readable build evidence](BUILD-EVIDENCE-2026-10-24.json) records both
runs, artifact metadata/digests and successful final job steps. Evidence was
inspected via GitHub; these archives were not independently downloaded or
binary-inspected in this audit. No physical execution or live account/tuner
mutation occurred. The documentation-only publication commit changes no built
application source and does not start another native run.

## Exact continuation point

Install/test the final SHA using [HARDWARE-ACCEPTANCE.md](HARDWARE-ACCEPTANCE.md).
Preserve runtime data and matching symbols. Priority device evidence is subtitle
switch/Off/late external completion, avatar request-stage errors, poster retention,
and five cold/warm startup runs. These results determine the remaining critical
fixes; the physical avatar root cause cannot be established from the host alone.
Remaining independent development is adaptive bandwidth quality, multipart video,
more dedicated audio shelves/normalized metadata where actually provided, full
localization, DVR template-based create/edit/multi-day guide/tuner cleanup and
verified external-provider capabilities. Free cloud TV/Rentals remain blocked
without a supported authorization/catalog/DRM contract. No complete Plex parity
or final physical release acceptance is claimed.
