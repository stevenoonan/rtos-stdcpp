// SPDX-License-Identifier: MIT
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

#include "thread_gthread.h"
#include <cerrno>
#include <new>

namespace rtos_stdcpp {
namespace {
internal::thread_id_registry thread_ids;
} // namespace

void gthr_freertos::release(completion* shared) {
    bool last;
    {
        critical_section guard;
        configASSERT(shared->references != 0);
        last = --shared->references == 0;
        if (last) {
            thread_ids.erase(shared->registration);
        }
    }
    if (last) {
        // No references remain. Deallocation may suspend scheduling, but must
        // not keep timer IRQs masked while walking allocator/kernel lists.
        vEventGroupDelete(shared->event);
        vPortFree(shared);
    }
}

bool gthr_freertos::create_thread(task_foo function, void* argument) {
    void* storage = pvPortMalloc(sizeof(completion));
    if (!storage) {
        return false;
    }
    auto* shared = ::new (storage) completion{};
    shared->event = xEventGroupCreate();
    if (!shared->event) {
        vPortFree(shared);
        return false;
    }
    shared->argument = argument;
    shared->task = nullptr;
    shared->references = 2; // std::thread owner and worker
    {
        scheduler_section guard;
        const auto& attr = *internal::attributes_lock::_attrib;
        bool registered;
        {
            critical_section id_guard;
            const auto seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(shared)) ^
                              static_cast<uint32_t>(xTaskGetTickCount());
            registered = thread_ids.insert(shared->registration, seed);
        }
        // The stable shared block is the entry argument. Nothing in the worker
        // depends on the address/lifetime of a std::thread object or temporary.
        if (registered && xTaskCreate(function, attr.task_name, attr.stack_word_count, shared,
                                      attr.priority, &shared->task) == pdPASS) {
            vTaskSetThreadLocalStoragePointer(shared->task, tls_slot, shared);
            _identity = {reinterpret_cast<uintptr_t>(shared), shared->registration.id};
            configASSERT((_identity.address & 1U) == 0);
            return true;
        }
        if (registered) {
            critical_section id_guard;
            thread_ids.erase(shared->registration);
        }
    }
    vEventGroupDelete(shared->event);
    vPortFree(shared);
    return false;
}

int gthr_freertos::join() {
    if (!managed()) {
        return EINVAL;
    }
    if (*this == self()) {
        return EDEADLK;
    }
    auto* shared = state();
    while ((xEventGroupWaitBits(shared->event, completed, pdFALSE, pdTRUE, portMAX_DELAY) &
            completed) == 0) {
    }
    release(shared);
    _identity = {};
    return 0;
}

int gthr_freertos::detach() {
    if (!managed()) {
        return EINVAL;
    }
    release(state());
    _identity = {};
    return 0;
}

[[noreturn]] void gthr_freertos::notify_joined() {
    auto* shared = state();
    vTaskSetThreadLocalStoragePointer(nullptr, tls_slot, nullptr);
    // Keep the worker reference until set-bits returns: a newly unblocked
    // joiner can run immediately and release its own reference in the meantime.
    xEventGroupSetBits(shared->event, completed);
    release(shared);
    // The owner never inspects/deletes this TCB. Kernel idle cleanup owns it.
    vTaskDelete(nullptr);
    for (;;) {
    } // vTaskDelete's C declaration does not carry noreturn.
}

} // namespace rtos_stdcpp
