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
    entries.push_back({image.key, texture, frame});
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
ImTextureID Artwork::avatar(const std::string &url,const std::string &token) {
    if(diagnostic(TextOnly) || diagnostic(NoDownloads))return 0;
    bool trusted=false;
    for(const char *prefix:{"https://plex.tv/","https://images.plex.tv/","https://resources-cdn.plexapp.com/"})
        if(url.rfind(prefix,0)==0)trusted=true;
    if(!trusted || url.find_first_of("\r\n#")!=std::string::npos)return 0;
    Request q{"GET",url,{},""};
    if(url.rfind("https://plex.tv/",0)==0)q.headers["X-Plex-Token"]=token;
    return request_image(q,"avatar:"+url);
}
ImTextureID Artwork::request_image(const Request &request,const std::string &key) {
    for (auto &e : entries)
        if (e.key == key) {
            e.used = frame;
            return e.texture;
        }
    if (pending.valid())
        return 0;
    auto network = transport;
    auto epoch = generation;
    bool decode=!diagnostic(NoDecode);
    checkpoint(11,"artwork schedule");
    if(home_active)home_checkpoint(10,"first Home artwork scheduled");
    pending = std::async(std::launch::async, [network, request, key, epoch, decode] {
        Decoded out;
        out.key = key;
        out.generation = epoch;
        auto reply = network(request);
        checkpoint(12,"artwork downloaded",reply.body.size());
        if(!decode) return out;
        if (reply.status != 200 || !reply.error.empty() || reply.body.empty() ||
            reply.body.size() > 4 * 1024 * 1024)
            return out;
        const auto *bytes = reinterpret_cast<const unsigned char *>(reply.body.data());
        int w, h, c;
        if (!stbi_info_from_memory(bytes, reply.body.size(), &w, &h, &c) || w <= 0 || h <= 0 ||
            w > 640 || h > 960)
            return out;
        checkpoint(13,"image decode");
        if(home_active)home_checkpoint(11,"first Home image decode");
        unsigned char *decoded = stbi_load_from_memory(bytes, reply.body.size(), &w, &h, &c, 4);
        if (decoded && w>0 && h>0 && w<=640 && h<=960) {
            out.width = w;
            out.height = h;
            out.rgba.assign(decoded, decoded + static_cast<size_t>(w) * h * 4);
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
