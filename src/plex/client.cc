// SPDX-License-Identifier: GPL-3.0-or-later
#include "client.h"
#include <algorithm>
#include <charconv>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace plex {
std::string encode(const std::string &s) {
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~')
            out += c;
        else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}
std::string text(const Json &j, const char *k, const std::string &fallback) {
    auto i = j.find(k);
    return i != j.end() && i->is_string() ? i->get<std::string>() : fallback;
}
int64_t number(const Json &j, const char *k, int64_t fallback) {
    auto i = j.find(k);
    if (i == j.end())
        return fallback;
    if (i->is_number_integer())
        return i->get<int64_t>();
    if (i->is_string()) {
        auto s = i->get<std::string>();
        int64_t n;
        auto r = std::from_chars(s.data(), s.data() + s.size(), n);
        if (r.ec == std::errc() && r.ptr == s.data() + s.size())
            return n;
    }
    return fallback;
}
static const Json &container(const Json &j) {
    static const Json empty = Json::object();
    auto it = j.find("MediaContainer");
    return it != j.end() && it->is_object() ? *it : empty;
}
std::vector<Item> items(const Json &j) {
    std::vector<Item> out;
    const auto &c = container(j);
    const char *key = c.contains("Metadata") ? "Metadata" : "Directory";
    auto it = c.find(key);
    if (it == c.end() || !it->is_array())
        return out;
    for (const auto &v : *it) {
        Item x;
        x.id = text(v, "ratingKey", text(v, "key"));
        x.key = text(v, "key");
        x.type = text(v, "type");
        x.title = text(v, "title");
        x.summary = text(v, "summary");
        x.thumb = text(v, "thumb");
        x.art = text(v, "art");
        x.year = number(v, "year");
        x.duration = number(v, "duration");
        x.offset = number(v, "viewOffset");
        x.watched = number(v, "viewCount") > 0;
        x.season = number(v, "parentIndex");
        x.episode = number(v, "index");
        if (v.contains("Media") && v["Media"].is_array())
            x.media = v["Media"];
        out.push_back(std::move(x));
    }
    return out;
}
std::vector<Server> servers(const Json &j) {
    std::vector<Server> out;
    if (!j.is_array())
        return out;
    for (const auto &v : j) {
        if (text(v, "provides").find("server") == std::string::npos)
            continue;
        Server s;
        s.id = text(v, "clientIdentifier");
        s.name = text(v, "name");
        s.token = text(v, "accessToken");
        auto it = v.find("connections");
        if (it != v.end() && it->is_array())
            for (const auto &c : *it) {
                auto flag = [&](const char *k) {
                    auto p = c.find(k);
                    return p != c.end() && ((p->is_boolean() && p->get<bool>()) ||
                                            (p->is_number_integer() && p->get<int>() != 0));
                };
                s.connections.push_back({text(c, "uri"), flag("local"), flag("relay")});
            }
        std::stable_sort(s.connections.begin(), s.connections.end(),
                         [](const Connection &a, const Connection &b) {
                             auto rank = [](const Connection &c) {
                                 return (c.local ? 0 : 4) + (c.relay ? 2 : 0) +
                                        (c.uri.rfind("https://", 0) == 0 ? 0 : 1);
                             };
                             return rank(a) < rank(b);
                         });
        out.push_back(std::move(s));
    }
    return out;
}
bool valid_base(const std::string &base, bool allow_http) {
    size_t start = base.rfind("https://", 0) == 0                ? 8
                   : allow_http && base.rfind("http://", 0) == 0 ? 7
                                                                 : 0;
    if (!start || base.size() <= start ||
        base.find_first_of("\r\n\t @?#\\", start) != std::string::npos)
        return false;
    return base.find('/', start) == std::string::npos;
}
Client::Client(Transport t, std::string id) : transport(std::move(t)), client_id(std::move(id)) {}
Result Client::call(const std::string &method, const std::string &base, const std::string &path,
                    const std::string &token, const std::string &body) {
    if (!valid_base(base, true) || path.empty() || path[0] != '/' || path.rfind("//", 0) == 0 ||
        path.find_first_of("\r\n") != std::string::npos ||
        token.find_first_of("\r\n") != std::string::npos || client_id.empty() ||
        client_id.find_first_of("\r\n") != std::string::npos)
        return {{}, "Invalid request address or header", 0};
    Request q{method,
              base + path,
              {{"Accept", "application/json"},
               {"X-Plex-Product", "PlexStation"},
               {"X-Plex-Version", "0.5.0"},
               {"X-Plex-Client-Identifier", client_id},
               {"X-Plex-Platform", "PlayStation 5"},
               {"X-Plex-Device", "PS5"},
               {"X-Plex-Device-Name", "PlexStation"}},
              body};
    if (!token.empty())
        q.headers["X-Plex-Token"] = token;
    if (!body.empty())
        q.headers["Content-Type"] = "application/x-www-form-urlencoded";
    Reply r = transport(q);
    // Do not include response bodies, addresses, tokens or transport diagnostics in errors.
    if (!r.error.empty()) {
        std::string safe = "Network or TLS request failed";
        if (r.error == "DNS failed") safe = "Could not resolve Plex. Check DNS and Internet access.";
        else if (r.error == "Connect failed") safe = "Could not connect to Plex. Check Internet access and retry.";
        else if (r.error == "CA store unavailable") safe = "TLS certificate store unavailable. Reinstall the complete certs folder.";
        else if (r.error == "TLS verification or handshake failed")
            safe = "TLS verification failed. Check the PS5 clock and certificate store.";
        return {{}, safe, r.status};
    }
    if (r.status == 401 || r.status == 403)
        return {{}, "Access denied: sign in again or check server sharing", r.status};
    if (r.status < 200 || r.status >= 300)
        return {{}, "Server returned HTTP " + std::to_string(r.status), r.status};
    if (r.body.empty())
        return {Json::object(), "", r.status};
    auto j = Json::parse(r.body, nullptr, false);
    if (j.is_discarded())
        return {{}, "Server returned invalid JSON", r.status};
    return {std::move(j), "", r.status};
}
Result Client::create_pin(bool strong) {
    auto result = call("POST", "https://plex.tv", "/api/v2/pins", "", strong ? "strong=true" : "strong=false");
    if (result && text(result.data, "clientIdentifier", client_id) != client_id)
        result.error = "Plex returned a PIN for another client.";
    return result;
}
Result Client::validate_account(const std::string &token) {
    auto result = call("GET", "https://plex.tv", "/api/v2/user", token);
    if (result && (!result.data.is_object() || number(result.data, "id") <= 0))
        result.error = "Invalid Plex account response";
    return result;
}
std::string authorization_url(const std::string &client_id, const std::string &code) {
    if (client_id.empty() || code.empty()) return "";
    return "https://app.plex.tv/auth#?clientID=" + encode(client_id) + "&code=" + encode(code) +
           "&context%5Bdevice%5D%5Bproduct%5D=PlexStation";
}
Result Client::poll_pin(int64_t id, const std::string &code) {
    if (id <= 0 || code.empty()) return {{}, "Invalid activation session", 0};
    auto result = call("GET", "https://plex.tv",
                "/api/v2/pins/" + std::to_string(id) + "?code=" + encode(code));
    if (result && text(result.data, "clientIdentifier", client_id) != client_id)
        result.error = "Plex returned a PIN for another client.";
    return result;
}
Result Client::resources(const std::string &token) {
    return call("GET", "https://plex.tv", "/api/v2/resources?includeHttps=1&includeRelay=1", token);
}
Result Client::select(const Server &s, bool allow_http, std::string &selected) {
    selected.clear();
    Result last{{}, "No permitted server connections", 0};
    for (const auto &c : s.connections) {
        if (!valid_base(c.uri, allow_http && c.local))
            continue;
        last = call("GET", c.uri, "/identity", s.token);
        if (last && text(container(last.data), "machineIdentifier") == s.id) {
            selected = c.uri;
            return last;
        }
        if (last)
            last.error = "Server identity did not match selected resource";
    }
    return last;
}
Result Client::sections(const std::string &b, const std::string &t) {
    return call("GET", b, "/library/sections", t);
}
Result Client::browse(const std::string &b, const std::string &t, const std::string &path,
                      int start, int size) {
    if (start < 0 || size < 1 || size > 200)
        return {{}, "Invalid page range", 0};
    return call("GET", b,
                path + (path.find('?') == std::string::npos ? "?" : "&") +
                    "X-Plex-Container-Start=" + std::to_string(start) +
                    "&X-Plex-Container-Size=" + std::to_string(size),
                t);
}
Result Client::timeline(const std::string &b, const std::string &t, const Item &i,
                        const std::string &state, int64_t pos) {
    if (state != "playing" && state != "paused" && state != "stopped")
        return {{}, "Invalid playback state", 0};
    return call("GET", b,
                "/:/timeline?ratingKey=" + encode(i.id) + "&key=" + encode(i.key) +
                    "&state=" + state + "&time=" + std::to_string(std::max<int64_t>(0, pos)) +
                    "&duration=" + std::to_string(i.duration),
                t);
}
Result Client::watched(const std::string &b, const std::string &t, const Item &i, bool yes) {
    return call("GET", b,
                std::string("/:/") + (yes ? "scrobble" : "unscrobble") +
                    "?identifier=com.plexapp.plugins.library&key=" + encode(i.id),
                t);
}
Pin pin(const Json &j) {
    return {number(j, "id"), text(j, "code"), text(j, "authToken"),
            number(j, "expiresIn", -1)};
}
Playback direct_play(const std::string &base, const std::string &token, const Item &i,
                     size_t version) {
    if (!valid_base(base, true) || !i.media.is_array() || version >= i.media.size())
        return {Mode::Unsupported, "", "No playable version"};
    const auto &m = i.media[version];
    auto parts = m.find("Part");
    if (parts == m.end() || !parts->is_array() || parts->size() != 1)
        return {Mode::Unsupported, "", "Multipart media is not implemented"};
    std::string key = text((*parts)[0], "key");
    if (key.rfind("/library/parts/", 0) != 0 || key.find_first_of("?#\r\n") != std::string::npos)
        return {Mode::Unsupported, "", "Invalid media part path"};
    auto codec = text(m, "videoCodec"), fmt = text(m, "container");
    if ((codec != "h264" && codec != "hevc") || (fmt != "mkv" && fmt != "mp4"))
        return {Mode::Unsupported, "",
                "This adapter currently supports Original H.264/HEVC MKV/MP4 only"};
    return {Mode::DirectPlay, base + key + "?X-Plex-Token=" + encode(token), ""};
}
bool Progress::due(int64_t ms, const std::string &state) {
    if (last_state == "stopped" && state == "stopped")
        return false;
    if (state != last_state || last_ms < 0 || ms - last_ms >= 10000) {
        last_state = state;
        last_ms = ms;
        return true;
    }
    return false;
}
int audio_offset(int current, int steps) { return std::clamp(current + steps * 25, -500, 500); }
bool save_credentials(const std::string &file, const std::string &id, const std::string &token) {
    auto tmp = file + ".tmp";
    int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0)
        return false;
    std::string s = Json{{"clientId", id}, {"token", token}}.dump();
    size_t at = 0;
    while (at < s.size()) {
        ssize_t n = write(fd, s.data() + at, s.size() - at);
        if (n <= 0) {
            close(fd);
            unlink(tmp.c_str());
            return false;
        }
        at += n;
    }
    bool ok = fsync(fd) == 0;
    if (close(fd) != 0)
        ok = false;
    if (ok)
        ok = rename(tmp.c_str(), file.c_str()) == 0;
    if (!ok)
        unlink(tmp.c_str());
    return ok;
}
bool load_credentials(const std::string &file, std::string &id, std::string &token) {
    int fd = open(file.c_str(), O_RDONLY | O_NOFOLLOW);
    if (fd < 0)
        return false;
    struct stat st {};
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
        (st.st_mode & 077) != 0 || st.st_size > 65536) {
        close(fd);
        return false;
    }
    std::string s;
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof buf)) > 0)
        s.append(buf, n);
    close(fd);
    if (n < 0)
        return false;
    auto j = Json::parse(s, nullptr, false);
    if (j.is_discarded())
        return false;
    id = text(j, "clientId");
    token = text(j, "token");
    return !id.empty();
}
} // namespace plex
