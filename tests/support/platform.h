// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <thread>
#include <utility>

#ifdef RTOS_STDCPP_TEST_EMBEDDED
#include <rtos_stdcpp/thread_with_attributes.h>
#endif
namespace rtos_test::platform {
[[noreturn]] void fatal(const char*, unsigned);
void idle_opportunity();
uint32_t millis();
void observe_stack();
#if __cplusplus >= 202002L
template <class F> std::jthread jworker(unsigned tier, F&& function, uint32_t stack_words = 0);
#endif
#ifdef RTOS_STDCPP_TEST_EMBEDDED
// The integration selects native priorities, stack units and task names.
rtos_stdcpp::attributes worker_attributes(unsigned tier, uint32_t stack_words);
#endif
template <class F> std::thread worker(unsigned tier, F&& function, uint32_t stack_words = 0) {
#ifdef RTOS_STDCPP_TEST_EMBEDDED
    return rtos_stdcpp::std_thread(worker_attributes(tier, stack_words), std::forward<F>(function));
#else
    (void)tier;
    (void)stack_words;
    return std::thread(std::forward<F>(function));
#endif
}
#if __cplusplus >= 202002L
template <class F> std::jthread jworker(unsigned tier, F&& function, uint32_t stack_words) {
#ifdef RTOS_STDCPP_TEST_EMBEDDED
    return rtos_stdcpp::std_jthread(worker_attributes(tier, stack_words),
                                    std::forward<F>(function));
#else
    (void)tier;
    (void)stack_words;
    return std::jthread(std::forward<F>(function));
#endif
}
#endif
} // namespace rtos_test::platform
