// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the production UI with real ImGui, fake player/network/platform only.
#include "../src/plex_ui.cc"
#include <cassert>
#include <thread>
Gfx gfx{};
static double clock_now;
static bool paused;
static int64_t position = 20000;
extern "C" double plat_time() { return clock_now; }
extern "C" const char *plat_data_dir() { return "/tmp"; }
int pref_int(const char *, int v) { return v; }
void pref_set(const char *, int) {}
namespace plex {
Transport ps5_transport() { return [](const Request &) { return Reply{}; }; }
void Artwork::update() {}
void Artwork::clear() {}
ImTextureID Artwork::get(const std::string &, const std::string &, const std::string &) { return 0; }
}
bool player_open(const std::string &, int64_t, const std::vector<std::string> &) { return true; }
void player_stop() {}
PlayerEvent player_tick() { return PLAYER_EVENT_NONE; }
bool player_ended() { return false; }
bool player_failed() { return false; }
bool player_paused() { return paused; }
void player_toggle_pause() { paused = !paused; }
int64_t player_time() { return position; }
int64_t player_length() { return 100000; }
void player_seek(int64_t value) { position = value; }
int64_t player_audio_delay() { return 0; }
void player_set_audio_delay(int64_t) {}
std::vector<Track> player_audio_tracks() { return {{1,"A"},{2,"B"}}; }
std::vector<Track> player_subtitle_tracks() { return {}; }
int player_audio_track() { return 1; }
int player_subtitle_track() { return -1; }
void player_set_audio_track(int) {}
void player_set_subtitle_track(int) {}
static void frame(uint32_t buttons = 0, float lx = 0, float ly = 0, float dt = 1.f/60) {
    PadState pad{}; pad.connected = true; pad.buttons = buttons; pad.lx = lx; pad.ly = ly;
    clock_now += dt;
    controller_imgui(pad);
    ImGui::GetIO().DeltaTime = dt;
    ImGui::NewFrame(); ui_frame(pad, dt); ImGui::Render();
}
static void press(uint32_t button) { frame(); frame(button); frame(); }
int main() {
    ImGui::CreateContext();
    auto &io = ImGui::GetIO(); io.IniFilename = nullptr; io.DisplaySize = {1920,1080};
    io.Fonts->AddFontDefault(); unsigned char *pixels; int w,h;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    gfx.width=1920; gfx.height=1080;
    artwork = std::make_unique<Artwork>(ps5_transport());
    client = std::make_unique<Client>(ps5_transport(), "controller-test");
    // Initial sign-in has a real custom selection. No network needed to move UI.
    assert(focus == 0 && nav == 0 && sidebar && account.empty()); frame();
    assert(io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad);
    account="fixture"; base="https://fixture.invalid"; sidebar=true;
    press(PAD_DOWN); assert(nav==1); press(PAD_UP); assert(nav==0);
    press(PAD_RIGHT); assert(!sidebar); press(PAD_LEFT); assert(sidebar);
    press(PAD_OPTIONS); assert(settings && !sidebar && focus==0);
    press(PAD_DOWN); assert(focus==1);
    // Cross selects the focused setting, while holding Cross does not repeat.
    press(PAD_DOWN); assert(focus==2 && !allow_http);
    frame(PAD_CROSS); assert(allow_http); frame(PAD_CROSS); assert(allow_http); frame();
    press(PAD_CIRCLE); assert(!settings && sidebar);
    // Independent repeat timers keep a held direction moving across other presses.
    press(PAD_OPTIONS); frame(PAD_DOWN);
    for(int i=0;i<10;++i) { frame(PAD_DOWN); } assert(focus==1);
    frame(PAD_DOWN|PAD_CROSS);
    for(int i=0;i<20;++i) { frame(PAD_DOWN); } assert(focus>=2);
    frame(); focus=0;
    for(int i=0;i<40;++i) { frame(0,.12f,.12f); } assert(focus==0);
    frame(0,0,.9f); assert(focus==1); frame();
    // Tabs go through the existing sidebar activation. Settings is section 6.
    settings=false; sidebar=true; nav=5; press(PAD_R1); assert(nav==6 && settings);
    // Back from keyboard and detail restore the previous context.
    editor=1; press(PAD_CIRCLE); assert(editor==0);
    settings=false; in_detail=true; sidebar=false; focus=2;
    press(PAD_UP); assert(focus==1); press(PAD_UP); assert(focus==0); press(PAD_CIRCLE); assert(!in_detail);
    listing.resize(12); page_path="/library/metadata/grid"; focus=0; sidebar=false;
    press(PAD_DOWN); assert(focus==5); press(PAD_RIGHT); assert(focus==6);
    press(PAD_LEFT); assert(focus==5); press(PAD_UP); assert(focus==0);
    frame(0,.9f,0); assert(focus==1); frame();
    history.push_back({"/old", "Previous", listing, 3, 0, 12});
    press(PAD_CIRCLE); assert(page_path=="/old" && focus==3 && history.empty());
    frame(PAD_CROSS); frame(PAD_CROSS);
    assert(ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown));
    frame(); frame(); assert(!ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown));
    // Playback Cross pauses; directions seek; track modal consumes Cross/Back.
    video=true; tracks=false; progress_pending=false; progress={}; job=None;
    frame(); press(PAD_CROSS); assert(paused);
    press(PAD_RIGHT); assert(position==30000);
    press(PAD_LEFT); assert(position==20000);
    press(PAD_OPTIONS); assert(tracks);
    press(PAD_CROSS); assert(paused); // select track does not also pause
    press(PAD_CIRCLE); assert(video && !tracks);
    press(PAD_CIRCLE); assert(!video && in_detail && !sidebar && focus==0);
    // A held stop button cannot also dismiss the returned detail screen.
    frame(PAD_CIRCLE); assert(!in_detail); frame(); // fresh press is Back
    // UI remains responsive while an asynchronous request is unfinished.
    std::promise<Result> response; pending=response.get_future(); job=Browse;
    settings=true; sidebar=false; focus=0; frame(); press(PAD_DOWN); assert(focus==1);
    press(PAD_CIRCLE); assert(!settings && sidebar);
    response.set_value(Result{}); frame();
    ControllerInput disconnected; PadState pad{}; pad.buttons=PAD_CROSS;
    assert(disconnected.update(pad,.016f)==0);
    pad.connected=true; assert(disconnected.update(pad,.016f)&PAD_CROSS);
    pad.connected=false; assert(disconnected.update(pad,.016f)==0);
    pad.connected=true; assert(disconnected.update(pad,.016f)&PAD_CROSS);
    ImGui::DestroyContext();
    puts("Controller UI scenarios passed: focus, D-pad, Cross, Back, repeat, analog, tabs, playback return, async responsiveness");
}
