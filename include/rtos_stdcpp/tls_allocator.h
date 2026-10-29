// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>

// Supply both functions when building the adapter with
// RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR=ON. They serve TLS values, pointer tables,
// and destructor records. Allocation must be thread-safe, return normally
// aligned storage (at least alignof(std::max_align_t), or nullptr). Both hooks
// must avoid accessing language thread_local, including indirectly.
// The runtime handles extended alignment and terminates on allocation failure.
// Deallocation receives only non-null pointers returned by the paired allocator.
extern "C" {
void* rtos_stdcpp_tls_allocate(std::size_t bytes) noexcept;
void rtos_stdcpp_tls_deallocate(void* pointer) noexcept;
}
