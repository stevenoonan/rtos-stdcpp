// SPDX-License-Identifier: MIT
#include "runner.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
int main(int argc, char **argv) {
    const char *mode = argc == 2 ? argv[1] : "pass";
    rtos_test::Report report;
    rtos_test::reset(report, rtos_test::Suite::fixture);
    rtos_test::begin(report, rtos_test::Case::harness_assertions);
    TEST_EQ(2, 1 + 1);
    TEST_ASSERT(std::strcmp(mode, "fail") != 0 && std::strcmp(mode, "repeat-fail") != 0);
    if (std::strcmp(mode, "timeout") == 0) std::this_thread::sleep_for(std::chrono::seconds(30));
    if (std::strcmp(mode, "crash") == 0) std::abort();
    if (std::strcmp(mode, "exit") == 0) return 0;
    rtos_test::end(report, rtos_test::Case::harness_assertions);
    if (std::strcmp(mode, "repeat-fail") == 0) {
        rtos_test::begin(report, rtos_test::Case::harness_assertions);
        TEST_ASSERT(true);
        rtos_test::end(report, rtos_test::Case::harness_assertions);
    }
    bool pass = rtos_test::finish(report);
    if (std::strcmp(mode, "incomplete") == 0) report.complete = 0;
    if (std::strcmp(mode, "missing") == 0) report.results[uint32_t(rtos_test::Case::harness_assertions)].state = 0;
    if (std::strcmp(mode, "wrong-count") == 0) ++report.passed;
    rtos_test::write_report(report, [](const char *text) { std::fputs(text, stdout); });
    if (std::strcmp(mode, "exit-after-report") == 0) return 7;
    // A lying zero exit after an assertion failure must still be rejected.
    return (std::strcmp(mode, "fail") == 0 || std::strcmp(mode, "repeat-fail") == 0) ? 0 : pass ? 0 : 1;
}
