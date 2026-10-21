// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <pthread.h>
namespace plex {
enum DiagnosticFlag : unsigned {
    TextOnly=1, NoDownloads=2, NoDecode=4, NoUpload=8, TenItems=16, DetailedTrace=32
};
inline std::atomic<unsigned> diagnostic_flags{0}, diagnostic_lines{0};
inline bool diagnostic(DiagnosticFlag flag) {return (diagnostic_flags.load() & flag)!=0;}
// Labels are compile-time developer strings, never request paths/metadata.
inline void checkpoint(unsigned stage,const char *label,size_t count=0) {
    auto n=diagnostic_lines.fetch_add(1);
    if(n >= (diagnostic(DetailedTrace) ? 1024u : 128u)) return;
    fprintf(stderr,"library: CP%02u %s count=%zu\n",stage,label,count);
    if(diagnostic(DetailedTrace)) fprintf(stderr,"library: CP%02u thread=%p\n",stage,(void*)pthread_self());
    if(stage<=10 || diagnostic(DetailedTrace)) fflush(stderr);
}
}
