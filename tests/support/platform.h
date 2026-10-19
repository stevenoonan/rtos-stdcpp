// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <thread>
#include <utility>
#ifdef RTOS_STDCPP_TEST_FREERTOS
#include <rtos_stdcpp/thread_with_attributes.h>
#endif
namespace rtos_test::platform {
[[noreturn]] void fatal(const char *, unsigned);
void idle_opportunity();
uint32_t millis();
void observe_stack();
#ifdef RTOS_STDCPP_TEST_FREERTOS
// The integration selects native priorities, stack units and task names.
free_rtos_std::attributes worker_attributes(unsigned tier, uint32_t stack_words);
#endif
template<class F> std::thread worker(unsigned tier, F &&function, uint32_t stack_words = 0) {
#ifdef RTOS_STDCPP_TEST_FREERTOS
    return free_rtos_std::std_thread(worker_attributes(tier, stack_words), std::forward<F>(function));
#else
    (void)tier; (void)stack_words;
    return std::thread(std::forward<F>(function));
#endif
}
}
