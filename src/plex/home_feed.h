// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "client.h"
#include "home.h"
#include "presentation.h"
#include <algorithm>
#include <cctype>
#include <set>
namespace plex {
inline bool personal_library(const Item &section) {
    if(section.personal)return true;
    auto name=section.title;std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});
    return name=="home movies";
}
struct Hub {std::string id,title;bool continuing=false;std::vector<Item> entries;};
inline std::string stable_media_identity(const Item &item){return item.type+":"+(item.guid.empty()?item.id:item.guid);}
inline std::vector<Hub> home_rows(const Json &data) {
    std::vector<Hub> rows;auto list=data.find("rows");if(list==data.end() || !list->is_array())return rows;
    for(const auto &row:*list) {
        auto payload=row.find("payload");if(payload==row.end())continue;
        auto entries=items(*payload,false);if(entries.empty())continue;
        if(entries.size()>12)entries.resize(12);
        rows.push_back({text(row,"id"),text(row,"title"),flag(row,"continuing",false),std::move(entries)});
        if(rows.size()==12)break;
    }
    return rows;
}
// Home is a closed six-slot composition. Global/scoped aliases feed one slot,
// never independent rails; IDs remain stable through refresh and warm snapshots.
inline const std::vector<std::string> &home_order(){
    static const std::vector<std::string> value={"continueWatching","recentlyReleasedMovies","recentlyReleasedTV","recentlyAddedMovies","recentlyAddedTV","seasonal"};return value;
}
inline Json ordered_home(const Json &source){
    Json rows=Json::array();auto list=source.find("rows");
    if(list!=source.end() && list->is_array())for(const auto &id:home_order())for(const auto &row:*list)
        if(text(row,"id")==id){rows.push_back(row);break;}
    return Json{{"rows",rows},{"warnings",source.value("warnings",Json::array())}};
}
inline Result home_feed(Client &client,const std::string &base,const std::string &token,
                        const std::vector<Item> &sections,const std::function<bool()> &current=[] {return true;},
                        const std::function<void(const Json&)> &publish={}) {
    std::map<std::string,std::vector<Item>> slots;Json warnings=Json::array();
    std::set<std::string> authorized;for(const auto &section:sections)authorized.insert(section.id);
    auto snapshot=[&] {
        Json rows=Json::array();for(const auto &id:home_order()) {
            auto &entries=slots[id];if(entries.empty())continue;
            if(id.find("Released")!=std::string::npos)std::stable_sort(entries.begin(),entries.end(),[](const Item &a,const Item &b){return a.released==b.released?stable_media_identity(a)<stable_media_identity(b):a.released>b.released;});
            else if(id.find("Added")!=std::string::npos)std::stable_sort(entries.begin(),entries.end(),[](const Item &a,const Item &b){return a.added==b.added?stable_media_identity(a)<stable_media_identity(b):a.added>b.added;});
            Json values=Json::array();for(const auto &item:entries){values.push_back(item_json(item));if(values.size()==12)break;}
            rows.push_back({{"id",id},{"title",semantic_title(id,id)},{"continuing",id=="continueWatching"},{"payload",{{"MediaContainer",{{"Metadata",values}}}}}});
        }return Json{{"rows",rows},{"warnings",warnings}};
    };
    auto merge=[&](const std::string &id,const Json &payload,const std::string &section="") {
        if(std::find(home_order().begin(),home_order().end(),id)==home_order().end())return;
        auto &target=slots[id];std::set<std::string> seen;for(const auto &item:target)seen.insert(stable_media_identity(item));
        for(auto item:items(payload,false)) {
            if(item.id.empty() || (!item.section_id.empty() && !authorized.count(item.section_id)))continue;
            if(item.section_id.empty())item.section_id=section;
            if(id=="continueWatching") {if(item.type!="movie" && item.type!="episode")continue;if(item.watched && item.offset<=0)continue;if(item.duration>0 && item.offset>=item.duration)continue;}
            else if(id=="recentlyAddedTV") {if(item.type!="show")continue;}
            else if(item.type!=(id=="recentlyReleasedTV"?"episode":"movie"))continue;
            if(id.find("Released")!=std::string::npos && !released_date(item.released))continue;
            if(seen.insert(stable_media_identity(item)).second && target.size()<384)target.push_back(std::move(item));
            else if(id!="continueWatching")for(auto &old:target)if(old.id==item.id){
                auto fill=[](std::string &value,const std::string &fallback){if(value.empty())value=fallback;};
                fill(old.thumb,item.thumb);fill(old.art,item.art);fill(old.parent_thumb,item.parent_thumb);fill(old.grandparent_thumb,item.grandparent_thumb);fill(old.parent_art,item.parent_art);fill(old.grandparent_art,item.grandparent_art);
                old.added=std::max(old.added,item.added);if(old.released.empty())old.released=item.released;
                break;
            }
        }
        if(publish && current())publish(snapshot());
    };
    auto get=[&](const std::string &path){return current()?client.call("GET",base,path,token):Result{{},"Home loading cancelled",0};};
    auto cw=get("/hubs/continueWatching?X-Plex-Container-Start=0&X-Plex-Container-Size=12");
    if(cw)merge("continueWatching",cw.data);else warnings.push_back("Continue Watching unavailable on this server (HTTP "+std::to_string(cw.status)+")");
    auto hubs=get("/hubs?count=12&includeEmpty=0");
    if(hubs){auto mc=hubs.data.find("MediaContainer");if(mc!=hubs.data.end() && mc->is_object()){
        auto list=mc->find("Hub");if(list!=mc->end() && list->is_array())for(const auto &hub:*list){
            auto scope=text(hub,"librarySectionID",number(hub,"librarySectionID")>0?std::to_string(number(hub,"librarySectionID")):"");
            if(!scope.empty() && !authorized.count(scope))continue;
            merge(hub_semantic(hub),Json{{"MediaContainer",hub}},scope);
        }
    }}
    struct Category {const char *id,*type,*path;};
    const Category categories[]={
        {"recentlyReleasedMovies","movie","/all?type=1&sort=originallyAvailableAt:desc"},
        {"recentlyReleasedTV","show","/all?type=4&sort=originallyAvailableAt:desc"},
        {"recentlyAddedMovies","movie","/recentlyAdded?type=1"},
        {"recentlyAddedTV","show","/all?type=2&sort=addedAt:desc"}};
    for(const auto &category:categories){int queried=0;for(const auto &section:sections){
        if(!current())return {{},"Home loading cancelled",0};
        if(section.type!=category.type || personal_library(section))continue;
        if(++queried>32){warnings.push_back("Home aggregation capped at 32 authorized sections per category");break;}
        auto result=get("/library/sections/"+encode(section.id)+category.path+"&X-Plex-Container-Size=12");
        if(result)merge(category.id,result.data,section.id);
    }}
    if(!current())return {{},"Home loading cancelled",0};
    return {snapshot(),"",200};
}
}
