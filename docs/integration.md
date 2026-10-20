# Integrating rtos-stdcpp

## Acquire and select inputs

Clone the private [maintained repository](https://github.com/stevenoonan/rtos-stdcpp)
on branch `rtos-stdcpp` with authorized read access. An integrating application
should record an exact submodule commit, not fetch a branch tip during a build.
For a first checkout `git submodule update --init path/to/rtos-stdcpp` acquires the
application's recorded revision. The library's kernel and selected GCC sources
are tracked files; recursive demo-submodule initialization is unnecessary.

Install the exact ST GNU compiler distribution in the selected
[profile](support-matrix.md). On Ubuntu 24.04, the explicit setup in
[testing](testing.md#host-and-qemu-matrix) can acquire the official ST
bundle. Put that distribution's `bin` directory on PATH for a custom toolchain.
CMake 3.22+, Ninja and Python 3 are required. Ordinary configure/build/test commands
verify local dependencies and use no network. See [licensing](licensing.md) before
changing a dependency or distributing its sources.

The reusable Cortex-M55 profile is `stm32-gcc-14.3.1-cxx17`, hard float and
newlib-nano. The standalone A9 platform uses `qemu-a9-gcc-14.3.1-cxx17`, with its
own multilib/kernel/port. Replace `cxx17` with `cxx20` for C++20; the selected
profile sets the language mode for adapter objects and affected targets.
Do not substitute a nearby GCC version or mix these
profiles. The bundled kernel is selected only by the A9 platform; an application
supplies its own qualified kernel/port/configuration.

## CMake integration

Create the executable and its FreeRTOS provider target before adding the library.
The provider must export the application's `FreeRTOSConfig.h`, kernel headers
and matching port headers. In this example `application_freertos` is an existing
INTERFACE target; the application links its actual kernel objects separately.

```cmake
set(RTOS_STDCPP_PROFILE stm32-gcc-14.3.1-cxx17 CACHE STRING "" FORCE)
set(RTOS_STDCPP_FREERTOS_TARGET application_freertos CACHE STRING "" FORCE)
add_subdirectory(external/rtos-stdcpp)
target_link_libraries(application_freertos INTERFACE rtos-stdcpp::config)
target_link_libraries(firmware PRIVATE rtos-stdcpp::rtos-stdcpp application_freertos)
set_target_properties(firmware PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
```

Use `PUBLIC` instead of `INTERFACE` when the provider itself compiles the kernel.
Apply `rtos-stdcpp::config` to every other affected C/C++ target, including runtime
objects. C++ translation units must agree on the native gthread types and clock
hooks; C/newlib users must agree on the nano `_reent` layout. Rebuild all affected
objects after changing these settings or native types.

| Target or option | Meaning |
|---|---|
| `rtos-stdcpp::rtos-stdcpp` | Object library: three backend units plus selected GCC `mutex.cc` and `condition_variable.cc` |
| `rtos-stdcpp::config` | Object-free ABI/include/compile requirements; no dependency back onto the kernel provider |
| `RTOS_STDCPP_BUILD_LIBRARY` | ON by default; embedded library needs an exact profile and existing provider |
| `RTOS_STDCPP_BUILD_HOST_TESTS` | OFF by default; host tests use `BUILD_LIBRARY=OFF` |
| `RTOS_STDCPP_CXX_STANDARD` | Host language mode, 17 (default) or 20; embedded mode derives from the profile and rejects a conflicting explicit value |
| `RTOS_STDCPP_BUILD_TEST_SUPPORT` | OFF by default; enables optional `rtos-stdcpp::test-support` objects |

Link the primary object target directly into the final executable so all five
providers are delivered exactly once. An interface-only wrapper does not forward
its object files; if wrapping, explicitly forward `$<TARGET_OBJECTS:rtos_stdcpp>`
or retain the direct executable link. Do not compile another copy of these
sources. This is an `add_subdirectory` interface, not an installed `find_package`
package. No global compiler/linker flags are replaced.

The profile propagates CPU/multilib flags, compile-time `--specs=nano.specs`,
GCC hook precedence, `_GLIBCXX_HAS_GTHREADS=1`, forced
`gcc_threading_config.h`, thread-safe statics, disabled exceptions/RTTI and the
selected C++17 or C++20 language mode. The application also selects the matching nano libraries at link
time, its linker script, startup and one kernel/port/heap. It owns newlib locks,
C++ static guards, allocation bounds, syscalls, fatal handling, interrupt/FPU/cache
configuration and any security/memory-map requirements. The
[A9 platform](../platforms/qemu-vexpress-a9/README.md) is an executable test example
of these responsibilities; its policy is not injected into application builds.

## Kernel and runtime contract

Enable mutexes, recursive mutexes, dynamic allocation, event groups, task
notifications and task reentrancy. Important settings are:

| Setting | Required contract |
|---|---|
| `configUSE_NEWLIB_REENTRANT` | 1, with matching nano headers/libraries and working task deletion cleanup |
| `configNUM_THREAD_LOCAL_STORAGE_POINTERS` | At least 1; pointer slot 0 belongs to managed-thread identity |
| `configUSE_TASK_NOTIFICATIONS` | 1; notification index 0 belongs to CV waits |
| `configTASK_NOTIFICATION_ARRAY_ENTRIES` | Add another index if application notifications are needed; explicitly use indexed APIs |
| `TickType_t` | 32 bits; use the selected kernel's setting for 32-bit ticks |
| `configTICK_RATE_HZ` | Positive, with an integral nanosecond tick period (`1,000,000,000 % rate == 0`) |
| `INCLUDE_xTaskGetSchedulerState`, `INCLUDE_xTaskGetCurrentTaskHandle` | 1 |
| `INCLUDE_vTaskDelay`, `INCLUDE_vTaskDelete`, `INCLUDE_vTaskSuspend` | 1 |

The [compile fixture configuration](../examples/compile-check/FreeRTOSConfig.h)
and [A9 configuration](../platforms/qemu-vexpress-a9/FreeRTOSConfig.h) show concrete
settings for their ports; they are not drop-in board configurations. Enable
priority APIs where application/test hooks inspect worker priorities.

Construct and use RTOS-backed C++ objects in task context after scheduler startup.
Pre-scheduler initialization must stay single threaded; a global constructor is
not a place to create workers or block. The clock's pre-start return value does
not make general threading services safe before startup.

Reserve TLS pointer slot 0 and notification index 0. CMSIS thread flags and
stream/message-buffer defaults can also use index 0, so do not mix them with CV
waits on the same task. These RTOS pointers do not implement C++ `thread_local`.
Only the existing single gthread exit key is supported; do not allocate extra
keys or expect native-task exit callbacks. Do not delete managed workers with
`vTaskDelete` from another task: let the callable return. Idle must run to reclaim
self-deleted TCBs/stacks, including after join. No library thread/synchronization
operations or allocation are supported in an ISR. SMP is unqualified.

## Minimal C++17 task example and extensions

Call this function from an already running application task. The joined worker's
write is visible when `join()` returns:

```cpp
#include <thread>

int run_worker_example() {
    int result = 0;
    std::thread worker([&result] { result = 42; });
    worker.join();
    return result;
}
```

For native worker attributes, include
`<rtos_stdcpp/thread_with_attributes.h>` and use the existing `free_rtos_std` API:

```cpp
#include <rtos_stdcpp/thread_with_attributes.h>

void run_custom_worker_example() {
    free_rtos_std::attributes options;
    options.taskName = "worker";
    options.stackWordCount = 1024; // StackType_t words, not bytes; measure for the port.
    options.priority = tskIDLE_PRIORITY + 1;
    auto worker = free_rtos_std::std_thread(options, [] { /* task work */ });
    worker.join();
}
```

Attributes apply to that construction only. The helper serializes construction
by suspending scheduling; callable/argument copy and move constructors used by
it must not block or yield into the RTOS. Default stack size is
`configDEFAULT_STD_THREAD_STACK_SIZE` when defined, otherwise 512 words; default
name is `Task`, priority idle + 1. Choose stacks from measured high-water marks.
The single-property `attr_stack_size`, `attr_priority` and `attr_name` factories
each start from defaults; fill one `attributes` object when setting several.

`<rtos_stdcpp/freertos_time.h>` declares `SetSystemClockTime(time_point)` for
`system_clock`. It changes wall time without changing `steady_clock` deadlines.
The flat header spellings and existing C++ symbols remain; use the
prefixed public includes in new code. Backend/GCC headers are internal interfaces.

For optional shared cases, enable test support before `add_subdirectory`, link
`rtos-stdcpp::test-support` directly and implement
[the test platform hooks](testing.md#embedded-compile-and-integration). Leave it
off for production. Allocation/resource failures terminate under the qualified
no-exceptions policy; explicit nothrow allocation requires the platform's
recoverable provider. See [support limits](support-matrix.md).
