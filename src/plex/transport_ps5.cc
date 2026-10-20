// SPDX-License-Identifier: GPL-3.0-or-later
// Uses the same GnuTLS dependency as upstream OpenSubtitles. No browser runtime.
#include "client.h"
#include "platform.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <gnutls/gnutls.h>
#include <mutex>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
namespace plex {
Transport ps5_transport() {
    return [](const Request &q) {
        Reply r;
        bool tls = q.url.rfind("https://", 0) == 0;
        size_t start = tls ? 8 : 7;
        size_t slash = q.url.find('/', start);
        std::string authority = q.url.substr(start, slash - start);
        if (!valid_base((tls ? "https://" : "http://") + authority, true))
            return Reply{0, "", "Invalid address"};
        std::string host = authority, port = tls ? "443" : "80";
        if (host.front() == '[')
            return Reply{0, "", "IPv6 literal addresses are not yet supported"};
        size_t colon = host.find(':');
        if (colon != std::string::npos) {
            port = host.substr(colon + 1);
            host.resize(colon);
        }
        addrinfo hints{}, *addresses = nullptr;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(host.c_str(), port.c_str(), &hints, &addresses) != 0)
            return Reply{0, "", "DNS failed"};
        int fd = -1;
        for (auto *a = addresses; a; a = a->ai_next) {
            fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
            if (fd < 0)
                continue;
            int flags = fcntl(fd, F_GETFL, 0);
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            int rc = connect(fd, a->ai_addr, a->ai_addrlen);
            pollfd p{fd, POLLOUT, 0};
            int err = 0;
            socklen_t len = sizeof err;
            if ((rc == 0 || poll(&p, 1, 5000) > 0) &&
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && !err) {
                fcntl(fd, F_SETFL, flags);
                break;
            }
            close(fd);
            fd = -1;
        }
        freeaddrinfo(addresses);
        if (fd < 0)
            return Reply{0, "", "Connect failed"};
        timeval tv{5, 0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
        gnutls_session_t session = nullptr;
        gnutls_certificate_credentials_t cred = nullptr;
        auto cleanup = [&] {
            if (session)
                gnutls_deinit(session);
            if (cred)
                gnutls_certificate_free_credentials(cred);
            close(fd);
        };
        if (tls) {
            static std::once_flag once;
            std::call_once(once, [] { gnutls_global_init(); });
            if (gnutls_certificate_allocate_credentials(&cred) < 0) {
                cleanup();
                return Reply{0, "", "TLS initialization failed"};
            }
            auto ca = std::string(plat_data_dir()) + "/certs/cacert.pem";
            if (gnutls_certificate_set_x509_trust_file(cred, ca.c_str(), GNUTLS_X509_FMT_PEM) <=
                    0 ||
                gnutls_init(&session, GNUTLS_CLIENT) < 0) {
                cleanup();
                return Reply{0, "", "CA store unavailable"};
            }
            gnutls_set_default_priority(session);
            gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, cred);
            gnutls_server_name_set(session, GNUTLS_NAME_DNS, host.data(), host.size());
            gnutls_session_set_verify_cert(session, host.c_str(), 0);
            gnutls_transport_set_int(session, fd);
            gnutls_handshake_set_timeout(session, 5000);
            gnutls_record_set_timeout(session, 5000);
            if (gnutls_handshake(session) < 0) {
                cleanup();
                return Reply{0, "", "TLS verification or handshake failed"};
            }
        }
        std::string request = q.method + " " +
                              (slash == std::string::npos ? "/" : q.url.substr(slash)) +
                              " HTTP/1.1\r\nHost: " + authority + "\r\nConnection: close\r\n";
        for (const auto &h : q.headers)
            request += h.first + ": " + h.second + "\r\n";
        request += "Content-Length: " + std::to_string(q.body.size()) + "\r\n\r\n" + q.body;
        size_t at = 0;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (at < request.size()) {
            ssize_t n = tls ? gnutls_record_send(session, request.data() + at, request.size() - at)
                            : send(fd, request.data() + at, request.size() - at, 0);
            if (n <= 0 || std::chrono::steady_clock::now() > deadline) {
                cleanup();
                return Reply{0, "", "Send failed"};
            }
            at += n;
        }
        std::string all;
        char buf[16384];
        for (;;) {
            ssize_t n =
                tls ? gnutls_record_recv(session, buf, sizeof buf) : recv(fd, buf, sizeof buf, 0);
            if (n == 0)
                break;
            if (n < 0 || all.size() + n > 8 * 1024 * 1024 ||
                std::chrono::steady_clock::now() > deadline) {
                cleanup();
                return Reply{0, "", "Receive failed or response limit exceeded"};
            }
            all.append(buf, n);
        }
        cleanup();
        auto split = all.find("\r\n\r\n");
        if (split == std::string::npos || sscanf(all.c_str(), "HTTP/%*s %d", &r.status) != 1)
            return Reply{0, "", "Invalid HTTP response"};
        auto headers = all.substr(0, split);
        std::transform(headers.begin(), headers.end(), headers.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        r.body = all.substr(split + 4);
        if (headers.find("transfer-encoding: chunked") != std::string::npos) {
            std::string body;
            size_t p = 0;
            for (;;) {
                auto end = r.body.find("\r\n", p);
                if (end == std::string::npos)
                    return Reply{0, "", "Invalid chunk"};
                size_t len = 0;
                auto num = std::from_chars(r.body.data() + p, r.body.data() + end, len, 16);
                if (num.ec != std::errc())
                    return Reply{0, "", "Invalid chunk size"};
                if (!len) {
                    if (r.body.compare(end + 2, 2, "\r\n") != 0)
                        return Reply{0, "", "Invalid terminal chunk"};
                    break;
                }
                p = end + 2;
                if (len > r.body.size() - p || r.body.compare(p + len, 2, "\r\n") != 0)
                    return Reply{0, "", "Truncated chunk"};
                body.append(r.body, p, len);
                p += len + 2;
            }
            r.body = std::move(body);
        } else {
            auto length_at = headers.find("\r\ncontent-length:");
            if (length_at != std::string::npos) {
                length_at += 17;
                while (length_at < headers.size() && headers[length_at] == ' ')
                    ++length_at;
                auto end = headers.find("\r\n", length_at);
                if (end == std::string::npos)
                    end = headers.size();
                size_t length = 0;
                auto parsed =
                    std::from_chars(headers.data() + length_at, headers.data() + end, length);
                if (parsed.ec != std::errc() || parsed.ptr != headers.data() + end ||
                    length != r.body.size())
                    return Reply{0, "", "Invalid or truncated content length"};
            }
        }
        return r;
    };
}
} // namespace plex
