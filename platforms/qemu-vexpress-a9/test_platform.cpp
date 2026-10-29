// SPDX-License-Identifier: MIT
// Test transport support, to be linked only by a separately qualified A9 image.
#include "platform.h"
#include "console.h"
#include "runner.h"
#include "FreeRTOS.h"
#include "task.h"
#include <chrono>
#include <cstdlib>

namespace rtos_test::platform {
[[noreturn]] void fatal(const char* file, unsigned line) {
    print(file);
    print(":");
    print(line);
    print("\n");
    std::_Exit(EXIT_FAILURE);
}

void idle_opportunity() {
    vTaskDelay(1);
}

uint32_t millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void observe_stack() {
    TEST_REQUIRE(uxTaskGetStackHighWaterMark(nullptr) >= 128);
}

rtos_stdcpp::attributes worker_attributes(unsigned tier, uint32_t words) {
    rtos_stdcpp::attributes attributes{};
    attributes.priority = tskIDLE_PRIORITY + 1 + 2 * tier;
    attributes.task_name = "test-worker";
    if (words) {
        attributes.stack_word_count = words;
    }
    return attributes;
}
} // namespace rtos_test::platform
