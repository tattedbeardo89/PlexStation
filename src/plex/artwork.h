// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "client.h"
#include "imgui.h"
#include <future>
#include <vector>
namespace plex {
class Artwork {
    struct Decoded {
        std::string key;
        int width = 0, height = 0;
        uint64_t generation = 0;
        std::vector<unsigned char> rgba;
    };
    struct Entry {
        std::string key;
        ImTextureID texture = 0;
        uint64_t used = 0;
        uint64_t retry_after = 0;
        int width=0,height=0;
    };
    std::future<Decoded> pending;
    std::vector<Entry> entries;
    std::vector<ImTextureID> retired;
    uint64_t frame = 0, generation = 0;
    Transport transport;
    ImTextureID request_image(const Request &request,const std::string &key,bool avatar_image=false);

  public:
    explicit Artwork(Transport t) : transport(std::move(t)) {}
    bool busy() const {return pending.valid();}
    size_t cached() const {return entries.size();}
    float aspect(ImTextureID texture) const {
        for(const auto &entry:entries)
            if(texture && entry.texture==texture && entry.width>0 && entry.height>0)
                return static_cast<float>(entry.width)/entry.height;
        return 0;
    }
    void update();
    ImTextureID get(const std::string &base, const std::string &token, const std::string &thumb);
    ImTextureID landscape(const std::string &base,const std::string &token,const std::string &path);
    ImTextureID avatar(const std::string &url,const std::string &primary_token);
    void clear();
};
} // namespace plex
