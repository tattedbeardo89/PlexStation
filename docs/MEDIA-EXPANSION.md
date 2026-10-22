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
