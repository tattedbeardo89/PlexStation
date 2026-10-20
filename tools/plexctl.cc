// SPDX-License-Identifier: GPL-3.0-or-later
#include "plex/client.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
using namespace plex;
int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "Usage: plexctl STATE_DIRECTORY login|servers|signout\n";
        return 2;
    }
    std::string dir = argv[1], command = argv[2], file = dir + "/credentials.json", id, token;
    struct stat st {};
    if (mkdir(dir.c_str(), 0700) != 0 && (lstat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode) ||
                                          st.st_uid != geteuid() || (st.st_mode & 077))) {
        std::cerr << "State directory must be owned by you and mode 0700\n";
        return 1;
    }
    if (command == "signout") {
        if (unlink(file.c_str()) != 0 && access(file.c_str(), F_OK) == 0)
            return 1;
        std::cout << "Local credentials removed\n";
        return 0;
    }
    if (!load_credentials(file, id, token)) {
        unsigned char bytes[16];
        std::ifstream random("/dev/urandom", std::ios::binary);
        if (!random.read(reinterpret_cast<char *>(bytes), sizeof bytes))
            return 1;
        const char *hex = "0123456789abcdef";
        for (auto b : bytes) {
            id += hex[b >> 4];
            id += hex[b & 15];
        }
    }
    Client client(curl_transport(), id);
    if (command == "login") {
        auto r = client.create_pin();
        if (!r) {
            std::cerr << r.error << "\n";
            return 1;
        }
        auto p = pin(r.data);
        if (!p.id || p.code.empty() || p.expires <= 0) {
            std::cerr << "Invalid device-link response\n";
            return 1;
        }
        std::cout << "Open on your phone or PC:\nhttps://plex.tv/auth#?clientID=" << encode(id)
                  << "&code=" << encode(p.code) << "&context%5Bdevice%5D%5Bproduct%5D=PlexStation\n"
                  << std::flush;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(p.expires);
        while (std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            auto polled = client.poll_pin(p.id, p.code);
            if (!polled) {
                std::cerr << polled.error << "\n";
                return 1;
            }
            auto updated = pin(polled.data);
            if (!updated.token.empty()) {
                if (!save_credentials(file, id, updated.token)) {
                    std::cerr << "Unable to persist credentials securely\n";
                    return 1;
                }
                std::cout << "Signed in\n";
                return 0;
            }
        }
        std::cerr << "Device link expired; run login again\n";
        return 1;
    }
    if (command == "servers") {
        if (token.empty()) {
            std::cerr << "Sign in first\n";
            return 1;
        }
        auto r = client.resources(token);
        if (!r) {
            std::cerr << r.error << "\n";
            return 1;
        }
        for (const auto &s : servers(r.data))
            std::cout << s.name << " (" << s.connections.size() << " candidate endpoints)\n";
        return 0;
    }
    std::cerr << "Unknown command\n";
    return 2;
}
