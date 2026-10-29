// SPDX-License-Identifier: MIT
#pragma once
#include <atomic>

namespace rtos_test::tls_test {
inline thread_local unsigned cross_unit = 41;
#ifdef RTOS_STDCPP_TEST_EMBEDDED
extern thread_local unsigned external;
unsigned* external_address();
#endif
unsigned* peer_address();
void peer_lifetime(std::atomic<unsigned>& destroyed);
void check_common();
void check_dynamic();
} // namespace rtos_test::tls_test
