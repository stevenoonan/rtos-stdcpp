// SPDX-License-Identifier: MIT
#include "runner.h"
#include "platform.h"
#include <atomic>
#include <cstdio>

namespace rtos_test {
Report rtos_stdcpp_conformance_report{}, rtos_stdcpp_regression_report{}, rtos_stdcpp_cxx20_report{};
namespace {
std::atomic<uint32_t> checks{0}, failures{0};
Report *active_report = nullptr;
Case active_case{};
const char *failure_file = nullptr;
unsigned failure_line = 0;
void totals(Report &r) {
    r.passed = r.failed = r.skipped = r.checks = 0;
    for (uint32_t i = 0; i < case_count; ++i) if (catalog[i].suite == Suite(r.suite)) {
        const auto &result = r.results[i];
        r.passed += result.state == 1; r.failed += result.state == 2;
        r.skipped += result.state == 3; r.checks += result.checks;
    }
}
}
void reset(Report &r, Suite suite) {
    if (active_report) require_failed(__FILE__, __LINE__);
    failure_file = nullptr; failure_line = 0;
    r = {}; r.version = 1; r.suite = uint32_t(suite);
    for (uint32_t i = 0; i < case_count; ++i) if (catalog[i].suite == suite) {
        ++r.expected;
        if (catalog[i].skip_reason[0]) r.results[i].state = 3;
    }
    totals(r);
}
void begin(Report &r, Case id) {
    auto i = uint32_t(id);
    if (active_report || i >= case_count || catalog[i].suite != Suite(r.suite) || catalog[i].skip_reason[0])
        require_failed(__FILE__, __LINE__);
    r.complete = 0;
    checks.store(r.results[i].checks); failures.store(r.results[i].failures);
    r.results[i].state = 0; active_case = id; active_report = &r;
}
bool check(bool value, const char *file, unsigned line) {
    if (!active_report) require_failed(__FILE__, __LINE__);
    checks.fetch_add(1, std::memory_order_relaxed);
    if (!value && failures.fetch_add(1, std::memory_order_relaxed) == 0) {
        failure_file = file; failure_line = line;
    }
    return value;
}
void end(Report &r, Case id) {
    if (active_report != &r || active_case != id) require_failed(__FILE__, __LINE__);
    auto &result = r.results[uint32_t(id)];
    result.checks = checks.load(); result.failures = failures.load();
    result.state = result.checks && !result.failures ? 1 : 2;
    // A failure is sticky across repeated stress rounds.
    active_report = nullptr; totals(r);
}
bool finish(Report &r) {
    if (active_report) require_failed(__FILE__, __LINE__);
    totals(r);
    r.complete = 1;
    return r.failed == 0 && r.passed + r.skipped == r.expected;
}
[[noreturn]] void require_failed(const char *file, unsigned line) { platform::fatal(file, line); }
void write_report(const Report &r, Writer write) {
    char buffer[512];
    if (failure_file) {
        std::snprintf(buffer, sizeof(buffer), "RTOS_STDCPP_FAILURE %s:%u\n", failure_file, failure_line);
        write(buffer);
    }
    std::snprintf(buffer, sizeof(buffer), "RTOS_STDCPP_REPORT {\"version\":%u,\"suite\":%u,\"expected\":%u,\"passed\":%u,\"failed\":%u,\"skipped\":%u,\"checks\":%u,\"complete\":%u,\"results\":[",
        r.version, r.suite, r.expected, r.passed, r.failed, r.skipped, r.checks, r.complete);
    write(buffer);
    bool first = true;
    for (uint32_t i = 0; i < case_count; ++i) if (catalog[i].suite == Suite(r.suite)) {
        const auto &result = r.results[i];
        std::snprintf(buffer, sizeof(buffer), "%s{\"id\":\"%s\",\"state\":%u,\"checks\":%u,\"failures\":%u}",
            first ? "" : ",", catalog[i].id, result.state, result.checks, result.failures);
        write(buffer); first = false;
    }
    write("]}\n");
}
}
