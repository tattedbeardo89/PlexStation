# Media expansion checkpoints

## Preserved physical-testing baseline

Source tag: `baseline/ps5-physical-test-d52e5cd`, pointing to
`d52e5cd39ad6689f1f917179a6d1761132adde03`.
[Existing successful run 39702538057](https://github.com/tattedbeardo89/PlexStation/actions/runs/39702538057)
is unchanged. Its installation and matching native-debug artifacts carry that
full SHA. New checkpoint artifacts use different SHAs; do not install them over
the physical-testing baseline until ready to compare. Download both baseline
artifacts now for permanent local retention: Actions retention is finite and
this session cannot extend/download authenticated artifacts. A source tag does
not preserve binary artifacts beyond GitHub's retention period.

## Audit before implementation

| Area | Baseline status |
| --- | --- |
| Authentication, profiles/PIN, endpoint fallback, remembered PMS | Implemented; owner confirmed original account/connectivity, new Home flows awaiting hardware/live acceptance |
| Home, movies, TV hierarchy/details, basic sorts/search | Implemented with bounded async requests; supplemental Home uses first movie/TV section |
| Music artists/albums/tracks/playlists and audio queue | Missing; artist sections discovered as metadata but hidden in navigation; video-only direct-play guard rejects tracks |
| Audio-only engine | Existing VLC amem callbacks and player state independent of video; new audio UI/transition validation needed |
| Chapters, rates, subtitle look/languages/delay | Existing player APIs, mostly not exposed in Plex UI |
| Audiobooks/designation/bookmarks/sleep timer | Missing; no first-class audiobook type assumed |
| Video | Original single-part H.264/HEVC MP4/MKV adapter; tracks/seek/pause/audio delay available |
| Direct Stream/transcode/quality/external subtitle flow/autoplay | Missing |
| Progress | Scoped timeline and watched calls, coalesced 10s; no durable offline retry/reconciliation |
| Watchlist | Missing and hidden; cloud API access must be established |
| Unified search/filter/aggregation/playback information | Basic ungrouped server search, basic video filters; remaining features missing/partial |
| Native stability | Instrumentation, isolation flags, debug artifacts, bounded images, deferred texture cleanup; physical Movies SIGABRT and unmatched SIGSYS remain unproven |

This audit is not a claim of feature completeness or physical acceptance.
Checkpoint results and explicit limitations are appended as implementation lands.

## Checkpoint A implementation — Music

Conditional Music navigation uses actual accessible artist-type sections. Native
paged artists/albums/tracks, recent albums/played tracks, audio playlists,
collections and genres use PMS library endpoints. Missing/empty responses show
an empty result; no content is fabricated. Music search requests actual tracks.
Artist/album children request parent metadata for artwork/biography. Square
opens album/artist/playlist Play/Shuffle or selected track Add/Play Next actions.
Now Playing and the Touchpad mini-player support pause, previous/next, repeat,
shuffle, queue removal/clear and retry. Queue is capped at 2000 tracks; complete
album/playlist loading fails rather than silently playing a truncated queue.
Multi-disc albums sort by disc then track; playlist order/duplicates are retained.
Profile/server transitions stop audio and clear its queue/history and stale work.
Natural completion submits a scoped scrobble separately from timeline updates.

Audio streams use the existing libVLC player with `:no-video`; the audio-only
UI never asks for a video texture. Actual runtime decode/output remains a device
test. API fixtures do not establish audible playback or server history acceptance.
Music queue persistence is session-only; retry is explicit and does not silently
skip an unplayable song. A new playback request replaces the current queue.

### Codec evidence and limits

Pinned FFmpeg 8.1.2 enables its decoders/demuxers (no per-codec disable list),
with `avcodec`/`avformat`, MP4, WAV/FLAC demuxers in the VLC static module table.
The adapter admits MP3, AAC, FLAC, ALAC, WAV PCM, Vorbis and Opus for matching
known audio containers. This is build configuration evidence and adapter fixture
coverage, **not physical codec acceptance**. Unsupported combinations fail
explicitly. Audio transcoding fallback is not yet implemented at this checkpoint.
No libVLC engine replacement or dependency-pin/cache change is introduced.

| Source | Candidate Direct Play | PS5 audible playback |
| --- | --- | --- |
| MP3 | MP3 / FFmpeg decoder | Pending |
| AAC | AAC or MP4/M4A | Pending |
| FLAC | FLAC | Pending |
| ALAC | MP4/M4A | Pending |
| PCM | WAV | Pending |
| Vorbis | Ogg | Pending |
| Opus | Ogg/Opus | Pending |

The actual audio callback requests 48 kHz signed 16-bit PCM, stereo by default
(optional existing surround output). Lossless **source** Direct Play avoids PMS
encoding but does not imply original sample-rate/bit-depth output or bit-perfect
playback. Decoder/channel/rate observations still require real files on PS5.

### Checkpoint A native result

[Run 39705677578](https://github.com/tattedbeardo89/PlexStation/actions/runs/39705677578)
passed all host/ASan/UBSan gates, real native compilation, debug collection,
complete packaging and ZIP modes at `b3cc83265d63c84590e8030b268eeb904daa037a`.
Artifacts: `PlexStation-PPSA85301-b3cc83265d63c84590e8030b268eeb904daa037a`
(37.4 MB), `PlexStation-native-debug-b3cc83265d63c84590e8030b268eeb904daa037a`
(86.7 MB), `PlexStation-build-diagnostics-b3cc83265d63c84590e8030b268eeb904daa037a`.
Public run/artifact metadata was inspected; no authenticated artifact download
or physical playback test was available. Later checkpoints fix a transition
ordering issue found in review (stop old audio before opening new audio).

## Checkpoint B implementation — Audiobooks

Settings → Audio / Audiobook Libraries designates an accessible music section
for this profile/PMS only. No server metadata is rewritten or first-class Plex
"audiobook" type invented. Audiobooks uses actual author/album/track/playlist/
collection organization; author biography is parent metadata where available.
Optional narrator/series metadata has no guaranteed PMS schema and is not
invented. Dedicated normalized narrator/series browsing remains incomplete.

Now Playing adds 0.75/1/1.10/1.25/1.5/1.75/2x requested speed through libVLC,
existing compiled scaletempo/time-stretch support, actual observed VLC rate,
±10s / ±30s seek, embedded chapters or next/previous-track fallback, restart
chapter/book, local finished state and up to 16 local bookmarks per book.
The stored sleep preference is Off/15/30/45/60/end-of-track; countdowns are
session-only and expiry pauses (or stops at track end) without continuing into
the next track. Audible pitch preservation/rate behavior requires hardware
acceptance; linking scaletempo is not proof of it.

`plexstation-listening.json` contains **PlexStation-local** book/track positions,
completion, bookmarks and stable scope IDs. It is bounded (512 records/512 KiB),
mode 0600, owner/type/symlink checked and atomically replaced after file fsync.
Corrupt/unsafe existing files are not silently overwritten. Preserve this file
alongside prefs/account credentials during updates. It contains no auth token
or playback URL. Continue Listening and Completed are explicitly local lists.
Newer observed server timestamps supersede older local resume entries; when
server metadata lacks reliable freshness, cross-client book-level reconciliation
cannot be guaranteed. Actual scoped track timelines/scrobbles still go to PMS;
local book-level completion/bookmarks are not claimed synchronized to other apps.

Queue/positions are bounded and profile/server scoped. Profile changes stop VLC
and invalidate stale work. Multi-file books play sequential track items; an
individual track with multiple Media Parts remains unsupported. Missing chapter
metadata falls back to track navigation. Persistent paused queue reconstruction,
per-book speed overrides, a full bookmark browser, and narrator/series-specific
filters remain incomplete. The latest local bookmark can be restored in Now Playing.

The audio transition review also found native ps5http's independent logger
printed media and redirect URLs despite suppression of VLC's logger. Both
messages now redact the address unconditionally. No credentials/URLs are added
to listening records or diagnostics. A host ImGui fixture now rebuilds its legacy
font atlas when new text invalidates it; the production Vulkan backend owns its
actual atlas updates. Fixture success does not validate console font rendering.

## Checkpoint C implementation — advanced playback

Original supported video remains the fast path. Unsupported originals or chosen
quality caps request the PMS universal streaming decision asynchronously. Accepted
server responses must identify supported H.264/HEVC output and explicit video
copy/transcode behavior; unknown/rejected decisions fail visibly. Video copy is
labelled Direct Stream, separately from full video transcode. HLS startup still
requires actual PMS/VLC acceptance, segment authorization and PS5 testing.
No fixture proves playable HLS, quality adaptation or audible fallback.

Quality is stored per profile with separate local/remote choices: Original,
Automatic, 4K/40 Mbps, 1080p/8 Mbps, 720p/4 Mbps, 480p/1.5 Mbps, 360p/720 kbps.
Automatic is an explicit fixed policy (Original local, 1080p cap remote), not a
measured bandwidth estimate or adaptive streaming claim. Original failures can
request one server fallback; unsupported/failed audio requests AAC/HLS via the
actual music universal endpoint. Server rejection is an error, never silently
skipped or reported as working. Transcode sessions receive best-effort stop
requests; actual server cleanup/expiry remains an acceptance item.

During video press Square for playback information, per-media persisted audio
±25 ms/reset, subtitle delay, subtitle size, external subtitle selection and Off.
External supported subtitle keys must remain under the selected PMS's
`/library/streams/`; arbitrary host URLs are rejected. SRT/WebVTT/ASS/SSA are
adapter candidates, not physical rendering acceptance. Existing libVLC embedded
tracks remain available. Playback settings cycle audio/subtitle languages;
profile activation restores that profile's choice. Forced-subtitle selection
and subtitle style persistence have remaining player/version limitations.
Per-media audio offsets use profile + server + ratingKey, retain ±500 ms limits,
and are reapplied on open; real sign/seek/restart effectiveness needs PS5 testing.

Next Episode uses actual accessible show allLeaves order with bounded pagination,
retaining specials and skipping no episodes based on watched state. Autoplay is
Off by default; enabling it offers a cancellable ten-second countdown. Missing
series identity, no subsequent item or a server pagination failure disables it.
Completion threshold is profile-scoped, configurable 50–100%; completion scrobbles
are best-effort and do not establish server-side watch acceptance.

A token-free durable progress outbox coalesces latest per-item updates, rejects
stale acknowledgements, retries with bounded backoff, and revalidates metadata
with the freshly authorized profile/PMS before retry. Newer observed server
lastViewedAt supersedes older queued positions. Preserve mode-0600
`plexstation-progress.json` during updates; no auth token or playback URL is
stored there. Missing/unreliable PMS timestamps and device clock drift limit
cross-client reconciliation; it is not a transactional cross-device sync system.

### Protocol evidence

Inspected official Plex for Kodi commit
`97191f248a20223bb93296789c010ff2204a9c76`, plexnet/plexplayer.py,
serverdecision.py and audioobject.py: universal decision/start endpoints,
1000–1999 success range, copy/transcode stream decisions and music HLS path.
Inspected Python-PlexAPI commit `dea7cad9227ec976fa93c363c43c01b3cee305af`,
plexapi/base.py and myplex.py for stream parameters and cloud Watchlist semantics.
These references are source evidence, not live protocol verification against
the owner's server. No Plex credentials or live server were available here.

## Checkpoint D implementation — discovery and UI integration

Supplemental Home rows aggregate all accessible matching movie/TV libraries up
to a documented safe cap of 32 sections/category; server-provided aggregate hubs
remain preferred. Stable ratingKey deduplication retains distinct editions with
different IDs. Only the best 12 bounded normalized candidates remain in memory.
Audio queues likewise retain bounded identity/display/progress fields; playback
versions/streams are resolved afresh for each track instead of storing complete
Media trees for 2000 tracks.

Global Search requests real PMS search hubs, grouped into nonempty media types,
including artists/albums/tracks/collections. Audio results route to native audio
screens; known section IDs respect audiobook designation. Search uses async work,
stale-result rejection and paged requests. PMS versions may not implement the
same per-hub pagination; refine the query if paging does not advance. Audiobooks
are Plex music metadata; separate author/book type classification is not guessed
when the response lacks library identity. Cloud-wide catalog search is not added.

Library Square menu adds actual server-supplied genre/year choices, rating and
duration sorts, while preserving title/added/release, unwatched/all and Search.
PMS errors remain explicit. Music retains album/artist/track/genre browsing and
search; comprehensive type-specific filter combinations and normalized audiobook
narrator/series filters remain incomplete.

Music/Audiobook libraries also offer a Home shelf: actual recent tracks/albums/
artists, permitted playlists/favorites, and local Continue Listening for books.
Empty/unsupported content rows are omitted. Existing carousels/artwork/focus are
reused. Music browsing retains a compact Now Playing entry and Touchpad shortcut.
Album menu Play/Shuffle uses the current parent album, not just the focused track;
track listings display disc/track/duration and loaded album duration. Artist
biography is wrapped when present. Playlist order and duplicates remain intact.

### Actual cloud Watchlist, conditionally available

Settings → Playback / Quality / Subtitles → Check Plex Cloud Watchlist access
uses the **active validated profile token** against discover.provider.plex.tv.
Only a successful provider MediaContainer response enables Watchlist navigation.
Managed profiles remain disabled pending provider isolation acceptance; no owner
fallback is used. Primary credentials are never substituted for denied access.
Reading/paging and explicit Add/Remove detail actions use the real cloud API.
Mutations require an actual `plex://movie/...` or `plex://show/...` cloud GUID;
local ratingKeys are not mislabeled as cloud IDs. Provider 401/403/unsupported
responses are explicit limitations, not a fabricated local list.

Cloud items resolve by Plex GUID through the freshly authorized selected PMS
before local details/playback; unavailable items show a useful error. Add/Remove
are explicit user actions. Actual provider availability, GUID mapping, profile
attribution and writes are **not live-verified**. Local playlists/collections are
separate features and are never presented as Plex Cloud Watchlist.

### Intermediate CI limitation and corrected fixture

Audiobook run 39793484467 at 4fe5b10 failed its sanitizer controller gate before
native compilation. The fixture issued a book-play command while a previous
track timeline still owned the async worker; under ASan it was correctly deferred.
The fixture now drains prior work before issuing that command. No failed run is
presented as a published installation. Local ASan/UBSan subsequently passed all
nine suites. Common instrumented objects are compiled once without dropping any
instrumentation, and controller tests run first to expose async fixture issues
early. Combined native checkpoint results are recorded after actual completion.

## Release-candidate feature / acceptance matrix

| Area | Current implementation | Unverified or incomplete |
| --- | --- | --- |
| Music | Artist/album/track metadata, artwork/biography, recent/favorite/playlist shelves, genres/collections where PMS supplies them, audio-only VLC, queue ordering/shuffle/repeat/skip, mini-player | Actual codecs/audio/history on PS5; paused queue reconstruction across restarts; deeper music filter combinations |
| Audiobooks | Designated music libraries, authors/albums/tracks/playlists, local Continue Listening/completed, sequential tracks, chapters/fallback, speeds, seeks, resume/bookmarks, sleep timer | Normalized narrator/series schema, per-book speed override, complete bookmark browser, cross-app book progress synchronization |
| Video | Original, explicit server copy/transcode decision, quality caps/fallback, external subtitle adapter, next episode/countdown, completion threshold, per-media offset | Live HLS/session/segment authorization, hardware codecs, forced subtitles, bandwidth adaptation, physical timing/rendering |
| Cloud Watchlist | Actual conditional cloud list/Add/Remove, cloud GUID mapping to authorized PMS item | Live provider permissions/writes; managed users disabled; unavailable cloud media cannot play locally |
| Search/filter/Home | Grouped actual PMS hubs, cancellation/paged requests, genre/year choices and sorts, multiple-library supplements | Per-hub pagination varies; audiobook classification requires section identity; aggregation capped at 32 sections/category |
| Progress | Scoped timeline/scrobble, latest-update coalescing, stale-ack rejection, durable token-free retry, fresh metadata reconciliation | PMS timestamps/clock skew limit reconciliation; completion and session cleanup are best-effort |
| Isolation/stability | Profile changes stop playback, clear queue/cache/history and reject stale results; bounded metadata/artwork; PIN log privacy; matching symbols/checkpoints retained | Movies SIGABRT and unmatched SIGSYS remain unsolved physical reports; no PS5 acceptance inferred |

## Physical comparison and verification

1. Keep installation/debug artifacts for baseline d52e5cd offline. Record exact
   installed eboot hash/build. First run the baseline test planned by the owner.
2. Close PlexStation before deployment. Extract the outer Actions ZIP and inner
   installation ZIP; upload the entire PPSA85301 folder. Preserve prefs.txt,
   plexstation-account.json, plexstation-listening.json and
   plexstation-progress.json. Apply 755 directories/executables, 644 ordinary
   resources, 600 private credential/listening/progress files, with correct owner.
3. Recheck launch/auth/profile PIN/PMS fallback/controller, then the original
   Movies crash sequence. Compare text-only/no-download/ten-item isolation and
   collect redacted CP/fatal/gfx messages with the matching symbols/build ID.
4. Music: browse artists/albums, missing artwork, multi-disc and repeated-track
   playlists; play real MP3/AAC/FLAC/ALAC/WAV/Vorbis/Opus files independently.
   Check sound without a video track, pause/seek, next/previous, repeat/shuffle,
   queue edits, mini-player/browsing and a long listening session. Verify PMS
   history belongs to the correct Home user; do not infer bit-perfect output.
5. Designate an audiobook library for one profile/server. Test single-file M4B
   chapters, multi-track MP3/FLAC books, track fallback, restart/resume, all rates,
   pitch, ±10/30s, bookmark restore, chapter/track timer and app restart. Verify
   local-only labels, wrong-profile/server isolation and newer PMS progress.
6. Video: Original first, then each quality cap and a known remux/transcode case.
   Inspect returned mode and real PMS session, HLS segment authorization, stop
   cleanup, multiple versions, SRT/WebVTT/ASS, tracks/languages/forced behavior,
   subtitle delay/size and audio ±offset after seek/restart. Check 1080p/4K output.
7. Autoplay: actual server episode order including specials, next action, ten-
   second countdown, Cancel, final episode and restricted/missing episodes.
8. Disconnect network while playing; inspect saved progress files locally,
   reconnect, ensure stale retries cannot replace newer server/profile progress.
   Never share those files without reviewing personal metadata.
9. Search all media types, large/empty libraries, filters, multiple-library rows,
   long titles and rapid navigation/cancellation. Check cloud access explicitly;
   verify Add/Remove in Plex itself and unavailable-item behavior. Managed-user
   cloud controls must remain unavailable. No local fake Watchlist is used.
10. Switch profiles during audio/video/pending requests and sign out. Verify old
    playback stops, queues are cleared, restricted libraries stay inaccessible,
    private credentials remain protected and original profiles retain preferences.

These are pending physical/live checks, not automated pass claims. Runtime
requires the existing firmware 13.40 / kstuff Lite / ShadowMountPlus 1.7 Beta 5
Fix 1 environment, plus the complete packaged fonts/certs/shaders/libc assets.
No new controller/browser library or replacement jailbreak is introduced.

### Baseline archive workflow

`Archive PS5 physical-testing baseline` downloads the original run 39702538057
installation, debug and diagnostics via GitHub's read-only Actions token. It
requires the exact original commit, successful validation report, matching ELF
hashes and matching installed eboot/libc hashes before publishing
`PlexStation-physical-baseline-d52e5cd39ad6689f1f917179a6d1761132adde03`.
No source is regenerated and no executable is rebuilt. Requested retention is
90 days, subject to repository policy; download for permanent offline retention.
A fixture checks identity matching and rejects substituted symbols. This workflow
is only a preservation mechanism; its queued/unrun state is not an archived pass.

## Checkpoint commits and build status

- A Music: `b3cc83265d63c84590e8030b268eeb904daa037a`;
  [native success 39705677578](https://github.com/tattedbeardo89/PlexStation/actions/runs/39705677578).
- B Audiobooks: `4fe5b10`;
  [39793484467](https://github.com/tattedbeardo89/PlexStation/actions/runs/39793484467)
  failed the sanitizer fixture before native compile; corrected later.
- C Advanced playback: `25317e7`;
  [39794750214](https://github.com/tattedbeardo89/PlexStation/actions/runs/39794750214)
  queued at this record.
- D Discovery/UI: `1721e591486f4b6c2ab4a737f6713f3384fbb568`;
  [39796324867](https://github.com/tattedbeardo89/PlexStation/actions/runs/39796324867)
  queued at this record. Full local regular and nine-suite ASan/UBSan regression passed.

For an owner-triggered build: Actions → Build PlexStation PS5 → Run workflow →
`feature/plexstation-initial-implementation` → Run workflow. A green successful
run publishes `PlexStation-PPSA85301-<built-SHA>`,
`PlexStation-native-debug-<built-SHA>`, and
`PlexStation-build-diagnostics-<built-SHA>`. A queued run has no installation;
a failed run publishes diagnostics only. Public GitHub metadata is available in
this session, but authenticated artifact downloads/dispatch API are unavailable.
Pushing source already triggers the existing workflow; no native success is
inferred from a push or passing host test.
