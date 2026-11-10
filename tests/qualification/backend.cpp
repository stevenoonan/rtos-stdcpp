// SPDX-License-Identifier: MIT
#include "qualification_platform.h"
#include "platform.h"
#include "runner.h"
#include "thread_gthread.h"
#include <chrono>
#include <cstdlib>
#include <malloc.h>
#ifdef RTOS_STDCPP_BACKEND_THREADX
#include "tx_timer.h"
extern "C" void rtos_stdcpp_threadx_test_seed_ticks(uint64_t);
#else
#include "FreeRTOS.h"
#include "task.h"
#endif
#if RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR
extern "C" unsigned rtos_test_tls_live_allocations();
#endif

namespace rtos_test::qualification {
Resources resources() {
    Resources result{};
    result.cpp_used = mallinfo().uordblks;
#ifdef RTOS_STDCPP_BACKEND_THREADX
    result.threads = _tx_thread_created_count;
#else
    result.kernel_free = xPortGetFreeHeapSize();
    result.threads = uxTaskGetNumberOfTasks();
#endif
#if RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR
    result.tls_live = rtos_test_tls_live_allocations();
#endif
    return result;
}

void reclaim() {
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
}

uint32_t kernel_ticks() {
#ifdef RTOS_STDCPP_BACKEND_THREADX
    return tx_time_get();
#else
    return xTaskGetTickCount();
#endif
}

void prepare_ticks() {
#if RTOS_STDCPP_TEST_TICK_WRAP && defined(RTOS_STDCPP_BACKEND_THREADX)
    rtos_stdcpp_threadx_test_seed_ticks(UINT32_MAX - 30ULL);
#endif
}

void native_worker(void (*entry)()) {
#ifdef RTOS_STDCPP_BACKEND_THREADX
    static TX_THREAD native;
    alignas(8) static unsigned char stack[8192];
    if (tx_thread_create(
            &native, const_cast<char*>("native-probe"),
            [](ULONG value) {
                reinterpret_cast<void (*)()>(value)();
                std::_Exit(90);
            },
            reinterpret_cast<ULONG>(entry), stack, sizeof(stack), 12, 12, TX_NO_TIME_SLICE,
            TX_AUTO_START) != TX_SUCCESS) {
        std::_Exit(90);
    }
#else
    if (xTaskCreate(
            [](void* value) {
                reinterpret_cast<void (*)()>(value)();
                std::_Exit(90);
            },
            "native-probe", 2048, reinterpret_cast<void*>(entry), 5, nullptr) != pdPASS) {
        std::_Exit(90);
    }
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::_Exit(90);
}
} // namespace rtos_test::qualification

namespace rtos_test::platform {
[[noreturn]] void fatal(const char* file, unsigned line);

void idle_opportunity() {
#ifdef RTOS_STDCPP_BACKEND_THREADX
    rtos_stdcpp::check(tx_thread_sleep(1));
#else
    vTaskDelay(1);
#endif
}

uint32_t millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void observe_stack() {
#ifdef RTOS_STDCPP_BACKEND_THREADX
    auto* t = tx_thread_identify();
    auto* p = static_cast<ULONG*>(t->tx_thread_stack_start);
    auto* end = static_cast<ULONG*>(t->tx_thread_stack_end);
    unsigned untouched = 0;
    while (p < end && *p++ == 0xefefefef) {
        ++untouched;
    }
    TEST_REQUIRE(untouched >= 128);
#else
    TEST_REQUIRE(uxTaskGetStackHighWaterMark(nullptr) >= 128);
#endif
}

rtos_stdcpp::attributes worker_attributes(unsigned tier, uint32_t words) {
    rtos_stdcpp::attributes result{};
#ifdef RTOS_STDCPP_BACKEND_THREADX
    result.priority = 20 - 4 * tier;
#else
    result.priority = 1 + 2 * tier;
#endif
    result.task_name = "test-worker";
    if (words) {
        result.stack_word_count = words;
    }
    return result;
}
} // namespace rtos_test::platform
