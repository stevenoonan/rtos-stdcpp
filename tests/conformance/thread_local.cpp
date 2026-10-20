// SPDX-License-Identifier: MIT
#include "suite.h"
#include "platform.h"
#include "tls_peer.h"
#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <utility>

using namespace std::chrono_literals;
namespace rtos_test {
namespace {
thread_local unsigned zero;
thread_local unsigned initialized = 17;
struct Member { static thread_local unsigned value; };
thread_local unsigned Member::value = 23;
struct alignas(256) Aligned { unsigned words[4]; };

template<unsigned I> unsigned &slot() {
    static thread_local unsigned value = I;
    return value;
}
template<std::size_t... I> void grow_table(std::index_sequence<I...>) {
    ((TEST_EQ(I, slot<I>()), slot<I>() += 100), ...);
}

void isolation() {
    std::mutex mutex; std::condition_variable cv;
    unsigned ready = 0; bool release = false;
    std::array<unsigned *, 3> addresses{};
    std::array<std::thread, 3> workers;
    for (unsigned n = 0; n < workers.size(); ++n) {
        workers[n] = platform::worker(n, [&, n] {
            TEST_EQ(0U, zero); TEST_EQ(17U, initialized); TEST_EQ(23U, Member::value);
            TEST_EQ(41U, tls_test::cross_unit);
            auto *cross = tls_test::peer_address();
            TEST_REQUIRE(cross == &tls_test::cross_unit);
#ifdef RTOS_STDCPP_TEST_FREERTOS
            // Also qualify externally declared TLS with the embedded compiler.
            TEST_EQ(53U, tls_test::external);
            TEST_REQUIRE(tls_test::external_address() == &tls_test::external);
            tls_test::external = n;
            TEST_EQ(n, *tls_test::external_address());
#endif
            tls_test::check_common();
            tls_test::check_dynamic();
            static thread_local Aligned aligned{};
            TEST_EQ(0U, reinterpret_cast<uintptr_t>(&aligned) % alignof(Aligned));
            for (auto value : aligned.words) TEST_EQ(0U, value);
            auto *stable = &initialized;
            // More than the initial pointer-table capacity; all variable
            // addresses must survive growth and sparse global indices.
            grow_table(std::make_index_sequence<48>{});
            TEST_REQUIRE(stable == &initialized);
            addresses[n] = stable;
            initialized = n + 100; zero = n + 1;
            Member::value = n; *cross = n + 200;
            aligned.words[0] = n + 300;
            {
                std::unique_lock<std::mutex> lock(mutex);
                ++ready; cv.notify_all();
                TEST_REQUIRE(cv.wait_for(lock, 2s, [&] { return release; }));
            }
            TEST_EQ(n + 100, *stable); TEST_EQ(n + 1, zero);
            TEST_EQ(n, Member::value); TEST_EQ(n + 200, tls_test::cross_unit);
            TEST_REQUIRE(cross == tls_test::peer_address());
            TEST_EQ(n + 300, aligned.words[0]);
            TEST_EQ(147U, slot<47>());
            platform::observe_stack();
        });
    }
    {
        std::unique_lock<std::mutex> lock(mutex);
        TEST_REQUIRE(cv.wait_for(lock, 2s, [&] { return ready == workers.size(); }));
        TEST_REQUIRE(addresses[0] != addresses[1] && addresses[1] != addresses[2]
                     && addresses[0] != addresses[2]);
        release = true; cv.notify_all();
    }
    for (auto &worker : workers) worker.join();
}

void await(const std::atomic<bool> &value) {
    auto start = platform::millis();
    while (!value.load(std::memory_order_acquire)) {
        TEST_REQUIRE(platform::millis() - start < 2000);
        platform::idle_opportunity();
    }
}

struct Log {
    unsigned order[3]{};
    unsigned size = 0, constructions = 0;
    std::atomic<bool> done{false};
};
struct Ordered;
template<unsigned I> Ordered &ordered();
struct Ordered {
    Log *log = nullptr;
    unsigned id;
    bool spawn = false;
    explicit Ordered(unsigned value) : id(value) {}
    void attach(Log &value) { if (!log) { log = &value; ++log->constructions; } }
    ~Ordered() {
        if (spawn) ordered<3>().attach(*log);
        TEST_REQUIRE(log && log->size < 3);
        log->order[log->size++] = id;
        if (id == 1) log->done.store(true, std::memory_order_release);
    }
};
template<unsigned I> Ordered &ordered() {
    static thread_local Ordered object{I};
    return object;
}

void lifetimes() {
    for (unsigned mode = 0; mode < 3; ++mode) {
        Log log;
        std::atomic<bool> entered{false}, leave{false};
        std::atomic<unsigned> peer_destroyed{0};
        auto worker = platform::worker(1, [&] {
            // Construct this first so its destructor runs last, after the log.
            tls_test::peer_lifetime(peer_destroyed);
            ordered<1>().attach(log); ordered<1>().attach(log);
            ordered<2>().attach(log); ordered<2>().spawn = true;
            entered.store(true, std::memory_order_release);
            await(leave);
        });
        await(entered);
        TEST_EQ(2U, log.constructions); TEST_EQ(0U, log.size);
        std::thread moved(std::move(worker));
        if (mode == 1) moved.detach(); // Owner disappears before worker exit.
        leave.store(true, std::memory_order_release);
        if (mode == 0) moved.join();
        else {
            // peer_destroyed is the last TLS destructor and protects the
            // controller's captured data even for the detached worker.
            auto start = platform::millis();
            while (peer_destroyed.load() != 1) {
                TEST_REQUIRE(platform::millis() - start < 2000);
                platform::idle_opportunity();
            }
            if (mode == 2) { platform::idle_opportunity(); moved.detach(); }
        }
        TEST_REQUIRE(log.done.load(std::memory_order_acquire));
        TEST_EQ(3U, log.constructions); TEST_EQ(3U, log.size);
        TEST_EQ(2U, log.order[0]); TEST_EQ(3U, log.order[1]); TEST_EQ(1U, log.order[2]);
        TEST_EQ(1U, peer_destroyed.load());
    }
}

struct ExitGate {
    std::atomic<bool> entered{false}, release{false}, destroyed{false};
};
void exit_notification() {
    std::mutex mutex; std::condition_variable cv; bool ready = false;
    ExitGate gate;
    auto worker = platform::worker(2, [&] {
        struct Object {
            ExitGate *gate;
            ~Object() {
                gate->entered.store(true, std::memory_order_release);
                await(gate->release);
                gate->destroyed.store(true, std::memory_order_release);
            }
        };
        static thread_local Object object{&gate};
        (void)object;
        std::unique_lock<std::mutex> lock(mutex);
        ready = true;
        std::notify_all_at_thread_exit(cv, std::move(lock));
    });
    await(gate.entered);
    TEST_REQUIRE(!mutex.try_lock()); // Exit notification must still own it.
    std::atomic<bool> joined{false};
    auto joiner = platform::worker(1, [&] { worker.join(); joined.store(true); });
    platform::idle_opportunity();
    TEST_REQUIRE(!joined.load());
    gate.release.store(true, std::memory_order_release);
    {
        std::unique_lock<std::mutex> lock(mutex);
        TEST_REQUIRE(cv.wait_for(lock, 2s, [&] { return ready; }));
        TEST_REQUIRE(gate.destroyed.load(std::memory_order_acquire));
    }
    joiner.join(); TEST_REQUIRE(joined.load());
}
}

void run_tls_round(Report &report, Metrics &metrics) {
    begin(report, Case::thread_local_storage);
    // Repeat in new workers to verify guards, values, and destructors reset.
    isolation(); isolation(); lifetimes(); exit_notification();
    metrics.threads += 11;
    end(report, Case::thread_local_storage);
}
}
