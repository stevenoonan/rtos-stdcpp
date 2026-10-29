// SPDX-License-Identifier: MIT
#pragma once

#include "critical_section.h"
#include <cerrno>

namespace rtos_stdcpp {
struct mutex {
    TX_MUTEX native{};
    bool initialized = false;

    void ensure() {
        critical_section guard;
        if (!initialized) {
            check(tx_mutex_create(&native, const_cast<char*>("C++ mutex"), TX_INHERIT));
            initialized = true;
        }
    }

    int get(ULONG ticks) {
        if (backend_interrupt_context()) {
            std::abort();
        }
        if (!tx_thread_identify()) {
            return 0;
        }
        ensure();
        return tx_mutex_get(&native, ticks) == TX_SUCCESS ? 0 : EBUSY;
    }

    int put() {
        if (backend_interrupt_context()) {
            std::abort();
        }
        if (!tx_thread_identify()) {
            return 0;
        }
        return tx_mutex_put(&native) == TX_SUCCESS ? 0 : EPERM;
    }

    void destroy() {
        if (initialized) {
            check(tx_mutex_delete(&native));
            initialized = false;
        }
    }
};

struct recursive_mutex : mutex {};

struct Once {
    bool done = false;
    mutex lock;

    ~Once() {
        lock.destroy();
    }
};
} // namespace rtos_stdcpp
