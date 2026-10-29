// SPDX-License-Identifier: MIT
#include "thread_gthread.h"
#include <exception>
#include <cstring>

extern "C" __attribute__((weak)) void rtos_stdcpp_threadx_thread_cleanup(TX_THREAD*) {}

namespace rtos_stdcpp {
TX_MUTEX internal::attributes_mutex;

namespace {
TX_THREAD reaper;
alignas(8) unsigned char reaper_stack[2048];
TX_SEMAPHORE reap_signal;
gthr_threadx::completion* pending;
bool initialized;

void reap(ULONG) {
    for (;;) {
        check(tx_semaphore_get(&reap_signal, TX_WAIT_FOREVER));
        for (;;) {
            gthr_threadx::completion* item = nullptr;
            bool more;
            {
                critical_section guard;
                auto** p = &pending;
                while (*p) {
                    if ((*p)->task.tx_thread_state == TX_COMPLETED) {
                        item = *p;
                        *p = item->next;
                        break;
                    }
                    p = &(*p)->next;
                }
                more = pending != nullptr;
            }
            if (item) {
                rtos_stdcpp_threadx_thread_cleanup(&item->task);
                check(tx_thread_delete(&item->task));
                std::free(item->stack);
                check(tx_event_flags_set(&item->event, 1, TX_OR));
                gthr_threadx::release(item);
            } else if (more) {
                check(tx_thread_sleep(1));
            } else {
                break;
            }
        }
    }
}
} // namespace

void gthr_threadx::release(completion* p) {
    bool last;
    {
        critical_section guard;
        last = --p->references == 0;
    }
    if (last) {
        check(tx_event_flags_delete(&p->event));
        p->~completion();
        std::free(p);
    }
}

bool gthr_threadx::create_thread(void (*function)(void*), void* argument) {
    if (!initialized || !tx_thread_identify()) {
        std::terminate();
    }
    check(tx_mutex_get(&internal::attributes_mutex, TX_WAIT_FOREVER));
    const auto attr = *internal::attributes_lock::_attrib;
    check(tx_mutex_put(&internal::attributes_mutex));
    if (attr.stack_word_count < TX_MINIMUM_STACK / sizeof(ULONG) ||
        attr.stack_word_count > ULONG(-1) / sizeof(ULONG) || attr.priority >= TX_MAX_PRIORITIES) {
        return false;
    }
    auto* p = static_cast<completion*>(std::malloc(sizeof(completion)));
    if (!p) {
        return false;
    }
    ::new (p) completion{};
    p->stack = std::malloc(attr.stack_word_count * sizeof(ULONG));
    if (!p->stack) {
        std::free(p);
        return false;
    }
    if (tx_event_flags_create(&p->event, const_cast<char*>("C++ completion")) != TX_SUCCESS) {
        std::free(p->stack);
        std::free(p);
        return false;
    }
    std::strncpy(p->name, attr.task_name ? attr.task_name : "C++ thread", sizeof p->name - 1);
    p->argument = argument;
    p->function = function;
    p->references = 2;
    auto status = tx_thread_create(&p->task, p->name, entry, reinterpret_cast<ULONG>(p), p->stack,
                                   attr.stack_word_count * sizeof(ULONG), attr.priority,
                                   attr.priority, TX_NO_TIME_SLICE, TX_DONT_START);
    if (status != TX_SUCCESS) {
        tx_event_flags_delete(&p->event);
        std::free(p->stack);
        std::free(p);
        return false;
    }
    p->task.rtos_stdcpp_context = p;
    identity = reinterpret_cast<uintptr_t>(p);
    check(tx_thread_resume(&p->task));
    return true;
}

void gthr_threadx::entry(ULONG value) {
    auto* p = reinterpret_cast<completion*>(value);
    p->function(p);
}

void gthr_threadx::notify_joined() {
    auto* p = state();
    {
        critical_section guard;
        p->next = pending;
        pending = p;
    }
    auto status = tx_semaphore_ceiling_put(&reap_signal, 1);
    if (status != TX_SUCCESS && status != TX_CEILING_EXCEEDED) {
        std::terminate();
    }
    // Return through the ThreadX shell. Only the reaper may delete this TCB.
}

gthr_threadx gthr_threadx::self() {
    auto* t = tx_thread_identify();
    if (!t) {
        return {};
    }
    return gthr_threadx(t->rtos_stdcpp_context ? reinterpret_cast<uintptr_t>(t->rtos_stdcpp_context)
                                               : reinterpret_cast<uintptr_t>(t) | 1U);
}

internal::tls_context& gthr_threadx::thread_storage() {
    auto* t = tx_thread_identify();
    if (TX_THREAD_GET_SYSTEM_STATE() != 0 || !t || !t->rtos_stdcpp_context) {
        std::terminate();
    }
    return static_cast<completion*>(t->rtos_stdcpp_context)->tls;
}
} // namespace rtos_stdcpp

extern "C" void rtos_stdcpp_threadx_initialize() {
    using namespace rtos_stdcpp;
    if (initialized) {
        return;
    }
    check(tx_mutex_create(&internal::attributes_mutex, const_cast<char*>("C++ attributes"),
                          TX_INHERIT));
    check(tx_semaphore_create(&reap_signal, const_cast<char*>("C++ reaper"), 0));
    check(tx_thread_create(&reaper, const_cast<char*>("C++ reaper"), reap, 0, reaper_stack,
                           sizeof reaper_stack, TX_MAX_PRIORITIES - 1, TX_MAX_PRIORITIES - 1,
                           TX_NO_TIME_SLICE, TX_AUTO_START));
    initialized = true;
}
