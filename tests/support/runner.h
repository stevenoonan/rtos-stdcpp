// SPDX-License-Identifier: MIT
#pragma once
#include "test_catalog.h"
#include <cstdint>

namespace rtos_test {
// Fixed uint32_t layout permits a transport to read this after a result stop,
// including firmware built without C++ debug type information.
struct Result { uint32_t state, checks, failures; }; // 0 missing, 1 pass, 2 fail, 3 skip
struct Report {
    uint32_t version, suite, expected, passed, failed, skipped, checks, complete;
    Result results[case_count];
};
static_assert(sizeof(Report) == (8 + 3 * case_count) * sizeof(uint32_t), "Test transport ABI changed");
extern "C" {
extern Report rtos_stdcpp_conformance_report;
extern Report rtos_stdcpp_regression_report;
extern Report rtos_stdcpp_cxx20_report;
}
void reset(Report &, Suite);
void begin(Report &, Case);
void end(Report &, Case);
bool check(bool, const char *, unsigned);
bool finish(Report &);
using Writer = void (*)(const char *);
void write_report(const Report &, Writer);
[[noreturn]] void require_failed(const char *, unsigned);
}
#define TEST_ASSERT(expression) rtos_test::check(bool(expression), __FILE__, __LINE__)
#define TEST_EQ(expected, received) TEST_ASSERT((expected) == (received))
#define TEST_REQUIRE(expression) do { if (!TEST_ASSERT(expression)) rtos_test::require_failed(__FILE__, __LINE__); } while (0)
