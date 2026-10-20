# Validation record

Observed in the Linux cloud environment on 2026-10-20.

| Check | Result | Scope |
| --- | --- | --- |
| C++ core tests (`bash host/build-core.sh`) | 63 assertions passed | Mock Plex data, endpoint fallback and identity mismatch, bad/missing metadata, version/multipart handling, URL encoding, PIN creation, auth failure, malformed JSON, progress throttling, delay limits, atomic credential permissions |
| Real libcurl loopback fixture | 10 scenarios passed | Headers and JSON, 401, malformed JSON, redirects refused, response-size cap, TLS against plain HTTP and self-signed certificates rejected, chunked framing and truncated responses checked |
| PS5 GnuTLS/socket transport executed on Linux | 10 scenarios passed | Same local fixture suite as libcurl; not PS5 runtime/ABI validation |
| New native source with `-Werror -fno-exceptions -fno-rtti` | Passed host syntax validation | New UI, artwork, client and PS5 transport, using Linux headers; not a cross-build or link |
| glslang 15.2.0 build | Passed | Built with GCC, 4 jobs; upstream compiler warnings occurred |
| Shader generation / full host app attempt | Shader generation succeeded; full app blocked | Static VLC plugin libraries are missing; no runnable player binary |
| PS5 title build attempt | Blocked | Missing PS5 release RADV archive, installed SDK/toolchain and static VLC/dependencies |
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
