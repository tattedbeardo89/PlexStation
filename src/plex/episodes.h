// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "client.h"
namespace plex {
inline Result next_episode(Client &client,const std::string &base,const std::string &token,const Item &current) {
    if(current.type!="episode" || current.grandparent_id.empty())return {{},"Series identity unavailable; choose the next episode manually",0};
    bool found=false;std::string previous_first;
    for(int start=0;start<2000;start+=200) {
        auto page=client.browse(base,token,"/library/metadata/"+encode(current.grandparent_id)+"/allLeaves?sort=parentIndex:asc,index:asc",start,200);
        if(!page)return page;
        auto entries=items(page.data);if(entries.empty())break;
        if(entries.front().id==previous_first)return {{},"Server did not advance episode pagination",0};
        previous_first=entries.front().id;
        for(const auto &item:entries) {
            if(item.type!="episode")continue;
            if(found)return {Json{{"MediaContainer",{{"Metadata",Json::array({{{"ratingKey",item.id},{"type",item.type},{"title",item.title},{"Media",item.media},{"duration",item.duration},{"parentRatingKey",item.parent_id},{"grandparentRatingKey",item.grandparent_id},{"parentIndex",item.season},{"index",item.episode}}})}}}},"",200};
            if(item.id==current.id)found=true;
        }
        auto mc=page.data.find("MediaContainer");auto total=mc==page.data.end()?0:number(*mc,"totalSize",entries.size());
        if(start+static_cast<int>(entries.size())>=total)break;
    }
    return {{},"No subsequent episode in the accessible server order",0};
}
}
