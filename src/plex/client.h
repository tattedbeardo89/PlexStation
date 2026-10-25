// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "nlohmann/json.hpp"
#include "net.h"
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
    int connect_timeout_ms = 5000, timeout_ms = 20000;
};
struct Reply {
    int status = 0;
    std::string body, error;
    Failure failure = Failure::None;
    bool tcp_connected = false, tls_verified = false;
    std::string location = {};
    std::string content_type = {};
};
using Transport = std::function<Reply(const Request &)>;
struct Result {
    Json data;
    std::string error;
    int status = 0;
    Failure failure = Failure::None;
    bool tcp_connected = false, tls_verified = false;
    explicit operator bool() const { return error.empty(); }
};
struct Connection {
    std::string uri;
    bool local = false, relay = false;
};
struct Server {
    std::string id, name, token;
    std::vector<Connection> connections;
    bool https_required = false;
};
struct Item {
    std::string id, key, type, title, summary, thumb, art;
    int year = 0, season = 0, episode = 0;
    int64_t duration = 0, offset = 0;
    bool watched = false;
    Json media;
    std::string series, content_rating, released;
    std::vector<std::string> genres, cast;
    std::string artist, album, parent_id, grandparent_id, section_id, guid;
    int disc=0, track=0;
    int64_t added=0, last_played=0;
    std::string parent_thumb,grandparent_thumb,parent_art,grandparent_art;
    bool personal = false; // local section designation; never trusted from server JSON
};
struct Pin {
    int64_t id = 0;
    std::string code, token;
    int64_t expires = 0;
};
std::string encode(const std::string &);
std::string text(const Json &, const char *, const std::string &fallback = "");
int64_t number(const Json &, const char *, int64_t fallback = 0);
std::vector<Item> items(const Json &,bool include_media=true);
std::vector<Server> servers(const Json &);
bool valid_base(const std::string &, bool allow_http);
struct ConnectionAttempt {
    std::string endpoint, route, protocol, tcp = "not attempted", tls = "not attempted",
        identity = "not attempted", libraries = "not attempted", result;
    unsigned port = 0;
};
class Client {
    Transport transport;
    std::string client_id;

  public:
    explicit Client(Transport transport, std::string client_id);
    Result call(const std::string &method, const std::string &base, const std::string &path,
                const std::string &token = "", const std::string &body = "");
    Result create_pin(bool strong = false);
    Result validate_account(const std::string &account_token);
    Result poll_pin(int64_t id, const std::string &code);
    Result home_users(const std::string &primary_token);
    Result switch_home(const std::string &primary_token,const std::string &id,const std::string &pin="");
    Result resources(const std::string &account_token);
    Client bounded(int connect_ms,int total_ms) const;
    Result select(const Server &, bool allow_http, std::string &selected,
                  std::vector<ConnectionAttempt> *attempts = nullptr, const std::string &preferred = "");
    Result identity(const std::string &base, const std::string &token, const std::string &expected);
    Result sections(const std::string &base, const std::string &token);
    Result browse(const std::string &base, const std::string &token, const std::string &path,
                  int start = 0, int size = 60);
    Result timeline(const std::string &base, const std::string &token, const Item &,
                    const std::string &state, int64_t position);
    Result watched(const std::string &base, const std::string &token, const Item &, bool watched);
};
Pin pin(const Json &);
std::string authorization_url(const std::string &client_id, const std::string &code);
enum class Mode { DirectPlay, Unsupported, DirectStream, Transcode };
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
