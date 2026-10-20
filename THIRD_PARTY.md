# Licenses and attribution

PlexStation is a derivative of [ZiZc3/VLC](https://github.com/ZiZc3/VLC),
imported at `a91113b9731241dbfc159fd671290ff42afb2db3`. The upstream root
license is GNU GPL version 3. Original source SPDX notices, including
GPL-2.0-or-later notices, are retained. New PlexStation code is
GPL-3.0-or-later. See LICENSE and docs/UPSTREAM-README.md for upstream credits.

The imported source includes Dear ImGui (MIT), volk (MIT), stb (dual public
domain/MIT), AMD FSR 1 (MIT), FSRCNNX (LGPL-3.0), Anime4K (MIT), and fonts
with their own licenses. License files remain alongside third_party and
assets/fonts. Shader sources and all original attribution remain intact.
Original PlexStation branding is in assets/plexstation.svg; it does not use
Plex's logo. Imported VLC artwork is retained as upstream source material,
but the PlexStation title uses the new icon and loading images.

New JSON dependency: [nlohmann/json 3.11.3](https://github.com/nlohmann/json/tree/v3.11.3),
MIT; the complete license is third_party/nlohmann/LICENSE.MIT.
The host-only HTTP client links libcurl under its curl license; it is not
bundled as a binary here. Native PS5 networking uses upstream's GnuTLS
prerequisite, with certificate and hostname verification enabled.

The playback build additionally compiles VLC 3.0.24, FFmpeg and the libraries
listed in ps5/deps/build-deps.sh. Their licenses are separate and must be
retained in any distribution. Codec and platform licensing need review for
any intended distribution; no claim of Sony or Plex endorsement is made.
PS5_Vulkan (GPL-3.0) and its pinned PS5 payload SDK fork are separate build
dependencies, not proprietary SDK components bundled into this repository.

No PS5 binary release has been produced. Before distributing one, supply the
complete corresponding source for that exact build: this application,
patched VLC, FFmpeg, other linked libraries and platform runtime, and the
scripts needed to rebuild them, with their notices. The application-only
source archive helper is not by itself a complete source release for a
statically linked title.
