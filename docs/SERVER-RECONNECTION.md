# Remembered server and automatic reconnection

Current startup first attempts the last verified endpoint after fresh profile/PIN
authorization, before full resource discovery. It still checks TLS, server
identity and authenticated current library access, then falls back to discovery.
Fresh authorized candidates prefer the previous successful route. Idle Home
refresh preserves row/item focus. See [the current audit and metrics](FULL-IMPLEMENTATION-AUDIT.md)
and [cold/warm physical measurement procedure](HARDWARE-ACCEPTANCE.md). The older
checkpoint narrative below predates these optimizations.


PlexStation validates the restored private account token with Plex before
loading servers. The validated primary account ID and freshly validated active Plex Home profile
identity scope the preferred PMS machineIdentifier in the existing `prefs.txt`
settings. Primary-account-only preferences are migrated only for the validated
owner profile; another Home user cannot inherit them. No account/server
tokens are written to preferences. Each account retains its own selection
across sign-out. Newly linked accounts are validated before using preferences.

After successful identity verification **and authenticated library access**, the
selected server becomes that account's preference. Startup matches only that
machineIdentifier in freshly authorized resources and uses the existing HTTPS,
TLS verification, identity and library-access checks with endpoint fallback.
There is no automatic selection of another PMS. If the preferred server is
absent, the picker explains this and retains the preference. A connection
failure offers Cross to retry, Square to rediscover/change servers, and Circle
to return to the picker. Triangle opens existing redacted endpoint diagnostics.

Discovery and connection run on the existing asynchronous worker. Circle or
Square during connection discards its eventual result; the rendering/controller
loop continues while the outstanding bounded request finishes. OS DNS lookup
duration remains platform-dependent. Cancellation never remembers a late
successful result. Square then refreshes the authorized server list.

Use **Options → Settings → Server → Change Server**, choose the PMS, and wait
for verified libraries. The successful choice replaces only this account's
preference. Manual tested endpoints are now account-scoped too; older unscoped
manual settings are retained but not automatically adopted for any account.
Retest/save an old manual endpoint once. Existing sessions need one successful
selection after this update to establish their initial preference.

## Controller diagnostics and movie-list safety

**Settings → Developer Options → Controller diagnostic overlay** toggles the on-screen
strip, default Off. The choice persists; safe throttled input logging and
Triangle's endpoint diagnostics remain available independently.

A concrete asynchronous list safety defect was fixed: completing a shorter
library response now resets focus; opening an item checks its index; restoring
a page clamps focus. Server changes also clear navigation history, details,
artwork and queued progress from the previous server. This addresses the
out-of-range selection path, but confirmation that it resolves the reported
physical movie-library crash still requires device testing. The owner reproduces
CE-108255-1 / signal 6 on build 2458e45 by choosing Movies, then entering the
Movies library, before posters/details/playback. No stack trace is available.
The metadata/artwork/renderer inspection has not proved the abort cause.
Malformed metadata rows and overflowing unsigned numeric values are now rejected;
poster dimensions are rechecked after decode and texture allocation failure
cleanup is safer. Real decoder/cache tests use a fake GPU; they cannot validate
PS5 Vulkan behavior. New `browse:`, `art:` and `gfx: ImGui Vulkan result=` lines
report phases, sizes/counts and numeric errors without titles, tokens or URLs.

## Successful native test build

[Actions run 39659172075](https://github.com/tattedbeardo89/PlexStation/actions/runs/39659172075)
built commit `1d140463824cda2eb9055e3242a471e21a0ce861`. All host regression
suites, real native title compilation, complete-package checks and ZIP mode
validation passed. Download **PlexStation-PPSA85301-1d140463824cda2eb9055e3242a471e21a0ce861**
(37.3 MB) under Artifacts on that run page while signed into GitHub. The separate
`PlexStation-build-diagnostics-1d140463824cda2eb9055e3242a471e21a0ce861` artifact
contains host/native build logs and the validation report. Physical behavior
remains pending testing.

To rebuild manually: repository → Actions → Build PlexStation PS5 → Run workflow
→ branch `feature/plexstation-initial-implementation` → Run workflow. Wait for
success, open the run, and download its installation artifact (named with the
built commit SHA). Failed builds publish diagnostics only.

## Updating from Windows and physical acceptance

Close PlexStation before updating. Extract the outer Actions artifact ZIP,
then the inner installation ZIP.
Deploy the entire PPSA85301 folder as described in
[Windows FTP permissions](CONTROLLER-TESTING.md). **Preserve both `prefs.txt`
and private `plexstation-account.json` on the console**: do not delete the
installation/data directory during an update or copy another device's runtime
files. The package contains neither file. Keep runtime directory writable by
the application; use 755 directories/executables, 644 ordinary resources and
600 private account files, with correct ownership. Windows FTP uploads need
remote POSIX mode application; 777 is not the permanent requirement.

Test on firmware 13.40 / kstuff Lite / ShadowMountPlus 1.7 Beta 5 Fix 1:

1. Sign in/select a PMS; verify libraries, then close/relaunch. The next launch
   should validate the account and open that PMS without the picker.
2. Stop that PMS and relaunch: verify useful error, responsive input, Retry,
   Change Server, cancellation, and no silent alternate selection. Restart PMS
   and Retry; its preference must remain.
3. With two accessible servers, use Settings → Server → Change Server. Confirm
   the new libraries and preference after relaunch, with no old-server Back history.
4. Sign out and link another account. It must not inherit the first account's
   choice. Relink the original account and verify its remembered server returns.
5. Revoke credentials in Plex and relaunch: expect sign-in, not saved-PMS access.
6. Repeatedly open Movies, navigate during loading, open details, return from
   playback and switch libraries. If it still crashes, copy `plexstation.log`
   (and `plexstation.old.log` before a later launch overwrites it) from the
   installation folder using FTP. Share the crash header/address lines plus the
   last `browse:`, `art:` and `gfx:` lines, after reviewing/redacting other log
   content. Do not share credentials, raw responses or playback URLs.
7. Toggle the controller overlay, restart and update the application while
   preserving both runtime files; confirm the overlay and account preferences.

Host fixtures cover identity mismatch, failed endpoint followed by successful
fallback, account isolation, auto-connect, missing/offline PMS, explicit switch,
cancellation, invalid credentials, list focus safety and separate-process
settings persistence. These do not establish physical PS5 behavior.
