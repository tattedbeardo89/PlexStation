# Physical PS5 acceptance — current audit candidate

No check below passes merely because a host fixture or native compilation passes.
Use PS5 13.40, kstuff Lite, ShadowMountPlus 1.7 Beta 5 Fix 1, DualSense and
PPSA85301. Keep `5d8adb9` and `1ad4f6c4ab562762bd4c2ffa958cc098e234528a`
for comparison. Record the installed build SHA, package/debug hashes and media
codecs/version, active profile and actual PMS mode for every failure.

## Install and retain evidence

Close PlexStation. Extract the Actions outer ZIP, then the inner installation
ZIP. Upload the entire PPSA85301 title; never overwrite VLC PPSA85300. Preserve
`prefs.txt`, `plexstation-account.json`, `plexstation-listening.json`,
`plexstation-progress.json` and `/data/plexstation-client` caches. Do not delete
runtime data when updating. Use correct ownership, directories/executables 755,
resources 644 and private account/listening/progress files 600. See the existing
Windows FTP guide. Keep matching native-debug artifacts offline, including all
ELFs/modules/maps/build IDs. Debug artifacts are not an installation package.

Refresh a stale shell icon by closing the title and restarting the launcher or
console normally; do not delete system databases or saved data.

## Regression and acceptance checklist

| Test | Expected behavior | Physical result |
|---|---|---|
| Existing launch/auth/profile/PMS/native video | Confirm previously accepted behavior still works | Pending |
| Fresh movie/episode | Captions initially Off, correct resume position | Pending |
| Details → Subtitles → track → Play | Selected embedded or supported external track appears after discovery | Pending |
| Options → Tracks → Subtitles | Switch language, Off, switch back without new media-open or position reset | Pending |
| Playback Information → Subtitle tracks | Same session selector and uninterrupted audio/video | Pending |
| Re-select external track | Reuses attached VLC track, no timeout/reopen/re-download | Pending |
| Off during external load | Late completion cannot re-enable captions; seek/pause keep Off | Pending |
| Missing/ambiguous/unsupported subtitles | Visible failure and Off; native track choice remains available | Pending |
| Profile defaults/version/media memory | No cross-profile or cross-version track reuse; fresh unrelated media stays Off | Pending |
| Seek/pause/audio track/delay | No new drift or loss of video; test ±500 ms and reset | Pending |
| Original / copy / transcode / quality | Confirm actual PMS sessions and HLS segments; caps are not bandwidth adaptation | Pending |
| Who's Watching/sidebar/avatar retry | Actual PNG/JPEG avatar or useful stage-specific error; Square retries picker | Pending |
| Artwork scroll/return/restart/offline | Healthy posters remain; disk hits rise without repeated downloads | Pending |
| GPU pressure/rapid navigation | No descriptor/image-use crash, valid focus and bounded cache | Pending |
| Warm/cold startup | Five runs each, report profile-submit → populated Home from safe startup metrics | Pending |
| Offline saved route / changed remote route | Bounded fallback, identity/current-library validation and responsive input | Pending |
| Protected/restricted profile | PIN never bypassed; no other profile's Home/artwork/progress | Pending |
| Idle Home refresh | Visible rows remain, focused hub/item persists after a server update | Pending |
| Movies/TV defaults | Direct Recommended entry; switch tabs and Change Library persist correct IDs | Pending |
| Categories → genre → movie/show | Real genre/collection, playable details and normal TV hierarchy | Pending |
| Home rows/episode cards | Real collections/seasonal results, series title and S0/E where applicable | Pending |
| Home Movies/rename/pin | Stable ID after rename, own navigation, landscape thumbnail/date/duration | Pending |
| Watchlist | Actual account items/count/filter/sort/pages; add/remove reflected in Plex | Pending |
| Unavailable Watchlist/managed user | Explicit cloud error/retry; no owner's token or local fake list | Pending |
| Music | Default/library/categories/playlists, real sound, queue/shuffle/repeat/history | Pending |
| Audiobooks | Designated library, chapters/multi-file/resume/bookmarks/rate/timer; separate local semantics | Pending |
| Music ↔ audiobook isolation | Ordinary tracks cannot inherit book handling from a prior screen | Pending |
| Sidebar | Requested default order, hide/reorder, stable library pins and reset | Pending |
| Seasonal settings | Disable and select seasons; only actual matching permitted media appears | Pending |
| Personal Live TV | Authorized channels/logos/current/upcoming guide/tune/stop; verify tuner release | Pending |
| DVR schedule/rules | Server's real lists; first Cross only confirms, Circle preserves rule; owner confirms cancellation before second Cross | Pending |
| DVR authorization/failure | Denied or unsupported endpoint gives an error; no silent success/mutation | Pending |
| Free cloud TV/Rentals | Accurate unavailable states; no DRM, prices or entitlements fabricated | Pending |
| Angular icon/splash | Correct original navy/indigo artwork packaged and visible in shell | Pending |
| 1080p/4K/large/empty/corrupt/network | Readable layout, reachable controller actions, no stale result or crash | Pending |

Recording-rule cancellation deletes the entire subscription and all its scheduled
grabs, as stated by the official API. It is not a single-airing skip or deletion
of completed media. Create/edit recording rules in Plex's supported application;
template/storage-location setup is not implemented in this client.

## Private diagnostic collection

For avatars, collect only `avatar: request=... stage=...` and `avatar: phase=...`
lines plus the build SHA. They identify HTTP status, TLS state, redirects,
content format, byte count, decode, disk and GPU outcomes without printing URLs,
tokens, PINs or response bodies. Include the visible request number/error.
A WebP/SVG error establishes an unsupported image response, not a TLS or GPU
failure. A healthy decode with failed upload points to a different stage.

For startup, record `startup: authorized-server-ms` and `startup: first-home-ms`
with cached-route/cached-home flags, five cold and five warm runs. Report median
and range separately. Host fixture CPU timings are not PS5 startup measurements.
For caches, use Developer → Library / Artwork Diagnostics and Settings →
Interface → artwork storage; compare disk/GPU hits, downloads and failures.

For crashes, retain the original first fault and matching symbols/load markers;
do not infer a syscall/decoder cause from a secondary signal. Review all logs
before sharing. Never upload credential files, private cached metadata, raw
account responses or authenticated playback URLs.
