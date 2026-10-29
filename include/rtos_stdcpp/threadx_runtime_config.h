#pragma once

#ifndef __ASSEMBLER__
#include <reent.h>
#include "threadx_config.h"

#undef TX_THREAD_USER_EXTENSION
#define TX_THREAD_USER_EXTENSION                                                                   \
    RTOS_STDCPP_THREADX_THREAD_EXTENSION                                                           \
    struct _reent rtos_stdcpp_reent;                                                               \
    unsigned rtos_stdcpp_reent_initialized;
#endif
