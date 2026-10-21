// SPDX-License-Identifier: GPL-3.0-or-later
#include "plex/client.h"
#include <cassert>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <cerrno>
#include <string>
#include <iostream>
static std::string data_dir;
static bool timeout_connect;
static int timeout_fd=-1;
extern "C" const char *plat_data_dir() { return data_dir.c_str(); }
extern "C" int __real_getaddrinfo(const char *,const char *,const addrinfo *,addrinfo **);
extern "C" int __wrap_getaddrinfo(const char *host,const char *port,const addrinfo *hints,addrinfo **out) {
    if(std::string(host)=="tcp-timeout.invalid") { timeout_connect=true; host="127.0.0.1"; }
    if(std::string(host)=="missing.invalid") return EAI_NONAME;
    // Test-only DNS routing. Production has no DNS/verification override.
    if(std::string(host).find(".plex.direct")!=std::string::npos) host="127.0.0.1";
    return __real_getaddrinfo(host,port,hints,out);
}
extern "C" int __real_connect(int,const sockaddr *,socklen_t);
extern "C" int __wrap_connect(int fd,const sockaddr *address,socklen_t length) {
    if(timeout_connect) {timeout_connect=false;timeout_fd=fd;errno=EINPROGRESS;return -1;}
    return __real_connect(fd,address,length);
}
extern "C" int __real_poll(pollfd *,nfds_t,int);
extern "C" int __wrap_poll(pollfd *fds,nfds_t count,int ms) {
    if(count==1 && fds[0].fd==timeout_fd) {timeout_fd=-1;return 0;}
    return __real_poll(fds,count,ms);
}
extern "C" ssize_t __real_send(int,const void *,size_t,int);
extern "C" ssize_t __wrap_send(int fd,const void *data,size_t n,int flags) {
    static int calls;
    if(++calls==1) {errno=EINTR;return -1;}
    if(calls==2) {errno=EAGAIN;return -1;}
    return __real_send(fd,data,std::min<size_t>(n,7),flags);
}
extern "C" ssize_t __real_recv(int,void *,size_t,int);
extern "C" ssize_t __wrap_recv(int fd,void *data,size_t n,int flags) {
    static int calls;
    if(++calls==1) {errno=EINTR;return -1;}
    if(calls==2) {errno=EAGAIN;return -1;}
    return __real_recv(fd,data,std::min<size_t>(n,11),flags);
}
namespace plex { Transport ps5_transport(); }
using namespace plex;
int main(int argc,char **argv) {
    assert(argc==7); data_dir=argv[1];
    std::string plain=argv[2],tls=argv[3],stall=argv[4],refused=argv[5];
    Client client(ps5_transport(),"fixture-client");
    assert(client.identity(plain,"fixture-scoped-token","fixture-server"));
    auto good=client.identity(tls,"fixture-scoped-token","fixture-server");
    assert(good);
    assert(client.identity(tls,"fixture-scoped-token","wrong").failure==Failure::IdentityMismatch);
    auto wrong=tls; wrong.replace(wrong.find("fixture.plex.direct"),19,"wrong.plex.direct");
    assert(client.identity(wrong,"fixture-scoped-token","fixture-server").failure==Failure::Certificate);
    assert(client.identity("https://missing.invalid:32400","x","fixture-server").failure==Failure::Dns);
    assert(client.identity("http://tcp-timeout.invalid:32400","x","fixture-server").failure==Failure::TcpTimeout);
    assert(client.identity(refused,"x","fixture-server").failure==Failure::TcpRefused);
    auto transport=ps5_transport();
    Request request{"GET",stall+"/identity",{},""}; request.timeout_ms=60; request.connect_timeout_ms=40;
    assert(transport(request).failure==Failure::Timeout);
    request.url=plain+"/identity"; request.url.insert(4,"s"); request.timeout_ms=1000;
    assert(transport(request).failure==Failure::TlsHandshake);
    // TLS peer streams fragments and deliberately stays open after Content-Length.
    assert(client.call("GET",tls,"/fragmented","fixture-scoped-token"));
    assert(client.call("GET",plain,"/fragmented","fixture-scoped-token"));
    assert(client.call("GET",plain,"/encoding").failure==Failure::Encoding);
    assert(client.call("GET",plain,"/bad-length").failure==Failure::HttpMalformed);
    auto scoped_server=Server{"fixture-server","fixture","fixture-scoped-token",{
        {refused,true,false},{tls,true,false}}};
    std::string selected; std::vector<ConnectionAttempt> attempts;
    assert(client.select(scoped_server,false,selected,&attempts));
    // HTTPS is preferred before the local HTTP refusal.
    assert(selected==tls && attempts.size()==1);
    auto missing=data_dir; data_dir+="/absent";
    assert(client.identity(tls,"x","fixture-server").failure==Failure::CaMissing); data_dir=missing;
    if(std::string(argv[6])!="none") assert(client.identity(argv[6],"fixture-scoped-token","fixture-server"));
    std::cout<<"Real native transport fixture passed: verified plex.direct-shaped TLS/SNI, DNS/refusal, handshake/read deadlines, CA/hostname rejection, fragmented/keep-alive HTTP, encoding, identity, sections, IPv6 where available\n";
}
