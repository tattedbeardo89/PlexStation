// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <algorithm>
#include <cctype>
// Selection IDs come from VLC's description list, including its disabled entry.
// Off survives late discovery, seeks, reconnects and restored player selections.
struct SubtitleLatch {
    bool off=true,explicit_choice=false;
    void choose(bool disabled){off=disabled;explicit_choice=true;}
    void begin(bool same_file,bool preference_off){if(!same_file)explicit_choice=false;if(!explicit_choice)off=preference_off;}
    template<class Tracks,class Select> bool enforce(const Tracks &tracks,int current,Select select) const {
        if(!off)return true;
        for(const auto &track:tracks)if(track.id<0) {
            return current==track.id || select(track.id);
        }
        return false; // discovery pending: never guess an ES identifier
    }
};

inline bool subtitle_language_matches(std::string actual,std::string wanted) {
    auto normalize=[](std::string value){
        std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return std::tolower(c);});
        auto end=value.find_first_of("-_");if(end!=std::string::npos)value.resize(end);
        if(value=="eng")return std::string("en");
        if(value=="spa")return std::string("es");
        if(value=="fra" || value=="fre")return std::string("fr");
        if(value=="jpn")return std::string("ja");
        if(value=="deu" || value=="ger")return std::string("de");
        if(value=="ita")return std::string("it");
        if(value=="por")return std::string("pt");
        return value;
    };
    return !wanted.empty() && normalize(actual)==normalize(wanted);
}
