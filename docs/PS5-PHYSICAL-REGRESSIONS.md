# Physical PS5 regression checkpoint

This continues draft [PR #1](https://github.com/tattedbeardo89/PlexStation/pull/1)
on `feature/ps5-ui-artwork-stabilization`. Inspection found no merged successor:
integration remains `e79d5bd3a882cb6c841de4d0a8cd084bd1eb840a`; the review branch
was `f5099913c57e47c4014a2346392be3868156015b`. The owner physically tested native
source `f5f34a412573710ce95e37e16d7e92105a820c30` on PS5 13.40 and reported the
regressions addressed here. That is evidence of the reported symptoms, not
acceptance of these new changes. Earlier `5d8adb9` and `1ad4f6c` remain preserved.
No merge, release, console connection, deployment, uninstall or database change
is performed by this checkpoint.

## Root causes and resulting behavior

### PS5 shell icon: cause still requires console evidence

The packaged angular icon is a 512×512 RGB PNG. Its expected SHA-256 remains
`d6b3eaac084816c97312bdc36a29b31493efb37fad43616b43d7a4dc9a1d14ec`.
The previously delivered build used those same bytes. Package correctness cannot
prove which image the PS5 shell/launcher registered, which staged copy launched,
or whether a different registration/cache is displayed. **The actual stale
source has not been established and the physical logo is not claimed fixed.**

The owner provided a historical LAN address, `192.168.7.9`, FTP port `2120`, but
explicitly prohibited this environment from connecting to or modifying console
files. The new standalone [Windows-compatible diagnostic](../tools/ps5-artwork-report.py)
therefore runs on the owner's computer. It uses Python 3's standard library.
It lists bounded known parent directories and reads only PlexStation artwork
and title metadata. No credentials, settings, saves, system database, or unrelated
title content is read. FTP operations are login/list/read/quit; there are no
upload, rename, delete, registration, cache-clear or directory-creation commands.

It compares local package artwork with `/app0` only after matching title metadata,
PPSA85301 staging roots, matching `/mnt/sandbox/PPSA85301_NNN/app0` mounts, and
title-specific `/user/appmeta` and `/system_data/priv/appmeta` artwork where
readable. Duplicate matching mounts/artwork directories are reported separately.
An exact title-scoped additional `--root` can be provided for another launcher
location. It does not infer database registrations from directory counts.
Missing/denied files and inaccessible FTP are reported honestly. Reads are bounded
to 32 title roots, 2,048 entries per parent listing, and 12 MiB per artwork file.
The report includes full inspected paths, SHA-256, PNG dimensions/format, and
comparison findings; passwords/tokens/server exception messages are excluded.

### Server selection

`Resources` completion previously auto-connected only a remembered identity.
A fresh user with one valid server still reached the picker. Resource parsing
also treated duplicate device records as different servers. Records now merge
by `machineIdentifier` (Plex resources' `clientIdentifier` fallback), with unique
connection URIs. Multiple addresses are routes to one server. Empty-token records
are excluded from selectable resources.

A remembered authorized server is attempted first using the existing verified
endpoint/identity/library checks. Otherwise a single authorized server with a
permitted connection auto-connects; actual identity and profile-authorized sections
must validate before its preference is saved. An offline attempt stays in the
retry/unavailable state and does not substitute credentials or an unauthorized
server. Multiple identities retain the picker. Settings → Server → Change Server
explicitly bypasses automatic selection. No additional resource-discovery sweep
is introduced; the cached connection path remains intact.

### Authorized sidebar and library classification

The previous sidebar primarily showed fixed Movies/TV/Music/Audiobooks entries
plus manually pinned video libraries. It did not represent every accessible
library or consistently hide unconfigured destinations. The new sidebar derives
individual destinations from the selected user's authenticated PMS sections:

1. Continue Watching
2. Home
3. Authorized movie libraries, individually named
4. Authorized TV libraries
5. Music-classified libraries
6. Audiobook-classified libraries
7. Other authorized libraries
8. Watchlist, Search, and authorized configured Personal DVR
9. Settings

Each route uses its actual server/library section ID. Library order preferences
remain ID-based and are applied within the required media groups. Visual and
controller navigation share the same ordered destinations. Per-library visibility
preferences are user/server/section scoped. Continue Watching, Home and Settings
remain available. The former Free Live TV ID is reserved/hidden and has no page.

Profile changes clear sections, listings, active library and private presentation.
Server changes rebuild them from that user's authenticated response. Initial Home reuses the sections just verified during connection, avoiding a
redundant startup request. Subsequent Home refreshes re-fetch authorized sections
before composing metadata; Settings →
Libraries → Refresh accessible libraries also performs that refresh. Selection
survives section reordering by stable ID. Removed destinations fall back to Home;
removed active libraries clear their listing/history/audio browse state. A 401/403
clears the current accessible-library presentation rather than keeping another
user's or revoked metadata. Network failure does not borrow administrator tokens.
Presentation cache schema v4 isolates the new composition and current section set.

Plex reports both music and audiobook libraries as `artist`. Classification was
already a persistent per-section flag, but the previous controls did not expose a
clear list of all eligible libraries and both unclassified destinations appeared
under Music. Settings → Libraries → **Library Classification** now lists every
authorized music-type library and toggles each explicitly between Music and
Audiobooks. Keys retain the existing user/server/section-scoped format, so prior
designations migrate without modifying Plex metadata. Names never silently
override a saved choice. Multiple classified libraries remain individually
accessible in the sidebar. Restarting UI/profile switching does not erase choices.

Audiobook browsing retains book covers, Authors, Continue Listening, Recently
Added, collections/genres and supported local bookmarks. Existing local listening
state remains separate from Plex progress; no new server chapter/bookmark API is
claimed. Native music/track playback paths are preserved.

### Home composition and duplicate library rails

Home previously accepted extra/default/scoped hubs as independent rows, appended
On Deck and collections, and let server response order affect composition.
It now has exactly these stable semantic slots, omitting empty slots in place:

1. Continue Watching
2. Recently Released Movies
3. Recently Released Episodes
4. Recently Added Movies
5. Recently Added TV Shows
6. Seasonal Recommendations

There is no On Deck, separate Home Movies, arbitrary hub or collections rail on
Home. Authorized sources aggregate into one row per semantic slot. Explicit
unauthorized section IDs are rejected; relative authorized section requests retain
their source section IDs. Items deduplicate by media GUID when present, otherwise
server media type/rating key. Date sorting uses legitimate original release/air
dates for Released and `addedAt` for Added. Added TV queries actual series
(`type=2`), not renamed imported episodes. Completed/nonresumable Continue Watching
items are excluded; explicit authorized resume state with valid remaining progress
is retained. Seasonal rows are appended once, last, preserving full artwork fields.
Partial snapshots and warm reconciliation use the same closed order.

The library composer previously recognized `continueWatching` but missed Plex's
`movie.inprogress`/`tv.inprogress` and title-only aliases. It also treated a TV
recently-added episode hub as a second added rail beside generated series entries.
Aliases now normalize before page insertion. The manually generated top Continue
Watching and first series-level Recently Added rows take precedence. Movie/episode
types, section IDs, progress and artwork remain attached to their original items.
If only a server fallback supplies Continue Watching, it is inserted at the top
even when other recommended rows have already been composed. Connecting a server
also clears the prior library/tab context before opening its Home.

### Blank initial pages

The native library tab-focus branch returned before `home_frame`/grid drawing.
While `library_tabs_focus` remained set, only navigation/header surfaces painted;
D-pad Down cleared that flag, making the body appear. This is a concrete native
condition matching the observed input dependency. There is no React/QuickJS
renderer in this implementation.

Tab focus now controls input ownership, never whether the body paints. Activation
resets tab focus, row columns/scroll, and vertical position. Completed data is
reconciled before the frame draws, and bounded skeleton cards show during fetches.
Text/placeholder layout does not wait for images. Later texture readiness appears
on the next continuously scheduled frame. No synthetic input or forced Down event
is used. Existing refresh focus is retained by row/item identity.

Regression tests call the actual native UI with no input, complete asynchronous
fixtures, and software-rasterize completed production ImGui commands including
clips, textured triangles, the bundled Inter font and mock uploaded artwork.
They validate visible image pixels at 1920×1080 and 3840×2160 for all three pages,
including retained tab focus and delayed artwork. Six compact BMP views are retained
in the installation artifact's `build/` folder alongside its `artifacts/` package
and checksum folder. Logs are in the separate diagnostics artifact. These are
host-rendered fixture frames, **not PS5 GPU screenshots**;
the console fix still requires owner acceptance.

### Text-only categories and Live TV decision

Genre cards no longer request/display illustrated genre icons. They draw the
actual category label with centered native typography, proportional wrapping,
UTF-8-safe clipping, restrained theme background and focus outline. Arbitrary
labels use this same path; no genre-name list determines whether a card renders.
Latin extended, Greek, Cyrillic and Vietnamese font ranges are explicitly loaded
from bundled Inter; Unicode strings are retained without byte-splitting. Rendering
other scripts depends on the bundled font's glyph coverage; no universal text
shaping engine is claimed. Long labels shrink/wrap and elide safely if necessary.
The sample SVG masters/PNG resources are now text-only; unknown-category SVG
templates escape/preserve Unicode. Native packaging copies reproducible checked-in
PNGs and adds no image-library dependency to the native toolchain.

The prior investigation of the official [PMS API](https://developer.plex.tv/pms/)
and [Plex Free TV offering](https://www.plex.tv/watch-free-tv/) established only
supported personal-tuner/DVR contracts, not an authorized complete third-party
free-channel catalog/session/advertising/playback contract. No new entitlement or
authorized interface was supplied. Consequently the nonfunctional free cloud-TV
entry and placeholder page are removed. No scraping, DRM/auth/geo/ad bypass or
free-channel playback claim is made. Personal DVR remains separate and appears
after a bounded asynchronous active-user DVR probe returns configured authorized
tuners. That probe never occupies the navigation job. Existing channel, guide,
tune, scheduled-recording and explicit cancellation paths remain unchanged.

## Reproducible host performance observation

[Raw samples](PHYSICAL-REGRESSION-MEASUREMENTS.json), reproduced with
`bash host/benchmark-startup.sh`, use five runs per variant and a controlled
10 ms/request transport. The original integration feed at e79d5bd issued 15
requests and had median complete metadata at 152.854 ms. This six-slot feed issues
10 requests, publishes first usable metadata at median 10.273 ms and completes
at median 102.158 ms. The reduction removes no-longer-requested extra Home rails;
it is not a total PS5 boot, live Plex latency, decoder or GPU performance claim.
Initial connection also avoids re-fetching the library sections just verified by
Select/FastConnect. Cold/warm console phase logs remain the real-device evidence.

## Windows read-only artwork command

Requires Python 3, with no additional packages. In PowerShell, download the tool
and provide the actual local path of the downloaded installation ZIP (either the
GitHub outer artifact or its inner title ZIP is accepted):

```powershell
Set-Location "$env:USERPROFILE\Downloads"
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/tattedbeardo89/PlexStation/feature/ps5-ui-artwork-stabilization/tools/ps5-artwork-report.py" -OutFile ".\ps5-artwork-report.py"
py -3 .\ps5-artwork-report.py --host 192.168.7.9 --port 2120 --package ".\PlexStation-PPSA85301.zip" --output ".\PlexStation-artwork-report.json"
```

FTP must already be running; this tool does not start it. Omit `--package` if the
ZIP is not available. For an unusual known installation path, append for example
`--root "/data/homebrew/PPSA85301"`. Anonymous FTP is the default. If the server
requires a login, `--user YOUR_FTP_USER` prompts privately for a password; neither
is written to the report. Do not paste credentials or raw private account files
into chat. The resulting artwork-only JSON is the evidence needed to identify
which readable copy differs.

## Backup-first installation and artwork recovery

No uninstall/reinstall or registration/cache mutation is presently justified by
the available evidence. Use the report before choosing a recovery action:

- Packaged bytes differ from expected: use the verified artifact and validate its
  inner `SHA256SUMS`; do not diagnose console caching from a bad package.
- Staged/mounted bytes differ from the package: identify the actual launcher root
  from the report before copying the full verified title to that intended root.
- Mounted bytes match but registered artwork differs: a title-specific launcher
  registration refresh needs the launcher's verified documented procedure. No
  safe registration API has been established here; do not edit app databases.
- All readable copies match: the report cannot prove a cache cause or exclude an
  unreadable launcher source. Preserve evidence and test normal launcher/console
  restart behavior before considering a reinstall.

Before any owner-performed full-title update, close PlexStation and make a private
read-only FTP download of the entire **actual** PPSA85301 title root identified by
the report, plus `/data/plexstation-client` if present. Retain `prefs.txt`,
`plexstation-account.json`, `plexstation-listening.json`, `plexstation-progress.json`,
bookmarks/listening state, local playlists and other existing title-local user
files. A full-root backup avoids guessing a launcher's data placement. Keep these
backups private: they may contain credentials. The artwork diagnostic intentionally
does not perform or include that backup. Do not overwrite/delete unrelated titles,
PPSA85300, jailbreak files, or another PPSA85301 launch copy.

Extract the installation artifact. Its `artifacts/` directory contains the inner
`PlexStation-PPSA85301.zip`, `SHA256SUMS` and validation metadata; its `build/`
directory contains host fixture BMPs. Verify the inner ZIP against `SHA256SUMS`,
then extract that ZIP preserving its PPSA85301 root and Unix permissions. Only
the extracted PPSA85301 application folder is intended for the console. With the app stopped,
update the intended title's application resources while retaining backed-up user
files and `/data/plexstation-client`. Preserve directory/executable modes 755,
resource modes 644, private files 600 and ownership. Retain exact matching debug
symbols with the tested build. Do not use a blanket delete-before-copy/uninstall.
If a launcher ultimately requires reinstalling registration, stop for its exact
documented procedure and verify where it deletes persistent data; that destructive
step is not authorized or automated by these tools.

## Physical acceptance checklist

- Record installed source SHA; launch on PS5 13.40 and run the Windows artwork
  report. Confirm the shell icon visually after any separately chosen recovery.
- Fresh one-server profile auto-connects; remembered online/offline and multiple
  server behavior stays correct; Settings can reopen the picker.
- Compare administrator/managed/PIN users' sidebar against their permitted section
  IDs. Test multiple movie libraries, server changes, section reorder/removal and
  explicit accessible-library refresh. No other user's names/items may appear.
- Confirm the six Home slots/order, omissions for empty sections, no On Deck/Home
  Movies/extra rows, and no duplicates after repeated warm refreshes.
- Movies/TV keep one top Continue Watching; TV has one series-level Recently Added.
  Confirm release vs import dates and episode stills/progress.
- Open Movies, TV and Home Movies without touching the controller at 1080p/4K;
  see loading cards, rails, then images. Test tabs, back/return, first/last focus.
- Check text-only standard, long and Unicode category labels at TV distance.
- Classify each authorized music-type library in Settings; verify separate
  Music/Audiobooks destinations across restart, user/server changes and playback.
- Free cloud TV is absent; authorized Personal DVR, guide, tune and recording
  management still work on a configured supported server.
- Regress authentication/PIN, server persistence, native video/audio, subtitle
  preselection/runtime changes, audio track selection, Watchlist, Search, seasonal
  rows, artwork cache/retention, controller and private preferences.

## Completed hosted checkpoint

Final native source: **`53a766675c42d5b4f6b1496f94c3b56ad32a1653`**.
[Workflow 40211142558](https://github.com/tattedbeardo89/PlexStation/actions/runs/40211142558)
completed successfully. All 17 required hosted regression suites passed, including
ASan/UBSan, controller/pad, initial-frame rasterization, profile/PIN, artwork,
presentation, subtitles, cached connections, native GnuTLS, strict native source,
crash/symbolization, package identity/assets and ZIP permissions. Real native
compilation, signed SELF/static VLC gates and exact matching debug collection
also passed. Six completed initial-frame cases passed in both the normal and
sanitizer controller runs. The earlier eaf82d1 checkpoint also passed native
[run 40210604266](https://github.com/tattedbeardo89/PlexStation/actions/runs/40210604266).

- [Final installation and fixture-frame artifact](https://github.com/tattedbeardo89/PlexStation/actions/runs/40211142558/artifacts/12606027477)
- [Exact matching debug symbols/build.json](https://github.com/tattedbeardo89/PlexStation/actions/runs/40211142558/artifacts/12606326998)
- [Final diagnostics and validation.json](https://github.com/tattedbeardo89/PlexStation/actions/runs/40211142558/artifacts/12606177297)
- [Source, checksums, workflow and verification evidence](PHYSICAL-REGRESSION-BUILD-EVIDENCE.json)

The diagnostics archive was independently downloaded and SHA-256 verified, then
checked for the exact final source/title/success and all required host logs.
Installation/debug archive digests come from GitHub Actions; inner `SHA256SUMS`
and debug `build.json` retain package/executable/runtime/ELF identities verified
in CI. Those larger archives were not independently re-downloaded here because
they exceed the file tool's 32 MiB limit. GitHub downloads may require sign-in.
Documentation-only follow-ups can advance the PR head without changing the native
source above. [PR #1](https://github.com/tattedbeardo89/PlexStation/pull/1) remains
draft and unmerged. No hardware pass is inferred from these successful checks.
