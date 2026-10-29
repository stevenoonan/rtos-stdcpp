#pragma once

#include <cstdlib>

namespace rtos_stdcpp {
inline void* backend_allocate(size_t n) {
    return std::malloc(n);
}

inline void backend_deallocate(void* p) {
    std::free(p);
}
} // namespace rtos_stdcpp
