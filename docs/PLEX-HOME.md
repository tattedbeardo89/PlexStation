# Plex Home profiles, authorization and persistence

Implementation references inspected from accessible source:

- [Official Plex for Kodi, MyPlexAccount](https://github.com/plexinc/plex-for-kodi/blob/97191f248a20223bb93296789c010ff2204a9c76/lib/_included_packages/plexnet/myplexaccount.py)
  uses GET `/api/home/users`, POST `/api/home/users/{id}/switch`, form PIN and
  XML `authenticationToken`.
- [Maintained Python PlexAPI](https://github.com/pkkid/python-plexapi/blob/dea7cad9227ec976fa93c363c43c01b3cee305af/plexapi/myplex.py)
  corroborates the switch POST and XML token. This is a community client, not
  official API documentation or live-server validation.

PlexStation first tries GET `/api/v2/home/users` with JSON Accept and the primary
account token. A 404/405 alone permits fallback to the legacy listing observed
in official source; authorization errors do not. JSON lists and bounded XML
`users/User` responses are parsed. POST `/api/home/users/{numeric-id}/switch`
sends the PIN in a form body, never a logged URL. Both XML and JSON switch
responses must contain the requested identity and a valid nonempty token.
That token is then independently validated with `/api/v2/user` before server
resources are loaded. Live behavior, endpoint availability, restricted-user
resource responses and exact permissions still require owner acceptance.

A primary account explicitly reporting no Plex Home uses its actual validated
identity as its sole profile. No fictitious users are inserted. Home-enabled
accounts cannot silently fall back to the administrator if listing/switching
fails. Unknown protection flags are treated as protected. There is no offline
PIN bypass, locally validated PIN, or administrator-token substitution for a
managed user's PMS discovery. The primary account's private 0600 credential
file remains separate; switched profile tokens exist only in memory. PINs are
masked, cleared after use/cancellation, and never written to settings/logs.

## Controller paths

After linking, **Who's Watching?** shows actual names, permitted Plex HTTPS
avatars, fallback icons and PIN indicators. D-pad/stick moves; Cross selects;
protected profiles use a masked native four-digit keypad. Square erases, Circle
cancels PIN entry. Incorrect authorization keeps the profile locked. Options
signs out when idle. Requests are asynchronous; cancellation discards late work.

From the main sidebar, Up from Home focuses the visible profile name. Cross
opens **Profile → Switch User**. The same controls are available through
**Settings → Profiles**, including **Show Who's Watching at Startup** (default
On). Turning it Off restores the saved profile if it is still listed; a protected
profile still prompts for its PIN and obtains fresh authorized tokens. It never
reuses a persisted switched token.

## Isolation and updates

Settings are scoped by validated primary account ID plus stable profile UUID
(or numeric ID if no UUID is provided). Each profile remembers its own PMS and
audio default. Only the validated original owner can migrate its earlier
account-only PMS preference. No managed profile inherits it. Switching users
stops playback, invalidates old operations, clears metadata/history/artwork and
queued progress, then verifies the new identity and PMS access. Already-running
old timeline calls retain their captured old token; queued unsent progress is
dropped rather than attributed to the next user. Network failure of such an
update can lose recent progress; there is no durable offline progress queue.

Discovery, identity checks, libraries, metadata, artwork and timeline/watched
requests use the active profile's resource-scoped server token. Native VLC's
media URL is derived from that same server token. Authorization/progress
attribution is fixture-tested, not yet confirmed on Plex/PS5. Home/Continue
Watching uses that context too when implemented by the Home interface layer.

Close PlexStation before deploying updates. Preserve `prefs.txt` and private
`plexstation-account.json`, correct ownership, and account-file mode 600. Normal
package files remain 755 executable/directory and 644 ordinary resources.

Raw button/focus diagnostics are suppressed during PIN entry in the UI, main
loop and PS5 pad backend, even if the overlay is enabled. This prevents keypad
choices being inferred from debug logs. The separate Home layer now implements
actual scoped Continue Watching and other supported PMS hubs; live permissions
and progress attribution still require the checklist in TV-CLIENT-STATUS.md.
