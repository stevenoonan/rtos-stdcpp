# ThreadX integration

Start with the [shared integration guide](../integration.md) for profiles, ABI
requirements, standard APIs and language TLS. This page owns ThreadX-specific
configuration and lifecycle requirements. See also the
[compiler runtime](../runtime.md), [testing](../testing.md) and
[support matrix](../support-matrix.md).

## Provider and CMake

Create an existing target exporting `tx_api.h`, `tx_user.h` and the matching
port headers. This example uses an INTERFACE provider and the Cortex-M55 C++20
profile; select `stm32-gcc-14.3.1-cxx17` for C++17. Link the actual kernel
implementation separately if the provider only exports headers.

```cmake
set(RTOS_STDCPP_PROFILE stm32-gcc-14.3.1-cxx20)
set(RTOS_STDCPP_BACKEND ThreadX)
set(RTOS_STDCPP_THREADX_TARGET application_threadx)
add_subdirectory(external/rtos-stdcpp)
target_link_libraries(application_threadx INTERFACE rtos-stdcpp::config)
target_link_libraries(firmware PRIVATE rtos-stdcpp::rtos-stdcpp application_threadx)
```

Use PUBLIC instead of INTERFACE when the provider compiles kernel sources.
Apply `rtos-stdcpp::config` to every affected target. ThreadX currently supports
only the STM32 Cortex-M55 profiles; the standalone QEMU A9 platform is FreeRTOS.

## Kernel configuration

Define `TX_INCLUDE_USER_DEFINE_FILE` consistently. Include
`rtos_stdcpp/threadx_config.h` from `tx_user.h` to reserve `rtos_stdcpp_context`
in every TCB. To append consumer fields, define `TX_THREAD_USER_EXTENSION` using
`RTOS_STDCPP_THREADX_THREAD_EXTENSION` plus those fields. All kernel/application
objects must agree on that layout.

When using the optional compiler runtime, include
`rtos_stdcpp/threadx_runtime_config.h` instead; it reserves both the managed
context and per-thread newlib state. Its additional requirements are described
under [allocation and compiler runtime](#allocation-and-compiler-runtime).

Keep preemption thresholds enabled: do not define `TX_DISABLE_PREEMPTION_THRESHOLD`.
Broadcasts use a short threshold scope to prevent notified waiters from rejoining
an in-progress notification batch. Keep native API error checking enabled.
Use a positive `TX_TIMER_TICKS_PER_SECOND` that divides 1,000,000,000 evenly;
clock resolution is the configured kernel tick.

## Startup and interrupts

Call `rtos_stdcpp_threadx_initialize()` from `tx_application_define()` before
creating managed workers. Call `rtos_stdcpp_threadx_tick()` exactly once per kernel
timer interrupt, before `_tx_timer_interrupt()`. Declarations are in
`<rtos_stdcpp/threadx.h>`. The hook maintains the extended monotonic count across
wrap without depending on frequent thread execution. Do not change `tx_time_set()`
after startup. Set wall time through the
[common clock extension](../integration.md#standard-apis-and-extensions).

Mutex/once use during single-threaded startup defers native mutex creation until
thread context. Creating workers or waiting on condition variables requires a
running thread. ISR threading operations are unsupported; timer callbacks must
not invoke blocking C++ threading operations. SMP is unqualified.

## Thread attributes

```cpp
#include <rtos_stdcpp/thread_with_attributes.h>

void run_custom_worker_example() {
    rtos_stdcpp::attributes options;
    options.task_name = "worker";
    options.stack_word_count = 1024;
    options.priority = 16;
    auto worker = rtos_stdcpp::std_thread(options, [] {
        // Thread work.
    });
    worker.join();
}
```

Call this from a running thread. Stack sizes are 32-bit words. Native priority
zero is highest; smaller numbers have higher priority. Defaults are 2048 words
at priority 16 with the name `C++ thread`. Names are copied and truncated to 31
characters. Set a valid priority for the consumer's `TX_MAX_PRIORITIES`.

The helper serializes construction with a recursive ThreadX mutex. Callable and
argument construction runs on the creator and must follow its context and
lifetime rules. Attributes apply only to that construction. Common factories
and C++20 helpers are covered in
[shared integration](../integration.md#standard-apis-and-extensions).

## Thread lifetime and cleanup

Managed IDs name a stable completion block. Join waits for language TLS and exit
callbacks, native thread completion, TCB deletion and stack reclamation. Detach
releases the owner's reference; a reaper deletes completed workers and releases
their final reference. Allow the reaper at priority `TX_MAX_PRIORITIES - 1` to
run. It has a permanent, statically allocated 2 KiB stack.

Do not terminate/delete managed workers externally. Native IDs use tagged TCB
addresses; language TLS is supported only on managed workers. The one existing
gthread key is reserved for libstdc++ exit work. The optional
`rtos_stdcpp_threadx_thread_cleanup(TX_THREAD*)` hook runs on the reaper before
TCB deletion and can reclaim consumer per-thread state.

## Allocation and compiler runtime

Managed completion blocks, stacks and default language-TLS allocations use
`std::malloc`/`std::free`. The common
[TLS allocator option](../integration.md#language-thread-local-storage) changes
only TLS-owned allocations. The consumer must provide thread-safe newlib locks
and per-thread newlib reentrancy.

The [optional runtime module](../runtime.md) supplies those requirements with
`rtos_stdcpp_add_runtime(... BACKEND ThreadX ...)`. Include
`rtos_stdcpp/threadx_runtime_config.h` from `tx_user.h` and enable
`TX_ENABLE_EXECUTION_CHANGE_NOTIFY` throughout the kernel and application. Call
`rtos_stdcpp_runtime_initialize()` from `tx_application_define()`, after the
kernel's object lists are ready and before creating managed workers.

That provider uses separate recursive priority-inheriting mutexes for allocation
and static guards. It owns the `_tx_execution_*` hooks, switches `_impure_ptr`
on thread entry and reclaims managed workers' newlib state through the reaper's
cleanup hook. Do not install competing execution/cleanup hooks. Native ThreadX
workers require consumer cleanup before deletion. Heap bounds, syscalls, `abort`
and fault capture remain consumer policy.

## Validation and limits

The [standalone compile fixture](../../examples/compile-check/README.md) and
[release checker](../testing.md#release-checks) build ThreadX public API, bootstrap
and compiler runtime objects in C++17/C++20. CI checks Debug/Release and both TLS
allocator policies with pinned public kernel headers. These compile checks do
not execute a kernel or produce a board image. The QEMU runner remains FreeRTOS.
ThreadX execution and final-image checks belong to the consumer's board
integration; use the shared
[test support](../testing.md#shared-reports-and-hardware) in its runner. The [support matrix](../support-matrix.md) distinguishes available
backend behavior, recorded board checks and remaining qualification work.
