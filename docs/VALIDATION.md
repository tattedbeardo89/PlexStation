# Validation record

> Historical checkpoint evidence follows. For the current source, defects,
> corrections and owner-confirmed baseline, use [the full implementation audit](FULL-IMPLEMENTATION-AUDIT.md).
> Older statements that music, Watchlist, persistent artwork, streaming or
> native video are absent describe the named old build, not the current tree.


Host observations in the Linux cloud environment on 2026-10-20 are preserved
below. Successful hosted native build evidence from 2026-10-21 follows.

| Check | Result | Scope |
| --- | --- | --- |
| C++ core tests (`bash host/build-core.sh`) | 63 assertions passed | Mock Plex data, endpoint fallback and identity mismatch, bad/missing metadata, version/multipart handling, URL encoding, PIN creation, auth failure, malformed JSON, progress throttling, delay limits, atomic credential permissions |
| Real libcurl loopback fixture | 10 scenarios passed | Headers and JSON, 401, malformed JSON, redirects refused, response-size cap, TLS against plain HTTP and self-signed certificates rejected, chunked framing and truncated responses checked |
| PS5 GnuTLS/socket transport executed on Linux | 10 scenarios passed | Same local fixture suite as libcurl; not PS5 runtime/ABI validation |
| New native source with `-Werror -fno-exceptions -fno-rtti` | Passed host syntax validation | New UI, artwork, client and PS5 transport, using Linux headers; not a cross-build or link |
| glslang 15.2.0 build | Passed | Built with GCC, 4 jobs; upstream compiler warnings occurred |
| Shader generation / full host app attempt | Shader generation succeeded; full app blocked | Static VLC plugin libraries are missing; no runnable player binary |
| Initial local PS5 title build attempt (2026-10-20) | Blocked locally; superseded by hosted success below | Missing local PS5 release RADV archive, installed SDK/toolchain and static VLC/dependencies |
| Official API docs and VLC source URL | HTTP 403 / proxy denied | Network draft additions saved; runtime access has not been established |
| Real Plex login, server browsing and video | Unrun | No live account/server acceptance performed |
| Native UI appearance/controller behavior | Unrun | Syntax passes do not verify focus, rendering or playback |
| PS5 13.40 launch / kstuff Lite / ShadowMountPlus | Awaiting device verification | User's exact target is recorded, compatibility is not claimed |
| Audio/video sync or improvement | Awaiting device verification | No measurement made, no fix claimed |

No application-completion milestone is marked complete. Milestone 1 has source,
independent identity and passing host core builds, but still lacks a successful
full VLC host build. Later milestones have implementation pieces only.

## Device acceptance checklist — all awaiting verification

Use the user's existing kstuff Lite and ShadowMountPlus 1.7 Beta 5 Fix 1 on
firmware 13.40. Record exact build hashes and media properties for each test.

- [ ] Confirm independent PPSA85301 installation without altering PPSA85300.
- [ ] Launch, render correctly on 1080p and 4K, navigate entirely by DualSense.
- [ ] Link a Plex account with another device; expire/cancel a pending link.
- [ ] Relaunch and retain valid sign-in; revoke access and recover; sign out.
- [ ] Discover multiple servers, fail the first endpoint and use a valid one.
- [ ] Verify HTTPS, reject invalid certificates, test explicit local HTTP only.
- [ ] Browse actual movies/shows/seasons/episodes, large-library paging, search.
- [ ] Verify poster loading, missing artwork and bounded texture memory.
- [ ] Play actual H.264/HEVC, MKV/MP4 files, including 1080p/4K where supported.
- [ ] Confirm active hardware/software decoder, rather than inferring from codec.
- [ ] Pause, resume, seek, change available audio/subtitle tracks and versions.
- [ ] Stop, return to details and resume at server-saved progress.
- [ ] Confirm timeline state on the server and manual watched/unwatched changes.
- [ ] Measure audio sync at zero and +/- offsets; repeat after seek/buffering.
- [ ] Test stereo and supported surround paths with the actual output hardware.
- [ ] Disconnect server during playback, observe errors and preserve usability.
- [ ] Check account permissions and logs for credential disclosure.

Transcoding, watchlist and the other incomplete features in README are not
ready for acceptance testing. Do not check them off based on mock responses.

## GitHub Actions build work (2026-10-21)

The existing host results above are preserved; no additional Plex features were
developed. Native build automation is in .github/workflows/build-ps5.yml. The
pinned upstream host support was inspected: Arch/CachyOS is supported, while
its Ubuntu CI demonstrates the converter rather than the full RADV toolchain.
An Arch container on a GitHub Ubuntu runner is therefore used for the full build.

A real local Docker build was attempted. The official pinned amd64 image
resolved and package signature keyring initialization worked. The build then
failed at Arch snapshot database acquisition: the Codex proxy returned HTTP
403 for archive.archlinux.org. The VLC source host was also denied. No TLS or
package signature verification was bypassed. No native executable was produced
by that local attempt. Its diagnostics are in ignored artifacts/logs.

GitHub API attempts to inspect/dispatch workflows returned Forbidden. This is
a limitation of this chat environment, not proof that Actions is disabled in
the repository or that hosted Linux runners cannot build the application.
Git push triggers the new workflow automatically; its eventual run result must
be inspected before claiming a native build. YAML/shell checks and rejection
of missing/placeholder package output do not establish build success.

The Windows PowerShell/WSL2 container entry point is supplied but has not been
run on Windows. PS5 launch, firmware 13.40, kstuff Lite, ShadowMountPlus 1.7
Beta 5 Fix 1 and playback/A-V sync remain awaiting physical device verification.

### Hosted toolchain evidence

Git push access works, and public Actions run pages can be inspected despite
the API restriction. [Hosted run 39632232556](https://github.com/tattedbeardo89/PlexStation/actions/runs/39632232556)
built the pinned Arch image, SDK, native converter and release RADV driver.
The runtime was reproduced twice with the upstream byte-identity checks and
recorded checksum. This establishes that those toolchain stages can execute
on GitHub's Ubuntu runner inside the supported Arch container.

That run failed while applying the existing VLC HDR patch to pinned Mesa.
The patch was adapted to Mesa's per-size buffer sets, preserving HDR and
refresh support. Its dry-run against the pinned source passed.
[Run 39633799498](https://github.com/tattedbeardo89/PlexStation/actions/runs/39633799498)
completed successfully in **28m 14s** at commit
`dcf18ddaf5d98b0e941ee0106977cf9b1e833b03`.

### Native application build — passed, 2026-10-21

The job reported success for the actual PS5 compilation and validation step,
complete installation ZIP upload, and diagnostics upload. This includes the
static media dependencies and libVLC, PlexStation native compilation/linking,
SELF conversion, independent PPSA85301 package validation, ELF/SELF format and
size checks, linked video-output plugin and runtime-import checks.

GitHub lists `PlexStation-PPSA85301-dcf18ddaf5d98b0e941ee0106977cf9b1e833b03`
(37.2 MB) and the matching diagnostics artifact (224 KB). The outer installation
artifact SHA-256 is `139826a6202163f2c7661849e62cb1c83c0c806b9b95795cbeb7bb98652ad5f5`.
This evidence comes from the successful job/step states and artifact listing;
artifact downloads require GitHub sign-in and were not independently inspected
in this cloud environment. The included validation.json and logs record the
build details.

The application source and existing tests were preserved. Subsequent CI changes
only refine dependency checkpoint caching. Native build success does not close
any live Plex, display/controller, firmware, decoder or A/V-sync acceptance item.

### Cached native rebuild — passed

[Run 39634054358](https://github.com/tattedbeardo89/PlexStation/actions/runs/39634054358)
at `f4a9666fa071572e808c6d5ab1ed43d29362e05d` also passed every native
compilation/validation and artifact upload step. The actual job took 2m 44s
after its queue wait, exercising the exact-fingerprint dependency checkpoint
restore and per-run save. GitHub lists the complete installation artifact
(37.2 MB) and diagnostics. The outer installation artifact digest is
`b5a4586d63658659d29883e24f1ecc04cc72eb6056370b9bcc488f89701e27d9`.
No application or test source changed between the two successful runs.

## DualSense and launch follow-up

The owner confirmed physical launch and UI rendering of build f4a9666
(run 39634054358) on PS5 13.40 / kstuff Lite / ShadowMountPlus 1.7 Beta 5 Fix 1.
Initial CE-107750-0 was resolved by setting installation permissions to 777.
Controller navigation failed. This supersedes the earlier untested launch
status, but establishes neither input nor media playback acceptance.

The controller follow-up passed the existing 63 core checks, 10 libcurl
transport scenarios, 10 Linux GnuTLS/socket scenarios and strict host syntax
checks. New host harnesses exercise actual UI/input code with real ImGui and
fake platform/network/player boundaries, the PS5 polling backend with mocked
service calls, and exact workflow ZIP permission preservation/rejection.
Device navigation and corrected 755/644 deployment remain pending hardware
verification. See [investigation and test instructions](CONTROLLER-TESTING.md).

### Controller native build artifacts — passed

- [Run 39644508127](https://github.com/tattedbeardo89/PlexStation/actions/runs/39644508127),
  commit 7019ec298fb2df8d744962315144b14da0c500a4: succeeded in 2m 37s.
- Final [run 39644828464](https://github.com/tattedbeardo89/PlexStation/actions/runs/39644828464),
  commit 26c5116044c2936364ce76cf94ab6054eb83e87f: succeeded; the reported
  total duration, including any queue wait, was 3m 27s.

Both runs passed actual native compilation, packaging, the new ZIP executable
permission check and installation/diagnostics uploads. The final complete
installation artifact is
`PlexStation-PPSA85301-26c5116044c2936364ce76cf94ab6054eb83e87f` (37.2 MB).
Outer artifact SHA-256:
`6ee9231ff775ed90fed4c02e6aa25e21b421d882e7294e7ef0eaaede5a91de58`.
Public job states and artifact listings were inspected; artifact downloads
require GitHub sign-in and were not independently performed here.
No claim of fixed device navigation or 755/644 launch acceptance is made.

## TV authentication follow-up

The owner confirms Cross starts authentication on the physical PS5. The old
flow returned a long code and an unusable manually entered URL, and waited
for authorization. This confirms a request reached Plex from the PS5, not
completed account authorization or full controller/playback acceptance.

The TV flow now requests strong=false and uses plex.tv/link instructions with
the exact returned short code, plus a locally generated active-session QR
using app.plex.tv/auth. Host auth fixtures, actual UI completion/persistence
transitions, existing controller/PS5-reader tests, 63 core assertions and both
10-scenario transport suites passed, along with strict native-source host
syntax validation. The generated fixture QR was independently decoded with
zxing-cpp and matched the complete expected URL.

Direct official documentation/auth-host access returned HTTP 403; a live
unauthenticated PIN probe failed at the environment network layer (URLError).
Accessible official Plex Kodi source and community API references were
inspected; details and limitations are in AUTHENTICATION.md. No live account
authorization was performed. PS5 linking, QR scanning, restart restoration,
server browsing and playback remain pending owner acceptance.

### Authentication native artifact — passed

[Actions run 39648185013](https://github.com/tattedbeardo89/PlexStation/actions/runs/39648185013)
completed successfully in 2m 41s at
`cdfe73356568c5a91c90a83e17d6d1c1b3a252f5` (job 2m 36s). Native
compilation, complete installation validation, ZIP permission verification,
cache save and both artifact uploads passed. The complete 37.2 MB installation
artifact is `PlexStation-PPSA85301-cdfe73356568c5a91c90a83e17d6d1c1b3a252f5`;
matching build diagnostics are 181 KB. Outer artifact SHA-256:
`f48ba0085f4d66c42b4041fff789c87a3092fe229e1c9cb0268a0d074a360697`.
The public job states/artifact listing were inspected; no authenticated
artifact download or physical account authorization was performed here.

The first regression pass caught a changed generic error string expected by
the existing transport probes. The generic string was preserved while keeping
safe DNS/CA/TLS explanations. Both full transport suites passed on rerun.
No test result from that initial failed pass is presented as passing.

## PMS connectivity follow-up

The owner confirmed successful physical account linking and server discovery
on authentication build cdfe733. Selecting the PMS failed with the generic
network/TLS message, before libraries loaded. Linking and discovery are now
confirmed; restart persistence, PMS connection, browsing and playback remain
separate acceptance items.

The targeted changes introduce typed native failure diagnostics, nonblocking
retry/deadline handling, framed-response completion, XML/JSON identity parsing,
identity plus authenticated-sections validation with candidate fallback, a
controller diagnostics panel, and tested-before-saving manual overrides.
See SERVER-CONNECTIVITY.md for the audit, test methods and physical checklist.

### Connection validation and native artifact — passed

All existing suites and new connection suites passed: 64 core checks (the
extra counted header assertion comes from authenticated sections probing),
both 10-scenario transport suites including self-signed certificate rejection,
PIN/auth lifecycle tests, UI auth/controller/manual connection assertions,
PS5 pad backend fixtures, preference persistence across processes, package
permission tests, strict native-source host syntax and independent QR decode.
New production GnuTLS fixtures verified a test-CA plex.direct-shaped hostname
and SNI, wrong-host and missing-CA rejection, handshake failure, deadline
handling, fragmented keep-alive bodies, IPv6 loopback, and typed DNS/refusal
failures. Deterministic DNS/TCP-timeout/readiness and short/EINTR/EAGAIN I/O
are injected only by the host test linker. They are not production overrides
or proof of access to real Plex-issued certificates, private PMS or relay.

[Actions run 39652707008](https://github.com/tattedbeardo89/PlexStation/actions/runs/39652707008)
succeeded in 2m 53s at `2458e45962407f70428ead9a71360008bab8c919`.
Actual native compilation, complete-package checks, ZIP permissions and
installation/diagnostic uploads passed. The complete artifact is
`PlexStation-PPSA85301-2458e45962407f70428ead9a71360008bab8c919` (37.3 MB).
Outer installation artifact digest:
`115442178758951e0dd3c2383fd815f24db0e550a8f0c7a19f7a750d58f890aa`.
This result was confirmed from public steps/artifact metadata, without an
authenticated artifact download. Real PS5 authenticated PMS connection,
movie/TV library loading, browsing, remote/relay access and actual Plex
certificate acceptance remain pending owner verification.

## Remembered server, controller overlay and movie-library safety — 2026-10-21

Validated Plex account IDs now scope the preferred machineIdentifier and manual
endpoint settings. Both new and restored sign-ins validate the account before
resource discovery; asynchronous automatic selection uses the existing identity
and authenticated-library endpoint fallback. Failed/missing servers retain their
preferences. Explicit switching bypasses auto-selection; cancellation discards
late connection results. Settings adds Server → Change Server and Diagnostics →
Controller diagnostic overlay (persistent, default Off).

The owner reports CE-108255-1 / SIGABRT entering the Movies library on 2458e45,
before selecting a movie or playback. There is no stack trace. A concrete
out-of-range focus path is fixed and regression-tested; metadata parsing rejects
non-object rows and out-of-range unsigned numbers. Decoded image bounds are
rechecked, texture allocation failure cleanup improved, and redacted browse,
artwork and Vulkan result logs identify the failing phase. These changes are
not proof of the cause or resolution of the physical abort.

See SERVER-RECONNECTION.md for update preservation and device verification.
Host validation passed: 67 core checks, 10 libcurl and 10 native GnuTLS
transport scenarios, authentication, production UI controller/reconnection
transitions, PS5 pad backend fixtures, separate-process preference persistence,
secure connection/identity/fallback fixtures, real artwork decode/cache tests,
package permission regressions and strict native-source host syntax. Actions
now reruns the host suites and retains their logs before the real cross-build.
[Actions run 39659172075](https://github.com/tattedbeardo89/PlexStation/actions/runs/39659172075)
succeeded at `1d140463824cda2eb9055e3242a471e21a0ce861` (5m 42s total).
All host regression suites, actual native PS5 compilation, complete installation
validation, ZIP permission checks and both artifact uploads passed. The host
GnuTLS fixture's old hardcoded checkout path was corrected; no production trust
checks were relaxed. Local real artwork decoder/cache fixtures also passed
AddressSanitizer and UndefinedBehaviorSanitizer with leak detection enabled.

Installation artifact:
`PlexStation-PPSA85301-1d140463824cda2eb9055e3242a471e21a0ce861`
(37.3 MB); outer artifact SHA256:
`eae75c50cc4856271ad91b3a75fed5ed020d7fd647e9f004e7690e6dc187208c`.
Logs/report artifact:
`PlexStation-build-diagnostics-1d140463824cda2eb9055e3242a471e21a0ce861`
(183 KB). These results were verified from public Actions step and artifact
metadata; an authenticated artifact download was not available. Account-scoped
startup/retry/switching, overlay persistence and resolution of the owner's
movie-library abort remain pending physical PS5 acceptance.

## Checkpoint A — native stability investigation

New owner evidence confirms authentication, PMS connection and retrieval of
library directories; entering a Movies library aborts before posters/playback.
The photographed SIGABRT and ShadowMountPlus target SIGSYS(12), PID242 at
15:03:20, are separate evidence tracks. Build/timing correlation is pending.
Neither the unconditional legacy `eboot+` subtraction nor the supplied high
RIP/backtrace can reliably identify an application symbol. See
CRASH-DIAGNOSTICS.md for the audit, uncertainties, evidence needed and matching
ELF symbolization procedure. No unsupported syscall or physical root cause
has been confirmed.

Implemented numbered bounded/flushable CP01–CP15 checkpoints, text-only and
independent artwork download/decode/upload switches, ten-item requests,
detailed tracing, hidden-by-default upgraded overlay and build information.
Added stale-screen response invalidation and atomic accepted page replacement,
bounded metadata extraction and nesting, invalid identifier filtering, GPU
resource failure cleanup and initialized descriptor-allocation failure returns.
Playback engine and pinned dependencies remain unchanged.

Host core (72 checks), auth, controller/pad, real artwork, both ten-scenario
transport suites, connection policy/fallback, settings and permission tests
passed. Core/artwork/UI ran with AddressSanitizer/UndefinedBehaviorSanitizer
and leak detection. Modified graphics source also passed host syntax checking.
Symbolization helper rejects out-of-module addresses and wrong ELF checksums.
Native workflow now retains matching unstripped application/debug information,
converted ELF, linker map, symbols, build IDs and executable hashes separately
from the signed production installation. Dependency cache fingerprint is
unchanged. Native Checkpoint A result will be recorded when it completes.

## Checkpoint B — Plex Home authorization and isolation

Implemented actual profile listing (v2 JSON, 404/405-only legacy fallback,
JSON/XML parsing), protected profile switching via POST form PIN, independent
returned-token identity validation, masked controller keypad, actual HTTPS Plex
avatars with neutral fallback, Switch User, remembered profile/startup option,
and primary-account/profile-scoped PMS/audio preferences. No profile tokens or
PINs enter plaintext preferences; only the primary linked account is persisted
in its existing private credential file. Protected remembered profiles prompt
for a PIN even with the picker disabled.

Production client fixtures passed for listing/switching, incorrect PIN,
unauthorized/mismatched sessions, valid JSON/XML, fallback and stable scopes.
Production UI/controller tests passed for PIN entry/retry, profile identity
validation, child-only libraries, resource/timeline token attribution, stale
old-user request rejection, no admin fallback and settings with no token values.
Actual decoder/cache avatar/isolation tests and strict native-source host
syntax passed. Accessible official Kodi and maintained PlexAPI references and
live-verification limits are recorded in PLEX-HOME.md. Live Plex Home/PMS and
physical PS5 behavior remain unverified.

### Independently deployable Checkpoint A — native passed

[Run 39697533557](https://github.com/tattedbeardo89/PlexStation/actions/runs/39697533557)
produced the complete Checkpoint A build at
`8e6ae8e3e756721f4dcd66279503712b68b2950d`. Native compilation, package and
permission validation, matching debug collection and uploads passed. The first
attempt compiled successfully but did not upload an install: debug collection
incorrectly required a nonempty success-only linker log; the guard now allows
that log to be empty while requiring all ELF/map/symbol files.

- Installation: `PlexStation-PPSA85301-8e6ae8e3e756721f4dcd66279503712b68b2950d` (37.4 MB).
- Debug: `PlexStation-native-debug-8e6ae8e3e756721f4dcd66279503712b68b2950d` (86 MB).
- Logs: `PlexStation-build-diagnostics-8e6ae8e3e756721f4dcd66279503712b68b2950d` (184 KB).

Artifact listings and job states were verified publicly; authenticated downloads
were unavailable. This is the preserved stability-only installation, independent
of the later profiles/Home interface. Physical SIGABRT/SIGSYS resolution and
module symbolization remain unconfirmed.

## Checkpoint C — personalized Home and native TV UI

Implemented actual scoped Continue Watching and nonempty server hubs, bounded
supplemental recently-added movie/TV, release-date sorted movie and On Deck
requests. Native carousels show real progress/series/episode/remaining time,
scroll and restore focus after details. Added original sidebar vector icons,
active profile/avatar entry, movie sort/unwatched/all controls, existing search
and pagination, show details/seasons/episodes, real metadata/poster/backdrop/
genre/cast presentation and a scrollable description. Unsupported cloud
Watchlist/music/audiobook navigation is hidden; limitations and fallback library
scope are explicit in TV-CLIENT-STATUS.md. No screenshot was attached, so exact
visual matching is not claimed.

Metadata list extraction avoids copying playback Media trees. Detailed Media is
bounded to eight versions and two part descriptors (preserving multipart
rejection), with bounded labels/history and existing request/image/cache caps.
PIN privacy now suppresses UI focus/action, main-loop and PS5 raw-pad logs, while
leaving actual input delivery intact. A controller test race from outstanding
old timeline work was exposed by the intermediate Checkpoint B CI gate; fixture
jobs are now explicitly settled before manual completions rather than relying
on worker timing. That failed run did not publish an installation.

Host core passed 74 checks plus ten real libcurl scenarios. New Home fixtures
verify real Continue Watching progress/identity, empty/unsupported responses,
release-date requests, profile-scoped authorization and cancellation. Production
UI fixtures verify carousel focus/return, PIN retry/privacy, child libraries and
timeline/media URL tokens, full TV navigation and stale-user results. Existing
pad, artwork, auth, real native TLS, connection/fallback, permission and strict
syntax tests passed. Final ASan/UBSan and native pipeline results follow when
complete; fixtures do not establish live Plex permissions or PS5 acceptance.

### Final validation hardening

The combined title compiled successfully, but the next debug/report step exposed
a separate workflow ownership error: container-created validation.json was
root-owned and the host collector opened it for writing. It now publishes an
atomic replacement in the runner-owned directory. An isolated collector test
checks read-only report replacement, required files, empty successful link logs
and binary hashes; failures also receive a dedicated annotation and failed report.
The installation remains gated on all steps succeeding.

The pinned SDK source at b83202be confirms normal-context `ps5_backtrace` uses
libunwind, and pinned PS5_Vulkan f410e67a maps backtrace to that implementation.
C++ termination now captures up to 24 raw callers before SIGABRT, outside signal
context. The host unwinding test passed; PS5/unmatched-module unwinding is not
claimed. SIGSYS/SIGABRT still require physical reproduction/build correlation.

### Texture lifetime and upload failure review

A newly reachable same-frame lifetime defect was confirmed in code: an avatar
can be queued before a profile switch/developer toggle clears the artwork cache.
Its descriptors now retire until the next pre-draw update, and UI workers/images
are drained before renderer teardown. The real artwork-cache test verifies clear
cannot immediately free current-frame handles; controller tests cover shutdown.
This is not evidence that artwork caused the older 2458e45 SIGABRT.

One-shot upload command handles/results are now checked. Failed/unconfirmed
uploads do not publish a texture; unknown queue completion disables further
custom uploads and retains a bounded batch until teardown. Video plane setup
checks the same upload availability/idle results without changing decoding or
audio algorithms. GPU device-loss recovery is explicitly not claimed. Modified
graphics source passed host syntax; actual GPU error paths require device tests.


## Checkpoint D — successful combined native handoff

[Actions run 39702538057](https://github.com/tattedbeardo89/PlexStation/actions/runs/39702538057)
completed successfully in approximately ten minutes for built commit
`d52e5cd39ad6689f1f917179a6d1761132adde03`, on branch
`feature/plexstation-initial-implementation`.

All host regression suites passed, including real curl/native GnuTLS transport,
authentication, Plex Home/feed, artwork/cache, controller/UI, endpoint fallback,
crash/symbolization/collector and strict source checks. ASan/UBSan (with leak
detection) passed the five core/Home/feed/artwork/controller fixtures. These
include the final deferred-texture-clear and UI-shutdown changes. The pinned
Arch container on the GitHub Ubuntu runner compiled the actual PS5 title, then
complete-package validation, matching debug collection and ZIP Unix-mode checks
passed. This is a native build result, not only host syntax verification.

Published artifacts reported by the successful run:

| Artifact | Size | Actions outer artifact SHA256 |
| --- | --- | --- |
| `PlexStation-PPSA85301-d52e5cd39ad6689f1f917179a6d1761132adde03` | 37.4 MB | `17fa2d6e87213cc5518aab1bd8b441fc4236e9633791e39fa00a2d3f3d71e126` |
| `PlexStation-native-debug-d52e5cd39ad6689f1f917179a6d1761132adde03` | 86.4 MB | `917d406088107e08267979330e64c693e5d53587709feef032e54d254d5d2563` |
| `PlexStation-build-diagnostics-d52e5cd39ad6689f1f917179a6d1761132adde03` | 187 KB | `9d44b160440236c5911bb593d5ae11ea824afe16762156b58a21d759a0421239` |

Artifact names, sizes/digests and workflow conclusion were checked using public
GitHub metadata. Authenticated artifact downloads were unavailable in this
session, so no separate downloaded-binary inspection is claimed. The workflow
itself validates the built installation and matching executable hashes before
upload. Outer artifact hashes above are not the installed eboot.bin hash; use
SHA256SUMS/build.json inside the matching artifacts for executable correlation.

The separate successful stability-only Checkpoint A remains run 39697533557.
Intermediate failed/cancelled runs are not installation recommendations. GitHub
reported a Node.js 20 action deprecation warning (actions ran under Node.js 24);
it did not fail this build. Pinned action updates are outside this stability pass.

Physical testing remains required: original Movies SIGABRT is not root-caused,
SIGSYS remains unmatched to a build/launch, and new Plex Home PIN/permissions,
profile-specific progress, TV navigation and native playback need owner
acceptance. No unsupported-syscall diagnosis or firmware compatibility claim is
inferred from this successful compile. The feature/limitation matrix and exact
Windows deployment/test sequence are in TV-CLIENT-STATUS.md. Documentation-only
handoff commits do not alter the built source SHA above.

## Media expansion release-candidate host regression

The physical-testing baseline remains d52e5cd, run 39702538057, with the matching
source tag baseline/ps5-physical-test-d52e5cd. No development artifact replaces
that installation. MEDIA-EXPANSION.md records the initial audit, implemented
feature matrix, protocol references, codec evidence and pending physical tests.

Checkpoint A at b3cc832 passed the real native pipeline in run 39705677578;
installation, matching debug and diagnostics artifacts are named with full SHA
b3cc83265d63c84590e8030b268eeb904daa037a. This proves compilation/package gates,
not audible music, codec support, server attribution or physical stability.
Checkpoint B run 39793484467 failed before native compilation in the sanitizer
controller fixture: a new Play command raced an outstanding timeline job. The
fixture now explicitly drains that work; failure was not bypassed or labelled
as a native success. Checkpoint C and combined D runs were queued at this record;
results must be recorded when they actually complete.

Local release-candidate source 1721e591486f4b6c2ab4a737f6713f3384fbb568 passed:
core (74 checks plus 10 real curl HTTP/TLS scenarios), authentication, Plex Home/
feed, audio/listening persistence, streaming/episodes/outbox, grouped discovery/
cloud API fixtures, artwork/cache, production controller UI and pad backend,
connections/preferences, 10 real native GnuTLS transport scenarios, crash/debug/
symbolization, strict source and ZIP permission regressions. All nine ASan/UBSan
suites with leak detection passed: controller, core, Home, Home feed, audio,
listening, streaming, discovery and artwork. Shared instrumented sources are
compiled once; no sanitizer was disabled. The baseline-archive identity fixture
also passed and rejects substituted ELF data.

No live Plex account/provider write or PS5 playback/device test was run. HLS
segment authorization/session cleanup, actual audio formats/rates/chapters/
pitch, external subtitles, autoplay, timeline attribution, cloud access and the
original Movies SIGABRT/unmatched SIGSYS remain acceptance requirements. Host
fixtures and YAML checks are not native cross-compilation or device acceptance.

## f511344 post-server SIGILL checkpoint A

Owner confirms launch, restored authentication, actual Home username/profile
selection and PMS listing on f511344; selecting PMS then fails. Matching binary
inspection run 39844239411 verified original package/debug hashes. Conditional
0x400000 load bias maps the repeated raw RIP 0x004f93d7 to crash_handler's UD2
after its exit call; zero bias maps inside an instruction, not a reliable call
site. Console hash/load marker and original crash header are pending. Neither
the first fault nor a physical fix is claimed. SIGILL, earlier Movies SIGABRT
and unmatched SIGSYS remain separate until correlated with original headers.

Host core (74 checks + ten curl scenarios), real native GnuTLS transport,
production controller/UI safe-startup, returning-exit bridge, debug collection/
symbolization and strict source checks passed for the checkpoint A changes.
Native/ASan workflow gates will be recorded only after actual completion.
See SIGILL-AVATARS-THEME.md for exact hashes, instruction evidence and FTP steps.

### Numeric parser first-fault follow-up (2026-10-22)

Owner logs confirm f511344 SIGABRT in JSON scan_number:8661. A host fault
injection using the original header and an empty locale decimal separator
reproduced that exact assertion (process SIGABRT). The patched production header
passes normal and empty-separator numeric tests with assertions enabled and
native no-exception/no-RTTI flags. Strict native source, Home and real native
transport checks passed locally. A new native startup fixture checks numeric
parsing before authenticated requests. Physical PS5 acceptance remains pending;
see SIGILL-AVATARS-THEME.md for evidence, secondary signal distinction and tests.

Avatar follow-up adds production-code tests for relative URLs, cross-origin
redirect credential withholding, blocked redirect hosts, delayed negative-cache
retry, missing/invalid/auth-denied/timeout images, PNG/JPEG decoding, 1024-square
input downsampling, generation invalidation and render-thread texture cleanup.
The native and host HTTP transports expose Location but keep global redirect
following disabled; duplicate Location rejection has framing regression coverage.

Local follow-up checks passed: core and curl transport, real native GnuTLS
transport, authentication, Home, audio/listening, streaming, discovery,
controller/package permissions, artwork, connection framing, crash tools and
strict native source syntax. The complete ASan/UBSan runner passed all existing
suites plus both numeric locale variants. These are host results; native build
and physical acceptance are reported separately.

### Native diagnostic checkpoint and theme follow-up (2026-10-22)

Native run 39845185729 (db9f21d) succeeded: host regressions, actual PS5 compile,
complete package, matching debug collection and ZIP permission gates passed.
This is an instrumentation-only checkpoint and predates the numeric correction;
it must not be represented as the fixed parser binary. The intermediate queued
c476647 run was superseded by GitHub concurrency when the newer bfb5eee commit
was pushed; this was cancellation, not a compiler result. Run 39846419456 builds
bfb5eee with the numeric fix and avatar changes independently of the theme.

The native theme uses shared palette/type/spacing/radius/motion tokens and the
existing ImGui/Vulkan rendering path. Controller fixtures exercise settings,
profiles, long-title details and Home at 1080p and 4K, checking valid focus,
finite geometry and bounded clipping, plus persisted reduced motion and safe
startup restoration. Source policy checks prevent individual linked Plex UI
files from adding hardcoded colors. These checks do not prove PS5 visual quality
or device stability. A final native build and physical acceptance remain separate.

Final local follow-up: all nine existing ASan/UBSan suites plus normal and
empty-separator numeric suites passed. Normal core/curl/native-GnuTLS transport,
artwork, controller/package/theme-boundary and strict native-source checks
passed. Music/listening, authentication, Home, streaming and discovery suites
also passed with the shared theme changes present. The targeted controller
fixture covers real UI draw lists at 1080p/4K, rapid navigation, media return,
async cancellation, profile authorization and startup preference restoration.
No physical playback, avatars, Home crash resolution or firmware acceptance is
inferred from these host checks.

Themed checkpoint 761d562 native run 39848648128 succeeded, including complete
installation/debug/log artifacts. Final layout follow-up preserves real decoded
image aspect ratios, center-crops banners, and centralizes custom list spacing.
Its core/curl/native transport, artwork, controller/package/theme-boundary,
strict native-source and all ASan/UBSan/numeric-locale checks completed with
passing log summaries. The final native build is tracked separately below.

## October 23, 2026 remaining-features candidate

Owner acceptance of `5d8adb9` covers launch/authentication/server/native video,
default subtitles Off and normal numeric parsing only. The new source candidate
adds subtitle lifecycle controls, avatar/cache diagnostics, authorized cached
startup, library routing/tabs, cloud filters/sidebar customization and branding.
See [the detailed status and physical checklist](REMAINING-FEATURES-2026-10-23.md).
No new physical acceptance is claimed. The local native attempt is blocked by
its missing RADV release archive; hosted build results are tracked separately.
