// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform.h"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <cmath>

// Upstream's direct draw-list UI owns focus. ImGui events are also forwarded
// for widgets, but ImGui NavActive is not the focus source for drawn rows.
class ControllerInput {
    uint32_t held = 0;
    std::array<float, 32> repeat{};
public:
    uint32_t update(const PadState &pad, float dt) {
        uint32_t buttons = pad.connected ? pad.buttons : 0;
        if (pad.connected) {
            if (pad.lx > .60f) buttons |= PAD_RIGHT;
            if (pad.lx < -.60f) buttons |= PAD_LEFT;
            if (pad.ly > .60f) buttons |= PAD_DOWN;
            if (pad.ly < -.60f) buttons |= PAD_UP;
        }
        uint32_t hit = buttons & ~held;
        float elapsed = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        for (unsigned i = 0; i < 32; ++i) {
            uint32_t bit = 1u << i;
            if (!(buttons & bit)) repeat[i] = .38f;
            else if (!(held & bit)) repeat[i] = .38f;
            else if (bit & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT | PAD_L2 | PAD_R2)) {
                repeat[i] -= elapsed;
                if (repeat[i] <= 0) { hit |= bit; repeat[i] += .09f; }
            }
        }
        held = buttons;
        return hit;
    }
};
inline void controller_imgui(const PadState &pad) {
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    if (pad.connected) io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    else io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    const struct { uint32_t button; ImGuiKey key; } keys[] = {
        {PAD_UP, ImGuiKey_GamepadDpadUp}, {PAD_DOWN, ImGuiKey_GamepadDpadDown},
        {PAD_LEFT, ImGuiKey_GamepadDpadLeft}, {PAD_RIGHT, ImGuiKey_GamepadDpadRight},
        {PAD_CROSS, ImGuiKey_GamepadFaceDown}, {PAD_CIRCLE, ImGuiKey_GamepadFaceRight},
        {PAD_SQUARE, ImGuiKey_GamepadFaceLeft}, {PAD_TRIANGLE, ImGuiKey_GamepadFaceUp},
        {PAD_L1, ImGuiKey_GamepadL1}, {PAD_R1, ImGuiKey_GamepadR1},
        {PAD_OPTIONS, ImGuiKey_GamepadStart}
    };
    for (auto k : keys) io.AddKeyEvent(k.key, pad.connected && (pad.buttons & k.button));
    auto axis = [&](ImGuiKey key, float value) {
        float v = pad.connected ? std::clamp((value - .25f) / .75f, 0.f, 1.f) : 0.f;
        io.AddKeyAnalogEvent(key, v > 0, v);
    };
    axis(ImGuiKey_GamepadLStickLeft, -pad.lx); axis(ImGuiKey_GamepadLStickRight, pad.lx);
    axis(ImGuiKey_GamepadLStickUp, -pad.ly); axis(ImGuiKey_GamepadLStickDown, pad.ly);
}
