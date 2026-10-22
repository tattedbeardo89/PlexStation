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
