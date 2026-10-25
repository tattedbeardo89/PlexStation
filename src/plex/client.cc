// SPDX-License-Identifier: GPL-3.0-or-later
#include "client.h"
#include "diagnostics.h"
#include "home.h"
#include "tinyxml2/tinyxml2.h"
#include <algorithm>
#include <charconv>
#include <limits>
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
    if (i->is_number_unsigned() && i->get<uint64_t>() > static_cast<uint64_t>(INT64_MAX))
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
static std::string display_text(std::string value,size_t limit) {
    if(value.size()>limit) {size_t end=limit;while(end && (static_cast<unsigned char>(value[end])&0xc0)==0x80)--end;value.resize(end);}
    return value;
}
std::vector<Item> items(const Json &j,bool include_media) {
    std::vector<Item> out;
    checkpoint(7,"container");
    const auto &c = container(j);
    const char *key = c.contains("Metadata") ? "Metadata" : "Directory";
    auto it = c.find(key);
    if (it == c.end() || !it->is_array())
        return out;
    const size_t limit=diagnostic(TenItems) ? 10 : 200;
    checkpoint(9,"listing reserve",std::min(it->size(),limit));
    out.reserve(std::min(it->size(),limit));
    for (const auto &v : *it) {
        if(out.size()>=limit) break;
        if(!v.is_object()) continue;
        Item x;
        x.id = text(v, "ratingKey", text(v, "key"));
        if(x.id.rfind("/library/metadata/",0)==0) x.id=x.id.substr(18);
        if(x.id.empty() || x.id.size()>128 || x.id.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-")!=std::string::npos) continue;
        x.key = text(v, "key");
        x.type = text(v, "type");
        x.title = text(v, "title", "Untitled");
        if(x.title.empty()) x.title="Untitled";
        x.title=display_text(x.title,256);
        x.summary = display_text(text(v, "summary"),2048);
        x.series=display_text(text(v,"grandparentTitle",text(v,"parentTitle")),256);
        x.content_rating=display_text(text(v,"contentRating"),40);x.released=text(v,"originallyAvailableAt");
        auto tags=[&](const char *name,std::vector<std::string> &out) {
            auto list=v.find(name);if(list==v.end() || !list->is_array())return;
            for(const auto &tag:*list) {auto label=display_text(text(tag,"tag"),80);if(!label.empty())out.push_back(label);if(out.size()==12)break;}
        };
        tags("Genre",x.genres);tags("Role",x.cast);
        x.thumb = display_text(text(v, "thumb"),4096);
        x.art = display_text(text(v, "art"),4096);
        x.parent_thumb=display_text(text(v,"parentThumb"),4096);x.grandparent_thumb=display_text(text(v,"grandparentThumb"),4096);
        x.parent_art=display_text(text(v,"parentArt"),4096);x.grandparent_art=display_text(text(v,"grandparentArt"),4096);
        x.year = static_cast<int>(std::clamp<int64_t>(number(v,"year"),0,9999));
        x.duration = std::max<int64_t>(0,number(v,"duration"));
        x.offset = std::max<int64_t>(0,number(v,"viewOffset"));
        x.watched = number(v, "viewCount") > 0;
        x.artist=display_text(text(v,"grandparentTitle",text(v,"originalTitle")),256);
        x.album=display_text(text(v,"parentTitle"),256);
        x.section_id=display_text(text(v,"librarySectionID",number(v,"librarySectionID")>0?std::to_string(number(v,"librarySectionID")):""),128);x.guid=display_text(text(v,"guid"),256);
        if(x.guid.rfind("plex://",0)!=0){auto guids=v.find("Guid");if(guids!=v.end() && guids->is_array())for(const auto &g:*guids){auto id=text(g,"id");if(id.rfind("plex://",0)==0){x.guid=display_text(id,256);break;}}}
        x.parent_id=display_text(text(v,"parentRatingKey"),128);x.grandparent_id=display_text(text(v,"grandparentRatingKey"),128);
        x.disc=static_cast<int>(std::clamp<int64_t>(number(v,"parentIndex"),0,10000));
        x.track=static_cast<int>(std::clamp<int64_t>(number(v,"index"),0,100000));
        x.added=std::max<int64_t>(0,number(v,"addedAt"));x.last_played=std::max<int64_t>(0,number(v,"lastViewedAt"));
        if(x.thumb.empty())x.thumb=display_text(text(v,"parentThumb",text(v,"grandparentThumb")),4096);
        x.season = number(v, "parentIndex");
        x.episode = number(v, "index");
        if (include_media && v.contains("Media") && v["Media"].is_array()) {
            x.media=Json::array();
            for(const auto &version:v["Media"]) {
                if(!version.is_object())continue;
                Json parts=Json::array();auto list=version.find("Part");
                if(list!=version.end() && list->is_array())for(const auto &part:*list) {
                    auto key=text(part,"key");
                    Json streams=Json::array();auto ss=part.find("Stream");
                    if(ss!=part.end() && ss->is_array())for(const auto &stream:*ss) {
                        Json value=Json::object();for(const char *field:{"id","streamType","codec","language","languageCode","languageTag","title","displayTitle","hearingImpaired","key","forced","selected","channels","decision"}) {
                            auto v=stream.find(field);if(v!=stream.end() && (v->is_string() || v->is_number() || v->is_boolean()))value[field]=v->is_string()?Json(display_text(v->get<std::string>(),512)):*v;
                        }
                        streams.push_back(std::move(value));if(streams.size()==32)break;
                    }
                    parts.push_back({{"key",key.size()<=4096 ? key : ""},{"Stream",std::move(streams)}});
                    if(parts.size()==2)break; // preserve multipart rejection without copying every part
                }
                x.media.push_back({{"container",display_text(text(version,"container"),40)},{"videoCodec",display_text(text(version,"videoCodec"),40)},{"audioCodec",display_text(text(version,"audioCodec"),40)},{"audioChannels",number(version,"audioChannels")},{"width",number(version,"width")},{"height",number(version,"height")},{"videoFrameRate",display_text(text(version,"videoFrameRate"),20)},{"Part",std::move(parts)}});
                if(x.media.size()==8)break;
            }
        }
        out.push_back(std::move(x));
    }
    checkpoint(8,"metadata extracted",out.size());
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
        auto required=v.find("httpsRequired");
        s.https_required=required!=v.end() && ((required->is_boolean() && required->get<bool>()) ||
            (required->is_number_integer() && required->get<int>()!=0));
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
                                 return (c.uri.rfind("https://",0)==0 ? 0 : 10) + (c.relay ? 4 : c.local ? 0 : 2);
                             };
                             return rank(a) < rank(b);
                         });
        out.push_back(std::move(s));
    }
    return out;
}
bool valid_base(const std::string &base, bool allow_http) {
    Endpoint parsed;
    return endpoint(base,parsed) && (parsed.tls || allow_http);
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
    bool library_request=path.rfind("/library/",0)==0;
    if(library_request) checkpoint(3,"http start");
    Reply r = transport(q);
    if(library_request) checkpoint(4,"http response",r.body.size());
    // Do not include response bodies, addresses, tokens or transport diagnostics in errors.
    if (r.failure != Failure::None)
        return {{}, failure_message(r.failure), r.status, r.failure, r.tcp_connected, r.tls_verified};
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
        return {{}, "Access denied: sign in again or check server sharing", r.status, Failure::HttpStatus};
    if (r.status < 200 || r.status >= 300)
        return {{}, "Server returned HTTP " + std::to_string(r.status), r.status, Failure::HttpStatus};
    if(library_request) checkpoint(5,"http validated");
    auto body_start=r.body.find_first_not_of(" \t\r\n",r.body.compare(0,3,"\xEF\xBB\xBF")==0 ? 3:0);
    if(path.find("/home/users")!=std::string::npos && body_start!=std::string::npos && r.body[body_start]=='<') {
        tinyxml2::XMLDocument document;
        if(r.body.find("<!DOCTYPE")!=std::string::npos || document.Parse(r.body.data(),r.body.size())!=tinyxml2::XML_SUCCESS)
            return {{},"Invalid Plex Home XML",r.status};
        auto *root=document.RootElement(); if(!root || root->NextSiblingElement())return {{},"Invalid Plex Home response",r.status};
        auto object=[](tinyxml2::XMLElement *element) {
            Json out=Json::object();
            for(const char *key:{"id","uuid","title","username","thumb","avatar","avatarUrl","protected","admin","restricted","authenticationToken","authToken"})
                if(auto value=element->Attribute(key))out[key]=value;
            return out;
        };
        std::string name=root->Name();
        if(name=="user" || name=="User")return {object(root),"",r.status};
        if(name!="users" && name!="MediaContainer")return {{},"Invalid Plex Home user list",r.status};
        Json list=Json::array();
        for(auto *user=root->FirstChildElement();user && list.size()<32;user=user->NextSiblingElement()) {
            std::string tag=user->Name();if(tag=="User" || tag=="user")list.push_back(object(user));
        }
        return {Json{{"users",std::move(list)}},"",r.status};
    }
    if (path == "/identity" && body_start != std::string::npos && r.body[body_start] == '<') {
        tinyxml2::XMLDocument document;
        if (r.body.find("<!DOCTYPE") != std::string::npos ||
            document.Parse(r.body.data(),r.body.size()) != tinyxml2::XML_SUCCESS)
            return {{},failure_message(Failure::IdentityMalformed),r.status,Failure::IdentityMalformed};
        auto *root=document.RootElement();
        if (!root || std::string(root->Name())!="MediaContainer" || root->NextSiblingElement() ||
            !root->Attribute("machineIdentifier") || !*root->Attribute("machineIdentifier"))
            return {{},failure_message(Failure::IdentityMalformed),r.status,Failure::IdentityMalformed};
        return {Json{{"MediaContainer",{{"machineIdentifier",root->Attribute("machineIdentifier")}}}},"",r.status};
    }
    if(path.rfind("/video/:/transcode/universal/decision",0)==0 && body_start!=std::string::npos && r.body[body_start]=='<') {
        tinyxml2::XMLDocument document;
        if(r.body.find("<!DOCTYPE")!=std::string::npos || document.Parse(r.body.data(),r.body.size())!=tinyxml2::XML_SUCCESS)return {{},"Invalid streaming decision XML",r.status};
        auto *root=document.RootElement();if(!root || std::string(root->Name())!="MediaContainer" || root->NextSiblingElement())return {{},"Invalid streaming decision",r.status};
        size_t nodes=0;bool invalid=false;
        std::function<Json(tinyxml2::XMLElement *,int)> convert=[&](tinyxml2::XMLElement *element,int depth)->Json {
            if(depth>8 || ++nodes>512){invalid=true;return Json::object();}
            Json out=Json::object();
            for(auto *a=element->FirstAttribute();a;a=a->Next()) {std::string value=a->Value();if(value.size()>4096){invalid=true;return out;}out[a->Name()]=value;}
            for(auto *child=element->FirstChildElement();child;child=child->NextSiblingElement()) {
                std::string name=child->Name();if(name=="Video" || name=="Track")name="Metadata";
                if(name!="Metadata" && name!="Media" && name!="Part" && name!="Stream")continue;
                if(!out.contains(name))out[name]=Json::array();
                out[name].push_back(convert(child,depth+1));if(invalid)break;
            }
            return out;
        };
        auto decision=convert(root,0);if(invalid)return {{},"Streaming decision exceeds safe limits",r.status};
        return {Json{{"MediaContainer",std::move(decision)}},"",r.status};
    }
    if (r.body.empty())
        return {Json::object(), "", r.status};
    const char *parse_operation=library_request?"library":path.find("/hubs")!=std::string::npos?"home-hubs":path.find("/home/users")!=std::string::npos?"home-profiles":path.find("/metadata")!=std::string::npos?"media-metadata":path.find("/resources")!=std::string::npos?"server-discovery":path.find("/pins")!=std::string::npos?"account-link":"api";
    if(library_request) checkpoint(6,"json parse start");
    bool too_deep=false;
    static std::atomic<unsigned> parse_reports{0};
    const bool trace_parse=parse_reports.fetch_add(1,std::memory_order_relaxed)<256;
    if(trace_parse) {fprintf(stderr,"json: operation=%s bytes=%zu parse=begin\n", parse_operation,r.body.size());fflush(stderr);}
    auto j = Json::parse(r.body,[&](int depth,Json::parse_event_t,Json &) {
        if(depth>48) {too_deep=true; return false;} return true;
    }, false);
    if(trace_parse) {fprintf(stderr,"json: operation=%s parse=%s\n",parse_operation,too_deep?"depth-limit":j.is_discarded()?"invalid-json":"complete");fflush(stderr);}
    if(too_deep) return {{},"Server metadata nesting exceeds the safe limit",r.status};
    if (j.is_discarded())
        return {{}, path=="/identity" ? failure_message(Failure::IdentityMalformed) : "Server returned invalid JSON",
                r.status, path=="/identity" ? Failure::IdentityMalformed : Failure::None};
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
Result Client::home_users(const std::string &token) {
    auto result=call("GET","https://plex.tv","/api/v2/home/users",token);
    if(!result && (result.status==404 || result.status==405))result=call("GET","https://plex.tv","/api/home/users",token);
    if(result && profiles(result.data).empty())result.error="Plex returned no valid Home profiles";
    return result;
}
Result Client::switch_home(const std::string &token,const std::string &id,const std::string &pin_value) {
    if(id.empty() || id.size()>32 || id.find_first_not_of("0123456789")!=std::string::npos || (!pin_value.empty() && !valid_pin(pin_value)))
        return {{},"Invalid profile or four-digit PIN",0};
    auto result=call("POST","https://plex.tv","/api/home/users/"+id+"/switch",token,"pin="+encode(pin_value));
    if(!result)return result;
    auto user=result.data.find("user");
    if(user!=result.data.end() && user->is_object()) result.data=*user;
    auto switched=text(result.data,"authenticationToken",text(result.data,"authToken"));
    if(profile_id(result.data)!=id || switched.empty() || switched.size()>4096 || switched.find_first_of(" \r\n\t")!=std::string::npos)
        return {{},"Plex returned an invalid or mismatched profile session",result.status};
    result.data["authToken"]=std::move(switched);
    return result;
}
Result Client::resources(const std::string &token) {
    return call("GET", "https://plex.tv", "/api/v2/resources?includeHttps=1&includeRelay=1&includeIPv6=1", token);
}
Result Client::identity(const std::string &base, const std::string &token, const std::string &expected) {
    auto result=call("GET",base,"/identity",token);
    if (!result) return result;
    auto id=text(container(result.data),"machineIdentifier");
    if (id.empty() || expected.empty())
        return {{},failure_message(Failure::IdentityMalformed),result.status,Failure::IdentityMalformed};
    if (id!=expected)
        return {{},failure_message(Failure::IdentityMismatch),result.status,Failure::IdentityMismatch};
    return result;
}
Client Client::bounded(int connect_ms,int total_ms) const {
    auto network=transport;
    return Client([network,connect_ms,total_ms](const Request &request){auto q=request;q.connect_timeout_ms=connect_ms;q.timeout_ms=total_ms;return network(q);},client_id);
}
Result Client::select(const Server &s, bool allow_http, std::string &selected,
                      std::vector<ConnectionAttempt> *attempts, const std::string &preferred) {
    selected.clear(); if(attempts) attempts->clear();
    Result last{{},"No permitted server connections",0,Failure::Policy};
    // Also sort callers' manually constructed candidates, not just resource JSON.
    auto connections=s.connections;
    std::stable_sort(connections.begin(),connections.end(),[](const Connection &a,const Connection &b){
        auto rank=[](const Connection &c){return (c.uri.rfind("https://",0)==0 ? 0:10)+(c.relay ? 4:c.local ? 0:2);};
        return rank(a)<rank(b);
    });
    // Prefer only an endpoint present in fresh authorized resources. Normal policy
    // checks still reject forbidden HTTP, invalid addresses or HTTPS-only downgrade.
    auto remembered=std::find_if(connections.begin(),connections.end(),[&](const Connection &c){return c.uri==preferred;});
    if(remembered!=connections.end())std::rotate(connections.begin(),remembered,remembered+1);
    unsigned index=0;
    for(const auto &c:connections) {
        ConnectionAttempt report; Endpoint parsed;
        report.endpoint="endpoint "+std::to_string(++index);
        report.route=c.relay ? "relay" : c.local ? "local" : "remote";
        bool valid=endpoint(c.uri,parsed);
        report.protocol=parsed.tls ? "HTTPS" : "HTTP"; report.port=parsed.port;
        if(valid) report.endpoint+=parsed.host.size()>=12 && parsed.host.compare(parsed.host.size()-12,12,".plex.direct")==0 ? " [plex.direct]" : parsed.numeric ? " [IP redacted]" : " [host redacted]";
        if(!valid || (!parsed.tls && (!allow_http || !c.local || s.https_required))) {
            report.result=failure_message(valid ? Failure::Policy:Failure::Address);
            if(attempts) attempts->push_back(report);
            continue;
        }
        last=identity(c.uri,s.token,s.id);
        home_checkpoint(4,"server identity verification",last?0:(last.status?last.status:-1));
        report.tcp=(last.tcp_connected || last.status>0) ? "connected" : last.error;
        report.tls=!parsed.tls ? "not encrypted (approved)" :
            (last.tls_verified || last.status>0) ? "verified" : last.tcp_connected ? last.error : "not attempted";
        report.identity=last ? "matched" : last.status>0 ? last.error : "not attempted";
        if(last) {
            last=sections(c.uri,s.token);
            report.libraries=last ? "authenticated" : last.error;
            if(parsed.tls && !last && last.status==0) report.tls=last.tls_verified ? "verified" : last.error;
            // A successful identity alone is insufficient; try later candidates
            // if authenticated sections or its response shape cannot be loaded.
            if(last && (!last.data.contains("MediaContainer") || !last.data["MediaContainer"].is_object() ||
                (last.data["MediaContainer"].contains("Directory") && !last.data["MediaContainer"]["Directory"].is_array())))
                last={{},"Malformed library sections response",last.status,Failure::HttpMalformed};
            report.libraries=last ? "authenticated" : last.error;
            if(last) selected=c.uri;
        }
        report.result=last ? "Connected; libraries loaded" : last.error;
        if(attempts) attempts->push_back(report);
        if(last) return last;
    }
    return last;
}
Result Client::sections(const std::string &b, const std::string &t) {
    home_checkpoint(5,"authenticated library discovery started");
    auto result=call("GET", b, "/library/sections", t);
    home_checkpoint(6,"authenticated library discovery completed",result?0:(result.status?result.status:-1));
    return result;
}
Result Client::browse(const std::string &b, const std::string &t, const std::string &path,
                      int start, int size) {
    if(diagnostic(TenItems)) size=std::min(size,10);
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
    static std::atomic<unsigned> credential_parse_reports{0};
    const bool trace=credential_parse_reports.fetch_add(1)<128;
    if(trace){fprintf(stderr,"json: operation=credentials parse=begin\n");fflush(stderr);}
    auto j = Json::parse(s, nullptr, false);
    if(trace){fprintf(stderr,"json: operation=credentials parse=%s\n",j.is_discarded()?"invalid-json":"complete");fflush(stderr);}
    if (j.is_discarded())
        return false;
    id = text(j, "clientId");
    token = text(j, "token");
    return !id.empty();
}
} // namespace plex
