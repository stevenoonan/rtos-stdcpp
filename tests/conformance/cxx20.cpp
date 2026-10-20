// SPDX-License-Identifier: MIT
#include "suite.h"
#include "platform.h"
#include <array>
#include <atomic>
#include <barrier>
#include <condition_variable>
#include <latch>
#include <mutex>
#include <semaphore>
#include <stop_token>
#include <thread>
#include <utility>

static_assert(__cplusplus == 202002L);
static_assert(__cpp_lib_jthread >= 201911L && __cpp_lib_atomic_wait >= 201907L);
static_assert(__cpp_lib_semaphore >= 201907L && __cpp_lib_latch >= 201907L);
static_assert(__cpp_lib_barrier >= 201907L);
static_assert(std::atomic<unsigned>::is_always_lock_free && std::atomic<void *>::is_always_lock_free);
using namespace std::chrono_literals;
namespace {
template<class S> void take(S &sem) { TEST_REQUIRE(sem.try_acquire_for(2s)); }
template<class F> std::thread checked_worker(unsigned tier, F &&function) {
    return rtos_test::platform::worker(tier, [body = std::forward<F>(function)]() mutable {
        body(); rtos_test::platform::observe_stack();
    }, 1024);
}

void jthreads() {
    std::binary_semaphore entered(0), exited(0);
    std::atomic<unsigned> observed{0};
    {
        auto first = rtos_test::platform::jworker(1, [&](std::stop_token stop) {
#ifdef RTOS_STDCPP_TEST_FREERTOS
            const auto expected = rtos_test::platform::worker_attributes(1, 1024);
            TEST_EQ(expected.priority, uxTaskPriorityGet(nullptr));
#endif
            rtos_test::platform::observe_stack();
            entered.release();
            std::mutex mutex; std::condition_variable_any cv;
            std::unique_lock lock(mutex);
            TEST_REQUIRE(!cv.wait_for(lock, stop, 2s, [] { return false; }));
            TEST_REQUIRE(stop.stop_requested());
            ++observed; rtos_test::platform::observe_stack(); exited.release();
        }, 1024);
        take(entered);
        auto token = first.get_stop_token();
        std::jthread second(std::move(first));
        TEST_REQUIRE(!first.joinable() && second.joinable() && token.stop_possible());
        // Destruction requests stop and joins before the surrounding scope ends.
    }
    take(exited); TEST_EQ(1U, observed.load());
    auto explicit_stop = rtos_test::platform::jworker(0, [](std::stop_token stop) {
        while (!stop.stop_requested()) std::this_thread::sleep_for(1ms);
    });
    TEST_REQUIRE(explicit_stop.request_stop());
    TEST_REQUIRE(!explicit_stop.request_stop()); explicit_stop.join();
    auto target = rtos_test::platform::jworker(0, [] {});
    auto replacement = rtos_test::platform::jworker(1, [] {});
    target = std::move(replacement);
    TEST_REQUIRE(!replacement.joinable() && target.joinable()); target.join();
}

void callbacks() {
    std::stop_source empty(std::nostopstate);
    TEST_REQUIRE(!empty.stop_possible() && !empty.request_stop());
    std::stop_token orphan;
    { std::stop_source source; orphan = source.get_token(); TEST_REQUIRE(orphan.stop_possible()); }
    TEST_REQUIRE(!orphan.stop_possible() && !orphan.stop_requested());
    std::stop_source source; auto copy = source;
    auto token = source.get_token(); unsigned called = 0;
    {
        std::stop_callback removed(token, [&] { called += 100; });
    }
    {
        std::stop_callback kept(token, [&] { ++called; });
        TEST_REQUIRE(copy.request_stop()); TEST_EQ(1U, called);
        TEST_REQUIRE(source.stop_requested() && token.stop_requested());
        TEST_REQUIRE(!source.request_stop());
        const auto caller = std::this_thread::get_id();
        std::stop_callback immediate(token, [&] {
            TEST_REQUIRE(std::this_thread::get_id() == caller); ++called;
        });
        TEST_EQ(2U, called);
    }
    // Destroy a callback while its invocation is running on another task.
    std::stop_source concurrent;
    std::binary_semaphore running(0), release(0), removing(0), removed(0);
    std::atomic<bool> done{false};
    auto callback = [&] { running.release(); take(release); done.store(true); };
    using Callback = std::stop_callback<decltype(callback)>;
    auto *registered = new Callback(concurrent.get_token(), callback);
    auto requester = checked_worker(0, [&] { TEST_REQUIRE(concurrent.request_stop()); });
    take(running);
    auto remover = checked_worker(2, [&] {
        removing.release(); delete registered;
        TEST_REQUIRE(done.load()); removed.release();
    });
    take(removing); TEST_REQUIRE(!removed.try_acquire());
    release.release(); requester.join(); remover.join(); take(removed);
}

#ifdef RTOS_STDCPP_TEST_STOP_LOCK
std::atomic<TaskHandle_t> armed_owner{nullptr};
std::atomic<unsigned> backoffs{0};
std::binary_semaphore *contender_ready = nullptr;
#endif
void priority_progress() {
#ifdef RTOS_STDCPP_TEST_STOP_LOCK
    std::stop_source source;
    std::binary_semaphore start(0), finished(0);
    contender_ready = &start; backoffs.store(0);
    auto high = checked_worker(2, [&] {
        take(start); TEST_REQUIRE(source.request_stop()); finished.release();
    });
    auto low = checked_worker(0, [&] {
        armed_owner.store(xTaskGetCurrentTaskHandle());
        std::stop_callback callback(source.get_token(), [] {});
    });
    take(finished); low.join(); high.join();
    TEST_REQUIRE(backoffs.load() > 0); contender_ready = nullptr;
#else
    // Host scheduling has no FreeRTOS priority tiers; exercise shared stop-state contention.
    std::stop_source source;
    auto worker = checked_worker(0, [&] {
        for (unsigned i = 0; i < 64; ++i) { std::stop_callback cb(source.get_token(), [] {}); }
    });
    TEST_REQUIRE(source.request_stop()); worker.join();
#endif
}

void stop_waits() {
    for (unsigned mode = 0; mode < 3; ++mode) {
        std::mutex mutex; std::condition_variable_any cv; std::stop_source source;
        std::binary_semaphore entered(0);
        auto worker = checked_worker(1, [&] {
            std::unique_lock lock(mutex); entered.release(); bool result;
            if (mode == 0) result = cv.wait(lock, source.get_token(), [] { return false; });
            else if (mode == 1) result = cv.wait_for(lock, source.get_token(), 2s, [] { return false; });
            else result = cv.wait_until(lock, source.get_token(), std::chrono::steady_clock::now()+2s, [] { return false; });
            TEST_REQUIRE(!result && lock.owns_lock() && source.stop_requested());
        });
        take(entered);
        { std::lock_guard lock(mutex); } // The worker released the mutex to enter its wait.
        TEST_REQUIRE(source.request_stop()); worker.join();
        std::unique_lock lock(mutex);
        TEST_REQUIRE(!cv.wait(lock, source.get_token(), [] { return false; }));
        TEST_REQUIRE(cv.wait(lock, source.get_token(), [] { return true; }));
    }
    for (unsigned n = 0; n < 4; ++n) {
        std::mutex mutex; std::condition_variable_any cv; std::stop_source source;
        bool ready = false;
        auto worker = checked_worker(1, [&] {
            { std::lock_guard lock(mutex); ready = true; }
            cv.notify_all(); source.request_stop();
        });
        { std::unique_lock lock(mutex);
          bool result = cv.wait_for(lock, source.get_token(), 2s, [&] { return ready; });
          TEST_EQ(ready, result); }
        worker.join();
    }
    std::mutex mutex; std::condition_variable_any cv; std::stop_source source;
    std::unique_lock lock(mutex); auto before = std::chrono::steady_clock::now();
    TEST_REQUIRE(!cv.wait_for(lock, source.get_token(), 2ms, [] { return false; }));
    TEST_REQUIRE(std::chrono::steady_clock::now()-before >= 2ms);
}

template<class T> void atomic_type(T original, T next) {
    static_assert(std::atomic<T>::is_always_lock_free);
    std::atomic<T> value{original};
    auto worker = checked_worker(1, [&] {
        value.wait(original, std::memory_order_acquire);
        TEST_EQ(next, value.load());
    });
    value.store(next, std::memory_order_release); value.notify_all(); worker.join();
    std::atomic_wait(&value, original);
}
void atomic_waits() {
    // Equal GCC hash buckets: verify notifications cannot complete another address's wait.
    static_assert(sizeof(std::atomic<unsigned>) == 4);
    alignas(64) std::array<std::atomic<unsigned>, 17> values{};
    TEST_EQ((reinterpret_cast<uintptr_t>(&values[0]) >> 2) % 16,
            (reinterpret_cast<uintptr_t>(&values[16]) >> 2) % 16);
    std::counting_semaphore<3> entered(0), finished(0);
    unsigned payload = 0;
    auto one = checked_worker(0, [&] {
        entered.release(); values[0].wait(0, std::memory_order_acquire);
        TEST_EQ(42U, payload); finished.release();
    });
    auto two = checked_worker(1, [&] {
        entered.release(); std::atomic_wait_explicit(&values[0], 0U, std::memory_order_acquire);
        TEST_EQ(42U, payload); finished.release();
    });
    auto collision = checked_worker(2, [&] {
        entered.release(); values[16].wait(0, std::memory_order_acquire);
        TEST_EQ(7U, values[16].load()); finished.release();
    });
    take(entered); take(entered); take(entered);
    values[0].notify_one(); TEST_REQUIRE(!finished.try_acquire_for(2ms));
    payload = 42; values[0].store(1, std::memory_order_release);
    std::atomic_notify_all(&values[0]); take(finished); take(finished);
    TEST_REQUIRE(!finished.try_acquire());
    values[16].store(7, std::memory_order_release); values[16].notify_one(); take(finished);
    one.join(); two.join(); collision.join();
    values[0].wait(0); std::atomic_wait(&values[0], 0U); // already changed
    std::atomic_flag flag{};
    auto flag_worker = checked_worker(1, [&] { flag.wait(false); TEST_REQUIRE(flag.test()); });
    flag.test_and_set(std::memory_order_release); flag.notify_all(); flag_worker.join();
    int object = 9; std::atomic<int *> pointer{nullptr};
    auto pointer_worker = checked_worker(0, [&] { pointer.wait(nullptr); TEST_EQ(9, *pointer.load()); });
    pointer.store(&object); pointer.notify_one(); pointer_worker.join();
    atomic_type<int>(-1, 7); atomic_type<uint8_t>(0, 3); atomic_type<uint16_t>(0, 9);
    std::atomic_flag free_flag{};
    auto flag_free_worker = checked_worker(1, [&] {
        std::atomic_flag_wait_explicit(&free_flag, false, std::memory_order_acquire);
        TEST_REQUIRE(free_flag.test());
    });
    free_flag.test_and_set(std::memory_order_release);
    std::atomic_flag_notify_one(&free_flag); flag_free_worker.join();
}

void semaphores() {
    std::binary_semaphore binary(0);
    TEST_REQUIRE(!binary.try_acquire() && !binary.try_acquire_for(-1ms));
    auto before = std::chrono::steady_clock::now();
    TEST_REQUIRE(!binary.try_acquire_until(before + 2ms));
    TEST_REQUIRE(std::chrono::steady_clock::now() - before >= 2ms);
    auto worker = checked_worker(1, [&] { binary.release(); });
    take(binary); worker.join();
    std::counting_semaphore<4> counting(0);
    std::atomic<unsigned> count{0}; std::binary_semaphore entered(0);
    auto consumer = checked_worker(2, [&] {
        entered.release(); counting.acquire(); ++count; counting.acquire(); ++count;
    });
    take(entered); counting.release(3); consumer.join();
    TEST_EQ(2U, count.load()); take(counting);
    TEST_REQUIRE(!counting.try_acquire());
}

void latches() {
    std::latch latch(3); std::array<unsigned, 2> data{};
    auto first = checked_worker(0, [&] { data[0]=11; latch.count_down(); });
    auto second = checked_worker(2, [&] { data[1]=22; latch.arrive_and_wait(); });
    TEST_REQUIRE(!latch.try_wait()); latch.count_down(); latch.wait();
    TEST_REQUIRE(latch.try_wait()); TEST_EQ(11U,data[0]); TEST_EQ(22U,data[1]);
    first.join(); second.join();
}

void barriers() {
    std::atomic<unsigned> completions{0}; std::array<unsigned, 2> data{};
    auto completion = [&]() noexcept { ++completions; };
    std::barrier barrier(3, completion);
    auto first = checked_worker(0, [&] {
        data[0]=1; barrier.arrive_and_wait();
        TEST_EQ(1U,data[1]); barrier.arrive_and_drop();
    });
    auto second = checked_worker(2, [&] {
        data[1]=1; barrier.arrive_and_wait();
        TEST_EQ(1U,data[0]); barrier.arrive_and_wait();
    });
    auto arrival = barrier.arrive(); barrier.wait(std::move(arrival));
    TEST_REQUIRE(completions.load() >= 1); barrier.arrive_and_wait();
    first.join(); second.join(); TEST_EQ(2U,completions.load());
}
}
#ifdef RTOS_STDCPP_TEST_STOP_LOCK
extern "C" void rtos_stdcpp_test_stop_lock(bool acquired) {
    if (!acquired) { backoffs.fetch_add(1); return; }
    if (armed_owner.load() != xTaskGetCurrentTaskHandle()) return;
    armed_owner.store(nullptr);
    contender_ready->release();
    // Let the higher-priority contender observe the held stop-state lock.
    vTaskDelay(2);
}
#endif
namespace rtos_test {
void run_cxx20_round(Report &report) {
    struct Entry { Case id; void (*function)(); };
    for (auto entry : {Entry{Case::jthread_lifecycle,jthreads}, {Case::stop_callbacks,callbacks},
         {Case::stop_priority_progress,priority_progress}, {Case::condition_stop_wait,stop_waits},
         {Case::atomic_wait_notify,atomic_waits}, {Case::semaphore_operations,semaphores},
         {Case::latch_operations,latches}, {Case::barrier_phases,barriers}}) {
        begin(report,entry.id); entry.function(); end(report,entry.id);
    }
}
bool run_cxx20_suite(Report &report) {
    reset(report,Suite::cxx20); run_cxx20_round(report); return finish(report);
}
}
