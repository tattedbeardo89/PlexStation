/*
 * VLC-PS5's interface: the library (Home, Videos, Music, Browse) and the
 * player's on-screen display, drawn with Dear ImGui's draw lists and driven
 * by the DualSense.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "platform.h"

bool ui_init();
/* Input, state and this frame's drawing (between ImGui::NewFrame and Render). */
void ui_frame(const PadState &pad, float dt);
/* Where the video goes this frame, in pixels; false: no video. */
bool ui_video_rect(float *x, float *y, float *w, float *h);
bool ui_quit_requested();

/* Private PIN entry: main/backend logs must not expose its button/focus sequence. */
bool ui_sensitive_input();

/* Release UI textures/workers while the renderer/device are still alive. */
void ui_shutdown();

/* After a frame is submitted, for Home diagnostics only. */
void ui_frame_presented();
