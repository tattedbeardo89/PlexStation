# Coordinated stability, profiles and TV interface delivery

This is an implemented native client update, with host fixtures and native build
gates. It is **not a claim of physical PS5 acceptance or complete Plex parity**.
No screenshot was attached to this request; layout follows the written near-black,
charcoal, gold, poster/carousel and controller requirements using original
PlexStation branding and vector UI elements. Visual matching/tuning still needs
the reference and a real 1080p/4K display check.

## Implementation status

| Area | Implemented behavior | Remaining verification / limits |
| --- | --- | --- |
| Native stability | CP01–CP15 traces, independent download/decode/upload/text/ten-item modes, bounds, stale-result rejection, atomic page replacement, resource cleanup | Confirmed Movies SIGABRT is still not reproduced/symbolized here; no physical fix claimed |
| SIGSYS | Separate target signal capture, raw registers, build SHA and thread/phase evidence | Unknown installed build and same-launch milestones; do not conflate with 2458e45 SIGABRT |
| Debugging | Matching unstripped ELF, converted ELF, map, symbols, build IDs/hashes, guarded LLVM symbolizer | Shared-module frames need matching module/load map; inherited context offsets need device validation |
| Plex Home | Actual users, JSON/XML API handling, masked PIN switch, independent identity validation, actual permitted HTTPS avatars/fallback | Live Plex Home API versions, PIN behavior and managed-user resource permissions pending |
| Isolation | Primary token stays private; switched tokens memory-only; profile-scoped discovery, server tokens, metadata, playback URLs, timeline/watched updates | Real server-side attribution/sharing must be tested; no offline PIN bypass/admin fallback |
| Persistence | Account/profile UUID-or-ID keys, remembered profile/PMS/audio, protected restore prompts for PIN, startup-picker option, Change Server, retry/cancel | Device restart/update persistence pending; preserve both runtime files |
| Home | Real Continue Watching first, progress/series/episode/remaining-time labels, nonempty hubs, recently added movies/TV, release-date query, On Deck | Endpoint support varies; no invented content; dedicated CW can fall back only to actual server Continue Watching hub |
| Home fallback scope | Server-provided aggregate hubs retained; supplemental recent/released queries use first accessible movie/TV section | Does not merge all supplementary queries across every section; all movie/TV sections remain browsable |
| Movies | Real paginated metadata, poster grid, year/watched/progress, lazy images, title/date/added sorts, unwatched/all filter, search | Genre/rating filters beyond these basic controls are not implemented |
| TV | Show details → seasons → season details → episodes → episode details; real thumbnails/series/runtime/progress | Physical hierarchy/focus testing pending; no automatic next-episode playback |
| Details | Poster/backdrop, title/summary/year/runtime/rating/genres/cast, scrollable description, Play/Resume, watched toggle, media versions | At most 8 versions, bounded metadata; original-quality single-part H.264/HEVC MKV/MP4 adapter remains |
| DualSense | Existing pad/ImGui integration, carousel navigation/scrolling, details focus restoration, settings and native PIN keypad | Physical controller/frame pacing pending; no mouse/keyboard required by implemented flows |
| PIN privacy | UI, main-loop and PS5 raw-pad diagnostics suppressed during keypad entry; digits masked and cleared | Review real-device logs before sharing; credential files must never be uploaded |
| Developer Options | Overlay default Off after old-build upgrade, server/library/artwork diagnostics, isolation flags, build information | Triangle server diagnostics remains independent; isolation flags default Off |
| Resource limits | 60-result requests (10 diagnostic), max 200 parsed entries if server ignores pagination, 12 hubs × 12 cards, 12 textures, one fetch/decode worker, 24 history pages | No whole-library artwork preload; server JSON/body limits still apply |
| Playback | Existing native VLC/decoder/audio/track/pause/seek/resume path, active-profile token and coalesced timeline | No PS5 playback/A-V sync/codec acceptance yet; no transcoding/Direct Stream/quality adaptation or durable offline progress queue |
| Conditional navigation | Movie/TV/Continue Watching/Search/Settings/Profile | Cloud Watchlist is not implemented and is hidden; Music/Audiobooks unsupported by current video adapter and not exposed |

The original successful application implementation and title PPSA85301 are
preserved. No jailbreak replacement, new controller library or browser runtime
was introduced. Dependency/toolchain pins and cache fingerprint are unchanged.

## Controls

- D-pad/stick navigates; Cross selects; Circle backs out; L1/R1 changes sections.
- Home: Up/Down chooses a row, Left/Right a card; Left at the first card enters
  the sidebar. Cross opens details; Back restores row/card focus.
- Sidebar: Up from Home focuses the account/profile name; Cross opens
  **Profile → Switch User**. Also **Settings → Profiles**.
- Library: Square opens title/date/added sorts, unwatched/all filter and Search.
  L2/R2 pages. Movie/episode Cross opens details; shows/seasons open their details
  with Browse Seasons/Episodes actions.
- Details: Cross selects Play/Resume, watched state or a version; Square opens
  the full bounded description, with Up/Down scroll and Circle/Square close.
- Playback retains Cross pause/resume, directional seek, Options tracks,
  L1/R1 audio delay and Triangle reset. These remain device-test requirements.
- Options opens Settings. Developer toggles can be changed while Home loads.
  Profile switches discard stale work and stop old playback.

## Build and download

**Latest successful combined build:** [run 39702538057](https://github.com/tattedbeardo89/PlexStation/actions/runs/39702538057),
commit `d52e5cd39ad6689f1f917179a6d1761132adde03`.

- Installation: `PlexStation-PPSA85301-d52e5cd39ad6689f1f917179a6d1761132adde03` (37.4 MB).
- Matching symbols: `PlexStation-native-debug-d52e5cd39ad6689f1f917179a6d1761132adde03` (86.4 MB).
- Logs/report: `PlexStation-build-diagnostics-d52e5cd39ad6689f1f917179a6d1761132adde03` (187 KB).

Host regression/ASan/UBSan, real native compilation, complete packaging, debug
collection and ZIP permission gates passed. Physical crash resolution, live Plex
Home permissions and playback acceptance remain unverified. Documentation-only
commits after this build do not change its executable.


Actions → **Build PlexStation PS5** → **Run workflow** → branch
`feature/plexstation-initial-implementation` → **Run workflow**. Wait for green
success. Under Artifacts download `PlexStation-PPSA85301-<built-commit>`;
`PlexStation-native-debug-<built-commit>` contains matching symbols, and
`PlexStation-build-diagnostics-<built-commit>` contains host/native logs/report.
Failed builds publish diagnostics and no installation. The pipeline now runs
core/auth/Home/artwork/controller/transport/connection/syntax and ASan/UBSan
gates before the actual PS5 cross-build. Fixtures are not live Plex acceptance.

The independently deployable stability-only build and exact debug artifact are
linked in [CRASH-DIAGNOSTICS.md](CRASH-DIAGNOSTICS.md), run 39697533557. Current
combined native validation is recorded in [VALIDATION.md](VALIDATION.md).

## Windows deployment and physical acceptance

Close PlexStation. Extract the outer Actions artifact ZIP, then the inner
installation ZIP. Upload the entire PPSA85301 folder; preserve `prefs.txt` and
`plexstation-account.json`. Apply remote POSIX permissions/ownership as in
[CONTROLLER-TESTING.md](CONTROLLER-TESTING.md): 755 executable/directories,
644 ordinary files, 600 private account file. Windows extraction/FTP does not
reliably preserve Unix modes. Do not permanently use 777.

1. Record the exact installed commit and eboot SHA256. On firmware 13.40,
   kstuff Lite, ShadowMountPlus 1.7 Beta 5 Fix 1, verify launch and main UI.
2. Test the separate Checkpoint A build entering Movies normally; on failure
   compare text-only/no-download/10-item mode, then re-enable one artwork stage
   at a time. Retrieve `plexstation.log`, `plexstation.old.log`, `vlc-crash.txt`
   and ShadowMountPlus's corresponding `debug.log`; keep signals/builds separate.
3. In the combined build, verify actual Home users/avatars, primary and managed
   users, wrong/right PIN, Cancel, switch user and fresh profile identity.
4. Check restricted libraries and different real Continue Watching/progress for
   each profile. Play/pause/seek/stop and confirm server-side history attribution;
   never assume fixture token checks prove permissions on your server.
5. Select a PMS once; restart with the picker On and Off. A protected restored
   profile must still ask for PIN. Verify retry/offline/missing PMS, endpoint
   fallback, Change Server and no silent alternate selection.
6. Browse Movies, show details/seasons/episodes, sorts/filter/search, paging,
   missing/corrupt artwork, repeated entry/Back and rapid navigation while loading.
7. Test horizontal/vertical Home focus, long titles, details return, native
   playback return, tracks/audio delay and full description, at 1080p and 4K.
8. Toggle the developer overlay, then restart/update preserving both runtime
   files. Verify hidden default on an older-build migration and persisted choice.

Do not send credentials, PINs, tokens, raw response bodies or playback URLs.
For native crashes send only reviewed/redacted phase/error/crash evidence and
the installed build identity. Physical root-cause resolution is still pending.
