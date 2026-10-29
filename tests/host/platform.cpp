// SPDX-License-Identifier: MIT
#include "platform.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
namespace rtos_test::platform {
[[noreturn]] void fatal(const char *file, unsigned line) {
    std::fprintf(stderr, "Test requirement failed: %s:%u\n", file, line); std::abort();
}
void idle_opportunity() { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
uint32_t millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void observe_stack() {} // Host reference run makes no embedded stack claim.
}
