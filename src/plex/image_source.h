// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "client.h"
#include <algorithm>
namespace plex {
inline std::string image_origin(const std::string &url){auto slash=url.find('/',url.find("://")==std::string::npos?0:url.find("://")+3);return slash==std::string::npos?url:url.substr(0,slash);}
inline bool plex_image_origin(const std::string &origin){return origin=="https://plex.tv" || origin=="https://images.plex.tv" || origin=="https://resources-cdn.plexapp.com" || origin=="https://metadata-static.plex.tv" || origin=="https://provider-static.plex.tv";}
inline bool public_image_origin(const std::string &origin){return plex_image_origin(origin) || origin=="https://image.tmdb.org" || origin=="https://artworks.thetvdb.com";}
inline bool clean_image_url(std::string &url){
    if(url.empty() || url.size()>4096 || url.find_first_of("#\\")!=std::string::npos || std::any_of(url.begin(),url.end(),[](unsigned char c){return c<=32 || c>=127;}))return false;
    auto query=url.find('?');if(query!=std::string::npos){std::string clean=url.substr(0,query);bool first=true;size_t at=query+1;
        while(at<url.size()){auto end=url.find('&',at);if(end==std::string::npos)end=url.size();auto pair=url.substr(at,end-at),key=pair.substr(0,pair.find('='));std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return std::tolower(c);});
            if(!pair.empty() && key!="token" && key!="x-plex-token" && key!="auth_token" && key!="authtoken" && key!="access_token" && key!="authorization" && key!="jwt" && key.find('%')==std::string::npos){clean+=first?'?':'&';clean+=pair;first=false;}at=end+1;}
        url=std::move(clean);
    }return !url.empty();
}
inline bool image_request(const std::string &base,const std::string &token,std::string path,bool landscape,Request &request,std::string &identity){
    if(!clean_image_url(path))return false;
    if(path.rfind("//",0)==0)path="https:"+path;
    if(path.front()=='/'){
        if(!valid_base(base,true))return false;
        identity=path;request={"GET",base+"/photo/:/transcode?width="+(landscape?"640&height=360":"320&height=480")+"&minSize=0&upscale=0&url="+encode(path),{{"X-Plex-Token",token}},""};
    }else {
        auto origin=image_origin(path);
        if(!valid_base(origin,true) || (origin!=base && !public_image_origin(origin)))return false;
        if(origin!=base && path.rfind("https://",0)!=0)return false;
        identity=path;request={"GET",path,{{"Accept","image/png,image/jpeg;q=0.9"}},""};
        if(origin==base)request.headers["X-Plex-Token"]=token;
    }
    request.connect_timeout_ms=2000;request.timeout_ms=5000;return true;
}
inline bool image_redirect(const Request &original,Request &current,std::string location){
    if(!location.empty() && location.front()=='/' && location.rfind("//",0)!=0)location=image_origin(current.url)+location;
    else if(location.rfind("//",0)==0)location="https:"+location;
    if(!clean_image_url(location))return false;
    auto next=image_origin(location),origin=image_origin(original.url);
    if(next!=origin && !public_image_origin(next))return false;
    if(current.url.rfind("https://",0)==0 && location.rfind("https://",0)!=0)return false;
    current.url=location;current.headers={{"Accept","image/png,image/jpeg;q=0.9"}};
    auto token=original.headers.find("X-Plex-Token");if(next==origin && token!=original.headers.end())current.headers["X-Plex-Token"]=token->second;
    return true;
}
}
