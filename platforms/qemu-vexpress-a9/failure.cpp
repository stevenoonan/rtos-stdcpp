// SPDX-License-Identifier: MIT
#include "FreeRTOS.h"
#include "task.h"
#include "console.h"
#include <cstdlib>
#include <new>
#include <thread>
namespace {
#if defined(FAIL_TLS_STARTUP) || defined(FAIL_TLS_NATIVE)
thread_local unsigned tls_value = 42;
unsigned *volatile tls_pointer;
void touch_tls() { tls_pointer = &tls_value; }
#endif
#ifdef FAIL_TLS_STARTUP
struct StartupTls {
    StartupTls() { print("RTOS_STDCPP_ARMED "); print(FAILURE_NAME); print("\n"); touch_tls(); }
} startup_tls;
#endif
#ifdef FAIL_RECURSIVE_GUARD
int local();
int recurse() { return local(); }
int local() { static int value = recurse(); return value; }
#endif
}
int main() {
    print("RTOS_STDCPP_ARMED "); print(FAILURE_NAME); print("\n");
#ifdef FAIL_THROWING_NEW
    void *volatile pointer = ::operator new(1024 * 1024);
    (void)pointer;
#elif defined(FAIL_KERNEL_ALLOC)
    void *volatile pointer = pvPortMalloc(configTOTAL_HEAP_SIZE * 2);
    (void)pointer;
#elif defined(FAIL_JOINABLE)
    { std::thread child([] { vTaskDelay(10); }); }
#elif defined(FAIL_RECURSIVE_GUARD)
    volatile int value = local(); (void)value;
#elif defined(FAIL_TLS_NATIVE)
    touch_tls();
#elif defined(FAIL_TLS_ALLOC)
    std::thread child([] {
        static thread_local unsigned char large[configTOTAL_HEAP_SIZE * 2];
        unsigned char *volatile pointer = large;
        (void)pointer;
    });
    child.join();
#elif defined(FAIL_TLS_OVERFLOW)
    std::thread child([] {
        extern void tls_overflow_probe();
        tls_overflow_probe();
    });
    child.join();
#endif
    // Reaching here is an unexpected survival, never a passing failure test.
    std::_Exit(90);
}

#ifdef FAIL_TLS_OVERFLOW
extern "C" void *__emutls_get_address(void *);
void tls_overflow_probe() {
    uintptr_t descriptor[] = {UINTPTR_MAX, 8, 0, 0};
    (void)__emutls_get_address(descriptor);
}
#endif
