// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sys/stat.h>
#include <unistd.h>
#include <string>
namespace plex {
inline std::string cache_root(const std::string &app) {
#ifdef __FreeBSD__
    // Separate from the installed title so updates retain cached artwork.
    const char *root="/data/plexstation-client";
    mkdir(root,0700);struct stat st{};
    if(lstat(root,&st)==0 && S_ISDIR(st.st_mode) && st.st_uid==geteuid() && !(st.st_mode&077))return root;
#endif
    return app; // host fixture / writable title fallback; failures are counted
}
}
