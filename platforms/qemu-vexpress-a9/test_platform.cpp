// SPDX-License-Identifier: MIT
#include "platform.h"
#include "console.h"
#include "qualification_platform.h"
#include "ca9_global_timer.h"
#include "critical_section.h"
#include <cstdlib>
extern "C" {
volatile uint32_t rtos_test_tick_count;

void vApplicationTickHook() {
    rtos_test_tick_count = rtos_test_tick_count + 1;
}
}

namespace rtos_test::platform {
[[noreturn]] void fatal(const char* file, unsigned line) {
    print(file);
    print(":");
    print(line);
    print("\n");
    std::_Exit(1);
}
} // namespace rtos_test::platform

namespace rtos_test::qualification {
bool masked_tick_probe() {
    rtos_stdcpp::critical_section guard;
    const auto before = xTaskGetTickCount();
    const auto start = GlobTimer_Counter();
    while (GlobTimer_Counter() - start < 3 * (100000000ULL / configTICK_RATE_HZ)) {
    }
    return xTaskGetTickCount() == before;
}
} // namespace rtos_test::qualification
