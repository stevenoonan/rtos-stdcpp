// SPDX-License-Identifier: MIT
#pragma once

#define TX_SINGLE_MODE_SECURE
#define TX_TIMER_TICKS_PER_SECOND 1000
#define TX_ENABLE_STACK_CHECKING
#define TX_ENABLE_EXECUTION_CHANGE_NOTIFY
#define TX_TIMER_THREAD_STACK_SIZE 2048

#include <rtos_stdcpp/threadx_runtime_config.h>
