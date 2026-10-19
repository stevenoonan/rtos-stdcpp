// SPDX-License-Identifier: MIT
#include "lifecycle_regression.h"
#include <thread>
#include <atomic>
#include <functional>
#include "FreeRTOS.h"
#include "task.h"

namespace
{
void await(const std::atomic<bool> &flag)
{
  for (unsigned count = 0; !flag.load(std::memory_order_acquire); ++count)
  {
    configASSERT(count < 1000);
    vTaskDelay(1);
  }
}
}

uint32_t freertos_cpp11_lifecycle_regression(void)
{
  uint32_t passed = 0;
  auto self = __gthread_self();
  configASSERT(__gthread_equal(self, self) != 0);
  configASSERT(__gthread_equal(self, __gthread_t{}) == 0);
  passed |= 1;

  // A completed owner's identity must stay valid through idle reclamation and
  // task allocation churn. Joining must never delete an unrelated reused TCB.
  std::atomic<bool> done{false}, stop{false};
  std::atomic<unsigned> progress{0};
  std::thread finished([&] { done.store(true, std::memory_order_release); });
  auto finished_id = finished.get_id();
  await(done);
  vTaskDelay(pdMS_TO_TICKS(20));
  std::thread survivor([&] {
    while (!stop.load()) { ++progress; vTaskDelay(1); }
  });
  configASSERT(finished_id != survivor.get_id());
  std::thread moved;
  moved = std::move(finished);
  configASSERT(!finished.joinable() && moved.get_id() == finished_id);
  moved.join();
  auto before = progress.load();
  vTaskDelay(pdMS_TO_TICKS(20));
  configASSERT(progress.load() > before);
  stop.store(true);
  survivor.join();
  passed |= 2;

  // Detach after completion/idle cleanup, including moving the finished owner.
  done.store(false);
  std::thread completed([&] { done.store(true, std::memory_order_release); });
  await(done);
  vTaskDelay(pdMS_TO_TICKS(20));
  std::thread moved_completed(std::move(completed));
  moved_completed.detach();
  configASSERT(!moved_completed.joinable());
  passed |= 4;

  // Detach before completion, destroy the owner, then let the worker return.
  done.store(false);
  {
    std::thread detached([&] {
      vTaskDelay(pdMS_TO_TICKS(10));
      done.store(true, std::memory_order_release);
    });
    detached.detach();
    configASSERT(!detached.joinable());
  }
  await(done);
  vTaskDelay(pdMS_TO_TICKS(20));
  passed |= 8;

  // The creator can inspect/copy/move an ID before the worker runs. The worker
  // must see the same identity and hash, regardless of ownership and copies.
  std::thread::id worker_id;
  size_t worker_hash = 0;
  std::thread identity([&] {
    worker_id = std::this_thread::get_id();
    worker_hash = std::hash<std::thread::id>{}(worker_id);
    configASSERT(__gthread_equal(__gthread_self(), __gthread_self()) != 0);
  });
  auto copied = identity.get_id();
  std::thread relocated(std::move(identity));
  auto expected_hash = std::hash<std::thread::id>{}(copied);
  relocated.join();
  configASSERT(copied == worker_id && expected_hash == worker_hash);
  configASSERT(!identity.joinable());
  passed |= 16;

  vTaskDelay(pdMS_TO_TICKS(20));
  return passed;
}
