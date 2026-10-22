#include "plex/home.h"
#include <cassert>
#include <iostream>
using namespace plex;
int main() {
    auto users=profiles(Json{{"users",Json::array({{{"id",42},{"uuid","owner"},{"title","Owner"},{"protected",true},{"admin",true}},{{"id","84"},{"title","Managed"},{"restricted",true},{"protected",false}}})}});
    assert(users.size()==2 && users[0].protected_user && users[1].managed && !users[1].protected_user);
    assert(profiles(Json{{"users",Json::array({{{"id",3}}})}})[0].protected_user);
    assert(profile_scope("id:42",users[0])!=profile_scope("id:42",users[1]));
    assert(valid_pin("0123") && !valid_pin("123") && !valid_pin("12a4"));
    int calls=0;
    Client client([&](const Request &q) {
        assert(q.headers.at("X-Plex-Token")=="primary-fixture");++calls;
        if(q.url=="https://plex.tv/api/v2/home/users")return Reply{404,"",""};
        if(q.url=="https://plex.tv/api/home/users")return Reply{200,"<users><User id='42' title='Owner' protected='1' admin='1'/><User id='84' title='Managed' protected='0' restricted='1'/></users>",""};
        assert(q.method=="POST" && q.url=="https://plex.tv/api/home/users/84/switch" && q.body=="pin=0123");
        return Reply{200,"<user id='84' authenticationToken='managed-fixture'/>",""};
    },"fixture");
    auto list=client.home_users("primary-fixture");assert(list && profiles(list.data).size()==2 && calls==2);
    auto session=client.switch_home("primary-fixture","84","0123");assert(session && text(session.data,"authToken")=="managed-fixture");
    assert(!client.switch_home("primary-fixture","../42","0123"));
    Client wrong([](const Request &){return Reply{200,R"({"id":42,"authenticationToken":"wrong"})",""};},"fixture");
    assert(!wrong.switch_home("primary-fixture","84","0123"));
    Client denied([](const Request &){return Reply{401,"private",""};},"fixture");
    assert(!denied.switch_home("primary-fixture","84","9999"));assert(!denied.home_users("primary-fixture"));
    Client json([](const Request &){return Reply{200,R"({"users":[{"id":84,"title":"Managed","protected":false,"restricted":true}]})",""};},"fixture");
    assert(json.home_users("primary-fixture"));
    Client wrapped([](const Request &){return Reply{200,R"({"user":{"id":84,"authToken":"child-fixture"}})",""};},"fixture");
    assert(wrapped.switch_home("primary-fixture","84","0123"));
    std::cout<<"Plex Home production client tests passed: JSON/XML, documented legacy fallback, PIN request, mismatched/unauthorized rejection, isolated stable scope\n";
}
