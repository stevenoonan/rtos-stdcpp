// SPDX-License-Identifier: MIT
#include "rtos_stdcpp/runtime.h"
#include <stdlib.h>

__attribute__((weak, noreturn)) void rtos_stdcpp_runtime_fail(const char* message, const char* file,
                                                              unsigned line) {
    (void)message;
    (void)file;
    (void)line;
    abort();
}

__attribute__((weak)) unsigned rtos_stdcpp_runtime_cycles(void) {
    return 0;
}

int rtos_stdcpp_runtime_is_interrupt(void) {
    unsigned status;
#if __ARM_ARCH_PROFILE == 'M'
    __asm volatile("mrs %0, ipsr" : "=r"(status));
    return status != 0;
#else
#error "The optional newlib runtime currently requires an ARM M profile"
#endif
}
