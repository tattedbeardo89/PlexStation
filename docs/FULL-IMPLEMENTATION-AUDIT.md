# Full implementation audit — October 24, 2026

Audit base: `7513c0e307a911b902dc8e5f370a600bb5d13361`, fetched feature branch,
clean working tree. Native source checkpoint `208b5be1ec73f6fa11797dbd3ae2b21262eafce3`
passed [run 40060131391](https://github.com/tattedbeardo89/PlexStation/actions/runs/40060131391).
`5d8adb9` and `1ad4f6c4ab562762bd4c2ffa958cc098e234528a` remain rollback references.

The matrix records the inspected starting state, not a declaration that every
row is finished. Corrections and validation evidence are appended per milestone.
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
| Pre-play subtitles | Requires physical PS5 verification | plex_video_ui.h prepare_subtitle_selection/subtitle_picker_frame/apply_video_plan; a5fbca9 | Extend actual session tests for repeated external choices | subtitle_tests; controller_tests | Off accepted; switching pending |
| In-player subtitle switching/Off | Requires physical PS5 verification | service_subtitle_selection; player.cc player_set_subtitle_track/player_add_subtitle; 208b5be | Exercise external cancellation and unsupported choices | controller_tests; subtitle_tests | Pending; never infer from stub player |
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
| Avatar diagnostics/retry | Implemented | Artwork::avatar_state/diagnostics/retry_avatars; Developer Retry Avatar | Expose retry on profile picker and precise current image status | artwork_tests | Pending |
| Fast endpoint startup | Implemented | activate_profile FastConnect 800/1400ms; identity + authenticated sections | Cache route can fail with token needing rediscovery; DNS may exceed timeout | controller_tests; connections | Cold/warm device measurements missing |
| Warm Home presentation | Implemented | restore_presentation/presentation_scope/home_presentation_cache | Validate cached schema; retain scoped permission gate | cache_tests; controller_tests | Pending |
| Background Home refresh/focus | Partially implemented | connected load_home; BackgroundResources; HomeFeed completion preserves IDs | No periodic idle Home metadata refresh; add scheduling/coalescing | controller_tests | Pending |
| Startup measurements | Partially implemented | startup: authorized-server-ms; home checkpoints | Report route/cache/first-render metrics; no physical latency claims | controller_tests fast-path fixture | Device five-run cold/warm benchmark pending |
| Home layout/episode cards | Implemented | home_frame/poster shared Theme; home_feed.h | Personal libraries currently included in professional aggregates; fix; no screenshots available | controller_tests 1080/4K; home_feed_tests | Visual/focus acceptance pending |
| Personalized Collections on Home | Partially implemented | home_feed parses unknown server hubs | Collection items discarded by row serializer; preserve actionable real collections | discovery_tests; home_feed_tests | Pending |
| Watchlist sync/grid/count/filter/pages | Requires physical PS5 verification | cloud.h cloud_watchlist/cloud_change/resolve_cloud; plex_cloud_ui.h; grid renderer | Circle interception is already implemented; recommended cloud feed unavailable | discovery_tests; controller_tests | Cloud access/write acceptance pending; managed profiles blocked |
| Movies/TV default routing/tabs | Implemented | navigation.h default_library; open_library/activate_library_tab; scoped prefs | Recommended TV added endpoint may return episodes; request type=2 explicitly | discovery_tests; controller_tests | Pending |
| Categories genres/collections | Implemented but malfunctioning | activate_library_tab(2); open_item category branch | Only genres loaded; every item after drilling treated as genre; implement explicit category routes | New controller/discovery cases required | Pending |
| Library tools/sorts/search/pages | Implemented but malfunctioning | library_tools renderer labels[10] loops to 13 | Out-of-bounds read when scrolled; bound by array length | New controller rendering case required | Pending |
| Home Movies | Partially implemented | navigation.h personal_library; nav 9; default_library | Use landscape personal cards and duration/date; retain section ID and permissions | discovery_tests; controller_tests | Pending |
| Music browsing/queue/audio-only | Implemented | audio.h; plex_audio_ui.h audio_queue_load/audio_start/audio_frame; player :no-video | Genre route handling exists; sole-library defaults absent; direct-item/default-feed race | audio_tests; controller_tests | Audible formats not yet accepted |
| Music Recommended/categories/default | Partially implemented | music_feed.h; audio_home/audio_library_tab | Improve sole/default routing, genre+collections+playlists, artists; avoid stale video library kind | discovery_tests; controller_tests | Pending |
| Audiobooks designated libraries/progress | Implemented | book_section; ListeningStore/book_resume/book_save; AudioQueue | Keep local semantics explicit and library-scope local recommendations | listening_tests; audio_tests | Pending |
| Audiobook chapters/rates/bookmarks/timer | Implemented | player chapters/rate; SleepTimer; audio_frame | Narrator/series unavailable unless actual server metadata; no invented type | listening_tests; controller_tests | Audibility/pitch/timer hardware acceptance pending |
| Seasonal Home/library recommendations | Partially implemented | navigation.h append_seasonal_home/library_feed | Library feed lacks winter/summer/collections; customization is toggle only | discovery_tests | Pending |
| Sidebar hide/order/pins/theme | Partially implemented | sidebar_order/nav_visible; settings pages 10/11; Theme | Default ordering differs; diagnostics reject indices >=9; pins cannot reorder/reset; improve | controller_tests; test_theme.py | Pending |
| Personal Live TV guide/tune | Requires physical PS5 verification | live_tv.h live_channels/live_guide/live_tune; LiveChannels/Guide/Tune jobs | Current-day bounded guide; idle tuner release unverified; recording controls missing | discovery_tests; controller_tests | No tuner/device acceptance |
| DVR recordings | Missing | No recording list/control path | Inspect supported official API; only add documented authorized behavior | No existing end-to-end test | Not tested |
| Free cloud Live TV | Blocked by external API/platform | Live TV unavailable state separates personal tuner from cloud | No verified cloud authorization/playback adapter; do not use fake channels | Unsupported UI inspection | Not supported |
| Rentals catalog/entitlements/DRM | Blocked by external API/platform | rentals screen; no licensed DRM path or supported catalog binding | Accurate unavailable state; no pricing/payment/entitlement fabrication | Unsupported UI inspection | Not supported |
| Angular icon/splash/title | Implemented | assets/plexstation.svg; ps5/sce_sys; make-brand.py; check-package.py; 596ea9f/816d7eb | Verify packaged dimensions and small-size asset; safe shell-cache refresh | package validation; test_package_permissions | Installed icon pending |
| Shared UI architecture | Partially implemented | Theme/poster/home_frame/row/sidebar shared; media adapters | Tabs duplicated video/audio; centralize shared tab drawing; avoid framework rewrite | test_theme.py; controller_tests | 1080/4K fixture only |
| Crash/numeric/TLS/secrets | Implemented | diagnostics.h; patched json.hpp; crash/debug scripts; transports; secure stores | Preserve instrumentation and no URL/token logs | test-json-numeric; test-crash; transports/sanitizers | Parser/video baseline accepted; distinct historical signals need matching logs |
| Native build/symbols/package | Implemented | build-ps5.yml; collect-debug/check-package/package-title; 40060131391 | Run full gates and native compile for final new source | All host suites + Actions native | Build success is not device acceptance |
| README/status accuracy | Implemented but malfunctioning | README/ARCHITECTURE/TV-CLIENT-STATUS claim no playback/no cache/no Watchlist | Replace stale present-tense claims; retain historical evidence labeled as historical | Documentation/source cross-check | Use owner evidence only |

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
labeled historical. The initial matrix above retains the starting defects so
reviewers can compare them against each correction below.

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
