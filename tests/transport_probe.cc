// SPDX-License-Identifier: GPL-3.0-or-later
#include "plex/client.h"
#include <iostream>
using namespace plex;
#ifdef TEST_PS5_TRANSPORT
namespace plex {
Transport ps5_transport();
}
extern "C" const char *plat_data_dir() { return "/workspace/PlexStation/assets"; }
#define TRANSPORT ps5_transport
#else
#define TRANSPORT curl_transport
#endif
int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    Client c(TRANSPORT(), "test-device");
    std::string base = argv[1];
    auto ok = c.call("GET", base, "/ok", "test-only-token");
    if (!ok || text(ok.data, "status") != "ok")
        return 1;
    auto denied = c.call("GET", base, "/denied", "test-only-token");
    if (denied || denied.status != 401)
        return 1;
    auto invalid = c.call("GET", base, "/invalid");
    if (invalid || invalid.error != "Server returned invalid JSON")
        return 1;
    auto redirect = c.call("GET", base, "/redirect", "test-only-token");
    if (redirect || redirect.status != 302)
        return 1;
    auto huge = c.call("GET", base, "/huge");
    if (huge || huge.error != "Network or TLS request failed")
        return 1;
    auto tls = c.call("GET", base.substr(0, 4) + "s" + base.substr(4), "/ok");
    if (tls)
        return 1;
    auto self_signed = c.call("GET", argv[2], "/ok", "test-only-token");
    if (self_signed)
        return 1;
    auto chunked = c.call("GET", base, "/chunked");
    if (!chunked || text(chunked.data, "status") != "ok")
        return 1;
    auto truncated = c.call("GET", base, "/truncated");
    if (truncated)
        return 1;
    auto broken_chunk = c.call("GET", base, "/broken-chunk");
    if (broken_chunk)
        return 1;
    std::cout << "10 HTTP/TLS transport scenarios passed\n";
}
