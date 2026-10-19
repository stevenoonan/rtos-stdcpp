// SPDX-License-Identifier: MIT
#include "lifecycle_regression.h"
#include <thread>
#include <atomic>
#include <functional>
#include "freertos/regression_platform.h"
using namespace rtos_test::regression_platform;

namespace
{
void await(const std::atomic<bool> &flag)
{
  for (unsigned count = 0; !flag.load(std::memory_order_acquire); ++count)
  {
    REGRESSION_CHECK(count < 1000);
    idle_ticks(1);
  }
}
}

uint32_t freertos_cpp11_lifecycle_regression(void)
{
#ifdef RTOS_STDCPP_TEST_FREERTOS
  rtos_test::reset(rtos_test::rtos_stdcpp_regression_report, rtos_test::Suite::regression);
  rtos_test::begin(rtos_test::rtos_stdcpp_regression_report, rtos_test::Case::lifecycle_identity);
#endif
  uint32_t passed = 0;
  auto self = __gthread_self();
  REGRESSION_CHECK(__gthread_equal(self, self) != 0);
  REGRESSION_CHECK(__gthread_equal(self, __gthread_t{}) == 0);
  passed |= 1;

  // A completed owner's identity must stay valid through idle reclamation and
  // task allocation churn. Joining must never delete an unrelated reused TCB.
  std::atomic<bool> done{false}, stop{false};
  std::atomic<unsigned> progress{0};
  std::thread finished([&] { done.store(true, std::memory_order_release); });
  auto finished_id = finished.get_id();
  await(done);
  delay_ms(20);
  std::thread survivor([&] {
    while (!stop.load()) { ++progress; idle_ticks(1); }
  });
  REGRESSION_CHECK(finished_id != survivor.get_id());
  std::thread moved;
  moved = std::move(finished);
  REGRESSION_CHECK(!finished.joinable() && moved.get_id() == finished_id);
  moved.join();
  auto before = progress.load();
  delay_ms(20);
  REGRESSION_CHECK(progress.load() > before);
  stop.store(true);
  survivor.join();
  passed |= 2;

  // Detach after completion/idle cleanup, including moving the finished owner.
  done.store(false);
  std::thread completed([&] { done.store(true, std::memory_order_release); });
  await(done);
  delay_ms(20);
  std::thread moved_completed(std::move(completed));
  moved_completed.detach();
  REGRESSION_CHECK(!moved_completed.joinable());
  passed |= 4;

  // Detach before completion, destroy the owner, then let the worker return.
  done.store(false);
  {
    std::thread detached([&] {
      delay_ms(10);
      done.store(true, std::memory_order_release);
    });
    detached.detach();
    REGRESSION_CHECK(!detached.joinable());
  }
  await(done);
  delay_ms(20);
  passed |= 8;

  // The creator can inspect/copy/move an ID before the worker runs. The worker
  // must see the same identity and hash, regardless of ownership and copies.
  std::thread::id worker_id;
  size_t worker_hash = 0;
  std::thread identity([&] {
    worker_id = std::this_thread::get_id();
    worker_hash = std::hash<std::thread::id>{}(worker_id);
    REGRESSION_CHECK(__gthread_equal(__gthread_self(), __gthread_self()) != 0);
  });
  auto copied = identity.get_id();
  std::thread relocated(std::move(identity));
  auto expected_hash = std::hash<std::thread::id>{}(copied);
  relocated.join();
  REGRESSION_CHECK(copied == worker_id && expected_hash == worker_hash);
  REGRESSION_CHECK(!identity.joinable());
  passed |= 16;

  delay_ms(20);
#ifdef RTOS_STDCPP_TEST_FREERTOS
  rtos_test::end(rtos_test::rtos_stdcpp_regression_report, rtos_test::Case::lifecycle_identity);
  if (!rtos_test::finish(rtos_test::rtos_stdcpp_regression_report)) rtos_test::require_failed(__FILE__, __LINE__);
#endif
  return passed;
}
