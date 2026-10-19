// SPDX-License-Identifier: MIT
#include "backend_time.h"
#include "runner.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
using namespace free_rtos_std;
namespace {
void rounding() {
    for (int64_t ns : {INT64_MIN, INT64_C(-1), INT64_C(0), INT64_C(1), INT64_C(999999),
                       INT64_C(1000000), INT64_C(1000001), INT64_C(86400000000000), INT64_MAX}) {
        auto ticks = deadline_ticks(ns);
        TEST_REQUIRE(ticks < portMAX_DELAY);
        if (ns <= 0) TEST_EQ(0U, ticks);
        else {
            long double requested = std::ceil(static_cast<long double>(ns) * configTICK_RATE_HZ / 1000000000.0L) + 1;
            auto expected = requested > portMAX_DELAY / 2 ? portMAX_DELAY / 2 : static_cast<uint32_t>(requested);
            TEST_EQ(expected, ticks);
        }
    }
    uint32_t random = 0x12345678;
    for (unsigned i = 0; i < 100000; ++i) {
        random = random * 1664525U + 1013904223U;
        auto ns = int64_t(random) * 1000;
        auto ticks = deadline_ticks(ns);
        TEST_REQUIRE(ticks > 0 && ticks < portMAX_DELAY);
        TEST_REQUIRE(uint64_t(ticks) * (1000000000 / configTICK_RATE_HZ) >= uint64_t(ns));
    }
}
void extension() {
    uint32_t random = 0x12345678;
    for (unsigned i = 0; i < 100000; ++i) {
        random = random * 1664525U + 1013904223U;
        uint32_t wraps = random % 1000;
        auto before = extend_ticks(wraps, UINT32_MAX);
        auto after = extend_ticks(wraps + 1, 0);
        TEST_REQUIRE(after > before && after - before == 1);
        TEST_EQ(uint64_t(wraps) * (UINT64_C(1) << 32) + random, extend_ticks(wraps, random));
    }
}
}
int main() {
    rtos_test::Report report;
    rtos_test::reset(report, rtos_test::Suite::arithmetic);
    rtos_test::begin(report, rtos_test::Case::time_deadline_rounding);
    rounding();
    rtos_test::end(report, rtos_test::Case::time_deadline_rounding);
    rtos_test::begin(report, rtos_test::Case::time_tick_extension);
    extension();
    rtos_test::end(report, rtos_test::Case::time_tick_extension);
    bool pass = rtos_test::finish(report);
    rtos_test::write_report(report, [](const char *text) { std::fputs(text, stdout); });
    return pass ? 0 : 1;
}
