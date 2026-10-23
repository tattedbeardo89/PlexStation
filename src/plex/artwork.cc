// SPDX-License-Identifier: GPL-3.0-or-later
#include "artwork.h"
#include "diagnostics.h"
#include "library.h"
#include "stb_image.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <set>
#include <mutex>
namespace plex {
namespace {
std::atomic<unsigned> avatar_reports{0};
void avatar_checkpoint(const char *phase,int status,size_t count) {
    static std::mutex lock;static std::set<std::string> reported;
    std::lock_guard<std::mutex> guard(lock);
    if(reported.size()>=128 || !reported.insert(std::string(phase)+":"+std::to_string(status)).second)return;
    if(avatar_reports.fetch_add(1,std::memory_order_relaxed)>=128)return;
    fprintf(stderr,"avatar: phase=%s status=%d count=%zu\n",phase,status,count);fflush(stderr);
}
}
void Artwork::update() {
    // Called before this frame draws any images. Previous frame commands may
    // still reference retired descriptors; library_free_image waits for the GPU.
    for(auto texture:retired)library_free_image(texture);
    retired.clear();
    ++frame;
    if((clear_target || trim_disk_pending) && !pending.valid() && !ready) {
        bool erase=clear_target.has_value();auto cache=erase?*clear_target:disk;auto state=stats;
        clear_target.reset();trim_disk_pending=false;
        pending=std::async(std::launch::async,[cache,state,erase]{if(erase)cache.clear_scope();else cache.trim();state->bytes=cache.usage();return Decoded{};});
    }
    if(!ready) {
        if(!pending.valid() || pending.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
        ready=pending.get();
    }
    if(ready->generation!=generation || ready->key.empty()){ready.reset();return;}
    auto bytes=ready->rgba.size();
    while(entries.size()>=64 || gpu_bytes+bytes>gpu_budget) {
        auto old=entries.end();
        for(auto it=entries.begin();it!=entries.end();++it) {
            if(it->used+1>=frame || (it->texture && it->key.rfind("avatar:",0)==0 && it->used+600>=frame))continue; // visible in the previous/current frame
            if(old==entries.end() || (old->key.rfind("avatar:",0)==0 && it->key.rfind("avatar:",0)!=0) ||
               ((old->key.rfind("avatar:",0)==0)==(it->key.rfind("avatar:",0)==0) && it->used<old->used))old=it;
        }
        if(old==entries.end())return; // retain decoded result until a safe victim exists
        if(old->texture){gpu_bytes-=static_cast<size_t>(old->width)*old->height*4;library_free_image(old->texture);}
        ++stats->evictions;entries.erase(old);
    }
    auto image=std::move(*ready);ready.reset();
    ImTextureID texture = 0;
    if (!diagnostic(NoUpload) && !diagnostic(TextOnly) && !image.rgba.empty()) {
        checkpoint(14,"gpu texture create",image.rgba.size());
        texture = library_upload_rgba(image.rgba.data(), image.width, image.height);
        if(home_active)home_checkpoint(12,"first Home texture upload",texture?0:-1);
    }
    if(texture)gpu_bytes+=image.rgba.size();
    else if(!image.rgba.empty()){++stats->upload_failures;image.failure="GPU upload failed";}
    if(image.key.rfind("avatar:",0)==0)fprintf(stderr,"avatar: request=%u stage=gpu-upload result=%s bytes=%zu\n",image.request_id,texture?"ready":"failed",image.rgba.size());
    if(image.key.rfind("avatar:",0)==0)avatar_checkpoint(texture?"gpu-cache-ready":"gpu-cache-unavailable",texture?0:-1,image.rgba.size());
    entries.push_back({image.key, texture, frame, texture?0:frame+300,image.width,image.height,image.failure,image.request_id});
}
ImTextureID Artwork::get(const std::string &base, const std::string &token,
                         const std::string &thumb) {
    if(diagnostic(TextOnly) || diagnostic(NoDownloads)) return 0;
    if (thumb.empty() || thumb.front() != '/' || thumb.rfind("//", 0) == 0 ||
        !valid_base(base, true))
        return 0;
    Request q{"GET",base+"/photo/:/transcode?width=320&height=480&minSize=1&upscale=0&url="+encode(thumb),{{"X-Plex-Token",token}},""};
    return request_image(q,"poster:"+(disk_scope.empty()?base:std::string())+thumb);
}
ImTextureID Artwork::landscape(const std::string &base,const std::string &token,const std::string &path) {
    if(diagnostic(TextOnly) || diagnostic(NoDownloads) || path.empty() || path.front()!='/' || path.rfind("//",0)==0 || !valid_base(base,true))return 0;
    Request q{"GET",base+"/photo/:/transcode?width=640&height=360&minSize=0&upscale=0&url="+encode(path),{{"X-Plex-Token",token}},""};
    return request_image(q,"landscape:"+(disk_scope.empty()?base:std::string())+path);
}
namespace {
// Restrict credentials and redirects to Plex's known HTTPS image origins.
bool avatar_url(std::string &url) {
    if(url.empty() || url.size()>4096 || url.find_first_of("#\\")!=std::string::npos ||
       std::any_of(url.begin(),url.end(),[](unsigned char c){return c<=32 || c>=127;}))return false;
    if(url.rfind("//",0)==0)url="https:"+url;
    else if(url.front()=='/')url="https://plex.tv"+url;
    else if(url.rfind("http://plex.tv/",0)==0 || url.rfind("http://images.plex.tv/",0)==0 || url.rfind("http://resources-cdn.plexapp.com/",0)==0)url.replace(0,7,"https://");
    bool allowed=false;
    for(const char *prefix:{"https://plex.tv/","https://images.plex.tv/","https://resources-cdn.plexapp.com/"})
        if(url.rfind(prefix,0)==0)allowed=true;
    // Authentication is carried by headers only, never propagated in CDN query strings.
    auto query=url.find('?');
    if(query!=std::string::npos) {
        std::string safe=url.substr(0,query);bool first=true;
        size_t at=query+1;
        while(at<url.size()) {
            auto end=url.find('&',at);if(end==std::string::npos)end=url.size();
            auto pair=url.substr(at,end-at),key=pair.substr(0,pair.find('='));
            std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return std::tolower(c);});
            if(key!="x-plex-token" && key!="token" && key.find('%')==std::string::npos) {safe+=first?'?':'&';safe+=pair;first=false;}
            at=end+1;
        }
        url=std::move(safe);
    }
    return allowed;
}
}
ImTextureID Artwork::avatar(const std::string &url,const std::string &token) {
    if(diagnostic(TextOnly) || diagnostic(NoDownloads))return 0;
    if(url.empty()){avatar_checkpoint("source-missing",0,0);return 0;}
    auto normalized=url;
    if(!avatar_url(normalized)){avatar_checkpoint("url-rejected",0,0);return 0;}
    avatar_checkpoint(normalized.rfind("https://plex.tv/",0)==0?"host-plex-auth":normalized.rfind("https://images.plex.tv/",0)==0?"host-plex-image":"host-plex-cdn",0,0);
    Request q{"GET",normalized,{{"Accept","image/png,image/jpeg;q=0.9"}},""};
    if(normalized.rfind("https://plex.tv/",0)==0)q.headers["X-Plex-Token"]=token;
    q.connect_timeout_ms=3000;q.timeout_ms=5000;
    return request_image(q,"avatar:"+normalized,true);
}
ImTextureID Artwork::request_image(const Request &request,const std::string &key,bool avatar_image) {
    for (auto it=entries.begin();it!=entries.end();++it)
        if (it->key == key) {
            it->used = frame;
            if(it->texture)++stats->gpu_hits;
            if(it->texture || frame<it->retry_after){if(avatar_image)avatar_checkpoint(it->texture?"display-ready":"display-unavailable",0,0);return it->texture;}
            entries.erase(it);break;
        }
    if (pending.valid() || ready)
        return 0;
    ++stats->gpu_misses;pending_key=key;
    auto network = transport;
    auto epoch = generation;
    auto cache=avatar_image?avatar_disk:disk;auto state=stats;
    bool decode=!diagnostic(NoDecode);
    checkpoint(11,"artwork schedule");
    if(home_active)home_checkpoint(10,"first Home artwork scheduled");
    pending = std::async(std::launch::async, [network, request, key, epoch, decode, avatar_image,cache,state] {
        Decoded out;
        static std::atomic<unsigned> serial{0};out.request_id=++serial;
        out.key = key;
        out.generation = epoch;
        auto current=request;int redirect_count=0;
        auto cached=cache.get(key,4u*1024*1024);
        if(!cached.empty()) {
            int width=0,height=0,channels=0;
            auto *data=reinterpret_cast<const unsigned char *>(cached.data());
            bool valid=stbi_info_from_memory(data,static_cast<int>(cached.size()),&width,&height,&channels) && width>0 && height>0 && width<=(avatar_image?2048:640) && height<=(avatar_image?2048:960);
            unsigned char *checked=valid?stbi_load_from_memory(data,static_cast<int>(cached.size()),&width,&height,&channels,4):nullptr;
            if(!checked){++state->decode_failures;cache.erase(key);cached.clear();}
            stbi_image_free(checked);
        }
        const bool disk_hit=!cached.empty();
        if(disk_hit)++state->hits;else ++state->misses;
        if(!disk_hit)++state->downloads;
        auto reply=disk_hit?Reply{200,std::move(cached),""}:network(current);
        for(int redirects=0;avatar_image && redirects<3 && reply.error.empty() &&
            (reply.status==301 || reply.status==302 || reply.status==303 || reply.status==307 || reply.status==308);++redirects) {
            auto next=reply.location;
            if(!next.empty() && next.front()=='/' && next.rfind("//",0)!=0) {
                auto slash=current.url.find('/',8);next=current.url.substr(0,slash)+next;
            }
            if(!avatar_url(next)){avatar_checkpoint("redirect-rejected",reply.status,0);break;}
            const auto original_token=request.headers.find("X-Plex-Token");
            ++redirect_count;current.url=next;current.headers.clear();
            current.headers["Accept"]="image/png,image/jpeg;q=0.9";
            if(next.rfind("https://plex.tv/",0)==0 && original_token!=request.headers.end())
                current.headers["X-Plex-Token"]=original_token->second;
            ++state->downloads;reply=network(current);
        }
        if(avatar_image) {
            fprintf(stderr,"avatar: request=%u stage=fetch http=%d tls=%s redirects=%d bytes=%zu cache=%s format=%s\n",out.request_id,reply.status,disk_hit?"cached":reply.tls_verified?"verified":"unverified",redirect_count,reply.body.size(),disk_hit?"hit":"miss",reply.content_type.rfind("image/png",0)==0?"png":reply.content_type.rfind("image/jpeg",0)==0?"jpeg":reply.content_type.rfind("image/webp",0)==0?"webp-unsupported":"other");
            avatar_checkpoint("redirect-count",redirect_count,0);
            avatar_checkpoint(disk_hit?"tls-cached":reply.tls_verified?"tls-verified":"tls-not-verified",static_cast<int>(reply.failure),reply.body.size());
            const char *type=reply.content_type.rfind("image/png",0)==0?"type-png":reply.content_type.rfind("image/jpeg",0)==0?"type-jpeg":reply.content_type.rfind("image/webp",0)==0?"type-webp-unsupported":reply.content_type.rfind("image/svg",0)==0?"type-svg-unsupported":disk_hit?"type-cached":"type-other-or-missing";
            avatar_checkpoint(type,reply.status,reply.body.size());
        }
        if(avatar_image)avatar_checkpoint(reply.error.empty()?"fetch-complete":"fetch-failed",reply.status,reply.body.size());
        checkpoint(12,"artwork downloaded",reply.body.size());
        if(!decode) return out;
        if (reply.status != 200 || !reply.error.empty() || reply.body.empty() ||
            reply.body.size() > 4 * 1024 * 1024) {
            if(avatar_image)avatar_checkpoint("http-or-size-rejected",reply.status,reply.body.size());
            out.failure="HTTP "+std::to_string(reply.status)+" / transport "+std::to_string(static_cast<int>(reply.failure));
            return out;
        }
        const auto *bytes = reinterpret_cast<const unsigned char *>(reply.body.data());
        int w, h, c;
        if (!stbi_info_from_memory(bytes, reply.body.size(), &w, &h, &c) || w <= 0 || h <= 0 ||
            w > (avatar_image?2048:640) || h > (avatar_image?2048:960)) {
            ++state->decode_failures;out.failure="Unsupported image format or dimensions";
            if(avatar_image)avatar_checkpoint("header-or-dimensions-rejected",0,reply.body.size());
            return out;
        }
        checkpoint(13,"image decode");
        unsigned char *decoded = stbi_load_from_memory(bytes, reply.body.size(), &w, &h, &c, 4);
        if (decoded && w>0 && h>0 && w<=(avatar_image?2048:640) && h<=(avatar_image?2048:960)) {
            if(home_active)home_checkpoint(11,"first Home image decoded");
            if(avatar_image) {
                // Center-crop and bound retained RGBA to 256 square (256 KiB).
                const int side=std::min(256,std::min(w,h)),crop=std::min(w,h);
                out.width=out.height=side;out.rgba.resize(static_cast<size_t>(side)*side*4);
                for(int y=0;y<side;++y)for(int x=0;x<side;++x) {
                    int sx=(w-crop)/2+x*crop/side,sy=(h-crop)/2+y*crop/side;
                    std::copy_n(decoded+(static_cast<size_t>(sy)*w+sx)*4,4,out.rgba.data()+(static_cast<size_t>(y)*side+x)*4);
                }
            } else {
                out.width = w;out.height = h;
                out.rgba.assign(decoded, decoded + static_cast<size_t>(w) * h * 4);
            }
            if(avatar_image)avatar_checkpoint("decode-dimensions",out.width,static_cast<size_t>(out.height));
        }
        if(out.rgba.empty()){++state->decode_failures;out.failure="Image decoding failed";cache.erase(key);}
        else if(!disk_hit && !cache.put(key,reply.body)){++state->write_failures;if(avatar_image)avatar_checkpoint("disk-write-failed",0,reply.body.size());}
        if(avatar_image)state->avatar_bytes=cache.usage();else state->bytes=cache.usage();
        if(avatar_image)fprintf(stderr,"avatar: request=%u stage=decode result=%s bytes=%zu\n",out.request_id,out.rgba.empty()?"failed":"ready",out.rgba.size());
        if(avatar_image)avatar_checkpoint(disk_hit?"disk-hit":"disk-miss",0,out.rgba.size());
        if(avatar_image && out.rgba.empty())avatar_checkpoint("decode-failed",0,0);
        stbi_image_free(decoded);
        return out;
    });
    return 0;
}
std::string Artwork::diagnostics() const {
    return "Disk H/M "+std::to_string(stats->hits.load())+"/"+std::to_string(stats->misses.load())+
        " GPU H/M "+std::to_string(stats->gpu_hits.load())+"/"+std::to_string(stats->gpu_misses.load())+
        " downloads "+std::to_string(stats->downloads.load())+" decode failures "+std::to_string(stats->decode_failures.load())+
        " evictions "+std::to_string(stats->evictions.load())+" writes failed "+std::to_string(stats->write_failures.load())+
        " uploads failed "+std::to_string(stats->upload_failures.load());
}
std::string Artwork::avatar_state(const std::string &url) const {
    auto normalized=url;
    if(url.empty())return "No profile image provided";
    if(!avatar_url(normalized))return "Unsupported avatar origin";
    auto key="avatar:"+normalized;
    for(const auto &entry:entries)if(entry.key==key)return entry.texture?"Ready":entry.failure.empty()?"Avatar unavailable":entry.failure+" (#"+std::to_string(entry.request_id)+")";
    return pending_key==key && busy()?"Loading avatar…":"Waiting for avatar…";
}
void Artwork::retry_avatars() {
    // Retain healthy posters and avatars. Retry failures without disk destruction.
    for(auto it=entries.begin();it!=entries.end();) {
        if(!it->texture && it->key.rfind("avatar:",0)==0)it=entries.erase(it);else ++it;
    }
}
void Artwork::set_scope(const std::string &root,const std::string &scope,size_t max_bytes) {
    if(scope!=disk_scope){clear();disk_scope=scope;stats=std::make_shared<Stats>();}
    disk=DiskCache(root,scope,max_bytes);trim_disk_pending=true;
}
void Artwork::clear() {
    ready.reset();pending_key.clear();gpu_bytes=0;
    ++generation; // An obsolete in-flight request is discarded by update(), without blocking the
                  // UI.
    // clear() can run after an avatar was drawn earlier in the SAME UI frame.
    // Keep its descriptor/image alive until the next update, after submission.
    for(auto &entry:entries)if(entry.texture)retired.push_back(entry.texture);
    entries.clear();
}
} // namespace plex
