#include "backend_time.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <initializer_list>
using namespace free_rtos_std;
void check(bool value) { if (!value) std::abort(); }
int main() {
    for (int64_t ns : {INT64_MIN, INT64_C(-1), INT64_C(0), INT64_C(1), INT64_C(999999),
                       INT64_C(1000000), INT64_C(1000001), INT64_C(86400000000000), INT64_MAX}) {
        auto ticks = deadline_ticks(ns);
        check(ticks < portMAX_DELAY);
        if (ns <= 0) check(ticks == 0);
        else {
            long double requested = std::ceil(static_cast<long double>(ns) * configTICK_RATE_HZ / 1000000000.0L) + 1;
            auto expected = requested > portMAX_DELAY / 2 ? portMAX_DELAY / 2 : static_cast<uint32_t>(requested);
            check(ticks == expected);
        }
    }
    uint32_t random = 0x12345678;
    for (unsigned i = 0; i < 100000; ++i) {
        random = random * 1664525U + 1013904223U;
        uint32_t wraps = random % 1000;
        auto before = extend_ticks(wraps, UINT32_MAX);
        auto after = extend_ticks(wraps + 1, 0);
        check(after > before && after - before == 1);
        check(extend_ticks(wraps, random) == uint64_t(wraps) * (UINT64_C(1) << 32) + random);
        auto ns = int64_t(random) * 1000;
        auto ticks = deadline_ticks(ns);
        check(ticks > 0 && ticks < portMAX_DELAY);
        check(uint64_t(ticks) * (1000000000 / configTICK_RATE_HZ) >= uint64_t(ns));
    }
    std::cout << "100000 wrap/rounding cases passed at " << configTICK_RATE_HZ << " Hz\n";
}
