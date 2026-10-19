// SPDX-License-Identifier: MIT
#include <rtos_stdcpp/thread_with_attributes.h>
#include <rtos_stdcpp/freertos_time.h>
#include <condition_variable>
#include <mutex>
static_assert(__cplusplus == 201703L, "Qualification uses explicit C++17");
void public_api_compile_check() {
    auto thread = free_rtos_std::std_thread(free_rtos_std::attr_stack_size(512U), [] {});
    thread.join();
    std::timed_mutex mutex;
    if (mutex.try_lock_for(std::chrono::milliseconds(1))) mutex.unlock();
    std::condition_variable condition;
    condition.notify_one();
}
