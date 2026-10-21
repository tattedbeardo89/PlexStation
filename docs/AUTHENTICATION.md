# Plex TV account linking

## Audit and references

The owner confirms that the controller-fix build launches on PS5 and Cross
starts authentication. The old implementation explicitly requested
`strong=true`, displayed that long code as an activation code, and printed
`https://plex.tv/auth#?...`. The observed 1800-second lifetime is server-provided,
not proof that the correct TV flow was selected. Returning a PIN successfully
does not establish successful linking.

References inspected during this change:

- Plex-owned [Plex for Kodi PIN login](https://github.com/plexinc/plex-for-kodi/blob/97191f248a20223bb93296789c010ff2204a9c76/lib/_included_packages/plexnet/myplex.py)
  creates a non-strong PIN and polls for auth_token. Its
  [sign-in UI](https://github.com/plexinc/plex-for-kodi/blob/97191f248a20223bb93296789c010ff2204a9c76/lib/windows/signin.py)
  renders four characters. This is official client source, using the older
  XML endpoint; PlexStation retains the v2 JSON API.
- The [available authentication guide](https://github.com/LukeHagar/plex-api-spec/blob/e144d650d6b03fac2aee93e954a7bf0b236245da/description.md)
  distinguishes non-strong short-lived codes from longer strong codes,
  documents POST /api/v2/pins and GET /api/v2/pins/{id}, and specifies
  `https://app.plex.tv/auth#?clientID=...&code=...&context[device][product]=...`.
  This is a community-maintained reference, not a directly retrieved current
  official documentation page. Its OpenAPI PIN description names plex.tv/link.
- [Python-PlexAPI PIN login](https://github.com/pkkid/python-plexapi/blob/dea7cad9227ec976fa93c363c43c01b3cee305af/plexapi/myplex.py)
  separately supports four-character plex.tv/link login and strong browser
  authentication. Its browser router uses the /#!? variant; PlexStation uses
  the #? fragment documented by the available guide.

Direct requests to developer.plex.tv, Plex Support and app.plex.tv/auth were
HTTP 403 in this environment. An unauthenticated live PIN-creation probe also
failed at the environment network layer (URLError); no code/body/credential
was printed. These limitations are not evidence that the PS5 cannot connect.
Current official-host documentation and live authorization remain unverified;
the implementation is grounded in the accessible source/references above.

## Implemented flow

1. Cross requests a fresh v2 PIN with **strong=false**, JSON Accept and stable
   X-Plex-Client-Identifier plus PlexStation application/device metadata.
2. A four-character alphanumeric server code is displayed in large type with
   **https://plex.tv/link** instructions. The code is never shortened or altered.
3. A local QR code encodes the active session's complete **app.plex.tv/auth**
   URL: encoded clientID, actual issued code, and product context. No permanent
   token, forward URL or external QR-image service is involved. QR modules use
   integral pixels and a four-module quiet zone at 1080p/4K.
4. If the server unexpectedly issues a long code, it is preserved and used in
   that browser-auth QR. The UI shows short browser instructions, never a long
   URL or a truncated code. Failure to generate a usable QR becomes a visible
   retry error in that case.
5. Poll GET /api/v2/pins/{id}?code=... at two-second intervals after completion
   of the previous request. Requests run on the existing asynchronous worker.
   Rendering and controller input never wait for network completion.
6. Respect expiresIn using a monotonic deadline, conservatively accounting for
   request time. Poll responses can shorten but cannot extend the session.
   Validate response identity/code and authToken type. Pending null/empty tokens
   continue polling; authorized tokens are privately persisted at mode 600.
7. Automatically load Plex resources and focus the first server. Saving failures
   are explicitly reported as session-only sign-in, not successful persistence.
8. On restart, validate the saved account through GET /api/v2/user before loading
   resources. A 401/403 clears the rejected saved account; temporary network
   failures preserve it and offer Retry. Sign-out clears saved authentication.

Circle cancels linking. A generation counter discards in-flight responses from
cancelled sessions. Cross requests a new session once that outstanding worker
finishes; the screen explains the wait. Expiration, invalid responses, network
failures and TLS problems produce visible errors with Retry instead of an
indefinite waiting screen. The original PIN API remains available with
create_pin(true) for tests/explicit browser callers; normal TV sign-in uses false.

No controller polling/mapping, VLC playback/decoder/audio, title ID, toolchain
pins, dependency fingerprint or packaging modes were changed by this task.
A pinned MIT C QR library is compiled into the title and its license is shipped.

## Validation and device acceptance

Run:

```sh
bash host/test-auth.sh
bash host/build-core.sh
bash host/test-controller.sh
bash host/check-native-source.sh
bash host/test-native-transport.sh
```

Authentication fixtures cover short/strong creation, URL encoding, pending,
approval, malformed/mismatched/expired PINs, cancellation/late responses,
network failure, token persistence/restoration and sign-out. UI tests exercise
actual production completion handlers, server transition, startup validation
and invalid-token clearing with real ImGui and fake network/player boundaries.
The generated fixture QR was independently decoded with zxing-cpp and matched
the entire expected authorization URL. To repeat that optional check, install
zxing-cpp and Pillow in a host test environment and run
`python3 tests/decode_auth_qr.py` after host/test-auth.sh. This is not a live account QR test.

On your existing PS5 13.40 / kstuff Lite / ShadowMountPlus 1.7 Beta 5 Fix 1:

1. Close PlexStation, deploy the complete new PPSA85301 folder using the
   [755/644 FTP procedure](CONTROLLER-TESTING.md#windows-11-ftp-deployment-and-permissions).
   Preserve the private account file at 600; do not recursively chmod runtime
   credentials or replace the jailbreak environment.
2. Cross: confirm a new readable code, plex.tv/link instructions and QR.
3. On your phone/PC, open **https://plex.tv/link**, sign in to Plex and enter the
   **entire code shown**. Alternatively scan the QR and approve PlexStation.
4. Confirm the PS5 advances automatically to server selection without restart.
5. Close/relaunch and confirm the same account is restored and servers load.
6. Sign out and relaunch: confirm it stays signed out.
7. Test Circle cancellation, new-code retry, expiration and a temporary network
   outage. Cancelled/expired QR sessions must not sign in later. Restore the
   connection and retry. Do not change TLS validation settings.

Do not share tokens, account files, QR images or active PINs. Report the visible
status and whether the phone accepted the code/QR. **Live Plex authorization,
physical PS5 QR readability, account restoration and full navigation remain
pending owner confirmation.**
