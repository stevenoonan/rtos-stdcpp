// SPDX-License-Identifier: MIT
#pragma once

#include "tx_api.h"
#include <cstdint>

namespace rtos_stdcpp {
constexpr ULONG deadline_ticks(int64_t ns) {
    constexpr uint64_t quantum = 1000000000ULL / TX_TIMER_TICKS_PER_SECOND;
    static_assert(1000000000ULL % TX_TIMER_TICKS_PER_SECOND == 0);
    if (ns <= 0) {
        return 0;
    }
    auto ticks = uint64_t(ns) / quantum + (uint64_t(ns) % quantum != 0) + 1;
    return ULONG(ticks > 0x7fffffffULL ? 0x7fffffffULL : ticks);
}

int64_t monotonic_ns();
int64_t realtime_ns();
void sleep_ns(int64_t);
} // namespace rtos_stdcpp

extern "C" void rtos_stdcpp_threadx_tick();
