// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
/* Some native exit bridges may return while process teardown is pending.
 * Call through an opaque volatile pointer, then remain terminal: never execute
 * compiler-generated unreachable/trap bytes after a presumed noreturn call. */
static inline void crash_terminate(int status,void (*exit_function)(int),void (*returned)(void)) {
    void (*volatile dispatch)(int)=exit_function;
    dispatch(status);
    returned();
    for(;;) __asm__ volatile("pause");
}
