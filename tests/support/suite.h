// SPDX-License-Identifier: MIT
#pragma once
#include "runner.h"
namespace rtos_test {
struct Metrics {
    uint32_t threads = 0, mutex_operations = 0, cv_wakeups = 0,
        timeout_races = 0, exit_callbacks = 0, payload_destructors = 0;
};
void run_shared_round(Report &, Metrics &);
void run_inherited_cases(Report &);
void run_extended_cases(Report &);
inline bool run_suite(Report &report) {
    reset(report, Suite::cxx17);
    Metrics metrics;
    run_shared_round(report, metrics);
    run_inherited_cases(report);
    return finish(report);
}
}
