// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "home_feed.h"
#include <ctime>
namespace plex {
inline bool personal_library(const Item &section) {
    auto name=section.title;std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});
    return name=="home movies";
}
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
inline void append_seasonal_home(Result &home,Client &client,const std::string &base,const std::string &token,
                                 const std::vector<Item> &sections,int month,const std::function<bool()> &current) {
    if(!home || (month!=10 && month!=12 && month!=1 && month!=2 && (month<6 || month>8)))return;
    Json metadata=Json::array();std::set<std::string> seen;int queried=0;
    auto matches=[&](std::string name){std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});
        if(month==10)return name.find("halloween")!=std::string::npos || name=="horror";
        if(month==12)return name.find("christmas")!=std::string::npos || name.find("holiday")!=std::string::npos;
        return name.find(month==1 || month==2?"winter":"summer")!=std::string::npos;
    };
    for(const auto &section:sections) {
        if(!current())return;
        if((section.type!="movie" && section.type!="show") || personal_library(section))continue;
        if(++queried>4)break;
        auto path="/library/sections/"+encode(section.id);Result matched{{},"No match",0};
        auto genres=client.call("GET",base,path+"/genre",token);
        if(genres){auto mc=genres.data.find("MediaContainer");if(mc!=genres.data.end()){auto list=mc->find("Directory");if(list!=mc->end() && list->is_array())for(const auto &genre:*list)if(matches(text(genre,"title",text(genre,"tag")))){matched=client.browse(base,token,path+"/all?genre="+encode(text(genre,"key")),0,12);break;}}}
        if(!matched && current()){
            auto collections=client.browse(base,token,path+"/collections",0,60);
            if(collections)for(const auto &collection:items(collections.data,false))if(matches(collection.title)){matched=client.browse(base,token,"/library/collections/"+encode(collection.id)+"/children",0,12);break;}
        }
        if(matched)for(const auto &item:items(matched.data,false)){
            if((item.type!="movie" && item.type!="show" && item.type!="episode") || !seen.insert(item.id).second || metadata.size()>=12)continue;
            metadata.push_back({{"ratingKey",item.id},{"type",item.type},{"title",item.title},{"thumb",item.thumb},{"art",item.art},{"year",item.year},{"duration",item.duration},{"viewOffset",item.offset},{"viewCount",item.watched?1:0},{"grandparentTitle",item.series},{"parentIndex",item.season},{"index",item.episode}});
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
    auto add=[&](const std::string &id,const std::string &title,const Result &response,bool continuing=false){
        if(response && !items(response.data,false).empty() && rows.size()<12)rows.push_back({{"id",id},{"title",title},{"continuing",continuing},{"payload",response.data}});
    };
    auto path="/library/sections/"+encode(section.id);
    auto type=section.type=="show"?"4":"1";
    add("continuing","Continue Watching",client.browse(base,token,path+"/all?type="+type+"&inProgress=1",0,12),true);
    if(!personal_library(section))add("released",section.type=="show"?"Recently Released Episodes":"Recently Released Movies",client.browse(base,token,path+"/all?type="+type+"&sort=originallyAvailableAt:desc",0,12));
    add("added",personal_library(section)?"Recently Added Home Movies":section.type=="show"?"Recently Added TV Shows":"Recently Added Movies",client.browse(base,token,path+"/recentlyAdded",0,12));
    auto hubs=client.call("GET",base,"/hubs/sections/"+encode(section.id)+"?count=12&includeEmpty=0",token);
    if(hubs){auto mc=hubs.data.find("MediaContainer");if(mc!=hubs.data.end()){auto list=mc->find("Hub");if(list!=mc->end() && list->is_array())for(const auto &hub:*list)add("hub:"+text(hub,"hubIdentifier"),text(hub,"title","From your library"),{Json{{"MediaContainer",hub}},"",200});}}
    if(seasonal && !personal_library(section) && (month==10 || month==12)) {
        auto genres=client.call("GET",base,path+"/genre",token);
        if(genres){auto mc=genres.data.find("MediaContainer");if(mc!=genres.data.end()){auto dirs=mc->find("Directory");if(dirs!=mc->end() && dirs->is_array())for(const auto &tag:*dirs){
            auto name=text(tag,"title",text(tag,"tag")),lower=name;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return std::tolower(c);});
            bool match=month==10?(lower=="horror" || lower=="halloween"):(lower=="christmas" || lower=="holiday");
            if(match){add("seasonal",month==10?"Halloween / Horror":"Christmas / Holiday",client.browse(base,token,path+"/all?genre="+encode(text(tag,"key")),0,12));break;}
        }}}
    }
    add("collections","Collections",client.browse(base,token,path+"/collections",0,12));
    return {Json{{"rows",std::move(rows)}},"",200};
}
}
