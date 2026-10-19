// SPDX-License-Identifier: MIT
// Reusable standard API cases transferred from the integrating application.
#include "suite.h"
#include "platform.h"
#include "runner.h"
#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
using namespace std::chrono_literals;
namespace rtos_test {
namespace {
using platform::worker;
Metrics *metrics;
void bounded_wait(const std::atomic<unsigned> &value, unsigned expected) {
    auto start = platform::millis();
    while (value.load(std::memory_order_acquire) != expected) {
        TEST_REQUIRE(platform::millis() - start < 500);
        platform::idle_opportunity();
    }
}
void lifecycle() {
    std::thread empty;
    TEST_REQUIRE(!empty.joinable());
    std::thread relocated_empty(std::move(empty));
    TEST_REQUIRE(!empty.joinable() && !relocated_empty.joinable());
    std::atomic<unsigned> destroyed{0}, done{0};
    struct Payload { std::atomic<unsigned> &count; ~Payload() { ++count; } };
    for (unsigned mode = 0; mode < 4; ++mode) {
        destroyed = 0;
        auto ptr = std::unique_ptr<Payload>(new Payload{destroyed});
        auto child = worker(mode == 2 ? 0 : mode % 3, [p = std::move(ptr), &done] {
            platform::observe_stack();
            done.fetch_add(1, std::memory_order_release);
        });
        auto id = child.get_id();
        TEST_REQUIRE(id != std::this_thread::get_id());
        std::thread moved(std::move(child));
        TEST_REQUIRE(!child.joinable() && moved.get_id() == id);
        if (mode & 1) { bounded_wait(done, mode + 1); std::this_thread::sleep_for(2ms); }
        if (mode < 2) moved.join(); else moved.detach();
        bounded_wait(done, mode + 1);
        auto start = platform::millis();
        while (destroyed.load() != 1) {
            TEST_REQUIRE(platform::millis() - start < 500);
            platform::idle_opportunity();
        }
        ++metrics->payload_destructors;
        ++metrics->threads;
    }
}
void mutexes() {
    std::mutex mutex;
    unsigned count = 0;
    std::atomic<unsigned> ready{0};
    mutex.lock();
    auto contend = [&](unsigned tier) { return worker(tier, [&] {
        TEST_REQUIRE(!mutex.try_lock());
        ++ready;
        for (unsigned n = 0; n < 64; ++n) {
            std::lock_guard<std::mutex> held(mutex);
            ++count;
            std::this_thread::yield();
        }
        platform::observe_stack();
    }); };
    auto low = contend(0), high = contend(2);
    bounded_wait(ready, 2);
    mutex.unlock();
    low.join(); high.join();
    TEST_REQUIRE(count == 128);
    TEST_REQUIRE(mutex.try_lock()); mutex.unlock();
    std::recursive_mutex recursive;
    recursive.lock(); recursive.lock();
    TEST_REQUIRE(recursive.try_lock()); recursive.unlock();
    auto denied = worker(1, [&] { TEST_REQUIRE(!recursive.try_lock()); });
    denied.join(); recursive.unlock(); recursive.unlock();
    metrics->mutex_operations += count;
    metrics->threads += 3;
}
void conditions() {
    std::mutex mutex;
    std::condition_variable cv;
    unsigned ready = 0, tokens = 0, woke = 0;
    std::array<std::thread, 3> consumers;
    for (unsigned tier = 0; tier < consumers.size(); ++tier) {
        consumers[tier] = worker(tier, [&] {
            std::unique_lock<std::mutex> lock(mutex);
            ++ready; cv.notify_all();
            TEST_REQUIRE(cv.wait_for(lock, 500ms, [&] { return tokens != 0; }));
            --tokens; ++woke;
            cv.notify_all();
            lock.unlock();
            platform::observe_stack();
        });
    }
    {
        std::unique_lock<std::mutex> lock(mutex);
        TEST_REQUIRE(cv.wait_for(lock, 500ms, [&] { return ready == 3; }));
        tokens = 1;
        cv.notify_one();
        TEST_REQUIRE(cv.wait_for(lock, 500ms, [&] { return woke == 1; }));
        TEST_REQUIRE(tokens == 0);
        tokens = 2; cv.notify_all();
    }
    for (auto &thread : consumers) thread.join();
    TEST_REQUIRE(woke == 3 && tokens == 0);
    metrics->cv_wakeups += woke;
    metrics->threads += 3;
    for (unsigned tier = 0; tier < 3; ++tier) for (unsigned race = 0; race < 4; ++race) {
        bool signaled = false;
        auto notifier = worker(tier, [&] {
            std::this_thread::sleep_for(1ms);
            std::lock_guard<std::mutex> held(mutex);
            signaled = true; cv.notify_one();
        });
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait_for(lock, 2ms, [&] { return signaled; });
        }
        notifier.join();
        std::unique_lock<std::mutex> lock(mutex);
        TEST_REQUIRE(cv.wait_for(lock, 1ms) == std::cv_status::timeout);
        ++metrics->timeout_races;
        ++metrics->threads;
        lock.unlock();
        platform::idle_opportunity();
    }
}
void exit_callbacks() {
    std::mutex mutex; std::condition_variable cv; bool ready = false;
    auto exiting = worker(2, [&] {
        std::unique_lock<std::mutex> lock(mutex);
        ready = true;
        std::notify_all_at_thread_exit(cv, std::move(lock));
        platform::observe_stack();
    });
    {
        std::unique_lock<std::mutex> lock(mutex);
        TEST_REQUIRE(cv.wait_for(lock, 500ms, [&] { return ready; }));
    }
    // Keep mutex/CV alive through the entire exit callback. A predicate can
    // become observable after unlock but before notify_all has completed.
    // Detached owner lifetimes are exercised independently by lifecycle cases.
    exiting.join();
    TEST_REQUIRE(mutex.try_lock()); mutex.unlock();
    ++metrics->exit_callbacks;
    ++metrics->threads;
}
}
void run_shared_round(Report &report, Metrics &values) {
    metrics = &values;
    begin(report, Case::thread_lifecycle); lifecycle(); end(report, Case::thread_lifecycle);
    begin(report, Case::mutex_contention); mutexes(); end(report, Case::mutex_contention);
    begin(report, Case::condition_notification_timeout); conditions(); end(report, Case::condition_notification_timeout);
    begin(report, Case::thread_exit_notification); exit_callbacks(); end(report, Case::thread_exit_notification);
}
}
