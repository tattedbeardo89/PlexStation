// SPDX-License-Identifier: GPL-3.0-or-later
struct CleanupTask {std::string base,token,path;};
std::vector<CleanupTask> cleanups;
StreamPlan stream_plan;
bool stream_fallback=false,video_info=false;
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
void apply_video_plan(const StreamPlan &plan) {
    if(plan.playback.mode==Mode::Unsupported){message=plan.playback.error;return;}
    audio_reset();retire_stream();stream_plan=plan;
    if(!player_open(plan.playback.url,detail.offset)){message="VLC could not open media";retire_stream();return;}
    next_candidate={};next_requested=false;autoplay_deadline=0;
    playing=detail;video=true;tracks=false;progress={};video_info=false;stream_fallback=plan.playback.mode!=Mode::DirectPlay;
    auto offset=std::clamp(pref_int((account_key("plex_media_audio")+"."+encode(selected_server.id)+"."+encode(detail.id)).c_str(),global_delay),-500,500);
    player_set_audio_delay(offset);overlay_until=plat_time()+5;
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
    rect(80,90,1750,910,IM_COL32(16,18,23,245));
    const auto &info=player_video_info();
    draw_text(110,115,36,"Playback Information / Controls");
    draw_text(110,175,26,stream_plan.server_mode);
    draw_text(110,215,24,"Observed VLC: "+info.codec+" / "+info.audio_codec+" • "+std::to_string(info.width)+"x"+std::to_string(info.height)+" • "+std::to_string(info.fps)+" fps • "+std::to_string(info.audio_channels)+" channels");
    draw_text(110,255,24,info.hardware?"Decoder reports hardware active":"No hardware-active report (codec alone does not establish hardware decoding)");
    if(hit & PAD_UP)video_info_focus=std::max(0,video_info_focus-1);
    if(hit & PAD_DOWN)video_info_focus=std::min(8,video_info_focus+1);
    const char *labels[]={"Audio offset +25 ms (save for this media)","Audio offset -25 ms (save for this media)","Reset media offset","Subtitle delay +100 ms","Subtitle delay -100 ms","Subtitle size +10%","Load next external subtitle","Subtitles Off","Next Episode (server order)"};
    for(int n=0;n<9;++n)row(n,video_info_focus,310+n*75,labels[n]);
    if(hit & PAD_CROSS) {
        if(video_info_focus==8){if(next_candidate.id.empty())message="No next episode is available";else{stop();autoplay_deadline=plat_time();}}
        if(video_info_focus<=2){auto offset=video_info_focus==2?0:audio_offset(player_audio_delay(),video_info_focus==0?1:-1);player_set_audio_delay(offset);pref_set((account_key("plex_media_audio")+"."+encode(selected_server.id)+"."+encode(playing.id)).c_str(),offset);}
        if(video_info_focus==3 || video_info_focus==4)player_set_subtitle_delay(std::clamp<int64_t>(player_subtitle_delay()+(video_info_focus==3?100:-100),-10000,10000));
        if(video_info_focus==5){auto style=player_sub_style();style.size=style.size>=200?50:style.size+10;player_set_sub_style(style);}
        if(video_info_focus==7)player_set_subtitle_track(-1);
        if(video_info_focus==6) {
            std::vector<std::string> subtitles;
            if(playing.media.is_array() && static_cast<size_t>(version)<playing.media.size()){
                auto parts=playing.media[version].find("Part");if(parts!=playing.media[version].end() && parts->is_array())for(const auto &part:*parts) {
                    auto streams=part.find("Stream");if(streams!=part.end() && streams->is_array())for(const auto &stream:*streams){auto url=external_subtitle(base,server_token,stream);if(!url.empty())subtitles.push_back(url);}
                }
            }
            static size_t selected=0;if(subtitles.empty())message="No supported external subtitle provided";
            else {if(!player_add_subtitle(subtitles[selected++%subtitles.size()]))message="VLC could not attach external subtitle";}
        }
    }
    if(hit & (PAD_CIRCLE|PAD_SQUARE)){video_info=false;tracks=false;}
}
void video_options_frame(uint32_t hit) {
    if(hit & PAD_UP)focus=std::max(0,focus-1);
    if(hit & PAD_DOWN)focus=std::min(5,focus+1);
    auto local=static_cast<Quality>(std::clamp(pref_int(account_key("plex_quality_local").c_str(),0),0,6));
    auto remote=static_cast<Quality>(std::clamp(pref_int(account_key("plex_quality_remote").c_str(),0),0,6));
    row(0,focus,230,std::string("Local quality: ")+quality_name(local));row(1,focus,330,std::string("Remote quality: ")+quality_name(remote));
    row(2,focus,430,"Audio/subtitle languages: default / English / Spanish / French / Japanese");row(3,focus,530,"Subtitle policy: selected file default / Off (forced remains player-dependent)");
    row(4,focus,630,std::string("Automatically play next episode: ")+(pref_int(account_key("plex_autoplay").c_str(),0)?"On":"Off"));
    row(5,focus,730,"Completion threshold: "+std::to_string(std::clamp(pref_int(account_key("plex_complete_pct").c_str(),95),50,100))+"%");
    if(hit & PAD_CROSS) {
        if(focus==4)pref_set(account_key("plex_autoplay").c_str(),pref_int(account_key("plex_autoplay").c_str(),0)?0:1);
        if(focus==5){int n=std::clamp(pref_int(account_key("plex_complete_pct").c_str(),95),50,100);pref_set(account_key("plex_complete_pct").c_str(),n>=100?50:n+5);}
        if(focus<2)pref_set(account_key(focus==0?"plex_quality_local":"plex_quality_remote").c_str(),(static_cast<int>(focus==0?local:remote)+1)%7);
        if(focus==2){const char *langs[]={"","en","es","fr","ja"};int n=(pref_int(account_key("plex_language_choice").c_str(),0)+1)%5;pref_set(account_key("plex_language_choice").c_str(),n);player_set_languages(langs[n],langs[n]);}
        if(focus==3){bool off=pref_int(account_key("plex_subs_off").c_str(),0)==0;pref_set(account_key("plex_subs_off").c_str(),off?1:0);player_set_languages(player_audio_language(),off?"off":"");}
    }
    draw_text(390,850,23,"Automatic uses a fixed local/remote policy; no measured bandwidth or 4K decode claim.");
}

void clear_next(){next_candidate={};next_requested=true;autoplay_deadline=0;}
void service_next(){
    if(!video || playing.type!="episode" || next_requested || job!=None || progress_pending)return;
    next_requested=true;auto b=base,t=server_token;auto item=playing;
    submit(NextEpisode,[b,t,item]{return next_episode(*client,b,t,item);});
}
void next_ready(const Result &r){if(r){auto entries=items(r.data);if(!entries.empty())next_candidate=entries[0];}}
void offer_autoplay(double now){if(pref_int(account_key("plex_autoplay").c_str(),0) && !next_candidate.id.empty())autoplay_deadline=now+10;}
bool next_countdown_frame(uint32_t hit,double now){
    if(autoplay_deadline<=0 || video || profile_screen || settings)return false;
    rect(390,230,1420,430,IM_COL32(25,27,33,255));draw_text(430,280,34,"Next Episode: "+next_candidate.title);
    draw_text(430,370,28,"Starting in "+std::to_string(std::max(0,static_cast<int>(autoplay_deadline-now)))+" seconds");
    draw_text(430,470,26,"Cross: play now • Circle: cancel autoplay");
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
    player_set_languages(langs[n],pref_int(account_key("plex_subs_off").c_str(),0)?"off":langs[n]);
}
