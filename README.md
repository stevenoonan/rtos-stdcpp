# rtos-stdcpp

rtos-stdcpp connects standard C++17/C++20 threading APIs to FreeRTOS or ThreadX
through GCC/libstdc++.

Applications use `<thread>`, `<mutex>`, `<condition_variable>` and `<chrono>`.
C++20 adds `jthread`, stop tokens, stop-aware waits, atomic wait/notify,
semaphores, latch and barrier. Extensions configure native thread attributes and
wall-clock time. Embedded profiles use newlib-nano with exceptions and RTTI
disabled. Embedded builds are tested with ST GCC 14.3.1. See the
[support matrix](docs/support-matrix.md) for tested configurations.

## Build and test

```sh
git clone https://github.com/stevenoonan/rtos-stdcpp.git
cd rtos-stdcpp
cmake --preset host-debug -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13
cmake --build --preset host-debug
ctest --preset host-debug --no-tests=error
```

Install CMake 3.22+, Ninja, Python 3 and GCC 13 first. These are host reference
and arithmetic tests. [Testing](docs/testing.md) describes Clang, embedded
compile checks and the standalone FreeRTOS QEMU A9 suite.

For firmware, start with [shared integration](docs/integration.md), then follow
[FreeRTOS integration](docs/backends/freertos.md) or
[ThreadX integration](docs/backends/threadx.md). Both use the same public C++ API,
profile selection and CMake targets. Supply the selected kernel's configuration
target, apply `rtos-stdcpp::config` to every affected target and link
`rtos-stdcpp::rtos-stdcpp` directly into the executable. Optional newlib/compiler
runtime providers have their own [runtime guide](docs/runtime.md).

## Support and limits

The [support matrix](docs/support-matrix.md) separates host checks, FreeRTOS
QEMU/board results and ThreadX board results.

Language `thread_local` is supported in managed workers, including constructors,
alignment and destructors. The backend guides specify native context storage,
allocation defaults and cleanup. `RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR=ON` selects
[application TLS allocation hooks](docs/integration.md#language-thread-local-storage).
Startup, native-worker and ISR language-TLS access terminate; native ELF TLS is
unsupported. Extra gthread keys, ISR threading, external deletion of managed
workers, general future/async, shared mutexes and non-lock-free
atomic fallbacks are outside the supported feature set.

## Documentation

- [Integration](docs/integration.md): shared CMake/ABI requirements, APIs and TLS.
- [FreeRTOS](docs/backends/freertos.md) and [ThreadX](docs/backends/threadx.md): matching backend setup and lifecycle guides.
- [Compiler runtime](docs/runtime.md): optional newlib/guard helpers and custom providers.
- [Architecture](docs/architecture.md): shared design and backend differences.
- [Support matrix](docs/support-matrix.md): profiles, evidence and feature limits.
- [Changelog](CHANGELOG.md): release history and public API migration notes.
- [Testing](docs/testing.md) and [CI](docs/ci.md): host, emulator, standalone compile and release checks.
- [Licensing](docs/licensing.md) and [component notices](THIRD_PARTY_NOTICES.md).

Project-owned functions and data members use snake_case. C++ extensions live in
`rtos_stdcpp`; C-linkage hooks use the `rtos_stdcpp_` prefix. Compiler/libstdc++
entry points and native kernel APIs retain their required spellings. The
project's `.clang-format` expands function bodies and separates definitions;
keep a blank line after include groups and declare each member separately.

Originally based on Piotr Grygorczuk's
[FreeRTOS_cpp11](https://github.com/grygorek/FreeRTOS_cpp11).
