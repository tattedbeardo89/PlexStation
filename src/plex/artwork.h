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
    };
    std::future<Decoded> pending;
    std::vector<Entry> entries;
    uint64_t frame = 0, generation = 0;
    Transport transport;

  public:
    explicit Artwork(Transport t) : transport(std::move(t)) {}
    void update();
    ImTextureID get(const std::string &base, const std::string &token, const std::string &thumb);
    void clear();
};
} // namespace plex
