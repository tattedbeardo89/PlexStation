// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "home_feed.h"
namespace plex {
inline Result global_search(Client &c,const std::string &base,const std::string &token,const std::string &query,int start=0) {
    if(query.empty() || query.size()>200 || start<0)return {{},"Enter a search query",0};
    auto result=c.call("GET",base,"/hubs/search?query="+encode(query)+"&limit=12&includeCollections=1&X-Plex-Container-Start="+std::to_string(start)+"&X-Plex-Container-Size=12",token);
    if(!result)return result;
    Json rows=Json::array();auto mc=result.data.find("MediaContainer");
    if(mc==result.data.end() || !mc->is_object())return {{},"Invalid grouped search response",0};
    auto hubs=mc->find("Hub");if(hubs!=mc->end() && hubs->is_array())for(const auto &hub:*hubs){
        auto entries=items(Json{{"MediaContainer",hub}},false);if(entries.empty())continue;
        rows.push_back({{"id",text(hub,"hubIdentifier",text(hub,"type"))},{"title",text(hub,"title",text(hub,"type","Search results"))},{"payload",{{"MediaContainer",hub}}}});
        if(rows.size()==12)break;
    }
    return {Json{{"rows",std::move(rows)}},"",200};
}
inline std::string library_query(const std::string &path,const std::map<std::string,std::string> &filters,const std::string &sort) {
    auto result=path.substr(0,path.find('?'));std::string delimiter="?";
    if(!sort.empty()){result+="?sort="+encode(sort);delimiter="&";}
    for(const auto &f:filters){result+=delimiter+encode(f.first)+"="+encode(f.second);delimiter="&";}
    return result;
}
}
