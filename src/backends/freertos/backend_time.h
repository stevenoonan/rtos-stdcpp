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
#include <cstdint>
#include <limits>
namespace free_rtos_std {
// No more than half a tick-counter range per blocking call. portMAX_DELAY
// means infinity in several kernel APIs and must never represent a deadline.
constexpr TickType_t deadline_ticks(int64_t ns) {
  constexpr uint64_t quantum = 1000000000ULL / configTICK_RATE_HZ;
  static_assert(1000000000ULL % configTICK_RATE_HZ == 0);
  constexpr uint64_t limit = portMAX_DELAY / 2;
  if (ns <= 0) return 0;
  auto ticks = uint64_t(ns) / quantum + (uint64_t(ns) % quantum != 0) + 1;
  return TickType_t(ticks > limit ? limit : ticks);
}
constexpr uint64_t extend_ticks(uint32_t wraps, TickType_t ticks) {
  static_assert(sizeof(TickType_t) == 4, "This clock implementation requires 32-bit ticks");
  return (uint64_t(wraps) << 32) | ticks;
}
int64_t monotonic_ns();
int64_t realtime_ns();
void sleep_ns(int64_t ns);
}
