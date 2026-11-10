// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>

// Keep the faulty access observable in Release builds; this executable is run
// only by the negative-control checker, never as a passing application test.
int main(int argc, char**) {
    auto* data = static_cast<unsigned char*>(std::malloc(16));
    if (!data) {
        return 2;
    }
    data[0] = 42;
    auto* volatile saved = data;
    std::free(data);
    std::fputs("RTOS_STDCPP_ARMED asan_use_after_free\n", stderr);
    std::fflush(stderr);
    return saved[static_cast<unsigned>(argc) - 1];
}
