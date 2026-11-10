// SPDX-License-Identifier: MIT
#pragma once
#ifdef RTOS_STDCPP_BACKEND_THREADX
#include "tx_api.h"
#include "runner.h"

namespace rtos_test::regression_platform {
inline void idle_ticks(unsigned ticks) {
    TEST_REQUIRE(tx_thread_sleep(ticks) == TX_SUCCESS);
}

inline void delay_ms(unsigned ms) {
    idle_ticks((ms * TX_TIMER_TICKS_PER_SECOND + 999) / 1000);
}
} // namespace rtos_test::regression_platform

#define REGRESSION_CHECK(expression) TEST_REQUIRE(expression)
#else
#include "freertos/regression_platform.h"
#endif
