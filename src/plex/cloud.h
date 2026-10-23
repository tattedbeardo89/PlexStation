// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "client.h"
namespace plex {
inline std::string cloud_id(const Item &item) {
    auto prefix=item.type=="movie"?"plex://movie/":item.type=="show"?"plex://show/":"";
    if(!*prefix || item.guid.rfind(prefix,0)!=0)return "";
    auto id=item.guid.substr(std::string(prefix).size());
    return !id.empty() && id.size()<=64 && id.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos?id:"";
}
inline Result cloud_watchlist(Client &c,const std::string &token,int start=0,int type=0,int sort=0) {
    if(token.empty() || start<0)return {{},"Cloud Watchlist authorization unavailable",0};
    return c.call("GET","https://discover.provider.plex.tv","/library/sections/watchlist/all?includeCollections=1&includeExternalMedia=1&X-Plex-Container-Start="+std::to_string(start)+"&X-Plex-Container-Size=60"+(type==1?"&type=1":type==2?"&type=2":"")+(sort==1?"&sort=titleSort%3Aasc":sort==2?"&sort=watchlistedAt%3Aasc":"&sort=watchlistedAt%3Adesc"),token);
}
inline Result cloud_change(Client &c,const std::string &token,const Item &item,bool add) {
    auto id=cloud_id(item);if(id.empty() || token.empty())return {{},"No supported Plex cloud identity",0};
    return c.call("PUT","https://discover.provider.plex.tv",std::string("/actions/")+(add?"addToWatchlist":"removeFromWatchlist")+"?ratingKey="+encode(id),token);
}
inline Result resolve_cloud(Client &c,const std::string &base,const std::string &token,const Item &item) {
    if(cloud_id(item).empty())return {{},"Cloud item has no supported Plex GUID",0};
    return c.call("GET",base,"/library/all?guid="+encode(item.guid)+"&type="+(item.type=="movie"?"1":"2"),token);
}
}
