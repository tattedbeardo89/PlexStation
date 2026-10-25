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
inline Result home_feed(Client &client,const std::string &base,const std::string &token,
                        const std::vector<Item> &sections,const std::function<bool()> &current=[] {return true;},
                        const std::function<void(const Json&)> &publish={}) {
    Json rows=Json::array(), warnings=Json::array(), extra_rows=Json::array();
    auto row=[&](const std::string &id,const std::string &title,const Json &data,bool continuing=false,bool extra=false) {
        Json &target=extra ? extra_rows : rows;
        if(continuing)for(const auto &r:rows)if(flag(r,"continuing"))return;
        for(const auto *list:{&rows,&extra_rows})for(const auto &r:*list)if(text(r,"id")==id)return;
        auto entries=items(data,false);if(entries.empty() || target.size()>=(extra ? 4u : 12u))return;
        Json metadata=Json::array();std::set<std::string> item_ids;
        for(const auto &i:entries) {
            if(i.type!="movie" && i.type!="episode" && i.type!="show" && i.type!="season" && i.type!="collection")continue;
            if(id.rfind("recentlyAddedTV",0)==0 && i.type!="show")continue;
            if(id.rfind("recentlyReleasedTV",0)==0 && i.type!="episode")continue;
            if((id.rfind("recentlyAddedMovies",0)==0 || id.rfind("recentlyReleasedMovies",0)==0) && i.type!="movie")continue;
            if(id.find("recentlyReleased")!=std::string::npos && !released_date(i.released))continue;
            if(!item_ids.insert(i.type+":"+i.id).second)continue;
            metadata.push_back(item_json(i));
            if(metadata.size()==12)break;
        }
        if(id.rfind("recentlyReleased",0)==0)std::stable_sort(metadata.begin(),metadata.end(),[](const Json &a,const Json &b){return text(a,"originallyAvailableAt")>text(b,"originallyAvailableAt");});
        if(id.rfind("recentlyAdded",0)==0)std::stable_sort(metadata.begin(),metadata.end(),[](const Json &a,const Json &b){return number(a,"addedAt")>number(b,"addedAt");});
        // A global/scoped alias with identical content is one logical rail.
        // Distinct explicit library scopes remain distinct even with similar titles.
        auto at=id.find('@');auto semantic=id.substr(0,at);
        if(!metadata.empty())for(const auto &old:rows){auto old_id=text(old,"id");auto split=old_id.find('@');
            if(old_id.substr(0,split)!=semantic || (at!=std::string::npos && split!=std::string::npos))continue;
            std::set<std::string> before,after;for(const auto &item:items(old["payload"],false))before.insert(item.type+":"+item.id);
            for(const auto &item:metadata)after.insert(text(item,"type")+":"+text(item,"ratingKey"));
            if(before==after)return;
        }
        if(!metadata.empty())target.push_back({{"id",id},{"title",title},{"continuing",continuing},{"payload",{{"MediaContainer",{{"Metadata",std::move(metadata)}}}}}});
        if(publish && current())publish(Json{{"rows",rows}});
    };
    auto get=[&](const std::string &path) {
        if(!current())return Result{{},"Home loading cancelled",0};
        return client.call("GET",base,path,token);
    };
    auto cw=get("/hubs/continueWatching?X-Plex-Container-Start=0&X-Plex-Container-Size=12");
    if(cw)row("continueWatching","Continue Watching",cw.data,true);
    else warnings.push_back("Continue Watching unavailable on this server (HTTP "+std::to_string(cw.status)+")");
    if(!current())return {{},"Home loading cancelled",0};
    auto hubs=get("/hubs?count=12&includeEmpty=0");
    if(hubs) {
        auto c=hubs.data.find("MediaContainer");
        if(c!=hubs.data.end() && c->is_object()) {
            auto h=c->find("Hub");if(h!=c->end() && h->is_array())
                for(const auto &hub:*h) {
                    auto original=text(hub,"hubIdentifier",text(hub,"key"));auto semantic=hub_semantic(hub);
                    auto scope=text(hub,"librarySectionID",number(hub,"librarySectionID")>0?std::to_string(number(hub,"librarySectionID")):"");
                    if(scope.empty()){
                        auto entries=items(Json{{"MediaContainer",hub}},false);std::string common;bool mixed=false;
                        for(const auto &item:entries){if(item.section_id.empty()){mixed=true;break;}if(common.empty())common=item.section_id;else if(common!=item.section_id){mixed=true;break;}}
                        if(!mixed)scope=common;
                    }
                    std::string suffix;
                    if(!scope.empty())for(const auto &section:sections)if(section.id==scope){suffix=" • "+section.title;break;}
                    auto id=semantic.empty()?"hub:"+original:semantic+(scope.empty()?"":"@"+scope);
                    row(id,semantic_title(semantic,text(hub,"title","From your server"))+suffix,Json{{"MediaContainer",hub}},semantic=="continueWatching",semantic.empty());
                }
        }
    }
    auto has=[&](const std::string &part) {for(const auto &r:rows){auto id=text(r,"id");std::transform(id.begin(),id.end(),id.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});if(id.find(part)!=std::string::npos)return true;}return false;};
    auto add=[&](const std::string &id,const std::string &title,const std::string &path) {
        if(rows.size()>=12 || !current())return;
        auto r=get(path);if(r)row(id,title,r.data);
    };
    auto aggregate=[&](const std::string &id,const std::string &title,const std::string &type,const std::string &suffix,bool released=false){
        Json merged=Json::array();std::set<std::string> seen;int queried=0;
        for(const auto &section:sections) {
            if((type=="video"?(section.type!="movie" && section.type!="show"):section.type!=type) || personal_library(section) || !current())continue;
            if(++queried>(id=="personalizedCollections"?4:32)){warnings.push_back("Supplemental Home aggregation capped at 32 sections per category");break;}
            auto response=get("/library/sections/"+encode(section.id)+suffix+"&X-Plex-Container-Size=12");if(!response)continue;
            auto mc=response.data.find("MediaContainer");if(mc==response.data.end() || !mc->is_object())continue;
            int count=0;for(const auto &i:items(response.data,false)){
                if(++count>12)break;
                if((id=="personalizedCollections" && i.type!="collection") || !seen.insert(i.id).second)continue;
                if(released && !released_date(i.released))continue;
                auto value=item_json(i);if(i.section_id.empty())value["librarySectionID"]=section.id;
                merged.push_back(std::move(value));
            }
            std::stable_sort(merged.begin(),merged.end(),[&](const Json &a,const Json &b){return released?text(a,"originallyAvailableAt")>text(b,"originallyAvailableAt"):number(a,"addedAt")>number(b,"addedAt");});
            if(merged.size()>12)merged.erase(merged.begin()+12,merged.end());
        }
        std::stable_sort(merged.begin(),merged.end(),[&](const Json &a,const Json &b){return released?text(a,"originallyAvailableAt")>text(b,"originallyAvailableAt"):number(a,"addedAt")>number(b,"addedAt");});
        if(merged.size()>12)merged.erase(merged.begin()+12,merged.end());
        row(id,title,Json{{"MediaContainer",{{"Metadata",std::move(merged)}}}});
    };
    if(!has("recentlyaddedmovies"))aggregate("recentlyAddedMovies","Recently Added Movies","movie","/recentlyAdded?type=1");
    if(!has("recentlyreleasedmovies"))aggregate("recentlyReleasedMovies","Recently Released Movies","movie","/all?type=1&sort=originallyAvailableAt:desc",true);
    if(!has("recentlyaddedtv"))aggregate("recentlyAddedTV","Recently Added TV Shows","show","/all?type=2&sort=addedAt:desc");
    if(!has("recentlyreleasedtv"))aggregate("recentlyReleasedTV","Recently Released TV Episodes","show","/all?type=4&sort=originallyAvailableAt:desc",true);
    if(!has("collections"))aggregate("personalizedCollections","Personalized Collections","video","/collections?sort=titleSort:asc");
    if(!has("ondeck"))add("onDeck","On Deck / Next Episodes","/library/onDeck?X-Plex-Container-Size=12");
    if(!current())return {{},"Home loading cancelled",0};
    for(auto &row:extra_rows) {if(rows.size()==12)break;rows.push_back(std::move(row));}
    Json ordered=Json::array();bool continuing=false;
    for(const auto &row:rows)if(flag(row,"continuing")) {ordered.push_back(row);continuing=true;}
    const char *preferred[]={"recentlyReleasedMovies","recentlyAddedMovies","recentlyReleasedTV","recentlyAddedTV"};
    std::set<std::string> placed;
    for(const auto *id:preferred)for(const auto &row:rows)if(text(row,"id")==id || text(row,"id").rfind(std::string(id)+"@",0)==0){ordered.push_back(row);placed.insert(text(row,"id"));}
    for(const auto &row:rows)if(!flag(row,"continuing") && !placed.count(text(row,"id")))ordered.push_back(row);
    if(continuing)warnings=Json::array();
    return {Json{{"rows",std::move(ordered)},{"warnings",std::move(warnings)}},"",200};
}
}
