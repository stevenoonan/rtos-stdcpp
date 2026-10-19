// SPDX-License-Identifier: MIT
#include "suite.h"
#include <cstdio>
int main() {
    auto &report = rtos_test::rtos_stdcpp_conformance_report;
    bool pass = rtos_test::run_suite(report);
    rtos_test::write_report(report, [](const char *text) { std::fputs(text, stdout); });
    return pass ? 0 : 1;
}
