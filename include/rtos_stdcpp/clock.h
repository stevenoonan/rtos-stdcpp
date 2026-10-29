#pragma once

#include <chrono>

namespace rtos_stdcpp {

void set_system_clock_time(const std::chrono::system_clock::time_point& time);

} // namespace rtos_stdcpp
