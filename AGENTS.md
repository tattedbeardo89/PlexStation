# PlexStation development

Read README.md, docs/ARCHITECTURE.md, docs/PROJECT-SPEC.md and docs/VALIDATION.md before changing
implementation. The pasted user specification is the intended end state;
current source and tests do not establish feature completeness.

Use this existing isolated checkout; do not create a worktree unless requested.
Preserve the libVLC playback, PS5 hardware decoder and timestamped audio paths.
Do not replace them with browser playback or report simulated playback as real.
Keep the independent PPSA85301 title identity and preserve third-party notices.

Run `bash host/build-core.sh` for core and host transport tests.
Run `bash host/check-native-source.sh` for strict host syntax checks of new
native source, and `bash host/test-native-transport.sh` for Linux execution
of the GnuTLS/socket transport. These are not PS5 cross-builds.
Full build commands and current blockers are in docs/BUILD-AND-DEPLOY.md.
Do not claim live account, video, audio sync or firmware compatibility without
corresponding acceptance evidence. Update the validation record accurately.

Never log tokens, account files or playback URLs. Credentials belong only in
private local runtime storage and must not enter source or release archives.
Do not disable TLS/signature/checksum verification to work around setup errors.
