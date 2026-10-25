// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "client.h"
#include "cache.h"
#include <optional>
#include <memory>
#include "imgui.h"
#include <future>
#include <vector>
namespace plex {
class Artwork {
    struct Decoded {
        std::string key;
        int width = 0, height = 0;
        uint64_t generation = 0;
        std::string failure;
        unsigned request_id=0,attempt=1;
        std::vector<unsigned char> rgba;
    };
    struct Entry {
        std::string key;
        ImTextureID texture = 0;
        uint64_t used = 0;
        uint64_t retry_after = 0;
        int width=0,height=0;
        std::string failure;
        unsigned request_id=0,attempt=1;
    };
    std::future<Decoded> pending,media_pending,avatar_pending;
    std::optional<Decoded> ready;
    DiskCache disk,avatar_disk;
    std::string pending_key,media_pending_key,avatar_pending_key;
    std::string disk_scope;
    struct Stats {std::atomic<size_t> bytes{0},avatar_bytes{0},hits{0},misses{0},gpu_hits{0},gpu_misses{0},downloads{0},decode_failures{0},evictions{0},write_failures{0},upload_failures{0};};
    std::shared_ptr<Stats> stats=std::make_shared<Stats>();
    std::optional<DiskCache> clear_target;
    bool trim_disk_pending=false;
    size_t gpu_bytes=0;
    static constexpr size_t gpu_budget=64u*1024*1024;
    std::vector<Entry> entries;
    std::vector<ImTextureID> retired;
    uint64_t frame = 0, generation = 0;
    Transport transport;
    ImTextureID request_image(const Request &request,const std::string &key,bool avatar_image=false);

  public:
    explicit Artwork(Transport t) : transport(std::move(t)) {}
    bool busy() const {return pending.valid() || media_pending.valid() || avatar_pending.valid() || ready.has_value();}
    size_t cached() const {return entries.size();}
    float aspect(ImTextureID texture) const {
        for(const auto &entry:entries)
            if(texture && entry.texture==texture && entry.width>0 && entry.height>0)
                return static_cast<float>(entry.width)/entry.height;
        return 0;
    }
    void set_scope(const std::string &root,const std::string &scope,size_t max_bytes=256u*1024*1024);
    size_t disk_usage() const {return stats->bytes.load()+stats->avatar_bytes.load();}
    size_t disk_hits() const {return stats->hits.load();}
    size_t memory_usage() const {return gpu_bytes;}
    std::string diagnostics() const;
    std::string avatar_state(const std::string &url) const;
    void retry_avatars();
    void set_avatar_scope(const std::string &root,const std::string &scope) {avatar_disk=DiskCache(root,scope,16u*1024*1024);}
    void clear_disk() {clear_target=disk;}
    void update();
    ImTextureID get(const std::string &base, const std::string &token, const std::string &thumb);
    ImTextureID landscape(const std::string &base,const std::string &token,const std::string &path);
    ImTextureID media(const std::string &base,const std::string &token,const Item &item,bool landscape=false);
    ImTextureID asset(const std::string &root,const std::string &name);
    ImTextureID avatar(const std::string &url,const std::string &primary_token);
    void clear();
};
} // namespace plex
