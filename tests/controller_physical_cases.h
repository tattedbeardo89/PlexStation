// Production native ImGui commands, software-rasterized after ImGui::Render().
// Network/player/texture boundaries are fixtures. This is not PS5 GPU acceptance.
static std::vector<uint32_t> raster_frame(){
    auto *data=ImGui::GetDrawData();assert(data && data->Valid && data->TotalVtxCount>0);
    int width=gfx.width,height=gfx.height;std::vector<uint32_t> pixels(width*height,0);
    unsigned char *atlas;int aw,ah;ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&atlas,&aw,&ah);
    ImGui::GetIO().Fonts->SetTexID(1); // software backend's uploaded font atlas handle
    auto edge=[](ImVec2 a,ImVec2 b,ImVec2 p){return (p.x-a.x)*(b.y-a.y)-(p.y-a.y)*(b.x-a.x);};
    for(const auto *list:data->CmdLists)for(const auto &cmd:list->CmdBuffer){
        if(cmd.UserCallback)continue;
        for(unsigned n=0;n+2<cmd.ElemCount;n+=3){
            auto a=list->VtxBuffer[list->IdxBuffer[cmd.IdxOffset+n]+cmd.VtxOffset];
            auto b=list->VtxBuffer[list->IdxBuffer[cmd.IdxOffset+n+1]+cmd.VtxOffset];
            auto c=list->VtxBuffer[list->IdxBuffer[cmd.IdxOffset+n+2]+cmd.VtxOffset];
            float area=edge(a.pos,b.pos,c.pos);if(std::abs(area)<.001f)continue;
            int x0=std::max(0,static_cast<int>(std::max(cmd.ClipRect.x,std::min({a.pos.x,b.pos.x,c.pos.x}))));
            int y0=std::max(0,static_cast<int>(std::max(cmd.ClipRect.y,std::min({a.pos.y,b.pos.y,c.pos.y}))));
            int x1=std::min(width,static_cast<int>(std::min(cmd.ClipRect.z,std::ceil(std::max({a.pos.x,b.pos.x,c.pos.x})))));
            int y1=std::min(height,static_cast<int>(std::min(cmd.ClipRect.w,std::ceil(std::max({a.pos.y,b.pos.y,c.pos.y})))));
            for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
                ImVec2 p(x+.5f,y+.5f);float u=edge(b.pos,c.pos,p)/area,v=edge(c.pos,a.pos,p)/area,w=1-u-v;
                if(u<-.0001f || v<-.0001f || w<-.0001f)continue;
                float alpha=static_cast<float>((a.col>>24)&255)/255;
                uint32_t color=a.col;
                if(cmd.GetTexID()==9001){color=IM_COL32(35,190,205,255);}
                else {
                    float tx=u*a.uv.x+v*b.uv.x+w*c.uv.x,ty=u*a.uv.y+v*b.uv.y+w*c.uv.y;
                    int ix=std::clamp(static_cast<int>(tx*aw),0,aw-1),iy=std::clamp(static_cast<int>(ty*ah),0,ah-1);
                    alpha*=atlas[(iy*aw+ix)*4+3]/255.f;
                }
                if(alpha<=0)continue;
                auto &destination=pixels[y*width+x];uint32_t blended=0;
                for(int shift:{0,8,16}){int value=static_cast<int>(((color>>shift)&255)*alpha+((destination>>shift)&255)*(1-alpha));blended|=static_cast<uint32_t>(value)<<shift;}
                destination=blended;
            }
        }
    }
    return pixels;
}
static void initial_frame_cases(){
    while(job!=None){pending.wait();finish_job();}
    personal_dvr_probe_pending=false;warm_discovery_pending=false;home_refresh_at=clock_now+100000;
    account="fixture";account_id="physical-frame-user";selected_server.id="physical-frame-server";server_token="profile";base="https://fixture.invalid";
    available.clear();reconnect_screen=false;profile_screen=false;account_loading=false;settings=false;in_detail=false;editor=0;video=false;
    library_tools=false;filter_picker=false;full_description=false;progress_pending=false;search_screen=false;audio_reset();reset_cloud();
    auto &fonts=ImGui::GetIO().Fonts;fonts->Clear();
    std::string font_path=__FILE__;font_path=font_path.substr(0,font_path.find_last_of('/'))+"/../assets/fonts/Inter-Regular.ttf";
    ImGui::GetIO().FontDefault=fonts->AddFontFromFileTTF(font_path.c_str(),Theme::Font::Body,nullptr,Theme::glyph_ranges());
    unsigned char *font_pixels;int fw,fh;fonts->GetTexDataAsRGBA32(&font_pixels,&fw,&fh);fonts->SetTexID(1);
    assert(ImGui::GetIO().FontDefault->IsGlyphInFont(0x0416) && ImGui::GetIO().FontDefault->IsGlyphInFont(0x03a9));
    Transport response=[](const Request &request){
        if(request.url.find("/hubs/")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Hub":[]}})",""};
        if(request.url.find("originallyAvailableAt")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"released","type":"movie","title":"Visible release","originallyAvailableAt":"2020-01-01"}]}})",""};
        if(request.url.find("type=2")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"series","type":"show","title":"Visible series"}]}})",""};
        return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"visible","type":"movie","title":"Visible without controller input","duration":10000,"viewOffset":1000}]}})",""};
    };
    for(int height:{1080,2160})for(const char *kind:{"Movies","TV Shows","Home Movies"}){
        gfx.height=height;gfx.width=height*16/9;auto &io=ImGui::GetIO();io.DisplaySize={static_cast<float>(gfx.width),static_cast<float>(height)};
        Item section;section.id=kind;section.title=kind;section.type=section.title=="TV Shows"?"show":"movie";section.personal=section.title=="Home Movies";
        authorized_sections({section});active_library=section;library_context=true;library_tab=0;sidebar=false;
        std::promise<void> release;auto gate=release.get_future().share();
        client=std::make_unique<Client>([gate,response](const Request &request){gate.wait();return response(request);},"initial-render");
        activate_library_tab();assert(job==LibraryFeed);
        rendered_test_art=false;frame();assert(!rendered_cards.size());
        auto loading=raster_frame();assert(loading[500*(height/1080)*gfx.width+500*(height/1080)]!=0);
        release.set_value();pending.wait();finish_job();assert(!feed_rows.empty());
        // Tabs retaining focus must not turn off the body renderer.
        library_tabs_focus=true;frame();assert(!rendered_cards.empty());
        rendered_test_art=true;frame();auto pixels=raster_frame();
        size_t visible=std::count(pixels.begin(),pixels.end(),uint32_t(IM_COL32(35,190,205,0)&0x00ffffff));
        assert(visible>40000u*static_cast<size_t>(height/1080)*(height/1080));
        assert(!rendered_cards.empty() && rendered_cards[0].y<938*(height/1080));
        // Retain a compact view of the completed full-resolution fixture frame.
        std::string name="build/initial-"+std::string(kind)+"-"+std::to_string(height)+".bmp";
        std::ofstream image(name,std::ios::binary);auto le=[&](uint32_t n,int count){for(int i=0;i<count;++i)image.put(static_cast<char>(n>>(8*i)));};
        image.write("BM",2);le(54+960*540*4,4);le(0,4);le(54,4);le(40,4);le(960,4);le(static_cast<uint32_t>(-540),4);le(1,2);le(32,2);le(0,4);le(960*540*4,4);for(int i=0;i<4;++i)le(0,4);
        int divisor=height/540;
        for(int y=0;y<540;++y)for(int x=0;x<960;++x){int red=0,green=0,blue=0;for(int dy=0;dy<divisor;++dy)for(int dx=0;dx<divisor;++dx){auto color=pixels[(y*divisor+dy)*gfx.width+x*divisor+dx];red+=color&255;green+=(color>>8)&255;blue+=(color>>16)&255;}image.put(blue/(divisor*divisor));image.put(green/(divisor*divisor));image.put(red/(divisor*divisor));image.put(255);}
        printf("Completed initial native frame: %s %dx%d visible fixture artwork pixels=%zu, no controller input\n",kind,gfx.width,height,visible);
    }
    rendered_test_art=false;gfx.width=1920;gfx.height=1080;ImGui::GetIO().DisplaySize={1920,1080};library_tabs_focus=false;
}
static void permission_and_selection_cases(){
    while(job!=None){pending.wait();finish_job();}
    if(dvr_availability.valid()){dvr_availability.wait();dvr_availability=std::future<Result>{};}
    personal_dvr_probe_pending=false;warm_discovery_pending=false;home_refresh_at=clock_now+100000;
    account="fixture";account_id="classification-owner";selected_server.id="classification-server";base="https://fixture.invalid";
    profile_screen=false;account_loading=false;available.clear();editor=0;video=false;settings=false;in_detail=false;reconnect_screen=false;audio_reset();
    Item film;film.id="films-a";film.title="Family movies";film.type="movie";
    Item second=film;second.id="films-b";second.title="Other movies";
    Item show=film;show.id="tv";show.type="show";
    Item music=film;music.id="music-library";music.type="artist";music.title="Songs";
    Item books=music;books.id="books-library";books.title="Spoken books";
    authorized_sections({books,second,music,show,film});
    auto classify=[&](const Item &section,bool value){pref_set((audio_scope()+".books."+encode(section.id)).c_str(),value?1:0);};
    classify(books,true);classify(music,false);
    auto order=sidebar_order();assert(order[0]==3 && order[1]==0);
    std::vector<int> groups;for(int id:order)if(id>=12)groups.push_back(library_group(home_sections[id-12]));
    assert(std::is_sorted(groups.begin(),groups.end()) && groups==std::vector<int>({0,0,1,2,3}));
    assert(nav_label(12).find("Audiobooks")!=std::string::npos && nav_visible(12) && !nav_visible(10));
    audio_home(false,false);assert(audio.entries.size()==1 && audio.entries[0].id==music.id);
    audio_home(true,false);assert(audio.entries.size()==1 && audio.entries[0].id==books.id);
    audio_reset();assert(book_section(books)); // UI reset/reopen retains the persistent preference.
    account_id="classification-managed";assert(!book_section(books));classify(music,true);
    account_id="classification-owner";assert(book_section(books) && !book_section(music));
    selected_server.id="another-server";assert(!book_section(books));selected_server.id="classification-server";
    // Actual Settings/Libraries/Classification control toggles exactly one stable section.
    settings=true;settings_page=12;sidebar=false;focus=0;press(PAD_CROSS);assert(!book_section(books) && !book_section(music));
    press(PAD_CROSS);assert(book_section(books));settings=false;
    nav=13;authorized_sections({film,books,show,music,second});assert(nav>=12 && home_sections[nav-12].id==second.id);
    active_library=second;library_context=true;authorized_sections({show});
    assert(home_sections.size()==1 && nav==0 && !library_context && active_library.id.empty() && nav_visible(12) && !nav_visible(13));
    auto visible=sidebar_order();assert(visible[2]==12 && nav_label(12)==show.title);
    client=std::make_unique<Client>([](const Request &request){assert(request.url.find("/livetv/dvrs")!=std::string::npos);return Reply{200,R"({"MediaContainer":{"Dvr":[{"key":"12"}]}})",""};},"dvr-availability");
    last_input_at=clock_now-3;personal_dvr_probe_pending=true;service_dvr_availability();assert(dvr_availability.valid() && job==None);
    dvr_availability.wait();service_dvr_availability();assert(personal_dvr_available && nav_visible(11));
    personal_dvr_available=false;personal_dvr_probe_pending=true;service_dvr_availability();
    account_id="another-user-before-probe-completes";dvr_availability.wait();service_dvr_availability();assert(!personal_dvr_available && !nav_visible(11));
    clear_profile();assert(home_sections.empty() && !nav_visible(12));
    account="fixture";account_id="one-server-user";profile_screen=false;account_loading=false;force_picker=false;
    client=std::make_unique<Client>([](const Request &){return Reply{0,"","offline fixture"};},"server-selection");
    auto complete_case=[](Job kind,Result result){assert(job==None);std::promise<Result> reply;pending=reply.get_future();reply.set_value(std::move(result));job=kind;pending_generation=screen_generation;finish_job();};
    auto record=[](const char *id){return Json{{"provides","server"},{"clientIdentifier",id},{"accessToken","scoped"},{"connections",Json::array({{{"uri",std::string("https://")+id+".invalid"}}})}};};
    auto one=record("only");complete_case(Resources,{Json::array({one,one}),"",200});
    assert(available.size()==1 && job==Select && connection_target.id=="only");
    pending.wait();finish_job();assert(reconnect_screen && account_pref("plex_preferred_server").empty());
    reconnect_screen=false;force_picker=false;complete_case(Resources,{Json::array({one,record("other")}),"",200});assert(job==None && available.size()==2);
    save_account_pref("plex_preferred_server","other");complete_case(Resources,{Json::array({one,record("other")}),"",200});
    assert(job==Select && connection_target.id=="other");pending.wait();finish_job();assert(reconnect_screen && account_pref("plex_preferred_server")=="other");
    force_picker=true;reconnect_screen=false;complete_case(Resources,{Json::array({one}),"",200});assert(job==None && available.size()==1);
    // A new connection must open Home even if the new server reuses an old
    // section ID and the previous library's tabs retained input focus.
    client=std::make_unique<Client>([](const Request &){return Reply{200,R"({"MediaContainer":{"Metadata":[]}})",""};},"connection-context");
    active_library.id="retained-section";library_context=true;library_tabs_focus=true;
    connection_target={};connection_target.id="only";connection_target.token="scoped";next_base="https://only.invalid";
    complete_case(Select,{Json{{"MediaContainer",{{"Directory",Json::array({{{"key","retained-section"},{"type","movie"},{"title","Authorized library"}}})}}}},"",200});
    assert(!library_context && !library_tabs_focus && active_library.id.empty() && home_screen);
    while(job!=None){pending.wait();finish_job();}
    puts("Permission-aware library routes, profile/server-scoped classifications, Settings controls, stable selection, one/multiple/offline/remembered/duplicate server cases passed");
}
