# Codex Project: PlexStation, Native Plex Client for Jailbroken PS5

## Objective

Build a fully functional, native PlayStation 5 homebrew Plex Media Server client called **PlexStation**, using the open-source VLC for PS5 project as the foundation.

Source repository:
https://github.com/ZiZc3/VLC

Official Plex API documentation:
https://developer.plex.tv/pms/

Target environment:
- Jailbroken PlayStation 5
- Firmware 13.40
- Existing homebrew environment using kstuff and compatible homebrew launch mechanisms
- Linux development/build environment
- Native PS5 executable and deployable homebrew title
- DualSense controller navigation
- 1080p and 4K television support

This must be a real, functional application, not a prototype, demonstration, mockup, or web page.

**Primary goal:** Combine the native video playback functionality of VLC for PS5 with a polished, Plex-inspired television user interface and full Plex Media Server integration.

## 1. Project setup and architecture

1. Clone and inspect the VLC for PS5 repository, including its source code, dependencies, build system, UI implementation, networking, playback engine, and PS5 platform integrations.
2. Create an independent application named PlexStation.
3. Preserve and reuse the existing native video playback and hardware decoding infrastructure wherever practical.
4. Maintain the existing controller, video, audio, and graphics integrations.
5. Keep the application's Plex API code modular and separate from the playback engine.
6. Reuse the project's existing UI framework and rendering infrastructure when appropriate.
7. Avoid introducing Electron, Chromium, or a browser runtime.
8. Preserve required GPL-3.0 licensing, copyright notices, third-party attributions, and applicable source-distribution obligations.
9. Use an independently configured application identity and branding where supported. Never overwrite VLC's existing application or silently reuse its installed title identity.
10. Verify PS5 13.40 compatibility against the actual installed jailbreak and runtime prerequisites. Do not assume that a successful compilation guarantees compatibility.

The result should be a standalone, native Plex client optimized specifically for PS5 homebrew.

## 2. Plex account authentication

Implement proper Plex authentication using the currently documented Plex API.

Features:
- Sign in with Plex
- Display a device-link code and browser activation instructions on the television
- Allow authentication using another device, such as an iPhone or PC
- Poll authentication status correctly
- Securely persist credentials between launches
- Automatically reconnect when reopening the app
- Sign out and clear credentials
- Handle expired and revoked tokens gracefully
- Support current officially documented Plex authentication, including JWT/device authentication where applicable

Never ask users to type Plex passwords directly into the PS5 app.

Do not hardcode developer API tokens or credentials.

Do not print authentication secrets to logs.

## 3. Plex server discovery and connection

After signing in, automatically retrieve the user's accessible Plex Media Servers.

Support:
- Multiple Plex servers
- Local network servers
- Remote Plex servers
- Plex resource discovery
- Manual IP address and port configuration
- Automatic reconnection
- Connection status and diagnostics
- Server selection
- Secure HTTPS connections
- Locally configured HTTP connections when explicitly allowed

Test candidate endpoints and select a working connection rather than assuming the first endpoint is reachable.

Include useful troubleshooting messages for authentication failure, network errors, inaccessible servers, and TLS errors.

## 4. Plex-inspired graphical interface

Completely redesign the user-facing experience to resemble a modern Plex television client.

Do not simply add Plex networking into VLC's existing file browser.

Use a dark, cinematic, controller-first interface.

Visual requirements:
- Near-black background
- Dark gray surfaces
- Subtle orange/gold accents
- Large movie and television posters
- Wide media artwork and backdrops
- Smooth horizontal scrolling
- Animated selection highlights
- Readable typography from a couch
- Consistent focus states
- Smooth screen transitions
- Image caching
- Loading placeholders
- Responsive layouts for 1080p and 4K

The design should feel similar in organization and quality to Plex on smart TVs, Apple TV, and Android TV, but use original branding and assets rather than copying proprietary materials.

### Main navigation

Implement:

- Home
- Movies
- TV Shows
- Continue Watching
- Watchlist
- Search
- Settings

Navigation should be intuitive using the DualSense directional pad and analog sticks.

### Home screen

Display the following horizontal artwork carousels when supported by the connected server:

- Continue Watching
- Recently Added Movies
- Recently Added TV Episodes
- On Deck
- Recently Released
- Movies by Genre
- TV Shows by Genre

Allow the user to select an item and open its details page.

Do not manufacture media items to populate empty rows.

### Movies library

Display movies in an attractive poster grid.

Features:
- Alphabetical sorting
- Recently added sorting
- Release year sorting
- Genre filtering
- Watched/unwatched filtering
- Search
- Poster loading
- Metadata and availability

### TV Shows library

Support hierarchical browsing:

TV Shows → Show Details → Seasons → Episodes

Include:
- Series artwork
- Show summaries
- Season lists
- Episode thumbnails
- Episode descriptions
- Runtime
- Watch progress
- Watched indicators
- Next episode functionality

### Media details page

When selecting a movie or show, display:

- Large backdrop
- Poster artwork
- Title
- Synopsis
- Release year
- Runtime
- Content rating
- Genres
- Cast when available
- Playback progress
- Play or Resume button
- Mark Watched/Unwatched
- Add to Watchlist where supported
- More Info
- Playback Version selection when applicable

For television shows, include seasons, episodes, and next-unwatched playback.

## 5. Native VLC video playback integration

The VLC for PS5 playback engine is the foundation of this project.

Preserve as much existing decoding and rendering functionality as possible.

Requirements:
- H.264 support
- HEVC/H.265 support
- 1080p playback
- 4K playback where supported
- MKV and MP4 containers
- Multiple audio streams
- Embedded subtitles
- External subtitles
- Seeking and fast-forward
- Pause/resume
- Playback speed where supported
- Hardware decoding where supported
- Software fallback when appropriate

Implement an adapter between Plex Media Server playback decisions and the VLC playback engine.

### Playback modes

Support, in priority order where compatible:

1. Direct Play
2. Direct Stream
3. Transcoding through Plex Media Server

Do not assume a codec supported by VLC automatically works with every Plex streaming endpoint.

Choose playback methods based on container, codec, stream compatibility, network bandwidth, and server capabilities.

Use Plex's documented media APIs and validate playback URL generation against current server behavior.

### Playback quality settings

Allow:
- Original Quality
- Automatic Quality
- 4K where available
- 1080p
- 720p
- Lower bandwidth options

Separate local and remote streaming quality preferences.

Provide an on-screen indicator showing whether the server is Direct Playing, Direct Streaming, or Transcoding whenever the actual playback decision can be determined.

## 6. Audio synchronization improvements

Audio synchronization is a particularly important requirement.

The official PS5 Plex application has exhibited noticeable audio delay in my setup.

I want PlexStation to provide more control and better synchronization.

Implement:
- Timestamp-based audio/video synchronization
- Accurate audio clock handling
- Consistent A/V sync during seeking
- Synchronization after buffering events
- Synchronization after pause/resume
- Audio output device compatibility
- Stereo PCM support
- Surround output where supported by the VLC PS5 engine

### Adjustable audio delay

Add an in-player Audio Sync menu.

Support:
- Negative and positive audio offsets
- Minimum range of -500 ms to +500 ms
- Adjustments in 25 ms increments
- Reset to zero
- Per-session audio adjustments
- Global default audio offset
- Optional saved offset per media item

Make adjustments during playback without requiring the video to restart.

Clearly indicate which adjustment direction advances or delays audio.

Use the existing VLC audio-delay functionality if appropriate rather than inventing an unnecessary second audio synchronization engine.

Do not claim to have fixed audio synchronization without testing it.

## 7. Video player overlay

When playback begins, display the video fullscreen.

Pressing an appropriate DualSense button should display a modern Plex-style playback overlay.

Include:
- Current title
- Timeline
- Current position
- Total duration
- Play/Pause
- Seek backward
- Seek forward
- Audio Track
- Subtitle Track
- Audio Sync
- Playback Quality
- Playback Information
- Return to Details

For television episodes, include Next Episode where applicable.

Hide playback controls automatically after inactivity.

The overlay should not degrade playback performance.

## 8. Watch progress and Plex synchronization

Integrate playback progress with Plex Media Server.

Requirements:
- Update viewing progress during playback
- Preserve resume positions
- Resume movies from their last saved location
- Resume partially watched episodes
- Mark content watched according to configurable completion thresholds
- Allow manual watched/unwatched changes
- Reflect server watch-state changes
- Keep Continue Watching synchronized
- Stop active playback sessions appropriately

Handle intermittent server connectivity without corrupting playback state.

Do not send duplicate or excessive progress updates.

## 9. Subtitles and audio track selection

Support the audio and subtitle streams exposed by Plex and VLC.

Features:
- Audio language selection
- Subtitle language selection
- Disable subtitles
- Embedded subtitles
- External subtitles where supported
- Preferred language settings
- Subtitle font size
- Subtitle appearance
- Subtitle timing adjustments
- Forced subtitle preference

Preserve existing VLC subtitle capabilities where possible.

## 10. DualSense controller integration

Implement complete controller navigation.

Suggested mapping:

- D-pad: Navigate UI
- Left analog stick: Navigate UI
- Cross: Select
- Circle: Back
- Options: Context menu or playback settings
- L1/R1: Move between appropriate sections
- Left/Right during playback: Seek
- Cross during playback: Pause/Resume
- Circle during playback: Close controls or return

Use platform-consistent controls where appropriate.

Ensure:
- No mouse or keyboard is required
- Every interactive element is reachable
- Focus is always visible
- Focus state is preserved when returning from detail pages
- Long lists scroll smoothly
- Controller input does not repeat uncontrollably

## 11. Performance requirements

The application must remain responsive when handling large Plex libraries.

Implement:
- Asynchronous metadata retrieval
- Lazy artwork loading
- Texture caching
- Image memory limits
- Request timeouts
- Retry behavior
- Pagination
- Background library loading
- Cancellation of obsolete requests
- Proper resource cleanup
- GPU texture release
- Thread-safe UI updates

Avoid loading thousands of full-resolution posters into GPU memory simultaneously.

Avoid blocking the main rendering thread with network calls.

Use efficient artwork sizes appropriate for the UI.

## 12. Settings

Create a native settings interface.

Categories:

### Account
- Plex account status
- Sign out

### Servers
- Select Plex server
- Connection information
- Refresh server list
- Manual server entry

### Playback
- Default quality
- Local streaming quality
- Remote streaming quality
- Direct Play preference
- Preferred audio language
- Preferred subtitle language

### Audio
- Default audio delay
- Stereo/surround options where supported
- Audio output information

### Interface
- Poster grid density
- UI animation preferences
- Home screen sections
- Image cache management

### Developer/Diagnostics
- Application version
- Playback engine version
- Active decoder
- Streaming mode
- Connection status
- Diagnostic logs with secrets redacted

## 13. Application branding and deployment

Name: PlexStation

Create original application branding inspired by a premium media center, without copying official Plex logos or proprietary artwork.

Requirements:
- Native PS5 homebrew application
- Independent application identity
- Application icon
- Startup/loading screen
- Proper title metadata
- Release packaging
- Deployment instructions
- Compatibility notes
- Clean standalone installation

Follow the actual VLC for PS5 build and deployment mechanisms.

Do not assume a standard PS5 retail PKG workflow is necessary if the upstream homebrew uses a different installation format.

Identify whether the app needs kstuff, ShadowMountPlus, etaHEN, or other runtime components. Document the actual dependencies.

Do not overwrite the upstream VLC installation.

## 14. Development workflow

You are responsible for implementing the project.

Do not stop after writing an architecture proposal.

First:
1. Inspect the upstream repository.
2. Document the current architecture.
3. Identify the existing playback and UI components to reuse.
4. Verify third-party licenses.
5. Establish an independent build target.
6. Confirm host-side build capability.

Then implement the application in coherent, testable phases.

Use these milestones:

**Milestone 1: Foundation**
- Independent PlexStation project
- Existing VLC playback preserved
- Basic PlexStation interface
- Successful host build
- PS5 build attempted and verified where the toolchain is available

**Milestone 2: Plex connectivity**
- Device authentication
- Server discovery
- Library browsing
- Metadata loading
- Posters and backdrops

**Milestone 3: Media playback**
- Play movie from Plex server
- Play TV episode
- Seek and pause
- Resume tracking
- Audio/subtitle selection
- Playback quality selection

**Milestone 4: Full television UI**
- Complete home screen
- Poster carousels
- Movie libraries
- TV show hierarchy
- Details pages
- Search
- Watchlist where available
- Settings

**Milestone 5: Polish and validation**
- Audio synchronization controls
- Playback state synchronization
- Performance optimization
- Error recovery
- Controller validation
- Packaging
- Documentation

Implement actual code at each milestone.

If a component cannot be completed because a dependency, SDK, or device is unavailable, clearly identify the blocker, finish independently testable work, and provide exact reproduction and testing instructions.

## 15. Testing and acceptance criteria

Create host-side tests where possible.

Use mocked Plex responses to test:
- Authentication state handling
- Server connection selection
- Library parsing
- Movie/episode navigation
- Playback URL and decision handling
- Watch state updates
- Token expiration
- Missing artwork
- Server failures

Do not include fabricated passing test results.

Document the actual results of tests and builds.

The application is only considered functionally complete when these workflows work:

1. Launch PlexStation on the PS5.
2. Sign into a Plex account.
3. Select a reachable Plex server.
4. View the server's actual media libraries.
5. Browse movies using artwork.
6. Select a movie and play it.
7. Pause, resume, and seek.
8. Change audio/subtitle tracks when available.
9. Adjust audio delay during playback.
10. Stop playback and return to the details screen.
11. Resume from the saved position.
12. Navigate television shows and play episodes.
13. Exit and relaunch without losing valid authentication.

Tests that require my physical PS5 should be marked as awaiting device verification rather than reported as passing.

## 16. Deliverables

Produce:
- Complete PlexStation source code
- Build scripts
- Dependencies documentation
- Original branding assets
- PS5 build output, if the toolchain supports it
- Deployment instructions
- Configuration instructions
- Troubleshooting guide
- Testing checklist
- License and attribution files
- Clear summary of working versus incomplete functionality

Use the actual upstream directory structure and current build commands rather than guessing them.

Keep the source maintainable for future improvements.

## Important instructions

- Prioritize functional video playback over cosmetic features.
- Do not rewrite stable VLC playback functionality unnecessarily.
- Do not remove useful hardware decoding support.
- Do not silently replace native playback with an embedded browser.
- Do not hardcode Plex server addresses or credentials.
- Do not implement fake content or fake playback behavior.
- Do not claim compatibility that has not been tested.
- Preserve licenses and attributions.
- Provide meaningful error messages instead of silently failing.
- Do not modify unrelated projects in the workspace.
- Continue implementing instead of stopping at a plan.
- When blocked, explain the limitation and proceed with the next independently testable task.

**Begin by inspecting the actual upstream VLC PS5 source code and determining the smallest reliable path to a working PlexStation application. Then start implementing it.**