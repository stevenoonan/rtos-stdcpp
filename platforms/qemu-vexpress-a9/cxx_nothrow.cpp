// SPDX-License-Identifier: MIT
// Generic platform runtime support.
#include <new>
#include <cstdlib>
#include <malloc.h>

// The shipped no-exceptions nano wrappers call the throwing operator new and
// abort on OOM. Supply the recoverable forms explicitly. Configure new_handler
// only during startup; handlers must return after making memory available or
// fail fast, since exception propagation is disabled.
namespace {
void *allocate(std::size_t size) noexcept {
    if (!size) size = 1;
    for (;;) {
        if (auto *p = std::malloc(size)) return p;
        auto handler = std::get_new_handler();
        if (!handler) return nullptr;
        handler();
    }
}
}
void *operator new(std::size_t size, const std::nothrow_t &) noexcept { return allocate(size); }
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept { return allocate(size); }

namespace {
void *allocate_aligned(std::size_t size, std::align_val_t alignment) noexcept {
    if (!size) size = 1;
    for (;;) {
        if (auto *p = memalign(static_cast<std::size_t>(alignment), size)) return p;
        auto handler = std::get_new_handler();
        if (!handler) return nullptr;
        handler();
    }
}
}
void *operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    return allocate_aligned(size, alignment);
}
void *operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    return allocate_aligned(size, alignment);
}
