// SPDX-License-Identifier: MIT
#pragma once
#include "FreeRTOS.h"
#include "task.h"
namespace rtos_test::regression_platform {
inline void idle_ticks(unsigned ticks) { vTaskDelay(ticks); }
inline void delay_ms(unsigned ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
}
#ifdef RTOS_STDCPP_TEST_FREERTOS
#include "runner.h"
#define REGRESSION_CHECK(expression) TEST_REQUIRE(expression)
#else
#define REGRESSION_CHECK(expression) configASSERT(expression)
#endif
