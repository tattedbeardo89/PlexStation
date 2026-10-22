// SPDX-License-Identifier: GPL-3.0-or-later
// PlexStation controller-first interface; original VLC playback remains in player.cc.
#include "gfx.h"
#include "imgui.h"
#include "player.h"
#include "plex/artwork.h"
#include "plex/client.h"
#include "plex/diagnostics.h"
#include "plex/home.h"
#include "plex/home_feed.h"
#include "plex/audio.h"
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
int settings_page=0;
uint64_t screen_generation=0, pending_generation=0;
bool grid_checkpoint=false;
bool home_screen=false, library_tools=false;
std::vector<Item> home_sections;
std::vector<Hub> feed_rows;
std::vector<int> feed_columns;
std::vector<float> feed_scroll;
int feed_focus=0, tools_focus=0, requested_section=0;
float feed_vertical=0;
std::string feed_error, details_target;
bool full_description=false;float description_scroll=0;
std::atomic<uint64_t> feed_generation{0};
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
    HomeFeed,
    HomeUsers,
    HomeSwitch,
    ValidateProfile,
    Resources,
    Select,
    Manual,
    Sections,
    Browse,
    Details,
    Timeline,
    AudioBrowse, AudioQueueLoad, AudioResolve, AudioScrobble,
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
void audio_reset();
void audio_leave_browse();
void audio_finish(int kind,const Result &result);
void audio_tick(double now);
bool audio_frame(uint32_t hit,float dt);
bool audio_available(bool books=false);
void audio_home(bool books=false);
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
                            "Watchlist", "Search", "Settings", "Music", "Audiobooks"};
bool nav_visible(int i) {return i!=4 && (i!=7 || audio_available()) && (i!=8 || audio_available(true));}
int nav_step(int i,int direction,bool wrap=false) {
    for(int n=0;n<10;++n) {i+=direction;if(wrap)i=(i+9)%9;else i=std::clamp(i,-1,8);if(i<0 || nav_visible(i))return i;}return 0;
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
    submit(Browse, [b, t, path, start] { return client->browse(b, t, path, start); });
}
void connect_server(const Server &s) {
    if(job!=None) return;
    connection_target=s; next_attempts.clear(); next_base.clear(); manual_tested=false;
    connection_diagnostics=false; reconnect_screen=true; discard_connection=false;
    message.clear(); bool http=allow_http;
    submit(Select,[s,http]{return client->select(s,http,next_base,&next_attempts);});
}
void connected(const Json &data) {
    audio_reset();
    base=next_base; selected_server=connection_target; server_token=connection_target.token;
    save_account_pref("plex_preferred_server",selected_server.id);
    artwork->clear(); history.clear(); detail={}; playing={};
    progress_pending=false; progress={};
    available.clear(); settings=false; settings_page=0; in_detail=false; sidebar=false; focus=0; nav=0;
    reconnect_screen=false; connection_diagnostics=false; editor=0;
    listing=items(data,false); page_path="/library/sections"; page_title="Libraries";
    page_start=0; page_total=listing.size();
    home_sections=listing; home_screen=true;feed_rows.clear();feed_columns.clear();feed_scroll.clear();feed_focus=0;feed_vertical=0;
    if(listing.empty()) message="Connected; no accessible libraries were returned.";
    load_home();
}
void load_home() {
    if(job!=None || base.empty())return;
    auto b=base,t=server_token;auto sections=home_sections;
    auto epoch=++feed_generation;feed_error.clear();home_screen=true;
    submit(HomeFeed,[b,t,sections,epoch] {return home_feed(*client,b,t,sections,[epoch]{return feed_generation.load()==epoch;});});
}
void refresh() {
    if(!primary_id.empty() && profile_token.empty()) {profile_screen=true;profile_error="Select and authorize a profile first.";return;}
    auto token = primary_id.empty() ? account : profile_token;
    submit(Resources, [token] { return client->resources(token); });
}
void clear_profile() {
    audio_reset();
    if(video) {player_stop();video=false;}
    artwork->clear(); ++screen_generation; ++feed_generation;
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
    profile_screen=false;profile_pin=false;profile_error.clear();erase_pin();
    account_loading=true;refresh();
}
void choose_profile() {
    if(job!=None)return;
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
    audio_reset();
    artwork->clear();
    profile_token.clear();primary_id.clear();active_profile={};requested_profile={};home_profiles.clear();profile_screen=false;profile_pin=false;deferred_profiles=false;erase_pin();
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
    if((done==Browse || done==Sections || done==Details || done==HomeFeed || done==AudioBrowse || done==AudioQueueLoad || done==AudioResolve) && pending_generation!=screen_generation) return;
    if(done==AudioBrowse || done==AudioQueueLoad || done==AudioResolve || done==AudioScrobble) {audio_finish(done,result);return;}
    if ((done == PinStart || done == PinPoll) && auth_request_generation != auth.generation)
        return; // A cancelled request must never authorize a later session.
    if (cancel_bootstrap && (done == ValidateAccount || done == HomeUsers || done == Resources)) {
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
    if (done == ValidateAccount) {
        if (result) {
            auto id=number(result.data,"id",0);
            if(id<=0) { account_error="Plex account identity is missing. Retry sign-in validation."; return; }
            clear_profile();primary_id="id:"+std::to_string(id);
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
    if(done==HomeFeed) {
        if(!result) {feed_error=result.error;return;}
        feed_rows=home_rows(result.data);feed_columns.assign(feed_rows.size(),0);feed_scroll.assign(feed_rows.size(),0);
        grid_checkpoint=false;feed_focus=0;feed_vertical=0;home_screen=true;
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
        home_profiles=profiles(result.data); profiles_ready();return;
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
        in_detail = true;
        focus = 0;
        version = 0;
    }
}
void open_media(const Item &item) {
    if(job!=None)return;
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
    if (page_path == "/library/sections") {
        diagnostic_lines=0; checkpoint(1,"library selection");
        load("/library/sections/" + encode(item.id) + "/all", item.title);
    } else if(item.type=="show" || item.type=="season" || item.type=="movie" || item.type=="episode")open_media(item);
    else message="This media type is not supported by the current playback adapter.";
}
void play() {
    audio_reset();
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
bool custom_focus_valid() {
    if(profile_screen)return profile_pin ? pin_focus>=0 && pin_focus<10 : profile_focus>=0 && profile_focus<static_cast<int>(home_profiles.size());
    if(video)return !tracks || (track_focus>=0 && track_focus<2);
    if(editor)return edit_focus>=0 && edit_focus<static_cast<int>(alphabet.size());
    if(sidebar)return nav>=-1 && nav<9 && (nav<0 || nav_visible(nav));
    if(settings)return focus>=0 && focus<(settings_page==2 ? 10 : settings_page==3 ? 2 : settings_page ? 1 : 9);
    if(in_detail)return focus>=0 && focus<3;
    if(!available.empty())return focus>=0 && focus<static_cast<int>(available.size());
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
        draw->AddRectFilled({380*scale, 5*scale}, {1900*scale, 39*scale}, IM_COL32(0,0,0,210));
        draw->AddText(ImGui::GetFont(), 19*scale, {390*scale, 10*scale}, IM_COL32(235,220,150,255), line);
    }
};
void centered(float y, float size, const std::string &text, float center = 960) {
    float width = ImGui::GetFont()->CalcTextSizeA(size, 10000, 0, text.c_str()).x;
    draw_text(center - width / 2, y, size, text);
}
void poster(float x,float y,float width,float height,const Item &item,bool selected) {
    float scale=gfx.height/1080.f;
    rect(x-5,y-5,width+10,height+10,selected ? IM_COL32(230,166,47,255) : IM_COL32(32,34,41,255));
    auto texture=item.type=="episode" ? artwork->landscape(base,server_token,item.thumb) : artwork->get(base,server_token,item.thumb);
    if(texture) {
        float ratio=item.type=="episode" ? 16.f/9 : 2.f/3;
        float image_width=std::min(width,height*ratio);
        float image_height=image_width/ratio;
        float left=x+(width-image_width)*.5f;
        float top=y+(height-image_height)*.5f;
        ImGui::GetBackgroundDrawList()->AddImage(texture,{left*scale,top*scale},{(left+image_width)*scale,(top+image_height)*scale});
    }
    else {rect(x,y,width,height,IM_COL32(38,40,48,255));draw_text(x+12,y+height*.45f,22,item.thumb.empty()?"No poster":"Artwork unavailable");}
    if(item.watched) {rect(x+width-48,y+8,40,32,IM_COL32(160,105,20,245));draw_text(x+width-38,y+10,22,"✓");}
    if(item.duration>0 && item.offset>0) {
        float progress=std::clamp(static_cast<float>(item.offset)/item.duration,0.f,1.f);
        rect(x,y+height-8,width,8,IM_COL32(60,60,65,255));rect(x,y+height-8,width*progress,8,IM_COL32(235,170,40,255));
    }
}
void home_frame(uint32_t hit,float dt) {
    if(!feed_rows.empty() && !grid_checkpoint) {checkpoint(15,"first Home carousel",feed_rows.size());grid_checkpoint=true;}
    if(feed_rows.empty()) {
        draw_text(390,250,32,job==HomeFeed ? "Loading your Home..." : "No Home items are available");
        draw_text(390,330,25,"Open Movies or TV Shows to browse your libraries. Cross: refresh Home");
        if(!feed_error.empty())draw_text(390,420,23,feed_error);
        if(hit & PAD_LEFT)sidebar=true;
        if((hit & PAD_CROSS) && job==None)load_home();
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
        draw_text(390,180,32,selected.title);
        int first=std::max(0,column-4);
        for(int i=first;i<static_cast<int>(selected.entries.size()) && i<first+7;++i)row(i,column,250+(i-first)*85,selected.entries[i].title);
        if((hit & PAD_CROSS) && job==None && !sidebar && !selected.entries.empty())open_media(selected.entries[column]);
        return;
    }
    float amount=std::clamp(dt*14,0.f,1.f);
    float target=static_cast<float>(std::max(0,feed_focus-1));feed_vertical+=(target-feed_vertical)*amount;
    float scale=gfx.height/1080.f;auto *draw=ImGui::GetBackgroundDrawList();
    draw->PushClipRect({365*scale,155*scale},{1890*scale,938*scale},true);
    for(int r=std::max(0,feed_focus-2);r<static_cast<int>(feed_rows.size()) && r<=feed_focus+2;++r) {
        auto &hub=feed_rows[r];float y=170+(r-feed_vertical)*390;
        if(y>938 || y+390<155)continue;
        draw_text(390,y,31,hub.title);
        float dest=static_cast<float>(std::max(0,feed_columns[r]-4));feed_scroll[r]+=(dest-feed_scroll[r])*amount;
        for(int c=std::max(0,static_cast<int>(feed_scroll[r]));c<static_cast<int>(hub.entries.size()) && c<static_cast<int>(feed_scroll[r])+5;++c) {
            auto &item=hub.entries[c];float x=390+(c-feed_scroll[r])*280;
            if(plex::diagnostic(TextOnly))row(c,feed_columns[r],y+55+(c%3)*80,item.title);
            else {
                poster(x,y+60,240,245,item,!sidebar && r==feed_focus && c==feed_columns[r]);
                auto title=item.title;if(title.size()>27)title=title.substr(0,24)+"...";
                draw_text(x,y+315,22,title);
                std::string secondary=item.type=="episode" ? item.series+(item.season>0 ? " S"+std::to_string(item.season) : "")+(item.episode>0 ? " E"+std::to_string(item.episode) : "") : item.year ? std::to_string(item.year) : "";
                if(hub.continuing && item.duration>item.offset)secondary+="  "+std::to_string((item.duration-item.offset)/60000)+" min left";
                if(secondary.size()>40)secondary=secondary.substr(0,37)+"...";
                draw_text(x,y+347,18,secondary,IM_COL32(165,167,177,255));
            }
        }
    }
    draw->PopClipRect();
    if((hit & PAD_CROSS) && job==None && !sidebar && !selected.entries.empty())open_media(selected.entries[column]);
}
#include "plex_audio_ui.h"

void profile_screen_frame(uint32_t hit) {
    rect(0,0,1920,1080,IM_COL32(12,13,17,255));
    centered(130,58,profile_pin ? "Enter profile PIN" : "Who's Watching?");
    if(!profile_error.empty())centered(880,25,profile_error);
    if(job!=None)centered(820,28,deferred_profiles ? "Finishing previous user request..." : "Authorizing profile...");
    if(hit & PAD_CIRCLE) {
        if(job!=None) {deferred_profiles=true;return;}
        if(profile_pin) {profile_pin=false;erase_pin();profile_error.clear();}
        else {profile_error="Select a profile to continue. Options: sign out.";}
        return;
    }
    if((hit & PAD_OPTIONS) && job==None) {signout();return;}
    if(profile_pin) {
        centered(260,36,requested_profile.name);
        centered(330,50,std::string(pin_digits.size(),'*')+std::string(4-pin_digits.size(),'_'));
        if(job==None) {
            if(hit & PAD_LEFT)pin_focus=std::max(0,pin_focus-1);
            if(hit & PAD_RIGHT)pin_focus=std::min(9,pin_focus+1);
            if(hit & PAD_UP)pin_focus=std::max(0,pin_focus-5);
            if(hit & PAD_DOWN)pin_focus=std::min(9,pin_focus+5);
            if((hit & PAD_SQUARE) && !pin_digits.empty())pin_digits.pop_back();
            if((hit & PAD_CROSS) && pin_digits.size()<4) {pin_digits+=static_cast<char>('0'+pin_focus);if(pin_digits.size()==4)choose_profile();}
        }
        for(int i=0;i<10;++i) {float x=590+(i%5)*150,y=440+(i/5)*130;rect(x,y,115,95,i==pin_focus?IM_COL32(180,120,20,255):IM_COL32(32,34,40,255));draw_text(x+40,y+25,42,std::to_string(i));}
        centered(740,24,"Cross: digit   Square: erase   Circle: cancel");return;
    }
    if(home_profiles.empty()) {
        centered(390,30,"Loading Plex Home profiles");
        if(job==None && (hit & PAD_CROSS)) {profile_error.clear();auto token=account;submit(HomeUsers,[token]{return client->home_users(token);});}
        centered(550,25,"Cross: retry   Options: sign out");return;
    }
    if(job==None) {
        if(hit & PAD_LEFT)profile_focus=std::max(0,profile_focus-1);
        if(hit & PAD_RIGHT)profile_focus=std::min(static_cast<int>(home_profiles.size())-1,profile_focus+1);
        if(hit & PAD_UP)profile_focus=std::max(0,profile_focus-5);
        if(hit & PAD_DOWN)profile_focus=std::min(static_cast<int>(home_profiles.size())-1,profile_focus+5);
    }
    int first=(profile_focus/5)*5;
    int count=std::min(5,static_cast<int>(home_profiles.size())-first);
    for(int i=first;i<first+count;++i) {
        float x=960-count*150+(i-first)*300;
        rect(x,310,260,300,i==profile_focus?IM_COL32(165,110,20,255):IM_COL32(30,32,39,255));
        auto avatar=artwork->avatar(home_profiles[i].avatar,account);
        if(avatar) {
            float scale=gfx.height/1080.f;ImGui::GetBackgroundDrawList()->AddImage(avatar,{(x+50)*scale,340*scale},{(x+210)*scale,500*scale});
        } else centered(390,70,"●",x+130);
        centered(510,29,home_profiles[i].name,x+130);
        if(home_profiles[i].protected_user)centered(560,23,"PIN protected",x+130);
    }
    centered(720,26,"D-pad / Stick: choose   Cross: select   Options: sign out");
    if((hit & PAD_CROSS) && job==None) {requested_profile=home_profiles[profile_focus];profile_error.clear();choose_profile();}
}
void login_screen(uint32_t hit, double now) {
    rect(0, 0, 1920, 1080, IM_COL32(12,13,17,255));
    draw_text(80, 75, 38, "PlexStation", IM_COL32(235,171,54,255));
    centered(170, 52, "Sign in to Plex");
    rect(220, 265, 1480, 625, IM_COL32(24,26,32,255));
    if (account_loading) {
        centered(365, 38, job == ValidateAccount ? "Connecting to Plex" : "Authorization successful");
        centered(460, 30, account_error.empty() ? "Loading Plex servers..." : "Connection failed");
        if (!account_error.empty()) centered(545, 25, account_error);
        if (!credentials_saved) centered(610, 25, "Sign-in could not be saved. Check storage permissions.");
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
            centered(325, 30, "1. Open https://plex.tv/link on your phone or computer", 790);
            centered(390, 29, "2. Enter this activation code", 790);
            centered(465, 110, auth.current.code, 790);
        } else {
            centered(365, 34, "Scan the QR code to authorize PlexStation", 790);
            centered(440, 27, "Browser sign-in: app.plex.tv/auth", 790);
            centered(500, 25, "Use the QR code to open this exact sign-in session", 790);
        }
        if (have_qr) {
            int n = qrcodegen_getSize(qr);
            // Integral display pixels and four-module quiet zone at 1080p/4K.
            float scale = gfx.height / 1080.f;
            int cell = std::max(1, static_cast<int>(350 * scale / (n + 8)));
            float x = 1350 * scale, y = 340 * scale;
            auto *draw = ImGui::GetBackgroundDrawList();
            draw->AddRectFilled({x,y},{x+(n+8)*cell,y+(n+8)*cell},IM_COL32(255,255,255,255));
            for (int row = 0; row < n; ++row) for (int col = 0; col < n; ++col)
                if (qrcodegen_getModule(qr,col,row))
                    draw->AddRectFilled({x+(col+4)*cell,y+(row+4)*cell},
                        {x+(col+5)*cell,y+(row+5)*cell},IM_COL32(0,0,0,255));
        }
        centered(660, 31, "Waiting for authorization");
        centered(720, 26, "Expires in " + std::to_string(auth.remaining(now)) + " seconds");
        centered(775, 24, "PlexStation will continue automatically after approval");
        if (hit & PAD_CIRCLE) { auth.cancel(); have_qr=false; }
    } else {
        bool busy = job == PinStart || job == PinPoll;
        const char *status = auth.state == AuthState::Generating ? "Connecting to Plex / Generating activation code" :
            auth.state == AuthState::Expired ? "Authentication expired" :
            auth.state == AuthState::Failed ? "Connection or authentication failed" :
            auth.state == AuthState::Cancelled ? "Linking cancelled" : "Your media, on your television.";
        centered(380, 36, status);
        if (!auth.error.empty()) centered(475, 25, auth.error);
        if (busy) centered(560, 26, "Waiting for the current request to finish...");
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
    centered(950, 25, "Cross: Sign in / Retry     Circle: Cancel / Back");
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
    diagnostic_flags=static_cast<unsigned>(pref_int("plex_developer_flags_v1",0)) & 63u;
    account_id.clear();primary_id.clear();profile_token.clear();profile_screen=false;
    auto &io = ImGui::GetIO();
    auto font = std::string(plat_data_dir()) + "/fonts/Inter-Regular.ttf";
    if (access(font.c_str(), R_OK) == 0)
        io.Fonts->AddFontFromFileTTF(font.c_str(), 28);
    else
        io.Fonts->AddFontDefault();
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
    audio_tick(now);
    auth.tick(now);
    if (auth.poll_due(now) && job == None) {
        auto p = auth.current;
        auth_request_generation = auth.generation;
        submit(PinPoll, [p] { return client->poll_pin(p.id, p.code); });
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
        auto item = progress_item;
        auto pos = progress_time;
        progress_pending = false;
        submit(Timeline,
               [b, t, state, item, pos] { return client->timeline(b, t, item, state, pos); });
    }
    if (video)
        return;
    if (was_video) hit = 0; // The stop/back press must not also leave details.
    artwork->update();
    if (account.empty() || account_loading) { login_screen(hit, now); return; }
    if(profile_screen) {profile_screen_frame(hit);return;}
    rect(0, 0, 1920, 1080, IM_COL32(12, 13, 17, 255));
    rect(0, 0, 330, 1080, IM_COL32(20, 21, 26, 255));
    draw_text(45, 55, 38, "PlexStation", IM_COL32(235, 171, 54, 255));
    if(sidebar && nav==-1)rect(25,100,280,44,IM_COL32(120,78,16,255));
    draw_text(45,112,22,active_profile.name.empty() ? "Profile" : active_profile.name);
    for (int i=0,slot=-1;i<9;++i) {
        if(!nav_visible(i))continue;
        ++slot;
        if (sidebar && nav == i)
            rect(25, 150 + slot * 78, 280, 65, IM_COL32(120, 78, 16, 255));
        float scale=gfx.height/1080.f;auto *draw=ImGui::GetBackgroundDrawList();float y=182+slot*78;
        ImU32 color=nav==i ? IM_COL32(235,171,54,255) : IM_COL32(160,162,174,255);
        if(i==0) {draw->AddTriangle({43*scale,y*scale},{55*scale,(y-12)*scale},{67*scale,y*scale},color,2*scale);draw->AddRect({47*scale,y*scale},{63*scale,(y+13)*scale},color,0,0,2*scale);}
        else if(i==5) {draw->AddCircle({53*scale,(y-2)*scale},8*scale,color,12,2*scale);draw->AddLine({59*scale,(y+5)*scale},{68*scale,(y+14)*scale},color,2*scale);}
        else {draw->AddRect({44*scale,(y-12)*scale},{66*scale,(y+12)*scale},color,3*scale,0,2*scale);for(int line=0;line<std::min(i,3);++line)draw->AddLine({48*scale,(y-6+line*6)*scale},{62*scale,(y-6+line*6)*scale},color,scale);}
        draw_text(83,166+slot*78,28,navigation[i]);
    }
    draw_text(390, 65, 46, settings ? (settings_page==3 ? "Profile" : "Settings") : in_detail ? detail.title : home_screen ? "Home" : page_title);
    if(!active_profile.id.empty()) {auto avatar=artwork->avatar(active_profile.avatar,account);if(avatar){float scale=gfx.height/1080.f;ImGui::GetBackgroundDrawList()->AddImage(avatar,{270*scale,105*scale},{306*scale,141*scale});}}
    if (job != None)
        draw_text(390, 135, 26, "Loading…");
    if (!message.empty())
        draw_text(390, 950, 25, message, IM_COL32(240, 185, 90, 255));
    if(audio_frame(hit,dt))return;
    if(manual_confirm_http) {
        draw_text(390,230,34,"Approve unencrypted local HTTP?");
        draw_text(390,310,26,"Traffic and credentials are not encrypted on this connection.");
        draw_text(390,370,26,"HTTPS is recommended. No automatic downgrade is performed.");
        draw_text(390,470,27,"Cross: approve and test   Circle: cancel");
        if(hit & PAD_CIRCLE) manual_confirm_http=false;
        else if((hit & PAD_CROSS) && job==None) {
            allow_http=true; pref_set("plex_allow_http",1); manual_confirm_http=false; test_manual();
        }
        return;
    }
    if(connection_diagnostics) {
        rect(360,50,1510,960,IM_COL32(12,13,17,255));
        draw_text(390,70,38,"Server Connection Diagnostics");
        if(hit & PAD_UP) diagnostic_focus=std::max(0,diagnostic_focus-1);
        if(hit & PAD_DOWN) diagnostic_focus=std::min(std::max(0,static_cast<int>(connection_attempts.size())-1),diagnostic_focus+1);
        if(!connection_attempts.empty()) {
            const auto &d=connection_attempts[diagnostic_focus];
            draw_text(390,175,28,std::to_string(diagnostic_focus+1)+" / "+std::to_string(connection_attempts.size())+"   "+d.endpoint);
            draw_text(390,240,27,d.route+"   "+d.protocol+"   port "+std::to_string(d.port));
            draw_text(390,325,25,"TCP: "+d.tcp);
            draw_text(390,405,25,"TLS: "+d.tls);
            draw_text(390,485,25,"Identity: "+d.identity);
            draw_text(390,565,25,"Libraries: "+d.libraries);
            draw_text(390,660,25,"Result: "+d.result);
        } else draw_text(390,250,28,"No completed endpoint attempts yet.");
        if(job==Select || job==Manual) draw_text(390,750,25,"Testing permitted endpoints on the network worker...");
        draw_text(390,845,25,manual_tested ? "Cross: save verified endpoint and connect   Circle: back" : "Up/Down: endpoint attempts   Circle: back");
        if(hit & PAD_CIRCLE) {connection_diagnostics=false; manual_tested=false;}
        else if(manual_tested && (hit & PAD_CROSS) && job==None) {
            save_account_pref("plex_manual_uri",next_base); save_account_pref("plex_manual_server_id",connection_target.id);
            connected(manual_sections);
            connection_diagnostics=false; manual_tested=false; message="Verified endpoint saved; libraries loaded.";
        }
        return;
    }
    if(reconnect_screen) {
        draw_text(390,230,36,job==Select ? "Connecting to Server" : "Could not connect to your server");
        draw_text(390,310,28,connection_target.name);
        draw_text(390,430,25,job==Select ? "Circle: cancel   Square: change server" : "Cross: retry   Square: change server   Circle: server picker");
        draw_text(390,500,25,"Triangle: endpoint diagnostics");
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
        draw_text(390, 180, 30,
                  editor == 1 ? "Search your server"
                              : "Manual server: [http(s)://]host[:port] (default HTTPS / 32400)");
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
        draw_text(390, 870, 24, editor==1 ? "Cross: type • Square: erase • Triangle: submit • Circle: cancel" : "Cross: type • Square: erase • Triangle: test before saving • Circle: cancel");
        if (hit & PAD_CIRCLE)
            editor = 0;
        if ((hit & PAD_TRIANGLE) && job == None) {
            if (editor == 3) {audio_load("/search?type=10&query="+encode(edit_text),"Music Search");editor=0;}
            else if (editor == 1) {
                load("/search?query=" + encode(edit_text), "Search");
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
        rect(360,155,1510,780,IM_COL32(16,18,24,255));
        draw->PushClipRect({390*scale,190*scale},{1800*scale,870*scale},true);
        draw->AddText(ImGui::GetFont(),28*scale,{390*scale,(200-description_scroll)*scale},IM_COL32(235,235,240,255),detail.summary.c_str(),nullptr,1350*scale);draw->PopClipRect();
        draw_text(390,890,24,"Up/Down: scroll   Circle/Square: close");
        if(hit & (PAD_CIRCLE|PAD_SQUARE))full_description=false;
        return;
    }
    if(in_detail && (hit & PAD_SQUARE)) {full_description=true;description_scroll=0;return;}
    if(library_tools) {
        const char *labels[]={"Title A–Z","Recently Added","Release Date (newest)","Unwatched Only","All Items","Search"};
        if(hit & PAD_CIRCLE)library_tools=false;
        if(hit & PAD_UP)tools_focus=std::max(0,tools_focus-1);
        if(hit & PAD_DOWN)tools_focus=std::min(5,tools_focus+1);
        for(int i=0;i<6;++i)row(i,tools_focus,210+i*100,labels[i]);
        if((hit & PAD_CROSS) && job==None) {
            library_tools=false;
            if(tools_focus==5){editor=1;edit_text.clear();edit_focus=0;}
            else {const char *queries[]={"?sort=titleSort:asc","?sort=addedAt:desc","?sort=originallyAvailableAt:desc","?unwatched=1",""};load(page_path.substr(0,page_path.find('?'))+queries[tools_focus],page_title,false);}
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
    if (hit & PAD_CIRCLE) {
        ++screen_generation; ++feed_generation;
        if (settings) { if(settings_page) {focus=settings_page==1 ? 6 : settings_page==3 ? 8 : 7; settings_page=0;} else {settings = false; sidebar = true; focus = 0;} }
        else if (in_detail) { in_detail = false; sidebar = false; }
        else if (!history.empty() && (job==None || job==Browse || job==Sections || job==Details)) {
            auto p = history.back(); history.pop_back();
            page_title = p.title; page_path = p.path; listing = std::move(p.items);
            focus = std::clamp(p.focus,0,std::max(0,static_cast<int>(listing.size())-1)); page_start = p.start; page_total = p.total; home_screen=p.home;in_detail=p.show_details;if(p.show_details)detail=std::move(p.previous_detail);sidebar = false;
        } else sidebar = true;
        hit &= ~PAD_CIRCLE; // One back action per frame.
    }
    if (sidebar) {
        if (hit & PAD_UP)
            {nav=nav_step(nav,-1);}
        if (hit & PAD_DOWN)
            {nav=nav_step(nav,1);}
        if ((hit & PAD_CROSS) && job == None) {
            if(nav==-1) {settings=true;settings_page=3;sidebar=false;focus=0;return;}
            if(nav==7 || nav==8) {audio_home(nav==8);return;}
            audio_leave_browse();
            settings = nav == 6; settings_page=0;
            in_detail = false;
            sidebar = false;
            focus = 0;
            if (nav == 0) {load_home();}
            else if (nav == 3)
                load("/library/onDeck", "Continue Watching");
            else if (nav == 1 || nav == 2) {
                auto b = base, t = server_token; requested_section=nav;
                submit(Sections, [b, t] { return client->sections(b, t); });
            } else if (nav == 5) {
                editor = 1;
                edit_text.clear();
                edit_focus = 0;
            } else if (nav == 4)
                message = "Watchlist integration is not implemented yet";
        }
    } else if (settings) {
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
            const char *labels[]={"Show Controller Diagnostics","Text-Only Library Mode","Disable Artwork Downloads","Disable Artwork Decoding","Disable Artwork GPU Uploads","Limit Library Results to 10","Detailed Library Tracing","Server Connection Diagnostics","Library / Artwork Diagnostics","Application Build Information"};
            if(hit & PAD_UP) focus=std::max(0,focus-1);
            if(hit & PAD_DOWN) focus=std::min(9,focus+1);
            int first=std::max(0,focus-6);
            for(int i=first;i<10 && i<first+7;++i) {
                bool enabled=i==0 ? controller_overlay : i<=6 ? !!(diagnostic_flags.load() & (1u<<(i-1))) : false;
                row(i,focus,190+(i-first)*90,std::string(labels[i])+(i<=6 ? (enabled ? ": On" : ": Off") : ""));
            }
            if(hit & PAD_CROSS) {
                if(focus==0) {controller_overlay=!controller_overlay; pref_set("plex_controller_overlay_v2",controller_overlay?1:0);}
                else if(focus<=6) {diagnostic_flags.fetch_xor(1u<<(focus-1));pref_set("plex_developer_flags_v1",static_cast<int>(diagnostic_flags.load()));artwork->clear();diagnostic_lines=0;}
                else if(focus==7) {connection_diagnostics=true;diagnostic_focus=0;}
                else if(focus==8) message="Library CP01-CP15; artwork cached="+std::to_string(artwork->cached())+" worker="+(artwork->busy()?"busy":"idle")+"; logs: plexstation.log / vlc-crash.txt.";
                else message=std::string("PlexStation ")+PLEX_BUILD_COMMIT+"; PPSA85301; native VLC; hardware verification pending";
            }
            return;
        }
        if(settings_page==1) {
            row(0,0,210,"Change Server");
            if((hit & PAD_CROSS) && job==None) {force_picker=true; refresh();}
            return;
        }
        if (hit & PAD_UP)
            focus = std::max(0, focus - 1);
        if (hit & PAD_DOWN)
            focus = std::min(8, focus + 1);
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
            if(focus==8) {settings_page=3;focus=0;}
            if(focus==6 || focus==7) {settings_page=focus==6 ? 1 : 2; focus=0;}
            pref_set(account_key("plex_audio_delay").c_str(), global_delay);
            pref_set("plex_allow_http", allow_http ? 1 : 0);
        }
        row(0, focus, 180, "Default audio delay +25 ms (" + std::to_string(global_delay) + " ms)");
        row(1, focus, 260, "Default audio delay -25 ms");
        row(2, focus, 340, allow_http ? "Local HTTP: allowed" : "Local HTTP: disabled");
        row(3, focus, 420, "Refresh servers");
        row(4, focus, 500, "Sign out");
        row(5, focus, 580, "Manual server address");
        row(6,focus,660,"Server");
        row(7,focus,740,"Developer Options");
        row(8,focus,820,"Profiles");
    } else if (!available.empty()) {
        if (hit & PAD_UP)
            focus = std::max(0, focus - 1);
        if (hit & PAD_DOWN)
            focus = std::min(static_cast<int>(available.size()) - 1, focus + 1);
        int first = std::max(0, focus - 7);
        for (int i = first; i < static_cast<int>(available.size()) && i < first + 8; ++i)
            row(i, focus, 210 + (i - first) * 85, available[i].name);
        if ((hit & PAD_CROSS) && job == None) {
            connect_server(available[std::clamp(focus,0,static_cast<int>(available.size())-1)]);
        }
    } else if (in_detail) {
        if(!plex::diagnostic(TextOnly)) {
            poster(1560,220,250,340,detail,false);
            auto backdrop=artwork->landscape(base,server_token,detail.art);
            if(backdrop) {float scale=gfx.height/1080.f;auto *draw=ImGui::GetBackgroundDrawList();draw->AddImage(backdrop,{385*scale,155*scale},{1510*scale,540*scale},ImVec2(0,0),ImVec2(1,1),IM_COL32(255,255,255,35));}
        }
        draw_text(390, 220, 25,
                  (detail.year ? std::to_string(detail.year)+" • " : "")+(detail.duration>0 ? std::to_string(detail.duration/60000)+" min" : ""));
        float scale = gfx.height / 1080.f;
        ImGui::GetBackgroundDrawList()->AddText(
            ImGui::GetFont(), 28 * scale, {390 * scale, 280 * scale}, IM_COL32(225, 225, 230, 255),
            (detail.summary.size()>500 ? detail.summary.substr(0,500)+"... (Square: read more)" : detail.summary).c_str(), nullptr, 1100 * scale);
        row(0, focus, 640, detail.type=="show" ? "Browse seasons" : detail.type=="season" ? "Browse episodes" : detail.offset > 0 ? "Resume" : "Play");
        std::string tags=detail.content_rating;
        for(const auto &genre:detail.genres) {if(!tags.empty())tags+=" • ";tags+=genre;}
        draw_text(390,570,22,tags.size()>110 ? tags.substr(0,107)+"..." : tags);
        std::string cast="Cast: ";for(const auto &name:detail.cast){if(cast.size()>180)break;if(cast.size()>6)cast+=", ";cast+=name;}
        if(!detail.cast.empty())draw_text(390,610,20,cast.size()>130 ? cast.substr(0,127)+"..." : cast);
        row(1, focus, 725, detail.watched ? "Mark unwatched" : "Mark watched");
        row(2,focus,810,detail.type=="show" || detail.type=="season" ? "Read description" : detail.media.is_array() && !detail.media.empty() ? "Playback version: "+std::to_string(version+1)+" / "+std::to_string(detail.media.size())+" (Cross cycles)" : "No playable versions provided");
        if (hit & PAD_UP)
            focus = std::max(0, focus - 1);
        if (hit & PAD_DOWN)
            focus = std::min(2, focus + 1);
        if ((hit & PAD_CROSS) && job == None) {
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
    } else if(home_screen) {home_frame(hit,dt);}
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
                draw_text(x + 7, y + 285, 23, label);
                draw_text(x+7,y+315,18,listing[i].type=="episode" ? (listing[i].season>0 ? "S"+std::to_string(listing[i].season)+" " : "")+(listing[i].episode>0 ? "E"+std::to_string(listing[i].episode) : "Episode") : listing[i].year ? std::to_string(listing[i].year) : "");
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
            {library_tools=true;tools_focus=0;}
        if ((hit & PAD_R2) && static_cast<int64_t>(page_start) + (plex::diagnostic(TenItems)?10:60) < page_total)
            load(page_path, page_title, false, page_start + (plex::diagnostic(TenItems)?10:60));
        if ((hit & PAD_L2) && page_start >= (plex::diagnostic(TenItems)?10:60))
            load(page_path, page_title, false, page_start - (plex::diagnostic(TenItems)?10:60));
        if (page_total > 60)
            draw_text(390, 920, 22,
                      "Square Sort • L2/R2 Pages • " + std::to_string(page_start + 1) + "–" +
                          std::to_string(page_start + listing.size()) + " of " +
                          std::to_string(page_total));
    }
    draw_text(390, 985, 21,"Triangle: connection diagnostics • Square: library sort/filter or manual server");
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

bool ui_sensitive_input() {return profile_pin;}

void ui_shutdown() {
    ++screen_generation;++feed_generation;
    audio_reset();
    if(video){player_stop();video=false;}
    // Waiting is confined to application exit, never the rendering/input loop.
    if(pending.valid()){pending.wait();pending=std::future<Result>{};}
    job=None;
    if(artwork){artwork->clear();artwork->update();artwork.reset();}
    client.reset();plat_pad_set_private(false);
}
