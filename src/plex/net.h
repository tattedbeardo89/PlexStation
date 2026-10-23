// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cctype>
#include <map>
#include <string>
namespace plex {
enum class Failure { None, Address, Policy, Dns, TcpRefused, TcpTimeout, Tcp,
    TlsHandshake, Certificate, CaMissing, TlsConfig, Timeout, Send, Receive,
    Encoding, HttpMalformed, TooLarge, HttpStatus, IdentityMalformed, IdentityMismatch };
inline std::string failure_message(Failure f) {
    switch(f) {
    case Failure::Address: return "Invalid endpoint address or port";
    case Failure::Policy: return "HTTP blocked: explicit approval required or server requires HTTPS";
    case Failure::Dns: return "DNS resolution failed";
    case Failure::TcpRefused: return "TCP connection refused";
    case Failure::TcpTimeout: return "TCP connection timed out";
    case Failure::Tcp: return "Endpoint unreachable (TCP connection failed)";
    case Failure::TlsHandshake: return "TLS handshake failed";
    case Failure::Certificate: return "TLS certificate verification failed (check hostname, chain and PS5 clock)";
    case Failure::CaMissing: return "CA certificates unavailable: reinstall the complete certs folder";
    case Failure::TlsConfig: return "Unsupported TLS configuration";
    case Failure::Timeout: return "Network I/O timed out";
    case Failure::Send: return "Request transmission failed";
    case Failure::Receive: return "Response read failed";
    case Failure::Encoding: return "Unsupported HTTP response encoding";
    case Failure::HttpMalformed: return "Malformed or truncated HTTP response";
    case Failure::TooLarge: return "Response size limit exceeded";
    case Failure::IdentityMalformed: return "Malformed server identity response (expected XML or JSON)";
    case Failure::IdentityMismatch: return "Server identity mismatch: selected server was not verified";
    case Failure::HttpStatus: return "Server returned an HTTP error";
    default: return "Endpoint unreachable (transport failure)";
    }
}
struct Endpoint {
    bool tls=false, numeric=false;
    std::string host, authority;
    unsigned port=0;
};
inline bool endpoint(const std::string &base, Endpoint &e) {
    e={}; size_t start;
    if(base.rfind("https://",0)==0) { e.tls=true; start=8; }
    else if(base.rfind("http://",0)==0) start=7;
    else return false;
    e.authority=base.substr(start);
    if(e.authority.empty() || e.authority.size()>300 ||
       std::any_of(e.authority.begin(),e.authority.end(),[](unsigned char c){return c<=32 || c>=127;}) ||
       e.authority.find_first_of("/@?#\\%")!=std::string::npos) return false;
    std::string port;
    if(e.authority[0]=='[') {
        auto end=e.authority.find(']');
        if(end==std::string::npos) return false;
        e.host=e.authority.substr(1,end-1); e.numeric=true;
        if(e.host.find(':')==std::string::npos || e.host.find_first_not_of("0123456789abcdefABCDEF:.")!=std::string::npos) return false;
        if(end+1<e.authority.size()) {
            if(e.authority[end+1]!=':') return false;
            port=e.authority.substr(end+2); if(port.empty()) return false;
        }
    } else {
        auto colon=e.authority.find(':');
        e.host=e.authority.substr(0,colon);
        if(e.host.empty() || e.host.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-")!=std::string::npos) return false;
        e.numeric=e.host.find_first_not_of("0123456789.")==std::string::npos;
        if(colon!=std::string::npos) { port=e.authority.substr(colon+1); if(port.empty()) return false; }
    }
    e.port=e.tls ? 443 : 80;
    if(!port.empty()) {
        auto result=std::from_chars(port.data(),port.data()+port.size(),e.port);
        if(result.ec!=std::errc() || result.ptr!=port.data()+port.size() || e.port<1 || e.port>65535) return false;
    }
    return !e.host.empty();
}
inline std::string manual_endpoint(std::string input) {
    if(input.find("://")==std::string::npos) input="https://"+input;
    auto start=input.find("://")+3;
    if (input.size() <= start) return "";
    bool has_port=input[start]=='[' ? input.find("]:",start)!=std::string::npos : input.find(':',start)!=std::string::npos;
    if(!has_port) input+=":32400";
    Endpoint parsed; return endpoint(input,parsed) ? input : "";
}
// Incremental, bounded HTTP/1 framing: return as soon as the framed body is
// complete. Never wait for a keep-alive peer to close a complete response.
enum class Decode { More, Complete, Invalid, Encoding, TooLarge };
inline Decode decode_http(const std::string &wire, bool eof, int &status, std::string &body, std::string *location=nullptr,std::string *content_type=nullptr) {
    constexpr size_t limit=8*1024*1024;
    auto end=wire.find("\r\n\r\n");
    if(end==std::string::npos) return wire.size()>65536 ? Decode::Invalid : eof ? Decode::Invalid : Decode::More;
    if(end>65536 || wire.compare(0,7,"HTTP/1.")!=0 || wire.size()<12 ||
       (wire[7]!='0' && wire[7]!='1') || wire[8]!=' ') return Decode::Invalid;
    auto parsed=std::from_chars(wire.data()+9,wire.data()+12,status);
    if(parsed.ec!=std::errc() || parsed.ptr!=wire.data()+12 || status<100 || status>599 ||
       (wire[12]!=' ' && wire[12]!='\r')) return Decode::Invalid;
    std::map<std::string,std::string> headers;
    auto p=wire.find("\r\n")+2;
    while(p<end) {
        auto next=wire.find("\r\n",p), colon=wire.find(':',p);
        if(next==std::string::npos || colon==std::string::npos || colon>=next || colon==p) return Decode::Invalid;
        auto key=wire.substr(p,colon-p), value=wire.substr(colon+1,next-colon-1);
        std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return std::tolower(c);});
        auto a=value.find_first_not_of(" \t"), b=value.find_last_not_of(" \t");
        value=a==std::string::npos ? "" : value.substr(a,b-a+1);
        if(headers.count(key) && (key=="content-length" || key=="transfer-encoding" || key=="content-encoding" || key=="location")) return Decode::Invalid;
        if(key=="content-encoding") std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return std::tolower(c);});
        headers[key]=value; p=next+2;
    }
    if(content_type) *content_type=headers.count("content-type")?headers["content-type"]:"";
    if(location) *location=headers.count("location")?headers["location"]:"";
    if(headers.count("content-encoding") && headers["content-encoding"]!="identity") return Decode::Encoding;
    if(status>=100 && status<200) return Decode::Invalid; // no informational response expected
    if(status==204 || status==304) {body.clear(); return Decode::Complete;}
    size_t start=end+4;
    if(headers.count("transfer-encoding")) {
        std::string encoding=headers["transfer-encoding"];
        std::transform(encoding.begin(),encoding.end(),encoding.begin(),[](unsigned char c){return std::tolower(c);});
        if(encoding!="chunked" || headers.count("content-length")) return Decode::Encoding;
        p=start; body.clear();
        for(;;) {
            auto line=wire.find("\r\n",p);
            if(line==std::string::npos) return eof ? Decode::Invalid : Decode::More;
            auto semi=wire.find(';',p); size_t num_end=semi<line ? semi : line, length=0;
            auto r=std::from_chars(wire.data()+p,wire.data()+num_end,length,16);
            if(r.ec!=std::errc() || r.ptr!=wire.data()+num_end) return Decode::Invalid;
            p=line+2;
            if(length>limit-body.size()) return Decode::TooLarge;
            if(!length) {
                if(wire.size()<p+2) return eof ? Decode::Invalid : Decode::More;
                if(wire.compare(p,2,"\r\n")==0) return Decode::Complete;
                return wire.find("\r\n\r\n",p)==std::string::npos ? (eof ? Decode::Invalid : Decode::More) : Decode::Complete;
            }
            if(wire.size()-p<length+2) return eof ? Decode::Invalid : Decode::More;
            if(wire.compare(p+length,2,"\r\n")!=0) return Decode::Invalid;
            body.append(wire,p,length); p+=length+2;
        }
    }
    if(headers.count("content-length")) {
        const auto &value=headers["content-length"]; size_t length=0;
        auto r=std::from_chars(value.data(),value.data()+value.size(),length);
        if(r.ec!=std::errc() || r.ptr!=value.data()+value.size()) return Decode::Invalid;
        if(length>limit) return Decode::TooLarge;
        if(wire.size()-start<length) return eof ? Decode::Invalid : Decode::More;
        if(wire.size()-start!=length) return Decode::Invalid;
        body=wire.substr(start,length); return Decode::Complete;
    }
    if(wire.size()-start>limit) return Decode::TooLarge;
    if(!eof) return Decode::More;
    body=wire.substr(start); return Decode::Complete;
}
} // namespace plex
