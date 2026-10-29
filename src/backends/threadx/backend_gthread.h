// SPDX-License-Identifier: MIT
#pragma once

#include "thread_gthread.h"
#include "condition_variable.h"
#include "gthr_key.h"
#include <sys/time.h>
#include <time.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 4
#endif
#define __GTHREADS 1
#define __GTHREADS_CXX0X 1
#define __GTHREAD_COND_INIT_FUNCTION
#define _GLIBCXX_USE_SCHED_YIELD
using __gthread_t = rtos_stdcpp::gthr_threadx;
using __gthread_key_t = rtos_stdcpp::Key*;
using __gthread_once_t = rtos_stdcpp::Once;
using __gthread_mutex_t = rtos_stdcpp::mutex;
using __gthread_recursive_mutex_t = rtos_stdcpp::recursive_mutex;
using __gthread_cond_t = rtos_stdcpp::cv_task_list;
#define __GTHREAD_ONCE_INIT rtos_stdcpp::Once()
extern "C" {
static inline int __gthread_active_p() {
    return 1;
}

static inline void __GTHREAD_MUTEX_INIT_FUNCTION(__gthread_mutex_t* m) {
    ::new (m) __gthread_mutex_t{};
}

static inline void __GTHREAD_RECURSIVE_MUTEX_INIT_FUNCTION(__gthread_recursive_mutex_t* m) {
    ::new (m) __gthread_recursive_mutex_t{};
}

static inline int __gthread_mutex_destroy(__gthread_mutex_t* m) {
    m->destroy();
    return 0;
}

static inline int __gthread_recursive_mutex_destroy(__gthread_recursive_mutex_t* m) {
    m->destroy();
    return 0;
}

static inline int __gthread_mutex_lock(__gthread_mutex_t* m) {
    return m->get(TX_WAIT_FOREVER);
}

static inline int __gthread_mutex_trylock(__gthread_mutex_t* m) {
    return m->get(TX_NO_WAIT);
}

static inline int __gthread_mutex_unlock(__gthread_mutex_t* m) {
    return m->put();
}

static inline int __gthread_recursive_mutex_lock(__gthread_recursive_mutex_t* m) {
    return m->get(TX_WAIT_FOREVER);
}

static inline int __gthread_recursive_mutex_trylock(__gthread_recursive_mutex_t* m) {
    return m->get(TX_NO_WAIT);
}

static inline int __gthread_recursive_mutex_unlock(__gthread_recursive_mutex_t* m) {
    return m->put();
}

static inline int __gthread_once(__gthread_once_t* o, void (*f)()) {
    if (o->lock.get(TX_WAIT_FOREVER)) {
        return EINVAL;
    }
    if (!o->done) {
        f();
        o->done = true;
    }
    return o->lock.put();
}

static inline int __gthread_key_create(__gthread_key_t* k, void (*f)(void*)) {
    return rtos_stdcpp::freertos_gthread_key_create(k, f);
}

static inline int __gthread_key_delete(__gthread_key_t k) {
    return rtos_stdcpp::freertos_gthread_key_delete(k);
}

static inline void* __gthread_getspecific(__gthread_key_t k) {
    return rtos_stdcpp::freertos_gthread_getspecific(k);
}

static inline int __gthread_setspecific(__gthread_key_t k, const void* p) {
    return rtos_stdcpp::freertos_gthread_setspecific(k, p);
}

struct __gthread_time_t {
    time_t sec;
    long nsec;
};

static inline int __gthread_create(__gthread_t* t, void (*f)(void*), void* a) {
    return t->create_thread(f, a) ? 0 : EAGAIN;
}

static inline int __gthread_join(__gthread_t& t, void**) {
    return t.join();
}

static inline int __gthread_detach(__gthread_t& t) {
    return t.detach();
}

static inline int __gthread_equal(const __gthread_t& a, const __gthread_t& b) {
    return a == b;
}

static inline __gthread_t __gthread_self() {
    return __gthread_t::self();
}

static inline int __gthread_yield() {
    tx_thread_relinquish();
    return 0;
}

static inline int __gthread_cond_signal(__gthread_cond_t* c) {
    c->signal(false);
    return 0;
}

static inline int __gthread_cond_broadcast(__gthread_cond_t* c) {
    c->signal(true);
    return 0;
}

static inline int __gthread_cond_destroy(__gthread_cond_t*) {
    return 0;
}

static inline int __gthread_cond_wait(__gthread_cond_t* c, __gthread_mutex_t* m) {
    return c->wait(m, nullptr, false);
}

static inline int __gthread_cond_timedwait(__gthread_cond_t* c, __gthread_mutex_t* m,
                                           const __gthread_time_t* t) {
    const int64_t end = int64_t(t->sec) * 1000000000 + t->nsec;
    return c->wait(m, &end, false);
}

static inline int pthread_cond_clockwait(__gthread_cond_t* c, __gthread_mutex_t* m, clockid_t clock,
                                         const __gthread_time_t* t) {
    if (clock != CLOCK_MONOTONIC && clock != CLOCK_REALTIME) {
        return EINVAL;
    }
    const int64_t end = int64_t(t->sec) * 1000000000 + t->nsec;
    return c->wait(m, &end, clock == CLOCK_MONOTONIC);
}
}

static inline int threadx_mutex_clocklock(rtos_stdcpp::mutex* m, clockid_t clock,
                                          const __gthread_time_t* t) {
    if (clock != CLOCK_MONOTONIC && clock != CLOCK_REALTIME) {
        return EINVAL;
    }
    const int64_t end = int64_t(t->sec) * 1000000000 + t->nsec;
    auto now = [clock] {
        return clock == CLOCK_MONOTONIC ? rtos_stdcpp::monotonic_ns() : rtos_stdcpp::realtime_ns();
    };
    do {
        if (!m->get(rtos_stdcpp::deadline_ticks(end - now()))) {
            return 0;
        }
    } while (now() < end);
    return ETIMEDOUT;
}

static inline int pthread_mutex_clocklock(__gthread_mutex_t* m, clockid_t c,
                                          const __gthread_time_t* t) {
    return threadx_mutex_clocklock(m, c, t);
}

static inline int pthread_mutex_clocklock(__gthread_recursive_mutex_t* m, clockid_t c,
                                          const __gthread_time_t* t) {
    return threadx_mutex_clocklock(m, c, t);
}

static inline int __gthread_mutex_timedlock(__gthread_mutex_t* m, const __gthread_time_t* t) {
    return threadx_mutex_clocklock(m, CLOCK_REALTIME, t);
}

static inline int __gthread_recursive_mutex_timedlock(__gthread_recursive_mutex_t* m,
                                                      const __gthread_time_t* t) {
    return threadx_mutex_clocklock(m, CLOCK_REALTIME, t);
}
