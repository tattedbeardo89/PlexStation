// SPDX-License-Identifier: GPL-3.0-or-later
bool cloud_checked=false,cloud_ok=false,cloud_screen=false,cloud_requested=false;
std::string cloud_error;
int cloud_start=0,cloud_filter=0,cloud_sort=0;
bool cloud_tools=false;
bool cloud_available(){return cloud_ok;}
void reset_cloud(){cloud_checked=false;cloud_ok=false;cloud_screen=false;cloud_error.clear();cloud_start=0;cloud_requested=false;}
void service_cloud(){
    if(!cloud_requested || cloud_checked || job!=None || profile_token.empty() || base.empty() || profile_screen)return;
    cloud_checked=true;
    if(active_profile.managed){cloud_error="Cloud Watchlist is unavailable for managed profiles.";return;}
    auto token=profile_token;submit(CloudProbe,[token]{return cloud_watchlist(*client,token);});
}
void show_cloud(int start){
    if(job!=None || !cloud_ok)return;
    cloud_start=std::max(0,start);auto token=profile_token;auto offset=cloud_start;auto filter=cloud_filter,sort=cloud_sort;
    submit(CloudList,[token,offset,filter,sort]{return cloud_watchlist(*client,token,offset,filter,sort);});
}
void finish_cloud(int kind,const Result &result){
    if(kind==CloudProbe){auto mc=result.data.find("MediaContainer");cloud_ok=result && mc!=result.data.end() && mc->is_object();cloud_error=cloud_ok?"":result?"Unsupported cloud response":result.error;if(cloud_ok && nav==4)show_cloud();message=cloud_ok?"Plex Cloud Watchlist is available in the sidebar":"Cloud Watchlist unavailable: "+cloud_error;return;}
    if(!result){message=result.error;return;}
    if(kind==CloudList){listing=items(result.data,false);page_path="cloud-watchlist";page_title="Watchlist";cloud_screen=true;home_screen=false;in_detail=false;sidebar=false;focus=0;page_start=cloud_start;
        auto mc=result.data.find("MediaContainer");page_total=mc==result.data.end()?listing.size():static_cast<int>(std::clamp<int64_t>(number(*mc,"totalSize",listing.size()),0,INT32_MAX));return;}
    if(kind==CloudResolve){auto found=items(result.data);if(found.empty()){message="This Watchlist item is unavailable on your selected PMS";return;}cloud_screen=false;open_media(found[0]);return;}
    message="Plex Cloud Watchlist update accepted by provider";
}
void change_cloud(bool add){
    if(job!=None || !cloud_ok)return;
    auto token=profile_token;auto item=detail;submit(CloudChange,[token,item,add]{return cloud_change(*client,token,item,add);});
}

void request_cloud_check(){cloud_requested=true;cloud_checked=false;cloud_error.clear();message="Checking Plex Cloud Watchlist access; supported access adds Watchlist to the sidebar.";}
