// SPDX-License-Identifier: GPL-3.0-or-later
#include "plex/client.h"
#include <cstdlib>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
using namespace plex;
static int checks = 0;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            std::cerr << "FAIL line " << __LINE__ << ": " #x "\n";                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (0)
int main() {
    CHECK(encode("a b&é") == "a%20b%26%C3%A9");
    CHECK(valid_base("https://server.plex.direct:32400", false));
    CHECK(!valid_base("http://192.168.1.2:32400", false));
    CHECK(valid_base("http://192.168.1.2:32400", true));
    CHECK(!valid_base("https://evil@server", true));
    CHECK(!valid_base("https://server/path", true));
    auto j = Json::parse(
        R"({"MediaContainer":{"Metadata":[{"ratingKey":"42","key":"/library/metadata/42","type":"movie","title":"Film","duration":120000,"viewOffset":"25000","Media":[{"container":"mkv","videoCodec":"hevc","Part":[{"key":"/library/parts/7/file.mkv"}]}]}]}})");
    auto media = items(j);
    CHECK(media.size() == 1);
    CHECK(media[0].offset == 25000);
    CHECK(media[0].thumb.empty());
    CHECK(items(Json{{"MediaContainer", {{"Metadata", nullptr}}}}).empty());
    CHECK(items(Json{{"MediaContainer",{{"Metadata",Json::array({nullptr,7,"invalid",Json::array()})}}}}).empty());
    CHECK(number(Json{{"n",UINT64_MAX}},"n",-1)==-1);
    CHECK(number(Json{{"n","9999999999999999999999999"}},"n",-1)==-1);
    auto playback = direct_play("https://server", "a&b", media[0]);
    CHECK(playback.mode == Mode::DirectPlay);
    CHECK(playback.url == "https://server/library/parts/7/file.mkv?X-Plex-Token=a%26b");
    CHECK(direct_play("https://server", "x", media[0], 9).mode == Mode::Unsupported);
    auto broken = media[0];
    broken.media[0]["Part"][0]["key"] = "https://evil/steal";
    CHECK(direct_play("https://server", "x", broken).mode == Mode::Unsupported);
    broken = media[0];
    broken.media[0]["Part"].push_back(broken.media[0]["Part"][0]);
    CHECK(direct_play("https://server", "x", broken).mode == Mode::Unsupported);
    auto resources = Json::parse(
        R"([{"name":"Home","provides":"server","clientIdentifier":"abc","accessToken":"scoped","connections":[{"uri":"https://remote","local":false},{"uri":"http://local:32400","local":true},{"uri":"https://local","local":true}]}])");
    auto ss = servers(resources);
    CHECK(ss.size() == 1);
    CHECK(ss[0].connections[0].uri == "https://local");
    int calls = 0;
    Client client(
        [&](const Request &q) {
            ++calls;
            CHECK(q.headers.at("X-Plex-Token") == "scoped");
            if (q.url == "https://local/identity")
                return Reply{503, "", ""};
            if (q.url == "https://remote/library/sections")
                return Reply{200, R"({"MediaContainer":{"Directory":[]}})", ""};
            return Reply{200, R"({"MediaContainer":{"machineIdentifier":"abc"}})", ""};
        },
        "test-device");
    std::string selected;
    auto result = client.select(ss[0], false, selected);
    CHECK(bool(result));
    CHECK(calls == 3);
    CHECK(selected == "https://remote");
    Client mismatch(
        [](const Request &) {
            return Reply{200, R"({"MediaContainer":{"machineIdentifier":"other"}})", ""};
        },
        "test");
    CHECK(!mismatch.select(ss[0], false, selected));
    CHECK(selected.empty());
    Client auth(
        [](const Request &q) {
            CHECK(q.method == "POST");
            CHECK(q.body == "strong=false");
            CHECK(!q.headers.count("X-Plex-Token"));
            return Reply{201, R"({"id":12,"code":"LINK","expiresIn":60})", ""};
        },
        "test");
    auto p = auth.create_pin();
    CHECK(bool(p));
    CHECK(pin(p.data).id == 12);
    CHECK(pin(p.data).token.empty());
    Client expired(
        [](const Request &) { return Reply{401, "secret-token should never be surfaced", ""}; },
        "test");
    auto denied = expired.resources("private");
    CHECK(!denied);
    CHECK(denied.status == 401);
    CHECK(denied.error.find("secret-token") == std::string::npos);
    Client bad_json([](const Request &) { return Reply{200, "{bad", ""}; }, "test");
    CHECK(!bad_json.resources("x"));
    Client failure([](const Request &) { return Reply{0, "", "secret-url"}; }, "test");
    CHECK(failure.resources("x").error.find("secret-url") == std::string::npos);
    Client redirect([](const Request &) { return Reply{302, "", ""}; }, "test");
    CHECK(!redirect.resources("x"));
    Client request(
        [&](const Request &q) {
            CHECK(q.url.find("state=paused") != std::string::npos);
            CHECK(q.url.find("time=25000") != std::string::npos);
            return Reply{200, "", ""};
        },
        "test");
    CHECK(bool(request.timeline("https://server", "scoped", media[0], "paused", 25000)));
    CHECK(!request.timeline("https://server", "scoped", media[0], "bogus", 0));
    CHECK(!request.browse("https://server", "scoped", "/library", 0, 1000));
    CHECK(!request.call("GET", "https://server", "//evil", "x"));
    CHECK(!request.call("GET", "https://server", "/", "x\r\nInjected: yes"));
    Progress progress;
    CHECK(progress.due(0, "playing"));
    CHECK(!progress.due(9000, "playing"));
    CHECK(progress.due(10000, "playing"));
    CHECK(progress.due(10001, "paused"));
    CHECK(progress.due(10002, "stopped"));
    CHECK(!progress.due(20003, "stopped"));
    CHECK(audio_offset(0, 1) == 25);
    CHECK(audio_offset(0, -1) == -25);
    CHECK(audio_offset(500, 1) == 500);
    CHECK(audio_offset(-500, -1) == -500);
    char dir[] = "/tmp/plexstation-tests-XXXXXX";
    CHECK(mkdtemp(dir) != nullptr);
    std::string file = std::string(dir) + "/credentials.json";
    CHECK(save_credentials(file, "id", "token"));
    struct stat st {};
    CHECK(stat(file.c_str(), &st) == 0);
    CHECK((st.st_mode & 0777) == 0600);
    std::string id, token;
    CHECK(load_credentials(file, id, token));
    CHECK(id == "id" && token == "token");
    CHECK(save_credentials(file, "id", "replacement"));
    CHECK(load_credentials(file, id, token));
    CHECK(token == "replacement");
    chmod(file.c_str(), 0644);
    CHECK(!load_credentials(file, id, token));
    unlink(file.c_str());
    rmdir(dir);
    std::cout << checks << " checks passed\n";
}
