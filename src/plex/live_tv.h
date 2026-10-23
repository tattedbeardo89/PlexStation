// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "home.h"
#include <set>
namespace plex {
inline std::string live_label(std::string value){if(value.size()>256)value.resize(256);for(char &c:value)if(static_cast<unsigned char>(c)<32)c=' ';return value;}
inline bool live_identifier(const std::string &id){return !id.empty() && id.size()<=128 && id.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-")==std::string::npos;}
inline std::string live_provider(const std::string &id) {
    for(const char *kind:{"cloud","xmltv","custom"}) {
        auto prefix=std::string("tv.plex.providers.epg.")+kind+":";
        if(id.rfind(prefix,0)==0){auto suffix=id.substr(prefix.size());if(!suffix.empty() && suffix.size()<=16 && suffix.find_first_not_of("0123456789")==std::string::npos)return "/"+id;}
    }
    return {};
}
inline Result live_channels(Client &client,const std::string &base,const std::string &token) {
    auto dvrs=client.call("GET",base,"/livetv/dvrs",token);if(!dvrs)return dvrs;
    auto providers=client.call("GET",base,"/media/providers",token);if(!providers)return providers;
    std::set<std::string> authorized;
    auto mc=dvrs.data.find("MediaContainer");
    if(mc!=dvrs.data.end() && mc->is_object())for(const char *name:{"DVR","Dvr"}) {
        auto list=mc->find(name);if(list!=mc->end() && list->is_array())for(const auto &dvr:*list){if(!dvr.contains("key"))continue;auto key=text(dvr,"key",std::to_string(number(dvr,"key")));if(live_identifier(key))authorized.insert(key);}
    }
    Json channels=Json::array();mc=providers.data.find("MediaContainer");
    if(mc!=providers.data.end() && mc->is_object()) {
        auto list=mc->find("MediaProvider");int queried=0;
        if(list!=mc->end() && list->is_array())for(const auto &provider:*list) {
            auto id=text(provider,"identifier"),path=live_provider(id);if(path.empty())continue;
            auto dvr=id.substr(id.rfind(':')+1);if(!authorized.count(dvr) || ++queried>4)continue;
            auto response=client.call("GET",base,path+"/lineups/dvr/channels?X-Plex-Container-Size=200",token);
            if(!response)continue;
            auto container=response.data.find("MediaContainer");if(container==response.data.end())continue;
            auto entries=container->find("Channel");if(entries==container->end() || !entries->is_array())continue;
            for(const auto &channel:*entries) {
                if(!channel.contains("id"))continue;
                auto key=text(channel,"id",std::to_string(number(channel,"id"))),grid=text(channel,"gridKey");
                if(!live_identifier(key) || !live_identifier(grid) || channels.size()>=200)continue;
                channels.push_back({{"id",key},{"gridKey",grid},{"provider",id},{"dvr",dvr},{"title",live_label(text(channel,"title",text(channel,"callSign","Channel")))},{"number",text(channel,"vcn")},{"thumb",text(channel,"thumb")}});
            }
        }
    }
    return {Json{{"channels",std::move(channels)}},"",200};
}
inline Result live_guide(Client &client,const std::string &base,const std::string &token,const Json &channel,const std::string &date) {
    auto provider=live_provider(text(channel,"provider")),grid=text(channel,"gridKey");
    if(provider.empty() || !live_identifier(grid) || date.size()!=10 || date.find_first_not_of("0123456789-")!=std::string::npos)return {{},"Invalid authorized guide selection",0};
    auto result=client.call("GET",base,provider+"/grid?channelGridKey="+encode(grid)+"&date="+encode(date),token);if(!result)return result;
    Json programs=Json::array();auto mc=result.data.find("MediaContainer");
    if(mc!=result.data.end() && mc->is_object()) {
        auto metadata=mc->find("Metadata");if(metadata!=mc->end() && metadata->is_array())for(const auto &item:*metadata) {
            auto media=item.find("Media");if(media==item.end() || !media->is_array())continue;
            for(const auto &airing:*media) {
                auto begins=number(airing,"beginsAt"),ends=number(airing,"endsAt");if(begins<=0 || ends<=begins || programs.size()>=100)continue;
                programs.push_back({{"title",live_label(text(item,"title","Program"))},{"series",live_label(text(item,"grandparentTitle"))},{"beginsAt",begins},{"endsAt",ends}});
            }
        }
    }
    std::sort(programs.begin(),programs.end(),[](const Json &a,const Json &b){return number(a,"beginsAt")<number(b,"beginsAt");});
    return {Json{{"programs",std::move(programs)}},"",200};
}
inline Result live_tune(Client &client,const std::string &base,const std::string &token,const Json &channel,const std::string &consumer) {
    auto dvr=text(channel,"dvr"),id=text(channel,"id");
    if(!live_identifier(dvr) || !live_identifier(id) || !live_identifier(consumer))return {{},"Invalid authorized channel",0};
    auto result=client.call("POST",base,"/livetv/dvrs/"+encode(dvr)+"/channels/"+encode(id)+"/tune",token);if(!result)return result;
    auto mc=result.data.find("MediaContainer");if(mc==result.data.end())return {{},"No Live TV session returned",0};
    auto entries=mc->find("Metadata");if(entries==mc->end() || !entries->is_array() || entries->empty())return {{},"No Live TV media returned",0};
    const auto &item=(*entries)[0];auto media=item.find("Media");if(media==item.end() || !media->is_array() || media->empty())return {{},"No Live TV stream returned",0};
    const auto &version=(*media)[0];
    if(flag(item,"drm") || flag(version,"drm") || flag(version,"protected"))return {{},"Protected Live TV is unsupported",0};
    auto session=text(version,"uuid");if(!live_identifier(session))return {{},"Missing Live TV session identity",0};
    bool supported=false;auto parts=version.find("Part");if(parts!=version.end() && parts->is_array())for(const auto &part:*parts){auto streams=part.find("Stream");if(streams!=part.end() && streams->is_array())for(const auto &stream:*streams)if(number(stream,"streamType")==1){auto codec=text(stream,"codec");supported=codec=="h264" || codec=="hevc";}}
    if(!supported)return {{},"Live TV output codec is not supported by this adapter",0};
    return {Json{{"url",base+"/livetv/sessions/"+encode(session)+"/"+encode(consumer)+"/index.m3u8?X-Plex-Token="+encode(token)},{"session",session}},"",200};
}
}
