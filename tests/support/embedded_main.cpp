// SPDX-License-Identifier: MIT
#include "suite.h"
#include "console.h"
#include <cstdlib>
int main() {
    auto &report = rtos_test::rtos_stdcpp_conformance_report;
    bool pass = rtos_test::run_suite(report);
    rtos_test::write_report(report, [](const char *text) { print(text); });
    // The selected platform implements _exit. On A9 this reaches the
    // SYS_EXIT_EXTENDED semihost call. The outer process deadline covers faults.
    std::_Exit(pass ? EXIT_SUCCESS : EXIT_FAILURE);
}
