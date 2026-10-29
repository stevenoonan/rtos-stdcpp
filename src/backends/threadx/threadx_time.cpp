// SPDX-License-Identifier: MIT
#include "rtos_stdcpp/clock.h"
#include "backend_time.h"
#include "critical_section.h"
#include <sys/time.h>
#include <cerrno>

namespace rtos_stdcpp {
static volatile uint64_t ticks;
static int64_t wall_offset;

int64_t monotonic_ns() {
    critical_section guard;
    return int64_t(ticks) * (1000000000LL / TX_TIMER_TICKS_PER_SECOND);
}

int64_t realtime_ns() {
    critical_section guard;
    return monotonic_ns() + wall_offset;
}

void sleep_ns(int64_t ns) {
    if (ns <= 0) {
        return;
    }
    if (!tx_thread_identify()) {
        std::abort();
    }
    auto now = monotonic_ns();
    const auto end = ns > INT64_MAX - now ? INT64_MAX : now + ns;
    while ((now = monotonic_ns()) < end) {
        check(tx_thread_sleep(deadline_ticks(end - now)));
    }
}
} // namespace rtos_stdcpp

extern "C" void rtos_stdcpp_threadx_tick() {
    rtos_stdcpp::ticks = rtos_stdcpp::ticks + 1;
}

void rtos_stdcpp::set_system_clock_time(const std::chrono::system_clock::time_point& time) {
    rtos_stdcpp::critical_section guard;
    rtos_stdcpp::wall_offset =
        std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count() -
        rtos_stdcpp::monotonic_ns();
}

namespace std {
namespace chrono {
inline namespace _V2 {
steady_clock::time_point steady_clock::now() noexcept {
    return time_point(duration(rtos_stdcpp::monotonic_ns()));
}

system_clock::time_point system_clock::now() noexcept {
    return time_point(duration(rtos_stdcpp::realtime_ns()));
}
} // namespace _V2
} // namespace chrono
} // namespace std

extern "C" int _gettimeofday(timeval* tv, void*) {
    if (!tv) {
        errno = EINVAL;
        return -1;
    }
    auto ns = rtos_stdcpp::realtime_ns();
    auto sec = ns / 1000000000;
    auto usec = (ns % 1000000000) / 1000;
    if (usec < 0) {
        --sec;
        usec += 1000000;
    }
    tv->tv_sec = sec;
    tv->tv_usec = usec;
    return 0;
}

#ifdef RTOS_STDCPP_TEST_CLOCK
extern "C" void rtos_stdcpp_threadx_test_seed_ticks(uint64_t value) {
    rtos_stdcpp::critical_section guard;
    rtos_stdcpp::ticks = value;
    tx_time_set(ULONG(value));
}
#endif
