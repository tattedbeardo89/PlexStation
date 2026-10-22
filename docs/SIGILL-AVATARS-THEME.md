# f511344 SIGILL investigation and native redesign checkpoints

## Physical evidence and exact binary inspection

Owner-confirmed f511344 / run 39797264696 launches, restores authentication,
discovers real Home users and PMS choices, then fails after server selection.
ShadowMountPlus reports signal 4 twice (PIDs 96/99), raw RIP 0x004f93d7, caller
0x80000cc3f, exit_value=8c and a privileged-instruction classification. These
observations do not establish an unsupported instruction/syscall root cause.
The earlier 2458e45 Movies SIGABRT and unmatched SIGSYS remain separate evidence.
The original f511344 application headers later established SIGABRT as described
in the numeric-parser follow-up below.

[Matching artifact inspection run 39844239411](https://github.com/tattedbeardo89/PlexStation/actions/runs/39844239411)
downloaded the exact original debug and installation artifacts using read-only
Actions access. All manifest ELF hashes and packaged executable/runtime hashes
matched. No executable was rebuilt for that inspection.

- Original packaged eboot SHA256: `445edc0b298e1b6d8f1c9b5df15315929dd22ca0f2431c323c1694c03fe133ed`.
- Runtime libc SHA256: `e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036`.
- llvm-pie executable VMA range: `[0, 0x1f90750)`.
- With the subsequently marker-verified runtime bias 0x400000, raw RIP maps to ELF 0xf93d7:
  `crash_handler`, exactly `0f 0b` / `ud2`, immediately following the indirect
  `_exit(128 + sig)` call at 0xf93d1. Nearby lines are platform_ps5.c 122–126;
  exact fault line was reported as line 0, so no precise source line is invented.
- With bias zero, raw RIP maps inside pathbuf_reserve, in the immediate bytes
  of a `mov` starting at 0x4f93d6. This is not a normal instruction boundary;
  it is not reliable evidence that pathbuf_reserve caused the failure.

The exit observation 0x8c equals 128+12, but does not establish the first signal.
The subsequently retrieved original application headers explicitly record
SIGABRT and the JSON assertion. Marker verification is documented below. Shared
caller 0x80000cc3f cannot be symbolized with the application ELF. The inspection
artifact is `PlexStation-f511344-SIGILL-analysis`; its summary includes real
disassembly. Never symbolize a new executable against this old ELF.

## Checkpoint A changes

Crash termination calls the existing exit bridge through an opaque volatile
pointer and enters an explicit terminal wait if it unexpectedly returns. It
cannot fall into compiler-generated unreachable trap bytes. This does **not**
recover the app, suppress the original fatal signal, or fix the unknown initiating
fault. A returning-bridge host regression verifies the terminal fallback path.
Notifications and buffered stdio flushing are removed from fatal signal context;
null signal/context pointers are guarded. The direct crash record preserves the
original signal/raw RIP/build and last Home phase. No jailbreak/syscall table,
VLC decoder or native runtime replacement is introduced.

CP01–CP15 remain. Separate bounded, flushed H01–H13 records cover profile choice,
validated identity, accepted server choice, verified PMS identity, library start/
finish, Home request/parse/population, first artwork/decode/upload and submitted
Home frame. Build/thread/outcome/count are recorded without names, tokens, PINs,
URLs or bodies. Startup logs include PID and plat_init address for mapping.

Developer Options adds Detailed Home Initialization Tracing and Safe Startup.
Before launch, add `plex_safe_home_v1=1` to the existing console prefs.txt to skip
automatic Home loading after verified libraries. This opens the library UI so
Options → Settings → Developer Options is reachable. Cross/Home can explicitly
attempt Home loading. Other isolation flags remain independently available;
overlays remain hidden by default. Safe startup is a diagnostic mode, not a
claim of crash resolution, and initialization prior to its gate is still traced.

## FTP evidence for physical acceptance

Close the title before replacing files. From its actual PPSA85301 installation
folder (normally /data/homebrew/PPSA85301), retrieve plexstation.log,
plexstation.old.log and vlc-crash.txt plus the corresponding ShadowMountPlus
debug.log. Preserve credential/preferences/listening/progress files during updates.
Use Windows PowerShell `Get-FileHash .\eboot.bin -Algorithm SHA256` on the console
executable copied by FTP. Share reviewed crash headers/Hxx/CP/gfx lines and the
plat_init marker only; never share account files, PINs, URLs or raw responses.

The later first-fault/parser evidence is documented below. Physical runtime
acceptance remains pending. Avatar and theme checkpoints follow independent A.

## Original first-fault evidence and numeric parser correction

The owner subsequently retrieved BOTH original f511344 crash records: signal 6,
raw RIP 0x8000b90c0, and the identical nlohmann/json 3.11.3 `scan_number`
end-pointer assertion at json.hpp:8661 after authenticated library discovery.
This supersedes the earlier signal-12 inference from exit_value alone. The
unmatched historical SIGSYS report remains uncorrelated. The shared-module
abort address cannot be symbolized from the application ELF. SIGILL at the
handler's UD2 is consistent with a secondary termination failure, not evidence
that Home itself executed a privileged instruction.

The package SHA supplied by the owner matches the inspected debug manifest.
Their runtime plat_init marker is 0x4f8510; the inspection script now records the
matching ELF symbol and derives the load bias directly. Installed console bytes
remain independently unverified until an FTP copy is hashed.

The pinned SDK b83202be73e930050e00de0f1b4d9ec46c0391bf,
platform/src/locale.c, explicitly documents PS5 `localeconv()->decimal_point`
being empty while exported `strtod` accepts a dot. JSON 3.11.3 handles a null
separator but not an empty one: its lexer inserts a NUL into decimal tokens,
then asserts because strtod cannot consume the full buffer. Running the exact
unmodified production header with an empty-separator locale shim reproduces
SIGABRT and the same assertion at line 8661. This is a confirmed parser defect
under the documented runtime behavior; a native self-test will verify the
current console's behavior before authenticated requests.

The local vendored-header change treats an empty separator like the existing
null-separator case (dot). Assertions, overflow checks, malformed-input
rejection, compiler/toolchain pins and TLS checks remain enabled. All production
JSON parsing sites share this header: Client::call (Home hubs, discovery,
profiles, metadata), account decoding, and ListeningStore loading. No parser is
replaced. Numeric serialization round trips are included in regression coverage.

`host/test-json-numeric.sh` exercises normal and empty-separator configurations
with the native no-exceptions/no-RTTI flags, integers and unsigned limits,
floating values, exponents, subnormal/max-double boundaries, integer overflow,
float overflow/underflow and malformed syntax. Both variants also run under
ASan/UBSan. `json_numeric_selftest()` runs immediately after plat_init and before
graphics/network/authentication; only fixture results and separator category are
logged. A failed test stops startup with a useful message rather than continuing
into Home. Client parse begin/end diagnostics use fixed operation labels and
byte counts, never numeric token contents, account data, URLs or response bodies.

On PS5, first confirm `json: numeric selftest=pass` and review separator and dot
conversion categories. Then test remembered-server reconnect, server-to-Home,
Movies entry, text-only isolation and artwork-enabled browsing. Preserve the
complete logs if a failure occurs. Host reproduction and a successful native
build do not constitute physical crash-resolution verification.

### Verified marker mapping

[Inspection run 39845804159](https://github.com/tattedbeardo89/PlexStation/actions/runs/39845804159)
verified the exact ELF `plat_init=0xf8510`. Compared with the owner's runtime
marker 0x4f8510, this yields load bias **0x400000** and maps debugger RIP
0x4f93d7 to **crash_handler+0x3a7, ELF 0xf93d7, UD2**. Thus the marker resolves
the earlier ambiguous mapping. The original SIGABRT assertion is the initiating
failure identified by the application log; the later handler trap is secondary.
This mapping is tied to the owner-reported build/marker and verified package;
an FTP hash would additionally verify the deployed console file.

## Checkpoint B: real Plex Home avatars

Avatar loading now accepts Plex-origin relative paths, validates HTTPS origins,
and follows at most three redirects across the existing Plex image allowlist.
Transport implementations expose Location without enabling global redirect
following. Authentication headers are supplied only to plex.tv; token query
parameters are removed, including rejecting encoded query keys for propagation.
CDN requests never receive the account token header. Unexpected destinations,
HTTP downgrade, duplicate Location headers, oversized/corrupt images and
unsuccessful HTTP responses remain rejected. Logs report fixed outcome, HTTP
status and byte/dimension counts only.

Real PNG/JPEG and existing image decoders share the existing worker and render
thread upload path. Legitimate avatars up to 2048 square are center-cropped and
bounded to 256 square before upload. The poster bounds remain unchanged. The
12-entry cache and generation cancellation remain; failed fetches retry after
300 update frames rather than being remembered forever. Profile cards display a
loading/unavailable fallback and draw real images circularly. No new image or
controller library is introduced. URL, safe redirects, withheld credentials,
real PNG/JPEG decode, large avatars and transient retry receive host coverage.
Physical account avatars, Home entry and GPU lifetime acceptance remain pending.

## Checkpoint C: native navy/indigo theme

`src/theme.h` owns the shared palette, typography, radii, spacing, focus stroke,
ImGui widget colors and motion policy. All linked PlexStation UI files use its
color tokens; the retained upstream src/ui.cc is reference-only and remains
unchanged. The target navy/indigo palette replaces the former gold selection
colors. QR black/white contrast and original PlexStation name/icon/boot artwork
are preserved.

The shared design covers linking/loading/error states, profile/PIN selection,
server picker/diagnostics, Home/Continue Watching, movie/TV browsing and details,
Music/Audiobooks browsing/Now Playing/queues/bookmarks/sleep controls, search,
watchlist, settings and developer options. Native draw lists provide subtle
alpha gradients, rounded poster/album/landscape cards, indigo focus outlines,
watched/progress indicators, genre chips and readable clipped/wrapped text.
Home's feature banner uses actual selected-media artwork and metadata; no
catalogue entries or profile pictures are fabricated. Existing asynchronous
artwork transport/cache and Vulkan descriptors are reused.

The sidebar retains permission/capability-based visibility and adds distinct
native line icons, a persistent active indicator, real active-profile avatar and
bounded username/navigation labels. Music/album cards use square presentation;
Now Playing has a low-cost indigo gradient. The logical 1920x1080 layout scales
with output height for 3840x2160, including font rasterization, spacing and
focus strokes. No browser, new graphics library, shader pass, blur pipeline or
VLC decoder/audio changes are introduced.

Settings → Appearance / Motion → Reduced Motion persists in
`plex_reduce_motion_v1`. It disables focus pulsing and makes carousel movement
instant. Controller behavior and actions are unchanged. Diagnostic overlays
still default off. Safe Startup preference restoration additionally ignores a
stale SafeHome bit in the general diagnostic snapshot, so turning it off stays
off on a later launch.

Host controller fixtures exercise the actual ImGui UI, motion preference,
startup restoration, finite geometry and bounded draw-command clips at 1080p
and 4K, including long names/titles, profiles, details and Home. The theme
boundary test prevents linked screen files from bypassing centralized colors.
These are draw-list/controller regressions, not screenshots or PS5 visual
acceptance. Actual layout, avatar quality, smoothness, Home stability and
playback remain for physical acceptance.

## Physical comparison procedure

1. Keep the complete original f511344 package and matching symbols. First test
   the pre-theme parser/avatar checkpoint, then the final themed checkpoint.
   Do not use the earlier instrumentation-only binary as the parser fix.
2. Close PlexStation. Extract the outer Actions ZIP, then the inner installation
   ZIP. FTP the COMPLETE PPSA85301 folder using binary transfers. Preserve your
   private account, prefs, listening and progress files. Do not include them in
   shared logs or backups. Use 755 for directories/executables and 644 for
   read-only package data; private runtime account files remain 600. Windows
   extraction/FTP does not reliably preserve ZIP Unix modes: apply server-side
   FTP permissions recursively by category. Do not set everything to 777.
3. For normal Home acceptance, disable Safe Startup and the text-only/no-download/
   no-decode/no-upload isolation flags. If needed, set `plex_safe_home_v1=1`
   before launch to regain settings access for further isolation.
4. Confirm the new build line, plat_init marker and `json: numeric selftest=pass`.
   Confirm profile pictures/PIN navigation, remembered-server authorization,
   Home/Continue Watching and entering Movies before starting playback.
5. Browse rapidly, change profiles/servers, test empty/error libraries, Music,
   Audiobooks/Continue Listening, queues, bookmarks, resume and VLC playback.
   Check sidebar, long names, focus and Reduced Motion at available resolutions.
6. If a failure occurs, retrieve original crash headers and the final json/Home/
   CP/avatar/gfx checkpoints before a second launch overwrites them. Include the
   matching package/debug build SHA and an FTP-downloaded eboot hash. Never
   submit credentials, PINs, playback URLs or complete API responses.

Unresolved: original f511344 fractional-token contents are intentionally not
logged; the documented runtime mismatch and exact host assertion reproduction
identify a concrete defect, while the new native fixture/device run must verify
its correction on this console. The earlier unmatched SIGSYS and old Movies
SIGABRT cannot be correlated to an exact operation from the available evidence.
The PCI UUID warning and zero-byte artwork checkpoint are not established crash
causes. No physical crash-resolution claim is made.
