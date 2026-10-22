#include "plex/home_feed.h"
#include <cassert>
#include <iostream>
using namespace plex;
int main() {
    bool saw_release=false;int calls=0;
    Client client([&](const Request &q) {
        ++calls;assert(q.headers.at("X-Plex-Token")=="profile-scoped-token");
        if(q.url.find("/hubs/continueWatching")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"42","type":"episode","title":"Episode","grandparentTitle":"Series","viewOffset":1000,"duration":10000,"parentIndex":2,"index":3}]}})",""};
        if(q.url.find("sort=originallyAvailableAt:desc")!=std::string::npos)saw_release=true;
        return Reply{200,R"({"MediaContainer":{"Metadata":[]}})",""};
    },"fixture");
    Item movie;movie.id="1";movie.type="movie";Item tv;tv.id="2";tv.type="show";
    auto result=home_feed(client,"https://fixture.invalid","profile-scoped-token",{movie,tv});
    auto rows=home_rows(result.data);assert(rows.size()==1 && rows[0].continuing && saw_release);
    assert(rows[0].entries[0].offset==1000 && rows[0].entries[0].series=="Series");
    int prior=calls;assert(!home_feed(client,"https://fixture.invalid","profile-scoped-token",{},[]{return false;}));assert(calls==prior);
    Client empty([](const Request &){return Reply{200,R"({"MediaContainer":{"Metadata":[]}})",""};},"fixture");
    assert(home_rows(home_feed(empty,"https://fixture.invalid","profile-scoped-token",{}).data).empty());
    Client unavailable([](const Request &){return Reply{404,"",""};},"fixture");
    auto missing=home_feed(unavailable,"https://fixture.invalid","profile-scoped-token",{});assert(missing && home_rows(missing.data).empty());
    std::cout<<"Home feed tests passed: real Continue Watching identity/progress, empty rows omitted, release-date query, scoped token, cancellation, unsupported endpoints\n";
}
