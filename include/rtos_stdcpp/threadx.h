// SPDX-License-Identifier: MIT
#pragma once

#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif
// Call from tx_application_define, after ThreadX initializes object lists.
void rtos_stdcpp_threadx_initialize(void);

// Call exactly once per kernel timer interrupt, before _tx_timer_interrupt.
void rtos_stdcpp_threadx_tick(void);

// Optional application hook. Runs on the reaper before deleting a completed TCB.
void rtos_stdcpp_threadx_thread_cleanup(TX_THREAD* thread);

#ifdef __cplusplus
}
#endif
