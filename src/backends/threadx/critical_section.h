// SPDX-License-Identifier: MIT
#pragma once

#include "tx_api.h"
#include "tx_thread.h"
#include <cstdlib>

namespace rtos_stdcpp {
bool backend_interrupt_context();

inline void check(UINT result) {
    if (result != TX_SUCCESS) {
        std::abort();
    }
}

// ThreadX can request a context switch even inside an interrupt-masked API
// caller. Keep a notify_all batch closed to waiters that immediately re-wait.
struct scheduler_section {
    TX_THREAD* thread;
    UINT previous;

    scheduler_section() : thread(tx_thread_identify()) {
        if (!thread) {
            std::abort();
        }
        check(tx_thread_preemption_change(thread, 0, &previous));
    }

    ~scheduler_section() {
        UINT ignored;
        check(tx_thread_preemption_change(thread, previous, &ignored));
    }

    scheduler_section(const scheduler_section&) = delete;
};

struct critical_section {
    UINT posture;

    critical_section() : posture(tx_interrupt_control(TX_INT_DISABLE)) {}

    ~critical_section() {
        tx_interrupt_control(posture);
    }

    critical_section(const critical_section&) = delete;
};
} // namespace rtos_stdcpp
