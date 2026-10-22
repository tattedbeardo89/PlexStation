// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "listening.h"
namespace plex {
struct ProgressUpdate {Item item;std::string state;int64_t position=0,updated=0,sequence=0;int attempts=0;double due=0;};
struct ProgressOutbox {
    std::vector<ProgressUpdate> entries;int64_t serial=0;
    int64_t put(const Item &item,const std::string &state,int64_t position,int64_t updated) {
        auto found=std::find_if(entries.begin(),entries.end(),[&](const ProgressUpdate &u){return u.item.id==item.id;});
        if(found==entries.end()){if(entries.size()>=128)return 0;entries.emplace_back();found=entries.end()-1;}
        *found={item,state,std::max<int64_t>(0,position),updated,++serial,0,0};found->item.media={};return serial;
    }
    void ack(const std::string &id,int64_t sequence) {
        entries.erase(std::remove_if(entries.begin(),entries.end(),[&](const ProgressUpdate &u){return u.item.id==id && u.sequence==sequence;}),entries.end());
    }
    void failed(const std::string &id,int64_t sequence,double now) {
        for(auto &u:entries)if(u.item.id==id && u.sequence==sequence){u.attempts=std::min(u.attempts+1,8);u.due=now+std::min(300,5*(1<<u.attempts));}
    }
    Json json() const {
        Json list=Json::array();for(const auto &u:entries)list.push_back({{"id",u.item.id},{"key",u.item.key},{"type",u.item.type},{"duration",u.item.duration},{"state",u.state},{"position",u.position},{"updated",u.updated},{"sequence",u.sequence}});return Json{{"updates",std::move(list)}};
    }
    void restore(const Json &value,double now) {
        entries.clear();serial=0;auto updates=value.find("updates");if(updates==value.end() || !updates->is_array())return;
        for(const auto &r:*updates) {
            Item item;item.id=text(r,"id");item.key=text(r,"key");item.type=text(r,"type");item.duration=std::max<int64_t>(0,number(r,"duration"));auto state=text(r,"state");
            if(item.id.empty() || item.id.size()>128 || item.id.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-")!=std::string::npos || (state!="playing" && state!="paused" && state!="stopped"))continue;
            put(item,state,number(r,"position"),number(r,"updated"));
            for(auto &u:entries)if(u.item.id==item.id)u.due=now+5;
            if(entries.size()==128)break;
        }
    }
};
}
