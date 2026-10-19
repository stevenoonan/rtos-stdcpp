// SPDX-License-Identifier: MIT
#include "suite.h"
#include "platform.h"
#include "console.h"
#include "ca9_global_timer.h"
#include "FreeRTOS.h"
#include "task.h"
#include "freertos_time.h"
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <malloc.h>
#include <new>
using namespace std::chrono_literals;
extern "C" volatile unsigned rtos_test_idle_count;
namespace {
volatile unsigned startup_seed = 42;
unsigned startup_value() { static unsigned value = startup_seed; return value; }
struct Startup { Startup() { value = startup_value(); void *p = malloc(16); allocation = p != nullptr; free(p); } unsigned value; bool allocation; } startup;
void idle() {
    auto before = rtos_test_idle_count;
    vTaskDelay(3);
    TEST_REQUIRE(rtos_test_idle_count != before);
}

}
int main() {
    using namespace rtos_test;
    TimeOut_t initial; vTaskSetTimeOutState(&initial);
    auto initial_steady = std::chrono::steady_clock::now();
    // Measure with the independent hardware counter: the RTOS tick must not
    // advance inside a critical section even across several timer periods.
    taskENTER_CRITICAL();
    auto masked_tick = xTaskGetTickCount();
    auto timer_start = GlobTimer_Counter();
    while (GlobTimer_Counter() - timer_start < 3 * (100000000ULL / configTICK_RATE_HZ)) {}
    const bool tick_stayed_masked = xTaskGetTickCount() == masked_tick;
    taskEXIT_CRITICAL();
    Report warm; reset(warm, Suite::cxx17); Metrics metrics;
    for (unsigned i = 0; i < 4; ++i) run_shared_round(warm, metrics);
    run_inherited_cases(warm);
    vTaskDelay(3);
    const auto kernel_after_warmup = xPortGetFreeHeapSize();
    const auto cpp_after_warmup = mallinfo().uordblks;
    bool recovered = true;
    for (unsigned round = 0; round < 64; ++round) {
        run_shared_round(warm, metrics);
        vTaskDelay(3);
        recovered = recovered && xPortGetFreeHeapSize() == kernel_after_warmup
            && mallinfo().uordblks == cpp_after_warmup;
    }
    bool pass = finish(warm);
    // Warm-up is validated independently as part of the resource gate.
    Report report; reset(report, Suite::a9);
    begin(report, Case::runtime_startup);
    TEST_REQUIRE(tick_stayed_masked);
    TEST_REQUIRE(pass && startup.value == 42 && startup.allocation);
    TEST_REQUIRE(startup_value() == 42 && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
    end(report, Case::runtime_startup);
    begin(report, Case::time_clocks_sleep);
    using Clock = std::chrono::steady_clock;
#ifdef RTOS_STDCPP_A9_TICK_WRAP
    TimeOut_t after; vTaskSetTimeOutState(&after);
    TEST_REQUIRE(initial.xTimeOnEntering >= 0xffffffe2U && initial.xOverflowCount == 0);
    TEST_REQUIRE(after.xOverflowCount == 1 && after.xTimeOnEntering < 20000);
    TEST_REQUIRE(Clock::now() > initial_steady && Clock::now() - initial_steady < 20s);
#else
    (void)initial; (void)initial_steady;
#endif
    auto before = Clock::now();
    std::this_thread::sleep_for(-1ms); std::this_thread::sleep_for(0ms);
    TEST_REQUIRE(Clock::now() - before < 20ms);
    for (auto duration : {1ns, 1000001ns, 100000000ns}) {
        before = Clock::now(); std::this_thread::sleep_for(duration);
        TEST_REQUIRE(Clock::now() - before >= duration && Clock::now() - before < duration + 50ms);
    }
    auto wall = std::chrono::system_clock::now(); before = Clock::now();
    SetSystemClockTime(wall + 24h);
    TEST_REQUIRE(std::chrono::system_clock::now() >= wall + 24h);
    TEST_REQUIRE(Clock::now() - before < 20ms);
    SetSystemClockTime(wall - 24h);
    TEST_REQUIRE(std::chrono::system_clock::now() < wall - 23h);
    std::this_thread::sleep_until(Clock::now() + 3ms);
    TEST_REQUIRE(Clock::now() - before >= 3ms && Clock::now() - before < 50ms);
    SetSystemClockTime(wall);
    end(report, Case::time_clocks_sleep);
    begin(report, Case::condition_notification_isolation);
    std::mutex mutex; std::condition_variable cv; bool ready = false, done = false;
    auto child = platform::worker(1, [&] {
        xTaskNotifyGiveIndexed(xTaskGetCurrentTaskHandle(), 1);
        std::unique_lock<std::mutex> held(mutex); ready = true; cv.notify_one();
        TEST_REQUIRE(cv.wait_for(held, 100ms, [&] { return done; }));
        TEST_EQ(1U, ulTaskNotifyTakeIndexed(1, pdTRUE, 0));
    });
    { std::unique_lock<std::mutex> held(mutex); TEST_REQUIRE(cv.wait_for(held, 100ms, [&] { return ready; })); done = true; cv.notify_one(); }
    child.join(); end(report, Case::condition_notification_isolation);
    begin(report, Case::runtime_resources);
    TEST_REQUIRE(recovered);
    idle(); auto kernel = xPortGetFreeHeapSize(); auto used = mallinfo().uordblks;
    for (unsigned i = 0; i < 64; ++i) {
        // Separate small/big and aligned recoverable allocation forms.
        auto *p = new(std::nothrow) unsigned char[257]; TEST_REQUIRE(p); delete[] p;
        auto *q = ::operator new(128, std::align_val_t(64), std::nothrow);
        TEST_REQUIRE(q && (uintptr_t(q) % 64 == 0)); ::operator delete(q, std::align_val_t(64));
        auto worker = platform::worker(i % 3, [] { std::mutex m; std::lock_guard<std::mutex> lock(m); });
        worker.join(); idle();
        TEST_EQ(kernel, xPortGetFreeHeapSize()); TEST_EQ(used, mallinfo().uordblks);
    }
    auto *oom = ::operator new(1024 * 1024, std::nothrow); TEST_REQUIRE(oom == nullptr);
    auto *aligned_oom = ::operator new(1024 * 1024, std::align_val_t(64), std::nothrow); TEST_REQUIRE(aligned_oom == nullptr);
    TEST_REQUIRE(uxTaskGetStackHighWaterMark(nullptr) >= 128);
    end(report, Case::runtime_resources);
    pass = finish(report);
    write_report(report, [](const char *text) { print(text); });
    std::_Exit(pass ? 0 : 1);
}
