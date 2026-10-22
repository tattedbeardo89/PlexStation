// SPDX-License-Identifier: GPL-3.0-or-later
#include "plex/client.h"
#include <cassert>
#include <iostream>
using namespace plex;
int main() {
    Endpoint e;
    assert(endpoint("https://192-168-1-9.machine.plex.direct:32401",e) && e.host=="192-168-1-9.machine.plex.direct" && e.port==32401 && !e.numeric);
    assert(endpoint("http://[::1]:32400",e) && e.host=="::1" && e.numeric);
    for(auto bad:{"https://host:0","https://host:65536","https://host:abc","https://host:","https://u:p@host","https://host?X-Plex-Token=secret","https://host/path","http://::1:3"}) assert(!endpoint(bad,e));
    assert(manual_endpoint("192.168.1.4")=="https://192.168.1.4:32400");
    assert(manual_endpoint("http://[::1]")=="http://[::1]:32400");
    assert(manual_endpoint("https://server:32401")=="https://server:32401");
    assert(manual_endpoint("").empty());
    int status=0; std::string body;
    std::string wire="HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: keep-alive\r\n\r\n{}";
    for(size_t n=0;n<wire.size();++n) assert(decode_http(wire.substr(0,n),false,status,body)==Decode::More);
    assert(decode_http(wire,false,status,body)==Decode::Complete && body=="{}");
    std::string location;
    assert(decode_http("HTTP/1.1 302 Found\r\nLocation: https://images.plex.tv/avatar\r\nContent-Length: 0\r\n\r\n",false,status,body,&location)==Decode::Complete && location=="https://images.plex.tv/avatar");
    assert(decode_http("HTTP/1.1 302 Found\r\nLocation: /a\r\nLocation: /b\r\nContent-Length: 0\r\n\r\n",false,status,body,&location)==Decode::Invalid);
    assert(decode_http(wire.substr(0,wire.size()-1),true,status,body)==Decode::Invalid);
    assert(decode_http("HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\n\r\n",true,status,body)==Decode::Encoding);
    assert(decode_http("HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\nx",true,status,body)==Decode::Invalid);
    assert(decode_http("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2;ext=ok\r\n{}\r\n0\r\nTrailer: ok\r\n\r\n",false,status,body)==Decode::Complete && body=="{}");
    assert(decode_http("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2junk\r\n{}\r\n0\r\n\r\n",true,status,body)==Decode::Invalid);
    for(auto xml : {"<?xml version=\"1.0\"?><MediaContainer machineIdentifier=\"machine\"/>",
                    "<MediaContainer machineIdentifier='machine'></MediaContainer>"}) {
        Client c([&](const Request &q){assert(q.headers.at("Accept")=="application/json");return Reply{200,xml,""};},"fixture");
        assert(c.identity("https://fixture","scoped","machine"));
        assert(c.identity("https://fixture","scoped","other").failure==Failure::IdentityMismatch);
    }
    for(auto malformed : {"<MediaContainer", "<Other machineIdentifier='machine'/>",
        "<!DOCTYPE MediaContainer SYSTEM 'file:///etc/passwd'><MediaContainer machineIdentifier='machine'/>",
        "<MediaContainer/>", "{not-json}"}) {
        Client c([&](const Request &){return Reply{200,malformed,""};},"fixture");
        assert(c.identity("https://fixture","scoped","machine").failure==Failure::IdentityMalformed);
    }
    std::vector<std::string> visited;
    Client client([&](const Request &q){
        visited.push_back(q.url);
        assert(q.headers.at("X-Plex-Token")=="server-scoped-fixture");
        if(q.url.find("bad-local")!=std::string::npos) return Reply{0,"","private must not leak",Failure::Dns};
        if(q.url.find("unreachable")!=std::string::npos) return Reply{0,"","",Failure::TcpTimeout};
        if(q.url.find("wrong")!=std::string::npos) return Reply{200,R"({"MediaContainer":{"machineIdentifier":"other"}})",""};
        if(q.url.find("/identity")!=std::string::npos) return Reply{200,"<MediaContainer machineIdentifier='machine'/>",""};
        if(q.url.find("broken-library")!=std::string::npos) return Reply{503,"",""};
        return Reply{200,R"({"MediaContainer":{"Directory":[{"key":"1","title":"Movies","type":"movie"}]}})",""};
    },"fixture");
    Server server{"machine","Fixture","server-scoped-fixture",{
        {"http://192.168.1.9:32400",true,false}, {"https://relay.example:443",false,true},
        {"https://bad-local.example:32400",true,false},{"https://wrong.example:32401",true,false},
        {"https://broken-library.example:32402",true,false},{"https://unreachable.example:32400",false,false}}};
    std::string selected; std::vector<ConnectionAttempt> attempts;
    auto result=client.select(server,false,selected,&attempts);
    assert(result && selected=="https://relay.example:443" && items(result.data).size()==1);
    assert(visited.front().find("bad-local")!=std::string::npos && attempts.size()==5);
    assert(attempts[0].result==failure_message(Failure::Dns) && attempts[1].identity.find("mismatch")!=std::string::npos);
    for(const auto &a:attempts) assert(a.endpoint.find("example")==std::string::npos && a.result.find("private")==std::string::npos);
    server.connections={{"http://192.168.1.9:32400",true,false}};
    visited.clear(); assert(!client.select(server,false,selected,&attempts) && visited.empty());
    assert(client.select(server,true,selected,&attempts));
    server.https_required=true; visited.clear();
    assert(!client.select(server,true,selected,&attempts) && visited.empty());
    server.https_required=false; server.connections[0].local=false;
    assert(!client.select(server,true,selected,&attempts) && visited.empty());
    std::cout<<"Connection policy/parser scenarios passed: XML/JSON identity, fallback through sections, TLS/HTTP policy, IPv6/ports, framing and redacted reports\n";
}
