# DualSense investigation and device test

## Download this test build

[Actions run 39644828464](https://github.com/tattedbeardo89/PlexStation/actions/runs/39644828464)
passed native compilation, package checks, ZIP mode verification and both
artifact uploads. Built commit: `26c5116044c2936364ce76cf94ab6054eb83e87f`.
Download **PlexStation-PPSA85301-26c5116044c2936364ce76cf94ab6054eb83e87f**
(37.2 MB) from Artifacts while signed into GitHub. Extract the outer artifact
ZIP and its inner `PlexStation-PPSA85301.zip`, yielding the complete PPSA85301
folder. The matching `PlexStation-build-diagnostics-26c5116044c2936364ce76cf94ab6054eb83e87f`
artifact contains build logs and validation.json.

GitHub's outer installation artifact SHA-256 is
`6ee9231ff775ed90fed4c02e6aa25e21b421d882e7294e7ef0eaaede5a91de58`.
Use the included SHA256SUMS for the inner installation ZIP. The job/artifact
listing was checked publicly; downloading artifacts requires account sign-in
and was not independently performed in this cloud environment.
**Physical controller navigation and corrected permissions remain untested.**

## Evidence and code path

The owner reports that build f4a9666, Actions run 39634054358, launches and
renders on PS5 13.40 / kstuff Lite / ShadowMountPlus 1.7 Beta 5 Fix 1 after
changing installation permissions to 777. Controller interaction failed.
This is confirmed device feedback, not a successful controller acceptance test.
No controller log from that run was supplied, so the exact device-side failure
(initialization, ownership, samples or UI) cannot yet be identified.

The actual production path is `src/platform_ps5.c` -> `plat_pad_read()` ->
`src/main.cc` once per render frame -> `ui_frame(pad, dt)` in `src/plex_ui.cc`.
Original `src/ui.cc` remains in source but is not linked into PlexStation.
Upstream VLC reads up to 64 120-byte samples via scePadRead, chooses the newest
timestamp, filters disconnected/intercepted input, normalizes stick axes with
a radial dead zone and uses native button masks. Its UI implements its own
focus/selection and held-button repeats; it draws through ImGui draw lists.
PlexStation inherited that platform reader and same masks, but used a separate
repeat implementation and incomplete Back/tab/focus handling.

Neither original UI used ImGui widget navigation or AddKeyEvent. These are
custom drawn rows, not ImGui Button widgets, so enabling NavEnableGamepad alone
cannot make them navigable. The main loop now also forwards digital and analog
ImGui gamepad events before NewFrame, sets HasGamepad from connection state,
and enables NavEnableGamepad. Custom UI focus remains authoritative.
`ImGui:draw-list focus` / `imgui_nav=0` is expected for these screens.

## Changes

- Keep the upstream standard port 0, sample ABI and button mapping. ABI size,
  connection and timestamp offsets have compile-time assertions.
- Retry initialization/opening after failures; prefer the foreground user,
  then initial and logged-in users. Reuse scePadGetHandle when the launcher
  already opened a controller in this process. No new controller library.
- Reopen after read errors or sustained disconnection; clear stale state.
  Respect shell/system-dialog interception; never force controller ownership.
- Use independent directional repeat timers (380 ms delay, 90 ms repeat),
  a 0.60 normalized menu threshold and platform radial dead zone (0.18).
  Cross/Circle/Options do not repeat when held.
- Highlight sign-in initially. L1/R1 activate adjacent existing sections;
  Options opens Settings; Circle backs out of keyboard, settings, details and
  browsing. In-flight device-link completion is discarded after cancellation.
- During playback Cross pauses/resumes and left/right seeks 10 seconds.
  Options opens tracks, where Cross selects/cycles a track and Circle closes
  that menu first. Circle outside the menu stops and returns focus to Resume.
  The stop press cannot also dismiss details. L1/R1 retain audio delay control
  during playback. D-pad Up moves one row upward in details. Existing title
  sorting moves from R1 to Square in library lists.
- Network jobs still run with std::launch::async and zero-time readiness
  checks. Host tests exercise navigation while a response remains unfinished;
  there is no network wait added to the input/render path.

## Temporary diagnostics

A narrow strip at the top displays connection, raw native buttons, generated
UI actions, screen and custom focus index. It remains visible during playback.
Examples: Cross = `00004000`, Circle = `00002000`, Down = `00000040`.
Stick movement can generate an action with raw buttons zero.

`plexstation.log` records only controller/permission diagnostics here:

- `pad:init`: User Service/Pad initialization return values and user lookup
  results. An already-initialized service can return a nonzero result; inspect
  open/read outcomes rather than treating every nonzero init as fatal.
- `pad:open`: candidate user, existing handle and open result.
- `pad:poll`: sample count, connected/intercepted/stale state, raw buttons,
  delivered sample and button-change counts. Healthy idle has connected=1,
  raw=0 and increasing polling/delivery counts.
- `input:main`: frame count and polling result, state forwarded to UI.
- `input:ui`: received buttons accumulated since last log, generated action,
  screen, selected index, valid focus, busy state and ImGui flags.
- `install`: kernel effective uid/gid, resolved app directory ownership/mode
  and eboot mode. Mesa wraps the ordinary effective-ID functions, so these
  diagnostics explicitly use the kernel IDs. Newly created caches use 755.

Periodic lines are emitted every five seconds, plus limited UI event lines.
No Plex credentials, server names or playback addresses are added by this
logging. Existing VLC logs may contain other data: share **only these diagnostic
prefixes** and remove any unrelated private information.

Interpretation: failed open means user/handle acquisition; samples with
intercepted=1 mean the shell owns input; increasing healthy samples with raw=0
mean no buttons observed. Nonzero pad raw but zero main/UI buttons identifies
a forwarding/filtering boundary. UI actions with unchanged invalid focus
identify the UI context. Zero ImGui NavActive alone is not a failure here.

## Host validation

`bash host/test-controller.sh` runs production `plex_ui.cc` with real ImGui
and fake player/network/platform boundaries; no GPU/PS5 is simulated as real.
It covers D-pad/stick selection, Cross activation, Back, initial selection,
hold repeat, dead zones, tabs, track-modal isolation, pause/seek, return from
playback and input while a network future is pending. A separate harness
executes the production PS5 reader against fake service calls, including
open retry, foreground/handle reuse, sample ordering, idle, stale/intercepted
input, read failure and reconnection. Python tests check ZIP modes and reject
unsafe archives. Existing core and both transport suites are also retained.

## Physical PS5 acceptance — pending

1. Install the new complete PPSA85301 folder using the permissions procedure
   below. Keep the existing kstuff Lite and ShadowMountPlus installation.
2. At sign-in, confirm the highlighted row and `Pad:OK`. Press Cross and
   confirm it starts linking. Circle must cancel; retry with Cross.
3. After linking, test every existing sidebar section using D-pad, stick,
   Cross and L1/R1. Open Settings with Options, move through all six rows,
   return with Circle. Test search keyboard movement, Cross typing and cancel.
4. Hold Down/Up; check controlled repeat. Release and check movement stops.
   Leave the stick centered and verify no drift. Press another button while
   holding a direction and confirm repeat continues.
5. Enter a library, item details, Back, then start an available playable item.
   Cross pauses/resumes, Left/Right seeks, Options opens tracks, Cross changes
   the selected track, Circle closes tracks and then stops playback. Verify
   focus returns to Resume/Play and the next directional/select press works.
6. Open the PS home/system menu and return; disconnect/reconnect DualSense.
   Input must resume without stale held buttons or commandeering system input.
7. If input still fails, wait 10 seconds, press Cross/Down/Options, wait another
   10 seconds, then retrieve the diagnostic-prefixed log lines. Report what
   the strip showed and whether controls worked before/after the system menu.

Do not mark this checklist passed from host tests or native compilation.

## Windows 11 FTP deployment and permissions

The inner installation ZIP carries Unix modes: **755** for all directories,
`eboot.bin` and `sce_module/libc.prx`; **644** for shipped ordinary files.
The native build normalizes them and Actions rejects incorrect ZIP metadata.
The prior CE-107750-0 plus 777 workaround is consistent with missing execute
or directory traversal permission, but the pre-fix modes/ownership were not
recorded. That error code alone does not prove a specific permission cause.

Windows Explorer extraction does not apply POSIX permissions to NTFS, and FTP
upload generally creates new server-side modes instead of using ZIP metadata.
Therefore verify permissions **on the PS5**, even with the corrected artifact.

1. Close PlexStation. Download the installation artifact, extract the outer
   artifact ZIP and then the inner PlexStation-PPSA85301.zip. Keep its entire
   PPSA85301 directory; do not copy individual executables alone.
2. Connect FileZilla to your existing PS5 FTP service. Use **binary** transfer
   mode (Transfer > Transfer type > Binary), and upload the complete folder
   to `/data/homebrew/PPSA85301`. Do not overwrite VLC's PPSA85300.
3. In the remote pane, right-click PPSA85301 > File permissions, set **755**,
   recurse and select **Apply to directories only**. Verify the parent
   `/data/homebrew` is also traversable; do not broadly change other titles.
4. For a **fresh installation only**, set shipped files to **644**, applying
   to files only recursively. Then explicitly set `eboot.bin` and
   `sce_module/libc.prx` to **755**. Verify both with the remote permission
   listing. A server must support FTP SITE CHMOD; check FileZilla's command
   response for success. Refresh the remote listing after changing modes.
5. For an **in-place update**, apply these file modes only to uploaded static
   files. Preserve runtime account data: `plexstation-account.json` must stay
   **600**, with the app's owner. Never apply recursive 644/755/777 to account
   data or distribute it. Keep your existing account/prefs locally on PS5;
   the artifact contains no account file.
6. Ownership matters for runtime writes: a 755 directory is writable by its
   owner. The app's account, preferences, log and cache location must be owned
   by the effective application user (the `install` log records uid/gid).
   Use your FTP service's supported ownership controls or existing console
   maintenance method if needed. FTP chmod cannot change ownership by itself;
   do not substitute world-writable 777. No jailbreak replacement is required.
7. Relaunch through ShadowMountPlus. If the FTP server cannot set permissions,
   use an existing PS5-side file manager/extractor that preserves Unix modes,
   and verify the same modes/ownership before launch.

The folder modes and launch result must still be verified on your hardware.
