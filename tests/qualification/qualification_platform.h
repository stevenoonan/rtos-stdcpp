// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>

namespace rtos_test::qualification {
struct Resources {
    size_t cpp_used;
    size_t kernel_free;
    unsigned threads;
    unsigned tls_live;
};

Resources resources();
void reclaim();
uint32_t kernel_ticks();
void prepare_ticks();
void native_worker(void (*entry)());
// Implemented by the machine using a hardware counter while kernel IRQs are masked.
bool masked_tick_probe();
} // namespace rtos_test::qualification

extern "C" volatile uint32_t rtos_test_tick_count;
