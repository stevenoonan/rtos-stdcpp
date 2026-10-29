// SPDX-License-Identifier: MIT
#include "suite.h"
#include <cstdio>
int main() {
    #ifdef RTOS_STDCPP_RUN_CXX20
    auto &report = rtos_test::rtos_stdcpp_cxx20_report;
    bool pass = rtos_test::run_cxx20_suite(report);
#else
    auto &report = rtos_test::rtos_stdcpp_conformance_report;
    bool pass = rtos_test::run_suite(report);
#endif
    rtos_test::write_report(report, [](const char *text) { std::fputs(text, stdout); });
    return pass ? 0 : 1;
}
