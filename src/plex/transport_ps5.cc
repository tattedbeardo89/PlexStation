// SPDX-License-Identifier: GPL-3.0-or-later
// Nonblocking native HTTP/GnuTLS; certificate and hostname validation mandatory.
#include "client.h"
#include "platform.h"
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <gnutls/gnutls.h>
#include <mutex>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
namespace plex {
namespace {
using Clock = std::chrono::steady_clock;
using Deadline = Clock::time_point;
int wait_socket(int fd, short events, Deadline until) {
    for (;;) {
        int ms=static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(until-Clock::now()).count());
        if(ms<=0) return 0;
        pollfd p{fd,events,0}; int n=poll(&p,1,ms);
        if(n<0 && errno==EINTR) continue;
        return n;
    }
}
struct Channel {
    int fd=-1;
    gnutls_session_t session=nullptr;
    gnutls_certificate_credentials_t cred=nullptr;
    ~Channel() {
        if(session) gnutls_deinit(session);
        if(cred) gnutls_certificate_free_credentials(cred);
        if(fd>=0) close(fd);
    }
};
Reply failed(Failure f) { return {0,"",failure_message(f),f}; }
}
Transport ps5_transport() {
    return [](const Request &q) -> Reply {
        size_t slash=q.url.find('/',q.url.find("://")==std::string::npos ? 0 : q.url.find("://")+3);
        auto base=q.url.substr(0,slash); Endpoint e;
        if(!endpoint(base,e) || q.timeout_ms<=0 || q.connect_timeout_ms<=0) return failed(Failure::Address);
        auto until=Clock::now()+std::chrono::milliseconds(q.timeout_ms);
        addrinfo hints{}, *addresses=nullptr;
        hints.ai_socktype=SOCK_STREAM; hints.ai_family=AF_UNSPEC;
        if(getaddrinfo(e.host.c_str(),std::to_string(e.port).c_str(),&hints,&addresses)!=0) return failed(Failure::Dns);
        Channel c; bool verified=false;
        auto fail=[&](Failure f) { auto r=failed(f); r.tcp_connected=c.fd>=0; r.tls_verified=verified; return r; };
        Failure connect_failure=Failure::Tcp;
        for(auto *a=addresses; a; a=a->ai_next) {
            c.fd=socket(a->ai_family,a->ai_socktype,a->ai_protocol);
            if(c.fd<0) continue;
            int flags=fcntl(c.fd,F_GETFL,0);
            if(flags<0 || fcntl(c.fd,F_SETFL,flags|O_NONBLOCK)!=0) {
                close(c.fd); c.fd=-1; continue;
            }
            int rc=connect(c.fd,a->ai_addr,a->ai_addrlen), error=rc==0 ? 0 : errno;
            if(rc<0 && (error==EINPROGRESS || error==EWOULDBLOCK || error==EINTR)) {
                auto connect_until=std::min(until,Clock::now()+std::chrono::milliseconds(q.connect_timeout_ms));
                int ready=wait_socket(c.fd,POLLOUT,connect_until);
                if(ready==0) error=ETIMEDOUT;
                else if(ready<0) error=errno;
                else { socklen_t len=sizeof error; if(getsockopt(c.fd,SOL_SOCKET,SO_ERROR,&error,&len)<0) error=errno; }
            }
            if(error==0) break;
            connect_failure=error==ETIMEDOUT ? Failure::TcpTimeout : error==ECONNREFUSED ? Failure::TcpRefused : Failure::Tcp;
            close(c.fd); c.fd=-1;
            if(Clock::now()>=until) break;
        }
        freeaddrinfo(addresses);
        if(c.fd<0) return fail(connect_failure);
        if(e.tls) {
            static std::once_flag once; static int initialized;
            std::call_once(once,[]{initialized=gnutls_global_init();});
            if(initialized<0 || gnutls_certificate_allocate_credentials(&c.cred)<0 ||
               gnutls_init(&c.session,GNUTLS_CLIENT|GNUTLS_NONBLOCK)<0) return fail(Failure::TlsConfig);
            auto ca=std::string(plat_data_dir())+"/certs/cacert.pem";
            if(gnutls_certificate_set_x509_trust_file(c.cred,ca.c_str(),GNUTLS_X509_FMT_PEM)<=0) return fail(Failure::CaMissing);
            if(gnutls_set_default_priority(c.session)<0 ||
               gnutls_credentials_set(c.session,GNUTLS_CRD_CERTIFICATE,c.cred)<0) return fail(Failure::TlsConfig);
            if(!e.numeric && gnutls_server_name_set(c.session,GNUTLS_NAME_DNS,e.host.data(),e.host.size())<0) return fail(Failure::TlsConfig);
            gnutls_session_set_verify_cert(c.session,e.host.c_str(),0);
            gnutls_transport_set_int(c.session,c.fd);
            for(;;) {
                if(Clock::now()>=until) return fail(Failure::Timeout);
                int rc=gnutls_handshake(c.session);
                if(rc==0) break;
                if(rc==GNUTLS_E_INTERRUPTED) continue;
                if(rc==GNUTLS_E_AGAIN) {
                    if(wait_socket(c.fd,gnutls_record_get_direction(c.session) ? POLLOUT : POLLIN,until)>0) continue;
                    return fail(Failure::Timeout);
                }
                if(rc==GNUTLS_E_CERTIFICATE_VERIFICATION_ERROR) return fail(Failure::Certificate);
                if(rc==GNUTLS_E_UNSUPPORTED_VERSION_PACKET || rc==GNUTLS_E_NO_CIPHER_SUITES) return fail(Failure::TlsConfig);
                return fail(Failure::TlsHandshake);
            }
            unsigned status=0;
            if(gnutls_certificate_verify_peers3(c.session,e.host.c_str(),&status)<0 || status!=0) return fail(Failure::Certificate);
        }
        verified=e.tls;
        std::string request=q.method+" "+(slash==std::string::npos ? "/" : q.url.substr(slash))+
            " HTTP/1.1\r\nHost: "+e.authority+"\r\nConnection: close\r\nAccept-Encoding: identity\r\n";
        if(q.method.find_first_of("\r\n ")!=std::string::npos || request.find('\0')!=std::string::npos) return fail(Failure::Address);
        for(const auto &h:q.headers) {
            if(h.first.find_first_of("\r\n:")!=std::string::npos || h.second.find_first_of("\r\n")!=std::string::npos) return fail(Failure::Address);
            request+=h.first+": "+h.second+"\r\n";
        }
        request+="Content-Length: "+std::to_string(q.body.size())+"\r\n\r\n"+q.body;
        auto transfer = [&](bool send_data, const char *out, char *in, size_t length) -> ssize_t {
            for(;;) {
                if(Clock::now()>=until) return -2;
                ssize_t n=e.tls ? (send_data ? gnutls_record_send(c.session,out,length) : gnutls_record_recv(c.session,in,length)) :
                    (send_data ? send(c.fd,out,length,0) : recv(c.fd,in,length,0));
                if(n>=0) return n;
                bool interrupted=e.tls ? n==GNUTLS_E_INTERRUPTED : errno==EINTR;
                if(interrupted) continue;
                bool again=e.tls ? n==GNUTLS_E_AGAIN : errno==EAGAIN || errno==EWOULDBLOCK;
                if(!again) return -1;
                short events=e.tls ? (gnutls_record_get_direction(c.session) ? POLLOUT : POLLIN) : (send_data ? POLLOUT : POLLIN);
                if(wait_socket(c.fd,events,until)<=0) return -2;
            }
        };
        size_t at=0;
        while(at<request.size()) {
            auto n=transfer(true,request.data()+at,nullptr,request.size()-at);
            if(n<=0) return fail(n==-2 ? Failure::Timeout : Failure::Send);
            at+=n;
        }
        std::string wire; char buffer[16384]; Reply reply;
        reply.tcp_connected=true; reply.tls_verified=verified;
        for(;;) {
            auto n=transfer(false,nullptr,buffer,sizeof buffer);
            if(n<0) return fail(n==-2 ? Failure::Timeout : Failure::Receive);
            if(wire.size()+n>8*1024*1024+65536) return fail(Failure::TooLarge);
            wire.append(buffer,n);
            auto decoded=decode_http(wire,n==0,reply.status,reply.body);
            if(decoded==Decode::Complete) return reply;
            if(decoded==Decode::Invalid) return fail(Failure::HttpMalformed);
            if(decoded==Decode::Encoding) return fail(Failure::Encoding);
            if(decoded==Decode::TooLarge) return fail(Failure::TooLarge);
        }
    };
}
} // namespace plex
