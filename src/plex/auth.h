// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "client.h"
#include <algorithm>
#include <cmath>
namespace plex {
enum class AuthState { Idle, Generating, Waiting, Authorized, Expired, Cancelled, Failed };
// Monotonic deadlines; no network or storage on the render thread here.
struct AuthSession {
    AuthState state = AuthState::Idle;
    Pin current;
    std::string error;
    uint64_t generation = 0;
    double started = 0, deadline = 0, next_poll = 0;
    void begin(double now) {
        ++generation; current = {}; error.clear(); started = now;
        state = AuthState::Generating;
    }
    void cancel() {
        ++generation; current = {}; error.clear(); state = AuthState::Cancelled;
    }
    void fail(const std::string &reason, bool expired = false) {
        current = {}; error = reason;
        state = expired ? AuthState::Expired : AuthState::Failed;
    }
    static bool valid_code(const std::string &code) {
        return !code.empty() && code.size() <= 256 &&
            std::all_of(code.begin(), code.end(), [](unsigned char c) {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
            });
    }
    bool short_code() const { return current.code.size() == 4 && valid_code(current.code); }
    int remaining(double now) const { return std::max(0, static_cast<int>(std::ceil(deadline-now))); }
    void tick(double now) {
        if (state == AuthState::Waiting && now >= deadline)
            fail("Activation code expired. Press Cross for a new code.", true);
    }
    bool poll_due(double now) { tick(now); return state == AuthState::Waiting && now >= next_poll; }
    void created(const Result &result, double now) {
        if (state != AuthState::Generating) return;
        if (!result) { fail(result.error); return; }
        auto p = pin(result.data);
        auto token = result.data.find("authToken");
        if ((token != result.data.end() && !token->is_null() && !token->is_string()) ||
            !result.data.is_object() || p.id <= 0 || !valid_code(p.code) ||
            p.expires <= 0 || p.expires > 604800 || !p.token.empty()) {
            fail("Plex returned an invalid activation code. Press Cross to retry."); return;
        }
        current = std::move(p);
        deadline = started + current.expires; next_poll = now + 2;
        state = AuthState::Waiting; tick(now);
    }
    void polled(const Result &result, double now) {
        tick(now);
        if (state != AuthState::Waiting) return; // Includes late authorization after expiry.
        if (!result) {
            bool expired = result.status == 404 || result.status == 410;
            fail(expired ? "Activation code expired or is no longer valid. Press Cross to retry." : result.error, expired);
            return;
        }
        auto p = pin(result.data);
        auto token = result.data.find("authToken");
        if (!result.data.is_object() || p.id != current.id || p.code != current.code ||
            token == result.data.end() || (!token->is_null() && !token->is_string()) ||
            (result.data.contains("expiresIn") && p.expires < 0)) {
            fail("Plex returned an invalid authorization response. Press Cross to retry."); return;
        }
        if (p.expires >= 0) deadline = std::min(deadline, now + static_cast<double>(p.expires));
        tick(now);
        if (state != AuthState::Waiting) return;
        if (!p.token.empty()) {
            if (p.token.size() > 4096 || p.token.find_first_of("\r\n\t ") != std::string::npos) {
                fail("Plex returned an invalid account response."); return;
            }
            current.token = std::move(p.token); state = AuthState::Authorized;
        } else next_poll = now + 2;
    }
};
} // namespace plex
