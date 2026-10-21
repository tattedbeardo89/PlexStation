# Plex Media Server connection diagnostics

## Download this connection test build

[Actions run 39652707008](https://github.com/tattedbeardo89/PlexStation/actions/runs/39652707008)
succeeded in **2m 53s**, built commit
`2458e45962407f70428ead9a71360008bab8c919`.
Download **PlexStation-PPSA85301-2458e45962407f70428ead9a71360008bab8c919**
(**37.3 MB**) from Artifacts while signed into GitHub. Extract the outer artifact
ZIP and then its inner PlexStation-PPSA85301.zip, keeping the whole PPSA85301
folder. The matching PlexStation-build-diagnostics artifact contains logs and
validation.json. Installation/package and Unix ZIP mode gates all passed.

The outer installation artifact SHA-256 listed by GitHub is
`115442178758951e0dd3c2383fd815f24db0e550a8f0c7a19f7a750d58f890aa`.
The included SHA256SUMS verifies the inner installation ZIP. Public job/step
states and artifact metadata were checked; an authenticated artifact download
was not independently performed in this cloud environment. Physical PMS
connectivity and browsing remain pending owner confirmation.

## Device evidence and audited path

The owner confirms that authentication build cdfe733 launches on PS5 13.40,
kstuff Lite and ShadowMountPlus 1.7 Beta 5 Fix 1; Cross starts linking, account
linking completes, and PlexStation lists the owner's PMS. Selecting it then
failed with `Network or TLS request failed`, before libraries loaded.
This confirms account authorization/discovery, not a working PMS connection.
The original generic message is insufficient to identify the physical cause.

The production sequence is Client::resources -> servers -> UI server selection
-> Client::select -> candidate /identity -> /library/sections. The old reader
rejected IPv6 literals, treated negative TLS reads/writes as final errors, and
read until EOF even after a complete Content-Length/chunked response. It could
report receive failure when a keep-alive peer had already delivered the body.
Client::call parsed /identity only as JSON. Selection checked identity but
accepted the endpoint before authenticated sections, leaving no fallback when
that later request failed. A 401/403, other HTTP status, malformed JSON or
identity mismatch had separate messages; the reported generic text instead
came from unmapped transport errors (send/read/response framing, among others).
No existing device log establishes which of those failures occurred.

## Targeted changes

- Strict base-address/port parsing, IPv4/DNS and bracketed IPv6 literals,
  custom ports and IPv6 resource advertisement. Scoped IPv6 zone identifiers
  and reverse-proxy subpaths are not supported. The platform resolver still
  determines which address families actually work on hardware.
- Candidate order: HTTPS local, HTTPS remote, HTTPS relay, then explicitly
  approved local HTTP. Original plex.direct hostnames are preserved for SNI
  and certificate hostname checks; they are never replaced with their IPs.
- Retry later candidates after DNS/TCP/TLS errors, identity mismatch, denied
  library access or malformed library responses. Success requires matching
  machineIdentifier **and authenticated sections**, not merely /identity.
- Keep native sockets nonblocking. Handle EINTR/EAGAIN and GnuTLS direction
  changes; loop over partial sends/reads; bound connect and overall I/O time.
  The default is 5 seconds per address connect within a 20-second request
  budget. DNS getaddrinfo itself uses the platform resolver's timeout and
  cannot be interrupted by this deadline; it remains off the render thread.
- Verify default TLS configuration, CA loading, SNI, certificate chain and
  hostname. There is no insecure certificate fallback. Diagnose missing CA,
  certificate verification, unsupported TLS and handshake failures separately.
- Frame Content-Length/chunked responses incrementally and return when complete,
  without waiting for close. Validate lengths/chunks, bounded headers/body and
  response encoding; request identity encoding. Unsupported compression and
  malformed/truncated framing have explicit errors. No response/token/body is
  included in diagnostics. Redirects are not followed with credentials.
- Parse JSON or XML identity responses. TinyXML-2 is pinned at
  8224e427b655b83dae5e2298f1e6919523a78737, source/license retained and shipped.
  Only memory parsing is used; DTDs are rejected. Require MediaContainer and
  machineIdentifier, then compare with the selected resource's identifier.
- Carry typed DNS, TCP refusal/timeout/unreachable, TLS, I/O, encoding, HTTP,
  identity and library failures into safe user-facing diagnostics.

Authentication PIN/QR handling, native controller backend, playback/decoder,
audio, independent PPSA85301 identity, toolchain pins, compiled dependency
cache, workflow and 755/644 packaging remain intact. Host test/link recipes
and the title add the pinned XML parser; no SDK/dependency version is changed.

## Controller interface

**Triangle** opens **Server Connection Diagnostics** from signed-in, post-linking non-playback
screens outside the on-screen keyboard. A failed server selection opens it automatically.
**Up/Down** chooses an attempted endpoint. **Circle** closes the panel without
triggering the parent screen's Back action.

Each record shows local/remote/relay, HTTP/HTTPS, an ordinal/redacted host class
(IP, hostname or plex.direct), port, TCP result, TLS result, identity match,
authenticated library result and final failure/success. Tokens, full URIs,
private hostnames, machine IDs and response bodies are not displayed/logged.
The `conn:` log prefix contains the same sanitized information. Completed
attempts are published after the asynchronous operation finishes; while testing,
the panel shows a worker-in-progress message and remains controller-responsive.

Interpretation examples:

- DNS failure on plex.direct: check the PS5/router's resolver and DNS-rebinding
  protection. Do not disable certificate checks or rewrite a TLS hostname to IP.
- TCP refused: wrong port, PMS stopped, firewall actively rejecting connections.
- TCP timeout/unreachable: routing/firewall/NAT or an inaccessible advertised
  address. Later permitted candidates are still tried.
- Certificate verification: wrong hostname, missing chain, untrusted certificate
  or PS5 clock. HTTPS to an IP needs an IP SAN; a valid plex.direct DNS
  certificate does not automatically validate its numerical IP address.
- Identity mismatch: endpoint answered, but it is a different PMS; rejected.
- Identity matched, libraries HTTP 401/403: inspect sharing/server access and
  sign in again if needed. It is not a DNS or certificate failure.
- Encoding/framing/identity malformed: inspect a reverse proxy/server response;
  do not post its raw body or credentials in an issue.

## Manual endpoint: test, then save

1. On the **server picker**, highlight the intended discovered PMS and press
   **Square**. Settings > Manual server address also works for the currently
   selected server or a single discovered server. Selecting the resource first
   supplies the expected identity and server-scoped token; unknown/unadvertised
   server enrollment is not implemented by this manual override.
2. Enter `host`, `host:port`, `https://host[:port]` or `http://host[:port]`.
   IPv6 must be bracketed, for example `https://[::1]:32400` (use your real
   server's usable address). Omitted scheme is HTTPS; omitted port is **32400**.
   Cross types, Square erases, D-pad/stick moves, Triangle tests, Circle cancels.
3. For HTTP, read the unencrypted-traffic/credential warning and explicitly
   approve with Cross; Circle declines. This also saves the existing local HTTP
   permission preference. HTTPS-required servers reject HTTP even after approval.
   HTTPS failures never enable HTTP or rewrite the scheme automatically.
4. The test verifies the selected server identity and authenticated sections.
   Inspect diagnostics. Nothing is saved as an endpoint merely by testing it.
5. After success, **Cross saves and connects**; Circle goes back to editing.
   Only the base URI and expected server ID are saved in prefs.txt, not a token.
   Normal existing account credential protection is retained. Preferences require
   writable application storage with correct ownership, as documented for FTP.
6. After restart/sign-in, a saved override is added to that matching resource's
   candidates. Other advertised endpoints remain available. HTTPS-required and
   explicit HTTP policies still apply. Secure candidates retain priority.

A failed manual test or mismatched server must never be saved. An account change
only applies an override if the advertised server identity matches; tokens come
from that account's current resource response, never from endpoint preferences.

## Validation and PS5 test procedure

Run `bash host/test-connections.sh` plus the existing core, authentication,
controller, native transport and strict source checks. New coverage includes
JSON/XML identity, malformed/DTD/mismatched identities, secure/local/remote/relay
fallback through sections, explicit HTTP policy, HTTPS-required policy, custom
ports, IPv6 parsing, incremental HTTP framing and redacted error reports.

The real production GnuTLS/socket code is exercised against loopback peers:
a test-CA-signed certificate for a plex.direct-shaped hostname, exact SNI,
wrong-host rejection, missing CA, handshake failure, read deadlines, real TCP
refusal, fragmented Content-Length/keep-alive responses and IPv6 loopback when
available. Test-only linker wrappers supply deterministic DNS routing/failure,
TCP-timeout readiness, interrupted/would-block I/O and short writes/reads.
There are no such overrides in production. A test CA for fixture.plex.direct
is not a real Plex-issued certificate or a live Plex server test. Existing
transport fixtures also reject self-signed certificates against the app CA set.
UI tests cover diagnostics, manual HTTP consent, verification before saving
and return to connected libraries. Preferences are written/read in separate
host processes to verify persistence under normal filesystem permissions.

On PS5, close the old app and deploy the whole new PPSA85301 folder using the
[755/644 Windows FTP procedure](CONTROLLER-TESTING.md#windows-11-ftp-deployment-and-permissions).
Preserve account data at 600 and the current jailbreak/launcher installation.

1. Relaunch and confirm your account remains signed in and PMS is listed.
2. Select PMS. Check identity and real movie/TV libraries load, then browse.
3. Open Triangle diagnostics; record endpoint type/port and each phase result.
4. If local DNS/TCP fails, inspect fallback results and try a manual endpoint
   for the **same selected PMS**. Prefer the advertised HTTPS plex.direct name.
   Use HTTP only after your explicit approval and only if PMS permits it.
5. Restart after a successful manual save and repeat selection/library browsing.
6. Test the permitted remote/relay path separately (for example from an actual
   different network), checking it is reported as remote/relay and verified.
   Do not assume a LAN test establishes remote NAT/relay connectivity.
7. If it still fails, share only the redacted diagnostic screen fields or
   `conn:` lines and whether failure is identity or library access. Do not share
   account files, tokens, tokenized URLs, raw API responses or Plex credentials.

**Physical PS5 PMS connection, actual Plex-issued certificates, real remote/relay
access and media browsing remain pending owner verification.** Host fixtures
and native compilation cannot satisfy those acceptance criteria.
