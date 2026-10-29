// SPDX-License-Identifier: MIT
#include <rtos_stdcpp/runtime.h>
#include <rtos_stdcpp/thread_with_attributes.h>
#include <rtos_stdcpp/threadx.h>
#include <chrono>
#include <thread>

namespace {
TX_THREAD bootstrap;
alignas(8) unsigned char bootstrap_stack[16384];
volatile unsigned worker_result;

void application_thread(ULONG) {
    rtos_stdcpp::attributes attributes;
    attributes.task_name = "worker";
    attributes.stack_word_count = 2048;
    attributes.priority = 16;

    auto worker = rtos_stdcpp::std_thread(attributes, [] {
        worker_result = 42;
    });
    worker.join();

    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
} // namespace

extern "C" void tx_application_define(void*) {
    rtos_stdcpp_runtime_initialize();
    rtos_stdcpp_threadx_initialize();

    const UINT status = tx_thread_create(
        &bootstrap, const_cast<char*>("bootstrap"), application_thread, 0, bootstrap_stack,
        sizeof bootstrap_stack, 20, 20, TX_NO_TIME_SLICE, TX_AUTO_START);
    RTOS_STDCPP_RUNTIME_REQUIRE(status == TX_SUCCESS);
}

// A board supplies startup, vectors and _tx_initialize_low_level before calling this.
extern "C" void example_start(void) {
    tx_kernel_enter();
}
