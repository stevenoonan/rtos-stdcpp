// SPDX-License-Identifier: MIT
// Single-core test platform policy; these providers belong to this image.
#include "FreeRTOS.h"
#include "task.h"
#include "console.h"
#include <cstdlib>
#include <cerrno>
#include <cstddef>
#include <reent.h>
extern "C" {
struct __lock { unsigned unused; };
struct __lock __lock___sinit_recursive_mutex;
struct __lock __lock___sfp_recursive_mutex;
struct __lock __lock___atexit_recursive_mutex;
struct __lock __lock___at_quick_exit_mutex;
struct __lock __lock___malloc_recursive_mutex;
struct __lock __lock___env_recursive_mutex;
struct __lock __lock___tz_mutex;
struct __lock __lock___dd_hash_mutex;
struct __lock __lock___arc4random_mutex;
static struct __lock shared_lock;
void rtos_test_assert_fail(const char *file, unsigned line) { print(file); print(":"); print(line); print("\n"); std::_Exit(86); }
void vApplicationMallocFailedHook() { std::_Exit(87); }
void vApplicationStackOverflowHook(TaskHandle_t, char *) { std::_Exit(88); }
volatile unsigned rtos_test_idle_count;
void vApplicationIdleHook() { ++rtos_test_idle_count; }
void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack, uint32_t *words) {
    static StaticTask_t idle;
    static StackType_t buffer[configMINIMAL_STACK_SIZE];
    *tcb = &idle; *stack = buffer; *words = configMINIMAL_STACK_SIZE;
}
void __malloc_lock(_reent *) { if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) vTaskSuspendAll(); }
void __malloc_unlock(_reent *) { if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) xTaskResumeAll(); }
void __retarget_lock_init(_LOCK_T *lock) { *lock = &shared_lock; }
void __retarget_lock_init_recursive(_LOCK_T *lock) { *lock = &shared_lock; }
void __retarget_lock_close(_LOCK_T) {}
void __retarget_lock_close_recursive(_LOCK_T) {}
void __retarget_lock_acquire(_LOCK_T) { __malloc_lock(nullptr); }
void __retarget_lock_acquire_recursive(_LOCK_T) { __malloc_lock(nullptr); }
void __retarget_lock_release(_LOCK_T) { __malloc_unlock(nullptr); }
void __retarget_lock_release_recursive(_LOCK_T) { __malloc_unlock(nullptr); }
int __retarget_lock_try_acquire(_LOCK_T) { __malloc_lock(nullptr); return 1; }
int __retarget_lock_try_acquire_recursive(_LOCK_T) { __malloc_lock(nullptr); return 1; }
extern char heap_begin[] asm("Image$$HEAP$$ZI$$Base");
extern char heap_end[] asm("Image$$HEAP$$ZI$$Limit");
void *_sbrk(ptrdiff_t amount) {
    static char *position = heap_begin;
    if ((amount >= 0 && size_t(amount) > size_t(heap_end - position)) ||
        (amount < 0 && amount < heap_begin - position)) { errno = ENOMEM; return (void *)-1; }
    char *old = position; position += amount; return old;
}
[[noreturn]] void abort() noexcept { std::_Exit(89); }
}
