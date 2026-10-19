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

#pragma once
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "critical_section.h"
#include "backend_time.h"
#include <cerrno>

namespace free_rtos_std {
// Intrusive stack waiters allocate no list nodes. Index zero is exclusively
// reserved for this protocol; application notifications must use another index.
class cv_task_list {
  struct waiter {
    TaskHandle_t task;
    waiter *next;
    bool signaled;
  };
  waiter *head = nullptr;
  waiter *tail = nullptr;
  void remove(waiter *item) {
    waiter *previous = nullptr;
    for (auto *p = head; p; p = p->next) {
      if (p == item) {
        if (previous) previous->next = p->next; else head = p->next;
        if (tail == p) tail = previous;
        return;
      }
      previous = p;
    }
  }
public:
  constexpr cv_task_list() = default;
  cv_task_list(const cv_task_list &) = delete;
  cv_task_list &operator=(const cv_task_list &) = delete;
  ~cv_task_list() { configASSERT(head == nullptr); }
  void signal(bool all) {
    critical_section guard;
    while (head) {
      auto *p = head;
      head = p->next;
      if (!head) tail = nullptr;
      p->signaled = true;
      xTaskNotifyGiveIndexed(p->task, 0);
      if (!all) break;
    }
  }
  int wait(SemaphoreHandle_t *mutex, const int64_t *deadline, bool steady) {
    waiter self{xTaskGetCurrentTaskHandle(), nullptr, false};
    {
      critical_section guard;
      // Drain only our reserved notification before publishing the waiter.
      ulTaskNotifyTakeIndexed(0, pdTRUE, 0);
      if (tail) tail->next = &self; else head = &self;
      tail = &self;
    }
    auto semaphore_result = xSemaphoreGive(*mutex);
    configASSERT(semaphore_result == pdTRUE);
    (void)semaphore_result;
    int result = 0;
    for (;;) {
      auto remaining = deadline ? *deadline - (steady ? monotonic_ns() : realtime_ns()) : 0;
      if (!deadline || remaining > 0)
        ulTaskNotifyTakeIndexed(0, pdTRUE, deadline ? deadline_ticks(remaining) : portMAX_DELAY);
      critical_section guard;
      if (self.signaled) break;
      if (deadline && (steady ? monotonic_ns() : realtime_ns()) >= *deadline) {
        remove(&self);
        result = ETIMEDOUT;
        break;
      }
    }
    // A signal and timeout are resolved under the same critical section; the
    // notifier cannot retain this stack address or leave a stale notification.
    ulTaskNotifyTakeIndexed(0, pdTRUE, 0);
    semaphore_result = xSemaphoreTake(*mutex, portMAX_DELAY);
    configASSERT(semaphore_result == pdTRUE);
    return result;
  }
};
}
