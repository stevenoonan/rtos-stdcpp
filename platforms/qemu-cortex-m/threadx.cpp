// SPDX-License-Identifier: MIT
#include "thread_gthread.h"
#include <rtos_stdcpp/thread_with_attributes.h>
#include <rtos_stdcpp/runtime.h>
#include <cstdlib>
extern "C" unsigned char _ebss[], _estack[];
extern "C" void* _tx_initialize_unused_memory;
extern "C" volatile uint32_t rtos_test_tick_count;
extern "C" void rtos_stdcpp_threadx_tick();
int main();

namespace {
TX_THREAD bootstrap;
alignas(8) unsigned char bootstrap_stack[16384];

volatile uint32_t& reg(uintptr_t address) {
    return *reinterpret_cast<volatile uint32_t*>(address);
}
} // namespace

extern "C" void _tx_initialize_low_level() {
    _tx_initialize_unused_memory = _ebss;
    _tx_thread_system_stack_ptr = _estack;
    reg(0xe000ed08) = 0;
    reg(0xe000ed1c) = 0xff000000;
    reg(0xe000ed20) = 0x40ff0000;
    reg(0xe000e014) = RTOS_STDCPP_CPU_HZ / 1000 - 1;
    reg(0xe000e018) = 0;
    reg(0xe000e010) = 7;
}

extern "C" void rtos_test_threadx_tick() {
    rtos_test_tick_count = rtos_test_tick_count + 1;
    rtos_stdcpp_threadx_tick();
}

extern "C" void tx_application_define(void*) {
    rtos_stdcpp_runtime_initialize();
    rtos_stdcpp_threadx_initialize();
    rtos_stdcpp::check(tx_thread_create(
        &bootstrap, const_cast<char*>("bootstrap"),
        [](ULONG) {
            auto test = rtos_stdcpp::std_thread(rtos_stdcpp::attr_stack_size(8192), [] {
                std::_Exit(main());
            });
            test.join();
            std::_Exit(90);
        },
        0, bootstrap_stack, sizeof(bootstrap_stack), 24, 24, TX_NO_TIME_SLICE, TX_AUTO_START));
}

[[noreturn]] void start_scheduler() {
    tx_kernel_enter();
    std::_Exit(90);
}
