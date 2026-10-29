// SPDX-License-Identifier: MIT
// Test the public allocator contract using the separate newlib/C++ arena.
#include <rtos_stdcpp/tls_allocator.h>
#include "FreeRTOS.h"
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <new>

namespace {
constexpr uint32_t cookie = 0x71534c54;
struct alignas(std::max_align_t) Header { uint32_t marker; };
std::atomic<unsigned> live{0}, allocations{0};
}

extern "C" void *rtos_stdcpp_tls_allocate(std::size_t bytes) noexcept {
    if (bytes > std::numeric_limits<std::size_t>::max() - sizeof(Header)) return nullptr;
    auto *storage = std::malloc(sizeof(Header) + bytes);
    if (!storage) return nullptr;
    auto *header = ::new (storage) Header{cookie};
    ++live; ++allocations;
    return header + 1;
}

extern "C" void rtos_stdcpp_tls_deallocate(void *pointer) noexcept {
    configASSERT(pointer);
    auto *header = static_cast<Header *>(pointer) - 1;
    configASSERT(header->marker == cookie);
    header->marker = 0;
    auto previous = live.fetch_sub(1);
    configASSERT(previous != 0);
    std::free(header);
}

extern "C" unsigned rtos_test_tls_live_allocations() { return live.load(); }
extern "C" unsigned rtos_test_tls_allocation_count() { return allocations.load(); }
