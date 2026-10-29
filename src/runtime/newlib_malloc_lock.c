#include <stdint.h>
#include <reent.h>
#include <sys/lock.h>
#include "rtos_stdcpp/runtime.h"

/* The selected runtime provider serializes every newlib lock in one domain.
 * Startup is single threaded. ISRs may not use newlib. The shared recursive
 * domain avoids lock-order inversions during per-thread reent cleanup.
 */
struct __lock {
    unsigned unused;
};

#define LOCK(name) struct __lock __lock_##name
LOCK(__sinit_recursive_mutex);
LOCK(__sfp_recursive_mutex);
LOCK(__atexit_recursive_mutex);
LOCK(__at_quick_exit_mutex);
LOCK(__malloc_recursive_mutex);
LOCK(__env_recursive_mutex);
LOCK(__tz_mutex);
LOCK(__dd_hash_mutex);
LOCK(__arc4random_mutex);
static struct __lock dynamic_lock;
static unsigned depth;
static uint32_t start_cycles;
volatile uint32_t rtos_stdcpp_newlib_max_lock_cycles;
volatile uint32_t rtos_stdcpp_newlib_lock_calls;

static void acquire(void) {
    RTOS_STDCPP_RUNTIME_REQUIRE(!rtos_stdcpp_runtime_is_interrupt());
    rtos_stdcpp_runtime_lock();
    if (depth++ == 0U) {
        start_cycles = rtos_stdcpp_runtime_cycles();
    }
    ++rtos_stdcpp_newlib_lock_calls;
}

static void release(void) {
    RTOS_STDCPP_RUNTIME_REQUIRE(depth != 0U);
    if (--depth == 0U) {
        uint32_t cycles = rtos_stdcpp_runtime_cycles() - start_cycles;
        if (cycles > rtos_stdcpp_newlib_max_lock_cycles) {
            rtos_stdcpp_newlib_max_lock_cycles = cycles;
        }
    }
    rtos_stdcpp_runtime_unlock();
}

void __malloc_lock(struct _reent* r) {
    (void)r;
    acquire();
}

void __malloc_unlock(struct _reent* r) {
    (void)r;
    release();
}

void __retarget_lock_init(_LOCK_T* lock) {
    *lock = &dynamic_lock;
}

void __retarget_lock_init_recursive(_LOCK_T* lock) {
    *lock = &dynamic_lock;
}

void __retarget_lock_close(_LOCK_T lock) {
    (void)lock;
}

void __retarget_lock_close_recursive(_LOCK_T lock) {
    (void)lock;
}

void __retarget_lock_acquire(_LOCK_T lock) {
    (void)lock;
    acquire();
}

void __retarget_lock_acquire_recursive(_LOCK_T lock) {
    (void)lock;
    acquire();
}

int __retarget_lock_try_acquire(_LOCK_T lock) {
    (void)lock;
    acquire();
    return 1;
}

int __retarget_lock_try_acquire_recursive(_LOCK_T lock) {
    (void)lock;
    acquire();
    return 1;
}

void __retarget_lock_release(_LOCK_T lock) {
    (void)lock;
    release();
}

void __retarget_lock_release_recursive(_LOCK_T lock) {
    (void)lock;
    release();
}
