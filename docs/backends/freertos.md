# FreeRTOS integration

Start with the [shared integration guide](../integration.md) for profiles, ABI
requirements, standard APIs and language TLS. This page owns FreeRTOS-specific
configuration and lifecycle requirements. See also the
[compiler runtime](../runtime.md), [testing](../testing.md) and
[support matrix](../support-matrix.md).

## Provider and CMake

Create an existing target exporting `FreeRTOSConfig.h`, kernel headers and the
matching port headers. This example uses an INTERFACE provider and the Cortex-M55
C++20 profile; select `stm32-gcc-14.3.1-cxx17` for C++17. Link the actual kernel
implementation separately if the provider only exports headers.

```cmake
set(RTOS_STDCPP_PROFILE stm32-gcc-14.3.1-cxx20)
set(RTOS_STDCPP_BACKEND FreeRTOS)
set(RTOS_STDCPP_FREERTOS_TARGET application_freertos)
add_subdirectory(external/rtos-stdcpp)
target_link_libraries(application_freertos INTERFACE rtos-stdcpp::config)
target_link_libraries(firmware PRIVATE rtos-stdcpp::rtos-stdcpp application_freertos)
```

Use PUBLIC instead of INTERFACE when the provider compiles kernel sources.
Apply `rtos-stdcpp::config` to every affected target. FreeRTOS also supports the
qualified QEMU A9 profiles; the standalone
[A9 platform](../../platforms/qemu-vexpress-a9/README.md) supplies its kernel/port.

## Kernel configuration

Enable mutexes, recursive mutexes, dynamic allocation, event groups, task
notifications and task reentrancy.

| Setting | Required contract |
| --- | --- |
| `configUSE_NEWLIB_REENTRANT` | 1, with nano headers/libraries and working task-deletion cleanup |
| `configNUM_THREAD_LOCAL_STORAGE_POINTERS` | At least 1; slot 0 belongs to managed-thread identity and language TLS context |
| `configUSE_TASK_NOTIFICATIONS` | 1; notification index 0 belongs to CV waits |
| `configTASK_NOTIFICATION_ARRAY_ENTRIES` | Add another index for application notifications and use indexed APIs |
| `TickType_t` | 32 bits; select the matching kernel setting |
| `configTICK_RATE_HZ` | Positive, with an integral nanosecond tick period (`1,000,000,000 % rate == 0`) |
| `INCLUDE_xTaskGetSchedulerState`, `INCLUDE_xTaskGetCurrentTaskHandle` | 1 |
| `INCLUDE_vTaskDelay`, `INCLUDE_vTaskDelete`, `INCLUDE_vTaskSuspend` | 1 |

The [compile fixture configuration](../../examples/compile-check/freertos/FreeRTOSConfig.h)
and [A9 configuration](../../platforms/qemu-vexpress-a9/FreeRTOSConfig.h) show
concrete settings, not drop-in board configurations. Enable priority APIs where
application/test hooks inspect worker priorities.

Do not reuse TLS pointer slot 0 or notification index 0 on managed workers.
CMSIS thread flags and stream/message-buffer defaults can also use notification
index 0 and conflict with CV waits on the same task.

## Startup and interrupts

The consumer owns scheduler entry and the native port's timer/interrupt setup.
There is no additional adapter initialization or tick hook. The clock extends
the kernel's atomically sampled 32-bit tick/overflow pair; do not reset the
kernel tick count after startup.

Construct and use RTOS-backed C++ objects in running task context. The STM32 M55
consumer additionally qualifies ordinary mutex construction and uncontended
lock/try-lock/unlock/destruction in single-threaded startup, followed by
contention after startup. That allowance depends on startup-safe heap locks and
C++ guards; it does not permit blocking, worker creation, language TLS or
arbitrary synchronization before the scheduler runs. Allocation and threading
operations in ISRs are unsupported. SMP is unqualified.

## Thread attributes

```cpp
#include <rtos_stdcpp/thread_with_attributes.h>

void run_custom_worker_example() {
    rtos_stdcpp::attributes options;
    options.task_name = "worker";
    options.stack_word_count = 1024;
    options.priority = tskIDLE_PRIORITY + 1;
    auto worker = rtos_stdcpp::std_thread(options, [] {
        // Task work.
    });
    worker.join();
}
```

Call this from a running task. Stack sizes are `StackType_t` words, not bytes.
Higher native priority numbers have higher priority. Defaults are `Task`, idle
priority + 1 and `configDEFAULT_STD_THREAD_STACK_SIZE` words when defined,
otherwise 512 words. Name handling follows the native kernel's name limit.

The helper suspends scheduling around construction. Callable/argument copy and
move constructors used by it must not block or yield into the RTOS. Attributes
apply only to that construction. Common factories and C++20 thread helpers are
covered in [shared integration](../integration.md#standard-apis-and-extensions).

## Thread lifetime and cleanup

Managed IDs name a stable completion block. The worker destroys its callable,
drains language TLS and exit callbacks, reclaims TLS storage, publishes completion
and self-deletes. Join waits for completion and releases the owner's reference;
detach releases that reference without waiting. The last reference frees the
completion/event resources. The kernel idle task later reclaims the self-deleted
TCB and stack, so allow idle to run even after join returns.

Do not delete managed workers externally with `vTaskDelete`; let the callable
return. Native tasks do not have the managed language-TLS or exit-callback
contract. The one existing gthread key is reserved for libstdc++ exit work.

## Allocation and compiler runtime

Completion blocks, kernel objects and default language-TLS allocations use the
FreeRTOS heap. C++/newlib allocations can use a separate arena. The common
[TLS allocator option](../integration.md#language-thread-local-storage) changes
only TLS-owned allocations.

The consumer supplies newlib locks and C++ static guards, or selects
`rtos_stdcpp_add_runtime(... BACKEND FreeRTOS ...)` from the
[optional runtime module](../runtime.md). That provider suspends scheduling for
allocation locks and uses a separate recursive static mutex for guards, allowing
an initializer to block. It additionally requires static allocation and recursive
mutex support. FreeRTOS owns per-task `_reent` switching and deletion cleanup.
Heap bounds, syscalls, `abort` and fault capture remain consumer policy.

## Validation and limits

The library's standalone compile fixture and QEMU runner exercise FreeRTOS.
Follow [testing](../testing.md) for commands and distinguish compile-only,
emulator and physical-board evidence. The [support matrix](../support-matrix.md)
records qualified facilities and exclusions. Physical kernel/port qualification
belongs to the integrating application.
