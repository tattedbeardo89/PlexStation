// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "client.h"
#include <algorithm>
namespace plex {
struct Profile {
    std::string id, uuid, name, avatar;
    bool protected_user=true, admin=false, managed=false;
};
inline bool flag(const Json &j,const char *key,bool fallback=false) {
    auto i=j.find(key); if(i==j.end())return fallback;
    if(i->is_boolean())return i->get<bool>();
    if(i->is_number_integer())return number(j,key,0)!=0;
    if(i->is_string()) {if(*i=="1" || *i=="true")return true; if(*i=="0" || *i=="false")return false;}
    return fallback;
}
inline std::string profile_id(const Json &j) {
    auto id=text(j,"id"); if(id.empty() && number(j,"id",0)>0)id=std::to_string(number(j,"id"));
    if(id.empty() || id.size()>32 || id.find_first_not_of("0123456789")!=std::string::npos) return "";
    return id;
}
inline std::vector<Profile> profiles(const Json &j) {
    const Json *list=&j;
    if(j.is_object()) {auto it=j.find("users"); if(it==j.end())return {}; list=&*it;}
    if(!list->is_array())return {};
    std::vector<Profile> out;
    for(const auto &v:*list) {
        auto id=profile_id(v); if(id.empty())continue;
        if(std::any_of(out.begin(),out.end(),[&](const Profile &p){return p.id==id;}))continue;
        out.push_back({id,text(v,"uuid"),text(v,"title",text(v,"username","Plex user")),text(v,"thumb"),flag(v,"protected",true),flag(v,"admin"),flag(v,"restricted")});
        if(out.size()==32)break;
    }
    return out;
}
inline bool valid_pin(const std::string &pin) {return pin.size()==4 && pin.find_first_not_of("0123456789")==std::string::npos;}
inline std::string profile_scope(const std::string &primary,const Profile &profile) {
    return primary+"/profile:"+(profile.uuid.empty() ? profile.id : profile.uuid);
}
}
