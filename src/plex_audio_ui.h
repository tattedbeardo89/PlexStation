// SPDX-License-Identifier: GPL-3.0-or-later
// Included inside the native UI namespace: uses its existing async/auth/artwork infrastructure.
struct AudioPage {std::string path,title;std::vector<Item> entries;int focus=0,start=0,total=0;Item parent{};};
struct AudioUI {
    bool screen=false, active=false, now_playing=false, queue_view=false, menu=false, start_pending=false;
    bool books=false, playback_books=false, complete_album=false, shuffle_after=false;
    Item book;
    int speed=1, sleep_choice=0, sleep_chapter=-1;
    int64_t resume_position=0;
    bool rate_pending=false,force_transcode=false;
    std::string stream_session;
    SleepTimer sleep;
    double save_at=0;
    int focus=0, menu_focus=0, start=0,total=0, queue_focus=0;
    std::string path,title,request_path,request_title;
    int request_start=0;bool request_push=false;
    std::vector<Item> entries;
    std::vector<AudioPage> history;
    Item parent;
    AudioQueue queue;
    Item current;
    std::vector<Item> completed;
    Progress report;
    double retry_at=0;
} audio;
ListeningStore listening;
bool listening_loaded=false;
void book_save(bool finished=false);
std::string audio_scope() {return account_key("plex_audio")+"."+encode(selected_server.id);}
bool book_section(const Item &i) {return music_section(i) && pref_int((audio_scope()+".books."+encode(i.id)).c_str(),0)!=0;}
bool audio_available(bool books) {
    for(const auto &s:home_sections)if(music_section(s) && book_section(s)==books)return true;
    return false;
}
void retire_audio_stream() {
    if(!audio.stream_session.empty() && cleanups.size()<8)cleanups.push_back({base,server_token,"/music/:/transcode/universal/stop?session="+encode(audio.stream_session)});
    audio.stream_session.clear();
}
void audio_reset() {
    retire_audio_stream();
    if(audio.active){book_save();player_stop();}
    audio=AudioUI{};
}
void audio_leave_browse() {audio.screen=false;audio.now_playing=false;audio.queue_view=false;audio.menu=false;}
void audio_load(const std::string &path,const std::string &title,bool push=true,int start=0) {
    if(job!=None)return;
    audio.request_path=path;audio.request_title=title;audio.request_start=start;audio.request_push=push;
    auto b=base,t=server_token;
    submit(AudioBrowse,[b,t,path,start] {auto r=client->browse(b,t,path,start,200);
        auto end=path.find("/children");
        if(r && end!=std::string::npos && path.rfind("/library/metadata/",0)==0) {
            auto meta=client->call("GET",b,path.substr(0,end),t);if(meta)r.data["audioParent"]=std::move(meta.data);
        }
        return r;
    });
}
void audio_library_tab(int tab) {
    if(job!=None || active_library.id.empty())return;
    if(tab==0){auto b=base,t=server_token;auto section=active_library;bool books=audio.books;auto local=listening.books(audio_scope());submit(AudioFeed,[b,t,section,books,local]{return music_feed(*client,b,t,section,books,local);});return;}
    audio.screen=true;audio.now_playing=false;home_screen=false;
    audio_load("/library/sections/"+encode(active_library.id)+(tab==1?(audio.books?"/all?type=9":"/all?type=8"):"/genre?type=9"),active_library.title,false);
}
void audio_home(bool books) {
    if(!listening_loaded){listening.load(std::string(plat_data_dir())+"/plexstation-listening.json");listening_loaded=true;}
    audio.screen=true;audio.books=books;audio.now_playing=false;audio.queue_view=false;audio.menu=false;
    audio.entries.clear();audio.history.clear();audio.path="sections";audio.title=books?"Audiobooks":"Music";audio.focus=0;
    for(const auto &s:home_sections)if(music_section(s) && book_section(s)==books)audio.entries.push_back(s);
    home_screen=false;sidebar=false;settings=false;in_detail=false;
    auto saved=pref_str((audio_scope()+(books?".default-books":".default-music")).c_str(),"");
    for(const auto &section:audio.entries)if(section.id==saved){active_library=section;library_context=true;library_tab=0;audio_library_tab(0);break;}
}
void audio_start() {
    if(job!=None || !audio.queue.current())return;
    auto item=*audio.queue.current();auto b=base,t=server_token;
    audio.start_pending=false;
    submit(AudioResolve,[b,t,item]{return client->call("GET",b,"/library/metadata/"+encode(item.id),t);});
}
void audio_queue_load(const Item &item,bool shuffle=false) {
    if(job!=None){message="Finishing a server request; select Play again shortly.";return;}
    if(audio.active){book_save();queue_progress("stopped",player_time());player_stop();audio.active=false;}
    retire_audio_stream();audio.force_transcode=false;
    audio.playback_books=audio.books;
    audio.book=item;
    if(item.type=="track" && !item.parent_id.empty()) {audio.book.id=item.parent_id;audio.book.type="album";audio.book.title=item.album;}
    audio.speed=std::clamp(pref_int((audio_scope()+".speed").c_str(),1),0,6);
    audio.resume_position=0;audio.sleep_choice=std::clamp(pref_int((audio_scope()+".sleep").c_str(),0),0,5);audio.sleep.clear();
    if(item.type=="track") {
        if(audio.playback_books)audio.resume_position=book_resume({item},listening.get(audio_scope(),audio.book.id)).position;
        audio.queue.assign({item});audio.start_pending=true;return;
    }
    auto path=item.type=="playlist" ? "/playlists/"+encode(item.id)+"/items" : "/library/metadata/"+encode(item.id)+(item.type=="artist"?"/allLeaves":"/children");
    auto b=base,t=server_token;
    audio.complete_album=item.type=="album";audio.shuffle_after=shuffle;
    submit(AudioQueueLoad,[b,t,path]{return audio_listing(*client,b,t,path,[]{return true;},true);});
}
void audio_finish(int kind,const Result &result) {
    if(!result){message=result.error;audio.start_pending=false;return;}
    if(kind==AudioScrobble)return;
    if(kind==AudioFeed){library_context=true;library_tab=0;feed_rows=home_rows(result.data);feed_columns.assign(feed_rows.size(),0);feed_scroll.assign(feed_rows.size(),0);feed_focus=0;home_screen=true;search_screen=false;home_title=audio.books?"Audiobooks":"Music";audio.screen=false;sidebar=false;return;}
    if(kind==AudioBrowse) {
        if(audio.request_push) {
            if(audio.history.size()>=16)audio.history.erase(audio.history.begin());
            audio.history.push_back({audio.path,audio.title,audio.entries,audio.focus,audio.start,audio.total,audio.parent});
        }
        audio.path=audio.request_path;audio.title=audio.request_title;audio.start=audio.request_start;
        audio.entries=items(result.data,false);audio.focus=0;audio.parent={};
        if(result.data.contains("audioParent")) {auto parent=items(result.data["audioParent"],false);if(!parent.empty())audio.parent=parent[0];}
        if(audio.path.find("/genre?")!=std::string::npos)for(auto &i:audio.entries){i.type="audio-route";i.key=audio.path.substr(0,audio.path.find("/genre?"))+"/all?type=9&genre="+encode(i.id); }
        auto mc=result.data.find("MediaContainer");
        audio.total=mc==result.data.end()?0:static_cast<int>(std::clamp<int64_t>(number(*mc,"totalSize",audio.entries.size()),0,INT32_MAX));
        return;
    }
    if(kind==AudioQueueLoad) {
        auto tracks=audio_tracks(result.data,audio.complete_album);
        if(tracks.empty()){message="No permitted audio tracks returned";return;}
        audio.queue.assign(std::move(tracks));
        if(audio.playback_books) {
            auto resume=book_resume(audio.queue.entries,listening.get(audio_scope(),audio.book.id));
            audio.queue.cursor=resume.index;audio.resume_position=resume.position;
        }
        if(audio.shuffle_after && !audio.playback_books)audio.queue.shuffle(static_cast<uint32_t>(plat_time()*1000000));
        audio.start_pending=true;return;
    }
    auto parsed=items(result.data);
    if(parsed.empty() || !audio.queue.current() || parsed[0].id!=audio.queue.current()->id){message="Audio identity mismatch";return;}
    auto decision=audio_direct_play(base,server_token,parsed[0]);
    if(decision.mode!=Mode::DirectPlay || audio.force_transcode) {
        auto session=new_stream_id();decision=audio_transcode(base,server_token,parsed[0],session);
        if(decision.mode==Mode::Unsupported){message=decision.error;return;}
        retire_audio_stream();audio.stream_session=session;audio.force_transcode=true;
        message="PMS AAC/HLS transcode requested; actual playback requires server acceptance";
    }
    if(video){stop();} // one VLC player; never overlap video and audio
    if(audio.active){book_save();queue_progress("stopped",player_time());player_stop();audio.active=false;}
    // Reconcile again with freshly resolved server timestamps before applying local resume.
    if(audio.playback_books && parsed[0].last_played>number(listening.get(audio_scope(),audio.book.id),"updated"))audio.resume_position=parsed[0].offset;
    if(!player_open(decision.url,audio.resume_position,{":no-video",":audio-time-stretch"})){message="VLC could not open audio";return;}
    audio.resume_position=0;audio.rate_pending=audio.playback_books;
    audio.current=parsed[0];playing=audio.current;audio.active=true;audio.report={};audio.now_playing=true;
    audio.screen=true;sidebar=false;audio.retry_at=0;audio.focus=0;
    player_set_audio_delay(global_delay);
}
void book_save(bool finished) {
    if(!audio.playback_books || audio.book.id.empty() || !audio.queue.current())return;
    auto existing=listening.get(audio_scope(),audio.book.id);
    int64_t total=0,duration=0;
    for(size_t n=0;n<audio.queue.order.size();++n) {
        auto length=audio.queue.entries[audio.queue.order[n]].duration;
        duration+=std::min<int64_t>(length,INT64_MAX/4000);
        if(n<audio.queue.cursor)total+=std::min<int64_t>(length,INT64_MAX/4000);
    }
    int64_t position=std::max<int64_t>(0,player_time());
    existing["book"]=audio.book.id;existing["kind"]=audio.book.type;existing["title"]=audio.book.title;existing["thumb"]=audio.book.thumb;
    existing["track"]=audio.current.id;existing["position"]=position;existing["total_position"]=total+std::min<int64_t>(position,INT64_MAX/4000);
    existing["total_duration"]=duration;existing["finished"]=finished;existing["updated"]=static_cast<int64_t>(std::time(nullptr));
    if(!listening.put(audio_scope(),audio.book.id,std::move(existing)))message="Local listening position could not be saved; check storage permissions.";
}
void audio_next(bool previous=false,bool natural=false) {
    if(audio.active) {
        book_save(natural && !previous && audio.queue.cursor+1>=audio.queue.order.size());
        queue_progress("stopped",player_time());player_stop();audio.active=false;
    }
    bool next=previous?audio.queue.previous():audio.queue.advance(natural);
    if(next){retire_audio_stream();audio.force_transcode=false;audio.resume_position=0;audio.start_pending=true;}
}
void audio_tick(double now) {
    if(audio.active) {
        if(audio.rate_pending && !player_buffering()){player_set_rate(book_speed(audio.speed));audio.rate_pending=false;}
        if(audio.playback_books && now>=audio.save_at){book_save();audio.save_at=now+10;}
        if(audio.sleep.due(now,player_ended() && !player_failed()) || (audio.sleep.end_track && audio.sleep_chapter>=0 && player_chapter()>audio.sleep_chapter)) {
            book_save();queue_progress("paused",player_time());
            if(player_ended()){player_stop();audio.active=false;audio.sleep.clear();return;}
            if(!player_paused())player_toggle_pause();
            audio.sleep.clear();
        }
        if(player_ended()) {
            if(player_failed()){
                book_save();audio.resume_position=player_time();audio.active=false;player_stop();audio.retry_at=now;
                if(!audio.force_transcode){audio.force_transcode=true;audio.start_pending=true;message="Original audio failed; requesting PMS AAC/HLS fallback";}
                else message="Audio fallback failed. Retry or choose another item; no track was skipped.";
            }
            else {if(audio.completed.size()<32)audio.completed.push_back(audio.current);audio_next(false,true);}
        } else if(audio.report.due(static_cast<int64_t>(now*1000),player_paused()?"paused":"playing"))
            queue_progress(player_paused()?"paused":"playing",player_time());
    }
    if(audio.start_pending && job==None && !progress_pending)audio_start();
    if(job==None && !progress_pending && !audio.completed.empty()) {
        auto item=audio.completed.front();audio.completed.erase(audio.completed.begin());auto b=base,t=server_token;
        submit(AudioScrobble,[b,t,item]{return client->watched(b,t,item,true);});
    }
}
std::string audio_label(const Item &i) {
    return (i.type=="track" ? (i.disc>0?std::to_string(i.disc)+".":"")+std::to_string(i.track)+"  " : "")+i.title+
        (!i.artist.empty()?" • "+i.artist:"")+(i.duration>0?"  "+std::to_string(i.duration/60000)+":"+(i.duration/1000%60<10?"0":"")+std::to_string(i.duration/1000%60):"");
}
bool audio_frame(uint32_t hit,float) {
    if(audio.screen && !audio.now_playing && !audio.queue_view && !audio.menu && library_context && !sidebar && !settings) {
        const char *tabs[]={"Recommended","Library","Categories"};
        for(int i=0;i<3;++i){rect(390+i*320,145,300,48,library_tab==i?Theme::Selection:Theme::Surface);draw_text(405+i*320,155,Theme::Font::Caption,tabs[i]);}
        if(library_tabs_focus){if(hit & PAD_LEFT)library_tab=std::max(0,library_tab-1);if(hit & PAD_RIGHT)library_tab=std::min(2,library_tab+1);if(hit & PAD_CROSS){audio_library_tab(library_tab);library_tabs_focus=false;}if(hit & PAD_DOWN)library_tabs_focus=false;return true;}
        if((hit & PAD_UP) && audio.focus==0){library_tabs_focus=true;return true;}
    }

    if(audio.active && !audio.now_playing && !video && !settings && !profile_screen)
        draw_text(390,925, Theme::Font::Caption,(audio.playback_books?"Listening: ":"Music: ")+audio.current.title+" • Touchpad: Now Playing");
    if((hit & PAD_TOUCHPAD) && audio.queue.current() && !video && !profile_screen){audio.screen=true;audio.now_playing=true;sidebar=false;}
    if(!audio.screen || settings || profile_screen || !available.empty() || reconnect_screen || editor || video)return false;
    if(sidebar)return false;
    if(hit & PAD_OPTIONS){settings=true;settings_page=0;focus=0;return true;}
    if((hit & PAD_LEFT) && !audio.now_playing){sidebar=true;return true;}
    draw_text(390,165, Theme::Font::Heading,audio.now_playing?"Now Playing":audio.queue_view?"Queue":audio.title);
    if(hit & PAD_CIRCLE) {
        if(audio.menu){audio.menu=false;return true;}
        if(audio.now_playing || audio.queue_view){audio.now_playing=false;audio.queue_view=false;return true;}
        if(!audio.history.empty()) {auto p=std::move(audio.history.back());audio.history.pop_back();audio.path=p.path;audio.title=p.title;audio.entries=std::move(p.entries);audio.focus=p.focus;audio.start=p.start;audio.total=p.total;audio.parent=std::move(p.parent);}
        else sidebar=true;
        return true;
    }
    if(audio.now_playing) {
        gradient(390,220,1440,550,Theme::alpha(Theme::Accent,30),Theme::alpha(Theme::Background,0));
        if(!diagnostic(TextOnly))poster(400,235,480,480,audio.current,false);
        draw_text(940,245, Theme::Font::Heading,audio.current.title);draw_text(940,310, Theme::Font::Body,audio.current.artist);draw_text(940,360, Theme::Font::Body,audio.current.album);
        draw_text(940,390, Theme::Font::Caption,"VLC rate: "+std::to_string(player_rate())+"x • output 48 kHz S16 (not bit-perfect)");
        draw_text(940,425, Theme::Font::Body,std::to_string(player_time()/1000)+" / "+std::to_string(audio.current.duration/1000)+" s");
        float fraction=audio.current.duration>0?std::clamp(static_cast<float>(player_time())/audio.current.duration,0.f,1.f):0;
        rect(940,480,800,10,Theme::Border);rect(940,480,800*fraction,10,Theme::Accent);
        if(audio.playback_books) {
            if(hit & PAD_LEFT)player_seek(std::max<int64_t>(0,player_time()-10000));
            if(hit & PAD_RIGHT)player_seek(player_time()+10000);
            if(hit & PAD_L2)player_seek(std::max<int64_t>(0,player_time()-30000));
            if(hit & PAD_R2)player_seek(player_time()+30000);
            if(hit & PAD_TRIANGLE){
                auto saved=listening.get(audio_scope(),audio.book.id);auto bookmarks=saved.find("bookmarks");
                if(bookmarks==saved.end() || !bookmarks->is_array())saved["bookmarks"]=Json::array();
                if(saved["bookmarks"].size()<16)saved["bookmarks"].push_back({{"track",audio.current.id},{"position",player_time()}});
                if(!listening.put(audio_scope(),audio.book.id,std::move(saved)))message="Bookmark storage failed";
                else message="PlexStation-local bookmark saved (not Plex-synchronized)";
            }
        }
        const char *labels[]={"Play / Pause","Previous Track","Next Track","Shuffle","Repeat","Queue","Stop","Retry Current Track","Speed","Sleep Timer","Next Chapter","Previous Chapter","Restart Chapter","Restart Book","Mark Book Finished","Restore Last Local Bookmark"};
        if(hit & PAD_UP)audio.focus=std::max(0,audio.focus-1);
        if(hit & PAD_DOWN)audio.focus=std::min(audio.playback_books?15:7,audio.focus+1);
        int first=std::max(0,audio.focus-3);
        for(int j=first;j<(audio.playback_books?16:8) && j<first+4;++j)row(j,audio.focus,550+(j-first)*80,std::string(labels[j])+(j==4?audio.queue.repeat==Repeat::Off?": Off":audio.queue.repeat==Repeat::All?": All":": One":j==8?": "+std::to_string(book_speed(audio.speed))+"x":j==9?": "+std::string(audio.sleep_choice==0?"Off":audio.sleep_choice==5?"End of Chapter / Track":(std::to_string(audio.sleep_choice*15)+" min")):""));
        if(hit & PAD_R1)audio_next();
        if(hit & PAD_L1){if(player_time()>3000)player_seek(0);else audio_next(true);}
        if(hit & PAD_CROSS) {
            switch(audio.focus) {
            case 0: if(audio.active)player_toggle_pause();else audio.start_pending=true;break;
            case 1: if(player_time()>3000)player_seek(0);else audio_next(true);break;
            case 2:audio_next();break;
            case 3:if(audio.playback_books){message="Audiobook shuffle is disabled to preserve chapter order";break;}if(audio.queue.shuffled)audio.queue.unshuffle();else audio.queue.shuffle(static_cast<uint32_t>(plat_time()*1000000));break;
            case 4:audio.queue.repeat=static_cast<Repeat>((static_cast<int>(audio.queue.repeat)+1)%3);break;
            case 5:audio.now_playing=false;audio.queue_view=true;audio.queue_focus=audio.queue.cursor;break;
            case 6:retire_audio_stream();book_save();queue_progress("stopped",player_time());player_stop();audio.active=false;break;
            case 7:audio.start_pending=true;break;
            case 8:audio.speed=(audio.speed+1)%7;player_set_rate(book_speed(audio.speed));pref_set((audio_scope()+".speed").c_str(),audio.speed);break;
            case 9:audio.sleep_choice=(audio.sleep_choice+1)%6;audio.sleep.set(audio.sleep_choice==5?-1:audio.sleep_choice*15,plat_time());audio.sleep_chapter=audio.sleep_choice==5 && !player_chapters().empty()?player_chapter():-1;pref_set((audio_scope()+".sleep").c_str(),audio.sleep_choice);break;
            case 10: {auto chapters=player_chapters();int c=player_chapter();if(c>=0 && c+1<static_cast<int>(chapters.size()))player_set_chapter(c+1);else audio_next();break;}
            case 11: {int c=player_chapter();if(c>0)player_set_chapter(c-1);else audio_next(true);break;}
            case 12: {int c=player_chapter();if(c>=0)player_set_chapter(c);else player_seek(0);break;}
            case 13:book_save();audio.queue.cursor=0;audio.resume_position=0;audio.start_pending=true;break;
            case 14:retire_audio_stream();book_save(true);queue_progress("stopped",player_time());player_stop();audio.active=false;break;
            case 15: {auto saved=listening.get(audio_scope(),audio.book.id);auto m=saved.find("bookmarks");if(m!=saved.end() && m->is_array() && !m->empty()){auto mark=m->back();for(size_t n=0;n<audio.queue.order.size();++n)if(audio.queue.entries[audio.queue.order[n]].id==text(mark,"track")){audio.queue.cursor=n;audio.resume_position=number(mark,"position");audio.start_pending=true;break;}}break;}

            }
        }
        draw_text(390,910, Theme::Font::Caption,audio.playback_books?"Left/Right ±10s • L2/R2 ±30s • Triangle Local Bookmark • Circle Browse":"L1 Previous • R1 Next • Circle Browse • Options Settings");return true;
    }
    if(audio.queue_view) {
        if(hit & PAD_UP)audio.queue_focus=std::max(0,audio.queue_focus-1);
        if(hit & PAD_DOWN)audio.queue_focus=std::min(std::max(0,static_cast<int>(audio.queue.order.size())-1),audio.queue_focus+1);
        int first=std::max(0,audio.queue_focus-6);
        for(int j=first;j<static_cast<int>(audio.queue.order.size()) && j<first+7;++j)row(j,audio.queue_focus,235+(j-first)*Theme::Metric::CompactStep,audio_label(audio.queue.entries[audio.queue.order[j]]));
        if((hit & PAD_CROSS) && !audio.queue.order.empty()){audio.queue.cursor=audio.queue_focus;audio.start_pending=true;}
        if(hit & PAD_SQUARE)audio.queue.remove(audio.queue_focus);
        if(hit & PAD_TRIANGLE){if(audio.active){book_save();queue_progress("stopped",player_time());player_stop();audio.active=false;}audio.queue.clear();}
        draw_text(390,890, Theme::Font::Caption,"Cross Play • Square Remove (skip current first) • Triangle Clear Queue");return true;
    }
    if(audio.menu) {
        const char *labels[]={"Play selected album / playlist / artist / track","Shuffle selected","Add selected track to Queue","Play selected track Next","Now Playing","Queue"};
        if(hit & PAD_UP)audio.menu_focus=std::max(0,audio.menu_focus-1);
        if(hit & PAD_DOWN)audio.menu_focus=std::min(5,audio.menu_focus+1);
        for(int j=0;j<6;++j)row(j,audio.menu_focus,230+j*100,labels[j]);
        if((hit & PAD_CROSS) && !audio.entries.empty()) {
            auto item=audio.entries[std::clamp(audio.focus,0,static_cast<int>(audio.entries.size())-1)];
            if(audio.menu_focus<2)audio_queue_load(audio.parent.type=="album" || audio.parent.type=="artist"?audio.parent:item,audio.menu_focus==1);
            if(audio.menu_focus==2 || audio.menu_focus==3) {if(!audio.queue.add(item,audio.menu_focus==3))message="Select a track to enqueue (maximum 2000)";}
            if(audio.menu_focus==4)audio.now_playing=true;
            if(audio.menu_focus==5)audio.queue_view=true;
            audio.menu=false;
        }
        return true;
    }
    if(hit & PAD_SQUARE){audio.menu=true;audio.menu_focus=0;return true;}
    if(hit & PAD_UP)audio.focus=std::max(0,audio.focus-1);
    if(hit & PAD_DOWN)audio.focus=std::min(std::max(0,static_cast<int>(audio.entries.size())-1),audio.focus+1);
    int first=std::max(0,audio.focus-6);
    for(int j=first;j<static_cast<int>(audio.entries.size()) && j<first+7;++j) {
        rect(390,235+(j-first)*Theme::Metric::CompactStep,1050,70,j==audio.focus?Theme::Selection:Theme::Surface);
        auto label=audio_label(audio.entries[j]);draw_text(420,250+(j-first)*Theme::Metric::CompactStep,Theme::Font::Body,label,Theme::Text,990);
    }
    if(!audio.entries.empty() && !diagnostic(TextOnly))poster(1490,235,320,320,audio.entries[std::clamp(audio.focus,0,static_cast<int>(audio.entries.size())-1)],false);
    if(!audio.parent.summary.empty()) {float scale=gfx.height/1080.f;ImGui::GetBackgroundDrawList()->AddText(ImGui::GetFont(),20*scale,{1490*scale,590*scale},Theme::Text,audio.parent.summary.substr(0,250).c_str(),nullptr,320*scale);}
    int64_t total_duration=0;for(const auto &i:audio.entries)total_duration+=std::min<int64_t>(i.duration,INT64_MAX/4000);
    if(audio.parent.type=="album")draw_text(1490,560, Theme::Font::Caption,"Album: "+std::to_string(total_duration/60000)+" min (loaded tracks)");
    if(audio.entries.empty())draw_text(420,300, Theme::Font::Body,job==None?"No accessible content":"Loading audio library...");
    if((hit & PAD_R2) && audio.start+200<audio.total)audio_load(audio.path,audio.title,false,audio.start+200);
    if((hit & PAD_L2) && audio.start>=200)audio_load(audio.path,audio.title,false,audio.start-200);
    if((hit & PAD_CROSS) && job==None && !audio.entries.empty()) {
        auto item=audio.entries[audio.focus];
        if(audio.path=="sections") {
            active_library=item;library_context=true;library_tab=0;
            pref_set((audio_scope()+(audio.books?".default-books":".default-music")).c_str(),item.id);
            // Directory choices are local navigation to real server endpoints, not invented content.
            audio.history.push_back({audio.path,audio.title,audio.entries,audio.focus,audio.start,audio.total,audio.parent});
            audio.path="music/"+item.id;audio.title=item.title;audio.entries.clear();audio.focus=0;
            Item shelf;shelf.type="audio-shelf";shelf.id=item.id;shelf.title=audio.books?"Audiobook Home":"Music Home";audio.entries.push_back(shelf);
            const char *names[]={"Artists","Albums","Tracks","Recently Added Albums","Recently Played Tracks","Playlists","Collections","Genres","Favorite Tracks","Search Music"};
            const char *suffix[]={"/all?type=8&sort=titleSort","/all?type=9&sort=titleSort","/all?type=10&sort=titleSort","/recentlyAdded?type=9","/all?type=10&sort=lastViewedAt:desc&lastViewedAt>>=0","/playlists","/collections","/genre?type=9","/all?type=10&userRating>=8","search"};
            if(audio.books) {
                auto continuing=listening.books(audio_scope());if(!continuing.empty()){Item choice;choice.type="local-listening";choice.title="Continue Listening (PlexStation-local)";choice.id="progress";audio.entries.push_back(choice);}
                auto completed=listening.books(audio_scope(),1);if(!completed.empty()){Item choice;choice.type="local-listening";choice.title="Completed (PlexStation-local)";choice.id="completed";audio.entries.push_back(choice);}
            }
            for(int j=0;j<10;++j) {Item choice;choice.title=audio.books && j==0?"Authors":audio.books && j==1?"Books":names[j];choice.type="audio-route";choice.key=j==5?"/playlists?playlistType=audio":"/library/sections/"+encode(item.id)+suffix[j];choice.id=item.id;audio.entries.push_back(choice);}
        } else if(item.type=="local-listening") {audio.entries=listening.books(audio_scope(),item.id=="completed"?1:0);audio.path="local-listening";audio.title=item.title;audio.focus=0;}
        else if(item.type=="audio-shelf"){
            auto b=base,t=server_token;bool books=audio.books;auto local=listening.books(audio_scope());
            submit(AudioFeed,[b,t,item,books,local]{return music_feed(*client,b,t,item,books,local);});
        }
        else if(item.type=="audio-route") {if(item.key.find("search")!=std::string::npos){editor=3;edit_text.clear();edit_focus=0;}else audio_load(item.key,item.title); }
        else if(item.type=="artist" || item.type=="album")audio_load("/library/metadata/"+encode(item.id)+"/children",item.title);
        else if(item.type=="playlist")audio_load("/playlists/"+encode(item.id)+"/items",item.title);
        else if(item.type=="collection")audio_load("/library/collections/"+encode(item.id)+"/children",item.title);
        else if(item.type=="track")audio_queue_load(item);
        else message="This server category is not yet supported";
    }
    draw_text(390,890, Theme::Font::Caption,"Cross Browse / Play Track • Square Playback Menu • L2/R2 Pages • Left Sidebar");return true;
}

void audio_settings_frame(uint32_t hit) {
    std::vector<Item> sections;for(const auto &s:home_sections)if(music_section(s))sections.push_back(s);
    if(hit & PAD_UP)focus=std::max(0,focus-1);
    if(hit & PAD_DOWN)focus=std::min(std::max(0,static_cast<int>(sections.size())-1),focus+1);
    if(sections.empty()){draw_text(400,250, Theme::Font::Body,"No accessible music libraries to designate");return;}
    int first=std::max(0,focus-6);
    for(int n=first;n<static_cast<int>(sections.size()) && n<first+7;++n)row(n,focus,225+(n-first)*Theme::Metric::RowStep,sections[n].title+(book_section(sections[n])?": Audiobooks":": Music"));
    if(hit & PAD_CROSS){const auto &s=sections[std::clamp(focus,0,static_cast<int>(sections.size())-1)];pref_set((audio_scope()+".books."+encode(s.id)).c_str(),book_section(s)?0:1);}
    draw_text(390,895, Theme::Font::Caption,"Cross: designate library for this profile/server only; server metadata is unchanged");
}

int audio_focus_state() {
    if(!audio.screen)return -1;
    if(audio.now_playing)return audio.focus>=0 && audio.focus<(audio.playback_books?16:8);
    if(audio.menu)return audio.menu_focus>=0 && audio.menu_focus<6;
    if(audio.queue_view)return audio.queue.order.empty() || (audio.queue_focus>=0 && audio.queue_focus<static_cast<int>(audio.queue.order.size()));
    return audio.entries.empty() || (audio.focus>=0 && audio.focus<static_cast<int>(audio.entries.size()));
}

void open_audio_item(const Item &item){
    if(job!=None)return;
    bool books=false;for(const auto &section:home_sections)if(section.id==item.section_id)books=book_section(section);
    audio_home(books);
    if(item.type=="track")audio_queue_load(item);
    else if(item.type=="playlist")audio_load("/playlists/"+encode(item.id)+"/items",item.title);
    else audio_load("/library/metadata/"+encode(item.id)+"/children",item.title);
}
