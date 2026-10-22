// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "client.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <set>
namespace plex {
inline bool audio_type(const Item &i) {return i.type=="track";}
inline bool music_section(const Item &i) {return i.type=="artist";}
inline Playback audio_direct_play(const std::string &base,const std::string &token,const Item &i,size_t version=0) {
    if(!audio_type(i) || !valid_base(base,true) || !i.media.is_array() || version>=i.media.size())return {Mode::Unsupported,"","No audio version"};
    const auto &m=i.media[version];auto parts=m.find("Part");
    if(parts==m.end() || !parts->is_array() || parts->size()!=1)return {Mode::Unsupported,"","Audio requires one part per track"};
    auto key=text((*parts)[0],"key"),codec=text(m,"audioCodec"),fmt=text(m,"container");
    const std::set<std::string> codecs={"mp3","aac","flac","alac","pcm","pcm_s16le","pcm_s24le","pcm_s32le","pcm_f32le","vorbis","opus"};
    const std::set<std::string> formats={"mp3","mp4","m4a","m4b","aac","flac","wav","ogg","oga","opus"};
    if(!codecs.count(codec) || !formats.count(fmt))return {Mode::Unsupported,"","Audio codec/container not in compiled adapter capability list"};
    if(key.rfind("/library/parts/",0)!=0 || key.find_first_of("?#\r\n")!=std::string::npos)return {Mode::Unsupported,"","Invalid audio part"};
    return {Mode::DirectPlay,base+key+"?X-Plex-Token="+encode(token),""};
}
// Fetch all tracks in bounded pages. Never present a truncated album as complete.
inline Result audio_listing(Client &c,const std::string &b,const std::string &t,const std::string &path,
                            const std::function<bool()> &current=[] {return true;},bool complete=false) {
    Json metadata=Json::array();std::set<std::string> seen;
    for(int start=0;start<2000;start+=200) {
        if(!current())return {{},"Audio request cancelled",0};
        auto r=c.browse(b,t,path,start,200);if(!r)return r;
        auto list=items(r.data,true);const auto &mc=r.data["MediaContainer"];
        auto total=number(mc,"totalSize",number(mc,"size",list.size()));
        if(total>2000 && complete)return {{},"Queue exceeds safe 2000-track limit; select a smaller album/playlist",0};
        // Keep original bounded metadata including media; UI extraction remains typed.
        const char *field=mc.contains("Metadata")?"Metadata":"Directory";
        auto raw=mc.find(field);if(raw!=mc.end() && raw->is_array())for(const auto &v:*raw) {
            auto id=text(v,"ratingKey",text(v,"key"));
            // Playlist duplicates are intentional and must retain their position.
            if(metadata.size()>=2000)break;
            if(complete || seen.insert(id).second)metadata.push_back(v);
        }
        if(!complete || start+static_cast<int>(list.size())>=total || list.empty())
            return {Json{{"MediaContainer",{{"Metadata",std::move(metadata)},{"totalSize",total}}}},"",200};
        if(list.size()<200)return {{},"Server returned a partial queue; playback not started",0};
    }
    return {{},"Queue exceeds safe track limit",0};
}
inline std::vector<Item> audio_tracks(const Json &data,bool album_order=false) {
    // items() is intentionally capped per page; extract bounded chunks for complete queues.
    std::vector<Item> result;auto c=data.find("MediaContainer");if(c==data.end() || !c->is_object())return result;
    auto m=c->find("Metadata");if(m==c->end() || !m->is_array())return result;
    for(size_t at=0;at<m->size() && result.size()<2000;at+=200) {
        Json chunk=Json::array();for(size_t j=at;j<m->size() && j<at+200;++j)chunk.push_back((*m)[j]);
        auto page=items(Json{{"MediaContainer",{{"Metadata",std::move(chunk)}}}});
        for(auto &i:page)if(audio_type(i))result.push_back(std::move(i));
    }
    if(album_order)std::stable_sort(result.begin(),result.end(),[](const Item &a,const Item &b){return a.disc!=b.disc?a.disc<b.disc:a.track<b.track;});
    return result;
}
enum class Repeat {Off,All,One};
struct AudioQueue {
    std::vector<Item> entries;
    std::vector<size_t> order;
    size_t cursor=0;
    Repeat repeat=Repeat::Off;bool shuffled=false;
    const Item *current() const {return cursor<order.size() && order[cursor]<entries.size()?&entries[order[cursor]]:nullptr;}
    void clear() {entries.clear();order.clear();cursor=0;repeat=Repeat::Off;shuffled=false;}
    void assign(std::vector<Item> value) {clear();if(value.size()>2000)value.resize(2000);entries=std::move(value);order.resize(entries.size());std::iota(order.begin(),order.end(),0);}
    bool advance(bool ended=false) {
        if(order.empty())return false;
        if(ended && repeat==Repeat::One)return true;
        if(cursor+1<order.size()){++cursor;return true;}
        if(repeat==Repeat::All){cursor=0;return true;}return false;
    }
    bool previous() {if(cursor==0 || order.empty())return false;--cursor;return true;}
    bool add(const Item &i,bool next=false) {
        if(!audio_type(i) || entries.size()>=2000)return false;
        size_t id=entries.size();entries.push_back(i);
        order.insert(next && !order.empty()?order.begin()+cursor+1:order.end(),id);return true;
    }
    bool remove(size_t position) {
        if(position>=order.size() || position==cursor)return false; // stop/skip current first
        size_t removed=order[position];order.erase(order.begin()+position);entries.erase(entries.begin()+removed);
        for(auto &v:order)if(v>removed)--v;
        if(position<cursor)--cursor;
        return true;
    }
    void shuffle(uint32_t seed) {
        if(order.empty())return;
        std::mt19937 rng(seed);std::shuffle(order.begin()+cursor+1,order.end(),rng);shuffled=true;
    }
    void unshuffle() {
        auto id=current()?order[cursor]:0;std::sort(order.begin(),order.end());
        auto it=std::find(order.begin(),order.end(),id);cursor=it-order.begin();shuffled=false;
    }
};
}
