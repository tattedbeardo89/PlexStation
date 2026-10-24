// SPDX-License-Identifier: GPL-3.0-or-later
// PlexStation controller-first interface; original VLC playback remains in player.cc.
#include "gfx.h"
#include "imgui.h"
#include "theme.h"
#include "player.h"
#include "plex/artwork.h"
#include "plex/client.h"
#include "plex/diagnostics.h"
#include "plex/home.h"
#include "plex/home_feed.h"
#include "plex/audio.h"
#include "plex/listening.h"
#include "plex/streaming.h"
#include "plex/subtitles.h"
#include "plex/navigation.h"
#include "plex/storage.h"
#include "plex/live_tv.h"
#include "plex/episodes.h"
#include "plex/progress_outbox.h"
#include "plex/search.h"
#include "plex/cloud.h"
#include "plex/music_feed.h"
#include "plex/auth.h"
#include "qrcodegen/qrcodegen.h"
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
#ifndef PLEX_BUILD_COMMIT
#define PLEX_BUILD_COMMIT "host-or-unversioned"
#endif
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
    bool home=false;
    Item previous_detail{}; bool show_details=false;
};
std::vector<Page> history;
std::string page_path, page_title = "Home";
Item detail, playing;
AuthSession auth;
uint64_t auth_request_generation = 0;
bool account_loading = false, cancel_bootstrap = false, credentials_saved = true, have_qr = false;
std::string account_error, account_id;
bool force_picker=false, reconnect_screen=false, discard_connection=false, change_after_connection=false;
bool controller_overlay=false;
bool library_context=false,library_tabs_focus=false;
int library_kind=1,library_tab=0;
Item active_library;
Json live_channel_list=Json::array(),live_program_list=Json::array();
Json live_channel;
bool live_guide_screen=false;
std::string live_error,live_cancel_id;
Json live_subscriptions=Json::array(),live_schedule=Json::array();
bool live_rules_tab=false;
void open_library(int kind);
void activate_library_tab();
bool warm_discovery_pending=false;
double startup_begin=0,profile_select_begin=0,home_refresh_at=0,last_input_at=0;
bool startup_cached_route=false,startup_warm_home=false,startup_home_drawn=false;
void service_home_refresh(double now);
int settings_page=0;
uint64_t screen_generation=0, pending_generation=0;
bool grid_checkpoint=false;
bool home_screen=false, library_tools=false, search_screen=false;
std::string home_title="Home",search_query;int search_start=0;
std::map<std::string,std::string> browser_filters;std::string browser_sort,filter_field;
std::vector<std::pair<std::string,std::string>> filter_values;bool filter_picker=false;
std::vector<Item> home_sections;
std::vector<Hub> feed_rows;
std::vector<int> feed_columns;
std::vector<float> feed_scroll;
int feed_focus=0, tools_focus=0, requested_section=0;
float feed_vertical=0;
std::string feed_error, details_target;
bool full_description=false;float description_scroll=0;
std::atomic<uint64_t> feed_generation{0};
std::string primary_avatar;
std::string primary_id, profile_token, profile_error, pin_digits;
Profile active_profile, requested_profile;
std::vector<Profile> home_profiles;
bool profile_screen=false, profile_pin=false, home_enabled=false, deferred_profiles=false;
int profile_focus=0, pin_focus=0;
void erase_pin() {std::fill(pin_digits.begin(),pin_digits.end(),'0');pin_digits.clear();}
std::string primary_key(const std::string &name) {return name+"."+encode(primary_id);}
std::string account_key(const std::string &name) { return name + "." + encode(account_id); }
std::string account_pref(const std::string &name, const char *fallback="") {
    return account_id.empty() ? fallback : pref_str(account_key(name).c_str(),fallback);
}
void save_account_pref(const std::string &name,const std::string &value) {
    if(!account_id.empty()) pref_set(account_key(name).c_str(),value);
}
void configure_artwork_scope(const std::string &scope) {
    auto max_mb=std::clamp(pref_int("plex_artwork_cache_mb_v1",256),64,1024);
    artwork->set_scope(cache_root(plat_data_dir())+"/plexstation-artwork-cache",scope,static_cast<size_t>(max_mb)*1024*1024);
}
uint8_t qr[qrcodegen_BUFFER_LEN_MAX];
double overlay_until = 0;
int focus = 0, nav = 0, track_focus = 0, global_delay = 0, page_start = 0, page_total = 0,
    version = 0;
int editor = 0, edit_focus = 0;
std::string edit_text;
const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .:-_/[]";
bool sidebar = true, in_detail = false, settings = false, tracks = false, video = false,
     allow_http = false;
ControllerInput controller;
std::future<Result> pending;
enum Job {
    None,
    PinStart,
    PinPoll,
    ValidateAccount,
    HomeFeed, LibraryFeed, LiveChannels, LiveGuide, LiveTune, LiveRecordings, LiveCancel,
    HomeUsers,
    HomeSwitch,
    ValidateProfile,
    Resources, FastConnect, BackgroundResources,
    Select,
    Manual,
    Sections,
    Browse,
    Details,
    Timeline,
    AudioBrowse, AudioQueueLoad, AudioResolve, AudioScrobble, AudioFeed,
    VideoPlan, Cleanup, NextEpisode, GlobalSearch, FilterValues,
    CloudProbe,CloudList,CloudResolve,CloudChange,
    Watch
};
Job job = None;
std::string next_base, request_path, request_title;
int request_start=0; bool request_push=false;
Server selected_server, connection_target;
std::vector<ConnectionAttempt> connection_attempts, next_attempts;
bool connection_diagnostics=false, manual_confirm_http=false, manual_tested=false;
int diagnostic_focus=0;
Json manual_sections;
std::string manual_candidate;
void submit(Job kind, std::function<Result()> work);
void load_home();
void activate_profile();
void choose_profile();
void show_profiles();
bool cloud_available();
void reset_cloud();
void service_cloud();
void request_cloud_check();
void show_cloud(int start=0);
void finish_cloud(int kind,const Result &result);
void change_cloud(bool add);
void open_media(const Item &item);
void restore_video_preferences();
bool playback_tracks_focus_valid();
void prepare_subtitle_selection();
void service_subtitle_selection();
void subtitle_picker_frame(uint32_t);
bool detail_subtitles=false;
void load_outbox();
void record_progress(const Item &,const std::string &,int64_t);
void capture_timeline(const Item &,const std::string &,int64_t);
void timeline_complete(const Result &);
void retry_progress();
void request_video(bool force=false);
void service_next();
void service_video_retry();
void clear_next();
void next_ready(const Result &r);
void offer_autoplay(double now);
bool next_countdown_frame(uint32_t hit,double now);
void mark_video_completion(int64_t position);
void video_plan_ready(const Result &result);
void apply_video_plan(const StreamPlan &);
void retire_stream();
void service_cleanup();
void video_tools(uint32_t hit);
void video_options_frame(uint32_t hit);
void open_audio_item(const Item &item);
void audio_reset();
void audio_leave_browse();
int audio_focus_state();
void audio_finish(int kind,const Result &result);
void audio_tick(double now);
bool audio_frame(uint32_t hit,float dt);
bool audio_available(bool books=false);
void audio_home(bool books=false,bool load_default=true);
void audio_library_tab(int tab);
void library_tabs_frame();
void test_manual() {
    auto target=connection_target;
    target.connections={{manual_candidate,true,false}};
    manual_tested=false; next_attempts.clear(); next_base.clear();
    bool http=allow_http;
    submit(Manual,[target,http]{return client->select(target,http,next_base,&next_attempts);});
}


std::string progress_state;
int64_t progress_time = 0;
bool progress_pending = false;
Progress progress;
Item progress_item;
const char *navigation[] = {"Home",      "Movies", "TV Shows", "Continue Watching",
                            "Watchlist", "Search", "Settings", "Music", "Audiobooks", "Home Movies", "Live TV", "Rentals"};
bool nav_visible(int i) {
    if(i>=12){size_t index=i-12;if(index>=home_sections.size())return false;const auto &item=home_sections[index];return (item.type=="movie" || item.type=="show") && pref_int((account_key("plex_pinned_library_v1")+"."+encode(selected_server.id)+"."+encode(item.id)).c_str(),0)!=0;}
    if(i<0)return false;
    if(i!=0 && i!=6 && (pref_int(account_key("plex_sidebar_hidden_v1").c_str(),0)&(1<<i)))return false;
    if(i==9)return std::any_of(home_sections.begin(),home_sections.end(),personal_library);
    if((i==1 || i==2) && !selected_server.id.empty()) {
        const char *kind=i==1?"movie":"show";
        if(std::none_of(home_sections.begin(),home_sections.end(),[&](const Item &section){return section.type==kind;}))return false;
    }
    return (i!=7 || audio_available()) && (i!=8 || audio_available(true));
}
std::string pin_order_key(){return account_key("plex_sidebar_pins_v1")+"."+encode(selected_server.id);}
std::vector<int> library_pin_order(){
    std::vector<int> order;auto saved=Json::parse(pref_str(pin_order_key().c_str(),"[]"),nullptr,false);
    if(saved.is_array())for(const auto &id:saved)if(id.is_string())for(size_t i=0;i<home_sections.size();++i)if(home_sections[i].id==id.get<std::string>() && std::find(order.begin(),order.end(),static_cast<int>(i))==order.end())order.push_back(i);
    for(size_t i=0;i<home_sections.size();++i)if(std::find(order.begin(),order.end(),static_cast<int>(i))==order.end())order.push_back(i);
    return order;
}
std::vector<int> sidebar_order() {
    std::vector<int> order;
    auto json=Json::parse(pref_str(account_key("plex_sidebar_order_v1").c_str(),"[]"),nullptr,false);
    if(json.is_array())for(const auto &value:json)if(value.is_number_integer()){auto n=value.get<int>();if(n>=0 && n<12 && std::find(order.begin(),order.end(),n)==order.end())order.push_back(n);}
    for(int n:{0,4,1,2,9,8,10,11,7,5,6,3})if(std::find(order.begin(),order.end(),n)==order.end())order.push_back(n);
    for(int i:library_pin_order())if(nav_visible(12+i))order.push_back(12+i);
    return order;
}
std::string nav_label(int i){return i<12?navigation[i]:home_sections[i-12].title;}
int nav_step(int i,int direction,bool wrap=false) {
    auto order=sidebar_order();auto it=std::find(order.begin(),order.end(),i);
    int at=i<0?-1:it==order.end()?0:static_cast<int>(it-order.begin());
    for(size_t n=0;n<=order.size();++n){at+=direction;if(wrap)at=(at+static_cast<int>(order.size()))%order.size();else if(at<0)return -1;else if(at>=static_cast<int>(order.size()))return i;auto next=order[at];if(nav_visible(next))return next;}
    return 0;
}
void submit(Job kind, std::function<Result()> work) {
    if (job != None)
        return;
    job = kind; pending_generation=screen_generation;
    pending = std::async(std::launch::async, std::move(work));
}
void load(const std::string &path, const std::string &title, bool push = true, int start = 0) {
    if (job != None)
        return;
    checkpoint(2,"request preparation");
    grid_checkpoint=false;
    fprintf(stderr,"browse: begin start=%d history=%zu\n",start,history.size());
    request_path = path;
    request_title=title; request_start=start; request_push=push;
    auto b = base, t = server_token;
    if(library_context && path=="/library/sections/"+encode(active_library.id)+"/categories"){
        auto section=active_library;submit(Browse,[b,t,section,start]{return library_categories(*client,b,t,section,start,60);});
    }else submit(Browse, [b, t, path, start] { return client->browse(b, t, path, start); });
}
Result restore_presentation(Result result,const std::string &root,const std::string &scope,const std::string &server) {
    if(!result)return result;
    std::vector<std::string> ids;for(const auto &section:items(result.data,false))ids.push_back(section.id);
    std::sort(ids.begin(),ids.end());std::string key=scope+"/"+server;for(const auto &id:ids)key+="/"+id;
    DiskCache cache(root+"/plexstation-home-cache",key,4u*1024*1024);
    auto bytes=cache.get("home",1024*1024);auto saved=Json::parse(bytes,nullptr,false);
    if(!saved.is_discarded())result.data["_presentation"]=std::move(saved);
    return result;
}
void connect_server(const Server &s) {
    if(job!=None) return;
    home_reset();home_checkpoint(3,"server selection accepted");
    connection_target=s; next_attempts.clear(); next_base.clear(); manual_tested=false;
    connection_diagnostics=false; reconnect_screen=true; discard_connection=false;
    message.clear(); bool http=allow_http;
    auto root=cache_root(plat_data_dir()),scope=account_id;
    auto preferred=s.id==account_pref("plex_preferred_server")?account_pref("plex_verified_endpoint_v2"):"";
    submit(Select,[s,http,root,scope,preferred]{return restore_presentation(client->select(s,http,next_base,&next_attempts,preferred),root,scope,s.id);});
}
DiskCache home_presentation_cache;
std::string presentation_scope() {
    std::vector<std::string> ids;for(const auto &section:home_sections)ids.push_back(section.id);
    std::sort(ids.begin(),ids.end());std::string scope=account_id+"/"+selected_server.id;
    for(const auto &id:ids)scope+="/"+id;
    return scope;
}
void connected(const Json &data) {
    home_checkpoint(6,"authenticated libraries received",0,items(data,false).size());
    home_active=false;
    reset_cloud();
    live_cancel_id.clear();live_subscriptions=Json::array();live_schedule=Json::array();
    clear_next();audio_reset();restore_video_preferences();
    base=next_base; selected_server=connection_target; server_token=connection_target.token;
    save_account_pref("plex_preferred_server",selected_server.id);
    save_account_pref("plex_verified_endpoint_v2",base);
    save_account_pref("plex_verified_server_name_v2",selected_server.name);
    save_account_pref("plex_verified_at_v2",std::to_string(std::time(nullptr)));
    bool local=false;for(const auto &route:selected_server.connections)if(route.uri==base)local=route.local;
    pref_set(account_key("plex_verified_local_v2").c_str(),local?1:0);
    fprintf(stderr,"startup: authorized-server-ms=%d\n",startup_begin>0?static_cast<int>((plat_time()-startup_begin)*1000):-1);
    fprintf(stderr,"home:init persistent progress begin\n");fflush(stderr);
    load_outbox();
    fprintf(stderr,"home:init persistent progress completed\n");fflush(stderr);
    configure_artwork_scope(account_id+"/server:"+selected_server.id); history.clear(); detail={}; playing={};
    progress_pending=false; progress={};
    available.clear(); settings=false; settings_page=0; in_detail=false; sidebar=false; focus=0; nav=0;
    reconnect_screen=false; connection_diagnostics=false; editor=0;
    listing=items(data,false); page_path="/library/sections"; page_title="Libraries";
    page_start=0; page_total=listing.size();
    home_refresh_at=plat_time()+120;startup_warm_home=false;
    home_sections=listing;
    auto personal_id=pref_str((account_key("plex_default_library_v1")+"."+encode(selected_server.id)+".9").c_str(),"");
    for(auto &section:home_sections)if(section.id==personal_id && section.type=="movie")section.personal=true;
    home_screen=true;feed_rows.clear();feed_columns.clear();feed_scroll.clear();feed_focus=0;feed_vertical=0;
    if(listing.empty()) message="Connected; no accessible libraries were returned.";
    if(diagnostic(SafeHome)){home_screen=false;message="Safe startup: automatic Home loading paused. Options opens Developer Options.";}
    else {
        home_presentation_cache=DiskCache(cache_root(plat_data_dir())+"/plexstation-home-cache",presentation_scope(),4u*1024*1024);
        auto cached=data.find("_presentation");
        if(cached!=data.end()) {
            feed_rows=home_rows(*cached);startup_warm_home=!feed_rows.empty();feed_columns.assign(feed_rows.size(),0);feed_scroll.assign(feed_rows.size(),0);
            message="Refreshing your Home…";
        }
        load_home();
    }
}
int recommendation_month(){int mode=std::clamp(pref_int(account_key("plex_seasonal_mode_v1").c_str(),0),0,4);return mode==1?10:mode==2?12:mode==3?1:mode==4?7:season_month();}
void load_home() {
    library_context=false;
    if(job!=None || base.empty())return;
    search_screen=false;home_title="Home";
    home_refresh_at=plat_time()+120;
    auto b=base,t=server_token;auto sections=home_sections;
    home_active=true;home_draw_ready=false;home_checkpoint(7,"Home hubs requested");
    auto epoch=++feed_generation;feed_error.clear();home_screen=true;auto cache=home_presentation_cache;int month=recommendation_month();bool seasonal=pref_int(account_key("plex_seasonal_v1").c_str(),1)!=0;
    submit(HomeFeed,[b,t,sections,epoch,cache,seasonal,month] {auto current=[epoch]{return feed_generation.load()==epoch;};auto result=home_feed(*client,b,t,sections,current);if(seasonal)append_seasonal_home(result,*client,b,t,sections,month,current);if(result && feed_generation.load()==epoch)cache.put("home",result.data.dump());return result;});
}
void service_home_refresh(double now) {
    if(now<home_refresh_at || now-last_input_at<8 || job!=None || progress_pending || base.empty() ||
       account_loading || profile_screen || sidebar || settings || in_detail || editor || video ||
       !home_screen || library_context || search_screen || home_title!="Home")return;
    load_home(); // Keep existing rows; completion reconciles focused row/item IDs.
}
void refresh() {
    if(!primary_id.empty() && profile_token.empty()) {profile_screen=true;profile_error="Select and authorize a profile first.";return;}
    auto token = primary_id.empty() ? account : profile_token;
    submit(Resources, [token] { return client->resources(token); });
}
void clear_profile() {
    home_active=false;home_draw_ready=false;
    live_cancel_id.clear();live_subscriptions=Json::array();live_schedule=Json::array();
    warm_discovery_pending=false;library_context=false;active_library={};live_channel_list=Json::array();live_program_list=Json::array();live_channel={};live_error.clear();live_guide_screen=false;
    reset_cloud();
    clear_next();
    retire_stream();
    audio_reset();
    if(video) {player_stop();video=false;}
    artwork->clear();
    if(!primary_id.empty())configure_artwork_scope(primary_id+"/avatars");
            artwork->set_avatar_scope(cache_root(plat_data_dir())+"/plexstation-avatar-cache",primary_id);
    ++screen_generation; ++feed_generation;
    home_screen=false;home_sections.clear();feed_rows.clear();feed_columns.clear();feed_scroll.clear();feed_focus=0;feed_error.clear();library_tools=false;
    profile_pin=false;requested_profile={};
    profile_token.clear(); active_profile={}; account_id.clear(); base.clear(); server_token.clear();
    selected_server={};connection_target={}; available.clear();listing.clear();history.clear();detail={};playing={};
    full_description=false;description_scroll=0;
    progress_pending=false;progress={};tracks=false;in_detail=false;settings=false;editor=0;
    reconnect_screen=false;connection_diagnostics=false;manual_tested=false;manual_sections={};
    force_picker=false;focus=0;sidebar=false;erase_pin();
}
void show_profiles() {
    profile_screen=true; profile_pin=false; profile_error.clear();profile_focus=0;erase_pin();
    if(job!=None) {deferred_profiles=true;++screen_generation;++feed_generation;return;}
    clear_profile();
    if(home_profiles.empty() && home_enabled) {auto token=account;submit(HomeUsers,[token]{return client->home_users(token);});}
}
void activate_profile() {
    home_checkpoint(2,"active profile identity validated");
    active_profile=requested_profile;
    account_id=profile_scope(primary_id,active_profile);
    // Only the validated original owner may inherit its pre-profile preference.
    if("id:"+active_profile.id==primary_id) {
        for(const char *key:{"plex_preferred_server","plex_manual_uri","plex_manual_server_id"})
            if(account_pref(key).empty()) {
                auto old=pref_str((std::string(key)+"."+encode(primary_id)).c_str(),"");
                if(!old.empty())save_account_pref(key,old);
            }
    }
    pref_set(primary_key("plex_last_profile").c_str(),active_profile.id);
    global_delay=std::clamp(pref_int(account_key("plex_audio_delay").c_str(),0),-500,500);
    restore_video_preferences();
    profile_screen=false;profile_pin=false;profile_error.clear();erase_pin();
    account_loading=true;
    startup_begin=profile_select_begin>0?profile_select_begin:plat_time();
    startup_cached_route=false;startup_warm_home=false;startup_home_drawn=false;
    auto saved=account_pref("plex_verified_endpoint_v2");
    auto id=account_pref("plex_preferred_server");
    if(!saved.empty() && !id.empty() && valid_base(saved,allow_http)) {
        connection_target={};connection_target.id=id;connection_target.name=account_pref("plex_verified_server_name_v2");connection_target.token=profile_token;
        connection_target.connections={{saved,pref_int(account_key("plex_verified_local_v2").c_str(),0)!=0,false}};
        next_base=saved;auto token=profile_token;
        auto root=cache_root(plat_data_dir()),scope=account_id;
        submit(FastConnect,[saved,id,token,root,scope]{
            auto network=PLEX_TRANSPORT();
            Client fast([network](const Request &request){auto q=request;q.connect_timeout_ms=800;q.timeout_ms=1400;return network(q);},device);
            auto identity=fast.identity(saved,token,id);
            return restore_presentation(identity?fast.sections(saved,token):identity,root,scope,id);
        });
    } else refresh();
}
void choose_profile() {
    home_checkpoint(1,"profile selection received");
    if(job!=None)return;
    profile_select_begin=plat_time();
    if(requested_profile.protected_user && !valid_pin(pin_digits)) {profile_pin=true;pin_focus=0;erase_pin();return;}
    if(!home_enabled) {profile_token=account;activate_profile();return;}
    auto token=account,id=requested_profile.id,secret=pin_digits;erase_pin();
    submit(HomeSwitch,[token,id,secret]() mutable {
        auto result=client->switch_home(token,id,secret);std::fill(secret.begin(),secret.end(),'0');return result;
    });
}
void profiles_ready() {
    profile_screen=true;account_loading=false;profile_focus=0;
    auto last=pref_str(primary_key("plex_last_profile").c_str(),"");
    if(!pref_int(primary_key("plex_show_profiles").c_str(),1)) {
        auto found=std::find_if(home_profiles.begin(),home_profiles.end(),[&](const Profile &p){return p.id==last;});
        if(found!=home_profiles.end()) {requested_profile=*found;if(found->protected_user){profile_pin=true;erase_pin();pin_focus=0;}else choose_profile();}
    }
}
void signout() {
    if (job != None)
        return;
    if (video) {
        player_stop();
        video = false;
    }
    reset_cloud();clear_next();retire_stream();audio_reset();
    artwork->clear();
    profile_token.clear();primary_id.clear();primary_avatar.clear();active_profile={};requested_profile={};home_profiles.clear();profile_screen=false;profile_pin=false;deferred_profiles=false;erase_pin();
    account.clear(); account_id.clear(); reconnect_screen=false; force_picker=false; settings_page=0;
    server_token.clear();
    base.clear();
    available.clear();
    connection_attempts.clear(); selected_server={}; connection_target={};
    connection_diagnostics=false; manual_tested=false; manual_confirm_http=false; editor=0;
    listing.clear();
    history.clear();
    auth.cancel();
    auth.state = AuthState::Idle;
    have_qr = false;
    account_loading = false;
    account_error.clear();
    in_detail = false;
    settings = false;
    focus = 0;
    sidebar = true;
    if (!save_credentials(credential_file, device, "")) {
        // Revocation must not leave an old token to be restored after restart.
        if (unlink(credential_file.c_str()) != 0)
            auth.fail("Sign-out could not remove saved credentials. Check storage permissions.");
        message = "Sign-out persistence failed; check storage permissions";
    }
    else
        message = "Signed out. Cross to sign in";
}
void queue_progress(const std::string &state, int64_t position) {
    if(playing.type=="live")return;
    record_progress(playing,state,position);
    progress_item=playing;
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
    if(deferred_profiles) {deferred_profiles=false;show_profiles();return;}
    if((done==Browse || done==Sections || done==Details || done==HomeFeed || done==LibraryFeed || done==AudioBrowse || done==AudioQueueLoad || done==AudioResolve || done==AudioFeed) && pending_generation!=screen_generation) return;
    if(done==LiveRecordings || done==LiveCancel){
        if(pending_generation!=screen_generation)return;
        live_cancel_id.clear();
        if(!result){live_error=result.error;return;}
        live_error.clear();focus=0;
        if(done==LiveCancel){message="Recording rule cancellation accepted by PMS";auto b=base,t=server_token;submit(LiveRecordings,[b,t]{return live_recordings(*client,b,t);});}
        else {live_subscriptions=result.data["subscriptions"];live_schedule=result.data["scheduled"];}
        return;
    }
    if(done==LiveChannels || done==LiveGuide || done==LiveTune) {
        if(pending_generation!=screen_generation)return;
        if(!result){live_error=result.error;return;}
        live_error.clear();focus=0;
        if(done==LiveChannels){auto list=result.data.find("channels");live_channel_list=list!=result.data.end() && list->is_array()?*list:Json::array();live_guide_screen=false;}
        else if(done==LiveGuide){auto list=result.data.find("programs");live_program_list=list!=result.data.end() && list->is_array()?*list:Json::array();live_guide_screen=true;}
        else {detail={};detail.id=text(result.data,"session");detail.type="live";detail.title=text(live_channel,"title","Live TV");apply_video_plan({{Mode::DirectPlay,text(result.data,"url"),""},"","Personal Live TV / HLS requested"});}
        return;
    }
    if(done==VideoPlan){if(pending_generation==screen_generation)video_plan_ready(result);return;}
    if(done==CloudProbe || done==CloudList || done==CloudResolve || done==CloudChange){if(pending_generation==screen_generation)finish_cloud(done,result);return;}
    if(done==GlobalSearch){if(pending_generation==screen_generation){if(result){library_context=false;feed_rows=home_rows(result.data);feed_columns.assign(feed_rows.size(),0);feed_scroll.assign(feed_rows.size(),0);feed_focus=0;home_screen=true;search_screen=true;page_title="Search";}else message=result.error;}return;}
    if(done==FilterValues){
        filter_values.clear();filter_picker=false;
        if(!result){message=result.error;return;}
        auto mc=result.data.find("MediaContainer");if(mc!=result.data.end() && mc->is_object()){
            auto dirs=mc->find("Directory");if(dirs!=mc->end() && dirs->is_array())for(const auto &d:*dirs){auto key=text(d,"key"),title=text(d,"title");if(!key.empty() && key.size()<256 && !title.empty() && title.size()<256)filter_values.push_back({title,key});if(filter_values.size()==200)break;}
        }
        filter_picker=true;tools_focus=0;return;
    }
    if(done==Timeline){timeline_complete(result);return;}
    if(done==Cleanup)return;
    if(done==NextEpisode){if(pending_generation==screen_generation)next_ready(result);return;}
    if(done==AudioBrowse || done==AudioQueueLoad || done==AudioResolve || done==AudioScrobble || done==AudioFeed) {audio_finish(done,result);return;}
    if ((done == PinStart || done == PinPoll) && auth_request_generation != auth.generation)
        return; // A cancelled request must never authorize a later session.
    if (cancel_bootstrap && (done == ValidateAccount || done == HomeUsers || done == Resources || done==FastConnect)) {
        cancel_bootstrap = false; signout(); return;
    }
    if (done == PinStart || done == PinPoll) {
        if (done == PinStart) {
            auth.created(result, plat_time());
            have_qr = false;
            if (auth.state == AuthState::Waiting) {
                uint8_t scratch[qrcodegen_BUFFER_LEN_MAX];
                auto url = authorization_url(device, auth.current.code);
                have_qr = qrcodegen_encodeText(url.c_str(), scratch, qr, qrcodegen_Ecc_MEDIUM,
                    qrcodegen_VERSION_MIN, 15, qrcodegen_Mask_AUTO, true);
                if (!have_qr && !auth.short_code())
                    auth.fail("Could not prepare browser sign-in. Press Cross for a new code.");
            }
        } else auth.polled(result, plat_time());
        if (auth.state == AuthState::Authorized) {
            account = std::move(auth.current.token);
            auth.current.token.clear(); have_qr = false;
            credentials_saved = save_credentials(credential_file, device, account);
            account_loading = true; account_error.clear();
            auto token=account;
            submit(ValidateAccount,[token]{return client->validate_account(token);});
        }
        return;
    }
    if((done==FastConnect || done==BackgroundResources) && pending_generation!=screen_generation)return;
    if(done==FastConnect) {
        account_loading=false;
        if(!result){fprintf(stderr,"startup: cached-route=fallback status=%d\n",result.status);refresh();return;}
        startup_cached_route=true;warm_discovery_pending=true;connected(result.data);return;
    }
    if(done==BackgroundResources) {
        if(result)for(const auto &server:servers(result.data))if(server.id==selected_server.id && !server.token.empty()) {selected_server=server;server_token=server.token;break;}
        return; // background refresh never changes focus or navigation
    }
    if (done == ValidateAccount) {
        if (result) {
            auto id=number(result.data,"id",0);
            if(id<=0) { account_error="Plex account identity is missing. Retry sign-in validation."; return; }
            clear_profile();primary_id="id:"+std::to_string(id);
            primary_avatar=profile_avatar(result.data);
            configure_artwork_scope(primary_id+"/avatars");
            artwork->set_avatar_scope(cache_root(plat_data_dir())+"/plexstation-avatar-cache",primary_id);
            home_enabled=flag(result.data,"home",true); // missing/unknown must not bypass Home PIN discovery
            account_error.clear();profile_error.clear();home_profiles.clear();
            if(home_enabled) {profile_screen=true;account_loading=false;auto token=account;submit(HomeUsers,[token]{return client->home_users(token);});}
            else {
                home_profiles.push_back({std::to_string(id),text(result.data,"uuid"),text(result.data,"title",text(result.data,"username","Plex account")),text(result.data,"thumb"),false,true,false});
                profiles_ready();
            }
        }
        else if (result.status == 401 || result.status == 403) {
            signout();
            if (auth.state != AuthState::Failed)
                auth.fail("Saved sign-in is no longer valid. Press Cross to sign in again.");
        } else account_error = result.error;
        return;
    }
    if(done==HomeFeed || done==LibraryFeed) {
        home_checkpoint(8,"Home hub response ready",result?0:(result.status?result.status:-1));
        if(!result) {feed_error=result.error;home_refresh_at=plat_time()+45;return;}
        auto updated=home_rows(result.data);std::vector<int> columns(updated.size(),0);std::vector<float> scroll(updated.size(),0);
        std::string selected_row=feed_focus>=0 && feed_focus<static_cast<int>(feed_rows.size())?feed_rows[feed_focus].id:"";
        for(size_t i=0;i<updated.size();++i)for(size_t j=0;j<feed_rows.size();++j)if(updated[i].id==feed_rows[j].id){
            if(updated[i].id==selected_row)feed_focus=i;
            if(j<feed_columns.size() && feed_columns[j]<static_cast<int>(feed_rows[j].entries.size())){const auto &id=feed_rows[j].entries[feed_columns[j]].id;for(size_t n=0;n<updated[i].entries.size();++n)if(updated[i].entries[n].id==id)columns[i]=n;}
            if(j<feed_scroll.size())scroll[i]=feed_scroll[j];
        }
        feed_rows=std::move(updated);feed_columns=std::move(columns);feed_scroll=std::move(scroll);
        grid_checkpoint=false;feed_focus=std::clamp(feed_focus,0,std::max(0,static_cast<int>(feed_rows.size())-1));home_screen=true;
        home_checkpoint(9,"Home UI populated",0,feed_rows.size());
        auto warnings=result.data.find("warnings");
        if(warnings!=result.data.end() && warnings->is_array() && !warnings->empty() && (*warnings)[0].is_string())feed_error=(*warnings)[0].get<std::string>();
        return;
    }
    if(done==HomeUsers) {
        if(!result) {
            if(result.status==401 || result.status==403) {signout();auth.fail("Primary account authorization expired. Sign in again.");}
            else profile_error=result.error;
            return;
        }
        home_profiles=profiles(result.data);
        for(size_t i=0;i<home_profiles.size();++i) {
            auto &profile=home_profiles[i];
            if(profile.avatar.empty() && "id:"+profile.id==primary_id)profile.avatar=primary_avatar;
            fprintf(stderr,"avatar: profile_slot=%zu source_present=%d\n",i,profile.avatar.empty()?0:1);
        }
        profiles_ready();return;
    }
    if(done==HomeSwitch) {
        if(!result) {profile_error=(result.status==401 || result.status==403) ? "Incorrect PIN or profile access denied. Retry or select another user." : result.error;erase_pin();return;}
        profile_token=text(result.data,"authToken");auto token=profile_token;
        submit(ValidateProfile,[token]{return client->validate_account(token);});return;
    }
    if(done==ValidateProfile) {
        if(!result || profile_id(result.data)!=requested_profile.id) {profile_token.clear();profile_error="Profile authorization could not be verified. Select the profile and retry.";erase_pin();return;}
        activate_profile();return;
    }
    if (done == Resources && !result && (result.status == 401 || result.status == 403)) {
        account_loading=false;show_profiles();profile_error="This profile session expired or lost access. Select a profile to retry.";
        return;
    }
    if (done==Select || done==Manual) {
        connection_attempts=next_attempts; diagnostic_focus=0;
        for(const auto &attempt:connection_attempts)
            fprintf(stderr,"conn: %s route=%s protocol=%s port=%u TCP=%s TLS=%s identity=%s libraries=%s result=%s\n",
                attempt.endpoint.c_str(),attempt.route.c_str(),attempt.protocol.c_str(),attempt.port,
                attempt.tcp.c_str(),attempt.tls.c_str(),attempt.identity.c_str(),attempt.libraries.c_str(),attempt.result.c_str());
        if(done==Select && discard_connection) {
            discard_connection=false; reconnect_screen=false; message="Connection cancelled; choose a server.";
            if(change_after_connection) { change_after_connection=false; force_picker=true; refresh(); }
            return;
        }
        if(done==Manual) connection_diagnostics=true;
        if(done==Manual && result) {
            manual_sections=result.data; manual_tested=true;
            message="Manual endpoint verified. Cross saves and connects; Circle returns to editing.";
            return;
        }
    }
    if (!result) {
        message = result.error;
        if (done == Resources && account_loading) account_error = result.error;
        if (done == Timeline) {
            progress_pending = false;
            message = "Progress could not be saved; playback remains available";
        }
        return;
    }
    message.clear();
    if (done == Resources) {
        if (!result.data.is_array()) {
            account_error = "Plex returned an invalid server list. Press Cross to retry.";
            message = account_error;
            return;
        }
        account_loading = false; account_error.clear();
        if (!credentials_saved)
            message = "Signed in for this session; saving failed. Check account storage permissions.";
        home_screen=false;
        available = servers(result.data);
        auto saved_uri=account_pref("plex_manual_uri"), saved_id=account_pref("plex_manual_server_id");
        if(valid_base(saved_uri,true))
            for(auto &server:available) if(server.id==saved_id)
                server.connections.insert(server.connections.begin(),{saved_uri,true,false});
        listing.clear();
        focus = 0;
        sidebar = false;
        page_title = "Select server";
        settings = false;
        in_detail = false;
        auto preferred=account_pref("plex_preferred_server");
        bool automatic=!force_picker; force_picker=false;
        if(automatic && !preferred.empty()) {
            auto found=std::find_if(available.begin(),available.end(),[&](const Server &s){return s.id==preferred;});
            if(found!=available.end()) {connect_server(*found); return;}
            message="Your preferred server is not available. Choose a server or refresh from Settings.";
        }
        if (available.empty() && preferred.empty())
            message = credentials_saved ? "No accessible Plex Media Servers" :
                "No servers found. Sign-in could not be saved; check storage permissions.";
    } else if (done == Select) {
        connected(result.data);
    } else if (done == Sections || done == Browse) {
        if(done==Browse) {
            if(request_push) {if(history.size()>=24)history.erase(history.begin());history.push_back({page_path,page_title,listing,focus,page_start,page_total,home_screen,in_detail?detail:Item{},in_detail});}
            home_screen=false;
            page_path=request_path; page_title=request_title; page_start=request_start;
        }
        fprintf(stderr,"browse: response parse begin\n");
        listing = items(result.data,false);
        fprintf(stderr,"browse: parsed items=%zu\n",listing.size());
        checkpoint(10,"ui state replaced",listing.size());
        grid_checkpoint=false;
        focus = 0;
        in_detail = false;
        auto c = result.data.find("MediaContainer");
        page_total = static_cast<int>(std::clamp<int64_t>(c != result.data.end() ? number(*c,"totalSize",listing.size()) : 0,0,INT32_MAX));
        if (listing.empty())
            message = "No items in this section";
        if (done == Sections) {
            home_screen=false;history.clear();
            if (requested_section == 1 || requested_section == 2) {
                const std::string type = requested_section == 1 ? "movie" : "show";
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
        if (parsed.empty() || (!details_target.empty() && parsed.front().id!=details_target)) {
            message = "Metadata is unavailable";
            return;
        }
        detail = parsed.front();
        detail_subtitles=false;
        in_detail = true;
        focus = 0;
        version = 0;
    }
}
std::string library_preference(int kind){return account_key("plex_default_library_v1")+"."+encode(selected_server.id)+"."+std::to_string(kind);}
void activate_library_tab() {
    if(job!=None || active_library.id.empty())return;
    sidebar=false;in_detail=false;focus=0;history.clear();
    if(music_section(active_library)){audio_library_tab(library_tab);return;}
    auto path="/library/sections/"+encode(active_library.id);
    if(library_tab==1){load(path+"/all",active_library.title,false);return;}
    if(library_tab==2){load(path+"/categories",active_library.title+" / Categories",false);return;}
    auto b=base,t=server_token;auto section=active_library;int month=recommendation_month();bool seasonal=pref_int(account_key("plex_seasonal_v1").c_str(),1)!=0;
    home_screen=true;home_title=section.title+" / Recommended";feed_rows.clear();feed_focus=0;feed_vertical=0;
    submit(LibraryFeed,[b,t,section,seasonal,month]{return library_feed(*client,b,t,section,seasonal,month);});
}
void open_library(int kind) {
    library_kind=kind;browser_filters.clear();browser_sort.clear();
    active_library=default_library(home_sections,kind,pref_str(library_preference(kind).c_str(),""));
    if(active_library.id.empty()){message="No accessible library is available for this section";return;}
    pref_set(library_preference(kind).c_str(),active_library.id);
    library_context=true;library_tabs_focus=false;library_tab=0;activate_library_tab();
}
void open_media(const Item &item) {
    if(job!=None)return;
    if(item.type=="track" || item.type=="album" || item.type=="artist" || item.type=="playlist"){open_audio_item(item);return;}
    if(item.type=="collection"){load("/library/collections/"+encode(item.id)+"/children",item.title);return;}
    if(item.type=="movie" || item.type=="episode" || item.type=="show" || item.type=="season") {
        details_target=item.id;
        auto b=base,t=server_token,path="/library/metadata/"+encode(item.id);
        submit(Details,[b,t,path]{return client->call("GET",b,path,t);});
    }
}
void open_item() {
    if (job != None || listing.empty() || focus<0 || focus>=static_cast<int>(listing.size()))
        return;
    Item item = listing[focus];
    if(page_path=="cloud-watchlist"){auto b=base,t=server_token;submit(CloudResolve,[b,t,item]{return resolve_cloud(*client,b,t,item);});return;}
    if(item.type=="category"){browser_filters.clear();browser_sort.clear();load(item.key,active_library.title+" / "+item.title);return;}
    if(item.type=="collection"){open_media(item);return;}
    if (page_path == "/library/sections") {
        diagnostic_lines=0; checkpoint(1,"library selection");
        browser_filters.clear();browser_sort.clear();
        load("/library/sections/" + encode(item.id) + "/all", item.title);
    } else if(item.type=="show" || item.type=="season" || item.type=="movie" || item.type=="episode")open_media(item);
    else message="This media type is not supported by the current playback adapter.";
}
void play() {request_video();}

void stop() {
    retire_stream();
    auto position = player_time();
    mark_video_completion(position);
    queue_progress("stopped", position);
    detail.offset = position;
    player_stop();
    video = false;
    tracks = false;
    in_detail = playing.type!="live";
    sidebar = false;
    focus = 0; // Resume/Play owns focus after leaving playback.
}
void restore_diagnostic_preferences() {
    // Safe Startup has its own authoritative preference; older flag snapshots
    // may contain its bit after toggling another diagnostic while it was on.
    diagnostic_flags=static_cast<unsigned>(pref_int("plex_developer_flags_v1",0)) & (255u & ~static_cast<unsigned>(SafeHome));
    if(pref_int("plex_safe_home_v1",0))diagnostic_flags.fetch_or(SafeHome);
}
void draw_text(float x, float y, float size, const std::string &value,
               ImU32 color = Theme::Text,float max_width=0) {
    float scale = gfx.height / 1080.f;
    auto *draw=ImGui::GetBackgroundDrawList();
    const float right=max_width>0?x+max_width:x<330?310.f:1880.f;
    draw->PushClipRect({x*scale,y*scale},{right*scale,(y+size*1.5f)*scale},true);
    draw->AddText(ImGui::GetFont(), size * scale, {x * scale, y * scale},color,value.c_str());
    draw->PopClipRect();
}
void rect(float x, float y, float w, float h, ImU32 color) {
    float s = gfx.height / 1080.f;auto *draw=ImGui::GetBackgroundDrawList();
    draw->AddRectFilled({x*s,y*s},{(x+w)*s,(y+h)*s},color,Theme::Metric::Radius*s);
    if(color==Theme::Selection)
        draw->AddRect({x*s,y*s},{(x+w)*s,(y+h)*s},Theme::focus_color(plat_time()),Theme::Metric::Radius*s,0,Theme::Metric::FocusWidth*s);
}
void gradient(float x,float y,float w,float h,ImU32 top,ImU32 bottom) {
    const float s=gfx.height/1080.f;
    ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor({x*s,y*s},{(x+w)*s,(y+h)*s},top,top,bottom,bottom);
}
void cover_image(ImTextureID texture,float x,float y,float width,float height,ImU32 tint=Theme::White) {
    if(!texture)return;
    const float scale=gfx.height/1080.f;
    ImVec2 uv0,uv1;Theme::cover_uv(artwork->aspect(texture),width,height,uv0,uv1);
    ImGui::GetBackgroundDrawList()->AddImageRounded(texture,{x*scale,y*scale},{(x+width)*scale,(y+height)*scale},uv0,uv1,tint,Theme::Metric::Radius*scale);
}
void scene_background(bool sidebar_visible=false) {
    rect(0,0,1920,1080,Theme::Background);
    gradient(sidebar_visible?330:0,0,sidebar_visible?1590:1920,430,Theme::alpha(Theme::Accent,24),Theme::alpha(Theme::Background,0));
}
void row(int index, int selected, float y, const std::string &label, bool enabled = true) {
    rect(390,y,1440,Theme::Metric::RowHeight,index==selected && enabled?Theme::Selection:Theme::Surface);
    draw_text(420,y+16,Theme::Font::Body,label,enabled?Theme::Text:Theme::Secondary,1380);
}
void navigation_icon(int index,float x,float y,ImU32 color) {
    const float scale=gfx.height/1080.f;auto *draw=ImGui::GetBackgroundDrawList();
    auto point=[&](float a,float b){return ImVec2((x+a)*scale,(y+b)*scale);};
    auto line=[&](float a,float b,float c,float d){draw->AddLine(point(a,b),point(c,d),color,2*scale);};
    if(index==0) {draw->AddTriangle(point(-12,0),point(0,-12),point(12,0),color,2*scale);draw->AddRect(point(-8,0),point(8,13),color,0,0,2*scale);}
    else if(index==3 || index==5 || index==6) {
        draw->AddCircle(point(0,0),index==5?8*scale:11*scale,color,20,2*scale);
        if(index==3){line(0,-7,0,0);line(0,0,6,4);}
        if(index==5)line(6,7,14,15);
        if(index==6){draw->AddCircle(point(0,0),4*scale,color,16,2*scale);for(int n=0;n<8;++n){float a=n*3.14159265f/4;line(11*std::cos(a),11*std::sin(a),15*std::cos(a),15*std::sin(a));}}
    } else if(index==7) {line(5,-12,5,8);line(5,-12,13,-9);draw->AddCircleFilled(point(0,10),6*scale,color,16);}
    else if(index==4) {line(-8,-12,8,-12);line(-8,-12,-8,13);line(8,-12,8,13);line(-8,13,0,6);line(0,6,8,13);}
    else {
        draw->AddRect(point(-12,-10),point(12,12),color,3*scale,0,2*scale);
        if(index==8)line(0,-10,0,12);
        else if(index==2){line(-6,16,6,16);line(0,12,0,16);}
        else for(int n=0;n<3;++n){line(-8,-6+n*7,-5,-6+n*7);line(5,-6+n*7,8,-6+n*7);}
    }
}
bool custom_focus_valid() {
    if(profile_screen)return profile_pin ? pin_focus>=0 && pin_focus<10 : profile_focus>=0 && profile_focus<static_cast<int>(home_profiles.size());
    if(video)return !tracks || playback_tracks_focus_valid();
    if(editor)return edit_focus>=0 && edit_focus<static_cast<int>(alphabet.size());
    if(sidebar)return nav>=-1 && nav<12+static_cast<int>(home_sections.size()) && (nav<0 || nav_visible(nav));
    if(settings)return focus>=0 && focus<(settings_page==2 ? 13 : settings_page==3 ? 2 : settings_page==4 ? std::max(1,static_cast<int>(home_sections.size())) : settings_page==10 ? 13 : settings_page==11 ? std::max(1,static_cast<int>(home_sections.size())) : settings_page==9 ? 7 : settings_page==8 ? 8 : settings_page==7 ? 4 : settings_page==6 ? 4 : settings_page==5 ? 8 : settings_page==1 ? 2 : settings_page ? 1 : 13);
    if(audio_focus_state()>=0)return audio_focus_state()!=0;
    if(in_detail)return focus>=0 && focus<(cloud_available() && !cloud_id(detail).empty()?6:4);
    if(!available.empty())return focus>=0 && focus<static_cast<int>(available.size());
    if(page_path=="live-recordings")return focus>=0 && focus<std::max(1,static_cast<int>((live_rules_tab?live_subscriptions:live_schedule).size()));
    if(page_path=="live-tv")return focus>=0 && focus<std::max(1,static_cast<int>((live_guide_screen?live_program_list:live_channel_list).size()));
    if(home_screen) {
        if(feed_rows.empty())return true; // the refresh action owns focus
        return feed_focus>=0 && feed_focus<static_cast<int>(feed_rows.size()) && feed_columns.size()==feed_rows.size() &&
            feed_columns[feed_focus]>=0 && feed_columns[feed_focus]<static_cast<int>(feed_rows[feed_focus].entries.size());
    }
    return account.empty() || (focus>=0 && focus<static_cast<int>(listing.size()));
}
// Runs even on early returns (sign-in, keyboard, playback). Custom focus is
// deliberately reported separately from ImGui's widget navigation focus.
struct InputDiagnostic {
    const PadState &pad;
    uint32_t hit;
    ~InputDiagnostic() {
        plat_pad_set_private(profile_pin);
        if(profile_pin)return; // Never log keypad digits through focus/action traces.
        const char *screen = profile_screen ? (profile_pin ? "profile-pin" : "profiles") : video ? (tracks ? "tracks" : "playback") :
            editor ? "keyboard" : account.empty() ? "sign-in" : sidebar ? "sidebar" :
            settings ? "settings" : in_detail ? "detail" : home_screen && available.empty() ? "home" : !available.empty() ? "servers" : "library";
        int selected = profile_screen ? (profile_pin ? pin_focus : profile_focus) : video ? track_focus : editor ? edit_focus : sidebar ? nav : focus;
        bool home_content=home_screen && !profile_screen && !video && !editor && !sidebar && !settings && !in_detail && available.empty();
        if(home_content && feed_focus>=0 && feed_focus<static_cast<int>(feed_columns.size()))selected=feed_columns[feed_focus];
        bool valid=custom_focus_valid();
        static double log_at, event_log_at;
        static uint32_t received_since_log;
        received_since_log |= pad.buttons;
        double now = plat_time();
        auto &io = ImGui::GetIO();
        if (now >= log_at || (hit && now >= event_log_at)) {
            fprintf(stderr, "input:ui connected=%d received=%08x actions=%08x screen=%s focus=%d home_row=%d valid=%d busy=%d imgui_gamepad=%d imgui_nav=%d\n",
                    pad.connected, received_since_log, hit, screen, selected, home_content ? feed_focus : -1, valid, job != None,
                    !!(io.BackendFlags & ImGuiBackendFlags_HasGamepad), io.NavActive);
            received_since_log = 0;
            log_at = now + 5;
            event_log_at = now + .25;
        }
        if(!controller_overlay) return;
        char line[240];
        snprintf(line, sizeof line, "Pad:%s raw:%08x action:%08x  %s focus:%d %s  ImGui:%s",
                 pad.connected ? "OK" : "NO INPUT", pad.buttons, hit, screen, selected,
                 valid ? "OK" : "EMPTY/INVALID", io.NavActive ? "active" : "draw-list focus");
        float scale = gfx.height / 1080.f;
        auto *draw = ImGui::GetForegroundDrawList();
        draw->AddRectFilled({380*scale, 5*scale}, {1900*scale, 39*scale}, Theme::alpha(Theme::Background,230));
        draw->AddText(ImGui::GetFont(), Theme::Font::Caption*scale, {390*scale, 10*scale}, Theme::Secondary, line);
    }
};
void centered(float y, float size, const std::string &text, float center = 960,float max_width=1760,ImU32 color=Theme::Text) {
    float width=std::min(max_width,ImGui::GetFont()->CalcTextSizeA(size,10000,0,text.c_str()).x);
    draw_text(center-width/2,y,size,text,color,max_width);
}
void library_tabs_frame() {
    const char *tabs[]={"Recommended","Library","Categories"};
    for(int i=0;i<3;++i){rect(390+i*320,145,300,48,library_tab==i?Theme::Selection:Theme::Surface);draw_text(405+i*320,155,Theme::Font::Caption,tabs[i]);}
}
bool personal_item(const Item &item){
    for(const auto &section:home_sections)if(section.id==item.section_id)return personal_library(section);
    return library_context && !search_screen && personal_library(active_library);
}
std::string card_metadata(const Item &item){
    if(personal_item(item)){
        auto date=item.released;
        if(date.empty() && item.added>0){std::time_t stamp=item.added;std::tm local{};char value[20]="";if(localtime_r(&stamp,&local))std::strftime(value,sizeof value,"%Y-%m-%d",&local);date=value;}
        return (item.duration>0?std::to_string(item.duration/60000)+" min"+(date.empty()?"":" • "):"")+date;
    }
    if(item.type=="episode")return item.series+" S"+std::to_string(std::max(0,item.season))+" E"+std::to_string(std::max(0,item.episode));
    return item.year?std::to_string(item.year):"";
}
void poster(float x,float y,float width,float height,const Item &item,bool selected) {
    float scale=gfx.height/1080.f;
    rect(x-5,y-5,width+10,height+10,selected?Theme::focus_color(plat_time()):Theme::Border);
    auto texture=(item.type=="episode" || personal_item(item)) ? artwork->landscape(base,server_token,item.thumb) : artwork->get(base,server_token,item.thumb);
    if(texture) {
        float ratio=artwork->aspect(texture);
        if(ratio<=0)ratio=item.type=="episode"?16.f/9:(item.type=="album" || item.type=="artist" || item.type=="track")?1.f:2.f/3;
        float image_width=std::min(width,height*ratio);
        float image_height=image_width/ratio;
        float left=x+(width-image_width)*.5f;
        float top=y+(height-image_height)*.5f;
        ImGui::GetBackgroundDrawList()->AddImageRounded(texture,{left*scale,top*scale},{(left+image_width)*scale,(top+image_height)*scale},ImVec2(0,0),ImVec2(1,1),Theme::White,Theme::Metric::ImageRadius*scale);
    }
    else {rect(x,y,width,height,Theme::Surface);draw_text(x+12,y+height*.45f,Theme::Font::Caption,item.thumb.empty()?"No poster":"Artwork unavailable",Theme::Secondary,width-24);}
    if(item.watched) {rect(x+width-48,y+8,40,32,Theme::alpha(Theme::Success,245));draw_text(x+width-38,y+10,Theme::Font::Caption,"✓");}
    if(item.duration>0 && item.offset>0) {
        float progress=std::clamp(static_cast<float>(item.offset)/item.duration,0.f,1.f);
        rect(x,y+height-8,width,8,Theme::Border);rect(x,y+height-8,width*progress,8,Theme::Accent);
    }
}
void home_frame(uint32_t hit,float dt) {
    home_draw_ready=true;
    if(!startup_home_drawn && !feed_rows.empty() && !library_context && !search_screen){
        startup_home_drawn=true;
        fprintf(stderr,"startup: first-home-ms=%d cached-route=%d cached-home=%d rows=%zu\n",startup_begin>0?static_cast<int>((plat_time()-startup_begin)*1000):-1,startup_cached_route,startup_warm_home,feed_rows.size());
    }
    if(!feed_rows.empty() && !grid_checkpoint) {checkpoint(15,"first Home carousel",feed_rows.size());grid_checkpoint=true;}
    if(feed_rows.empty()) {
        draw_text(390,250, Theme::Font::Heading,search_screen?"No search results":job==HomeFeed ? "Loading your Home..." : "No Home items are available");
        draw_text(390,330, Theme::Font::Caption,search_screen?"Cross: refine search • L2/R2: search pages":"Open Movies or TV Shows to browse your libraries. Cross: refresh Home");
        if(!feed_error.empty())draw_text(390,420, Theme::Font::Caption,feed_error,Theme::Error);
        if(hit & PAD_LEFT)sidebar=true;
        if((hit & PAD_CROSS) && job==None){if(search_screen){editor=1;edit_text=search_query;edit_focus=0;}else load_home();}
        return;
    }
    if(feed_columns.size()!=feed_rows.size())feed_columns.assign(feed_rows.size(),0);
    if(feed_scroll.size()!=feed_rows.size())feed_scroll.assign(feed_rows.size(),0);
    feed_focus=std::clamp(feed_focus,0,static_cast<int>(feed_rows.size())-1);
    if(hit & PAD_UP)feed_focus=std::max(0,feed_focus-1);
    if(hit & PAD_DOWN)feed_focus=std::min(static_cast<int>(feed_rows.size())-1,feed_focus+1);
    auto &column=feed_columns[feed_focus];auto &selected=feed_rows[feed_focus];
    column=std::clamp(column,0,std::max(0,static_cast<int>(selected.entries.size())-1));
    if(hit & PAD_LEFT) {if(column>0)--column;else sidebar=true;}
    if(hit & PAD_RIGHT)column=std::min(static_cast<int>(selected.entries.size())-1,column+1);
    if(plex::diagnostic(TextOnly)) {
        draw_text(390,180, Theme::Font::Heading,selected.title);
        int first=std::max(0,column-4);
        for(int i=first;i<static_cast<int>(selected.entries.size()) && i<first+7;++i)row(i,column,250+(i-first)*Theme::Metric::CompactStep,selected.entries[i].title);
        if((hit & PAD_CROSS) && job==None && !sidebar && !selected.entries.empty())open_media(selected.entries[column]);
        return;
    }
    float amount=Theme::motion(dt);
    float target=static_cast<float>(feed_focus);feed_vertical+=(target-feed_vertical)*amount;
    float scale=gfx.height/1080.f;auto *draw=ImGui::GetBackgroundDrawList();
    if(!selected.entries.empty()) {
        const auto &featured=selected.entries[column];
        if(featured.type=="episode")artwork->landscape(base,server_token,featured.thumb);else artwork->get(base,server_token,featured.thumb);
        float feature_y=library_context?210.f:160.f,feature_height=library_context?125.f:175.f;
        rect(390,feature_y,1440,feature_height,Theme::Sidebar);
        auto backdrop=artwork->landscape(base,server_token,featured.art.empty()?featured.thumb:featured.art);
        cover_image(backdrop,390,feature_y,1440,feature_height,Theme::alpha(Theme::White,110));
        gradient(390,feature_y,1440,feature_height,Theme::alpha(Theme::Background,90),Theme::alpha(Theme::Background,240));
        draw_text(420,feature_y+22,Theme::Font::Caption,selected.continuing?"CONTINUE WATCHING":"YOUR LIBRARY",Theme::Focus,1300);
        draw_text(420,feature_y+60,Theme::Font::Heading,featured.title,Theme::Text,1300);
        if(!library_context)draw_text(420,277,Theme::Font::Caption,featured.summary.empty()?"Cross: open details • D-pad: browse your media":featured.summary,Theme::Secondary,1300);
    }
    draw->PushClipRect({365*scale,355*scale},{1890*scale,938*scale},true);
    for(int r=std::max(0,feed_focus-2);r<static_cast<int>(feed_rows.size()) && r<=feed_focus+2;++r) {
        auto &hub=feed_rows[r];float y=370+(r-feed_vertical)*Theme::Metric::CarouselStep;
        if(y>938 || y+Theme::Metric::CarouselStep<355)continue;
        draw_text(390,y, Theme::Font::Heading,hub.title);
        float dest=static_cast<float>(std::max(0,feed_columns[r]-4));feed_scroll[r]+=(dest-feed_scroll[r])*amount;
        for(int c=std::max(0,static_cast<int>(feed_scroll[r]));c<static_cast<int>(hub.entries.size()) && c<static_cast<int>(feed_scroll[r])+5;++c) {
            auto &item=hub.entries[c];float x=390+(c-feed_scroll[r])*Theme::Metric::CardStep;
            if(plex::diagnostic(TextOnly))row(c,feed_columns[r],y+55+(c%3)*80,item.title);
            else {
                poster(x,y+60,240,245,item,!sidebar && r==feed_focus && c==feed_columns[r]);
                draw->PushClipRect({x*scale,(y+315)*scale},{(x+240)*scale,(y+362)*scale},true);
                draw->AddText(ImGui::GetFont(),Theme::Font::Caption*scale,{x*scale,(y+315)*scale},Theme::Text,item.title.c_str(),nullptr,240*scale);
                draw->PopClipRect();
                std::string secondary=card_metadata(item);
                if(hub.continuing && item.duration>item.offset)secondary+="  "+std::to_string((item.duration-item.offset)/60000)+" min left";
                if(secondary.size()>40)secondary=secondary.substr(0,37)+"...";
                draw_text(x,y+365,Theme::Font::Caption,secondary,Theme::Secondary,240);
            }
        }
    }
    draw->PopClipRect();
    if(pref_int(account_key("plex_artwork_prefetch_v1").c_str(),1) && !artwork->busy() && column+1<static_cast<int>(selected.entries.size())) {
        const auto &next=selected.entries[column+1];
        if(next.type=="episode")artwork->landscape(base,server_token,next.thumb);else artwork->get(base,server_token,next.thumb);
    }
    if((hit & PAD_CROSS) && job==None && !sidebar && !selected.entries.empty())open_media(selected.entries[column]);
}
#include "plex_video_ui.h"
#include "plex_audio_ui.h"
#include "plex_progress_ui.h"
#include "plex_cloud_ui.h"

void profile_screen_frame(uint32_t hit) {
    scene_background();
    centered(130, Theme::Font::Activation,profile_pin ? "Enter profile PIN" : "Who's Watching?");
    if(!profile_error.empty())centered(880, Theme::Font::Caption,profile_error,960,1760,Theme::Error);
    if(job!=None)centered(820,Theme::Font::Body,deferred_profiles ? "Finishing previous user request..." : "Authorizing profile...");
    if(hit & PAD_CIRCLE) {
        if(job!=None) {deferred_profiles=true;return;}
        if(profile_pin) {profile_pin=false;erase_pin();profile_error.clear();}
        else {profile_error="Select a profile to continue. Options: sign out.";}
        return;
    }
    if((hit & PAD_OPTIONS) && job==None) {signout();return;}
    if(profile_pin) {
        centered(260, Theme::Font::Heading,requested_profile.name);
        centered(330, Theme::Font::Hero,std::string(pin_digits.size(),'*')+std::string(4-pin_digits.size(),'_'));
        if(job==None) {
            if(hit & PAD_LEFT)pin_focus=std::max(0,pin_focus-1);
            if(hit & PAD_RIGHT)pin_focus=std::min(9,pin_focus+1);
            if(hit & PAD_UP)pin_focus=std::max(0,pin_focus-5);
            if(hit & PAD_DOWN)pin_focus=std::min(9,pin_focus+5);
            if((hit & PAD_SQUARE) && !pin_digits.empty())pin_digits.pop_back();
            if((hit & PAD_CROSS) && pin_digits.size()<4) {pin_digits+=static_cast<char>('0'+pin_focus);if(pin_digits.size()==4)choose_profile();}
        }
        for(int i=0;i<10;++i) {float x=590+(i%5)*150,y=440+(i/5)*130;rect(x,y,115,95,i==pin_focus?Theme::Selection:Theme::Surface);draw_text(x+40,y+25, Theme::Font::Key,std::to_string(i));}
        centered(740, Theme::Font::Caption,"Cross: digit   Square: erase   Circle: cancel");return;
    }
    if(home_profiles.empty()) {
        centered(390, Theme::Font::Body,"Loading Plex Home profiles");
        if(job==None && (hit & PAD_CROSS)) {profile_error.clear();auto token=account;submit(HomeUsers,[token]{return client->home_users(token);});}
        centered(550, Theme::Font::Caption,"Cross: retry   Options: sign out");return;
    }
    if(job==None) {
        if(hit & PAD_LEFT)profile_focus=std::max(0,profile_focus-1);
        if(hit & PAD_RIGHT)profile_focus=std::min(static_cast<int>(home_profiles.size())-1,profile_focus+1);
        if(hit & PAD_UP)profile_focus=std::max(0,profile_focus-5);
        if(hit & PAD_DOWN)profile_focus=std::min(static_cast<int>(home_profiles.size())-1,profile_focus+5);
    }
    int first=(profile_focus/5)*5;
    int count=std::min(5,static_cast<int>(home_profiles.size())-first);
    if(hit & PAD_SQUARE)artwork->retry_avatars();
    artwork->avatar(home_profiles[profile_focus].avatar,account);
    for(int i=first;i<first+count;++i) {
        float x=960-count*150+(i-first)*300;
        rect(x,310,260,300,i==profile_focus?Theme::Selection:Theme::Surface);
        auto avatar=artwork->avatar(home_profiles[i].avatar,account);
        if(avatar) {
            float scale=gfx.height/1080.f;ImGui::GetBackgroundDrawList()->AddImageRounded(avatar,{(x+50)*scale,340*scale},{(x+210)*scale,500*scale},ImVec2(0,0),ImVec2(1,1),Theme::White,80*scale);
        } else {centered(390, Theme::Font::Avatar,"●",x+130);centered(470,Theme::Font::Caption,artwork->avatar_state(home_profiles[i].avatar),x+130);}
        centered(510,Theme::Font::Body,home_profiles[i].name,x+130,240);
        if(home_profiles[i].protected_user)centered(560, Theme::Font::Caption,"PIN protected",x+130);
    }
    centered(720, Theme::Font::Body,"D-pad / Stick: choose   Cross: select   Square: retry avatars   Options: sign out");
    if((hit & PAD_CROSS) && job==None) {requested_profile=home_profiles[profile_focus];profile_error.clear();choose_profile();}
}
void login_screen(uint32_t hit, double now) {
    scene_background();
    draw_text(80, 75, Theme::Font::Brand, "PlexStation", Theme::Accent);
    centered(170, Theme::Font::Hero, "Sign in to Plex");
    rect(220, 265, 1480, 625, Theme::Sidebar);
    if (account_loading) {
        centered(365, Theme::Font::Brand, job == ValidateAccount ? "Connecting to Plex" : "Authorization successful");
        centered(460, Theme::Font::Body, account_error.empty() ? "Loading Plex servers..." : "Connection failed");
        if (!account_error.empty()) centered(545, Theme::Font::Caption, account_error,960,1760,Theme::Error);
        if (!credentials_saved) centered(610, Theme::Font::Caption, "Sign-in could not be saved. Check storage permissions.");
        if (!account_error.empty() && job == None) {
            row(0,0,710,"Retry connection (Cross)");
            if (hit & PAD_CROSS) {
                account_error.clear(); auto token = account;
                submit(ValidateAccount, [token] { return client->validate_account(token); });
            }
        }
        if (hit & PAD_CIRCLE) {
            if (job == None) signout();
            else {
                cancel_bootstrap = true; account_error = "Cancelling connection...";
                if (!save_credentials(credential_file, device, "")) unlink(credential_file.c_str());
            }
        }
    } else if (auth.state == AuthState::Waiting) {
        if (auth.short_code()) {
            centered(325, Theme::Font::Body, "1. Open https://plex.tv/link on your phone or computer", 790);
            centered(390, Theme::Font::Body, "2. Enter this activation code", 790);
            centered(465, Theme::Font::DeviceCode, auth.current.code, 790);
        } else {
            centered(365, Theme::Font::Heading, "Scan the QR code to authorize PlexStation", 790);
            centered(440, Theme::Font::Body, "Browser sign-in: app.plex.tv/auth", 790);
            centered(500, Theme::Font::Caption, "Use the QR code to open this exact sign-in session", 790);
        }
        if (have_qr) {
            int n = qrcodegen_getSize(qr);
            // Integral display pixels and four-module quiet zone at 1080p/4K.
            float scale = gfx.height / 1080.f;
            int cell = std::max(1, static_cast<int>(350 * scale / (n + 8)));
            float x = 1350 * scale, y = 340 * scale;
            auto *draw = ImGui::GetBackgroundDrawList();
            draw->AddRectFilled({x,y},{x+(n+8)*cell,y+(n+8)*cell},Theme::White);
            for (int row = 0; row < n; ++row) for (int col = 0; col < n; ++col)
                if (qrcodegen_getModule(qr,col,row))
                    draw->AddRectFilled({x+(col+4)*cell,y+(row+4)*cell},
                        {x+(col+5)*cell,y+(row+5)*cell},Theme::Black);
        }
        centered(660, Theme::Font::Heading, "Waiting for authorization");
        centered(720, Theme::Font::Body, "Expires in " + std::to_string(auth.remaining(now)) + " seconds");
        centered(775, Theme::Font::Caption, "PlexStation will continue automatically after approval");
        if (hit & PAD_CIRCLE) { auth.cancel(); have_qr=false; }
    } else {
        bool busy = job == PinStart || job == PinPoll;
        const char *status = auth.state == AuthState::Generating ? "Connecting to Plex / Generating activation code" :
            auth.state == AuthState::Expired ? "Authentication expired" :
            auth.state == AuthState::Failed ? "Connection or authentication failed" :
            auth.state == AuthState::Cancelled ? "Linking cancelled" : "Your media, on your television.";
        centered(380, Theme::Font::Heading, status);
        if (!auth.error.empty()) centered(475, Theme::Font::Caption, auth.error,960,1760,Theme::Error);
        if (busy) centered(560, Theme::Font::Body, "Waiting for the current request to finish...");
        else {
            row(0,0,640, auth.state == AuthState::Idle ? "Sign in with Plex (Cross)" : "Retry / Get a new activation code (Cross)");
            if (hit & PAD_CROSS) {
                auth.begin(now); have_qr=false;
                auth_request_generation=auth.generation;
                submit(PinStart, [] { return client->create_pin(); });
            }
        }
        if (hit & PAD_CIRCLE) { auth.cancel(); have_qr=false; }
    }
    centered(950, Theme::Font::Caption, "Cross: Sign in / Retry     Circle: Cancel / Back");
}
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
    // A new schema starts hidden even if an older diagnostic build opted in.
    controller_overlay = pref_int("plex_controller_overlay_v2",0)!=0;
    restore_diagnostic_preferences();
    account_id.clear();primary_id.clear();profile_token.clear();profile_screen=false;
    auto &io = ImGui::GetIO();
    Theme::reduced_motion=pref_int("plex_reduce_motion_v1",0)!=0;
    Theme::apply(gfx.height/1080.f);
    auto font = std::string(plat_data_dir()) + "/fonts/Inter-Regular.ttf";
    if (access(font.c_str(), R_OK) == 0)
        io.Fonts->AddFontFromFileTTF(font.c_str(), Theme::Font::Body*gfx.height/1080.f);
    else {
        ImFontConfig config;config.SizePixels=Theme::Font::Body*gfx.height/1080.f;
        io.Fonts->AddFontDefault(&config);
    }
    if (!account.empty()) {
        account_loading = true;
        auto token = account;
        submit(ValidateAccount, [token] { return client->validate_account(token); });
    }
    return true;
}
void ui_frame(const PadState &pad, float dt) {
    player_tick();
    finish_job();
    uint32_t hit = controller.update(pad, dt);
    InputDiagnostic diagnostic{pad, hit};
    double now = plat_time();
    if(hit)last_input_at=now;
    service_video_retry();
    retry_progress();
    service_cleanup();
    if(warm_discovery_pending && job==None && !profile_screen && !base.empty() && home_screen && !sidebar && !settings && !in_detail && plat_time()>startup_begin+3) {
        warm_discovery_pending=false;auto token=profile_token;
        submit(BackgroundResources,[token]{return client->resources(token);});
    }
    service_next();
    service_cloud();
    service_home_refresh(now);
    audio_tick(now);
    auth.tick(now);
    if (auth.poll_due(now) && job == None) {
        auto p = auth.current;
        auth_request_generation = auth.generation;
        submit(PinPoll, [p] { return client->poll_pin(p.id, p.code); });
    }
    bool was_video = video;
    if (video) {
        service_subtitle_selection();
        if(video_info){video_tools(hit);hit=0;}
        if (hit)
            overlay_until = now + 5;
        if ((hit & PAD_CROSS) && !tracks)
            player_toggle_pause();
        if ((hit & PAD_LEFT) && !tracks)
            player_seek(std::max<int64_t>(0, player_time() - 10000));
        if ((hit & PAD_RIGHT) && !tracks)
            player_seek(player_time() + 10000);
        if (hit & PAD_CIRCLE) {
            if(subtitle_menu)subtitle_menu=false;
            else if (tracks) tracks = false;
            else stop();
        }
        if (hit & PAD_OPTIONS){tracks=!tracks;subtitle_menu=false;}
        if(hit & PAD_SQUARE){video_info=true;video_tools(0);hit=0;}
        if (hit & PAD_L1)
            player_set_audio_delay(audio_offset(player_audio_delay(), -1));
        if (hit & PAD_R1)
            player_set_audio_delay(audio_offset(player_audio_delay(), 1));
        if (hit & PAD_TRIANGLE)
            player_set_audio_delay(0);
        if(tracks && (hit & PAD_TRIANGLE))video_info=!video_info;
        if(tracks)playback_track_frame(hit);
        if (video && player_ended()) {
            if (player_failed())
                message = "Playback failed; check decoder and server connectivity";
            if(player_failed() && !stream_fallback){stream_fallback=true;player_stop();video=false;detail=playing;video_retry_pending=true;}
            else {bool ended_well=!player_failed();stop();if(ended_well)offer_autoplay(now);}
        }
        if (video && playing.type!="live" &&
            progress.due(static_cast<int64_t>(now * 1000), player_paused() ? "paused" : "playing"))
            queue_progress(player_paused() ? "paused" : "playing", player_time());
        if (video && !video_info && !subtitle_menu && (now < overlay_until || tracks)) {
            rect(70, 780, 1780, 230, Theme::alpha(Theme::Background,240));
            draw_text(100, 800, Theme::Font::Heading, playing.title);
            draw_text(100, 855, Theme::Font::Body,
                      std::to_string(player_time() / 1000) + " / " +
                          std::to_string(player_length() / 1000) +
                          " s   "+stream_plan.server_mode);
            draw_text(100, 900, Theme::Font::Body, "Cross Pause | Left/Right Seek | Options Tracks | Square Playback Info | Circle Stop");
            draw_text(100, 945, Theme::Font::Caption,
                      "L1/R1 Audio sync: " + std::to_string(player_audio_delay()) +
                          " ms (+ delays audio); Triangle resets");
        }
    }
    if (progress_pending && job == None) {
        auto b = base, t = server_token, state = progress_state;
        auto item = progress_item;
        auto pos = progress_time;
        progress_pending = false;
        capture_timeline(item,state,pos);
        submit(Timeline,
               [b, t, state, item, pos] { return client->timeline(b, t, item, state, pos); });
    }
    if (video)
        return;
    if (was_video) hit = 0; // The stop/back press must not also leave details.
    artwork->update();
    if (account.empty() || account_loading) { login_screen(hit, now); return; }
    if(profile_screen) {profile_screen_frame(hit);return;}
    scene_background(true);
    rect(0, 0, Theme::Metric::SidebarWidth, 1080, Theme::Sidebar);
    float brand_scale=gfx.height/1080.f;Theme::brand(ImGui::GetBackgroundDrawList(),35*brand_scale,50*brand_scale,50*brand_scale);
    draw_text(90, 55, Theme::Font::Brand, "PlexStation", Theme::Accent);
    if(sidebar && nav==-1)rect(25,100,280,44,Theme::Selection);
    draw_text(45,112,Theme::Font::Caption,active_profile.name.empty() ? "Profile" : active_profile.name,Theme::Secondary,210);
    std::vector<int> visible_sidebar;for(int i:sidebar_order())if(nav_visible(i))visible_sidebar.push_back(i);
    auto nav_position=std::find(visible_sidebar.begin(),visible_sidebar.end(),nav);
    int sidebar_first=nav_position==visible_sidebar.end()?0:std::max(0,static_cast<int>(nav_position-visible_sidebar.begin())-11);
    int sidebar_slot=-1;
    for (int i:visible_sidebar) {
        int &slot=sidebar_slot;
        if(!nav_visible(i))continue;
        ++slot;
        if(slot<sidebar_first || slot>=sidebar_first+12)continue;
        int display_slot=slot-sidebar_first;
        if (nav == i) {
            rect(25,150+display_slot*66,280,65,sidebar?Theme::Selection:Theme::alpha(Theme::Selection,140));
            rect(25,162+display_slot*66,4,41,Theme::Accent);
        }
        float y=182+display_slot*66;
        ImU32 color=nav==i ? Theme::Accent : Theme::Secondary;
        navigation_icon(i,55,y,color);
        draw_text(83,166+display_slot*66,i==3?Theme::Font::Caption:Theme::Font::Body,nav_label(i),nav==i?Theme::Text:Theme::Secondary,215);
    }
    draw_text(390, 65, Theme::Font::Title, settings ? (settings_page==3 ? "Profile" : "Settings") : in_detail ? detail.title : home_screen ? (search_screen?"Search":home_title) : page_title);
    if(!active_profile.id.empty()) {auto avatar=artwork->avatar(active_profile.avatar,account);if(avatar){float scale=gfx.height/1080.f;ImGui::GetBackgroundDrawList()->AddImageRounded(avatar,{270*scale,105*scale},{306*scale,141*scale},ImVec2(0,0),ImVec2(1,1),Theme::White,18*scale);}}
    if (job != None)
        draw_text(390, 135, Theme::Font::Body, "Loading…",Theme::Blue);
    if (!message.empty())
        draw_text(390, 950, Theme::Font::Caption, message, Theme::Warning);
    if(next_countdown_frame(hit,now))return;
    if(audio_frame(hit,dt))return;
    if(manual_confirm_http) {
        draw_text(390,230,Theme::Font::Heading,"Approve unencrypted local HTTP?");
        draw_text(390,310, Theme::Font::Body,"Traffic and credentials are not encrypted on this connection.");
        draw_text(390,370, Theme::Font::Body,"HTTPS is recommended. No automatic downgrade is performed.");
        draw_text(390,470, Theme::Font::Body,"Cross: approve and test   Circle: cancel");
        if(hit & PAD_CIRCLE) manual_confirm_http=false;
        else if((hit & PAD_CROSS) && job==None) {
            allow_http=true; pref_set("plex_allow_http",1); manual_confirm_http=false; test_manual();
        }
        return;
    }
    if(connection_diagnostics) {
        rect(360,50,1510,960,Theme::Background);
        draw_text(390,70,Theme::Font::Brand,"Server Connection Diagnostics");
        if(hit & PAD_UP) diagnostic_focus=std::max(0,diagnostic_focus-1);
        if(hit & PAD_DOWN) diagnostic_focus=std::min(std::max(0,static_cast<int>(connection_attempts.size())-1),diagnostic_focus+1);
        if(!connection_attempts.empty()) {
            const auto &d=connection_attempts[diagnostic_focus];
            draw_text(390,175,Theme::Font::Body,std::to_string(diagnostic_focus+1)+" / "+std::to_string(connection_attempts.size())+"   "+d.endpoint);
            draw_text(390,240, Theme::Font::Body,d.route+"   "+d.protocol+"   port "+std::to_string(d.port));
            draw_text(390,325, Theme::Font::Caption,"TCP: "+d.tcp);
            draw_text(390,405, Theme::Font::Caption,"TLS: "+d.tls);
            draw_text(390,485, Theme::Font::Caption,"Identity: "+d.identity);
            draw_text(390,565, Theme::Font::Caption,"Libraries: "+d.libraries);
            draw_text(390,660, Theme::Font::Caption,"Result: "+d.result);
        } else draw_text(390,250,Theme::Font::Body,"No completed endpoint attempts yet.");
        if(job==Select || job==Manual) draw_text(390,750, Theme::Font::Caption,"Testing permitted endpoints on the network worker...");
        draw_text(390,845, Theme::Font::Caption,manual_tested ? "Cross: save verified endpoint and connect   Circle: back" : "Up/Down: endpoint attempts   Circle: back");
        if(hit & PAD_CIRCLE) {connection_diagnostics=false; manual_tested=false;}
        else if(manual_tested && (hit & PAD_CROSS) && job==None) {
            save_account_pref("plex_manual_uri",next_base); save_account_pref("plex_manual_server_id",connection_target.id);
            connected(manual_sections);
            connection_diagnostics=false; manual_tested=false; message="Verified endpoint saved; libraries loaded.";
        }
        return;
    }
    if(reconnect_screen) {
        draw_text(390,230, Theme::Font::Heading,job==Select ? "Connecting to Server" : "Could not connect to your server");
        draw_text(390,310,Theme::Font::Body,connection_target.name);
        draw_text(390,430, Theme::Font::Caption,job==Select ? "Circle: cancel   Square: change server" : "Cross: retry   Square: change server   Circle: server picker");
        draw_text(390,500, Theme::Font::Caption,"Triangle: endpoint diagnostics");
        if(hit & PAD_TRIANGLE) {connection_diagnostics=true; diagnostic_focus=0;}
        if(hit & (PAD_CIRCLE|PAD_SQUARE)) {
            if(job==Select) {discard_connection=true; change_after_connection=!!(hit & PAD_SQUARE); message="Cancelling connection...";}
            else {reconnect_screen=false; if(hit & PAD_SQUARE) {force_picker=true; refresh();}}
        } else if((hit & PAD_CROSS) && job==None) connect_server(connection_target);
        return;
    }
    if(!editor && (hit & PAD_TRIANGLE)) {connection_diagnostics=true; diagnostic_focus=0; return;}
    if(!editor && !available.empty() && (hit & PAD_SQUARE) && job==None) {
        connection_target=available[std::clamp(focus,0,static_cast<int>(available.size())-1)];
        editor=2; edit_text=account_pref("plex_manual_uri","https://"); edit_focus=0; return;
    }
    if (editor) {
        draw_text(390, 180, Theme::Font::Body,
                  editor == 1 ? "Search your server"
                              : "Manual server: [http(s)://]host[:port] (default HTTPS / 32400)");
        draw_text(390, 245, Theme::Font::Body, edit_text);
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
                 i == edit_focus ? Theme::Selection : Theme::Surface);
            draw_text(x + 25, y + 10, Theme::Font::Body, std::string(1, alphabet[i]));
        }
        draw_text(390, 870, Theme::Font::Caption, editor==1 ? "Cross: type • Square: erase • Triangle: submit • Circle: cancel" : "Cross: type • Square: erase • Triangle: test before saving • Circle: cancel");
        if (hit & PAD_CIRCLE)
            editor = 0;
        if ((hit & PAD_TRIANGLE) && job == None) {
            if (editor == 3) {audio_load("/search?type=10&query="+encode(edit_text),"Music Search");editor=0;}
            else if (editor == 1) {
                search_query=edit_text;search_start=0;auto b=base,t=server_token,q=edit_text;
                submit(GlobalSearch,[b,t,q]{return global_search(*client,b,t,q);});
                editor = 0;
            } else {
                manual_candidate=manual_endpoint(edit_text);
                Endpoint parsed;
                if(manual_candidate.empty() || !endpoint(manual_candidate,parsed))
                    message="Enter a valid host/IP and port; bracket IPv6 addresses.";
                else if(!parsed.tls && connection_target.https_required)
                    message="This server requires HTTPS; HTTP is not permitted.";
                else if(!parsed.tls) manual_confirm_http=true;
                else test_manual();
            }
        }
        return;
    }
    if(full_description) {
        float scale=gfx.height/1080.f;auto *draw=ImGui::GetBackgroundDrawList();
        float height=ImGui::GetFont()->CalcTextSizeA(28*scale,10000,1350*scale,detail.summary.c_str()).y/scale;
        if(hit & PAD_UP)description_scroll=std::max(0.f,description_scroll-70);
        if(hit & PAD_DOWN)description_scroll=std::min(std::max(0.f,height-650),description_scroll+70);
        rect(360,155,1510,780,Theme::Sidebar);
        draw->PushClipRect({390*scale,190*scale},{1800*scale,870*scale},true);
        draw->AddText(ImGui::GetFont(),Theme::Font::Body*scale,{390*scale,(200-description_scroll)*scale},Theme::Text,detail.summary.c_str(),nullptr,1350*scale);draw->PopClipRect();
        draw_text(390,890, Theme::Font::Caption,"Up/Down: scroll   Circle/Square: close");
        if(hit & (PAD_CIRCLE|PAD_SQUARE))full_description=false;
        return;
    }
    if(in_detail && (hit & PAD_SQUARE)) {full_description=true;description_scroll=0;return;}
    if(filter_picker){
        if(hit & PAD_CIRCLE){filter_picker=false;return;}
        if(hit & PAD_UP)tools_focus=std::max(0,tools_focus-1);
        if(hit & PAD_DOWN)tools_focus=std::min(std::max(0,static_cast<int>(filter_values.size())-1),tools_focus+1);
        int first=std::max(0,tools_focus-6);for(int n=first;n<static_cast<int>(filter_values.size()) && n<first+7;++n)row(n,tools_focus,210+(n-first)*Theme::Metric::RowStep,filter_values[n].first);
        if(filter_values.empty())draw_text(410,280,Theme::Font::Body,"No filter values supplied by this library");
        if((hit & PAD_CROSS) && !filter_values.empty()){browser_filters[filter_field]=filter_values[tools_focus].second;filter_picker=false;load(library_query(page_path,browser_filters,browser_sort),page_title,false);}
        return;
    }
    if(library_tools) {
        const char *labels[]={"Title A–Z","Recently Added","Release Date (newest)","Unwatched Only","All Items","Search","Genre","Release Year","Rating (highest)","Duration (shortest)"};
        if(hit & PAD_CIRCLE)library_tools=false;
        if(hit & PAD_UP)tools_focus=std::max(0,tools_focus-1);
        if(hit & PAD_DOWN)tools_focus=std::min(9,tools_focus+1);
        int first=std::max(0,tools_focus-6);for(int i=first;i<static_cast<int>(sizeof(labels)/sizeof(labels[0])) && i<first+7;++i)row(i,tools_focus,210+(i-first)*Theme::Metric::RowStep,labels[i]);
        if((hit & PAD_CROSS) && job==None) {
            library_tools=false;
            if(tools_focus==5){editor=1;edit_text.clear();edit_focus=0;}
            else if(tools_focus==6 || tools_focus==7){
                auto end=page_path.find("/all");if(end==std::string::npos || page_path.rfind("/library/sections/",0)!=0){message="Open a library to use its server filter values";return;}
                filter_field=tools_focus==6?"genre":"year";auto path=page_path.substr(0,end)+"/"+filter_field;auto b=base,t=server_token;
                submit(FilterValues,[b,t,path]{return client->call("GET",b,path,t);});
            } else {
                if(tools_focus==3)browser_filters["unwatched"]="1";
                else if(tools_focus==4)browser_filters.clear();
                else {const char *sorts[]={"titleSort:asc","addedAt:desc","originallyAvailableAt:desc"};browser_sort=tools_focus<3?sorts[tools_focus]:tools_focus==8?"rating:desc":"duration:asc";}
                load(library_query(page_path,browser_filters,browser_sort),page_title,false);
            }
        }
        return;
    }
    bool on_home=home_screen && !settings && !in_detail && available.empty();
    bool grid = !on_home && !listing.empty() && page_path != "/library/sections" && !settings && !in_detail &&
                available.empty() && !plex::diagnostic(TextOnly);
    if ((hit & PAD_LEFT) && (!on_home || sidebar)) {
        if (!sidebar && grid && focus % 5 > 0)
            --focus;
        else
            sidebar = true;
    }
    if ((hit & PAD_RIGHT) && (!on_home || sidebar)) {
        if (!sidebar && grid)
            focus = std::min(static_cast<int>(listing.size()) - 1, focus + 1);
        else {sidebar = false;if(on_home)hit &= ~PAD_RIGHT;}
    }
    if (hit & PAD_OPTIONS) {
        if(job!=HomeFeed) {++screen_generation;++feed_generation;} settings = !settings; settings_page=0;
        sidebar = false;
        focus = 0;
    }
    if (hit & (PAD_L1 | PAD_R1)) {
        if(job==Browse || job==Sections || job==Details || job==HomeFeed) {++screen_generation;++feed_generation;}
        nav = nav_step(nav,(hit & PAD_R1)?1:-1,true);
        sidebar = true;
        hit |= PAD_CROSS; // Same section activation as the sidebar.
    }
    if(in_detail && detail_subtitles){subtitle_picker_frame(hit);return;}
    if(page_path=="live-recordings" && !settings && !sidebar && (hit & PAD_CIRCLE)){
        if(!live_cancel_id.empty())live_cancel_id.clear();else {page_path="live-tv";focus=0;}
        return;
    }
    if(page_path=="cloud-watchlist" && cloud_tools && (hit & PAD_CIRCLE)){cloud_tools=false;return;}
    if (hit & PAD_CIRCLE) {
        video_retry_pending=false;
        ++screen_generation; ++feed_generation;
        if (settings) { if(settings_page==10 || settings_page==11){settings_page=9;focus=4;} else if(settings_page==7){settings_page=6;focus=1;} else if(settings_page==8){settings_page=5;focus=7;} else if(settings_page) {focus=settings_page==1 ? 6 : settings_page==3 ? 8 : 7; settings_page=0;} else {settings = false; sidebar = true; focus = 0;} }
        else if (in_detail) { in_detail = false; sidebar = false; }
        else if (!history.empty() && (job==None || job==Browse || job==Sections || job==Details)) {
            auto p = history.back(); history.pop_back();
            page_title = p.title; page_path = p.path; listing = std::move(p.items);
            focus = std::clamp(p.focus,0,std::max(0,static_cast<int>(listing.size())-1)); page_start = p.start; page_total = p.total; home_screen=p.home;in_detail=p.show_details;if(p.show_details)detail=std::move(p.previous_detail);sidebar = false;
        } else sidebar = true;
        hit &= ~PAD_CIRCLE; // One back action per frame.
    }
    if(page_path=="cloud-watchlist" && !in_detail && !settings && !sidebar) {
        const char *filter_labels[]={"All","Movies","TV Shows"};
        for(int i=0;i<3;++i){rect(390+i*210,145,195,48,cloud_filter==i?Theme::Selection:Theme::Surface);draw_text(405+i*210,155,Theme::Font::Caption,filter_labels[i]);}
        draw_text(1040,155,Theme::Font::Caption,cloud_sort==1?"Title":cloud_sort==2?"Added Date ↑":"Added Date ↓");
        draw_text(1430,155,Theme::Font::Caption,std::to_string(page_total)+" items • Square: controls");
        if(hit & PAD_SQUARE){cloud_tools=true;focus=0;hit=0;}
        if(cloud_tools) {
            if(hit & PAD_UP)focus=0;
            if(hit & PAD_DOWN)focus=1;
            const char *filters[]={"All","Movies","TV Shows"};const char *sorts[]={"Added Date (newest)","Title","Added Date (oldest)"};
            row(0,focus,300,std::string("Filter: ")+filters[cloud_filter]);row(1,focus,400,std::string("Sort: ")+sorts[cloud_sort]);
            if(hit & PAD_CROSS){if(focus==0)cloud_filter=(cloud_filter+1)%3;else cloud_sort=(cloud_sort+1)%3;show_cloud(0);cloud_tools=false;}
            if(hit & PAD_CIRCLE)cloud_tools=false;
            return;
        }
    }
    if(library_context && !in_detail && !settings && !sidebar) {
        library_tabs_frame();
        if(library_tabs_focus) {
            if(hit & PAD_LEFT)library_tab=std::max(0,library_tab-1);
            if(hit & PAD_RIGHT)library_tab=std::min(2,library_tab+1);
            if(hit & PAD_CROSS){activate_library_tab();library_tabs_focus=false;}
            if(hit & PAD_DOWN)library_tabs_focus=false;
            draw_text(390,920,Theme::Font::Caption,"Left/Right choose tab • Cross open • Down browse • Change library in Settings / Libraries");
            return;
        }
        if((hit & PAD_UP) && (home_screen?feed_focus==0:focus<5)){library_tabs_focus=true;return;}
    }
    if (sidebar) {
        if (hit & PAD_UP)
            {nav=nav_step(nav,-1);}
        if (hit & PAD_DOWN)
            {nav=nav_step(nav,1);}
        if ((hit & PAD_CROSS) && job == None) {
            if(nav==-1) {settings=true;settings_page=3;sidebar=false;focus=0;return;}
            if(nav==7 || nav==8) {library_context=false;audio_home(nav==8);return;}
            audio_leave_browse();
            library_context=false;settings=false;in_detail=false;
            if(nav>=12){active_library=home_sections[nav-12];library_context=true;library_kind=personal_library(active_library)?9:active_library.type=="show"?2:1;library_tab=0;activate_library_tab();return;}
            if(nav==9){open_library(9);return;}
            if(nav==10 || nav==11){home_screen=false;listing.clear();page_title=navigation[nav];page_path=nav==10?"live-tv":"rentals";sidebar=false;
                if(nav==10){live_channel_list=Json::array();live_program_list=Json::array();live_guide_screen=false;live_error.clear();auto b=base,t=server_token;submit(LiveChannels,[b,t]{return live_channels(*client,b,t);});}
                return;}
            settings = nav == 6; settings_page=0;
            in_detail = false;
            sidebar = false;
            focus = 0;
            if (nav == 0) {load_home();}
            else if (nav == 3)
                load("/library/onDeck", "Continue Watching");
            else if (nav == 1 || nav == 2) {
                open_library(nav);
            } else if (nav == 5) {
                editor = 1;
                edit_text.clear();
                edit_focus = 0;
            } else if (nav == 4)
                {if(cloud_available())show_cloud();else request_cloud_check();}
        }
    } else if (settings) {
        if(settings_page==10) {
            if(hit & PAD_UP)focus=std::max(0,focus-1);
            if(hit & PAD_DOWN)focus=std::min(12,focus+1);
            auto order=sidebar_order();order.resize(12);int hidden=pref_int(account_key("plex_sidebar_hidden_v1").c_str(),0);
            int first=std::max(0,focus-6);
            for(int i=first;i<13 && i<first+7;++i)row(i,focus,210+(i-first)*85,i==12?"Reset Sidebar":nav_label(order[i])+((hidden&(1<<order[i]))?" [Hidden]":" [Shown]"));
            draw_text(390,885,Theme::Font::Caption,"Cross show/hide • Left/Right reorder • Square pin accessible libraries");
            if(hit & PAD_SQUARE){settings_page=11;focus=0;return;}
            if((hit & PAD_CROSS) && focus==12){pref_set(account_key("plex_sidebar_order_v1").c_str(),"[]");pref_set(account_key("plex_sidebar_hidden_v1").c_str(),0);pref_set(pin_order_key().c_str(),"[]");
                for(const auto &section:home_sections)pref_set((account_key("plex_pinned_library_v1")+"."+encode(selected_server.id)+"."+encode(section.id)).c_str(),0);
            }
            else if((hit & PAD_CROSS) && order[focus]!=0 && order[focus]!=6){hidden^=1<<order[focus];pref_set(account_key("plex_sidebar_hidden_v1").c_str(),hidden);}
            if(focus<12 && (hit & (PAD_LEFT|PAD_RIGHT))){int other=std::clamp(focus+((hit & PAD_RIGHT)?1:-1),0,11);std::swap(order[focus],order[other]);focus=other;pref_set(account_key("plex_sidebar_order_v1").c_str(),Json(order).dump());}
            return;
        }
        if(settings_page==11) {
            auto order=library_pin_order();
            if(hit & PAD_UP)focus=std::max(0,focus-1);
            if(hit & PAD_DOWN)focus=std::min(std::max(0,static_cast<int>(order.size())-1),focus+1);
            if(order.empty()){draw_text(390,250,Theme::Font::Body,"No accessible libraries to pin");return;}
            int first=std::max(0,focus-6);
            for(int i=first;i<static_cast<int>(order.size()) && i<first+7;++i){const auto &section=home_sections[order[i]];auto key=account_key("plex_pinned_library_v1")+"."+encode(selected_server.id)+"."+encode(section.id);row(i,focus,210+(i-first)*85,section.title+(pref_int(key.c_str(),0)?" [Pinned in Sidebar]":""),section.type=="movie" || section.type=="show");}
            if(hit & PAD_CROSS){const auto &section=home_sections[order[focus]];if(section.type=="movie" || section.type=="show"){auto key=account_key("plex_pinned_library_v1")+"."+encode(selected_server.id)+"."+encode(section.id);pref_set(key.c_str(),pref_int(key.c_str(),0)?0:1);}}
            if(hit & (PAD_LEFT|PAD_RIGHT)){int other=std::clamp(focus+((hit & PAD_RIGHT)?1:-1),0,static_cast<int>(order.size())-1);std::swap(order[focus],order[other]);focus=other;Json ids=Json::array();for(int index:order)ids.push_back(home_sections[index].id);pref_set(pin_order_key().c_str(),ids.dump());}
            draw_text(390,885,Theme::Font::Caption,"Cross pin/unpin • Left/Right reorder libraries • Circle back");
            return;
        }
        if(settings_page==9) {
            if(hit & PAD_UP)focus=std::max(0,focus-1);
            if(hit & PAD_DOWN)focus=std::min(6,focus+1);
            const char *labels[]={"Default Movies Library","Default TV Library","Default Home Movies Library","Seasonal Recommendations","Sidebar customization","Change current library","Season selection"};
            for(int i=0;i<7;++i)row(i,focus,210+i*85,labels[i]);
            const char *seasons[]={"Automatic","Halloween / Horror","Christmas / Holiday","Winter","Summer"};
            draw_text(390,855,Theme::Font::Caption,std::string("Current season: ")+seasons[std::clamp(pref_int(account_key("plex_seasonal_mode_v1").c_str(),0),0,4)]);
            if(hit & PAD_CROSS) {
                if(focus<3){int kind=focus==0?1:focus==1?2:9;auto choices=video_libraries(home_sections,kind);auto selected=default_library(home_sections,kind,pref_str(library_preference(kind).c_str(),""));for(size_t i=0;i<choices.size();++i)if(choices[i].id==selected.id){auto next=choices[(i+1)%choices.size()];pref_set(library_preference(kind).c_str(),next.id);message=std::string(labels[focus])+": "+next.title;break;}}
                if(focus==3){auto key=account_key("plex_seasonal_v1");bool enabled=pref_int(key.c_str(),1)!=0;pref_set(key.c_str(),enabled?0:1);message=enabled?"Seasonal rows disabled":"Seasonal rows enabled";}
                if(focus==6){auto key=account_key("plex_seasonal_mode_v1");pref_set(key.c_str(),(std::clamp(pref_int(key.c_str(),0),0,4)+1)%5);}
                if(focus==4){settings_page=10;focus=0;}
                if(focus==5 && music_section(active_library)){
                    std::vector<Item> choices;for(const auto &section:home_sections)if(music_section(section) && book_section(section)==audio.books)choices.push_back(section);
                    for(size_t i=0;i<choices.size();++i)if(choices[i].id==active_library.id){pref_set((audio_scope()+(audio.books?".default-books":".default-music")).c_str(),choices[(i+1)%choices.size()].id);settings=false;audio_home(audio.books);break;}
                }
                else if(focus==5){auto choices=video_libraries(home_sections,library_kind);for(size_t i=0;i<choices.size();++i)if(choices[i].id==active_library.id){pref_set(library_preference(library_kind).c_str(),choices[(i+1)%choices.size()].id);settings=false;open_library(library_kind);break;}}
            }
            return;
        }
        if(settings_page==8){subtitle_settings_frame(hit);return;}
        if(settings_page==5){video_options_frame(hit);return;}
        if(settings_page==4){audio_settings_frame(hit);return;}
        if(settings_page==3) {
            if(hit & PAD_UP)focus=0;
            if(hit & PAD_DOWN)focus=1;
            row(0,focus,220,"Switch User");
            bool show=pref_int(primary_key("plex_show_profiles").c_str(),1)!=0;
            row(1,focus,320,std::string("Show Who's Watching at Startup: ")+(show ? "On" : "Off"));
            if(hit & PAD_CROSS) {
                if(focus==0)show_profiles();
                else pref_set(primary_key("plex_show_profiles").c_str(),show?0:1);
            }
            return;
        }
        if(settings_page==2) {
            const char *labels[]={"Show Controller Diagnostics","Text-Only Library Mode","Disable Artwork Downloads","Disable Artwork Decoding","Disable Artwork GPU Uploads","Limit Library Results to 10","Detailed Library Tracing","Server Connection Diagnostics","Library / Artwork Diagnostics","Application Build Information","Safe Startup (skip automatic Home)","Detailed Home Initialization Tracing","Retry Avatar"};
            if(hit & PAD_UP) focus=std::max(0,focus-1);
            if(hit & PAD_DOWN) focus=std::min(12,focus+1);
            int first=std::max(0,focus-6);
            for(int i=first;i<13 && i<first+7;++i) {
                bool enabled=i==0 ? controller_overlay : i<=6 ? !!(diagnostic_flags.load() & (1u<<(i-1))) : i==10 ? plex::diagnostic(SafeHome) : i==11 ? plex::diagnostic(DetailedHome) : false;
                row(i,focus,190+(i-first)*Theme::Metric::RowStep,std::string(labels[i])+(i<=6 || i>=10 ? (enabled ? ": On" : ": Off") : ""));
            }
            if(hit & PAD_CROSS) {
                if(focus==0) {controller_overlay=!controller_overlay; pref_set("plex_controller_overlay_v2",controller_overlay?1:0);}
                else if(focus<=6) {diagnostic_flags.fetch_xor(1u<<(focus-1));pref_set("plex_developer_flags_v1",static_cast<int>(diagnostic_flags.load()));artwork->clear();diagnostic_lines=0;}
                else if(focus==7) {connection_diagnostics=true;diagnostic_focus=0;}
                else if(focus==12){artwork->retry_avatars();message="Avatar retry queued. "+artwork->avatar_state(active_profile.avatar);}
                else if(focus==8) message=artwork->diagnostics();
                else if(focus==10){diagnostic_flags.fetch_xor(SafeHome);pref_set("plex_safe_home_v1",plex::diagnostic(SafeHome)?1:0);}
                else if(focus==11){diagnostic_flags.fetch_xor(DetailedHome);pref_set("plex_developer_flags_v1",static_cast<int>(diagnostic_flags.load()));}
                else message=std::string("PlexStation ")+PLEX_BUILD_COMMIT+"; PPSA85301; native VLC; hardware verification pending";
            }
            return;
        }
        if(settings_page==7) {
            if(hit & PAD_UP)focus=std::max(0,focus-1);
            if(hit & PAD_DOWN)focus=std::min(3,focus+1);
            int maximum=std::clamp(pref_int("plex_artwork_cache_mb_v1",256),64,1024);
            row(0,focus,210,"Disk usage: "+std::to_string(artwork->disk_usage()/1024)+" KiB • GPU: "+std::to_string(artwork->memory_usage()/1024)+" KiB");
            row(1,focus,310,"Maximum disk cache: "+std::to_string(maximum)+" MiB");
            row(2,focus,410,"Clear this profile/server's artwork cache");
            row(3,focus,510,std::string("Nearby artwork prefetch: ")+(pref_int(account_key("plex_artwork_prefetch_v1").c_str(),1)?"On":"Off"));
            draw_text(390,670,Theme::Font::Caption,"Private hashed keys • validated images • atomic writes • 64 MiB GPU budget");
            if(hit & PAD_CROSS) {
                if(focus==1){pref_set("plex_artwork_cache_mb_v1",maximum>=1024?64:maximum*2);configure_artwork_scope(account_id+"/server:"+selected_server.id);}
                if(focus==2){artwork->clear();artwork->clear_disk();message="Artwork cache cleanup queued on the image worker.";}
                if(focus==3)pref_set(account_key("plex_artwork_prefetch_v1").c_str(),pref_int(account_key("plex_artwork_prefetch_v1").c_str(),1)?0:1);
            }
            return;
        }
        if(settings_page==6) {
            if(hit & PAD_UP)focus=std::max(0,focus-1);
            if(hit & PAD_DOWN)focus=std::min(3,focus+1);
            row(0,focus,210,Theme::reduced_motion?"Reduced motion: On":"Reduced motion: Off");
            row(1,focus,310,"Artwork Cache");row(2,focus,410,"Sidebar");row(3,focus,510,"Libraries / Seasonal Recommendations");
            draw_text(390,650,Theme::Font::Body,"Instant carousel movement, steady outlines and persistent artwork preferences.");
            if(hit & PAD_CROSS){if(focus==3){settings_page=9;focus=0;}else if(focus==2){settings_page=10;focus=0;}else if(focus==1){settings_page=7;focus=0;}else{Theme::reduced_motion=!Theme::reduced_motion;pref_set("plex_reduce_motion_v1",Theme::reduced_motion?1:0);}}
            return;
        }
        if(settings_page==1) {
            if(hit & PAD_UP)focus=0;
            if(hit & PAD_DOWN)focus=1;
            row(0,focus,210,"Change Server");row(1,focus,310,"Clear Cached Connection");
            if((hit & PAD_CROSS) && job==None) {
                if(focus==0){force_picker=true;refresh();}
                else {save_account_pref("plex_verified_endpoint_v2","");message="Cached connection cleared. Your preferred server is retained.";}
            }
            return;
        }
        if (hit & PAD_UP)
            focus = std::max(0, focus - 1);
        if (hit & PAD_DOWN)
            focus = std::min(12, focus + 1);
        if (hit & PAD_CROSS) {
            if(focus==12){settings_page=9;focus=0;return;}
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
            if (focus == 3) {force_picker=true; refresh();}
            if (focus == 4)
                signout();
            if (focus == 5) {
                if(job!=None) {message="Wait for the current connection request to finish."; return;}
                if(!selected_server.id.empty()) connection_target=selected_server;
                else if(available.size()==1) connection_target=available.front();
                else {message="Choose a server and press Square to test its manual endpoint."; return;}
                editor = 2;
                edit_text = account_pref("plex_manual_uri","https://");
                edit_focus = 0;
            }
            if(focus==11) {settings_page=6;focus=0;}
            else if(focus==10) {settings_page=5;focus=0;}
            else if(focus==9) {settings_page=4;focus=0;}
            else if(focus==8) {settings_page=3;focus=0;}
            else if(focus==6 || focus==7) {settings_page=focus==6 ? 1 : 2; focus=0;}
            pref_set(account_key("plex_audio_delay").c_str(), global_delay);
            pref_set("plex_allow_http", allow_http ? 1 : 0);
        }
        const std::string labels[]={"Default audio delay +25 ms ("+std::to_string(global_delay)+" ms)","Default audio delay -25 ms",allow_http?"Local HTTP: allowed":"Local HTTP: disabled","Refresh servers","Sign out","Manual server address","Server","Developer Options","Profiles","Audio / Audiobook Libraries","Playback / Quality / Subtitles","Appearance / Motion","Libraries / Seasonal / Sidebar"};
        int first=std::max(0,focus-7);for(int n=first;n<13 && n<first+8;++n)row(n,focus,180+(n-first)*Theme::Metric::RowStep,labels[n]);
    } else if (!available.empty()) {
        if (hit & PAD_UP)
            focus = std::max(0, focus - 1);
        if (hit & PAD_DOWN)
            focus = std::min(static_cast<int>(available.size()) - 1, focus + 1);
        int first = std::max(0, focus - 7);
        for (int i = first; i < static_cast<int>(available.size()) && i < first + 8; ++i)
            row(i, focus, 210 + (i - first) *Theme::Metric::CompactStep, available[i].name);
        if ((hit & PAD_CROSS) && job == None) {
            connect_server(available[std::clamp(focus,0,static_cast<int>(available.size())-1)]);
        }
    } else if (in_detail) {
        if(detail_subtitles){subtitle_picker_frame(hit);return;}
        prepare_subtitle_selection();
        if(!plex::diagnostic(TextOnly)) {
            poster(1560,220,250,340,detail,false);
            auto backdrop=artwork->landscape(base,server_token,detail.art);
            if(backdrop) {cover_image(backdrop,385,155,1125,385,Theme::alpha(Theme::White,150));gradient(385,155,1125,385,Theme::alpha(Theme::Background,100),Theme::Background);}
        }
        draw_text(390, 220, Theme::Font::Caption,
                  (detail.year ? std::to_string(detail.year)+" • " : "")+(detail.duration>0 ? std::to_string(detail.duration/60000)+" min" : ""));
        float scale = gfx.height / 1080.f;
        ImGui::GetBackgroundDrawList()->AddText(
            ImGui::GetFont(), Theme::Font::Body * scale, {390 * scale, 280 * scale}, Theme::Text,
            (detail.summary.size()>500 ? detail.summary.substr(0,500)+"... (Square: read more)" : detail.summary).c_str(), nullptr, 1100 * scale);
        row(0, focus, 615, detail.type=="show" ? "Browse seasons" : detail.type=="season" ? "Browse episodes" : detail.offset > 0 ? "Resume" : "Play");
        float chip_x=390;
        auto chip=[&](const std::string &label) {
            if(label.empty() || chip_x>1390)return;
            float width=std::min(180.f,ImGui::GetFont()->CalcTextSizeA(Theme::Font::Caption,10000,0,label.c_str()).x+24);
            rect(chip_x,570,width,34,Theme::Sidebar);
            draw_text(chip_x+12,575,Theme::Font::Caption,label,Theme::Secondary,width-24);chip_x+=width+12;
        };
        chip(detail.content_rating);for(const auto &genre:detail.genres)chip(genre);
        std::string cast="Cast: ";for(const auto &name:detail.cast){if(cast.size()>180)break;if(cast.size()>6)cast+=", ";cast+=name;}
        if(!detail.cast.empty())draw_text(390,610, Theme::Font::Caption,cast.size()>130 ? cast.substr(0,127)+"..." : cast);
        row(1, focus, 690, detail.watched ? "Mark unwatched" : "Mark watched");
        row(2,focus,765,detail.type=="show" || detail.type=="season" ? "Read description" : detail.media.is_array() && !detail.media.empty() ? "Playback version: "+std::to_string(version+1)+" / "+std::to_string(detail.media.size())+" (Cross cycles)" : "No playable versions provided");
        if (hit & PAD_UP)
            focus = std::max(0, focus - 1);
        if (hit & PAD_DOWN)
            focus = std::min(cloud_available() && !cloud_id(detail).empty()?5:3, focus + 1);
        if(cloud_available() && !cloud_id(detail).empty()){row(4,focus,915,"Add to Plex Cloud Watchlist");row(5,focus,990,"Remove from Plex Cloud Watchlist");}
        row(3,focus,840,"Subtitles (select before playback)");
        if ((hit & PAD_CROSS) && job == None) {
            if(focus==3){detail_subtitles=true;subtitle_focus=0;return;}
            if(focus==4 || focus==5){change_cloud(focus==4);return;}
            if (focus == 0) {
                if(detail.type=="show" || detail.type=="season")load("/library/metadata/"+encode(detail.id)+"/children",detail.title);
                else play();
            }
            else if (focus == 2) {
                if(detail.type=="show" || detail.type=="season") {full_description=true;description_scroll=0;}
                else if (detail.media.is_array() && !detail.media.empty())
                    version = (version + 1) % detail.media.size();
            } else {
                auto b = base, t = server_token;
                auto i = detail;
                submit(Watch, [b, t, i] { return client->watched(b, t, i, !i.watched); });
            }
        }
    } else if(page_path=="cloud-pending") {
        draw_text(390,250,Theme::Font::Heading,"Plex Cloud Watchlist");draw_text(390,350,Theme::Font::Body,cloud_error.empty()?"Checking authorized cloud access…":cloud_error);
        draw_text(390,470,Theme::Font::Caption,"Cross: retry supported Watchlist access • Circle: sidebar");if((hit & PAD_CROSS) && job==None)request_cloud_check();
    } else if(page_path=="live-tv") {
        if(hit & PAD_UP)focus=std::max(0,focus-1);
        const auto &entries=live_guide_screen?live_program_list:live_channel_list;
        if(hit & PAD_DOWN)focus=std::min(std::max(0,static_cast<int>(entries.size())-1),focus+1);
        draw_text(390,200,Theme::Font::Heading,live_guide_screen?text(live_channel,"title")+" / Guide":"Personal Plex Live TV & DVR");
        if(entries.empty())draw_text(390,310,Theme::Font::Body,job!=None?"Loading authorized Live TV…":live_guide_screen?"No guide programs returned for today":"No authorized, configured DVR channels were returned");
        int first=std::max(0,focus-6);
        for(int i=first;i<static_cast<int>(entries.size()) && i<first+7;++i){
            std::string label;
            if(live_guide_screen){auto clock=[](int64_t value){std::time_t stamp=value;std::tm local{};char time[20]="";if(localtime_r(&stamp,&local))std::strftime(time,sizeof time,"%H:%M",&local);return std::string(time);};auto start=number(entries[i],"beginsAt"),end=number(entries[i],"endsAt"),now=static_cast<int64_t>(std::time(nullptr));label=(start<=now && now<end?"Now • ":start>now?"Upcoming • ":"Earlier • ")+clock(start)+"–"+clock(end)+" "+text(entries[i],"series")+" / "+text(entries[i],"title");}
            else label=text(entries[i],"number")+" "+text(entries[i],"title");
            row(i,focus,275+(i-first)*80,label);
            if(!live_guide_screen){auto logo=artwork->get(base,server_token,text(entries[i],"thumb"));if(logo)cover_image(logo,1755,283+(i-first)*80,48,48);}
        }
        if(!live_error.empty())draw_text(390,860,Theme::Font::Caption,live_error,Theme::Warning);
        draw_text(390,910,Theme::Font::Caption,live_guide_screen?"Cross: watch channel now • Square: channel list":"Cross: program guide • Square: recordings • Free cloud TV is not integrated");
        if((hit & PAD_SQUARE) && job==None){
            if(live_guide_screen){live_guide_screen=false;focus=0;}
            else {page_path="live-recordings";live_schedule=Json::array();live_subscriptions=Json::array();live_cancel_id.clear();live_error.clear();focus=0;auto b=base,t=server_token;submit(LiveRecordings,[b,t]{return live_recordings(*client,b,t);});}
        }
        if((hit & PAD_CROSS) && job==None && !entries.empty()) {
            auto b=base,t=server_token;
            if(live_guide_screen){auto channel=live_channel;auto consumer=device;submit(LiveTune,[b,t,channel,consumer]{return live_tune(*client,b,t,channel,consumer);});}
            else {live_channel=live_channel_list[focus];auto channel=live_channel;std::time_t now=std::time(nullptr);std::tm today{};char date[20]="";localtime_r(&now,&today);std::strftime(date,sizeof date,"%Y-%m-%d",&today);std::string day=date;submit(LiveGuide,[b,t,channel,day]{return live_guide(*client,b,t,channel,day);});}
        }
    } else if(page_path=="live-recordings") {
        draw_text(390,200,Theme::Font::Heading,live_rules_tab?"DVR Recording Rules":"Scheduled DVR Recordings");
        if(!live_cancel_id.empty()) {
            draw_text(390,330,Theme::Font::Body,"Cancel this entire recording rule and all its scheduled recordings?");
            draw_text(390,440,Theme::Font::Caption,"Cross: confirm cancellation • Circle: keep rule");
            if((hit & PAD_CROSS) && job==None){auto b=base,t=server_token,id=live_cancel_id;submit(LiveCancel,[b,t,id]{return cancel_recording_rule(*client,b,t,id);});}
        }else {
            if(hit & (PAD_L2|PAD_R2)){live_rules_tab=!live_rules_tab;focus=0;}
            const auto &entries=live_rules_tab?live_subscriptions:live_schedule;
            if(hit & PAD_UP)focus=std::max(0,focus-1);
            if(hit & PAD_DOWN)focus=std::min(std::max(0,static_cast<int>(entries.size())-1),focus+1);
            int first=std::max(0,focus-6);
            for(int i=first;i<static_cast<int>(entries.size()) && i<first+7;++i)row(i,focus,280+(i-first)*80,text(entries[i],"title")+(live_rules_tab?"":" • "+text(entries[i],"status")));
            if(entries.empty())draw_text(390,330,Theme::Font::Body,job!=None?"Loading authorized DVR data…":"No permitted recordings returned");
            if((hit & PAD_CROSS) && live_rules_tab && !entries.empty() && job==None){if(active_profile.admin)live_cancel_id=text(entries[focus],"id");else live_error="Recording-rule changes require the owner's authorized profile.";}
            if((hit & PAD_SQUARE) && job==None){auto b=base,t=server_token;submit(LiveRecordings,[b,t]{return live_recordings(*client,b,t);});}
        }
        if(!live_error.empty())draw_text(390,850,Theme::Font::Caption,live_error,Theme::Warning);
        draw_text(390,910,Theme::Font::Caption,"L2/R2: schedule / rules • Cross: cancel selected rule • Square: refresh • Circle: channels");
        draw_text(390,950,Theme::Font::Caption,"Completed recordings are in your configured libraries. New recording setup uses Plex's supported app.");
    } else if(page_path=="rentals") {
        draw_text(390,240,Theme::Font::Heading,"Plex Rentals");
        draw_text(390,350,Theme::Font::Body,"Rental catalog and entitlement APIs are not available to this client.");
        draw_text(390,440,Theme::Font::Body,"Use an authorized Plex application for purchases and DRM-protected rental playback.");
    } else if(home_screen) {
        if(search_screen && (hit & (PAD_L2|PAD_R2)) && job==None){search_start=std::max(0,search_start+((hit & PAD_R2)?12:-12));auto b=base,t=server_token,q=search_query;auto start=search_start;submit(GlobalSearch,[b,t,q,start]{return global_search(*client,b,t,q,start);});}
        home_frame(hit,dt);
    }
    else {
        if (!sidebar && (hit & PAD_UP))
            focus = std::max(0, focus - (grid ? 5 : 1));
        if (!sidebar && (hit & PAD_DOWN) && !listing.empty())
            focus = std::min(static_cast<int>(listing.size()) - 1, focus + (grid ? 5 : 1));
        if (grid) {
            if(!grid_checkpoint) {checkpoint(15,"first movie grid",listing.size());grid_checkpoint=true;}
            int first = std::max(0, (focus / 5 - 1) * 5);
            for (int i = first; i < static_cast<int>(listing.size()) && i < first + 10; ++i) {
                float x = 390 + (i - first) % 5 * 285, y = 210 + (i - first) / 5 * 350;
                bool selected = i == focus && !sidebar;
                poster(x,y,250,270,listing[i],selected);
                auto label = listing[i].title;
                if (label.size() > 22)
                    label = label.substr(0, 19) + "…";
                draw_text(x + 7, y + 285, Theme::Font::Caption, label);
                draw_text(x+7,y+315,Theme::Font::Caption,page_path=="cloud-watchlist"?"Availability: check selected server":card_metadata(listing[i]));
            }
        } else {
            int first = std::max(0, focus - 7);
            for (int i = first; i < static_cast<int>(listing.size()) && i < first + 8; ++i)
                row(i, focus, 210 + (i - first) *Theme::Metric::CompactStep, listing[i].title, !sidebar);
        }
        if (!sidebar && (hit & PAD_CROSS))
            open_item();
        // Preserve the existing sort command, freeing R1 for section navigation.
        if ((hit & PAD_SQUARE) && !page_path.empty())
            {library_tools=true;tools_focus=0;}
        if(page_path=="cloud-watchlist"){if((hit & PAD_R2) && page_start+60<page_total)show_cloud(page_start+60);if((hit & PAD_L2) && page_start>=60)show_cloud(page_start-60);return;}
        if ((hit & PAD_R2) && static_cast<int64_t>(page_start) + (plex::diagnostic(TenItems)?10:60) < page_total)
            load(page_path, page_title, false, page_start + (plex::diagnostic(TenItems)?10:60));
        if ((hit & PAD_L2) && page_start >= (plex::diagnostic(TenItems)?10:60))
            load(page_path, page_title, false, page_start - (plex::diagnostic(TenItems)?10:60));
        if (page_total > 60)
            draw_text(390, 920, Theme::Font::Caption,
                      "Square Sort • L2/R2 Pages • " + std::to_string(page_start + 1) + "–" +
                          std::to_string(page_start + listing.size()) + " of " +
                          std::to_string(page_total));
    }
    draw_text(390, 985, Theme::Font::Caption,"Triangle: connection diagnostics • Square: library sort/filter or manual server");
    draw_text(390, 1020, Theme::Font::Caption,
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

bool ui_sensitive_input() {return profile_pin;}

void ui_shutdown() {
    ++screen_generation;++feed_generation;
    retire_stream();
    audio_reset();
    if(video){player_stop();video=false;}
    // Waiting is confined to application exit, never the rendering/input loop.
    if(pending.valid()){pending.wait();pending=std::future<Result>{};}
    job=None;
    if(artwork){artwork->clear();artwork->update();artwork.reset();}
    client.reset();plat_pad_set_private(false);
}

extern "C" unsigned plex_home_last_checkpoint(){return plex::home_last.load();}
void ui_frame_presented(){if(plex::home_draw_ready.exchange(false))plex::home_checkpoint(13,"Home frame submitted; display acceptance pending");}
