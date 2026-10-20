# rtos-stdcpp

rtos-stdcpp connects standard C++ threading APIs to FreeRTOS through GCC/libstdc++.
**C++17 is required.** It is independently maintained and based on Piotr
Grygorczuk's [FreeRTOS_cpp11](https://github.com/grygorek/FreeRTOS_cpp11).

Applications use `<thread>`, `<mutex>`, `<condition_variable>` and `<chrono>`.
Extensions configure thread attributes and wall-clock time. The embedded profiles
use ST GCC 14.3.1 and newlib-nano with exceptions and RTTI disabled.

## Build and test

The repository is private; authorized read access is required.

```sh
git clone --branch rtos-stdcpp https://github.com/stevenoonan/rtos-stdcpp.git
cd rtos-stdcpp
cmake --preset host-debug -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13
cmake --build --preset host-debug
ctest --preset host-debug --no-tests=error
```

Install CMake 3.22+, Ninja, Python 3 and GCC 13 first. These are host reference
and arithmetic tests. [Testing](docs/testing.md) describes Clang, embedded
compile checks and the standalone QEMU A9 suite.

For firmware, supply a qualified FreeRTOS target, select an exact profile and
link `rtos-stdcpp::rtos-stdcpp` directly into the executable. Apply
`rtos-stdcpp::config` to all affected C/C++ targets. See [integration](docs/integration.md).

## Support and limits

Qualified configurations cover Cortex-M55 application integrations and standalone
QEMU vexpress-a9. Ubuntu GCC 13 and Clang 18 run host checks. The
[support matrix](docs/support-matrix.md) defines the exact profiles and facilities.

Threading operations require running task context. TLS pointer slot 0 and
notification index 0 are reserved; idle must run to reclaim deleted tasks.
Language `thread_local`, extra gthread keys, ISR thread operations, external
deletion of managed tasks, future/async, shared mutexes and C++20 concurrency are
unsupported.

- [Integration](docs/integration.md): targets, configuration, examples and runtime obligations.
- [Architecture](docs/architecture.md): hooks, ownership, synchronization and clocks.
- [Testing](docs/testing.md) and [CI](docs/ci.md): commands, reports and hardware responsibilities.
- [Licensing](docs/licensing.md) and [component notices](THIRD_PARTY_NOTICES.md).

Retain Piotr Grygorczuk's original notices and the
[original license](LICENSES/FreeRTOS_cpp11-original.txt). Credit to Jakub Sosnovec
for the custom-stack solution that inspired thread attributes. The root
[MIT license](LICENSE) covers project code; vendor components keep their own terms.
