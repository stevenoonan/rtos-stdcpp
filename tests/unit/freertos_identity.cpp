// SPDX-License-Identifier: MIT
#include "thread_gthread.h"
#include <cassert>
#include <cerrno>
#include <cstring>
#include <type_traits>

using Handle = rtos_stdcpp::gthr_freertos;

namespace rtos_stdcpp {
const attributes* internal::attributes_lock::_attrib = &internal::attributes_lock::_default;
}

namespace {
FakeTask bootstrap, worker;
FakeTask* current = &bootstrap;
FakeEvent event;
unsigned critical{}, suspended{}, events{}, deleted{};
bool occupied{}, fail_allocate{}, fail_event{}, fail_create{};
alignas(std::max_align_t) unsigned char storage[512];
Handle expected;
void* expected_argument;

struct WorkerDeleted {};

void callback(void* argument) {
    const auto local = Handle::from_start_argument(argument);
    assert(local == expected && Handle::self() == expected);
    assert(local.numeric_id() == expected.numeric_id());
    assert(local.arg() == expected_argument);
    auto finishing = local;
    finishing.notify_joined();
}

void finish_worker() {
    current = &worker;
    try {
        worker.entry(worker.argument);
        assert(false);
    } catch (WorkerDeleted&) {
    }
    current = &bootstrap;
}
} // namespace

void enter_critical() {
    ++critical;
}

void exit_critical() {
    assert(critical);
    --critical;
}

BaseType_t xTaskGetSchedulerState() {
    return 1;
}

void vTaskSuspendAll() {
    ++suspended;
}

BaseType_t xTaskResumeAll() {
    assert(suspended);
    --suspended;
    return 1;
}

void* pvPortMalloc(size_t size) {
    assert(!critical && size <= sizeof(storage));
    if (fail_allocate) {
        return nullptr;
    }
    assert(!occupied);
    occupied = true;
    std::memset(storage, 0xa5, sizeof(storage));
    return storage;
}

void vPortFree(void* pointer) {
    assert(!critical && pointer == storage && occupied);
    occupied = false;
    std::memset(storage, 0xdd, sizeof(storage));
}

EventGroupHandle_t xEventGroupCreate() {
    assert(!critical);
    if (fail_event) {
        return nullptr;
    }
    ++events;
    event.flags = 0;
    return &event;
}

void vEventGroupDelete(EventGroupHandle_t pointer) {
    assert(!critical && pointer == &event && events == 1);
    --events;
}

BaseType_t xTaskCreate(void (*entry)(void*), const char*, configSTACK_DEPTH_TYPE, void* argument,
                       UBaseType_t, TaskHandle_t* task) {
    assert(suspended && !critical);
    if (fail_create) {
        return 0;
    }
    worker = {nullptr, entry, argument};
    *task = &worker;
    return pdPASS;
}

TaskHandle_t xTaskGetCurrentTaskHandle() {
    return current;
}

void* pvTaskGetThreadLocalStoragePointer(TaskHandle_t task, BaseType_t) {
    return (task ? task : current)->context;
}

void vTaskSetThreadLocalStoragePointer(TaskHandle_t task, BaseType_t, void* context) {
    (task ? task : current)->context = context;
}

void vTaskDelete(TaskHandle_t) {
    ++deleted;
    throw WorkerDeleted{};
}

TickType_t xTaskGetTickCount() {
    return 1234;
}

EventBits_t xEventGroupSetBits(EventGroupHandle_t pointer, EventBits_t bits) {
    assert(pointer == &event);
    return pointer->flags |= bits;
}

EventBits_t xEventGroupWaitBits(EventGroupHandle_t pointer, EventBits_t, BaseType_t, BaseType_t,
                                TickType_t) {
    assert(!critical && !suspended);
    if (!pointer->flags) {
        finish_worker();
    }
    return pointer->flags;
}

int main() {
    static_assert(sizeof(Handle) == 2 * sizeof(uintptr_t));
    static_assert(std::is_trivially_copyable_v<Handle>);
    static_assert(std::has_unique_object_representations_v<Handle>);
    auto native = Handle::self();
    assert(native != Handle{} && !native.numeric_id());
    assert(native.join() == EINVAL && native.detach() == EINVAL);
    Handle handle, saved;
    assert(handle.join() == EINVAL && handle.detach() == EINVAL);
    expected_argument = &bootstrap;
    uint32_t previous = 0;
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
        assert(handle != saved); // Completion storage is reused on every iteration.
        saved = handle;
        current = &worker;
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
        assert(handle == Handle{} && !handle.numeric_id());
        assert(!occupied && !events && !critical && !suspended);
        assert(saved == expected);
        assert(std::memcmp(&saved, &expected, sizeof(saved)) == 0);
    }
    assert(deleted == 128);
    for (unsigned failure = 0; failure < 3; ++failure) {
        fail_allocate = failure == 0;
        fail_event = failure == 1;
        fail_create = failure == 2;
        assert(!handle.create_thread(callback, expected_argument));
        assert(handle == Handle{} && !occupied && !events && !critical && !suspended);
    }
    fail_allocate = fail_event = fail_create = false;
    assert(handle.create_thread(callback, expected_argument));
    expected = handle;
    assert(handle.join() == 0);
    assert(!occupied && !events);
}
