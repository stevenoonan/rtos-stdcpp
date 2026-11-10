// SPDX-License-Identifier: MIT
#include "backend_time.h"
#include "runner.h"
#include "time_rounding.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>

using namespace rtos_stdcpp;

namespace {

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
} // namespace

int main() {
    rtos_test::Report report;
    rtos_test::reset(report, rtos_test::Suite::arithmetic);
    rtos_test::begin(report, rtos_test::Case::time_deadline_rounding);
    check_deadline_rounding(configTICK_RATE_HZ, portMAX_DELAY, [](bool value) {
        TEST_REQUIRE(value);
    });
    rtos_test::end(report, rtos_test::Case::time_deadline_rounding);
    rtos_test::begin(report, rtos_test::Case::time_tick_extension);
    extension();
    rtos_test::end(report, rtos_test::Case::time_tick_extension);
    bool pass = rtos_test::finish(report);
    rtos_test::write_report(report, [](const char* text) {
        std::fputs(text, stdout);
    });
    return pass ? 0 : 1;
}
