// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "client.h"
#include "home.h"
#include <algorithm>
#include <cctype>
namespace plex {
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
                        const std::vector<Item> &sections,const std::function<bool()> &current=[] {return true;}) {
    Json rows=Json::array(), warnings=Json::array(), extra_rows=Json::array();
    auto row=[&](const std::string &id,const std::string &title,const Json &data,bool continuing=false,bool extra=false) {
        Json &target=extra ? extra_rows : rows;
        if(continuing)for(const auto &r:rows)if(flag(r,"continuing"))return;
        for(const auto &r:target)if(text(r,"id")==id)return;
        auto entries=items(data,false);if(entries.empty() || target.size()>=(extra ? 4u : 12u))return;
        Json metadata=Json::array();
        for(const auto &i:entries) {
            if(i.type!="movie" && i.type!="episode" && i.type!="show" && i.type!="season")continue;
            metadata.push_back({{"ratingKey",i.id},{"type",i.type},{"title",i.title},{"thumb",i.thumb},{"art",i.art},{"year",i.year},{"duration",i.duration},{"viewOffset",i.offset},{"viewCount",i.watched?1:0},{"parentIndex",i.season},{"index",i.episode},{"grandparentTitle",i.series},{"originallyAvailableAt",i.released}});
            if(metadata.size()==12)break;
        }
        if(!metadata.empty())target.push_back({{"id",id},{"title",title},{"continuing",continuing},{"payload",{{"MediaContainer",{{"Metadata",std::move(metadata)}}}}}});
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
                    auto id=text(hub,"hubIdentifier",text(hub,"key")), lower=id;
                    std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
                    bool is_continue=lower.find("continue")!=std::string::npos;
                    if(is_continue && cw)continue;
                    bool known=is_continue;
                    auto type=text(hub,"type");
                    if(lower.find("recentlyadded")!=std::string::npos && type=="movie") {id="recentlyAddedMovies";known=true;}
                    if(lower.find("recentlyadded")!=std::string::npos && (type=="show" || type=="episode")) {id="recentlyAddedTV";known=true;}
                    if(lower.find("recentlyreleased")!=std::string::npos)known=true;
                    if(lower.find("ondeck")!=std::string::npos)known=true;
                    row(id,text(hub,"title","From your server"),Json{{"MediaContainer",hub}},is_continue,!known);
                }
        }
    }
    auto has=[&](const std::string &part) {for(const auto &r:rows){auto id=text(r,"id");std::transform(id.begin(),id.end(),id.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});if(id.find(part)!=std::string::npos)return true;}return false;};
    std::string movie,show;
    for(const auto &s:sections){if(s.type=="movie" && movie.empty())movie=s.id;if(s.type=="show" && show.empty())show=s.id;}
    auto add=[&](const std::string &id,const std::string &title,const std::string &path) {
        if(rows.size()>=12 || !current())return;
        auto r=get(path);if(r)row(id,title,r.data);
    };
    if(!movie.empty()) {
        if(!has("recentlyaddedmovies"))add("recentlyAddedMovies","Recently Added Movies","/library/sections/"+encode(movie)+"/recentlyAdded?type=1&X-Plex-Container-Size=12");
        if(!has("recentlyreleased"))add("recentlyReleasedMovies","Recently Released Movies","/library/sections/"+encode(movie)+"/all?type=1&sort=originallyAvailableAt:desc&X-Plex-Container-Size=12");
    }
    if(!show.empty() && !has("recentlyaddedtv"))add("recentlyAddedTV","Recently Added TV","/library/sections/"+encode(show)+"/recentlyAdded?type=4&X-Plex-Container-Size=12");
    if(!has("ondeck"))add("onDeck","On Deck / Next Episodes","/library/onDeck?X-Plex-Container-Size=12");
    if(!current())return {{},"Home loading cancelled",0};
    for(auto &row:extra_rows) {if(rows.size()==12)break;rows.push_back(std::move(row));}
    Json ordered=Json::array();bool continuing=false;
    for(const auto &row:rows)if(flag(row,"continuing")) {ordered.push_back(row);continuing=true;}
    for(const auto &row:rows)if(!flag(row,"continuing"))ordered.push_back(row);
    if(continuing)warnings=Json::array();
    return {Json{{"rows",std::move(ordered)},{"warnings",std::move(warnings)}},"",200};
}
}
