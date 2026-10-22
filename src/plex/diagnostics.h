// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <pthread.h>
namespace plex {
enum DiagnosticFlag : unsigned {
    TextOnly=1, NoDownloads=2, NoDecode=4, NoUpload=8, TenItems=16, DetailedTrace=32, SafeHome=64, DetailedHome=128
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

#ifndef PLEX_BUILD_COMMIT
#define PLEX_BUILD_COMMIT "host-or-unversioned"
#endif
namespace plex {
inline std::atomic<unsigned> home_lines{0},home_seen{0},home_last{0};
inline std::atomic<bool> home_active{false},home_draw_ready{false};
inline void home_reset(){home_lines=0;home_seen=0;home_last=0;home_draw_ready=false;}
inline void home_checkpoint(unsigned stage,const char *label,int result=0,size_t count=0) {
    if(stage<1 || stage>13)return;
    auto bit=1u<<stage;
    bool first=(home_seen.fetch_or(bit)&bit)==0;
    if(!first && stage>=10 && !diagnostic(DetailedHome))return;
    if(home_lines.fetch_add(1)>=128)return;
    home_last=stage;
    fprintf(stderr,"home: H%02u build=%s thread=%p op=%s result=%d count=%zu\n",stage,PLEX_BUILD_COMMIT,(void*)pthread_self(),label,result,count);
    fflush(stderr);
}
}
