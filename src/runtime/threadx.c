#include "rtos_stdcpp/runtime.h"
#include "tx_api.h"
#include "tx_thread.h"
#include <reent.h>

static TX_MUTEX runtime_mutex;
static TX_MUTEX guard_mutex;
static int initialized;

void rtos_stdcpp_runtime_initialize(void) {
    RTOS_STDCPP_RUNTIME_REQUIRE(tx_mutex_create(&runtime_mutex, "C runtime", TX_INHERIT) ==
                                TX_SUCCESS);
    RTOS_STDCPP_RUNTIME_REQUIRE(tx_mutex_create(&guard_mutex, "C++ guards", TX_INHERIT) ==
                                TX_SUCCESS);
    initialized = 1;
}

void rtos_stdcpp_runtime_lock(void) {
    RTOS_STDCPP_RUNTIME_REQUIRE(!rtos_stdcpp_runtime_is_interrupt());
    if (!tx_thread_identify()) {
        return;
    }
    RTOS_STDCPP_RUNTIME_REQUIRE(initialized);
    RTOS_STDCPP_RUNTIME_REQUIRE(tx_mutex_get(&runtime_mutex, TX_WAIT_FOREVER) == TX_SUCCESS);
}

void rtos_stdcpp_runtime_unlock(void) {
    if (tx_thread_identify()) {
        RTOS_STDCPP_RUNTIME_REQUIRE(tx_mutex_put(&runtime_mutex) == TX_SUCCESS);
    }
}

void _tx_execution_thread_enter(void) {
    TX_THREAD* thread = _tx_thread_current_ptr;
    if (!thread->rtos_stdcpp_reent_initialized) {
        _REENT_INIT_PTR(&thread->rtos_stdcpp_reent);
        thread->rtos_stdcpp_reent_initialized = 1;
    }
    _impure_ptr = &thread->rtos_stdcpp_reent;
}

void _tx_execution_thread_exit(void) {}

void _tx_execution_isr_enter(void) {}

void _tx_execution_isr_exit(void) {}

void rtos_stdcpp_threadx_thread_cleanup(TX_THREAD* thread) {
    if (thread->rtos_stdcpp_reent_initialized) {
        _reclaim_reent(&thread->rtos_stdcpp_reent);
    }
}

void _tx_execution_initialize(void) {}

void rtos_stdcpp_guard_lock(void) {
    RTOS_STDCPP_RUNTIME_REQUIRE(!rtos_stdcpp_runtime_is_interrupt());
    if (!tx_thread_identify()) {
        return;
    }
    RTOS_STDCPP_RUNTIME_REQUIRE(initialized);
    RTOS_STDCPP_RUNTIME_REQUIRE(tx_mutex_get(&guard_mutex, TX_WAIT_FOREVER) == TX_SUCCESS);
}

void rtos_stdcpp_guard_unlock(void) {
    if (tx_thread_identify()) {
        RTOS_STDCPP_RUNTIME_REQUIRE(tx_mutex_put(&guard_mutex) == TX_SUCCESS);
    }
}
