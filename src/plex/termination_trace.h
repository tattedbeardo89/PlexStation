// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdio>
// The pinned SDK maps this symbol to ps5_backtrace (_Unwind_Backtrace);
// Linux hosts provide the equivalent libc API. Never call from a signal handler.
extern "C" int backtrace(void **frames,int size);
namespace plex {
inline int termination_trace() {
    fprintf(stderr,"fatal: C++ terminate stack capture begin (raw addresses; module mapping required)\n");
    fflush(stderr);
    void *frames[24]{};
    int count=backtrace(frames,24);
    if(count<0)count=0;
    if(count>24)count=24;
    for(int i=0;i<count;++i)fprintf(stderr,"fatal: unwind frame[%d] raw=%p\n",i,frames[i]);
    fprintf(stderr,"fatal: C++ terminate stack capture complete count=%d\n",count);
    fflush(stderr);return count;
}
}
