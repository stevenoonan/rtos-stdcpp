// SPDX-License-Identifier: MIT
#pragma once

// Include from tx_user.h before tx_api.h. Append application-specific fields to
// TX_THREAD_USER_EXTENSION by using this macro as the first field group.
#define RTOS_STDCPP_THREADX_THREAD_EXTENSION void* rtos_stdcpp_context;
#ifndef TX_THREAD_USER_EXTENSION
#define TX_THREAD_USER_EXTENSION RTOS_STDCPP_THREADX_THREAD_EXTENSION
#endif
