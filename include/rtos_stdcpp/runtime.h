// SPDX-License-Identifier: MIT
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
void rtos_stdcpp_runtime_initialize(void);

void rtos_stdcpp_runtime_lock(void);

void rtos_stdcpp_runtime_unlock(void);

/* Static initializers may block; these locks must permit scheduling. */
void rtos_stdcpp_guard_lock(void);

void rtos_stdcpp_guard_unlock(void);

int rtos_stdcpp_runtime_is_interrupt(void);

unsigned rtos_stdcpp_runtime_cycles(void);

#ifdef __cplusplus
[[noreturn]]
#else
_Noreturn
#endif
void rtos_stdcpp_runtime_fail(const char* message, const char* file, unsigned line);

extern volatile uint32_t rtos_stdcpp_newlib_max_lock_cycles;
extern volatile uint32_t rtos_stdcpp_newlib_lock_calls;

#ifdef __cplusplus
}
#endif

#define RTOS_STDCPP_RUNTIME_REQUIRE(condition)                                                     \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            rtos_stdcpp_runtime_fail(#condition, __FILE__, __LINE__);                              \
        }                                                                                          \
    } while (0)
