// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
using UINT = unsigned;
using ULONG = uintptr_t;

struct TX_THREAD {
    void* rtos_stdcpp_context{};
    UINT tx_thread_state{};
    void (*entry)(ULONG){};
    ULONG argument{};
};

struct TX_EVENT_FLAGS_GROUP {
    ULONG flags{};
};

struct TX_MUTEX {};

struct TX_SEMAPHORE {
    unsigned count{};
};

constexpr UINT TX_SUCCESS = 0, TX_CEILING_EXCEEDED = 1, TX_COMPLETED = 2;
constexpr UINT TX_INT_DISABLE = 1, TX_INHERIT = 1, TX_OR = 1;
constexpr UINT TX_NO_TIME_SLICE = 0, TX_DONT_START = 0, TX_AUTO_START = 1;
constexpr ULONG TX_WAIT_FOREVER = ~ULONG{0}, TX_MINIMUM_STACK = 128;
constexpr UINT TX_MAX_PRIORITIES = 32, TX_TIMER_TICKS_PER_SECOND = 1000;
#define TX_THREAD_GET_SYSTEM_STATE() 0
TX_THREAD* tx_thread_identify();
UINT tx_interrupt_control(UINT);
UINT tx_thread_preemption_change(TX_THREAD*, UINT, UINT*);
UINT tx_mutex_get(TX_MUTEX*, ULONG);
UINT tx_mutex_put(TX_MUTEX*);
UINT tx_mutex_create(TX_MUTEX*, char*, UINT);
UINT tx_semaphore_get(TX_SEMAPHORE*, ULONG);
UINT tx_semaphore_create(TX_SEMAPHORE*, char*, UINT);
UINT tx_semaphore_ceiling_put(TX_SEMAPHORE*, ULONG);
UINT tx_thread_sleep(ULONG);
UINT tx_thread_delete(TX_THREAD*);
UINT tx_thread_resume(TX_THREAD*);
UINT tx_thread_create(TX_THREAD*, char*, void (*)(ULONG), ULONG, void*, ULONG, UINT, UINT, ULONG,
                      UINT);
UINT tx_event_flags_create(TX_EVENT_FLAGS_GROUP*, char*);
UINT tx_event_flags_delete(TX_EVENT_FLAGS_GROUP*);
UINT tx_event_flags_set(TX_EVENT_FLAGS_GROUP*, ULONG, UINT);
UINT tx_event_flags_get(TX_EVENT_FLAGS_GROUP*, ULONG, UINT, ULONG*, ULONG);
ULONG tx_time_get();
