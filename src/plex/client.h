// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "nlohmann/json.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>
namespace plex {
using Json = nlohmann::json;
struct Request {
    std::string method, url;
    std::map<std::string, std::string> headers;
    std::string body;
};
struct Reply {
    int status = 0;
    std::string body, error;
};
using Transport = std::function<Reply(const Request &)>;
struct Result {
    Json data;
    std::string error;
    int status = 0;
    explicit operator bool() const { return error.empty(); }
};
struct Connection {
    std::string uri;
    bool local = false, relay = false;
};
struct Server {
    std::string id, name, token;
    std::vector<Connection> connections;
};
struct Item {
    std::string id, key, type, title, summary, thumb, art;
    int year = 0, season = 0, episode = 0;
    int64_t duration = 0, offset = 0;
    bool watched = false;
    Json media;
};
struct Pin {
    int64_t id = 0;
    std::string code, token;
    int expires = 0;
};
std::string encode(const std::string &);
std::string text(const Json &, const char *, const std::string &fallback = "");
int64_t number(const Json &, const char *, int64_t fallback = 0);
std::vector<Item> items(const Json &);
std::vector<Server> servers(const Json &);
bool valid_base(const std::string &, bool allow_http);
class Client {
    Transport transport;
    std::string client_id;

  public:
    explicit Client(Transport transport, std::string client_id);
    Result call(const std::string &method, const std::string &base, const std::string &path,
                const std::string &token = "", const std::string &body = "");
    Result create_pin();
    Result poll_pin(int64_t id, const std::string &code);
    Result resources(const std::string &account_token);
    Result select(const Server &, bool allow_http, std::string &selected);
    Result sections(const std::string &base, const std::string &token);
    Result browse(const std::string &base, const std::string &token, const std::string &path,
                  int start = 0, int size = 60);
    Result timeline(const std::string &base, const std::string &token, const Item &,
                    const std::string &state, int64_t position);
    Result watched(const std::string &base, const std::string &token, const Item &, bool watched);
};
Pin pin(const Json &);
enum class Mode { DirectPlay, Unsupported };
struct Playback {
    Mode mode = Mode::Unsupported;
    std::string url, error;
};
// Conservative first adapter: only explicit Original quality. Never claim a transcode occurred.
Playback direct_play(const std::string &base, const std::string &token, const Item &,
                     size_t version = 0);
struct Progress {
    int64_t last_ms = -1;
    std::string last_state;
    bool due(int64_t monotonic_ms, const std::string &state);
};
int audio_offset(int current, int steps);
// Atomic, mode-0600 credentials; local filesystem permissions, not encryption.
bool save_credentials(const std::string &file, const std::string &id, const std::string &token);
bool load_credentials(const std::string &file, std::string &id, std::string &token);
Transport curl_transport(); // host-only transport, strict TLS, no redirects, bounded requests
} // namespace plex
