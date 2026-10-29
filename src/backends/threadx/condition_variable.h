// SPDX-License-Identifier: MIT
#pragma once

#include "mutex.h"
#include "backend_time.h"

namespace rtos_stdcpp {
class cv_task_list {
    struct waiter {
        TX_SEMAPHORE event;
        waiter* next;
        bool signaled;
    };

    waiter* head = nullptr;
    waiter* tail = nullptr;

    void remove(waiter* w) {
        waiter* prev = nullptr;
        for (auto* p = head; p; p = p->next) {
            if (p == w) {
                if (prev) {
                    prev->next = p->next;
                } else {
                    head = p->next;
                }
                if (tail == p) {
                    tail = prev;
                }
                return;
            }
            prev = p;
        }
    }

public:
    constexpr cv_task_list() = default;
    cv_task_list(const cv_task_list&) = delete;

    ~cv_task_list() {
        if (head) {
            std::abort();
        }
    }

    void signal(bool all) {
        scheduler_section scheduling;
        critical_section guard;
        while (head) {
            auto* p = head;
            head = p->next;
            if (!head) {
                tail = nullptr;
            }
            p->signaled = true;
            check(tx_semaphore_put(&p->event));
            if (!all) {
                break;
            }
        }
    }

    int wait(mutex* m, const int64_t* deadline, bool steady) {
        waiter w{};
        check(tx_semaphore_create(&w.event, const_cast<char*>("C++ CV waiter"), 0));
        {
            critical_section guard;
            if (tail) {
                tail->next = &w;
            } else {
                head = &w;
            }
            tail = &w;
        }
        if (m->put()) {
            std::abort();
        }
        int result = 0;
        for (;;) {
            auto now = steady ? monotonic_ns() : realtime_ns();
            if (!deadline || now < *deadline) {
                auto status = tx_semaphore_get(&w.event, deadline ? deadline_ticks(*deadline - now)
                                                                  : TX_WAIT_FOREVER);
                if (status != TX_SUCCESS && status != TX_NO_INSTANCE) {
                    std::abort();
                }
            }
            critical_section guard;
            if (w.signaled) {
                break;
            }
            if (deadline && (steady ? monotonic_ns() : realtime_ns()) >= *deadline) {
                remove(&w);
                result = ETIMEDOUT;
                break;
            }
        }
        check(tx_semaphore_delete(&w.event));
        if (m->get(TX_WAIT_FOREVER)) {
            std::abort();
        }
        return result;
    }
};
} // namespace rtos_stdcpp
