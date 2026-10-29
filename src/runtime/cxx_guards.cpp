#include <cstdint>
#include "rtos_stdcpp/runtime.h"

namespace {
void lock() {
    rtos_stdcpp_guard_lock();
}

void unlock() {
    rtos_stdcpp_guard_unlock();
}
} // namespace

// ARM EABI guard is one 32-bit word. Bit 0 publishes initialization; bit 8
// detects recursive entry into the same initializer. A recursive mutex permits
// an initializer to initialize other statics, including ones that allocate.
extern "C" int __cxa_guard_acquire(int* guard) {
    lock();
    if (__atomic_load_n(guard, __ATOMIC_ACQUIRE) & 1U) {
        unlock();
        return 0;
    }
    RTOS_STDCPP_RUNTIME_REQUIRE((*guard & 0x100U) == 0U);
    *guard = 0x100U;
    return 1;
}

extern "C" void __cxa_guard_release(int* guard) {
    RTOS_STDCPP_RUNTIME_REQUIRE(*guard == 0x100U);
    __atomic_store_n(guard, 1U, __ATOMIC_RELEASE);
    unlock();
}

extern "C" void __cxa_guard_abort(int* guard) {
    RTOS_STDCPP_RUNTIME_REQUIRE(*guard == 0x100U);
    __atomic_store_n(guard, 0U, __ATOMIC_RELEASE);
    unlock();
}
