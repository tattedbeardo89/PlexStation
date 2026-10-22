// SPDX-License-Identifier: GPL-3.0-or-later
#include "crash_termination.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
static jmp_buf done;
static int seen_status,returned;
static void exit_bridge_returns(int status){seen_status=status;}
static void record_terminal_fallback(void){returned=1;longjmp(done,1);}
int main(void){
    if(!setjmp(done))crash_terminate(140,exit_bridge_returns,record_terminal_fallback);
    assert(seen_status==140 && returned==1);
    puts("Returning native exit bridge reaches explicit terminal fallback without compiler unreachable trap");
}
