/// Copyright 2018-2023 Piotr Grygorczuk <grygorek@gmail.com>
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

#ifndef _GTHR_FREERTOS_X__H_
#define _GTHR_FREERTOS_X__H_

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "thread_gthread.h"
#include "condition_variable.h"
#include "gthr_key.h"

#include <sys/time.h>
#include <time.h>
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 4 // newlib clock ID, hidden unless POSIX timers are enabled
#endif
#include "backend_time.h"

typedef free_rtos_std::gthr_freertos __gthread_t;

namespace free_rtos_std
{
  struct RecursiveMutex {
    SemaphoreHandle_t value = nullptr;
    operator SemaphoreHandle_t() const { return value; }
    RecursiveMutex &operator=(SemaphoreHandle_t handle) { value = handle; return *this; }
  };
  struct Once
  {
    bool v = false;
    SemaphoreHandle_t m = xSemaphoreCreateMutex();
    ~Once() { vSemaphoreDelete(m); }
  };
}

extern "C"
{

#define __GTHREAD_COND_INIT_FUNCTION
#define __GTHREADS 1

  // returns: 1 - thread system is active; 0 - thread system is not active
  static int __gthread_active_p() { return 1; }

  typedef free_rtos_std::Key *__gthread_key_t;
  typedef free_rtos_std::Once __gthread_once_t;
  typedef SemaphoreHandle_t __gthread_mutex_t;
  typedef free_rtos_std::RecursiveMutex __gthread_recursive_mutex_t;
  typedef free_rtos_std::cv_task_list __gthread_cond_t;

#define __GTHREAD_ONCE_INIT free_rtos_std::Once()

  static inline void __GTHREAD_RECURSIVE_MUTEX_INIT_FUNCTION(
      __gthread_recursive_mutex_t *mutex)
  {
    *mutex = xSemaphoreCreateRecursiveMutex();
    configASSERT(*mutex);
  }
  static inline void __GTHREAD_MUTEX_INIT_FUNCTION(__gthread_mutex_t *mutex)
  {
    *mutex = xSemaphoreCreateMutex();
    configASSERT(*mutex);
  }

  static int __gthread_once(__gthread_once_t *once, void (*func)(void))
  {
    if (!once->m)
      return 12; // POSIX error: ENOMEM

    bool flag{true};
    xSemaphoreTake(once->m, portMAX_DELAY);
    std::swap(once->v, flag);
    if (flag == false)
      func();
    xSemaphoreGive(once->m);

    return 0;
  }

  static int __gthread_key_create(__gthread_key_t *keyp, void (*dtor)(void *))
  {
    return free_rtos_std::freertos_gthread_key_create(keyp, dtor);
  }

  static int __gthread_key_delete(__gthread_key_t key)
  {
    return free_rtos_std::freertos_gthread_key_delete(key);
  }

  static void *__gthread_getspecific(__gthread_key_t key)
  {
    return free_rtos_std::freertos_gthread_getspecific(key);
  }

  static int __gthread_setspecific(__gthread_key_t key, const void *ptr)
  {
    return free_rtos_std::freertos_gthread_setspecific(key, ptr);
  }
  //////////

  //////////
  static inline int __gthread_mutex_destroy(__gthread_mutex_t *mutex)
  {
    vSemaphoreDelete(*mutex);
    return 0;
  }
  static inline int __gthread_recursive_mutex_destroy(
      __gthread_recursive_mutex_t *mutex)
  {
    vSemaphoreDelete(*mutex);
    return 0;
  }

  static inline int __gthread_mutex_lock(__gthread_mutex_t *mutex)
  {
    return (xSemaphoreTake(*mutex, portMAX_DELAY) == pdTRUE) ? 0 : 1;
  }
  static inline int __gthread_mutex_trylock(__gthread_mutex_t *mutex)
  {
    return (xSemaphoreTake(*mutex, 0) == pdTRUE) ? 0 : 1;
  }
  static inline int __gthread_mutex_unlock(__gthread_mutex_t *mutex)
  {
    return (xSemaphoreGive(*mutex) == pdTRUE) ? 0 : 1;
  }

  static inline int __gthread_recursive_mutex_lock(
      __gthread_recursive_mutex_t *mutex)
  {
    return (xSemaphoreTakeRecursive(*mutex, portMAX_DELAY) == pdTRUE) ? 0 : 1;
  }
  static inline int __gthread_recursive_mutex_trylock(
      __gthread_recursive_mutex_t *mutex)
  {
    return (xSemaphoreTakeRecursive(*mutex, 0) == pdTRUE) ? 0 : 1;
  }
  static inline int __gthread_recursive_mutex_unlock(
      __gthread_recursive_mutex_t *mutex)
  {
    return (xSemaphoreGiveRecursive(*mutex) == pdTRUE) ? 0 : 1;
  }
////////////

////////////
#define __GTHREADS_CXX0X 1

  struct __gthread_time_t
  {
    time_t sec;
    long nsec;
    int64_t milliseconds() const
    {
      return int64_t(sec) * 1000 + nsec / 1'000'000;
    }
  };

  static inline int __gthread_mutex_timedlock(
      __gthread_mutex_t *m, const __gthread_time_t *abs_timeout)
  {
    const int64_t end = int64_t(abs_timeout->sec) * 1000000000 + abs_timeout->nsec;
    do {
      auto left = end - free_rtos_std::realtime_ns();
      if (xSemaphoreTake(*m, free_rtos_std::deadline_ticks(left)) == pdTRUE) return 0;
    } while (free_rtos_std::realtime_ns() < end);
    return ETIMEDOUT;
  }

  static inline int __gthread_recursive_mutex_timedlock(
      __gthread_recursive_mutex_t *m, const __gthread_time_t *abs_time)
  {
    const int64_t end = int64_t(abs_time->sec) * 1000000000 + abs_time->nsec;
    do {
      auto left = end - free_rtos_std::realtime_ns();
      if (xSemaphoreTakeRecursive(*m, free_rtos_std::deadline_ticks(left)) == pdTRUE) return 0;
    } while (free_rtos_std::realtime_ns() < end);
    return ETIMEDOUT;
  }

  // All functions returning int should return zero on success or the error
  //    number.  If the operation is not supported, -1 is returned.

  static inline int __gthread_create(__gthread_t *thread, void (*func)(void *),
                                     void *args)
  {
    return thread->create_thread(func, args) ? 0 : 1;
  }
  static inline int __gthread_join(__gthread_t &thread, void **value_ptr)
  {
    return thread.join();
  }
  static inline int __gthread_detach(__gthread_t &thread)
  {
    return thread.detach();
  }
  static inline int __gthread_equal(const __gthread_t &t1,
                                    const __gthread_t &t2)
  {
    return t1 == t2 ? 1 : 0;
  }
  static inline __gthread_t __gthread_self(void) { return __gthread_t::self(); }

#define _GLIBCXX_USE_SCHED_YIELD

  static inline int __gthread_yield(void)
  {
    taskYIELD();
    return 0;
  }

  static inline int __gthread_cond_signal(__gthread_cond_t *cond)
  {
    cond->signal(false);
    return 0;
  }
  static inline int __gthread_cond_broadcast(__gthread_cond_t *cond) {
    cond->signal(true);
    return 0;
  }
  static inline int __gthread_cond_destroy(__gthread_cond_t *) { return 0; }
  static inline int __gthread_cond_wait(__gthread_cond_t *cond, __gthread_mutex_t *mutex) {
    return cond->wait(mutex, nullptr, false);
  }
  static inline int __gthread_cond_timedwait(__gthread_cond_t *cond,
      __gthread_mutex_t *mutex, const __gthread_time_t *time) {
    const int64_t end = int64_t(time->sec) * 1000000000 + time->nsec;
    return cond->wait(mutex, &end, false);
  }
  // GCC 14's std::__condvar names this POSIX entry point directly. This inline
  // compatibility hook implements only its clock-selecting wait, not pthreads.
  static inline int pthread_cond_clockwait(__gthread_cond_t *cond,
      __gthread_mutex_t *mutex, clockid_t clock, const __gthread_time_t *time) {
    if (clock != CLOCK_MONOTONIC && clock != CLOCK_REALTIME) return EINVAL;
    const int64_t end = int64_t(time->sec) * 1000000000 + time->nsec;
    return cond->wait(mutex, &end, clock == CLOCK_MONOTONIC);
  }
} // extern "C"

static inline int freertos_mutex_clocklock(SemaphoreHandle_t mutex, bool recursive,
    clockid_t clock, const __gthread_time_t *time) {
  if (clock != CLOCK_MONOTONIC && clock != CLOCK_REALTIME) return EINVAL;
  const int64_t end = int64_t(time->sec) * 1000000000 + time->nsec;
  auto now = [clock] { return clock == CLOCK_MONOTONIC
                           ? free_rtos_std::monotonic_ns() : free_rtos_std::realtime_ns(); };
  do {
    auto ticks = free_rtos_std::deadline_ticks(end - now());
    if ((recursive ? xSemaphoreTakeRecursive(mutex, ticks) : xSemaphoreTake(mutex, ticks)) == pdTRUE)
      return 0;
  } while (now() < end);
  return ETIMEDOUT;
}
static inline int pthread_mutex_clocklock(__gthread_mutex_t *mutex,
    clockid_t clock, const __gthread_time_t *time) {
  return freertos_mutex_clocklock(*mutex, false, clock, time);
}
static inline int pthread_mutex_clocklock(__gthread_recursive_mutex_t *mutex,
    clockid_t clock, const __gthread_time_t *time) {
  return freertos_mutex_clocklock(*mutex, true, clock, time);
}
#endif
