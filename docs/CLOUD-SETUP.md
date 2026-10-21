# Cloud setup state

The working repository is /workspace/PlexStation. The initial implementation
and native build automation are committed and pushed to
https://github.com/tattedbeardo89/PlexStation on
`feature/plexstation-initial-implementation`. Dependency/source caches are
under /workspace/deps, plus /workspace/upstream-vlc as the preserved import
checkout. No unrelated existing project was changed.

Saved cloud draft fields:

- `install_script`: builds/runs core and both HTTP transport suites; checks new
  native source using the pinned Vulkan headers. It does not pretend to build
  the missing static VLC/PS5 dependencies.
- `start_skill`: existing-checkout guidance and tool/path initialization, host
  development commands, native-build limitations and the user's exact runtime
  target. No persistent service is needed for the verified workflow.
- Custom network additions: developer.plex.tv, plex.tv,
  download.videolan.org, dl.matroska.org, download.gnome.org,
  download.savannah.gnu.org, ffmpeg.org, ftp.gnu.org, www.gnupg.org,
  www.nasm.us and zlib.net. Existing package-manager presets were preserved.
  These are the documented API/source hosts, not a global wildcard.

Draft persistence was confirmed by the configuration tool. It reported that
publication is required. Review and save the changes in environment settings,
then publish when appropriate. Draft saving does not apply networking, execute
installation or publish a snapshot. No fresh-task restoration was tested.

Current host dependencies came from the provided machine, except the pinned
Vulkan headers, glslang build and Python-packaged CMake/Ninja/clang-format
under /workspace/deps. The saved refresh script requires g++, pkg-config,
libcurl and GnuTLS development packages, Python 3 and openssl, all present in
this instance. It has no secret values or Plex account binding. Current checks
use local fixtures only; no claim of live Plex authentication is made.

After networking is applied, retry official docs and VLC source acquisition,
verify source integrity, then prepare the installed SDK/LLVM and static native
libraries using the build guide. Update startup instructions only after a real
native executable is built and tested. Do not replace the user's jailbreak.
