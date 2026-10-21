// SPDX-License-Identifier: GPL-3.0-or-later
#include "client.h"
#include <curl/curl.h>
#include <mutex>
namespace plex {
namespace {
struct Body {
    std::string data;
};
size_t receive(char *p, size_t s, size_t n, void *user) {
    auto &b = *static_cast<Body *>(user);
    size_t size = s * n;
    if (size > 8 * 1024 * 1024 - b.data.size())
        return 0;
    b.data.append(p, size);
    return size;
}
} // namespace
Transport curl_transport() {
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    return [](const Request &q) {
        Reply r;
        auto *c = curl_easy_init();
        if (!c) {
            r.error = "Transport initialization failed";
            return r;
        }
        Body body;
        curl_slist *headers = nullptr;
        for (const auto &h : q.headers)
            headers = curl_slist_append(headers, (h.first + ": " + h.second).c_str());
        curl_easy_setopt(c, CURLOPT_URL, q.url.c_str());
        curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, q.method.c_str());
        curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR, "http,https");
        curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 0L); // never forward tokens across redirects
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(q.connect_timeout_ms));
        curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, static_cast<long>(q.timeout_ms));
        curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, receive);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
        if (!q.body.empty() || q.method == "POST") {
            curl_easy_setopt(c, CURLOPT_POSTFIELDS, q.body.data());
            curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, static_cast<long>(q.body.size()));
        }
        auto code = curl_easy_perform(c);
        long status = 0;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
        r.status = status;
        if (code != CURLE_OK) {
            r.failure=code==CURLE_COULDNT_RESOLVE_HOST ? Failure::Dns :
                code==CURLE_COULDNT_CONNECT ? Failure::Tcp : code==CURLE_OPERATION_TIMEDOUT ? Failure::Timeout :
                code==CURLE_PEER_FAILED_VERIFICATION ? Failure::Certificate :
                code==CURLE_SSL_CACERT_BADFILE ? Failure::CaMissing : code==CURLE_SSL_CONNECT_ERROR ? Failure::TlsHandshake :
                code==CURLE_WRITE_ERROR ? Failure::TooLarge : code==CURLE_PARTIAL_FILE ? Failure::HttpMalformed : Failure::Receive;
            r.error=failure_message(r.failure);
        } else
            r.body = std::move(body.data);
        curl_slist_free_all(headers);
        curl_easy_cleanup(c);
        return r;
    };
}
} // namespace plex
