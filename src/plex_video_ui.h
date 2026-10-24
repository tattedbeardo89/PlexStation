// SPDX-License-Identifier: GPL-3.0-or-later
struct CleanupTask {std::string base,token,path;};
std::vector<CleanupTask> cleanups;
StreamPlan stream_plan;
SubtitleSelection subtitle_selection;
bool subtitle_menu=false;
void choose_subtitle_mode(int mode);
bool stream_fallback=false,video_info=false,video_retry_pending=false;
int video_info_focus=0;
uint64_t stream_serial=0;
Item next_candidate;bool next_requested=false;double autoplay_deadline=0;
std::string new_stream_id(){return "ps-"+device+"-"+std::to_string(std::time(nullptr))+"-"+std::to_string(++stream_serial);}
void retire_stream() {
    if(!stream_plan.session.empty() && cleanups.size()<8)cleanups.push_back({base,server_token,"/video/:/transcode/universal/stop?session="+encode(stream_plan.session)});
    stream_plan={};video_info=false;
}
void service_cleanup() {
    if(job!=None || progress_pending || cleanups.empty())return;
    auto task=std::move(cleanups.front());cleanups.erase(cleanups.begin());
    submit(Cleanup,[task]{return client->call("GET",task.base,task.path,task.token);});
}
void begin_session_subtitles();
void reset_session_subtitles();
void apply_video_plan(const StreamPlan &plan) {
    if(plan.playback.mode==Mode::Unsupported){message=plan.playback.error;return;}
    prepare_subtitle_selection();
    if(plan.playback.mode!=Mode::DirectPlay && subtitle_selection.choice>=0){message="Selected subtitles require Original / Direct Play. Server stream reconfiguration is not supported; playback has not restarted.";return;}
    audio_reset();retire_stream();stream_plan=plan;
    if(!player_open(plan.playback.url,detail.offset)){message="VLC could not open media";retire_stream();return;}
    reset_session_subtitles();
    next_candidate={};next_requested=false;autoplay_deadline=0;
    playing=detail;video=true;tracks=false;subtitle_menu=false;progress={};video_info=false;stream_fallback=detail.type=="live" || plan.playback.mode!=Mode::DirectPlay;
    auto offset=std::clamp(pref_int((account_key("plex_media_audio")+"."+encode(selected_server.id)+"."+encode(detail.id)).c_str(),global_delay),-500,500);
    player_set_audio_delay(offset);overlay_until=plat_time()+5;
    begin_session_subtitles();
}
void request_video(bool force) {
    if(job!=None || progress_pending){message="Finishing previous playback request; select Play again shortly.";return;}
    bool local=false;for(const auto &c:selected_server.connections)if(c.uri==base)local=c.local;
    auto key=account_key(local?"plex_quality_local":"plex_quality_remote");
    auto quality=static_cast<Quality>(std::clamp(pref_int(key.c_str(),0),0,6));
    auto direct=direct_play(base,server_token,detail,version);
    if(!force && direct.mode==Mode::DirectPlay && (quality==Quality::Original || (quality==Quality::Automatic && local))){apply_video_plan({direct,"","Direct Play requested"});return;}
    auto b=base,t=server_token;auto item=detail;auto v=version;auto session=new_stream_id();
    submit(VideoPlan,[b,t,item,v,session,quality,local,force] {
        auto plan=negotiate_video(*client,b,t,item,v,quality,local,session,force);
        if(plan.playback.mode==Mode::Unsupported)return Result{{},plan.playback.error,0};
        return Result{Json{{"url",plan.playback.url},{"mode",static_cast<int>(plan.playback.mode)},{"session",plan.session},{"label",plan.server_mode}},"",200};
    });
}
void video_plan_ready(const Result &result) {
    if(!result){message=result.error;return;}
    StreamPlan plan;plan.playback={static_cast<Mode>(number(result.data,"mode")),text(result.data,"url"),""};plan.session=text(result.data,"session");plan.server_mode=text(result.data,"label");apply_video_plan(plan);
}
void video_tools(uint32_t hit) {
    rect(80,90,1750,910,Theme::alpha(Theme::Background,245));
    const auto &info=player_video_info();
    draw_text(110,115, Theme::Font::Heading,"Playback Information / Controls");
    draw_text(110,175, Theme::Font::Body,stream_plan.server_mode);
    draw_text(110,215, Theme::Font::Caption,"Observed VLC: "+info.codec+" / "+info.audio_codec+" • "+std::to_string(info.width)+"x"+std::to_string(info.height)+" • "+std::to_string(info.fps)+" fps • "+std::to_string(info.audio_channels)+" channels");
    draw_text(110,255, Theme::Font::Caption,info.hardware?"Decoder reports hardware active":"No hardware-active report (codec alone does not establish hardware decoding)");
    if(hit & PAD_UP)video_info_focus=std::max(0,video_info_focus-1);
    if(hit & PAD_DOWN)video_info_focus=std::min(8,video_info_focus+1);
    const char *labels[]={"Audio offset +25 ms (save for this media)","Audio offset -25 ms (save for this media)","Reset media offset","Subtitle delay +100 ms","Subtitle delay -100 ms","Subtitle size +10%","Subtitle tracks (embedded / external)","Subtitles Off","Next Episode (server order)"};
    for(int n=0;n<9;++n)row(n,video_info_focus,310+n*75,labels[n]);
    if(hit & PAD_CROSS) {
        if(video_info_focus==8){if(next_candidate.id.empty())message="No next episode is available";else{stop();autoplay_deadline=plat_time();}}
        if(video_info_focus<=2){auto offset=video_info_focus==2?0:audio_offset(player_audio_delay(),video_info_focus==0?1:-1);player_set_audio_delay(offset);pref_set((account_key("plex_media_audio")+"."+encode(selected_server.id)+"."+encode(playing.id)).c_str(),offset);}
        if(video_info_focus==3 || video_info_focus==4)player_set_subtitle_delay(std::clamp<int64_t>(player_subtitle_delay()+(video_info_focus==3?100:-100),-10000,10000));
        if(video_info_focus==5){auto style=player_sub_style();style.size=style.size>=200?50:style.size+10;player_set_sub_style(style);}
        if(video_info_focus==7)choose_subtitle_mode(0);
        if(video_info_focus==6) {
            // Use the same verified, cancellable selection path as Options / Tracks.
            // The legacy external loader could reopen playback and bypass session Off.
            video_info=false;tracks=true;subtitle_menu=true;
        }
    }
    if(hit & (PAD_CIRCLE|PAD_SQUARE)){video_info=false;tracks=false;}
}

int subtitle_focus=0,subtitle_default_mode=0,subtitle_language_choice=0;
bool subtitle_remember=false,subtitle_forced=false;
bool subtitle_pending=false,subtitle_external_pending=false;
int subtitle_native_id=-1;
double subtitle_deadline=0;
std::vector<int> subtitle_before_external;
std::map<std::string,int> subtitle_attached;
void reset_session_subtitles(){subtitle_attached.clear();subtitle_before_external.clear();}
std::string subtitle_status;
const char *subtitle_languages[]={"en","es","fr","ja","de","it","pt"};
const char *subtitle_mode_names[]={"Off","Automatic","Preferred Language"};
std::string subtitle_memory_key() {
    return account_key("plex_subtitle_media_v3")+"."+std::to_string(pref_int(account_key("plex_subtitle_memory_epoch_v3").c_str(),0))+"."+encode(selected_server.id)+"."+encode(detail.id)+"."+std::to_string(version);
}
void prepare_subtitle_selection() {
    if(subtitle_selection.media==detail.id && subtitle_selection.version==static_cast<size_t>(version))return;
    subtitle_selection.begin(detail.id,version,subtitle_default_mode);
    if(subtitle_forced && subtitle_default_mode!=0){auto entries=subtitle_streams(detail,version);int found=-1,count=0;for(size_t i=0;i<entries.size();++i)if(entries[i].forced && subtitle_language_matches(entries[i].language,subtitle_languages[subtitle_language_choice])){found=i;++count;}if(count==1)subtitle_selection.choice=found;}
    subtitle_status.clear();
    if(subtitle_remember) {
        auto saved=pref_str(subtitle_memory_key().c_str(),"");
        if(saved=="off")subtitle_selection.choice=-1;
        else {auto entries=subtitle_streams(detail,version);for(size_t i=0;i<entries.size();++i)if(!entries[i].id.empty() && saved=="stream:"+entries[i].id)subtitle_selection.choice=i;}
    }
}
void apply_subtitle_preferences(bool save) {
    // Settings change the next session's defaults. Current session overrides survive.
    player_set_languages(player_audio_language(),"off");
    if(save) {
        pref_set(account_key("plex_subtitle_remember_v3").c_str(),subtitle_remember?1:0);
        pref_set(account_key("plex_subtitle_mode_v3").c_str(),subtitle_default_mode);
        pref_set(account_key("plex_subtitle_language_v3").c_str(),subtitle_language_choice);
        pref_set(account_key("plex_subtitle_forced_v3").c_str(),subtitle_forced?1:0);
        if(!video)subtitle_selection.reset();
    }
}
void choose_subtitle_mode(int mode) {
    subtitle_selection.choice=mode==0?-1:mode==1?-2:-3;
    subtitle_pending=false;subtitle_external_pending=false;
    if(video) {
        if(mode==0)player_subtitles_off();
        else player_subtitles_automatic(mode==2?subtitle_languages[subtitle_language_choice]:"");
    }
    subtitle_status=mode==0?"Off":"Automatic selection requested";
}
void begin_session_subtitles() {
    player_subtitles_off(); // Off until an explicit/default policy is applied after discovery.
    subtitle_pending=true;subtitle_external_pending=false;subtitle_deadline=plat_time()+12;
    subtitle_status="Waiting for subtitle discovery";
}
void service_subtitle_selection() {
    if(!subtitle_pending)return;
    auto choice=subtitle_selection.choice;
    if(choice==-1){player_subtitles_off();subtitle_pending=false;subtitle_status="Off";return;}
    if(choice==-4){player_set_subtitle_track(subtitle_native_id);subtitle_pending=false;if(player_subtitle_track()==subtitle_native_id)subtitle_status="Native track active";else{player_subtitles_off();subtitle_selection.choice=-1;subtitle_status="Native track unavailable; Off retained";}return;}
    if(choice==-3) {
        SubtitleStream wanted;wanted.language=subtitle_languages[subtitle_language_choice];
        int native=match_subtitle(wanted,player_subtitle_catalog());
        if(native>=0){player_set_subtitle_track(native);if(player_subtitle_track()==native){subtitle_pending=false;subtitle_status="Preferred language active";return;}}
        if(plat_time()>subtitle_deadline){player_subtitles_off();subtitle_selection.choice=-1;subtitle_pending=false;subtitle_status="Preferred language unavailable or ambiguous; Off retained";message=subtitle_status;}
        return;
    }
    if(choice<0){player_subtitles_automatic("");subtitle_pending=false;subtitle_status="Automatic selection requested";return;}
    auto entries=subtitle_streams(playing,version);
    if(static_cast<size_t>(choice)>=entries.size()){player_subtitles_off();subtitle_pending=false;subtitle_selection.choice=-1;subtitle_status="Requested subtitle is unavailable";return;}
    const auto &entry=entries[choice];
    int native=-1;
    if(entry.external.empty())native=match_subtitle(entry,player_subtitle_catalog());
    else if(!subtitle_external_pending && subtitle_attached.count(entry.id)) {
        const int attached=subtitle_attached.at(entry.id);
        for(const auto &track:player_subtitle_tracks())if(track.id==attached)native=attached;
        if(native<0)subtitle_attached.erase(entry.id); // rediscover if VLC removed it
    }
    else if(!subtitle_external_pending && !player_subtitle_tracks().empty()) {
        std::string url;
        auto parts=playing.media[version].find("Part");
        for(const auto &stream:(*parts)[0]["Stream"])if(text(stream,"id",std::to_string(number(stream,"id")))==entry.id)url=external_subtitle(base,server_token,stream);
        if(url.empty()){subtitle_pending=false;subtitle_selection.choice=-1;subtitle_status="External subtitle format is unsupported";message=subtitle_status;return;}
        subtitle_before_external.clear();for(const auto &t:player_subtitle_tracks())subtitle_before_external.push_back(t.id);
        if(!player_add_subtitle(url,false)){subtitle_pending=false;subtitle_selection.choice=-1;subtitle_status="External subtitle could not be loaded";message=subtitle_status;return;}
        subtitle_external_pending=true;
    } else if(subtitle_external_pending) {
        int count=0;
        for(const auto &track:player_subtitle_tracks())if(track.id>=0 && std::find(subtitle_before_external.begin(),subtitle_before_external.end(),track.id)==subtitle_before_external.end()){native=track.id;++count;}
        if(count!=1)native=-1;
    }
    if(native>=0) {
        player_set_subtitle_track(native);
        if(player_subtitle_track()==native){if(!entry.external.empty())subtitle_attached[entry.id]=native;subtitle_pending=false;subtitle_status=entry.label;return;}
    }
    if(plat_time()>subtitle_deadline) {
        player_subtitles_off();subtitle_pending=false;subtitle_selection.choice=-1;
        subtitle_status="Subtitle could not be verified. Off retained; use a native track or Original quality.";
        message=subtitle_status;
    }
}
void subtitle_picker_frame(uint32_t hit) {
    const auto &item=video?playing:detail;
    auto entries=subtitle_streams(item,version);
    std::vector<Track> native;
    if(video)for(const auto &t:player_subtitle_tracks())if(t.id>=0)native.push_back(t);
    int metadata_count=static_cast<int>(entries.size())+3;
    int count=metadata_count+static_cast<int>(native.size());
    subtitle_focus=std::clamp(subtitle_focus,0,count-1);
    if(hit & PAD_UP)subtitle_focus=std::max(0,subtitle_focus-1);
    if(hit & PAD_DOWN)subtitle_focus=std::min(count-1,subtitle_focus+1);
    rect(70,70,1780,940,Theme::alpha(Theme::Background,255));
    draw_text(390,115,Theme::Font::Heading,video?"Subtitle Tracks":"Subtitles before playback");
    draw_text(390,175,Theme::Font::Caption,"Session selection takes precedence. Off disables all captions, including forced tracks.");
    int first=std::max(0,subtitle_focus-6);
    for(int i=first;i<count && i<first+7;++i) {
        int choice=i>=metadata_count?-4:i==0?-1:i==1?-2:i==2?-3:i-3;
        std::string label=i>=metadata_count?"Native: "+native[i-metadata_count].name:i==0?"Off":i==1?"Automatic":i==2?"Preferred Language":entries[i-3].label;
        bool selected=choice==-4?player_subtitle_track()==native[i-metadata_count].id:subtitle_selection.choice==choice;
        row(i,subtitle_focus,245+(i-first)*78,(selected?"[Selected] ":"")+label);
    }
    draw_text(390,825,Theme::Font::Caption,subtitle_status,Theme::Secondary,1370);
    draw_text(390,920,Theme::Font::Caption,"Cross select • Circle back • Track changes preserve playback position");
    if(hit & PAD_CROSS) {
        if(subtitle_focus>=metadata_count)subtitle_native_id=native[subtitle_focus-metadata_count].id;
        subtitle_selection.choice=subtitle_focus>=metadata_count?-4:subtitle_focus==0?-1:subtitle_focus==1?-2:subtitle_focus==2?-3:subtitle_focus-3;
        subtitle_pending=false;subtitle_external_pending=false;
        if(video)begin_session_subtitles();
        if(subtitle_remember) {
            std::string saved=subtitle_selection.choice==-1?"off":subtitle_selection.choice>=0?"stream:"+entries[subtitle_selection.choice].id:"";
            pref_set(subtitle_memory_key().c_str(),saved);
        }
    }
    if(hit & PAD_CIRCLE){detail_subtitles=false;subtitle_menu=false;}
}
bool playback_tracks_focus_valid(){return subtitle_menu?subtitle_focus>=0:track_focus>=0 && track_focus<2;}
void playback_track_frame(uint32_t hit) {
    if(subtitle_menu){subtitle_picker_frame(hit);return;}
    if(hit & PAD_UP)track_focus=0;
    if(hit & PAD_DOWN)track_focus=1;
    if(hit & (PAD_CROSS|PAD_SQUARE)) {
        if(track_focus==1){subtitle_menu=true;subtitle_focus=0;return;}
        auto audio=player_audio_tracks();int current=player_audio_track();
        for(size_t i=0;i<audio.size();++i)if(audio[i].id==current){player_set_audio_track(audio[(i+1)%audio.size()].id);break;}
    }
    draw_text(100,120,Theme::Font::Heading,track_focus==0?"Audio track (Cross cycles)":"Subtitles (Cross opens menu)");
    if(track_focus==0){for(const auto &track:player_audio_tracks())if(track.id==player_audio_track())draw_text(100,170,Theme::Font::Body,track.name);}
}
void subtitle_settings_frame(uint32_t hit) {
    if(hit & PAD_UP)focus=std::max(0,focus-1);
    if(hit & PAD_DOWN)focus=std::min(7,focus+1);
    const std::string labels[]={std::string("Default mode: ")+subtitle_mode_names[subtitle_default_mode],std::string("Preferred language: ")+subtitle_languages[subtitle_language_choice],std::string("Remember per-media selection: ")+(subtitle_remember?"On":"Off"),"Appearance: cycle subtitle size","Subtitle delay +100 ms","Subtitle delay -100 ms",std::string("Prefer forced metadata tracks: ")+(subtitle_forced?"On":"Off"),"Reset subtitle preferences"};
    int first=std::max(0,focus-6);for(int i=first;i<8 && i<first+7;++i)row(i,focus,210+(i-first)*85,labels[i]);
    if(hit & PAD_CROSS){
        if(focus==0)subtitle_default_mode=(subtitle_default_mode+1)%3;
        if(focus==1)subtitle_language_choice=(subtitle_language_choice+1)%7;
        if(focus==2)subtitle_remember=!subtitle_remember;
        if(focus==6)subtitle_forced=!subtitle_forced;
        if(focus==7){auto key=account_key("plex_subtitle_memory_epoch_v3");pref_set(key.c_str(),(std::clamp(pref_int(key.c_str(),0),0,999999)+1)%1000000);subtitle_default_mode=0;subtitle_language_choice=0;subtitle_remember=false;subtitle_forced=false;pref_set(subtitle_memory_key().c_str(),"");}
        if(focus<=2 || focus>=6)apply_subtitle_preferences(true);
        if(focus==3){auto style=player_sub_style();style.size=style.size>=200?50:style.size+10;player_set_sub_style(style);}
        if(focus==4 || focus==5)player_set_subtitle_delay(std::clamp<int64_t>(player_subtitle_delay()+(focus==4?100:-100),-10000,10000));
    }
}

void video_options_frame(uint32_t hit) {
    if(hit & PAD_UP)focus=std::max(0,focus-1);
    if(hit & PAD_DOWN)focus=std::min(7,focus+1);
    auto local=static_cast<Quality>(std::clamp(pref_int(account_key("plex_quality_local").c_str(),0),0,6));
    auto remote=static_cast<Quality>(std::clamp(pref_int(account_key("plex_quality_remote").c_str(),0),0,6));
    row(0,focus,230,std::string("Local quality: ")+quality_name(local));row(1,focus,330,std::string("Remote quality: ")+quality_name(remote));
    row(2,focus,430,"Audio/subtitle languages: default / English / Spanish / French / Japanese");row(3,focus,530,std::string("Subtitles: ")+subtitle_mode_names[subtitle_default_mode]);
    row(4,focus,630,std::string("Automatically play next episode: ")+(pref_int(account_key("plex_autoplay").c_str(),0)?"On":"Off"));
    row(5,focus,730,"Completion threshold: "+std::to_string(std::clamp(pref_int(account_key("plex_complete_pct").c_str(),95),50,100))+"%");
    row(6,focus,790,"Check Plex Cloud Watchlist access");row(7,focus,865,"Subtitles: full preferences");
    if(hit & PAD_CROSS) {
        if(focus==7){settings_page=8;focus=0;return;}
        if(focus==6)request_cloud_check();
        if(focus==4)pref_set(account_key("plex_autoplay").c_str(),pref_int(account_key("plex_autoplay").c_str(),0)?0:1);
        if(focus==5){int n=std::clamp(pref_int(account_key("plex_complete_pct").c_str(),95),50,100);pref_set(account_key("plex_complete_pct").c_str(),n>=100?50:n+5);}
        if(focus<2)pref_set(account_key(focus==0?"plex_quality_local":"plex_quality_remote").c_str(),(static_cast<int>(focus==0?local:remote)+1)%7);
        if(focus==2){const char *langs[]={"","en","es","fr","ja"};int n=(pref_int(account_key("plex_language_choice").c_str(),0)+1)%5;pref_set(account_key("plex_language_choice").c_str(),n);player_set_languages(langs[n],subtitle_default_mode==0?"off":subtitle_default_mode==2?std::string(subtitle_languages[subtitle_language_choice])+",none":"");}
        if(focus==3){subtitle_default_mode=(subtitle_default_mode+1)%3;apply_subtitle_preferences(true);}
    }
    draw_text(390,945, Theme::Font::Caption,"Automatic uses a fixed local/remote policy; no measured bandwidth or 4K decode claim.");
}

void clear_next(){next_candidate={};next_requested=true;autoplay_deadline=0;video_retry_pending=false;}
void service_next(){
    if(!video || playing.type!="episode" || next_requested || job!=None || progress_pending)return;
    next_requested=true;auto b=base,t=server_token;auto item=playing;
    submit(NextEpisode,[b,t,item]{return next_episode(*client,b,t,item);});
}
void next_ready(const Result &r){if(r){auto entries=items(r.data);if(!entries.empty())next_candidate=entries[0];}}
void offer_autoplay(double now){if(pref_int(account_key("plex_autoplay").c_str(),0) && !next_candidate.id.empty())autoplay_deadline=now+10;}
bool next_countdown_frame(uint32_t hit,double now){
    if(autoplay_deadline<=0 || video || profile_screen || settings)return false;
    rect(390,230,1420,430,Theme::Surface);draw_text(430,280, Theme::Font::Heading,"Next Episode: "+next_candidate.title);
    draw_text(430,370, Theme::Font::Body,"Starting in "+std::to_string(std::max(0,static_cast<int>(autoplay_deadline-now)))+" seconds");
    draw_text(430,470, Theme::Font::Body,"Cross: play now • Circle: cancel autoplay");
    if(hit & PAD_CIRCLE){autoplay_deadline=0;return true;}
    if((hit & PAD_CROSS) || now>=autoplay_deadline){if(job==None && !progress_pending){detail=next_candidate;autoplay_deadline=0;request_video();}}
    return true;
}
void mark_video_completion(int64_t position){
    int threshold=std::clamp(pref_int(account_key("plex_complete_pct").c_str(),95),50,100);
    if((playing.type=="movie" || playing.type=="episode") && playing.duration>0 && static_cast<double>(position)/playing.duration>=threshold/100.0 && cleanups.size()<8)
        cleanups.push_back({base,server_token,"/:/scrobble?identifier=com.plexapp.plugins.library&key="+encode(playing.id)});
}

void restore_video_preferences(){
    const char *langs[]={"","en","es","fr","ja"};int n=std::clamp(pref_int(account_key("plex_language_choice").c_str(),0),0,4);
    subtitle_remember=pref_int(account_key("plex_subtitle_remember_v3").c_str(),0)!=0;
    subtitle_default_mode=std::clamp(pref_int(account_key("plex_subtitle_mode_v3").c_str(),0),0,2);
    subtitle_language_choice=std::clamp(pref_int(account_key("plex_subtitle_language_v3").c_str(),0),0,6);
    subtitle_forced=pref_int(account_key("plex_subtitle_forced_v3").c_str(),0)!=0;
    subtitle_selection.reset();subtitle_pending=false;subtitle_external_pending=false;
    player_set_languages(langs[n],"off");
}

void service_video_retry(){if(video_retry_pending && job==None && !progress_pending){video_retry_pending=false;request_video(true);}}
