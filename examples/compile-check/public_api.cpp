// SPDX-License-Identifier: MIT
#include <rtos_stdcpp/thread_with_attributes.h>
#include <rtos_stdcpp/freertos_time.h>
#include <condition_variable>
#include <mutex>
#include <future>
namespace {
struct TlsObject { unsigned value = 42; ~TlsObject() { value = 0; } };
}
static_assert(__cplusplus == 201703L || __cplusplus == 202002L);
#if __cplusplus >= 202002L
#include <stop_token>
#include <semaphore>
#include <latch>
#include <barrier>
#include <atomic>
void cxx20_public_api_compile_check() {
    auto thread = free_rtos_std::std_jthread(free_rtos_std::attr_stack_size(512U), [](std::stop_token) {});
    thread.request_stop(); thread.join();
    std::atomic<unsigned> value{1}; value.wait(0); value.notify_all();
    std::binary_semaphore sem(1); sem.acquire(); sem.release();
    std::latch latch(1); latch.arrive_and_wait();
    std::barrier barrier(1); barrier.arrive_and_wait();
}
#endif
void public_api_compile_check() {
    auto thread = free_rtos_std::std_thread(free_rtos_std::attr_stack_size(512U), [] {
        static thread_local TlsObject object;
        ++object.value;
    });
    thread.join();
    std::timed_mutex mutex;
    if (mutex.try_lock_for(std::chrono::milliseconds(1))) mutex.unlock();
    std::condition_variable condition;
    condition.notify_one();
}

void promise_future_compile_check() {
    std::promise<void> promise;
    auto completion = promise.get_future();
    promise.set_value();
    completion.wait_for(std::chrono::milliseconds(1));
    completion.get();
}
