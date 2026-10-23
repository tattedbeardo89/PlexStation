// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "streaming.h"
#include "subtitle_latch.h"
namespace plex {
struct SubtitleStream {
    std::string id, language, title, label, external;
    bool forced=false, hearing_impaired=false;
};
inline std::vector<SubtitleStream> subtitle_streams(const Item &item,size_t version) {
    std::vector<SubtitleStream> out;
    if(!item.media.is_array() || version>=item.media.size())return out;
    auto parts=item.media[version].find("Part");
    if(parts==item.media[version].end() || !parts->is_array() || parts->size()!=1)return out;
    auto streams=(*parts)[0].find("Stream");
    if(streams==(*parts)[0].end() || !streams->is_array())return out;
    for(const auto &s:*streams) {
        if(number(s,"streamType")!=3 || out.size()>=96)continue;
        SubtitleStream track;
        track.id=text(s,"id",std::to_string(number(s,"id")));
        track.language=text(s,"languageCode",text(s,"languageTag"));
        track.title=text(s,"title");
        track.forced=flag(s,"forced");track.hearing_impaired=flag(s,"hearingImpaired");
        track.external=text(s,"key");
        track.label=text(s,"displayTitle",text(s,"language",track.language.empty()?"Unknown language":track.language));
        if(!track.title.empty() && track.label.find(track.title)==std::string::npos)track.label+=" / "+track.title;
        if(track.forced)track.label+=" [Forced]";
        if(track.hearing_impaired)track.label+=" [Hearing impaired]";
        track.label+=track.external.empty()?" [Embedded]":" [External]";
        out.push_back(std::move(track));
    }
    return out;
}
// Track IDs belong to different systems. Use a unique native language/title
// match, rejecting ambiguity. Never fall back to a numeric Plex stream ID.
template<class Tracks> int match_subtitle(const SubtitleStream &wanted,const Tracks &native) {
    int found=-1,count=0;
    for(const auto &track:native) {
        if(track.id<0 || wanted.language.empty() || !subtitle_language_matches(track.language,wanted.language))continue;
        if(!wanted.title.empty() && track.name.find(wanted.title)==std::string::npos)continue;
        found=track.id;++count;
    }
    return count==1?found:-1;
}
struct SubtitleSelection {
    // -1 Off, -2 Automatic, -3 Preferred Language, >=0 metadata stream index.
    int choice=-1;
    std::string media;
    size_t version=0;
    void begin(const std::string &id,size_t v,int default_mode) {
        if(media==id && version==v)return;
        media=id;version=v;choice=default_mode==1?-2:default_mode==2?-3:-1;
    }
    void reset(){media.clear();version=0;choice=-1;}
};
}
