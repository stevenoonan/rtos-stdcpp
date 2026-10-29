// SPDX-License-Identifier: MIT
#pragma once

#include "tx_api.h"
#include "tx_thread.h"
#include "critical_section.h"
#include "threadx_thread_attributes.h"
#include "tls.h"
#include <cstdint>
#include <cerrno>
#include <cstdlib>
#include <new>

#if __cplusplus > 201703L
#include <compare>
#endif
extern "C" void rtos_stdcpp_threadx_initialize();

namespace rtos_stdcpp {
namespace internal {
extern TX_MUTEX attributes_mutex;

struct attributes_lock {
    const attributes* previous;
    static const attributes* _attrib;
    static constexpr attributes _default{};

    explicit attributes_lock(const attributes& a) {
        check(tx_mutex_get(&attributes_mutex, TX_WAIT_FOREVER));
        previous = _attrib;
        _attrib = &a;
    }

    ~attributes_lock() {
        _attrib = previous;
        check(tx_mutex_put(&attributes_mutex));
    }
};
} // namespace internal

class gthr_threadx {
public:
    struct completion {
        TX_THREAD task;
        char name[32];
        TX_EVENT_FLAGS_GROUP event;
        void* stack;
        void* argument;
        void (*function)(void*);
        unsigned references;
        completion* next;
        internal::tls_context tls;
    };

private:
    uintptr_t identity = 0;

    explicit constexpr gthr_threadx(uintptr_t value) : identity(value) {}

    completion* state() const {
        return reinterpret_cast<completion*>(identity);
    }

    bool managed() const {
        return identity && !(identity & 1U);
    }

    static void entry(ULONG);

public:
    using native_task_type = TX_THREAD*;
    constexpr gthr_threadx() = default;
    static void release(completion*);
    bool create_thread(void (*function)(void*), void* argument);

    int join() {
        if (!managed()) {
            return EINVAL;
        }
        if (*this == self()) {
            return EDEADLK;
        }
        ULONG flags;
        check(tx_event_flags_get(&state()->event, 1, TX_OR, &flags, TX_WAIT_FOREVER));
        release(state());
        identity = 0;
        return 0;
    }

    int detach() {
        if (!managed()) {
            return EINVAL;
        }
        release(state());
        identity = 0;
        return 0;
    }

    static gthr_threadx from_start_argument(void* p) {
        return gthr_threadx(reinterpret_cast<uintptr_t>(p));
    }

    void* arg() const {
        return state()->argument;
    }

    static internal::tls_context& thread_storage();
    void notify_joined();
    static gthr_threadx self();

    static native_task_type native_task_handle() {
        return tx_thread_identify();
    }

    constexpr friend bool operator==(gthr_threadx a, gthr_threadx b) noexcept {
        return a.identity == b.identity;
    }
#if __cpp_lib_three_way_comparison
    constexpr friend auto operator<=>(gthr_threadx a, gthr_threadx b) noexcept {
        return a.identity <=> b.identity;
    }
#else
    constexpr friend bool operator!=(gthr_threadx a, gthr_threadx b) noexcept {
        return !(a == b);
    }

    constexpr friend bool operator<(gthr_threadx a, gthr_threadx b) noexcept {
        return a.identity < b.identity;
    }
#endif
};

using backend_thread = gthr_threadx;
} // namespace rtos_stdcpp
