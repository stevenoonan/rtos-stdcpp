#pragma once

#include "FreeRTOS.h"

namespace rtos_stdcpp {
inline void* backend_allocate(size_t n) {
    return pvPortMalloc(n);
}

inline void backend_deallocate(void* p) {
    vPortFree(p);
}
} // namespace rtos_stdcpp
