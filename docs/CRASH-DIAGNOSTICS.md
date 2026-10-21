# Native crash investigation and Checkpoint A

The owner reproduces SIGABRT/CE-108255-1 on entering a Movies library, before
movie selection or playback. Authentication, PMS connection and library
retrieval are now owner-confirmed. The reported `eboot+0x7ffcb90c0` is **not a
reliable app-relative address**: the old handler unconditionally subtracted
0x400000 from the raw RIP read using the imported PS5 context layout. That
reconstructs to 0x8000b90c0, outside its assumed [0x400000,0x10000000) executable
range. It may be a shared-module abort path, or the context/layout assumption
may be wrong. A bare offset cannot distinguish these cases. No matching old
unstripped artifact or full crash/module trace is currently available here.
Do not symbolize it against a new build or call it proof of an artwork fault.

The handler now labels raw RIP and fixed-base subtraction separately, warns
when outside the assumed range, and avoids unsafe raw-stack dereferences.
These bounds remain assumptions, not a verified loader map. Signal-context
layout and physical symbolization still require device validation. C++
termination now prints a safe generic marker before aborting. Existing assertion
messages and numeric Vulkan result logs remain in the application log.

## Numbered checkpoints

CP01 library selection; CP02 request preparation; CP03 HTTP start; CP04 response
received (size only); CP05 response validation; CP06 parser start; CP07
MediaContainer; CP08 metadata extracted; CP09 bounded listing reserve; CP10 UI
replacement; CP11 artwork scheduling; CP12 artwork download; CP13 image decode;
CP14 GPU upload; CP15 first grid render. Numbers label operations, not necessarily
monotonic completion order: extraction/reserve and async images overlap.
No paths, titles, tokens, PINs or response bodies enter these checkpoints.
Critical parsing checkpoints are flushed. Logging is capped at 128 records per
library selection, or 1024 with detailed tracing. Existing input logs remain
throttled; very long sessions should collect/rotate logs through normal restart.

## Isolation before entering a library

Options → Settings → Developer Options offers text-only rendering, independent
artwork download/decode/GPU-upload switches, 10-result requests, detailed tracing,
controller diagnostics, endpoint diagnostics and build information. All isolation
modes default Off. The controller overlay uses a new preference key, starting
hidden after upgrading older diagnostic builds. Triangle endpoint diagnostics
works independently. Text-only mode uses real metadata and the same parser.

First try the ordinary build, then text-only + no downloads + 10 items; separately
restore downloads, decode and GPU uploads. Change one setting at a time and
record the last checkpoint and exact build SHA. This isolates the failing phase
without pretending that hiding posters fixes the underlying problem.

## Debug artifact and symbolization

Each successful build retains a separate `PlexStation-native-debug-<commit>`
artifact with `llvm-pie.elf` (unstripped, application code compiled with `-g`),
converted `eboot.elf`, linker map, sorted symbols, ELF layout/build ID, link log,
and `build.json` checksums for both ELFs and the installed signed executable.
Dependencies retain whatever debug information their pinned builds provide.
The installable executable still follows the same conversion/signing pipeline;
this is not a substitute debug executable. Debug artifacts are not deployed.

On Linux/WSL with LLVM installed, extract the matching artifact and verify the
console's eboot.bin SHA256 against `installed_eboot_sha256`. Use the runtime
`plat_init` address marker, matching symbol/map, and verified module/load mapping
to establish any load bias. Never guess a shared module's offset. Then run:

```sh
python3 symbolize-crash.py llvm-pie.elf 0xRAW_INSTRUCTION_ADDRESS --load-bias 0xVERIFIED_BIAS
```

The helper verifies the ELF hash and rejects addresses outside executable
PT_LOAD ranges before invoking llvm-addr2line. If RIP is inside libc/libkernel,
matching symbols for that module plus trustworthy caller frames are required;
the application ELF cannot symbolize it. For old reports lacking raw registers,
module mapping and matching binaries, reliable symbolization is unavailable.

## FTP evidence and hardware test

Copy `plexstation.log`, `plexstation.old.log` (before the next launch rotates it)
and `vlc-crash.txt` from PPSA85301's actual installation directory, usually
`/data/homebrew/PPSA85301/`. During sandboxed execution this is `/app0`.
Close the title before updating and preserve prefs.txt/account credentials.
Review logs and share only CP/fatal/gfx messages and crash headers; never upload
private account files, PINs, raw responses or playback URLs.

Confirmed code defects: stale response/focus safety, unbounded item extraction,
and a misleading crash offset label. Preventive changes include isolation modes,
malformed entry filtering, nesting limits and resource diagnostics. No root cause
of the physical SIGABRT is yet proven, and no physical crash resolution is claimed.

## Separate ShadowMountPlus SIGSYS evidence

ShadowMountPlus reports launch/sandbox/core-dump/monitor setup at 15:03:19 for
PID 242, then target signal 12 (SIGSYS), thread eboot.bin, RIP 0x8000003ac and
backtrace[0] 0x80000cc3f at 15:03:20. On the PS5/FreeBSD target SIGSYS is 12;
Linux host signal numbering must not be applied. This is tracked separately
from photographed SIGABRT. A one-second timestamp difference suggests a startup
record but cannot prove it: the installed build, same-launch UI milestones and
complete ordered logs are not yet known. We cannot identify a library-selection
failure solely from the debugger message.

Both raw debugger addresses lie outside the old assumed application range.
Without a matching ELF **and** a trustworthy module/load map they must not be
fed to application addr2line. The precise installed build, eboot.bin SHA256,
ShadowMountPlus debug.log around the entire launch, plexstation.log and
vlc-crash.txt can correlate build, PID, timing, thread and last checkpoint.
They may identify a shared-module syscall wrapper or other failure; that is
currently a hypothesis. SIGSYS alone does not identify a syscall number or
prove unsupported syscall execution. No firmware-specific syscall replacement
or jailbreak change is justified by the available evidence.

The application audit found POSIX operations routed through the pinned SDK,
existing mmap/munmap and libc compatibility remaps, pthread-backed async work,
and GnuTLS/socket I/O. There is no new direct raw-syscall instruction in the
Plex browse/artwork path. Native runtime compatibility remains a PS5 acceptance
question. The handler now includes target SIGSYS, records build SHA, numeric
si_code, raw addresses and thread handle; detailed checkpoints identify worker
thread handles. Register offsets remain inherited assumptions. No unvalidated
stack scanner is used. ShadowMountPlus backtraces/module mapping are preferable
to guessing from application subtraction. To identify an unsupported syscall,
collect the faulting module and exact bytes/registers from the debugger and
compare them with the **matching module** disassembly/runtime imports.
