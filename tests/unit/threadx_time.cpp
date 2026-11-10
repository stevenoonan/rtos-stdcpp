// SPDX-License-Identifier: MIT
#include "backend_time.h"
#include "time_rounding.h"
#include <cstdlib>

int main() {
    static_assert(sizeof(ULONG) == 4);
    check_deadline_rounding(TX_TIMER_TICKS_PER_SECOND, UINT32_MAX, [](bool value) {
        if (!value) {
            std::abort();
        }
    });
}
