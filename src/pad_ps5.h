// SPDX-License-Identifier: GPL-2.0-or-later
// Included by platform_ps5.c and the host pad-backend harness.
typedef struct {
    uint32_t buttons;
    uint8_t lx, ly, rx, ry, l2, r2;
    uint8_t reserved_to_connected[66];
    int32_t connected;
    uint64_t timestamp_us;
    uint8_t extension[16];
    uint8_t connected_count;
    uint8_t remaining[15];
} PadSample;
#ifdef __cplusplus
#define PAD_ASSERT static_assert
#else
#define PAD_ASSERT _Static_assert
#endif
PAD_ASSERT(sizeof(PadSample) == 120, "the console's pad samples are 120 bytes");
PAD_ASSERT(offsetof(PadSample, connected) == 0x4c, "connected at 0x4c");
PAD_ASSERT(offsetof(PadSample, timestamp_us) == 0x50, "timestamp at 0x50");
#undef PAD_ASSERT
#define PAD_INTERCEPTED 0x80000000u

static int32_t pad_handle = -1, pad_user = -1;
static double pad_next_open;
static bool pad_input_private;
void plat_pad_set_private(bool enabled) {pad_input_private=enabled;}

static float stick_axis(uint8_t v)
{
    return (v - 127.5f) / 127.5f;
}

static void stick(uint8_t bx, uint8_t by, float *x, float *y)
{
    float fx = stick_axis(bx), fy = stick_axis(by);
    float len = sqrtf(fx * fx + fy * fy);
    const float dead = 0.18f;
    if (len < dead) {
        *x = *y = 0;
        return;
    }
    float scale = (len > 1 ? 1 : (len - dead) / (1 - dead)) / len;
    *x = fx * scale;
    *y = fy * scale;
}

/* The console doesn't give titles scePadSetVibration (run 10: unresolved);
 * no rumble until another way is found. */
void plat_pad_rumble(float strength, int ms)
{
    (void)strength;
    (void)ms;
}

bool plat_pad_read(PadState *state)
{
    static PadSample last;
    static bool have_last;
    memset(state, 0, sizeof(*state));
    double now = plat_time();
    static double last_sample_at, disconnected_at, log_at;
    static unsigned polls, deliveries, events;
    static uint32_t last_buttons;
    if (pad_handle < 0 && now >= pad_next_open) {
        pad_next_open = now + 1.0;
        // Retrying also handles a service that was unavailable during launch.
        int user_rc = sceUserServiceInitialize(NULL), init_rc = scePadInit();
        int32_t users[6] = {-1, -1, -1, -1, -1, -1};
        int foreground_rc = sceUserServiceGetForegroundUser(&users[0]);
        int initial_rc = sceUserServiceGetInitialUser(&users[1]);
        int list_rc = sceUserServiceGetLoginUserIdList(&users[2]);
        if (foreground_rc < 0) users[0] = -1;
        if (initial_rc < 0) users[1] = -1;
        if (list_rc < 0) for (int i = 2; i < 6; ++i) users[i] = -1;
        fprintf(stderr, "pad:init user=%#x pad=%#x foreground=%#x initial=%#x list=%#x\n",
                user_rc, init_rc, foreground_rc, initial_rc, list_rc);
        for (int i = 0; i < 6 && pad_handle < 0; ++i) {
            if (users[i] < 0 || (i == 0 && foreground_rc < 0) ||
                (i == 1 && initial_rc < 0) || (i >= 2 && list_rc < 0))
                continue;
            bool duplicate = false;
            for (int j = 0; j < i; ++j)
                if (users[i] == users[j]) duplicate = true;
            if (duplicate) continue;
            // Same standard port (0) as upstream VLC/PS5_vkQuake. A launcher
            // may already have opened the port in this process: reuse it.
            int existing = scePadGetHandle(users[i], 0, 0);
            pad_handle = existing >= 0 ? existing : scePadOpen(users[i], 0, 0, NULL);
            fprintf(stderr, "pad:open user=%d existing=%#x result=%#x\n",
                    users[i], existing, pad_handle);
            if (pad_handle >= 0) pad_user = users[i];
        }
        have_last = false;
        disconnected_at = now;
    }
    if (pad_handle < 0)
        return false;
    PadSample samples[64];
    int n = scePadRead(pad_handle, samples, 64);
    ++polls;
    if (n < 0 || n > 64) {
        fprintf(stderr, "pad:read failure result=%#x handle=%d\n", n, pad_handle);
        have_last = false;
        if (n < 0) {
            scePadClose(pad_handle);
            pad_handle = -1;
        }
        return false;
    }
    const PadSample *best = NULL;
    for (int i = 0; i < n; i++)
        if (!best || samples[i].timestamp_us >= best->timestamp_us)
            best = &samples[i];
    if (best) {
        last = *best;
        have_last = true;
        last_sample_at = now;
    }
    /* The shell has the pad (home screen, a system dialog): nothing is held. */
    bool stale = !have_last || now - last_sample_at > 0.5;
    bool connected = !stale && last.connected;
    bool intercepted = have_last && (last.buttons & PAD_INTERCEPTED);
    if (!pad_input_private && now >= log_at) {
        fprintf(stderr, "pad:poll count=%u samples=%d connected=%d intercepted=%d stale=%d raw=%08x delivered=%u changes=%u\n",
                polls, n, connected, intercepted, stale, have_last ? last.buttons : 0,
                deliveries, events);
        log_at = now + 5;
    }
    if (!connected) {
        if (now - disconnected_at > 2) {
            scePadClose(pad_handle);
            pad_handle = -1;
            pad_user = -1;
            have_last = false;
        }
        return false;
    }
    disconnected_at = now;
    // Never force ownership away from the shell or a system dialog.
    if (intercepted) return false;
    ++deliveries;
    if (last.buttons != last_buttons) { ++events; last_buttons = last.buttons; }
    state->connected = true;
    state->buttons = last.buttons & 0x00ffffffu;
    stick(last.lx, last.ly, &state->lx, &state->ly);
    stick(last.rx, last.ry, &state->rx, &state->ry);
    state->l2 = last.l2 / 255.0f;
    state->r2 = last.r2 / 255.0f;
    /* Touchpad (ScePadData's touch data at 0x34: count, then 8-byte points of
     * x, y at 0x3c; the pad is 1920x1080). */
    const uint8_t *raw = (const uint8_t *)&last;
    state->touches = raw[0x34] > 2 ? 2 : raw[0x34];
    if (state->touches) {
        uint16_t x, y;
        memcpy(&x, raw + 0x3c, 2);
        memcpy(&y, raw + 0x3e, 2);
        state->tx = x / 1919.0f;
        state->ty = y / 1079.0f;
    }
    if (state->l2 > 0.5f)
        state->buttons |= PAD_L2;
    if (state->r2 > 0.5f)
        state->buttons |= PAD_R2;
    return true;
}

