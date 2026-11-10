// SPDX-License-Identifier: MIT
#include "console.h"
#include "platform.h"
#include "qualification_platform.h"
#include "suite.h"
#include <rtos_stdcpp/clock.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <new>
#include <thread>

extern "C" void rtos_test_fpu_probe(const uint32_t*, uint32_t*, volatile uint32_t*);
#if RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR
extern "C" unsigned rtos_test_tls_allocation_count();
#endif
namespace {
using namespace std::chrono_literals;
using namespace rtos_test;
volatile unsigned startup_seed = 42;

unsigned startup_value() {
    static unsigned value = startup_seed;
    return value;
}

struct Startup {
    unsigned value;
    bool allocation;

    Startup() : value(startup_value()) {
        auto* pointer = std::malloc(16);
        allocation = pointer != nullptr;
        std::free(pointer);
    }
} startup;

thread_local unsigned tls_value = 7;
std::atomic<unsigned> destructed{0};

struct TlsLifetime {
    ~TlsLifetime() {
        ++destructed;
    }
};

thread_local TlsLifetime lifetime;

bool same_resources(const qualification::Resources& before) {
    const auto after = qualification::resources();
    return after.cpp_used == before.cpp_used && after.kernel_free == before.kernel_free &&
           after.threads == before.threads && after.tls_live == before.tls_live;
}

void churn(unsigned rounds) {
    for (unsigned i = 0; i < rounds; ++i) {
        auto* small = new (std::nothrow) unsigned char[257];
        TEST_REQUIRE(small);
        delete[] small;
        auto* aligned = ::operator new(128, std::align_val_t(64), std::nothrow);
        TEST_REQUIRE(aligned && uintptr_t(aligned) % 64 == 0);
        ::operator delete(aligned, std::align_val_t(64));
        std::atomic<bool> done{false};
        std::thread joined([] {
            TEST_REQUIRE(tls_value == 7);
            tls_value = 11;
            (void)&lifetime;
        });
        joined.join();
        std::thread detached([&] {
            TEST_REQUIRE(tls_value == 7);
            (void)&lifetime;
            done.store(true);
        });
        detached.detach();
        while (!done.load()) {
            std::this_thread::sleep_for(1ms);
        }
        qualification::reclaim();
    }
}

void floating_worker(unsigned tier) {
#if __ARM_ARCH_PROFILE == 'A'
    constexpr unsigned count = 64; // VFPv3 D0-D31.
#else
    constexpr unsigned count = 32; // Cortex-M S0-S31.
#endif
    uint32_t expected[count], observed[count];
    for (unsigned i = 0; i < count; ++i) {
        expected[i] = 0x3f800000U + tier * 0x10000U + i;
    }
    for (unsigned round = 0; round < 32; ++round) {
        rtos_test_fpu_probe(expected, observed, &rtos_test_tick_count);
        for (unsigned i = 0; i < count; ++i) {
            TEST_REQUIRE(observed[i] == expected[i]);
        }
        std::this_thread::sleep_for(1ms);
    }
    platform::observe_stack();
}
} // namespace

int main() {
    using namespace rtos_test;
    using Clock = std::chrono::steady_clock;
    Report report{};
    reset(report, Suite::qualification);
    qualification::prepare_ticks();
    const auto initial_kernel = qualification::kernel_ticks();
    const auto initial_steady = Clock::now();
    begin(report, Case::runtime_startup);
    TEST_REQUIRE(startup.value == 42 && startup.allocation && startup_value() == 42);
    TEST_REQUIRE(qualification::resources().threads >= 2);
    end(report, Case::runtime_startup);
    begin(report, Case::runtime_interrupt_masking);
    TEST_REQUIRE(qualification::masked_tick_probe());
    end(report, Case::runtime_interrupt_masking);

    begin(report, Case::time_clocks_sleep);
    std::this_thread::sleep_for(40ms);
    TEST_REQUIRE(Clock::now() - initial_steady >= 40ms);
    TEST_REQUIRE(uint32_t(qualification::kernel_ticks() - initial_kernel) >= 40);
#if RTOS_STDCPP_TEST_TICK_WRAP
    TEST_REQUIRE(initial_kernel > UINT32_MAX - 100);
    TEST_REQUIRE(qualification::kernel_ticks() < 1000);
    TEST_REQUIRE(Clock::now().time_since_epoch() > std::chrono::milliseconds(UINT32_MAX));
#endif
    auto before = Clock::now();
    std::this_thread::sleep_for(-1ms);
    std::this_thread::sleep_for(0ms);
    TEST_REQUIRE(Clock::now() - before < 20ms);
    for (auto duration : {1ns, 1000001ns, 100000000ns}) {
        before = Clock::now();
        std::this_thread::sleep_for(duration);
        const auto elapsed = Clock::now() - before;
        TEST_REQUIRE(elapsed >= duration && elapsed < duration + 50ms);
    }
    const auto wall = std::chrono::system_clock::now();
    before = Clock::now();
    rtos_stdcpp::set_system_clock_time(wall + 24h);
    TEST_REQUIRE(std::chrono::system_clock::now() >= wall + 24h);
    TEST_REQUIRE(Clock::now() - before < 20ms);
    rtos_stdcpp::set_system_clock_time(wall - 24h);
    TEST_REQUIRE(std::chrono::system_clock::now() < wall - 23h);
    std::this_thread::sleep_until(Clock::now() + 3ms);
    TEST_REQUIRE(Clock::now() - before >= 3ms && Clock::now() - before < 50ms);
    rtos_stdcpp::set_system_clock_time(wall);
    end(report, Case::time_clocks_sleep);

    Report stress{};
    reset(stress, Suite::cxx17);
    Metrics metrics;
    for (unsigned i = 0; i < 4; ++i) {
        run_shared_round(stress, metrics);
    }
    run_facility_cases(stress);
#if __cplusplus >= 202002L
    auto& cxx20 = rtos_stdcpp_cxx20_report;
    if (!run_cxx20_suite(cxx20)) {
        platform::fatal(__FILE__, __LINE__);
    }
#endif
    qualification::reclaim();
    const auto baseline = qualification::resources();
    bool recovered = true;
    for (unsigned round = 0; round < 64; ++round) {
        run_shared_round(stress, metrics);
#if __cplusplus >= 202002L
        run_cxx20_round(cxx20);
        if (!finish(cxx20)) {
            platform::fatal(__FILE__, __LINE__);
        }
#endif
        qualification::reclaim();
        recovered = same_resources(baseline) && recovered;
    }
    const bool stress_passed = finish(stress);
    begin(report, Case::runtime_stress);
    TEST_REQUIRE(stress_passed);
    TEST_REQUIRE(recovered);
    end(report, Case::runtime_stress);

    begin(report, Case::runtime_resources);
    churn(16);
    const auto warmed = qualification::resources();
#if RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR
    const auto allocations_before = rtos_test_tls_allocation_count();
#endif
    for (unsigned round = 0; round < 4; ++round) {
        const auto destructed_before = destructed.load();
        churn(16);
        TEST_REQUIRE(destructed.load() == destructed_before + 32);
        TEST_REQUIRE(same_resources(warmed));
    }
#if RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR
    TEST_REQUIRE(rtos_test_tls_allocation_count() > allocations_before);
#endif
    TEST_REQUIRE(::operator new(4 * 1024 * 1024, std::nothrow) == nullptr);
    TEST_REQUIRE(::operator new(4 * 1024 * 1024, std::align_val_t(64), std::nothrow) == nullptr);
    platform::observe_stack();
    end(report, Case::runtime_resources);

    begin(report, Case::runtime_fpu_context);
    auto low = platform::worker(
        0,
        [] {
            floating_worker(0);
        },
        2048);
    auto high = platform::worker(
        2,
        [] {
            floating_worker(2);
        },
        2048);
    low.join();
    high.join();
    platform::observe_stack();
    end(report, Case::runtime_fpu_context);
    const bool pass = finish(report);
    write_report(report, [](const char* text) {
        print(text);
    });
    std::_Exit(pass ? 0 : 1);
}
