// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "client.h"
#include "image_source.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <set>
namespace plex {
inline std::string utf8_prefix(const std::string &value,size_t limit){
    if(value.size()<=limit)return value;
    while(limit && (static_cast<unsigned char>(value[limit])&0xc0)==0x80)--limit;
    return value.substr(0,limit);
}
enum class CardKind {Poster,Episode,Square,Book,Category};
inline CardKind card_kind(const Item &item,bool books=false,bool personal=false) {
    if(item.type=="category" || item.type=="audio-route" || item.type=="collection")return CardKind::Category;
    if(item.type=="episode" || personal)return CardKind::Episode;
    if(item.type=="album" || item.type=="artist" || item.type=="track" || item.type=="playlist")return books?CardKind::Book:CardKind::Square;
    return CardKind::Poster;
}
struct CardSize {float width,height;};
inline CardSize card_size(CardKind kind,bool grid=false) {
    if(kind==CardKind::Episode || kind==CardKind::Category)return {320,180};
    if(kind==CardKind::Square)return {240,240};
    return grid?CardSize{216,324}:CardSize{180,270};
}
struct CardBox {float x,y,width,height;int row;};
struct CardMeasure {CardKind kind;float x,y,width,height,focus_x,focus_y,focus_width,focus_height;bool selected;};
inline std::vector<CardBox> card_flow(const std::vector<CardKind> &kinds,float width=1440,bool grid=true) {
    std::vector<CardBox> out;float x=0,y=0,row_height=0;int row=0;
    for(auto kind:kinds){auto size=card_size(kind,grid);if(x>0 && x+size.width>width){x=0;y+=row_height+100;row_height=0;++row;}out.push_back({x,y,size.width,size.height,row});x+=size.width+24;row_height=std::max(row_height,size.height);}
    return out;
}
inline int grid_neighbor(const std::vector<CardBox> &boxes,int selected,int direction) {
    if(boxes.empty())return 0;
    selected=std::clamp(selected,0,static_cast<int>(boxes.size())-1);
    int target=boxes[selected].row+direction,best=selected;float distance=1e9;
    for(size_t i=0;i<boxes.size();++i)if(boxes[i].row==target){float d=std::abs((boxes[i].x+boxes[i].width/2)-(boxes[selected].x+boxes[selected].width/2));if(d<distance){distance=d;best=i;}}
    return best;
}
inline float rail_offset(const std::vector<CardKind> &kinds,int selected,float prior,float viewport=1440) {
    float left=0,total=0,right=0;for(size_t i=0;i<kinds.size();++i){auto size=card_size(kinds[i]);if(static_cast<int>(i)==selected){left=total;right=left+size.width;}total+=size.width+24;}
    float result=prior;if(left<result)result=left;if(right>result+viewport)result=right-viewport;
    return std::clamp(result,0.f,std::max(0.f,total-24-viewport));
}
inline std::vector<std::string> image_candidates(const Item &item,bool landscape=false) {
    std::vector<std::string> result;auto add=[&](const std::string &path){if(!path.empty() && std::find(result.begin(),result.end(),path)==result.end())result.push_back(path);};
    if(item.type=="category" || item.type=="collection"){add(item.art);add(item.parent_art);add(item.grandparent_art);add(item.thumb);add(item.parent_thumb);add(item.grandparent_thumb);}
    else if(item.type=="episode" || landscape){
        if(item.thumb!=item.parent_thumb && item.thumb!=item.grandparent_thumb)add(item.thumb);
        add(item.art);add(item.parent_art);add(item.grandparent_art);add(item.parent_thumb);add(item.grandparent_thumb);add(item.thumb);
    }else {add(item.thumb);add(item.parent_thumb);add(item.grandparent_thumb);add(item.art);add(item.parent_art);add(item.grandparent_art);}
    return result;
}
inline std::string initials(const std::string &name) {
    std::string out;bool word=true;for(unsigned char c:name){if(c==' ' || c=='-' || c=='_'){word=true;continue;}if(word && c<128 && std::isalnum(c)){out+=static_cast<char>(std::toupper(c));if(out.size()==2)break;}word=false;}return out.empty()?"?":out;
}
inline std::string compact_key(std::string value){std::string out;for(unsigned char c:value)if(std::isalnum(c))out+=static_cast<char>(std::tolower(c));return out;}
inline std::string category_art(const std::string &title){
    auto key=compact_key(title);for(const char *prefix:{"genre","collection"})if(key.rfind(prefix,0)==0)key=key.substr(std::string(prefix).size());
    for(const char *name:{"action","adventure","animation","comedy","crime","documentary","drama","family","fantasy","horror","mystery","romance","thriller","western"})if(key==name)return name;
    return key=="scifi" || key=="sciencefiction"?"science-fiction":"other";
}
inline bool release_date(const std::string &date) {
    if(date.size()!=10 || date[4]!='-' || date[7]!='-')return false;
    for(size_t i=0;i<date.size();++i)if(i!=4 && i!=7 && !std::isdigit(static_cast<unsigned char>(date[i])))return false;
    int year=std::stoi(date.substr(0,4)),month=std::stoi(date.substr(5,2)),day=std::stoi(date.substr(8,2));
    if(year<1800 || month<1 || month>12 || day<1)return false;
    const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};return day<=days[month-1]+(month==2 && year%4==0 && (year%100!=0 || year%400==0));
}
inline bool released_date(const std::string &date){
    if(!release_date(date))return false;
    auto now=std::time(nullptr);std::tm utc{};char today[11]="";if(!gmtime_r(&now,&utc) || !std::strftime(today,sizeof today,"%Y-%m-%d",&utc))return false;
    return date<=today;
}
inline std::string safe_image_identity(std::string value){return clean_image_url(value)?value:std::string();}
inline Json item_json(const Item &i) {
    return {{"ratingKey",i.id},{"guid",i.guid},{"key",i.key},{"librarySectionID",i.section_id},{"type",i.type},{"title",i.title},{"summary",i.summary},{"thumb",safe_image_identity(i.thumb)},{"art",safe_image_identity(i.art)},{"parentThumb",safe_image_identity(i.parent_thumb)},{"grandparentThumb",safe_image_identity(i.grandparent_thumb)},{"parentArt",safe_image_identity(i.parent_art)},{"grandparentArt",safe_image_identity(i.grandparent_art)},{"year",i.year},{"duration",i.duration},{"viewOffset",i.offset},{"viewCount",i.watched?1:0},{"parentIndex",i.season},{"index",i.episode},{"grandparentTitle",i.series},{"originallyAvailableAt",i.released},{"addedAt",i.added}};
}
inline std::string hub_semantic(const Json &hub) {
    auto id=compact_key(text(hub,"hubIdentifier",text(hub,"key")))+compact_key(text(hub,"title")),type=text(hub,"type");
    if(type.empty()){auto entries=items(Json{{"MediaContainer",hub}},false);if(!entries.empty())type=entries.front().type;}
    if(id.find("ondeck")!=std::string::npos)return "onDeck";
    if(id.find("continuewatching")!=std::string::npos || id.find("inprogress")!=std::string::npos)return "continueWatching";
    if(id.find("recentlyadded")!=std::string::npos){if(type=="movie")return "recentlyAddedMovies";if(type=="show")return "recentlyAddedTV";if(type=="episode")return "recentlyAddedEpisodes";}
    if(id.find("recentlyreleased")!=std::string::npos || id.find("recentlyaired")!=std::string::npos){if(type=="movie")return "recentlyReleasedMovies";if(type=="episode")return "recentlyReleasedTV";}
    if(type=="collection" || id.find("collection")!=std::string::npos)return "collections";
    return {};
}
inline std::string semantic_title(const std::string &id,const std::string &fallback) {
    if(id=="recentlyAddedMovies")return "Recently Added Movies";
    if(id=="recentlyReleasedMovies")return "Recently Released Movies";
    if(id=="recentlyAddedTV")return "Recently Added TV Shows";
    if(id=="recentlyReleasedTV")return "Recently Released Episodes";
    if(id=="recentlyAddedEpisodes")return "Recently Added Episodes";
    if(id=="continueWatching")return "Continue Watching";
    return fallback;
}
}
