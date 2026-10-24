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
static std::vector<std::string> last_opened_options;
extern "C" void plat_pad_set_private(bool enabled) {private_input=enabled;}
static int64_t position = 20000;
static int64_t last_opened_start;
static int player_open_calls=0;
static float test_rate=1.f;
const VideoInfo &player_video_info(){static VideoInfo info{};return info;}
static int64_t test_sub_delay=0;
int64_t player_subtitle_delay(){return test_sub_delay;}
void player_set_subtitle_delay(int64_t n){test_sub_delay=n;}
SubStyle player_sub_style(){return {100,0,false};}
void player_set_sub_style(const SubStyle &){}
static int subtitle_attach_calls=0;
static bool subtitle_attach_allowed=true;
static bool subtitle_attach_reopen=false;
bool player_add_subtitle(const std::string &,bool allow_reopen){++subtitle_attach_calls;subtitle_attach_reopen=allow_reopen;return subtitle_attach_allowed;}
static std::vector<SubtitleTrack> native_subtitles;
std::vector<SubtitleTrack> player_subtitle_catalog(){return native_subtitles;}
std::string player_audio_language(){return "";}
static std::string selected_sub_language;
void player_set_languages(const std::string &,const std::string &sub){selected_sub_language=sub;}

float player_rate(){return test_rate;}
void player_set_rate(float r){test_rate=r;}
bool player_buffering(){return false;}
std::vector<Chapter> player_chapters(){return {{0,"One"},{50000,"Two"}};}
int player_chapter(){return position>=50000?1:0;}
void player_set_chapter(int n){position=n*50000;}
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
void Artwork::retry_avatars() {}
std::string Artwork::avatar_state(const std::string &) const {return "Waiting";}
std::string Artwork::diagnostics() const {return "Fixture";}
void Artwork::set_scope(const std::string &,const std::string &,size_t) {}
ImTextureID Artwork::landscape(const std::string &,const std::string &,const std::string &) {return 0;}
ImTextureID Artwork::avatar(const std::string &,const std::string &) {return 0;}
ImTextureID Artwork::get(const std::string &, const std::string &, const std::string &) { return 0; }
}
bool player_open(const std::string &url, int64_t start, const std::vector<std::string> &options) {++player_open_calls;last_opened_start=start;last_opened_url=url;last_opened_options=options;return true;}
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
std::vector<Track> player_subtitle_tracks() {std::vector<Track> out{{-42,"Off"}};for(const auto &t:native_subtitles)out.push_back({t.id,t.name});return out;}
int player_audio_track() { return 1; }
static int subtitle_current=-42;
int player_subtitle_track() { return subtitle_current; }
void player_set_audio_track(int) {}
void player_set_subtitle_track(int id) {subtitle_current=id;}
static int off_choices=0;
void player_subtitles_off() {++off_choices;subtitle_current=-42;}
void player_subtitles_automatic(const std::string &) {}
static void frame(uint32_t buttons = 0, float lx = 0, float ly = 0, float dt = 1.f/60) {
    PadState pad{}; pad.connected = true; pad.buttons = buttons; pad.lx = lx; pad.ly = ly;
    clock_now += dt;
    controller_imgui(pad);
    ImGui::GetIO().DeltaTime = dt;
    if(!ImGui::GetIO().Fonts->IsBuilt()){unsigned char *pixels;int w,h;ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);}
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
    press(PAD_DOWN); assert(nav==4); press(PAD_UP); assert(nav==0);
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
    int warm_requests=0;
    auto fixture_transport = [&](const Request &q) {
        if(q.timeout_ms==1400){assert(q.connect_timeout_ms==800);++warm_requests;}
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
    pending.wait(); finish_job(); assert(job==FastConnect);
    auto warm_started=std::chrono::steady_clock::now();
    pending.wait(); finish_job(); assert(!account_loading && !reconnect_screen && selected_server.id=="server" && warm_requests==2);
    printf("Warm startup fixture: 2 bounded authorized endpoint requests; completion %.3f ms (host fixture only)\n",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-warm_started).count());
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
    assert(custom_focus_valid());feed_columns[0]=999;assert(!custom_focus_valid());frame();assert(feed_columns[0]==7 && custom_focus_valid());feed_columns[0]=6;
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
    // Production Music UI, audio-only options, profile token and queue isolation.
    account="owner";account_id="owner/profile:child";profile_token="child";
    base="https://audio.invalid";server_token="child-pms";account_loading=false;profile_screen=false;
    selected_server.id="audio-pms";home_sections.clear();Item section;section.id="12";section.type="artist";section.title="Music";home_sections.push_back(section);
    assert(audio_available() && !audio_available(true) && nav_visible(7));
    client=std::make_unique<Client>([](const Request &q) {
        assert(q.headers.at("X-Plex-Token")=="child-pms");
        return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"101","type":"track","title":"Song","parentTitle":"Album","grandparentTitle":"Artist","Media":[{"audioCodec":"flac","container":"flac","Part":[{"key":"/library/parts/101/song.flac"}]}]}]}})",""};
    },device);
    auto audio_drain=[&] {for(int n=0;n<20;++n){frame();if(job!=None){pending.wait();finish_job();}if(!audio.start_pending && !progress_pending && job==None)return;}assert(false);};
    audio_home();assert(audio.screen && audio.entries.size()==1 && !video && job==AudioFeed);
    audio_drain();assert(home_screen && library_context && library_kind==7 && active_library.id=="12");
    Item song;song.id="101";song.type="track";song.title="Song";
    audio_queue_load(song);audio_drain();
    assert(audio.active && !video && audio.now_playing && last_opened_url.find("child-pms")!=std::string::npos);
    assert(!last_opened_options.empty() && last_opened_options[0]==":no-video");
    paused=false;audio.focus=0;press(PAD_CROSS);assert(paused);
    press(PAD_CIRCLE);assert(!audio.now_playing && audio.active);
    press(PAD_TOUCHPAD);assert(audio.now_playing);
    // Designation is profile/server scoped and never changes PMS metadata.
    audio_settings_frame(PAD_CROSS);assert(audio_available(true) && !audio_available());
    audio_home(true);audio_drain();audio_queue_load(song);audio_drain();
    assert(audio.playback_books && audio.active && !video);
    paused=false;audio.focus=8;press(PAD_CROSS);assert(test_rate==1.10f);
    position=70000;audio.focus=12;press(PAD_CROSS);assert(position==50000);
    audio.focus=9;press(PAD_CROSS);assert(audio.sleep.deadline>clock_now);
    clock_now=audio.sleep.deadline+1;frame();assert(paused && audio.sleep.deadline==0);
    position=45000;book_save();auto saved=listening.get(audio_scope(),audio.book.id);assert(number(saved,"position")==45000);
    auto old_scope=audio_scope();account_id="other-profile";assert(listening.get(audio_scope(),audio.book.id).empty());assert(!audio_available(true));account_id="owner/profile:child";
    assert(audio_scope()==old_scope);audio_drain();audio_reset();audio_home(true);audio_drain();audio_queue_load(song);audio_drain();assert(last_opened_start==45000);
    clear_profile();assert(!audio.active && !audio.queue.current() && !audio.screen);
    drain();
    // Safe startup reaches Developer Options before any Home request.
    account="owner";account_id="owner/profile:child";profile_screen=false;account_loading=false;
    connection_target.id="safe-server";connection_target.token="child-pms";next_base="https://audio.invalid";
    diagnostic_flags=SafeHome;connected(Json{{"MediaContainer",{{"Directory",Json::array()}}}});
    assert(job==None && !home_screen && !reconnect_screen && message.find("Safe startup")!=std::string::npos);
    press(PAD_OPTIONS);assert(settings);settings_page=2;focus=10;press(PAD_CROSS);assert(!plex::diagnostic(SafeHome));
    pref_set("plex_developer_flags_v1",static_cast<int>(SafeHome|DetailedHome));
    pref_set("plex_safe_home_v1",0);restore_diagnostic_preferences();
    assert(!diagnostic(SafeHome) && diagnostic(DetailedHome));
    pref_set("plex_safe_home_v1",1);restore_diagnostic_preferences();assert(diagnostic(SafeHome));
    pref_set("plex_developer_flags_v1",0);pref_set("plex_safe_home_v1",0);
    diagnostic_flags=0;settings=false;load_home();drain();assert(job==None && home_screen);
    ui_frame_presented();
    ImVec2 uv0,uv1;
    Theme::cover_uv(16.f/9,1440,175,uv0,uv1);
    assert(uv0.x==0 && uv1.x==1 && uv0.y>0 && uv1.y<1);
    assert(std::abs((16.f/9)*(uv1.x-uv0.x)/(uv1.y-uv0.y)-1440.f/175)<.001f);
    Theme::cover_uv(16.f/9,200,400,uv0,uv1);
    assert(uv0.x>0 && uv1.x<1 && uv0.y==0 && uv1.y==1);
    Theme::cover_uv(0,200,400,uv0,uv1);assert(uv0.x==0 && uv1.y==1);
    // Production native theme: focus and motion preferences remain controller usable.
    drain();audio_reset();video=false;profile_screen=false;settings=true;settings_page=6;sidebar=false;focus=0;
    Theme::reduced_motion=false;press(PAD_CROSS);
    assert(Theme::reduced_motion && pref_int("plex_reduce_motion_v1",0)==1 && Theme::motion(0)==1);
    press(PAD_CROSS);assert(!Theme::reduced_motion && Theme::motion(0)==0);
    for(int scale:{1,2}) {
        gfx.width=1920*scale;gfx.height=1080*scale;io.DisplaySize={float(gfx.width),float(gfx.height)};
        Theme::apply(float(scale));
        for(int view=0;view<4;++view) {
            settings=view==0;in_detail=view==1;profile_screen=view==2;home_screen=view==3;
            profile_pin=false;profile_focus=0;focus=0;
            if(view==2)home_profiles={Profile{"1","fixture-uuid",std::string(100,'A'),"",true,false,false}};
            if(view==1){detail.type="movie";detail.title=std::string(200,'A');detail.summary=std::string(2000,'B');}
            frame();assert(custom_focus_valid());
            assert(ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_NavCursor])==Theme::Focus);
            const auto *data=ImGui::GetDrawData();assert(data && data->Valid && data->TotalVtxCount>0);
            for(const auto *list:data->CmdLists) {
                for(const auto &v:list->VtxBuffer)assert(std::isfinite(v.pos.x) && std::isfinite(v.pos.y));
                for(const auto &cmd:list->CmdBuffer)assert(cmd.ClipRect.x>=0 && cmd.ClipRect.y>=0 && cmd.ClipRect.z<=gfx.width && cmd.ClipRect.w<=gfx.height);
            }
        }
        settings=true;settings_page=6;profile_screen=false;in_detail=false;home_screen=false;
    }
    gfx.width=1920;gfx.height=1080;io.DisplaySize={1920,1080};
    puts("Native theme draw-list checks passed at 1080p/4K with controller motion preference");
    // Per-profile default Off, remembered mode, and controller-only subtitle menu.
    video=false;test_prefs.clear();restore_video_preferences();assert(selected_sub_language=="off" && subtitle_default_mode==0);
    subtitle_default_mode=2;subtitle_language_choice=1;subtitle_remember=true;
    apply_subtitle_preferences(true);restore_video_preferences();assert(selected_sub_language=="off" && subtitle_default_mode==2 && subtitle_language_choice==1);
    subtitle_remember=false;apply_subtitle_preferences(true);restore_video_preferences();assert(selected_sub_language=="off" && subtitle_default_mode==2);
    video=true;subtitle_menu=true;subtitle_focus=0;int before_off=off_choices;
    playback_track_frame(PAD_CROSS);assert(off_choices==before_off+1 && selected_sub_language=="off");
    service_subtitle_selection();assert(subtitle_default_mode==2 && subtitle_selection.choice==-1);
    assert(playback_tracks_focus_valid());video=false;subtitle_menu=false;
    subtitle_selection.reset();subtitle_default_mode=0;detail.id="different-movie";prepare_subtitle_selection();assert(subtitle_selection.choice==-1);
    subtitle_selection.choice=0;detail.id="different-episode";prepare_subtitle_selection();assert(subtitle_selection.choice==-1);
    // Pre-play choice maps Plex metadata to verified native IDs after discovery.
    subtitle_default_mode=0;subtitle_language_choice=0;subtitle_selection.reset();version=0;
    detail.id="subtitle-movie";detail.type="movie";detail.media=Json::array({{{"Part",Json::array({{{"Stream",Json::array({{{"id",9001},{"streamType",3},{"languageCode","eng"},{"title","English"}}})}}})}}});
    prepare_subtitle_selection();assert(subtitle_selection.choice==-1);
    auto preselect_opens=player_open_calls;
    subtitle_focus=3;subtitle_picker_frame(PAD_CROSS);assert(subtitle_selection.choice==0 && subtitle_default_mode==0 && player_open_calls==preselect_opens);
    native_subtitles={{7,"English","en"}};playing=detail;video=true;begin_session_subtitles();service_subtitle_selection();
    assert(player_subtitle_track()==7 && !subtitle_pending && subtitle_default_mode==0);
    auto before_switch_url=last_opened_url;auto before_switch_position=position;auto before_switch_opens=player_open_calls;
    video_info=true;video_info_focus=6;tracks=false;subtitle_menu=false;
    video_tools(PAD_CROSS);
    assert(!video_info && tracks && subtitle_menu);
    assert(player_subtitle_track()==7 && position==before_switch_position && last_opened_url==before_switch_url);
    subtitle_menu=false;tracks=false;

    choose_subtitle_mode(0);assert(player_subtitle_track()==-42 && position==before_switch_position && last_opened_url==before_switch_url);
    player_seek(position+1000);player_toggle_pause();service_subtitle_selection();assert(player_subtitle_track()==-42);
    subtitle_selection.choice=-3;begin_session_subtitles();service_subtitle_selection();assert(player_subtitle_track()==7 && subtitle_selection.choice==-3);
    native_subtitles.clear();subtitle_selection.choice=-3;begin_session_subtitles();clock_now+=13;service_subtitle_selection();assert(player_subtitle_track()==-42 && subtitle_selection.choice==-1);
    // Re-select an already attached external track without duplicate attachment or reopening.
    playing.media[0]["Part"][0]["Stream"].push_back({{"id",9002},{"streamType",3},{"languageCode","spa"},{"key","/library/streams/9002.srt"},{"codec","srt"}});
    reset_session_subtitles();subtitle_selection.choice=1;begin_session_subtitles();service_subtitle_selection();
    assert(subtitle_external_pending && subtitle_attach_calls==1 && !subtitle_attach_reopen);
    native_subtitles={{23,"Spanish","es"}};service_subtitle_selection();assert(player_subtitle_track()==23 && !subtitle_pending);
    choose_subtitle_mode(0);assert(player_subtitle_track()==-42);
    subtitle_selection.choice=1;begin_session_subtitles();service_subtitle_selection();
    assert(player_subtitle_track()==23 && !subtitle_pending && subtitle_attach_calls==1);
    assert(position==before_switch_position+1000 && last_opened_url==before_switch_url && player_open_calls==before_switch_opens);
    reset_session_subtitles();assert(subtitle_attached.empty());
    video=false;native_subtitles.clear();detail_subtitles=false;
    sidebar=false;settings=false;in_detail=false;home_screen=false;available.clear();profile_screen=false;
    library_tools=true;tools_focus=9;frame();library_tools=false; // bottom of tool menu must stay in bounds
    for(int entry:{9,10,11}){sidebar=true;nav=entry;if(nav_visible(entry))assert(custom_focus_valid());}
    sidebar=false;
    // Periodic Home refresh waits for idle input, coalesces jobs and restores item identity.
    assert(job==None);base="https://fixture.invalid";server_token="profile";home_sections.clear();
    home_title="Home";home_screen=true;sidebar=false;settings=false;in_detail=false;editor=0;
    profile_screen=false;account_loading=false;library_context=false;search_screen=false;progress_pending=false;
    home_refresh_at=clock_now-1;last_input_at=clock_now;
    service_home_refresh(clock_now);assert(job==None);
    Item prior_card;prior_card.id="prior";prior_card.type="movie";Item kept_card=prior_card;kept_card.id="kept";
    feed_rows={{"continueWatching","Continue Watching",true,{prior_card,kept_card}}};feed_columns={1};feed_scroll={0};feed_focus=0;
    client=std::make_unique<Client>([](const Request &){return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"kept","type":"movie","title":"Kept","viewOffset":5},{"ratingKey":"prior","type":"movie","title":"Prior"}]}})",""};},"fixture");
    last_input_at=clock_now-9;service_home_refresh(clock_now);assert(job==HomeFeed);
    auto refresh_generation=feed_generation.load();service_home_refresh(clock_now);assert(feed_generation==refresh_generation);
    pending.wait();finish_job();assert(job==None && feed_rows[feed_focus].entries[feed_columns[feed_focus]].id=="kept");
    service_home_refresh(clock_now);assert(job==None && home_refresh_at>clock_now);
    // Categories are explicit directory routes; selecting a resulting movie opens details.
    home_screen=false;library_context=true;library_tab=2;active_library={};active_library.id="movies";active_library.title="Movies";active_library.type="movie";
    client=std::make_unique<Client>([](const Request &q){
        if(q.url.find("/genre")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Directory":[{"key":"17","title":"Drama"}]}})",""};
        if(q.url.find("/collections?")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"33","type":"collection","title":"Favorites"}]}})",""};
        return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"77","type":"movie","title":"Chosen movie"}]}})",""};
    },"fixture");
    activate_library_tab();assert(job==Browse);pending.wait();finish_job();assert(listing.size()==2 && listing[0].type=="category" && listing[1].type=="collection");
    open_item();assert(job==Browse);pending.wait();finish_job();assert(listing.size()==1 && listing[0].type=="movie");
    open_item();assert(job==Details);pending.wait();finish_job();assert(in_detail && detail.id=="77");
    in_detail=false;library_context=false;
    // Direct audio-item navigation starts its own request, never a competing default feed.
    Item album;album.id="book";album.type="album";album.section_id="music";
    Item music_section_item;music_section_item.id="music";music_section_item.type="artist";home_sections={music_section_item};
    open_audio_item(album);assert(job==AudioBrowse);pending.wait();finish_job();audio_leave_browse();
    // Recording-rule cancellation requires an explicit second confirmation.
    audio.screen=false;library_context=false;home_screen=false;sidebar=false;settings=false;in_detail=false;editor=0;reset_cloud();
    page_path="live-recordings";page_title="Live TV";live_rules_tab=true;live_cancel_id.clear();focus=0;
    live_subscriptions=Json::array({{{"id","77"},{"title","Record Series"}}});active_profile.admin=true;
    press(PAD_CROSS);assert(live_cancel_id=="77" && job==None);
    press(PAD_CIRCLE);assert(live_cancel_id.empty() && page_path=="live-recordings" && job==None);
    active_profile.admin=false;press(PAD_CROSS);assert(live_cancel_id.empty() && !live_error.empty() && job==None);
    press(PAD_CIRCLE);assert(page_path=="live-tv");
    // Library pins retain stable IDs through section reorder and remain profile-scoped.
    Item pin_a;pin_a.id="pin-a";pin_a.type="movie";Item pin_b=pin_a;pin_b.id="pin-b";
    home_sections={pin_a,pin_b};account_id="pin-owner";selected_server.id="pin-server";
    for(const auto &section:home_sections)pref_set((account_key("plex_pinned_library_v1")+"."+encode(selected_server.id)+"."+encode(section.id)).c_str(),1);
    pref_set(pin_order_key().c_str(),Json::array({"pin-b","pin-a"}).dump());
    auto pins=library_pin_order();assert(pins.size()==2 && home_sections[pins[0]].id=="pin-b");
    home_sections={pin_b,pin_a};pins=library_pin_order();assert(home_sections[pins[0]].id=="pin-b");
    account_id="different-pin-profile";assert(!nav_visible(12) && !nav_visible(13));account_id="pin-owner";
    settings=true;settings_page=10;focus=12;sidebar=false;press(PAD_CROSS);
    assert(!nav_visible(12) && !nav_visible(13));settings=false;
    active_profile.managed=true;profile_token="profile";request_cloud_check();assert(page_path=="cloud-pending" && listing.empty());
    service_cloud();assert(job==None && !cloud_ok && !cloud_error.empty());active_profile.managed=false;reset_cloud();
    // Every non-audio sidebar route closes the audio browsing layer before early return.
    reset_cloud();warm_discovery_pending=false;progress_pending=false;profile_screen=false;account_loading=false;editor=0;
    Item personal_section;personal_section.id="home-movies";personal_section.type="movie";personal_section.title="Home Movies";
    Item professional_section;professional_section.id="films";professional_section.type="movie";professional_section.title="Movies";
    home_sections={personal_section,professional_section};
    pref_set((account_key("plex_pinned_library_v1")+"."+encode(selected_server.id)+"."+encode(personal_section.id)).c_str(),1);
    client=std::make_unique<Client>([](const Request &){return Reply{200,R"({"MediaContainer":{"Metadata":[]}})",""};},"fixture");
    for(int target:{9,10,11,12}){
        audio.screen=true;audio.now_playing=true;sidebar=true;settings=true;in_detail=true;library_context=false;nav=target;page_path.clear();
        press(PAD_CROSS);if(job!=None){pending.wait();finish_job();}
        assert(!audio.screen && !audio.now_playing && !sidebar && !settings && !in_detail);
    }
    active_library=personal_section;library_context=true;search_screen=true;
    Item film;film.section_id="films";assert(!personal_item(film));
    film.section_id="home-movies";assert(personal_item(film));
    film.section_id.clear();assert(!personal_item(film));
    search_screen=false;library_context=false;
    puts("Subtitle controller settings: default Off, persistence, nonremembered reset, menu Off passed");
    unlink(credential_file.c_str());unlink((std::string(temp)+"/plexstation-listening.json").c_str());unlink((std::string(temp)+"/plexstation-progress.json").c_str()); rmdir(temp);
    puts("Authentication UI transitions passed: cancellation, approval, server selection, startup validation, retryable network failure, invalid-token clearing, sign-out");
    ui_shutdown();assert(!artwork && !client && job==None);
    ImGui::DestroyContext();
    puts("Controller UI scenarios passed: focus, D-pad, Cross, Back, repeat, analog, tabs, playback return, async responsiveness");
}
