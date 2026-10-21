// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the production UI with real ImGui, fake player/network/platform only.
#include "../src/plex_ui.cc"
#include <cassert>
#include <thread>
Gfx gfx{};
static double clock_now;
static std::string test_data_dir = "/tmp";
static bool paused;
static int64_t position = 20000;
extern "C" double plat_time() { return clock_now; }
extern "C" const char *plat_data_dir() { return test_data_dir.c_str(); }
int pref_int(const char *, int v) { return v; }
void pref_set(const char *, int) {}
namespace plex {
Transport transport_override;
Transport ps5_transport() { return transport_override ? transport_override : Transport([](const Request &) { return Reply{}; }); }
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
    // Authentication UI lifecycle uses production completion/cancellation code.
    char temp[]="/tmp/plexstation-ui-auth-XXXXXX"; assert(mkdtemp(temp));
    test_data_dir=temp; credential_file=test_data_dir+"/plexstation-account.json";
    device="fixture-device"; account.clear(); account_loading=false; settings=false;
    auto complete = [&](Job kind, Result result) {
        assert(job==None);
        std::promise<Result> ready; pending=ready.get_future(); job=kind;
        ready.set_value(std::move(result)); finish_job();
    };
    auto pin_reply = Result{Json{{"id",55},{"code","A7BZ"},{"authToken",nullptr},{"expiresIn",300}},"",201};
    auth.begin(clock_now); auth_request_generation=auth.generation;
    auth.cancel(); complete(PinStart,pin_reply);
    assert(account.empty() && auth.state==AuthState::Cancelled);
    auth.begin(clock_now); auth_request_generation=auth.generation;
    complete(PinStart,pin_reply); assert(auth.state==AuthState::Waiting && have_qr);
    auth.cancel(); pin_reply.data["authToken"]="cancelled-fixture-token";
    complete(PinPoll,pin_reply); assert(account.empty());
    auto fixture_transport = [](const Request &q) {
        if(q.url=="https://plex.tv/api/v2/user")
            return Reply{200,R"({"id":42,"username":"fixture"})",""};
        assert(q.url.find("/api/v2/resources?")!=std::string::npos);
        assert(q.headers.at("X-Plex-Token")=="fixture-account-token");
        return Reply{200,R"([{"name":"Fixture server","provides":"server","clientIdentifier":"server","accessToken":"fixture-scoped-token","connections":[]}])",""};
    };
    transport_override=fixture_transport;
    client=std::make_unique<Client>(fixture_transport,device);
    pin_reply.data["authToken"]=nullptr;
    auth.begin(clock_now); auth_request_generation=auth.generation;
    complete(PinStart,pin_reply);
    pin_reply.data["authToken"]="fixture-account-token";
    complete(PinPoll,pin_reply);
    assert(account=="fixture-account-token" && account_loading && job==Resources);
    pending.wait(); finish_job();
    assert(!account_loading && available.size()==1 && !sidebar && focus==0);
    std::string restored_id,restored_token;
    assert(load_credentials(credential_file,restored_id,restored_token));
    assert(restored_id==device && restored_token==account);
    // Simulate process startup using the real ui_init and saved private file.
    account.clear(); assert(ui_init()); assert(account_loading && job==ValidateAccount);
    pending.wait(); finish_job(); assert(job==Resources);
    pending.wait(); finish_job(); assert(!account_loading && available.size()==1);
    // Temporary network failures preserve the saved account for a retry.
    account_loading=true;
    complete(ValidateAccount,Result{{},"Network or TLS request failed",0});
    assert(!account.empty() && !account_error.empty());
    assert(load_credentials(credential_file,restored_id,restored_token) && !restored_token.empty());
    // Rejected saved tokens are removed; sign-out persists no account token.
    complete(ValidateAccount,Result{{},"Access denied",401});
    assert(account.empty() && !account_loading && auth.state==AuthState::Failed);
    assert(load_credentials(credential_file,restored_id,restored_token) && restored_token.empty());
    account="fixture-account-token"; signout();
    assert(account.empty() && load_credentials(credential_file,restored_id,restored_token) && restored_token.empty());
    unlink(credential_file.c_str()); rmdir(temp);
    puts("Authentication UI transitions passed: cancellation, approval, server selection, startup validation, retryable network failure, invalid-token clearing, sign-out");
    ImGui::DestroyContext();
    puts("Controller UI scenarios passed: focus, D-pad, Cross, Back, repeat, analog, tabs, playback return, async responsiveness");
}
