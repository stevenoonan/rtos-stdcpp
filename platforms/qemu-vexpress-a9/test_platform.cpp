// SPDX-License-Identifier: MIT
// Test transport support, to be linked only by a separately qualified A9 image.
#include "platform.h"
#include "FreeRTOS.h"
#include "task.h"
#include <chrono>
#include <cstdlib>
namespace rtos_test::platform {
[[noreturn]] void fatal(const char *, unsigned) { std::_Exit(EXIT_FAILURE); }
void idle_opportunity() { vTaskDelay(1); }
uint32_t millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void observe_stack() {} // No board-specific stack threshold is imposed here.
free_rtos_std::attributes worker_attributes(unsigned tier, uint32_t words) {
    free_rtos_std::attributes attributes{};
    attributes.priority = tskIDLE_PRIORITY + 1 + tier;
    attributes.taskName = "test-worker";
    if (words) attributes.stackWordCount = words;
    return attributes;
}
}
