// SPDX-License-Identifier: MIT
#pragma once
extern "C" {
#include "FreeRTOS.h"
}
#if !configUSE_NEWLIB_REENTRANT || configNUM_THREAD_LOCAL_STORAGE_POINTERS < 1
#error "The C++ threading adapter requires newlib task reentrancy and TLS slot 0"
#endif
// All C++ translation units must agree on the native timed-wait hooks.
#define _GLIBCXX_USE_PTHREAD_COND_CLOCKWAIT 1
#define _GLIBCXX_USE_PTHREAD_MUTEX_CLOCKLOCK 1
