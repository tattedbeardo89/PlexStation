// SPDX-License-Identifier: GPL-3.0-or-later
// PlexStation controller-first interface; original VLC playback remains in player.cc.
#include "gfx.h"
#include "imgui.h"
#include "player.h"
#include "plex/artwork.h"
#include "plex/client.h"
#include "prefs.h"
#include "ui.h"
#include "controller.h"
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <memory>
#include <unistd.h>
#ifdef VLCPS5_HOST
#define PLEX_TRANSPORT plex::curl_transport
#else
namespace plex {
Transport ps5_transport();
}
#define PLEX_TRANSPORT plex::ps5_transport
#endif
namespace {
using namespace plex;
std::unique_ptr<Client> client;
std::unique_ptr<Artwork> artwork;
std::string device, account, credential_file, base, server_token,
    message = "Sign in to your Plex account";
std::vector<Server> available;
std::vector<Item> listing;
struct Page {
    std::string path, title;
    std::vector<Item> items;
    int focus = 0, start = 0, total = 0;
};
std::vector<Page> history;
std::string page_path, page_title = "Home";
Item detail, playing;
Pin active_pin;
double expires_at = 0, poll_at = 0, overlay_until = 0;
int focus = 0, nav = 0, track_focus = 0, global_delay = 0, page_start = 0, page_total = 0,
    version = 0;
int editor = 0, edit_focus = 0;
std::string edit_text;
const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .:-_/";
bool sidebar = true, in_detail = false, settings = false, tracks = false, video = false,
     allow_http = false;
ControllerInput controller;
std::future<Result> pending;
enum Job {
    None,
    PinStart,
    PinPoll,
    Resources,
    Select,
    Manual,
    Sections,
    Browse,
    Details,
    Timeline,
    Watch
};
Job job = None;
std::string next_base, request_path;

std::string progress_state;
int64_t progress_time = 0;
bool progress_pending = false, cancel_pin = false;
Progress progress;
const char *navigation[] = {"Home",      "Movies", "TV Shows", "Continue Watching",
                            "Watchlist", "Search", "Settings"};
void submit(Job kind, std::function<Result()> work) {
    if (job != None)
        return;
    job = kind;
    pending = std::async(std::launch::async, std::move(work));
}
void load(const std::string &path, const std::string &title, bool push = true, int start = 0) {
    if (job != None)
        return;
    if (push)
        history.push_back({page_path, page_title, listing, focus, page_start, page_total});
    request_path = path;
    page_title = title;
    page_path = path;
    focus = 0;
    page_start = start;
    auto b = base, t = server_token;
    submit(Browse, [b, t, path, start] { return client->browse(b, t, path, start); });
}
void refresh() {
    auto token = account;
    submit(Resources, [token] { return client->resources(token); });
}
void signout() {
    if (job != None)
        return;
    if (video) {
        player_stop();
        video = false;
    }
    artwork->clear();
    account.clear();
    server_token.clear();
    base.clear();
    available.clear();
    listing.clear();
    history.clear();
    active_pin = {};
    in_detail = false;
    settings = false;
    focus = 0;
    sidebar = true;
    if (!save_credentials(credential_file, device, ""))
        message = "Sign-out persistence failed; check storage permissions";
    else
        message = "Signed out. Cross to sign in";
}
void queue_progress(const std::string &state, int64_t position) {
    progress_state = state;
    progress_time = position;
    progress_pending = true;
}
void finish_job() {
    if (job == None || pending.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        return;
    Result result = pending.get();
    Job done = job;
    job = None;
    if (cancel_pin && (done == PinStart || done == PinPoll)) {
        cancel_pin = false;
        return;
    }
    if (!result) {
        message = result.error;
        if (done == PinPoll)
            active_pin = {};
        if (done == Timeline) {
            progress_pending = false;
            message = "Progress could not be saved; playback remains available";
        }
        return;
    }
    message.clear();
    if (done == PinStart) {
        active_pin = pin(result.data);
        if (!active_pin.id || active_pin.code.empty() || active_pin.expires <= 0) {
            active_pin = {};
            message = "Invalid device-link response";
            return;
        }
        expires_at = plat_time() + active_pin.expires;
        poll_at = plat_time() + 2;
    } else if (done == PinPoll) {
        auto p = pin(result.data);
        if (!p.token.empty()) {
            account = p.token;
            active_pin = {};
            if (!save_credentials(credential_file, device, account))
                message = "Signed in, but credentials could not be saved";
            refresh();
        } else
            poll_at = plat_time() + 2;
    } else if (done == Resources) {
        available = servers(result.data);
        listing.clear();
        focus = 0;
        sidebar = false;
        page_title = "Select server";
        settings = false;
        in_detail = false;
        if (available.empty())
            message = "No accessible Plex Media Servers";
    } else if (done == Select || done == Manual) {
        base = next_base;
        available.clear();
        settings = false;
        sidebar = true;
        auto b = base, t = server_token;
        submit(Sections, [b, t] { return client->sections(b, t); });
    } else if (done == Sections || done == Browse) {
        listing = items(result.data);
        in_detail = false;
        auto c = result.data.find("MediaContainer");
        page_total = c != result.data.end() ? number(*c, "totalSize", listing.size()) : 0;
        if (listing.empty())
            message = "No items in this section";
        if (done == Sections) {
            if (nav == 1 || nav == 2) {
                const std::string type = nav == 1 ? "movie" : "show";
                listing.erase(std::remove_if(listing.begin(), listing.end(),
                                             [&](const Item &i) { return i.type != type; }),
                              listing.end());
            }
            page_title = "Libraries";
            page_path = "/library/sections";
        }
    } else if (done == Watch) {
        detail.watched = !detail.watched;
    } else if (done == Details) {
        auto parsed = items(result.data);
        if (parsed.empty()) {
            message = "Metadata is unavailable";
            return;
        }
        detail = parsed.front();
        in_detail = true;
        focus = 0;
        version = 0;
    }
}
void open_item() {
    if (job != None || listing.empty())
        return;
    Item item = listing[focus];
    if (page_path == "/library/sections")
        load("/library/sections/" + encode(item.id) + "/all", item.title);
    else if (item.type == "show" || item.type == "season")
        load("/library/metadata/" + encode(item.id) + "/children", item.title);
    else if (item.type == "movie" || item.type == "episode") {
        auto b = base, t = server_token, path = "/library/metadata/" + encode(item.id);
        submit(Details, [b, t, path] { return client->call("GET", b, path, t); });
    } else
        load("/library/sections/" + encode(item.id) + "/all", item.title);
}
void play() {
    if (progress_pending) {
        message = "Saving previous playback state; try again shortly";
        return;
    }
    auto decision = direct_play(base, server_token, detail, version);
    if (decision.mode != Mode::DirectPlay) {
        message = decision.error;
        return;
    }
    if (!player_open(decision.url, detail.offset)) {
        message = "VLC could not open the selected media";
        return;
    }
    playing = detail;
    video = true;
    tracks = false;
    progress = {};
    player_set_audio_delay(global_delay);
    overlay_until = plat_time() + 5;
}
void stop() {
    auto position = player_time();
    queue_progress("stopped", position);
    detail.offset = position;
    player_stop();
    video = false;
    tracks = false;
    in_detail = true;
    sidebar = false;
    focus = 0; // Resume/Play owns focus after leaving playback.
}
void draw_text(float x, float y, float size, const std::string &value,
               ImU32 color = IM_COL32(236, 236, 239, 255)) {
    float scale = gfx.height / 1080.f;
    ImGui::GetBackgroundDrawList()->AddText(ImGui::GetFont(), size * scale, {x * scale, y * scale},
                                            color, value.c_str());
}
void rect(float x, float y, float w, float h, ImU32 color) {
    float s = gfx.height / 1080.f;
    ImGui::GetBackgroundDrawList()->AddRectFilled({x * s, y * s}, {(x + w) * s, (y + h) * s}, color,
                                                  12 * s);
}
void row(int index, int selected, float y, const std::string &label, bool enabled = true) {
    rect(390, y, 1440, 70,
         index == selected && enabled ? IM_COL32(168, 108, 16, 255) : IM_COL32(27, 28, 33, 255));
    draw_text(420, y + 16, 29, label);
}
// Runs even on early returns (sign-in, keyboard, playback). Custom focus is
// deliberately reported separately from ImGui's widget navigation focus.
struct InputDiagnostic {
    const PadState &pad;
    uint32_t hit;
    ~InputDiagnostic() {
        const char *screen = video ? (tracks ? "tracks" : "playback") :
            editor ? "keyboard" : account.empty() ? "sign-in" : sidebar ? "sidebar" :
            settings ? "settings" : in_detail ? "detail" : !available.empty() ? "servers" : "library";
        int selected = video ? track_focus : editor ? edit_focus : sidebar ? nav : focus;
        bool valid = selected >= 0 && (sidebar ? nav < 7 : settings ? focus < 6 :
            in_detail ? focus < 3 : editor ? edit_focus < static_cast<int>(alphabet.size()) :
            account.empty() || video || (available.empty() ? focus < static_cast<int>(listing.size()) :
            focus < static_cast<int>(available.size())));
        static double log_at, event_log_at;
        static uint32_t received_since_log;
        received_since_log |= pad.buttons;
        double now = plat_time();
        auto &io = ImGui::GetIO();
        if (now >= log_at || (hit && now >= event_log_at)) {
            fprintf(stderr, "input:ui connected=%d received=%08x actions=%08x screen=%s focus=%d valid=%d busy=%d imgui_gamepad=%d imgui_nav=%d\n",
                    pad.connected, received_since_log, hit, screen, selected, valid, job != None,
                    !!(io.BackendFlags & ImGuiBackendFlags_HasGamepad), io.NavActive);
            received_since_log = 0;
            log_at = now + 5;
            event_log_at = now + .25;
        }
        char line[240];
        snprintf(line, sizeof line, "Pad:%s raw:%08x action:%08x  %s focus:%d %s  ImGui:%s",
                 pad.connected ? "OK" : "NO INPUT", pad.buttons, hit, screen, selected,
                 valid ? "OK" : "EMPTY/INVALID", io.NavActive ? "active" : "draw-list focus");
        float scale = gfx.height / 1080.f;
        auto *draw = ImGui::GetForegroundDrawList();
        draw->AddRectFilled({380*scale, 5*scale}, {1900*scale, 39*scale}, IM_COL32(0,0,0,210));
        draw->AddText(ImGui::GetFont(), 19*scale, {390*scale, 10*scale}, IM_COL32(235,220,150,255), line);
    }
};
} // namespace
bool ui_init() {
    credential_file = std::string(plat_data_dir()) + "/plexstation-account.json";
    if (!load_credentials(credential_file, device, account)) {
        if (access(credential_file.c_str(), F_OK) == 0) {
            message = "Existing account file is unreadable or insecure";
            return false;
        }
        unsigned char random[16];
        std::ifstream source("/dev/urandom", std::ios::binary);
        if (!source.read(reinterpret_cast<char *>(random), sizeof random)) {
            message = "Secure device identity generation failed";
            return false;
        }
        static const char *hex = "0123456789abcdef";
        for (auto b : random) {
            device += hex[b >> 4];
            device += hex[b & 15];
        }
        if (!save_credentials(credential_file, device, "")) {
            message = "Cannot save device identity";
            return false;
        }
    }
    client = std::make_unique<Client>(PLEX_TRANSPORT(), device);
    artwork = std::make_unique<Artwork>(PLEX_TRANSPORT());
    global_delay = std::clamp(pref_int("plex_audio_delay", 0), -500, 500);
    allow_http = pref_int("plex_allow_http", 0) != 0;
    auto &io = ImGui::GetIO();
    auto font = std::string(plat_data_dir()) + "/fonts/Inter-Regular.ttf";
    if (access(font.c_str(), R_OK) == 0)
        io.Fonts->AddFontFromFileTTF(font.c_str(), 28);
    else
        io.Fonts->AddFontDefault();
    if (!account.empty())
        refresh();
    return true;
}
void ui_frame(const PadState &pad, float dt) {
    player_tick();
    finish_job();
    uint32_t hit = controller.update(pad, dt);
    InputDiagnostic diagnostic{pad, hit};
    double now = plat_time();
    if (active_pin.id && job == None) {
        if (now >= expires_at) {
            active_pin = {};
            message = "Device link expired. Press Cross to retry";
        } else if (now >= poll_at) {
            auto p = active_pin;
            submit(PinPoll, [p] { return client->poll_pin(p.id, p.code); });
        }
    }
    bool was_video = video;
    if (video) {
        if (hit)
            overlay_until = now + 5;
        if ((hit & PAD_CROSS) && !tracks)
            player_toggle_pause();
        if ((hit & PAD_LEFT) && !tracks)
            player_seek(std::max<int64_t>(0, player_time() - 10000));
        if ((hit & PAD_RIGHT) && !tracks)
            player_seek(player_time() + 10000);
        if (hit & PAD_CIRCLE) {
            if (tracks) tracks = false;
            else stop();
        }
        if (hit & PAD_OPTIONS)
            tracks = !tracks;
        if (hit & PAD_L1)
            player_set_audio_delay(audio_offset(player_audio_delay(), -1));
        if (hit & PAD_R1)
            player_set_audio_delay(audio_offset(player_audio_delay(), 1));
        if (hit & PAD_TRIANGLE)
            player_set_audio_delay(0);
        if (tracks) {
            auto audio = player_audio_tracks(), subs = player_subtitle_tracks();
            if (hit & PAD_UP)
                track_focus = std::max(0, track_focus - 1);
            if (hit & PAD_DOWN)
                track_focus = std::min(1, track_focus + 1);
            if (hit & (PAD_CROSS | PAD_SQUARE)) {
                auto cycle = [](const std::vector<Track> &list, int id) {
                    for (size_t i = 0; i < list.size(); ++i)
                        if (list[i].id == id)
                            return list[(i + 1) % list.size()].id;
                    return list.empty() ? -1 : list.front().id;
                };
                if (track_focus == 0)
                    player_set_audio_track(cycle(audio, player_audio_track()));
                else
                    player_set_subtitle_track(cycle(subs, player_subtitle_track()));
            }
            draw_text(100, 120, 32,
                      track_focus == 0 ? "> Audio track (Cross to cycle)"
                                       : "> Subtitle track (Cross to cycle)");
            std::string name = "None";
            auto list = track_focus == 0 ? audio : subs;
            int id = track_focus == 0 ? player_audio_track() : player_subtitle_track();
            for (auto &t : list)
                if (t.id == id)
                    name = t.name;
            draw_text(100, 170, 28, name);
        }
        if (video && player_ended()) {
            if (player_failed())
                message = "Playback failed; check decoder and server connectivity";
            stop();
        }
        if (video &&
            progress.due(static_cast<int64_t>(now * 1000), player_paused() ? "paused" : "playing"))
            queue_progress(player_paused() ? "paused" : "playing", player_time());
        if (video && (now < overlay_until || tracks)) {
            rect(70, 780, 1780, 230, IM_COL32(10, 10, 14, 230));
            draw_text(100, 800, 36, playing.title);
            draw_text(100, 855, 27,
                      std::to_string(player_time() / 1000) + " / " +
                          std::to_string(player_length() / 1000) +
                          " s   Original / Direct Play requested");
            draw_text(100, 900, 26, "Cross Pause | Left/Right Seek | Options Tracks | Circle Stop");
            draw_text(100, 945, 25,
                      "L1/R1 Audio sync: " + std::to_string(player_audio_delay()) +
                          " ms (+ delays audio); Triangle resets");
        }
    }
    if (progress_pending && job == None) {
        auto b = base, t = server_token, state = progress_state;
        auto item = playing;
        auto pos = progress_time;
        progress_pending = false;
        submit(Timeline,
               [b, t, state, item, pos] { return client->timeline(b, t, item, state, pos); });
    }
    if (video)
        return;
    if (was_video) hit = 0; // The stop/back press must not also leave details.
    artwork->update();
    rect(0, 0, 1920, 1080, IM_COL32(12, 13, 17, 255));
    rect(0, 0, 330, 1080, IM_COL32(20, 21, 26, 255));
    draw_text(45, 55, 38, "PlexStation", IM_COL32(235, 171, 54, 255));
    for (int i = 0; i < 7; ++i) {
        if (sidebar && nav == i)
            rect(25, 150 + i * 87, 280, 65, IM_COL32(120, 78, 16, 255));
        draw_text(45, 166 + i * 87, 28, navigation[i]);
    }
    draw_text(390, 65, 46, settings ? "Settings" : in_detail ? detail.title : page_title);
    if (job != None)
        draw_text(390, 135, 26, "Loading…");
    if (!message.empty())
        draw_text(390, 950, 25, message, IM_COL32(240, 185, 90, 255));
    if (account.empty()) {
        draw_text(390, 240, 38, "Your media, on your television.");
        if (active_pin.id) {
            draw_text(390, 340, 34, "Link code: " + active_pin.code);
            draw_text(390, 420, 23, "Open this address on your phone or PC:");
            draw_text(390, 470, 18,
                      "https://plex.tv/auth#?clientID=" + encode(device) +
                          "&code=" + encode(active_pin.code));
            draw_text(390, 530, 26,
                      "Waiting for authorization… " +
                          std::to_string(static_cast<int>(expires_at - now)) +
                          " seconds remaining");
        } else {
            row(0, 0, 340, "Sign in with Plex (Cross)");
            if (hit & PAD_CROSS)
                submit(PinStart, [] { return client->create_pin(); });
        }
        if (hit & PAD_CIRCLE) {
            active_pin = {}; // In-flight PIN completion is discarded below.
            if (job == PinStart || job == PinPoll) cancel_pin = true;
            message = "Device linking cancelled. Cross to retry";
        }
        return;
    }
    if (editor) {
        draw_text(390, 180, 30,
                  editor == 1 ? "Search your server"
                              : "Manual server: scheme://host:port (no trailing slash)");
        draw_text(390, 245, 28, edit_text);
        if (hit & PAD_LEFT)
            edit_focus = std::max(0, edit_focus - 1);
        if (hit & PAD_RIGHT)
            edit_focus = std::min(static_cast<int>(alphabet.size()) - 1, edit_focus + 1);
        if (hit & PAD_UP)
            edit_focus = std::max(0, edit_focus - 10);
        if (hit & PAD_DOWN)
            edit_focus = std::min(static_cast<int>(alphabet.size()) - 1, edit_focus + 10);
        if ((hit & PAD_CROSS) && edit_text.size() < 200)
            edit_text += alphabet[edit_focus];
        if ((hit & PAD_SQUARE) && !edit_text.empty())
            edit_text.pop_back();
        for (int i = 0; i < static_cast<int>(alphabet.size()); ++i) {
            float x = 390 + (i % 10) * 100, y = 330 + (i / 10) * 70;
            rect(x, y, 80, 55,
                 i == edit_focus ? IM_COL32(170, 115, 25, 255) : IM_COL32(35, 37, 43, 255));
            draw_text(x + 25, y + 10, 28, std::string(1, alphabet[i]));
        }
        draw_text(390, 870, 24, "Cross: type • Square: erase • Triangle: submit • Circle: cancel");
        if (hit & PAD_CIRCLE)
            editor = 0;
        if ((hit & PAD_TRIANGLE) && job == None) {
            if (editor == 1) {
                load("/search?query=" + encode(edit_text), "Search");
                editor = 0;
            } else if (valid_base(edit_text, allow_http)) {
                next_base = edit_text;
                server_token = account;
                auto b = next_base, t = server_token;
                submit(Manual, [b, t] { return client->call("GET", b, "/identity", t); });
                editor = 0;
            } else
                message = "Use HTTPS, or explicitly enable HTTP in Settings";
        }
        return;
    }
    bool grid = !listing.empty() && page_path != "/library/sections" && !settings && !in_detail &&
                available.empty();
    if (hit & PAD_LEFT) {
        if (!sidebar && grid && focus % 5 > 0)
            --focus;
        else
            sidebar = true;
    }
    if (hit & PAD_RIGHT) {
        if (!sidebar && grid)
            focus = std::min(static_cast<int>(listing.size()) - 1, focus + 1);
        else
            sidebar = false;
    }
    if (hit & PAD_OPTIONS) {
        settings = !settings;
        sidebar = false;
        focus = 0;
    }
    if (hit & (PAD_L1 | PAD_R1)) {
        nav = (nav + ((hit & PAD_R1) ? 1 : 6)) % 7;
        sidebar = true;
        hit |= PAD_CROSS; // Same section activation as the sidebar.
    }
    if (hit & PAD_CIRCLE) {
        if (settings) { settings = false; sidebar = true; focus = 0; }
        else if (in_detail) { in_detail = false; sidebar = false; }
        else if (!history.empty() && job == None) {
            auto p = history.back(); history.pop_back();
            page_title = p.title; page_path = p.path; listing = std::move(p.items);
            focus = p.focus; page_start = p.start; page_total = p.total; sidebar = false;
        } else sidebar = true;
        hit &= ~PAD_CIRCLE; // One back action per frame.
    }
    if (sidebar) {
        if (hit & PAD_UP)
            nav = std::max(0, nav - 1);
        if (hit & PAD_DOWN)
            nav = std::min(6, nav + 1);
        if ((hit & PAD_CROSS) && job == None) {
            settings = nav == 6;
            in_detail = false;
            sidebar = false;
            focus = 0;
            if (nav == 0) {
                auto b = base, t = server_token;
                submit(Sections, [b, t] { return client->sections(b, t); });
            } else if (nav == 3)
                load("/library/onDeck", "Continue Watching");
            else if (nav == 1 || nav == 2) {
                auto b = base, t = server_token;
                submit(Sections, [b, t] { return client->sections(b, t); });
            } else if (nav == 5) {
                editor = 1;
                edit_text.clear();
                edit_focus = 0;
            } else if (nav == 4)
                message = "Watchlist integration is not implemented yet";
        }
    } else if (settings) {
        if (hit & PAD_UP)
            focus = std::max(0, focus - 1);
        if (hit & PAD_DOWN)
            focus = std::min(5, focus + 1);
        if (hit & PAD_CROSS) {
            if (focus == 0)
                global_delay = audio_offset(global_delay, 1);
            if (focus == 1)
                global_delay = audio_offset(global_delay, -1);
            if (focus == 2) {
                allow_http = !allow_http;
                message =
                    allow_http
                        ? "Explicit HTTP enabled: credentials travel unencrypted on these endpoints"
                        : "HTTPS only";
            }
            if (focus == 3)
                refresh();
            if (focus == 4)
                signout();
            if (focus == 5) {
                editor = 2;
                edit_text = "https://";
                edit_focus = 0;
            }
            pref_set("plex_audio_delay", global_delay);
            pref_set("plex_allow_http", allow_http ? 1 : 0);
        }
        row(0, focus, 210, "Default audio delay +25 ms (" + std::to_string(global_delay) + " ms)");
        row(1, focus, 295, "Default audio delay -25 ms");
        row(2, focus, 380, allow_http ? "Local HTTP: allowed" : "Local HTTP: disabled");
        row(3, focus, 465, "Refresh servers");
        row(4, focus, 550, "Sign out");
        row(5, focus, 635, "Manual server address");
        draw_text(
            390, 750, 23,
            "kstuff Lite + ShadowMountPlus 1.7 Beta 5 Fix 1: awaiting PS5 13.40 verification");
    } else if (!available.empty()) {
        if (hit & PAD_UP)
            focus = std::max(0, focus - 1);
        if (hit & PAD_DOWN)
            focus = std::min(static_cast<int>(available.size()) - 1, focus + 1);
        int first = std::max(0, focus - 7);
        for (int i = first; i < static_cast<int>(available.size()) && i < first + 8; ++i)
            row(i, focus, 210 + (i - first) * 85, available[i].name);
        if ((hit & PAD_CROSS) && job == None) {
            auto s = available[focus];
            server_token = s.token;
            bool http = allow_http;
            submit(Select, [s, http] { return client->select(s, http, next_base); });
        }
    } else if (in_detail) {
        draw_text(390, 220, 25,
                  std::to_string(detail.year) + " • " + std::to_string(detail.duration / 60000) +
                      " min");
        float scale = gfx.height / 1080.f;
        ImGui::GetBackgroundDrawList()->AddText(
            ImGui::GetFont(), 28 * scale, {390 * scale, 280 * scale}, IM_COL32(225, 225, 230, 255),
            detail.summary.c_str(), nullptr, 1250 * scale);
        row(0, focus, 640, detail.offset > 0 ? "Resume" : "Play");
        row(1, focus, 725, detail.watched ? "Mark unwatched" : "Mark watched");
        row(2, focus, 810, "Playback version: " + std::to_string(version + 1) + " (Cross cycles)");
        if (hit & PAD_UP)
            focus = std::max(0, focus - 1);
        if (hit & PAD_DOWN)
            focus = std::min(2, focus + 1);
        if ((hit & PAD_CROSS) && job == None) {
            if (focus == 0)
                play();
            else if (focus == 2) {
                if (detail.media.is_array() && !detail.media.empty())
                    version = (version + 1) % detail.media.size();
            } else {
                auto b = base, t = server_token;
                auto i = detail;
                submit(Watch, [b, t, i] { return client->watched(b, t, i, !i.watched); });
            }
        }
    } else {
        if (!sidebar && (hit & PAD_UP))
            focus = std::max(0, focus - (grid ? 5 : 1));
        if (!sidebar && (hit & PAD_DOWN) && !listing.empty())
            focus = std::min(static_cast<int>(listing.size()) - 1, focus + (grid ? 5 : 1));
        if (grid) {
            int first = std::max(0, (focus / 5 - 1) * 5);
            for (int i = first; i < static_cast<int>(listing.size()) && i < first + 10; ++i) {
                float x = 390 + (i - first) % 5 * 285, y = 210 + (i - first) / 5 * 350;
                bool selected = i == focus && !sidebar;
                rect(x - 5, y - 5, 260, 325,
                     selected ? IM_COL32(224, 160, 45, 255) : IM_COL32(30, 32, 40, 255));
                auto texture = artwork->get(base, server_token, listing[i].thumb);
                if (texture) {
                    float s = gfx.height / 1080.f;
                    ImGui::GetBackgroundDrawList()->AddImage(texture, {x * s, y * s},
                                                             {(x + 250) * s, (y + 270) * s});
                } else
                    draw_text(x + 20, y + 115, 22, "Artwork unavailable");
                auto label = listing[i].title;
                if (label.size() > 22)
                    label = label.substr(0, 19) + "…";
                draw_text(x + 7, y + 285, 23, label);
            }
        } else {
            int first = std::max(0, focus - 7);
            for (int i = first; i < static_cast<int>(listing.size()) && i < first + 8; ++i)
                row(i, focus, 210 + (i - first) * 85, listing[i].title, !sidebar);
        }
        if (!sidebar && (hit & PAD_CROSS))
            open_item();
        // Preserve the existing sort command, freeing R1 for section navigation.
        if ((hit & PAD_SQUARE) && !page_path.empty())
            load(page_path + (page_path.find('?') == std::string::npos ? "?" : "&") +
                     "sort=titleSort:asc", "Sorted by title");
        if ((hit & PAD_R2) && page_start + 60 < page_total)
            load(page_path, page_title, false, page_start + 60);
        if ((hit & PAD_L2) && page_start >= 60)
            load(page_path, page_title, false, page_start - 60);
        if (page_total > 60)
            draw_text(390, 920, 22,
                      "Square Sort • L2/R2 Pages • " + std::to_string(page_start + 1) + "–" +
                          std::to_string(page_start + listing.size()) + " of " +
                          std::to_string(page_total));
    }
    draw_text(390, 1020, 23,
              "D-pad / Stick Navigate • Cross Select • Circle Back • L1/R1 Sections • Options Settings");
}
bool ui_video_rect(float *x, float *y, float *w, float *h) {
    if (!video)
        return false;
    *x = 0;
    *y = 0;
    *w = gfx.width;
    *h = gfx.height;
    return true;
}
bool ui_quit_requested() { return false; }
