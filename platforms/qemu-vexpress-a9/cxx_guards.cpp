// SPDX-License-Identifier: MIT
// Generic platform runtime support.
#include <cstdint>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

namespace {
StaticSemaphore_t storage;
SemaphoreHandle_t mutex;
bool scheduled() { return xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED; }
void lock() {
    if (!scheduled()) return;
    taskENTER_CRITICAL();
    if (!mutex) mutex = xSemaphoreCreateRecursiveMutexStatic(&storage);
    taskEXIT_CRITICAL();
    configASSERT(mutex);
    const auto acquired = xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
    configASSERT(acquired == pdTRUE);
    (void)acquired;
}
void unlock() {
    if (scheduled()) {
        const auto released = xSemaphoreGiveRecursive(mutex);
        configASSERT(released == pdTRUE);
        (void)released;
    }
}
}
// ARM EABI guard is one 32-bit word. Bit 0 publishes initialization; bit 8
// detects recursive entry into the same initializer. A recursive mutex permits
// an initializer to initialize other statics, including ones that allocate.
extern "C" int __cxa_guard_acquire(int *guard) {
    lock();
    if (__atomic_load_n(guard, __ATOMIC_ACQUIRE) & 1U) { unlock(); return 0; }
    configASSERT((*guard & 0x100U) == 0U);
    *guard = 0x100U;
    return 1;
}
extern "C" void __cxa_guard_release(int *guard) {
    configASSERT(*guard == 0x100U);
    __atomic_store_n(guard, 1U, __ATOMIC_RELEASE);
    unlock();
}
extern "C" void __cxa_guard_abort(int *guard) {
    configASSERT(*guard == 0x100U);
    __atomic_store_n(guard, 0U, __ATOMIC_RELEASE);
    unlock();
}
