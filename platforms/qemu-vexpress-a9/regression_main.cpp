// SPDX-License-Identifier: MIT
#include "lifecycle_regression.h"
#include "runner.h"
#include "console.h"
#include <cstdlib>
int main() {
    bool pass = freertos_cpp11_lifecycle_regression() == 31;
    auto &report = rtos_test::rtos_stdcpp_regression_report;
    pass = rtos_test::finish(report) && pass;
    rtos_test::write_report(report, [](const char *text) { print(text); });
    std::_Exit(pass ? 0 : 1);
}
