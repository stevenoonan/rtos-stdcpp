# rtos-stdcpp

rtos-stdcpp began as a fork of Piotr Grygorczuk's
[FreeRTOS_cpp11](https://github.com/grygorek/FreeRTOS_cpp11) and is now maintained
independently. It integrates standard C++ threading facilities with FreeRTOS
through GCC/libstdc++ and requires **C++17 or later**.

C++17 is the qualified baseline. Selecting C++20 does not establish support for
its concurrency features; that work is deferred. Applications use `<thread>`,
`<mutex>`, `<condition_variable>` and `<chrono>`. Optional project extensions
configure worker attributes and wall-clock time.

## Supported configurations

| Configuration | Status |
|---|---|
| ST GCC 14.3.1, Cortex-M55, newlib-nano, FreeRTOS 11.2.0 | Executed Debug/Release through a separate board integration; the application supplies its kernel, startup and runtime policy |
| ST GCC 14.3.1, QEMU vexpress-a9, bundled FreeRTOS 10.4.3 snapshot | Standalone scheduler tests, Debug/Release and real tick-wrap variants |
| Ubuntu 24.04 x86_64, GCC 13 or Clang 18, system libstdc++ | Host reference/arithmetic tests with UBSan; does not execute the FreeRTOS adapter |

The [support matrix](docs/support-matrix.md) defines exact embedded profiles,
features and evidence. Supported cases cover thread lifecycle/identity, ordinary
and recursive/timed mutexes, condition variables, once/local-static initialization,
exit notification, monotonic clocks and sleeps. Exceptions and RTTI are disabled
in the embedded profiles.

Language `thread_local`, extra gthread keys, future/async, shared mutexes, C++20
concurrency, unsupported atomic fallbacks, embedded Clang/libc++, RISC-V and SMP
are outside this baseline. Threading operations require task context after
scheduler startup. TLS pointer slot 0 and task notification index 0 are reserved;
managed tasks must return normally and idle must run to reclaim deleted tasks.

## Quick start

The maintained repository is private; obtain read access before cloning:

```sh
git clone --branch rtos-stdcpp https://github.com/stevenoonan/rtos-stdcpp.git
cd rtos-stdcpp
cmake --preset host-debug -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13
cmake --build --preset host-debug
ctest --preset host-debug --no-tests=error
```

Install CMake 3.22+, Ninja, Python 3 and the supported host compiler first;
[testing](docs/testing.md) gives Ubuntu prerequisites and the complete matrix.
These five host CTest entries check reference behavior, actual time helpers and
the test harness. For embedded execution use the documented
[A9 setup and run commands](docs/testing.md#qualified-host-and-a9-matrix).

An application integrates the library with CMake `add_subdirectory`, an existing
FreeRTOS provider target and an exact profile. Link `rtos-stdcpp::rtos-stdcpp`
directly into the final executable and apply `rtos-stdcpp::config` to every
affected C/C++ target. The library includes the two selected GCC runtime sources;
it does not inject a kernel, startup code or allocation policy into applications.
See the [integration guide](docs/integration.md) for a minimal C++17 example and
the complete runtime/configuration contract.

## Documentation

- [Integration](docs/integration.md): acquiring inputs, CMake, extensions and platform obligations.
- [Architecture](docs/architecture.md): libstdc++/GCC hooks, native ABI, lifetime, clocks and ownership.
- [Support matrix](docs/support-matrix.md): tested profiles, feature limits and failure behavior.
- [Testing](docs/testing.md) and [CI](docs/ci.md): host, compile-only, emulator and hardware evidence.
- [Licensing and dependency updates](docs/licensing.md) and [component notices](THIRD_PARTY_NOTICES.md).
- [Migration](docs/migration.md): renamed checkout, targets, headers and required clean rebuilds.

The inherited implementation article is preserved in Git at commit `6677ce1`
(`README.md`, “Historical implementation guide”). It describes older layouts and
unqualified features; use the maintained guides above for current integration.
The inherited [C++20 checklist](cpp20_actions.md) is historical, not qualification.

## Attribution and licenses

Retain Piotr Grygorczuk's original notices and the
[original project license](LICENSES/FreeRTOS_cpp11-original.txt). Credit to Jakub
Sosnovec for the original custom-stack solution that inspired thread attributes.
The [root MIT license](LICENSE) covers project code; FreeRTOS, Arm support and
selected GCC runtime sources retain their separate licenses. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for component scope.
