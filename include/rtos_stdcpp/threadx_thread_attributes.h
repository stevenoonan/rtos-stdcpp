// SPDX-License-Identifier: MIT
#pragma once

#include "tx_api.h"

namespace rtos_stdcpp {
struct attributes {
    const char* task_name = "C++ thread";

    // 32-bit words, matching the existing extension API.
    ULONG stack_word_count = 2048;

    // ThreadX: smaller numbers have higher priority.
    UINT priority = 16;
};

constexpr attributes attr_stack_size(ULONG words) {
    attributes a{};
    a.stack_word_count = words;
    return a;
}

constexpr attributes attr_priority(UINT priority) {
    attributes a{};
    a.priority = priority;
    return a;
}

constexpr attributes attr_name(const char* name) {
    attributes a{};
    a.task_name = name;
    return a;
}
} // namespace rtos_stdcpp
