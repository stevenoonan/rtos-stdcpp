# Changelog

## 1.1.0 - Unreleased

- Prevent immediate managed thread ID reuse after memory recycling in both backends.
- Increase thread IDs to eight bytes on 32-bit targets, requiring all C++ consumers and runtime objects to be rebuilt.
- Move FreeRTOS lifecycle implementation into `lifecycle.cpp`.
- Add the backend `numeric_id()` accessor.
- Expand host and embedded thread identity and lifecycle tests.
- Qualify FreeRTOS and ThreadX equally on QEMU Cortex-M4, M7 and M55.
- Add native ASan and leak checks alongside UBSan.
- Share emulator qualification, failure tests, runtime audits and CI setup.
- Reduce CI artifact uploads and update pinned actions to Node.js 24.

## 1.0.0 - 2026-10-29

### Features

- FreeRTOS and ThreadX backends for GCC/libstdc++ standard C++ threading APIs,
  selected through `RTOS_STDCPP_BACKEND`.
- C++17 threads, mutexes, condition variables, once initialization, thread-exit
  notification, monotonic/wall clocks and sleep operations.
- C++20 `jthread`, stop tokens/callbacks, stop-aware waits, atomic wait/notify,
  semaphores, latch and barrier.
- Managed language `thread_local` storage with initialization, alignment,
  destructors and optional consumer allocation hooks.
- Native thread attributes and factories through the backend-neutral
  `rtos_stdcpp` namespace, plus `rtos_stdcpp::set_system_clock_time()`.
- Managed thread completion and resource cleanup, including a ThreadX reaper.
- Optional Cortex-M compiler runtime providers for newlib locks, C++ static
  guards, nothrow allocation and ThreadX per-thread newlib state. Custom runtime
  providers use the helper module's explicit `NONE` selection.
- Explicit CMake OBJECT/configuration targets, ST GCC 14.3.1 C++17/C++20
  profiles, and selected matching GCC mutex, condition-variable and future
  runtime providers.
- Isolated delivery checks, plus standalone
  FreeRTOS/ThreadX compile fixtures and CI coverage for both language modes.
