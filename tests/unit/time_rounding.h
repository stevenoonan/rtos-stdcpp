// SPDX-License-Identifier: MIT
#pragma once
#include <cmath>
#include <cstdint>
#include <initializer_list>

// Include the selected backend_time.h before this header. Both backends use
// the same independent oracle, boundary vectors and deterministic sample set.
template <class Check> void check_deadline_rounding(uint32_t rate, uint32_t maximum, Check check) {
    for (int64_t ns : {INT64_MIN, INT64_C(-1), INT64_C(0), INT64_C(1), INT64_C(999999),
                       INT64_C(1000000), INT64_C(1000001), INT64_C(86400000000000), INT64_MAX}) {
        auto ticks = rtos_stdcpp::deadline_ticks(ns);
        check(ticks < maximum);
        if (ns <= 0) {
            check(ticks == 0);
        } else {
            long double requested =
                std::ceil(static_cast<long double>(ns) * rate / 1000000000.0L) + 1;
            auto expected =
                requested > maximum / 2 ? maximum / 2 : static_cast<uint32_t>(requested);
            check(expected == ticks);
        }
    }
    uint32_t random = 0x12345678;
    for (unsigned i = 0; i < 100000; ++i) {
        random = random * 1664525U + 1013904223U;
        auto ns = int64_t(random) * 1000;
        auto ticks = rtos_stdcpp::deadline_ticks(ns);
        check(ticks > 0 && ticks < maximum);
        check(uint64_t(ticks) * (1000000000 / rate) >= uint64_t(ns));
    }
}
