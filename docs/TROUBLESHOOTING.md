# Troubleshooting

- **Source download or Plex documentation returns HTTP 403 / CONNECT denied:**
  this cloud proxy blocked the destination. Review saved network changes in
  environment settings; saving a draft alone does not change runtime access.
  Required initial additions are developer.plex.tv, plex.tv and
  download.videolan.org. Dependency recipes reference further source hosts;
  allow those specific hosts when needed. A remote Plex server also needs
  access to its actual hostname. Never disable TLS verification to bypass this.
- **`plugins not built` in host build:** prepare the exact static VLC source,
  dependencies and plugin list using the upstream commands in the build guide.
  Installing a stock VLC executable is insufficient.
- **Missing `libvulkan_radeon.ps5.a` / `prospero-lld`:** PS5_Vulkan and its pinned
  SDK/platform prerequisites have not been built. Follow that project's own
  bootstrap and release RADV instructions. Existing source checkouts do not
  imply installed cross-toolchain support.
- **Device-link expires:** request a new link and complete it on a phone/PC
  before its displayed expiration. The app polls every two seconds and does
  not collect a Plex password. Live authentication is still unverified.
- **Access denied:** check account/server sharing; sign out and sign in again
  for revoked or expired credentials. There is no automatic token refresh
  claim. Sign-out clears local credentials, not remote Plex device access.
- **No endpoint succeeds:** check server power/network, HTTPS name/certificate,
  server sharing and proxy access. Discovery validates the server identity and
  tries permitted endpoints. HTTP is explicitly opt-in and unencrypted; enable
  it only for a trusted local endpoint if appropriate.
- **Native startup cannot save account data:** the app needs writable /app0,
  mode-0600 file support, ownership metadata, and a secure random source. Do
  not overwrite an unreadable/insecure account file blindly. Preserve it
  privately for investigation; never attach it to a bug report.
- **Unsupported playback:** currently only single-part H.264/HEVC MKV/MP4
  Original quality is accepted. Other formats, multipart playback and
  transcodes require further implementation. Do not label these as working
  because upstream VLC supports the underlying codec.
- **Audio offset direction:** positive milliseconds delay audio; negative
  advance audio. L1/R1 adjust by 25 ms, Triangle resets to zero. These calls
  reuse VLC's existing audio-delay mechanism. Improvement must be measured on
  the actual TV/audio setup through seeks, buffering and pause/resume.
- **No artwork:** the account must access the selected item's server image
  route. Missing/invalid images remain placeholders; no media is fabricated.
  The initial cache is memory-only and retries are limited.

Logs deliberately suppress unstructured VLC messages because they may include
playback credentials. Report generic error text and build versions; do not
share tokenized URLs, account files, raw HTTP bodies or packet captures with
credentials. Do not attach /app0/plexstation-account.json.
