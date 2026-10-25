// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "home_feed.h"
#include <ctime>
namespace plex {
inline std::vector<Item> video_libraries(const std::vector<Item> &sections,int kind) {
    std::vector<Item> result;
    for(const auto &s:sections)if(kind==9?personal_library(s):(s.type==(kind==2?"show":"movie") && !personal_library(s)))result.push_back(s);
    return result;
}
inline Item default_library(const std::vector<Item> &sections,int kind,const std::string &remembered) {
    auto allowed=video_libraries(sections,kind);
    for(const auto &s:allowed)if(s.id==remembered)return s;
    return allowed.empty()?Item{}:allowed.front();
}
inline int season_month(){auto now=std::time(nullptr);std::tm value{};return localtime_r(&now,&value)?value.tm_mon+1:0;}
inline bool seasonal_tag(std::string name,int month) {
    std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});
    if(month==10)return name.find("halloween")!=std::string::npos || name=="horror";
    if(month==12)return name.find("christmas")!=std::string::npos || name.find("holiday")!=std::string::npos;
    if(month==1 || month==2)return name.find("winter")!=std::string::npos;
    return month>=6 && month<=8 && name.find("summer")!=std::string::npos;
}
inline Result seasonal_library(Client &client,const std::string &base,const std::string &token,const Item &section,int month,
                               const std::function<bool()> &current=[] {return true;}) {
    if((month!=10 && month!=12 && month!=1 && month!=2 && (month<6 || month>8)) || personal_library(section) || !current())return {{},"Seasonal section is unavailable",0};
    auto path="/library/sections/"+encode(section.id);
    auto genres=client.call("GET",base,path+"/genre",token);
    if(genres){auto mc=genres.data.find("MediaContainer");if(mc!=genres.data.end()){auto list=mc->find("Directory");if(list!=mc->end() && list->is_array())for(const auto &genre:*list)
        if(current() && seasonal_tag(text(genre,"title",text(genre,"tag")),month))return client.browse(base,token,path+"/all?genre="+encode(text(genre,"key")),0,12);}}
    if(!current())return {{},"Seasonal loading cancelled",0};
    auto collections=client.browse(base,token,path+"/collections",0,60);
    if(collections)for(const auto &collection:items(collections.data,false))if(current() && seasonal_tag(collection.title,month))
        return client.browse(base,token,"/library/collections/"+encode(collection.id)+"/children",0,12);
    return {{},"No matching seasonal genre or collection",0};
}
// Explicit routes keep category directories distinct from playable metadata.
inline Result library_categories(Client &client,const std::string &base,const std::string &token,const Item &section,int start=0,int count=200) {
    auto path="/library/sections/"+encode(section.id);Json entries=Json::array();
    auto genres=client.call("GET",base,path+"/genre"+(section.type=="artist"?"?type=9":""),token);
    if(genres){auto mc=genres.data.find("MediaContainer");if(mc!=genres.data.end()){auto list=mc->find("Directory");if(list!=mc->end() && list->is_array())for(const auto &genre:*list){
        auto key=text(genre,"key"),title=text(genre,"title",text(genre,"tag"));
        if(key.empty() || key.size()>256 || title.empty() || entries.size()>=100)continue;
        entries.push_back({{"ratingKey","genre-"+std::to_string(entries.size())},{"type","category"},{"thumb",safe_image_identity(text(genre,"thumb"))},{"art",safe_image_identity(text(genre,"art"))},{"title","Genre: "+title.substr(0,240)},{"key",path+"/all?"+(section.type=="artist"?"type=9&":"")+"genre="+encode(key)}});
    }}}
    auto collections=client.browse(base,token,path+"/collections",0,100);
    if(collections)for(const auto &item:items(collections.data,false))if(item.type=="collection" && entries.size()<200)
        {auto value=item_json(item);value["title"]="Collection: "+item.title;value["librarySectionID"]=section.id;entries.push_back(std::move(value));}
    if(!genres && !collections)return {{},"Library categories unavailable for this profile",genres.status};
    const int total=static_cast<int>(entries.size());Json page=Json::array();
    for(int i=std::max(0,start);i<total && i<std::max(0,start)+std::clamp(count,1,200);++i)page.push_back(std::move(entries[i]));
    return {Json{{"MediaContainer",{{"Metadata",std::move(page)},{"totalSize",total}}}},"",200};
}
inline void append_seasonal_home(Result &home,Client &client,const std::string &base,const std::string &token,
                                 const std::vector<Item> &sections,int month,const std::function<bool()> &current) {
    if(!home || (month!=10 && month!=12 && month!=1 && month!=2 && (month<6 || month>8)))return;
    Json metadata=Json::array();std::set<std::string> seen;int queried=0;
    for(const auto &section:sections) {
        if(!current())return;
        if((section.type!="movie" && section.type!="show") || personal_library(section))continue;
        if(++queried>4)break;
        auto matched=seasonal_library(client,base,token,section,month,current);
        if(matched)for(const auto &item:items(matched.data,false)){
            if((item.type!="movie" && item.type!="show" && item.type!="episode") || !seen.insert(item.id).second || metadata.size()>=12)continue;
            metadata.push_back({{"ratingKey",item.id},{"librarySectionID",section.id},{"type",item.type},{"title",item.title},{"thumb",item.thumb},{"art",item.art},{"year",item.year},{"duration",item.duration},{"viewOffset",item.offset},{"viewCount",item.watched?1:0},{"grandparentTitle",item.series},{"parentIndex",item.season},{"index",item.episode}});
        }
    }
    auto rows=home.data.find("rows");
    if(!metadata.empty() && rows!=home.data.end() && rows->is_array() && current()){
        auto at=rows->begin()+std::min<size_t>(5,rows->size());
        rows->insert(at,Json{{"id","seasonal"},{"title","Seasonal Recommendations"},{"payload",{{"MediaContainer",{{"Metadata",std::move(metadata)}}}}}});
        if(rows->size()>12)rows->erase(rows->begin()+12,rows->end());
    }
}
inline Result library_feed(Client &client,const std::string &base,const std::string &token,const Item &section,bool seasonal=true,int month=season_month()) {
    Json rows=Json::array();
    auto add=[&](std::string id,const std::string &title,const Result &response,bool continuing=false){
        if(id=="added")id=section.type=="show"?"recentlyAddedTV":"recentlyAddedMovies";
        if(id=="released")id=section.type=="show"?"recentlyReleasedTV":"recentlyReleasedMovies";
        if(id=="continuing")id="continueWatching";
        for(const auto &row:rows)if(text(row,"id")==id)return;
        if(!response || rows.size()>=12)return;
        Json metadata=Json::array();std::set<std::string> seen;
        for(const auto &item:items(response.data,false)){
            if(id=="recentlyAddedTV" && item.type!="show")continue;
            if((id=="recentlyReleasedTV" || id=="recentlyReleasedMovies") && !released_date(item.released))continue;
            if(!seen.insert(item.id).second)continue;
            auto value=item_json(item);if(item.section_id.empty())value["librarySectionID"]=section.id;metadata.push_back(std::move(value));
        }
        if(id.rfind("recentlyReleased",0)==0)std::stable_sort(metadata.begin(),metadata.end(),[](const Json &a,const Json &b){return text(a,"originallyAvailableAt")>text(b,"originallyAvailableAt");});
        if(id.rfind("recentlyAdded",0)==0)std::stable_sort(metadata.begin(),metadata.end(),[](const Json &a,const Json &b){return number(a,"addedAt")>number(b,"addedAt");});
        if(!metadata.empty())rows.push_back({{"id",id},{"title",title},{"continuing",continuing},{"payload",{{"MediaContainer",{{"Metadata",std::move(metadata)}}}}}});
    };
    auto path="/library/sections/"+encode(section.id);
    auto type=section.type=="show"?"4":"1";
    add("continuing","Continue Watching",client.browse(base,token,path+"/all?type="+type+"&inProgress=1",0,12),true);
    if(!personal_library(section))add("released",section.type=="show"?"Recently Released Episodes":"Recently Released Movies",client.browse(base,token,path+"/all?type="+type+"&sort=originallyAvailableAt:desc",0,12));
    add("added",personal_library(section)?"Recently Added Home Movies":section.type=="show"?"Recently Added TV Shows":"Recently Added Movies",client.browse(base,token,path+(section.type=="show"?"/all?type=2&sort=addedAt:desc":"/recentlyAdded?type=1"),0,12));
    auto hubs=client.call("GET",base,"/hubs/sections/"+encode(section.id)+"?count=12&includeEmpty=0",token);
    if(hubs){auto mc=hubs.data.find("MediaContainer");if(mc!=hubs.data.end()){auto list=mc->find("Hub");if(list!=mc->end() && list->is_array())for(const auto &hub:*list)add(hub_semantic(hub).empty()?"hub:"+text(hub,"hubIdentifier"):hub_semantic(hub),semantic_title(hub_semantic(hub),text(hub,"title","From your library")),{Json{{"MediaContainer",hub}},"",200});}}
    if(seasonal && !personal_library(section))add("seasonal","Seasonal Recommendations",seasonal_library(client,base,token,section,month));
    add("collections","Collections",client.browse(base,token,path+"/collections",0,12));
    return {Json{{"rows",std::move(rows)}},"",200};
}
}
