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
#include <cstdint>
#include <cerrno>
#include <new>

#if __cplusplus > 201703L
#include <compare>
#endif

namespace free_rtos_std
{
  namespace internal
  {
    // Attributes are consumed synchronously by xTaskCreate. No operation in
    // this scope waits for the worker to start, including a thread move.
    struct attributes_lock : scheduler_section
    {
      explicit attributes_lock(const attributes &attr) : _previous(_attrib) { _attrib = &attr; }
      ~attributes_lock() { _attrib = _previous; }
      static const attributes *_attrib;
    private:
      const attributes *_previous;
      static constexpr attributes _default{};
    };
  }

  class gthr_freertos
  {
    struct completion
    {
      EventGroupHandle_t event;
      TaskHandle_t task;
      void *argument;
      unsigned references;
    };
    static constexpr EventBits_t completed = 1;
    static constexpr BaseType_t tls_slot = 0;

    // A single canonical word makes IDs, copies and GCC's raw-byte ID hashes
    // agree. Managed IDs name the completion block, not a reusable/deleted TCB.
    // Native (non-adapter) tasks use a tagged TCB address; aligned blocks have
    // the low bit clear. ID copies are non-owning, as required by std::thread::id.
    uintptr_t _identity{0};
    explicit constexpr gthr_freertos(uintptr_t identity) : _identity(identity) {}

    completion *state() const { return reinterpret_cast<completion *>(_identity); }
    bool managed() const { return _identity != 0 && (_identity & 1U) == 0; }

    static void release(completion *shared)
    {
      bool last;
      {
        critical_section guard;
        configASSERT(shared->references != 0);
        last = --shared->references == 0;
      }
      if (last)
      {
        // No references remain. Deallocation may suspend scheduling, but must
        // not keep timer IRQs masked while walking allocator/kernel lists.
        vEventGroupDelete(shared->event);
        vPortFree(shared);
      }
    }

  public:
    using task_foo = void (*)(void *);
    using native_task_type = TaskHandle_t;
    constexpr gthr_freertos() = default;
    // Copy/move/default destruction never transfer or release ownership. Only
    // create/join/detach/worker-exit own references; std::thread enforces the
    // requirement to join/detach before destruction or move-assignment.

    bool create_thread(task_foo function, void *argument)
    {
      void *storage = pvPortMalloc(sizeof(completion));
      if (!storage)
        return false;
      auto *shared = ::new (storage) completion{};
      shared->event = xEventGroupCreate();
      if (!shared->event)
      {
        vPortFree(shared);
        return false;
      }
      shared->argument = argument;
      shared->task = nullptr;
      shared->references = 2; // std::thread owner and worker
      {
        scheduler_section guard;
        const auto &attr = *internal::attributes_lock::_attrib;
        // The stable shared block is the entry argument. Nothing in the worker
        // depends on the address/lifetime of a std::thread object or temporary.
        if (xTaskCreate(function, attr.taskName, attr.stackWordCount, shared,
                        attr.priority, &shared->task) == pdPASS)
        {
          vTaskSetThreadLocalStoragePointer(shared->task, tls_slot, shared);
          _identity = reinterpret_cast<uintptr_t>(shared);
          configASSERT((_identity & 1U) == 0);
          return true;
        }
      }
      vEventGroupDelete(shared->event);
      vPortFree(shared);
      return false;
    }

    int join()
    {
      if (!managed())
        return EINVAL;
      if (*this == self())
        return EDEADLK;
      auto *shared = state();
      while ((xEventGroupWaitBits(shared->event, completed, pdFALSE, pdTRUE, portMAX_DELAY)
              & completed) == 0) {}
      release(shared);
      _identity = 0;
      return 0;
    }

    int detach()
    {
      if (!managed())
        return EINVAL;
      release(state());
      _identity = 0;
      return 0;
    }

    static gthr_freertos from_start_argument(void *argument)
    {
      return gthr_freertos{reinterpret_cast<uintptr_t>(argument)};
    }

    void *arg() const { return state()->argument; }

    [[noreturn]] void notify_joined()
    {
      auto *shared = state();
      vTaskSetThreadLocalStoragePointer(nullptr, tls_slot, nullptr);
      // Keep the worker reference until set-bits returns: a newly unblocked
      // joiner can run immediately and release its own reference in the meantime.
      xEventGroupSetBits(shared->event, completed);
      release(shared);
      // The owner never inspects/deletes this TCB. Kernel idle cleanup owns it.
      vTaskDelete(nullptr);
      for (;;) {} // vTaskDelete's C declaration does not carry noreturn.
    }

    static gthr_freertos self()
    {
      auto task = xTaskGetCurrentTaskHandle();
      auto shared = pvTaskGetThreadLocalStoragePointer(task, tls_slot);
      return gthr_freertos{shared ? reinterpret_cast<uintptr_t>(shared)
                                : reinterpret_cast<uintptr_t>(task) | 1U};
    }
    static native_task_type native_task_handle() { return xTaskGetCurrentTaskHandle(); }

    constexpr friend bool operator==(gthr_freertos left, gthr_freertos right) noexcept
    { return left._identity == right._identity; }
#if __cpp_lib_three_way_comparison
    constexpr friend std::strong_ordering operator<=>(gthr_freertos left, gthr_freertos right) noexcept
    { return left._identity <=> right._identity; }
#else
    constexpr friend bool operator!=(gthr_freertos left, gthr_freertos right) noexcept
    { return !(left == right); }
    constexpr friend bool operator<(gthr_freertos left, gthr_freertos right) noexcept
    { return left._identity < right._identity; }
#endif
  };
}
#endif
