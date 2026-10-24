// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "home_feed.h"
namespace plex {
inline Result music_feed(Client &c,const std::string &base,const std::string &token,const Item &section,bool books,
                         const std::vector<Item> &listening={}) {
    Json rows=Json::array();
    auto add=[&](const std::string &id,const std::string &title,const std::vector<Item> &entries){
        Json metadata=Json::array();for(const auto &i:entries){if(i.type!="artist" && i.type!="album" && i.type!="track" && i.type!="playlist")continue;
            metadata.push_back({{"ratingKey",i.id},{"type",i.type},{"title",i.title},{"thumb",i.thumb},{"art",i.art},{"year",i.year},{"duration",i.duration},{"viewOffset",i.offset},{"grandparentTitle",i.artist},{"parentTitle",i.album},{"librarySectionID",section.id},{"parentRatingKey",i.parent_id},{"parentIndex",i.disc},{"index",i.track}});if(metadata.size()==12)break;}
        if(!metadata.empty())rows.push_back({{"id",id},{"title",title},{"payload",{{"MediaContainer",{{"Metadata",std::move(metadata)}}}}}});
    };
    if(books){std::vector<Item> scoped;for(const auto &item:listening)if(item.section_id==section.id)scoped.push_back(item);add("local-listening","Continue Listening (PlexStation-local)",scoped);}
    struct Query{const char *id,*title,*path;};
    const Query queries[]={{"recent-played","Recently Played","/all?type=10&sort=lastViewedAt:desc&lastViewedAt>>=0"},{"recent-albums","Recently Added Albums","/recentlyAdded?type=9"},{"recent-artists","Artists","/all?type=8&sort=titleSort:asc"},{"favorites","Favorite Tracks","/all?type=10&userRating>=8"}};
    for(const auto &q:queries){if(books && (std::string(q.id)=="favorites" || std::string(q.id)=="recent-played"))continue;
        auto r=c.browse(base,token,"/library/sections/"+encode(section.id)+q.path,0,12);if(r)add(q.id,books && std::string(q.id)=="recent-albums"?"Recently Added Audiobooks":q.title,items(r.data,false));}
    auto playlists=c.browse(base,token,"/playlists?playlistType=audio",0,12);if(playlists)add("playlists","Audio Playlists",items(playlists.data,false));
    return {Json{{"rows",std::move(rows)}},"",200};
}
}
