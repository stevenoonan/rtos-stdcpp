# Integrating rtos-stdcpp

This guide covers requirements shared by both backends. Follow the matching
backend guide for kernel configuration, startup, native attributes and cleanup:

| Backend | Integration guide | Supported embedded profiles |
| --- | --- | --- |
| FreeRTOS | [FreeRTOS integration](backends/freertos.md) | STM32 Cortex-M55 and QEMU Cortex-A9, C++17/C++20 |
| ThreadX | [ThreadX integration](backends/threadx.md) | STM32 Cortex-M55, C++17/C++20 |

The library defaults to FreeRTOS. Set `RTOS_STDCPP_BACKEND` explicitly in the
consumer so its selection is clear. An integrating firmware project can choose
a different default. The optional [compiler runtime](runtime.md) is configured
separately; its `NONE` choice does not add a third threading backend.

## Acquire and select inputs

Clone the [maintained repository](https://github.com/stevenoonan/rtos-stdcpp)
and record an exact release tag or submodule commit. For a first checkout,
`git submodule update --init path/to/rtos-stdcpp`
acquires the application's recorded revision. Builds do not fetch dependencies.
The bundled FreeRTOS kernel and selected GCC sources are tracked files;
recursive demo-submodule initialization is unnecessary.

Use CMake 3.22+, Ninja and GCC 14.3.1 with the newlib/libstdc++ ABI required by
[the selected profile](support-matrix.md). Tests also require Python 3.
The explicit setup in [testing](testing.md#host-and-qemu-matrix) acquires the ST
compiler bundle. Put its `bin` directory on PATH for a custom toolchain. CMake
checks both C and C++ compiler versions; ordinary builds do not verify compiler
or dependency file hashes. Review [licensing](licensing.md) before changing or
distributing a dependency.

Use `stm32-gcc-14.3.1-cxx17` or `stm32-gcc-14.3.1-cxx20` for Cortex-M55 hard-float
newlib-nano integration. The FreeRTOS-only A9 platform uses
`qemu-a9-gcc-14.3.1-cxx17` or `qemu-a9-gcc-14.3.1-cxx20`, with its own multilib,
kernel and port. The profile determines the embedded language mode. Do not mix
compiler versions, native layouts or profiles in one image.

## CMake targets and configuration

Create the executable and kernel provider target before adding the library.
The provider exports the consumer's kernel configuration, kernel headers and
matching port headers. The backend guides give complete CMake examples using
an INTERFACE provider; link the actual kernel implementation into the image too.

| Target or option | Meaning |
| --- | --- |
| `rtos-stdcpp::rtos-stdcpp` | OBJECT library containing the selected backend, GCC emulated TLS provider and selected GCC `mutex.cc`, `condition_variable.cc` and `future.cc` |
| `rtos-stdcpp::config` | Object-free ABI/include/compile requirements and nano library selection; no dependency back onto the kernel provider |
| `RTOS_STDCPP_BACKEND` | `FreeRTOS` or `ThreadX`; select before `add_subdirectory` |
| `RTOS_STDCPP_FREERTOS_TARGET` | Existing configuration/header target for FreeRTOS |
| `RTOS_STDCPP_THREADX_TARGET` | Existing configuration/header target for ThreadX |
| `RTOS_STDCPP_PROFILE` | Exact embedded compiler/runtime profile |
| `RTOS_STDCPP_BUILD_LIBRARY` | ON by default; requires an embedded profile and selected provider |
| `RTOS_STDCPP_BUILD_HOST_TESTS` | OFF by default; host tests use `BUILD_LIBRARY=OFF` |
| `RTOS_STDCPP_CXX_STANDARD` | Host mode, 17 (default) or 20; embedded mode derives from the profile and rejects a conflicting explicit value |
| `RTOS_STDCPP_BUILD_TEST_SUPPORT` | OFF by default; enables `rtos-stdcpp::test-support` objects |
| `RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR` | OFF by default; ON requires paired application TLS allocation hooks |

Apply `rtos-stdcpp::config` to every affected C/C++ target, including the kernel,
application and compiler runtime. Use INTERFACE linkage for a header-only
provider or PUBLIC linkage when the provider compiles kernel sources. C++ units
must agree on native gthread types and clock hooks; C/newlib users must agree on
the nano `_reent` layout. Rebuild all affected objects after changing settings.

Link the primary OBJECT target directly to the final executable to deliver every
provider exactly once. An INTERFACE-only wrapper does not forward its objects;
if wrapping, explicitly forward `$<TARGET_OBJECTS:rtos_stdcpp>` or retain the
direct link. Do not compile another copy of those sources. This is an
`add_subdirectory` interface, not an installed `find_package` package.

The profile propagates CPU/multilib options, `--specs=nano.specs`, GCC hook
precedence, `_GLIBCXX_HAS_GTHREADS=1`, the forced `gcc_threading_config.h`,
thread-safe statics, disabled exceptions/RTTI and the selected language mode.
It does not replace global compiler/linker flags.

## Application runtime responsibilities

The consumer owns startup, kernel/port/heap selection, linker script, newlib
reentrancy and locks, C++ static guards, allocation bounds, syscalls, fatal policy
and interrupt/FPU/cache/security configuration. It can use the optional
[compiler runtime providers](runtime.md) for some of those obligations; selecting
a threading backend alone does not install them. The
[A9 platform](../platforms/qemu-vexpress-a9/README.md) supplies a standalone
FreeRTOS test implementation of application policy.

Create managed workers and perform blocking synchronization only from a running
thread. Keep pre-scheduler initialization single threaded. Backend-specific
startup allowances, reserved resources and deletion rules are in the two backend
guides. ISR threading/allocation, external deletion of managed workers and SMP
are unsupported. Native ELF `.tdata`/`.tbss` TLS must be rejected by the platform.

## Language thread-local storage

`thread_local` is supported inside managed `std::thread` and `std::jthread`
workers in both language modes. The selected GCC profiles emit emulated TLS;
direct adapter linkage supplies `__emutls_get_address`,
`__emutls_register_common` and `__cxa_thread_atexit`. No additional kernel TLS
slot is needed beyond the backend's managed-thread context.

Each variable is allocated on first access, with its initializer or zero-fill
and requested alignment. A growable per-worker pointer table provides
constant-time lookup after initialization. Workers that never access language
TLS allocate no TLS storage. Allocation and size failures terminate through the
platform fatal policy.

The default allocator follows the selected backend: FreeRTOS uses
`pvPortMalloc`/`vPortFree`; ThreadX uses `std::malloc`/`std::free`. To override TLS
allocation, set `RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR=ON` before adding the library
and supply both hooks declared in `<rtos_stdcpp/tls_allocator.h>`:

```cpp
#include <rtos_stdcpp/tls_allocator.h>
#include <cstdlib>

extern "C" void* rtos_stdcpp_tls_allocate(std::size_t bytes) noexcept {
    return std::malloc(bytes);
}

extern "C" void rtos_stdcpp_tls_deallocate(void* pointer) noexcept {
    std::free(pointer);
}
```

This pair serves TLS values, pointer tables and destructor records. It does not
change thread stacks, completion blocks or other kernel allocations. Hooks must
be thread-safe, must not throw and must not access language TLS, including
indirectly through logging or allocation callbacks. Allocation must return
storage aligned to at least `alignof(std::max_align_t)` or null. Null terminates;
the TLS runtime handles larger requested alignments and returns the original
allocation pointer to the deallocation hook. Both hooks run in the owning worker.

Custom mode requires both hook definitions at final link; there is no runtime
switch or fallback. Rebuild the adapter after changing the option. Builds without
CMake can define `RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR=1` for the TLS provider.

After callable destruction, TLS destructors run in reverse registration order,
including registrations made during teardown. They finish before
`notify_all_at_thread_exit` and join completion. Storage is reclaimed on worker
exit, including detached workers. Access from startup/global constructors,
native kernel workers or ISRs terminates even in Release. Attribute-helper
callable copy/move constructors execute on the creator and inherit its context
restrictions. External deletion bypasses cleanup and remains unsupported.

## Standard APIs and extensions

Call this from an already running application thread. The joined worker's write
is visible after `join()`:

```cpp
#include <thread>

int run_worker_example() {
    int result = 0;
    std::thread worker([&result] {
        result = 42;
    });
    worker.join();
    return result;
}
```

For attributes, include `<rtos_stdcpp/thread_with_attributes.h>` and fill
`rtos_stdcpp::attributes`: `task_name`, `stack_word_count` and `priority`.
Pass it to `rtos_stdcpp::std_thread` or, in C++20, `rtos_stdcpp::std_jthread`.
Native priority conventions, stack-word units, defaults and construction
restrictions are documented separately for
[FreeRTOS](backends/freertos.md#thread-attributes) and
[ThreadX](backends/threadx.md#thread-attributes).
The `attr_stack_size`, `attr_priority` and `attr_name` factories each start from
defaults; fill one object when setting several attributes. Measure stack needs
on the actual port.

`<rtos_stdcpp/clock.h>` declares
`rtos_stdcpp::set_system_clock_time(time_point)`. It changes `system_clock`
without changing `steady_clock` deadlines. Backend/GCC headers are internal;
use prefixed public includes in application code. Project-owned functions and
data members use snake_case. C-linkage hooks use `rtos_stdcpp_` prefixes;
compiler, standard-library and native kernel APIs retain their required names.

## Test support and feature limits

For shared embedded cases, enable `RTOS_STDCPP_BUILD_TEST_SUPPORT` before adding
the library, link `rtos-stdcpp::test-support` directly and implement the
[test platform hooks](testing.md#shared-reports-and-hardware). Leave test support
off for production. Host reference, emulator and physical-board evidence have
different scopes; see [testing](testing.md) and the [support matrix](support-matrix.md).

The embedded target includes the matching, unmodified GCC `future.cc` provider
alongside the mutex/CV providers. Its source URL, checksum and license are in
the [GCC source manifest](../third_party/libstdcxx/stm32-gcc-14.3.1-cxx17/SOURCES.json).
The STM32 consumer exercises promise/future completion, timeouts and concurrent
fulfillment. The shared conformance suite still skips general future/async
qualification; including the provider does not establish arbitrary `std::async`
support. Shared mutexes, extra gthread keys and non-lock-free atomic fallbacks
also remain outside the qualified feature set.
