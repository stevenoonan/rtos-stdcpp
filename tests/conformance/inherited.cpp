/// Copyright 2018-2023 Piotr Grygorczuk <grygorek@gmail.com>
/// Copyright 2023 by NXP. All rights reserved.
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in all
/// copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
/// THE SOFTWARE.

// Release-safe replacements for inherited mutex/CV/once/attribute cases.
#include "suite.h"
#include "platform.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
using namespace std::chrono_literals;
namespace rtos_test {
namespace {
void recursive_timed() {
    std::recursive_timed_mutex recursive;
    TEST_REQUIRE(recursive.try_lock_for(10ms));
    TEST_REQUIRE(recursive.try_lock_for(10ms));
    auto denied = platform::worker(1, [&] { TEST_ASSERT(!recursive.try_lock_for(3ms)); });
    denied.join(); recursive.unlock(); recursive.unlock();
    TEST_REQUIRE(recursive.try_lock_until(std::chrono::steady_clock::now() + 20ms));
    recursive.unlock();
    std::timed_mutex mutex;
    mutex.lock();
    auto contender = platform::worker(1, [&] {
        // Try-locking one's own nonrecursive mutex is not a portable test.
        TEST_ASSERT(!mutex.try_lock());
        TEST_ASSERT(!mutex.try_lock_for(3ms));
        TEST_ASSERT(!mutex.try_lock_until(std::chrono::system_clock::now() + 3ms));
    });
    contender.join(); mutex.unlock();
    TEST_REQUIRE(mutex.try_lock_for(10ms)); mutex.unlock();
}
void condition_any() {
    std::mutex mutex; std::condition_variable_any condition;
    bool ready = false;
    auto producer = platform::worker(1, [&] {
        std::lock_guard<std::mutex> lock(mutex);
        ready = true; condition.notify_one();
    });
    {
        std::unique_lock<std::mutex> lock(mutex);
        TEST_REQUIRE(condition.wait_for(lock, 500ms, [&] { return ready; }));
        TEST_ASSERT(!condition.wait_for(lock, 3ms, [] { return false; }));
    }
    producer.join();
}
void call_once() {
    std::once_flag flag; std::atomic<unsigned> count{0};
    auto access = [&] { std::call_once(flag, [&] { ++count; }); };
    auto a = platform::worker(0, access), b = platform::worker(1, access), c = platform::worker(2, access);
    a.join(); b.join(); c.join(); TEST_EQ(1U, count.load());
}
void attributes() {
    std::atomic<unsigned> checksum{0};
    auto thread = platform::worker(1, [&] {
        volatile unsigned char bytes[3072];
        for (unsigned i = 0; i < 3072; ++i) bytes[i] = i;
        // Volatile accesses preserve the stack workload in Release builds.
        for (unsigned i = 0; i < 3072; ++i) checksum.fetch_add(bytes[i], std::memory_order_relaxed);
        platform::observe_stack();
    }, 1536);
    auto id = thread.get_id();
    std::thread moved;
    moved = std::move(thread);
    TEST_ASSERT(!thread.joinable() && moved.get_id() == id);
    moved.join();
    TEST_EQ(12U * 32640U, checksum.load());
}
}
void run_inherited_cases(Report &report) {
    begin(report, Case::mutex_recursive_timed); recursive_timed(); end(report, Case::mutex_recursive_timed);
    begin(report, Case::condition_any); condition_any(); end(report, Case::condition_any);
    begin(report, Case::once_concurrent); call_once(); end(report, Case::once_concurrent);
    begin(report, Case::thread_move_stack_workload); attributes(); end(report, Case::thread_move_stack_workload);
}
}
