// SPDX-License-Identifier: MIT
#include "thread_gthread.h"
#include <cassert>
#include <cstring>
#include <type_traits>

using Handle = rtos_stdcpp::gthr_threadx;

namespace rtos_stdcpp {
const attributes* internal::attributes_lock::_attrib = &internal::attributes_lock::_default;
}

namespace {
TX_THREAD bootstrap;
TX_THREAD* current = &bootstrap;
TX_THREAD* reaper;
TX_THREAD* worker;
unsigned deleted{}, events{};
bool fail_create{}, fail_event{}, fail_allocate{}, occupied{};
alignas(Handle::completion) unsigned char storage[sizeof(Handle::completion)];

struct NoPendingWork {};

void reap() {
    current = reaper;
    try {
        reaper->entry(reaper->argument);
    } catch (NoPendingWork&) {
    }
    current = &bootstrap;
}

void finish_worker() {
    current = worker;
    worker->entry(worker->argument);
    worker->tx_thread_state = TX_COMPLETED;
    reap();
}

Handle expected;
void* expected_argument;

void callback(void* context) {
    const auto local = Handle::from_start_argument(context);
    assert(local == expected && Handle::self() == expected);
    assert(local.numeric_id() == expected.numeric_id());
    assert(local.arg() == expected_argument);
    auto finishing = local;
    finishing.notify_joined();
}
} // namespace

extern "C" void* __real_malloc(size_t);
extern "C" void __real_free(void*);

extern "C" void* __wrap_malloc(size_t size) {
    if (fail_allocate) {
        return nullptr;
    }
    if (size == sizeof(storage)) {
        assert(!occupied);
        occupied = true;
        return storage;
    }
    return __real_malloc(size);
}

extern "C" void __wrap_free(void* pointer) {
    if (pointer == storage) {
        occupied = false;
    } else {
        __real_free(pointer);
    }
}

TX_THREAD* tx_thread_identify() {
    return current;
}

UINT tx_interrupt_control(UINT value) {
    return value;
}

UINT tx_thread_preemption_change(TX_THREAD*, UINT, UINT* previous) {
    *previous = 16;
    return 0;
}

UINT tx_mutex_get(TX_MUTEX*, ULONG) {
    return 0;
}

UINT tx_mutex_put(TX_MUTEX*) {
    return 0;
}

UINT tx_mutex_create(TX_MUTEX*, char*, UINT) {
    return 0;
}

UINT tx_semaphore_create(TX_SEMAPHORE*, char*, UINT) {
    return 0;
}

UINT tx_semaphore_get(TX_SEMAPHORE* semaphore, ULONG) {
    if (!semaphore->count) {
        throw NoPendingWork{};
    }
    --semaphore->count;
    return 0;
}

UINT tx_semaphore_ceiling_put(TX_SEMAPHORE* semaphore, ULONG) {
    semaphore->count = 1;
    return 0;
}

UINT tx_thread_sleep(ULONG) {
    return 0;
}

UINT tx_thread_delete(TX_THREAD*) {
    ++deleted;
    return 0;
}

UINT tx_thread_resume(TX_THREAD*) {
    return 0;
}

ULONG tx_time_get() {
    return 1234;
}

UINT tx_thread_create(TX_THREAD* task, char*, void (*entry)(ULONG), ULONG argument, void*, ULONG,
                      UINT, UINT, ULONG, UINT automatic) {
    if (fail_create) {
        return 3;
    }
    task->entry = entry;
    task->argument = argument;
    if (automatic == TX_AUTO_START) {
        reaper = task;
    } else {
        worker = task;
    }
    return 0;
}

UINT tx_event_flags_create(TX_EVENT_FLAGS_GROUP*, char*) {
    if (fail_event) {
        return 3;
    }
    ++events;
    return 0;
}

UINT tx_event_flags_delete(TX_EVENT_FLAGS_GROUP*) {
    --events;
    return 0;
}

UINT tx_event_flags_set(TX_EVENT_FLAGS_GROUP* event, ULONG flags, UINT) {
    event->flags |= flags;
    return 0;
}

UINT tx_event_flags_get(TX_EVENT_FLAGS_GROUP* event, ULONG, UINT, ULONG* result, ULONG) {
    if (!event->flags) {
        finish_worker();
    }
    assert(event->flags);
    *result = event->flags;
    return 0;
}

int main() {
    static_assert(sizeof(Handle) == 2 * sizeof(uintptr_t));
    static_assert(std::is_trivially_copyable_v<Handle>);
    static_assert(std::has_unique_object_representations_v<Handle>);
    rtos_stdcpp_threadx_initialize();
    assert(Handle::self() != Handle{}); // Native bootstrap remains identifiable.
    assert(Handle::self().numeric_id() == 0);
    auto native = Handle::self();
    assert(native.join() == EINVAL && native.detach() == EINVAL);

    Handle handle;
    assert(handle.join() == EINVAL && handle.detach() == EINVAL);
    expected_argument = &bootstrap;
    uint32_t previous = 0;
    Handle saved;
    for (unsigned i = 0; i < 128; ++i) {
        assert(handle.create_thread(callback, expected_argument));
        expected = handle;
        const auto number = handle.numeric_id();
        assert(number);
        if (!i) {
            assert(number > 0x11111111U);
        } else {
            assert(number == (previous == UINT32_MAX ? 1U : previous + 1));
        }
        previous = number;
        assert(handle != saved); // Allocation address is deliberately identical.
        saved = handle;
        current = worker;
        assert(handle.join() == EDEADLK);
        current = &bootstrap;
        if (i % 3 == 0) {
            assert(handle.join() == 0);
        } else if (i % 3 == 1) {
            assert(handle.detach() == 0);
            finish_worker();
        } else {
            finish_worker();
            assert(handle.detach() == 0);
        }
        assert(handle == Handle{} && handle.numeric_id() == 0);
        assert(!occupied && events == 0);
        assert(saved == expected); // Comparing copies never touches freed memory.
        assert(std::memcmp(&saved, &expected, sizeof(saved)) == 0);
    }
    assert(deleted == 128);
    for (unsigned failure = 0; failure < 3; ++failure) {
        fail_allocate = failure == 0;
        fail_event = failure == 1;
        fail_create = failure == 2;
        assert(!handle.create_thread(callback, expected_argument));
        assert(handle == Handle{} && !occupied && events == 0);
    }
    fail_allocate = fail_event = fail_create = false;
    assert(handle.create_thread(callback, expected_argument));
    expected = handle;
    assert(handle.join() == 0);
    assert(!occupied && events == 0);
}
