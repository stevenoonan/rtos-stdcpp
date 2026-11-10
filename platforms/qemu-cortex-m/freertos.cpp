// SPDX-License-Identifier: MIT
#include "FreeRTOS.h"
#include "task.h"
#include "thread_gthread.h"
#include <rtos_stdcpp/thread_with_attributes.h>
#include <rtos_stdcpp/runtime.h>
#include <cstdlib>
extern "C" volatile uint32_t rtos_test_tick_count;
extern "C" volatile unsigned rtos_test_idle_count;
int main();

extern "C" void rtos_test_assert_fail(const char* file, unsigned line) {
    rtos_stdcpp_runtime_fail("FreeRTOS assertion", file, line);
}

extern "C" void vApplicationMallocFailedHook() {
    std::_Exit(87);
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t, char*) {
    std::_Exit(88);
}

extern "C" void vApplicationIdleHook() {
    rtos_test_idle_count = rtos_test_idle_count + 1;
}

extern "C" void vApplicationTickHook() {
    rtos_test_tick_count = rtos_test_tick_count + 1;
}

extern "C" void vApplicationGetIdleTaskMemory(StaticTask_t** tcb, StackType_t** stack,
                                              uint32_t* words) {
    static StaticTask_t idle;
    static StackType_t storage[configMINIMAL_STACK_SIZE];
    *tcb = &idle;
    *stack = storage;
    *words = configMINIMAL_STACK_SIZE;
}

[[noreturn]] void start_scheduler() {
    rtos_stdcpp_runtime_initialize();
    if (xTaskCreate(
            [](void*) {
                auto test = rtos_stdcpp::std_thread(rtos_stdcpp::attr_stack_size(8192), [] {
                    std::_Exit(main());
                });
                test.join();
                std::_Exit(90);
            },
            "bootstrap", 4096, nullptr, 2, nullptr) != pdPASS) {
        std::_Exit(90);
    }
    vTaskStartScheduler();
    std::_Exit(90);
}
