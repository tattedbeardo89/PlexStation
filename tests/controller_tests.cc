// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the production UI with real ImGui, fake player/network/platform only.
#include "../src/plex_ui.cc"
#include <cassert>
#include <thread>
Gfx gfx{};
static double clock_now;
static std::string test_data_dir = "/tmp";
static bool paused, private_input;
static std::string last_opened_url;
extern "C" void plat_pad_set_private(bool enabled) {private_input=enabled;}
static int64_t position = 20000;
extern "C" double plat_time() { return clock_now; }
extern "C" const char *plat_data_dir() { return test_data_dir.c_str(); }
static std::map<std::string,std::string> test_prefs;
int pref_int(const char *key, int v) {auto it=test_prefs.find(key); return it==test_prefs.end() ? v : std::stoi(it->second);}
void pref_set(const char *key, int v) {test_prefs[key]=std::to_string(v);}
std::string pref_str(const char *key,const char *fallback) {auto it=test_prefs.find(key); return it==test_prefs.end() ? fallback:it->second;}
void pref_set(const char *key,const std::string &value) {test_prefs[key]=value;}
namespace plex {
Transport transport_override;
Transport ps5_transport() { return transport_override ? transport_override : Transport([](const Request &) { return Reply{}; }); }
void Artwork::update() {}
void Artwork::clear() {}
ImTextureID Artwork::landscape(const std::string &,const std::string &,const std::string &) {return 0;}
ImTextureID Artwork::avatar(const std::string &,const std::string &) {return 0;}
ImTextureID Artwork::get(const std::string &, const std::string &, const std::string &) { return 0; }
}
bool player_open(const std::string &url, int64_t, const std::vector<std::string> &) {last_opened_url=url;return true;}
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
    auto before_cancel=listing.size();
    response.set_value(Result{}); frame(); assert(listing.size()==before_cancel);
    while(job!=None) {pending.wait();finish_job();}
    if(progress_pending) {frame();while(job!=None){pending.wait();finish_job();}}
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
        std::promise<Result> ready; pending=ready.get_future(); job=kind; pending_generation=screen_generation;
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
        if(q.url.find("/hubs")!=std::string::npos || q.url.find("/library/onDeck")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[]}})",""};
        if(q.url=="https://plex.tv/api/v2/user")
            return Reply{200,R"({"id":42,"username":"fixture","home":false})",""};
        if(q.url.find("/identity")!=std::string::npos) return Reply{200,"<MediaContainer machineIdentifier='server'/>",""};
        if(q.url.find("/library/sections")!=std::string::npos) return Reply{200,R"({"MediaContainer":{"Directory":[]}})",""};
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
    assert(account=="fixture-account-token" && account_loading && job==ValidateAccount);
    pending.wait(); finish_job(); assert(primary_id=="id:42" && profile_screen && job==None);
    requested_profile=home_profiles.front();choose_profile();assert(job==Resources);
    pending.wait(); finish_job();
    assert(!account_loading && available.size()==1 && !sidebar && focus==0);
    std::string restored_id,restored_token;
    assert(load_credentials(credential_file,restored_id,restored_token));
    assert(restored_id==device && restored_token==account);
    // Controller diagnostics and manual test-before-save, including HTTP consent.
    frame(); press(PAD_TRIANGLE); assert(connection_diagnostics);
    press(PAD_CIRCLE); assert(!connection_diagnostics);
    connection_target=available.front();
    auto manual_transport=[](const Request &q) {
        assert(q.headers.at("X-Plex-Token")=="fixture-scoped-token");
        if(q.url.find("/identity")!=std::string::npos)
            return Reply{200,"<MediaContainer machineIdentifier='server'/>",""};
        if(q.url.find("/hubs")!=std::string::npos || q.url.find("/library/onDeck")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[]}})",""};
        assert(q.url=="http://127.0.0.1:32400/library/sections");
        return Reply{200,R"({"MediaContainer":{"Directory":[]}})",""};
    };
    client=std::make_unique<Client>(manual_transport,device);
    editor=2; edit_text="http://127.0.0.1"; edit_focus=0; allow_http=false;
    press(PAD_TRIANGLE); assert(manual_confirm_http && !allow_http);
    press(PAD_CIRCLE); assert(!manual_confirm_http && !allow_http && editor==2);
    press(PAD_TRIANGLE); press(PAD_CROSS);
    if(job==Manual) {pending.wait(); finish_job();}
    assert(allow_http && manual_tested && connection_diagnostics);
    assert(!test_prefs.count("plex_manual_uri")); // test alone does not save
    press(PAD_CROSS);
    assert(!connection_diagnostics && !manual_tested && editor==0);
    assert(test_prefs[account_key("plex_manual_uri")]=="http://127.0.0.1:32400" && test_prefs[account_key("plex_manual_server_id")]=="server");
    assert(base=="http://127.0.0.1:32400" && selected_server.id=="server");
    while(job!=None) {pending.wait();finish_job();}
    // Simulate process startup using the real ui_init and saved private file.
    pref_set(primary_key("plex_show_profiles").c_str(),0);
    account.clear(); assert(ui_init()); assert(account_loading && job==ValidateAccount);
    io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    pending.wait(); finish_job(); assert(job==Resources);
    pending.wait(); finish_job(); assert(!account_loading && job==Select && reconnect_screen);
    pending.wait(); finish_job(); assert(!reconnect_screen && available.empty() && selected_server.id=="server");
    while(job!=None) {pending.wait();finish_job();}
    assert(account_pref("plex_preferred_server")=="server");
    // A short movie response cannot retain an index from the previous long list.
    home_screen=false;listing.resize(12); focus=11;
    complete(Browse,Result{Json{{"MediaContainer",{{"Metadata",Json::array({{{"ratingKey","movie"},{"type","movie"}}})}}}},"",200});
    assert(listing.size()==1 && focus==0);
    focus=11; open_item(); assert(job==None); focus=-1; open_item(); assert(job==None);
    focus=0;
    // Account scope survives sign-out but cannot leak to another Plex identity.
    auto saved_scope=account_id;
    account_id="id:84/profile:84"; assert(account_pref("plex_preferred_server").empty());
    save_account_pref("plex_preferred_server","other-server");
    account_id=saved_scope; assert(account_pref("plex_preferred_server")=="server");
    // Offline preferred PMS retains its preference and supports retry/cancellation.
    reconnect_screen=true; connection_target=selected_server;
    complete(Select,Result{{},"Server temporarily offline",0});
    assert(reconnect_screen && account_pref("plex_preferred_server")=="server");
    std::promise<Result> slow_connection; pending=slow_connection.get_future(); job=Select;
    press(PAD_CIRCLE); assert(discard_connection && job==Select);
    slow_connection.set_value(Result{Json{{"MediaContainer",{{"Directory",Json::array()}}}},"",200});
    finish_job(); assert(!reconnect_screen && account_pref("plex_preferred_server")=="server");
    // Missing preferred server shows picker without silently choosing an alternate.
    complete(Resources,Result{Json::array({{{"name","Other"},{"provides","server"},{"clientIdentifier","other"},{"accessToken","scoped"}}}),"",200});
    assert(job==None && available.size()==1 && account_pref("plex_preferred_server")=="server");
    // Explicit switch skips auto-connect even when the preferred server is available.
    force_picker=true;
    complete(Resources,Result{Json::array({{{"name","Saved"},{"provides","server"},{"clientIdentifier","server"},{"accessToken","scoped"}},{{"name","Other"},{"provides","server"},{"clientIdentifier","other"},{"accessToken","scoped"}}}),"",200});
    assert(job==None && available.size()==2);
    connection_target=available[1]; next_base="https://other.invalid";
    complete(Select,Result{Json{{"MediaContainer",{{"Directory",Json::array()}}}},"",200});
    assert(account_pref("plex_preferred_server")=="other" && !reconnect_screen);
    while(job!=None) {pending.wait();finish_job();}
    // Real controller settings paths, default hidden overlay, persisted opt-in.
    assert(!controller_overlay); settings=true; sidebar=false; settings_page=0; focus=7;
    press(PAD_CROSS); assert(settings_page==2 && focus==0);
    press(PAD_CROSS); assert(controller_overlay && pref_int("plex_controller_overlay_v2",0)==1);
    press(PAD_CIRCLE); assert(settings_page==0 && focus==7);
    focus=6; press(PAD_CROSS); assert(settings_page==1);
    press(PAD_CROSS); assert((job==Resources && force_picker) || (job==None && !settings && available.size()==1));
    if(job==Resources) {pending.wait();finish_job();}
    assert(job==None && !settings && available.size()==1);

    // Real UI profile lifecycle: primary session stays linked; Home tokens are isolated.
    home_enabled=true; primary_id="id:42";
    home_profiles={{"42","owner","Owner","",true,true,false},{"84","child","Child","",true,false,true}};
    int child_resources=0, child_timelines=0;
    auto child_transport=[&](const Request &q) {
        if(q.url.find("/switch")!=std::string::npos) {
            assert(q.headers.at("X-Plex-Token")=="fixture-account-token");
            assert(q.method=="POST" && q.url.find("/84/switch")!=std::string::npos);
            if(q.body=="pin=0000")return Reply{401,"",""};
            assert(q.body=="pin=0123");
            return Reply{200,"<user id='84' authenticationToken='child-account-token'/>",""};
        }
        if(q.url.find("/api/v2/user")!=std::string::npos) {
            assert(q.headers.at("X-Plex-Token")=="child-account-token");return Reply{200,R"({"id":84})",""};
        }
        if(q.url.find("/api/v2/resources?")!=std::string::npos) {
            ++child_resources;assert(q.headers.at("X-Plex-Token")=="child-account-token");
            return Reply{200,R"([{"name":"Child server","provides":"server","clientIdentifier":"child-server","accessToken":"child-server-token","connections":[{"uri":"https://child.invalid","local":true}]}])",""};
        }
        assert(q.headers.at("X-Plex-Token")=="child-server-token");
        if(q.url.find("/hubs")!=std::string::npos || q.url.find("recentlyAdded")!=std::string::npos || q.url.find("originallyAvailableAt")!=std::string::npos || q.url.find("/library/onDeck")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[]}})",""};
        if(q.url.find("/identity")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"machineIdentifier":"child-server"}})",""};
        if(q.url.find("/:/timeline")!=std::string::npos) {++child_timelines;return Reply{200,"{}",""};}
        if(q.url.find("/library/metadata/show1/children")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"season1","type":"season","title":"Season"}]}})",""};
        if(q.url.find("/library/metadata/season1/children")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"episode1","type":"episode","title":"Episode"}]}})",""};
        if(q.url.find("/library/metadata/show1")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"show1","type":"show","title":"Show"}]}})",""};
        if(q.url.find("/library/metadata/season1")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"season1","type":"season","title":"Season"}]}})",""};
        if(q.url.find("/library/metadata/episode1")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"episode1","type":"episode","title":"Episode","Media":[{"container":"mp4","videoCodec":"h264","Part":[{"key":"/library/parts/episode1/file.mp4"}]}]}]}})",""};
        if(q.url.find("/library/metadata/")!=std::string::npos) {
            auto id=q.url.substr(q.url.find_last_of('/')+1);
            return Reply{200,Json{{"MediaContainer",{{"Metadata",Json::array({{{"ratingKey",id},{"type","movie"},{"title","Selected fixture movie"},{"duration",100000},{"viewOffset",1000}}})}}}}.dump(),""};
        }
        assert(q.url.find("/library/sections")!=std::string::npos);
        return Reply{200,R"({"MediaContainer":{"Directory":[{"key":"99","title":"Child library","type":"movie"}]}})",""};
    };
    client=std::make_unique<Client>(child_transport,device);
    auto drain=[&] {for(int i=0;i<8 && job!=None;++i){pending.wait();finish_job();}assert(job==None);};
    show_profiles();assert(profile_screen && profile_token.empty() && listing.empty() && history.empty());
    profile_focus=1;press(PAD_CROSS);assert(profile_pin && pin_digits.empty() && private_input && ui_sensitive_input());
    for(int i=0;i<4;++i)press(PAD_CROSS);
    drain();assert(profile_pin && !profile_error.empty() && pin_digits.empty() && child_resources==0);
    press(PAD_CROSS); // 0
    press(PAD_RIGHT);press(PAD_CROSS); // 1
    press(PAD_RIGHT);press(PAD_CROSS); // 2
    press(PAD_RIGHT);press(PAD_CROSS); // 3
    drain();frame();assert(!private_input && !ui_sensitive_input());
    assert(active_profile.id=="84" && account=="fixture-account-token" && profile_token=="child-account-token");
    assert(account_id=="id:42/profile:child" && child_resources==1 && available.size()==1);
    connect_server(available.front());drain();assert(listing.size()==1 && listing[0].id=="99");
    assert(server_token=="child-server-token" && account_pref("plex_preferred_server")=="child-server");
    playing.id="child-movie";playing.duration=100000;queue_progress("paused",1000);frame();drain();assert(child_timelines==1);
    for(const auto &entry:test_prefs)assert(entry.second.find("token")==std::string::npos);
    // Production Home carousel focus, long labels, progress metadata and details return.
    feed_rows.clear();feed_columns.clear();feed_scroll.clear();
    for(int r=0;r<3;++r) {
        Hub hub;hub.id="row"+std::to_string(r);hub.title=r ? "Recently Added" : "Continue Watching";hub.continuing=r==0;
        for(int c=0;c<8;++c){Item item;item.id="movie"+std::to_string(c);item.type="movie";item.title=std::string(90,'L');item.duration=100000;item.offset=25000;hub.entries.push_back(item);}
        feed_rows.push_back(hub);feed_columns.push_back(0);feed_scroll.push_back(0);
    }
    home_screen=true;in_detail=false;settings=false;sidebar=false;feed_focus=0;nav=0;
    for(int i=0;i<6;++i)press(PAD_RIGHT);
    assert(feed_columns[0]==6 && !sidebar);
    press(PAD_DOWN);assert(feed_focus==1 && feed_columns[1]==0);
    press(PAD_RIGHT);assert(feed_columns[1]==1);
    press(PAD_UP);assert(feed_focus==0 && feed_columns[0]==6);
    press(PAD_CROSS);drain();assert(in_detail && detail.id=="movie6");
    press(PAD_CIRCLE);assert(home_screen && !in_detail && feed_columns[0]==6);
    feed_columns[0]=0;press(PAD_LEFT);assert(sidebar);press(PAD_RIGHT);assert(!sidebar && feed_columns[0]==0);
    diagnostic_flags=TextOnly;frame();assert(home_screen);diagnostic_flags=0;
    // Production TV hierarchy opens details, then seasons, then episodes and keeps the profile token.
    home_screen=false;in_detail=false;sidebar=false;focus=0;history.clear();page_path="/library/sections/99/all";
    listing={};Item show;show.id="show1";show.type="show";listing.push_back(show);
    open_item();drain();assert(in_detail && detail.type=="show");
    press(PAD_CROSS);drain();assert(!in_detail && listing.size()==1 && listing[0].type=="season");
    open_item();drain();assert(in_detail && detail.type=="season");
    press(PAD_CROSS);drain();assert(!in_detail && listing[0].type=="episode");
    open_item();drain();assert(in_detail && detail.type=="episode");
    press(PAD_SQUARE);assert(full_description);press(PAD_CIRCLE);assert(!full_description && in_detail);
    press(PAD_CROSS);drain();assert(video && last_opened_url.find("child-server-token")!=std::string::npos && last_opened_url.find("fixture-account-token")==std::string::npos);
    press(PAD_CIRCLE);drain();assert(!video && in_detail);
    press(PAD_CIRCLE);drain();assert(!in_detail && !home_screen);
    press(PAD_CIRCLE);drain();assert(in_detail && detail.type=="season");

    // A cancelled old-user response must not replace the new profile picker.
    std::promise<Result> old_user;pending=old_user.get_future();job=Browse;pending_generation=screen_generation;
    show_profiles();assert(deferred_profiles && profile_screen);
    old_user.set_value(Result{Json{{"MediaContainer",{{"Metadata",Json::array({{{"ratingKey","old-user"}}})}}}},"",200});
    finish_job();assert(profile_screen && listing.empty() && profile_token.empty() && base.empty());
    // Remembered protected profiles still require selection/PIN, even with picker disabled.
    pref_set(primary_key("plex_show_profiles").c_str(),0);profiles_ready();assert(job==None && profile_screen);
    // A switched token which validates as the wrong profile never reaches resources.
    requested_profile=home_profiles[1];profile_token="untrusted-profile-token";
    complete(ValidateProfile,Result{Json{{"id",42}},"",200});
    assert(profile_token.empty() && child_resources==1 && !profile_error.empty());
    assert(pref_str(primary_key("plex_last_profile").c_str(),"")=="84");

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
    // Missing Home capability metadata must discover profiles, not assume PIN-free admin access.
    account="fixture-account-token";
    client=std::make_unique<Client>([](const Request &q) {
        assert(q.url=="https://plex.tv/api/v2/home/users");
        return Reply{200,R"({"users":[{"id":42,"title":"Owner","protected":true,"admin":true}]})",""};
    },device);
    complete(ValidateAccount,Result{Json{{"id",42},{"username","fixture"}},"",200});
    assert(job==HomeUsers);pending.wait();finish_job();
    requested_profile=home_profiles.front();choose_profile();
    assert(profile_pin && profile_token.empty() && job==None);signout();
    unlink(credential_file.c_str()); rmdir(temp);
    puts("Authentication UI transitions passed: cancellation, approval, server selection, startup validation, retryable network failure, invalid-token clearing, sign-out");
    ImGui::DestroyContext();
    puts("Controller UI scenarios passed: focus, D-pad, Cross, Back, repeat, analog, tabs, playback return, async responsiveness");
}
