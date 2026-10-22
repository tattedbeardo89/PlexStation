// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstddef>
static double now;
extern "C" double plat_time() { return now; }
int sceUserServiceInitialize(const void *) { return 0; }
int scePadInit() { return 0; }
int sceUserServiceGetForegroundUser(int32_t *u) { *u=42; return 0; }
int sceUserServiceGetInitialUser(int32_t *u) { *u=7; return 0; }
int sceUserServiceGetLoginUserIdList(int32_t *u) { for(int i=0;i<4;++i) u[i]=-1; return 0; }
static bool reuse, fail_open;
static int opens, closes, requested_user;
int scePadGetHandle(int32_t user,int32_t port,int32_t index) {
    assert(port==0 && index==0); requested_user=user; return reuse ? 9 : -1;
}
int scePadOpen(int32_t user,int32_t port,int32_t index,const void *) {
    assert(port==0 && index==0); requested_user=user; ++opens; return fail_open ? -1 : 9;
}
int scePadRead(int32_t,void *,int32_t);
int scePadClose(int32_t) { ++closes; return 0; }
#include "pad_ps5.h"
static int count=1;
static PadSample data[2];
int scePadRead(int32_t handle,void *out,int32_t capacity) {
    assert(handle==9 && capacity==64);
    if(count>0) memcpy(out,data,sizeof(PadSample)*count);
    return count;
}
int main() {
    PadState state{};
    fail_open=true; assert(!plat_pad_read(&state)); assert(!state.connected && opens==2);
    now=.5; assert(!plat_pad_read(&state)); assert(opens==2); // retry throttled
    now=1.1; reuse=true; fail_open=false;
    data[0].connected=1; data[0].lx=data[0].ly=data[0].rx=data[0].ry=128;
    data[0].buttons=PAD_CROSS; data[0].timestamp_us=1;
    plat_pad_set_private(true);
    assert(plat_pad_read(&state)); assert(state.connected && requested_user==42 && opens==2);
    assert(state.buttons==PAD_CROSS && state.lx==0 && state.ly==0);
    plat_pad_set_private(false); // privacy only suppresses diagnostics, never delivery
    // Read newest timestamp, regardless of batch order.
    count=2; data[1]=data[0]; data[0].timestamp_us=20; data[0].buttons=PAD_DOWN;
    data[1].timestamp_us=10; data[1].buttons=PAD_UP;
    assert(plat_pad_read(&state) && state.buttons==PAD_DOWN);
    count=0; now+=.1; assert(plat_pad_read(&state) && state.buttons==PAD_DOWN);
    now+=.6; assert(!plat_pad_read(&state) && !state.buttons); // no stuck held input
    count=1; data[0].buttons=PAD_INTERCEPTED|PAD_CROSS;
    assert(!plat_pad_read(&state) && !state.connected && closes==0);
    data[0].buttons=PAD_RIGHT; data[0].lx=255;
    assert(plat_pad_read(&state) && state.lx>.99f && state.buttons==PAD_RIGHT);
    count=-1; assert(!plat_pad_read(&state) && closes==1);
    now+=1.1; count=1; data[0].connected=1;
    assert(plat_pad_read(&state)); // read-error recovery
    data[0].connected=0; now+=.1; assert(!plat_pad_read(&state));
    now+=2.1; assert(!plat_pad_read(&state) && closes==2);
    puts("PS5 pad backend scenarios passed: retry, foreground/reused handle, samples, idle, stale, interception, dead zone, recovery");
}
