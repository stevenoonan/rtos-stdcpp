/// Copyright 2018-2023 Piotr Grygorczuk <grygorek@gmail.com>
/// Copyright 2023 by NXP. All rights reserved.
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in all
/// copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
/// THE SOFTWARE.

#ifndef GTHR_FREERTOS_INTERNAL_H
#define GTHR_FREERTOS_INTERNAL_H

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "critical_section.h"
#include "freertos_thread_attributes.h"
#include "tls.h"
#include "../../common/thread_identity.h"
#include <cstdint>

#if __cplusplus > 201703L
#include <compare>
#endif

namespace rtos_stdcpp {
namespace internal {
// Attributes are consumed synchronously by xTaskCreate. No operation in
// this scope waits for the worker to start, including a thread move.
struct attributes_lock : scheduler_section {
    explicit attributes_lock(const attributes& attr) : _previous(_attrib) {
        _attrib = &attr;
    }

    ~attributes_lock() {
        _attrib = _previous;
    }

    static const attributes* _attrib;

private:
    const attributes* _previous;
    static constexpr attributes _default{};
};
} // namespace internal

bool backend_interrupt_context();

class gthr_freertos {
    struct completion {
        EventGroupHandle_t event;
        TaskHandle_t task;
        void* argument;
        unsigned references;
        internal::tls_context tls;
        internal::thread_registration registration;
    };

    static constexpr EventBits_t completed = 1;
    static constexpr BaseType_t tls_slot = 0;

    // Two canonical words preserve direct lifecycle access and distinguish
    // successive workers at a recycled address. Native tasks use a tagged TCB
    // address and numeric ID zero. Copies are non-owning and never dereferenced
    // for comparisons or hashing, including after the completion block is freed.
    internal::thread_identity _identity{};

    explicit constexpr gthr_freertos(uintptr_t address, uint32_t number = 0)
        : _identity{address, number} {}

    completion* state() const {
        return reinterpret_cast<completion*>(_identity.address);
    }

    bool managed() const {
        return _identity.address && (_identity.address & 1U) == 0 && _identity.number;
    }

    static void release(completion* shared);

public:
    using task_foo = void (*)(void*);
    using native_task_type = TaskHandle_t;
    constexpr gthr_freertos() = default;

    uint32_t numeric_id() const noexcept {
        return static_cast<uint32_t>(_identity.number);
    }

    // Copy/move/default destruction never transfer or release ownership. Only
    // create/join/detach/worker-exit own references; std::thread enforces the
    // requirement to join/detach before destruction or move-assignment.

    bool create_thread(task_foo function, void* argument);

    int join();

    int detach();

    static gthr_freertos from_start_argument(void* argument) {
        return gthr_freertos{reinterpret_cast<uintptr_t>(argument),
                             static_cast<completion*>(argument)->registration.id};
    }

    void* arg() const {
        return state()->argument;
    }

    static internal::tls_context& thread_storage();

    [[noreturn]] void notify_joined();

    static gthr_freertos self() {
        auto task = xTaskGetCurrentTaskHandle();
        auto shared = pvTaskGetThreadLocalStoragePointer(task, tls_slot);
        return shared ? from_start_argument(shared)
                      : gthr_freertos{reinterpret_cast<uintptr_t>(task) | 1U};
    }

    static native_task_type native_task_handle() {
        return xTaskGetCurrentTaskHandle();
    }

    constexpr friend bool operator==(gthr_freertos left, gthr_freertos right) noexcept {
        return left._identity == right._identity;
    }
#if __cpp_lib_three_way_comparison
    constexpr friend std::strong_ordering operator<=>(gthr_freertos left,
                                                      gthr_freertos right) noexcept {
        return left._identity <=> right._identity;
    }
#else
    constexpr friend bool operator!=(gthr_freertos left, gthr_freertos right) noexcept {
        return !(left == right);
    }

    constexpr friend bool operator<(gthr_freertos left, gthr_freertos right) noexcept {
        return left._identity < right._identity;
    }
#endif
};

using backend_thread = gthr_freertos;
} // namespace rtos_stdcpp
#endif
