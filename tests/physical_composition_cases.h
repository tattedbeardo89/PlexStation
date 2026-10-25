// Called from the existing presentation suite; real parser/composition, fixture PMS.
inline void physical_composition_cases(){
    for(const auto &malformed:{Json(),Json::array(),Json(7),Json("invalid cache shape")})assert(home_rows(ordered_home(malformed)).empty());
    Item movies;movies.id="films";movies.type="movie";
    Item series;series.id="shows";series.type="show";
    Client feed([](const Request &request){
        if(request.url.find("/hubs/continueWatching")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"resume","type":"episode","librarySectionID":"shows","viewOffset":100,"duration":1000},{"ratingKey":"resume","type":"episode","librarySectionID":"shows","viewOffset":100},{"ratingKey":"finished","type":"movie","viewCount":1},{"ratingKey":"private","type":"movie","librarySectionID":"denied"}]}})",""};
        if(request.url.find("/hubs?")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Hub":[{"hubIdentifier":"movie.recentlyAdded","type":"movie","Metadata":[{"ratingKey":"film","type":"movie","librarySectionID":"films","addedAt":5}]},{"hubIdentifier":"movie.recentlyAdded","type":"movie","Metadata":[{"ratingKey":"film","type":"movie","librarySectionID":"films","addedAt":5}]},{"hubIdentifier":"ondeck","type":"episode","Metadata":[{"ratingKey":"no-ondeck","type":"episode"}]},{"hubIdentifier":"random","title":"Other extra","Metadata":[{"ratingKey":"extra","type":"movie"}]},{"hubIdentifier":"recentlyAdded","type":"movie","librarySectionID":"denied","Metadata":[{"ratingKey":"leak","type":"movie"}]}]}})",""};
        if(request.url.find("originallyAvailableAt")!=std::string::npos){bool tv=request.url.find("shows")!=std::string::npos;return Reply{200,Json{{"MediaContainer",{{"Metadata",Json::array({{{"ratingKey",tv?"air":"release"},{"type",tv?"episode":"movie"},{"originallyAvailableAt","2020-01-01"}}})}}}}.dump(),""};}
        if(request.url.find("type=2")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"series","type":"show","addedAt":10},{"ratingKey":"not-series","type":"episode","addedAt":20}]}})",""};
        return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"film","type":"movie","addedAt":5}]}})",""};
    },"physical-composition");
    auto result=home_feed(feed,"https://fixture.invalid","profile",{movies,series});
    auto rows=home_rows(result.data);assert(rows.size()==5);
    for(size_t i=0;i<rows.size();++i)assert(rows[i].id==home_order()[i] && rows[i].entries.size()==1);
    assert(rows[0].entries[0].id=="resume" && rows[4].entries[0].type=="show");
    assert(result.data==home_feed(feed,"https://fixture.invalid","profile",{movies,series}).data);
    Client seasonal([](const Request &request){
        if(request.url.find("/genre")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Directory":[{"key":"horror","title":"Horror"}]}})",""};
        return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"season-film","type":"movie","title":"Seasonal fixture","parentThumb":"/retained-parent"}]}})",""};
    },"seasonal-order");
    append_seasonal_home(result,seasonal,"https://fixture.invalid","profile",{movies,series},10,[]{return true;});
    append_seasonal_home(result,seasonal,"https://fixture.invalid","profile",{movies,series},10,[]{return true;});
    auto six=home_rows(result.data);assert(six.size()==6 && six.back().entries[0].parent_thumb=="/retained-parent");
    for(size_t i=0;i<six.size();++i)assert(six[i].id==home_order()[i]);
    Client aliases([](const Request &request){
        if(request.url.find("/hubs/sections/")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Hub":[{"hubIdentifier":"tv.inprogress","type":"episode","Metadata":[{"ratingKey":"second","type":"episode"}]},{"hubIdentifier":"movie.inprogress","type":"movie","Metadata":[{"ratingKey":"second","type":"movie"}]},{"hubIdentifier":"tv.recentlyadded","type":"episode","Metadata":[{"ratingKey":"wrong-added","type":"episode"}]},{"hubIdentifier":"alias","title":"Continue Watching","type":"episode","Metadata":[{"ratingKey":"third","type":"episode"}]}]}})",""};
        if(request.url.find("type=2")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"top-added","type":"show"}]}})",""};
        return Reply{200,Json{{"MediaContainer",{{"Metadata",Json::array({{{"ratingKey","top-resume"},{"type",request.url.find("films")!=std::string::npos?"movie":"episode"},{"viewOffset",20},{"duration",1000}}})}}}}.dump(),""};
    },"aliases");
    for(const auto &section:{movies,series}){
        auto library=home_rows(library_feed(aliases,"https://fixture.invalid","profile",section,false).data);
        assert(!library.empty() && library[0].id=="continueWatching" && library[0].entries[0].id=="top-resume");
        assert(std::count_if(library.begin(),library.end(),[](const Hub &row){return row.id=="continueWatching";})==1);
        if(section.type=="show")assert(std::count_if(library.begin(),library.end(),[](const Hub &row){return row.id=="recentlyAddedTV" && row.entries[0].id=="top-added";})==1);
    }
    auto resources=Json::array({{{"provides","server"},{"clientIdentifier","same"},{"accessToken","scoped"},{"connections",Json::array({{{"uri","https://local.invalid"}},{{"uri","https://local.invalid"}}})}},{{"provides","server"},{"machineIdentifier","same"},{"accessToken","scoped"},{"connections",Json::array({{{"uri","https://remote.invalid"}}})}}});
    Client fallback([](const Request &request){
        if(request.url.find("inProgress=1")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[]}})",""};
        if(request.url.find("/hubs/sections/")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Hub":[{"hubIdentifier":"movie.inprogress","type":"movie","Metadata":[{"ratingKey":"fallback-resume","type":"movie","viewOffset":50,"duration":1000}]}]}})",""};
        return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"added","type":"movie","originallyAvailableAt":"2020-01-01"}]}})",""};
    },"fallback-position");
    auto fallback_rows=home_rows(library_feed(fallback,"https://fixture.invalid","profile",movies,false).data);
    assert(fallback_rows.size()>=2 && fallback_rows[0].id=="continueWatching" && fallback_rows[0].entries[0].id=="fallback-resume");
    auto unique=servers(resources);assert(unique.size()==1 && unique[0].id=="same" && unique[0].connections.size()==2);
    std::cout<<"Physical report regressions: exact five populated Home slots, forbidden hubs/unauthorized items excluded, deterministic refresh, top Continue Watching/TV-added aliases and machine-ID dedup passed\n";
}
