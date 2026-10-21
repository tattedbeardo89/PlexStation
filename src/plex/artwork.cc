// SPDX-License-Identifier: GPL-3.0-or-later
#include "artwork.h"
#include "library.h"
#include "stb_image.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
namespace plex {
void Artwork::update() {
    ++frame;
    if (!pending.valid() ||
        pending.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        return;
    auto image = pending.get();
    if (image.generation != generation)
        return;
    ImTextureID texture = 0;
    if (!image.rgba.empty()) {
        fprintf(stderr,"art: upload begin width=%d height=%d cached=%zu\n",image.width,image.height,entries.size());
        texture = library_upload_rgba(image.rgba.data(), image.width, image.height);
        fprintf(stderr,"art: upload completed ok=%d\n",texture!=0);
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
    if (thumb.empty() || thumb.front() != '/' || thumb.rfind("//", 0) == 0 ||
        !valid_base(base, true))
        return 0;
    const std::string key = base + thumb;
    for (auto &e : entries)
        if (e.key == key) {
            e.used = frame;
            return e.texture;
        }
    if (pending.valid())
        return 0;
    auto network = transport;
    auto epoch = generation;
    pending = std::async(std::launch::async, [network, base, token, thumb, key, epoch] {
        Decoded out;
        out.key = key;
        out.generation = epoch;
        Request q{"GET",
                  base + "/photo/:/transcode?width=320&height=480&minSize=1&upscale=0&url=" +
                      encode(thumb),
                  {{"X-Plex-Token", token}},
                  ""};
        auto reply = network(q);
        if (reply.status != 200 || !reply.error.empty() || reply.body.empty() ||
            reply.body.size() > 4 * 1024 * 1024)
            return out;
        fprintf(stderr,"art: response bytes=%zu decode begin\n",reply.body.size());
        const auto *bytes = reinterpret_cast<const unsigned char *>(reply.body.data());
        int w, h, c;
        if (!stbi_info_from_memory(bytes, reply.body.size(), &w, &h, &c) || w <= 0 || h <= 0 ||
            w > 640 || h > 960)
            return out;
        unsigned char *decoded = stbi_load_from_memory(bytes, reply.body.size(), &w, &h, &c, 4);
        if (decoded && w>0 && h>0 && w<=640 && h<=960) {
            out.width = w;
            out.height = h;
            out.rgba.assign(decoded, decoded + static_cast<size_t>(w) * h * 4);
        }
        stbi_image_free(decoded);
        fprintf(stderr,"art: decode completed ok=%d width=%d height=%d\n",!out.rgba.empty(),out.width,out.height);
        return out;
    });
    return 0;
}
void Artwork::clear() {
    ++generation; // An obsolete in-flight request is discarded by update(), without blocking the
                  // UI.
    for (auto &entry : entries)
        library_free_image(entry.texture);
    entries.clear();
}
} // namespace plex
