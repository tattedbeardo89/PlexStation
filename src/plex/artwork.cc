// SPDX-License-Identifier: GPL-3.0-or-later
#include "artwork.h"
#include "diagnostics.h"
#include "library.h"
#include "stb_image.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
namespace plex {
void Artwork::update() {
    // Called before this frame draws any images. Previous frame commands may
    // still reference retired descriptors; library_free_image waits for the GPU.
    for(auto texture:retired)library_free_image(texture);
    retired.clear();
    ++frame;
    if (!pending.valid() ||
        pending.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        return;
    auto image = pending.get();
    if (image.generation != generation)
        return;
    ImTextureID texture = 0;
    if (!diagnostic(NoUpload) && !diagnostic(TextOnly) && !image.rgba.empty()) {
        checkpoint(14,"gpu texture create",image.rgba.size());
        texture = library_upload_rgba(image.rgba.data(), image.width, image.height);
        if(home_active)home_checkpoint(12,"first Home texture upload",texture?0:-1);
    }
    if (entries.size() >= 12) {
        auto old = std::min_element(entries.begin(), entries.end(),
                                    [](const Entry &a, const Entry &b) { return a.used < b.used; });
        library_free_image(old->texture);
        entries.erase(old);
    }
    entries.push_back({image.key, texture, frame, texture?0:frame+300});
}
ImTextureID Artwork::get(const std::string &base, const std::string &token,
                         const std::string &thumb) {
    if(diagnostic(TextOnly) || diagnostic(NoDownloads)) return 0;
    if (thumb.empty() || thumb.front() != '/' || thumb.rfind("//", 0) == 0 ||
        !valid_base(base, true))
        return 0;
    Request q{"GET",base+"/photo/:/transcode?width=320&height=480&minSize=1&upscale=0&url="+encode(thumb),{{"X-Plex-Token",token}},""};
    return request_image(q,base+thumb);
}
ImTextureID Artwork::landscape(const std::string &base,const std::string &token,const std::string &path) {
    if(diagnostic(TextOnly) || diagnostic(NoDownloads) || path.empty() || path.front()!='/' || path.rfind("//",0)==0 || !valid_base(base,true))return 0;
    Request q{"GET",base+"/photo/:/transcode?width=640&height=360&minSize=0&upscale=0&url="+encode(path),{{"X-Plex-Token",token}},""};
    return request_image(q,"landscape:"+base+path);
}
namespace {
// Restrict credentials and redirects to Plex's known HTTPS image origins.
bool avatar_url(std::string &url) {
    if(url.empty() || url.size()>4096 || url.find_first_of("\r\n#\\")!=std::string::npos)return false;
    if(url.front()=='/' && url.rfind("//",0)!=0)url="https://plex.tv"+url;
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
    auto normalized=url;
    if(!avatar_url(normalized))return 0;
    Request q{"GET",normalized,{},""};
    if(normalized.rfind("https://plex.tv/",0)==0)q.headers["X-Plex-Token"]=token;
    q.connect_timeout_ms=3000;q.timeout_ms=5000;
    return request_image(q,"avatar:"+normalized,true);
}
ImTextureID Artwork::request_image(const Request &request,const std::string &key,bool avatar_image) {
    for (auto it=entries.begin();it!=entries.end();++it)
        if (it->key == key) {
            it->used = frame;
            if(it->texture || frame<it->retry_after)return it->texture;
            entries.erase(it);break;
        }
    if (pending.valid())
        return 0;
    auto network = transport;
    auto epoch = generation;
    bool decode=!diagnostic(NoDecode);
    checkpoint(11,"artwork schedule");
    if(home_active)home_checkpoint(10,"first Home artwork scheduled");
    pending = std::async(std::launch::async, [network, request, key, epoch, decode, avatar_image] {
        Decoded out;
        out.key = key;
        out.generation = epoch;
        auto current=request;
        auto reply = network(current);
        for(int redirects=0;avatar_image && redirects<3 && reply.error.empty() &&
            (reply.status==301 || reply.status==302 || reply.status==303 || reply.status==307 || reply.status==308);++redirects) {
            auto next=reply.location;
            if(!next.empty() && next.front()=='/' && next.rfind("//",0)!=0) {
                auto slash=current.url.find('/',8);next=current.url.substr(0,slash)+next;
            }
            if(!avatar_url(next))break;
            const auto original_token=request.headers.find("X-Plex-Token");
            current.url=next;current.headers.clear();
            if(next.rfind("https://plex.tv/",0)==0 && original_token!=request.headers.end())
                current.headers["X-Plex-Token"]=original_token->second;
            reply=network(current);
        }
        if(avatar_image) {fprintf(stderr,"avatar: fetch=%s status=%d bytes=%zu\n",reply.error.empty()?"complete":"failed",reply.status,reply.body.size());fflush(stderr);}
        checkpoint(12,"artwork downloaded",reply.body.size());
        if(!decode) return out;
        if (reply.status != 200 || !reply.error.empty() || reply.body.empty() ||
            reply.body.size() > 4 * 1024 * 1024)
            return out;
        const auto *bytes = reinterpret_cast<const unsigned char *>(reply.body.data());
        int w, h, c;
        if (!stbi_info_from_memory(bytes, reply.body.size(), &w, &h, &c) || w <= 0 || h <= 0 ||
            w > (avatar_image?2048:640) || h > (avatar_image?2048:960))
            return out;
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
            if(avatar_image)fprintf(stderr,"avatar: decode=complete size=%dx%d\n",out.width,out.height);
        }
        stbi_image_free(decoded);
        return out;
    });
    return 0;
}
void Artwork::clear() {
    ++generation; // An obsolete in-flight request is discarded by update(), without blocking the
                  // UI.
    // clear() can run after an avatar was drawn earlier in the SAME UI frame.
    // Keep its descriptor/image alive until the next update, after submission.
    for(auto &entry:entries)if(entry.texture)retired.push_back(entry.texture);
    entries.clear();
}
} // namespace plex
