# Validation record

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
Native build result will be recorded after Actions completes.
