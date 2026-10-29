// SPDX-License-Identifier: MIT
#include "suite.h"
#include "platform.h"
#include <atomic>
#include <functional>
#include <memory>
#include <condition_variable>
using namespace std::chrono_literals;
namespace rtos_test {
namespace {
void arguments() {
    std::unique_ptr<unsigned> pointer(new unsigned(21));
    unsigned result = 0;
    std::thread child([](std::unique_ptr<unsigned> p, unsigned &out) { out = 2 * *p; }, std::move(pointer), std::ref(result));
    auto id = child.get_id(); auto hash = std::hash<std::thread::id>{}(id);
    std::thread moved; moved = std::move(child);
    TEST_REQUIRE(!pointer && !child.joinable() && moved.get_id() == id);
    TEST_EQ(hash, std::hash<std::thread::id>{}(moved.get_id()));
    moved.join(); TEST_EQ(42U, result);
}
std::atomic<unsigned> constructions{0};
struct Value { Value() { std::this_thread::sleep_for(2ms); ++constructions; } unsigned value = 42; };
Value &local() { static Value value; return value; }
void statics() {
    auto access = [] { TEST_EQ(42U, local().value); };
    auto a = platform::worker(0, access), b = platform::worker(1, access), c = platform::worker(2, access);
    a.join(); b.join(); c.join(); TEST_EQ(1U, constructions.load());
}
void spurious() {
    std::mutex mutex; std::condition_variable condition;
    bool ready = false, release = false; unsigned predicates = 0;
    auto child = platform::worker(1, [&] {
        std::unique_lock<std::mutex> lock(mutex); ready = true; condition.notify_all();
        TEST_REQUIRE(condition.wait_for(lock, 500ms, [&] { ++predicates; return release; }));
    });
    {
        std::unique_lock<std::mutex> lock(mutex);
        TEST_REQUIRE(condition.wait_for(lock, 500ms, [&] { return ready; }));
        condition.notify_all(); // A notification without a true predicate must not complete the wait.
    }
    auto deadline = std::chrono::steady_clock::now() + 500ms;
    for (;;) {
        {
            std::lock_guard<std::mutex> held(mutex);
            if (predicates >= 2) { release = true; condition.notify_all(); break; }
        }
        TEST_REQUIRE(std::chrono::steady_clock::now() < deadline);
        platform::idle_opportunity();
    }
    child.join();
}
}
void run_extended_cases(Report &r) {
    begin(r, Case::thread_arguments_hash); arguments(); end(r, Case::thread_arguments_hash);
    begin(r, Case::initialization_local_statics); statics(); end(r, Case::initialization_local_statics);
    begin(r, Case::condition_predicate_recheck); spurious(); end(r, Case::condition_predicate_recheck);
}
}
