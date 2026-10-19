// SPDX-License-Identifier: MIT
#include "FreeRTOS.h"
#include "task.h"
#include "console.h"
#include <cstdlib>
#include <new>
#include <thread>
namespace {
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
#endif
    // Reaching here is an unexpected survival, never a passing failure test.
    std::_Exit(90);
}
