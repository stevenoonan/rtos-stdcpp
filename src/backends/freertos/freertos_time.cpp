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

#include "freertos_time.h"
#include "backend_time.h"
#include "critical_section.h"
#include <sys/time.h>
#include <cerrno>

namespace free_rtos_std {
static int64_t wall_offset;
int64_t monotonic_ns() {
  if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) return 0;
  TimeOut_t sample;
  vTaskSetTimeOutState(&sample);
  return int64_t(extend_ticks(uint32_t(sample.xOverflowCount), sample.xTimeOnEntering))
       * (1000000000LL / configTICK_RATE_HZ);
}
int64_t realtime_ns() {
  if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) return wall_offset;
  critical_section guard;
  return monotonic_ns() + wall_offset;
}
void sleep_ns(int64_t ns) {
  if (ns <= 0) return;
  configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
  auto now = monotonic_ns();
  const auto end = ns > INT64_MAX - now ? INT64_MAX : now + ns;
  while ((now = monotonic_ns()) < end) vTaskDelay(deadline_ticks(end - now));
}
}
void SetSystemClockTime(const std::chrono::system_clock::time_point &time) {
  auto value = std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count();
  if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
    free_rtos_std::wall_offset = value;
    return;
  }
  free_rtos_std::critical_section guard;
  free_rtos_std::wall_offset = value - free_rtos_std::monotonic_ns();
}
namespace std { namespace chrono { inline namespace _V2 {
steady_clock::time_point steady_clock::now() noexcept {
  return time_point(duration(free_rtos_std::monotonic_ns()));
}
system_clock::time_point system_clock::now() noexcept {
  return time_point(duration(free_rtos_std::realtime_ns()));
}
}}}
extern "C" int _gettimeofday(timeval *tv, void *) {
  if (!tv) { errno = EINVAL; return -1; }
  auto ns = free_rtos_std::realtime_ns();
  auto sec = ns / 1000000000;
  auto usec = (ns % 1000000000) / 1000;
  if (usec < 0) { --sec; usec += 1000000; }
  tv->tv_sec = sec;
  tv->tv_usec = usec;
  return 0;
}
