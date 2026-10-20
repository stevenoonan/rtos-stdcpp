# Testing

Run commands from the library root. Use CMake 3.22+, Ninja and Python 3. The host
matrix uses Ubuntu 24.04 x86_64, GCC 13 and Clang 18 with libstdc++ and UBSan.
Install prerequisites explicitly:

```sh
sudo apt-get update
sudo apt-get install -y gcc-13 g++-13 clang-18 libclang-rt-18-dev cmake ninja-build qemu-system-arm
```

## Host

```sh
cmake --preset host-debug -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13
cmake --build --preset host-debug
ctest --preset host-debug --no-tests=error
```

Use `host-release` for Release. Build output goes to `../build/rtos-stdcpp`.
Set `-DRTOS_STDCPP_CXX_STANDARD=20` for C++20; use a separate build directory
when keeping both modes. Each C++17 configuration has five CTest entries; C++20
adds the required concurrency suite: actual backend arithmetic at two tick
rates, standard-library reference cases, and normal/optimized runner controls.
Host threads use the host runtime; they do not exercise the FreeRTOS scheduler.

## Embedded compile check

Put the qualified compiler on PATH and supply matching kernel/port headers:

```sh
cmake -S examples/compile-check -B ../build/rtos-stdcpp/compile-check -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-none-eabi.cmake" \
  -DFREERTOS_KERNEL_ROOT=/absolute/path/to/kernel \
  -DFREERTOS_PORT_INCLUDE=/absolute/path/to/kernel/portable/GCC/ARM_CM55_NTZ/non_secure
cmake --build ../build/rtos-stdcpp/compile-check
ctest --test-dir ../build/rtos-stdcpp/compile-check --output-on-failure --no-tests=error
```

Select `-DRTOS_STDCPP_PROFILE=stm32-gcc-14.3.1-cxx20` for the C++20 probe.
This compiles the public API and seven provider objects. It does not link or run
a board image. Applications must check their final ELF/map and kernel providers.

## Host and QEMU matrix

Prepare tools and choose a new empty result directory outside the source tree:

```sh
python3 tools/setup_ci.py --kind a9 --destination /absolute/path/to/tools
python3 tools/run_matrix.py \
  --toolchain-root /absolute/path/to/tools/toolchain \
  --qemu-root /absolute/path/to/tools/qemu --clang \
  --output /absolute/path/to/results
```

Setup verifies and extracts the exact ST compiler archive and QEMU packages.
Cached downloads are verified again; matrix execution uses no network. The system
QEMU package supplies shared dependencies, while runs use the prepared binary.

The full matrix tests C++17 and C++20 with GCC/Clang Debug/Release and A9
Debug/Release, each A9 configuration with ordinary and real tick-wrap starts
and both default and custom TLS allocation. The custom fixture uses tagged
`malloc`/`free` allocations in the separate C++ arena, checks hook usage and exact
recovery, and verifies the TLS object has no FreeRTOS allocation references.
Select `--tls-allocator freertos` or `--tls-allocator custom` to run one policy
(default: `both`); this switch affects A9 only.
Select `--standard 17` or `--standard 20` for one mode (default: both). Select
`--environment host --host-compiler both` for host only, `--environment a9` for A9
only, or `--config Debug` / `Release` for one configuration. Reports contain the
tested commit, compiler versions, commands/logs, JUnit and per-image results.

A9 runs conformance, lifecycle and platform suites plus eight isolated fatal
cases and link provenance. TLS failures cover startup/native-task access,
allocation exhaustion and size overflow. Shared TLS cases run in every stress
round and require exact resource recovery. Expected failures require the armed
marker and exact exit status; a crash or hang is a failure. Each emulator process has a 25-second
deadline, with an outer CTest limit. See the [platform contract](../platforms/qemu-vexpress-a9/README.md).

## Shared reports and hardware

[The catalog](../tests/catalog.json) defines case IDs, requirements and approved
skips. The C++17 suite requires 12 passes and 4 feature skips; the lifecycle
suite requires one pass. The C++20 suite requires eight passes and no skips. Assertions stay active in Release. Every required case
must run checks, and failures remain sticky across repeated rounds.

`tools/run_tests.py` requires a zero process exit and one complete, consistent
`RTOS_STDCPP_REPORT` line. Missing/duplicate cases, malformed reports, unapproved
skips, faults after reports and timeouts fail. For example:

```sh
python3 tools/run_tests.py --suite cxx17 --layer host --timeout 20 \
  --output ../build/rtos-stdcpp/host-reference.json \
  -- ../build/rtos-stdcpp/host-debug/tests/cxx17_reference
```

For embedded acceptance, enable `RTOS_STDCPP_BUILD_TEST_SUPPORT`, directly link
`rtos-stdcpp::test-support` and implement [platform.h](../tests/support/platform.h).
`run_suite` executes all cases once; stress runners can call `run_shared_round`
repeatedly and `run_facility_cases` once. The runner is serial at case level;
worker checks must finish before ending a case. In C++20, `run_cxx20_suite`
executes its own report; endurance runners reset that report to `Suite::cxx20`,
repeat `run_cxx20_round` and require `finish` success. Its embedded test hooks
force stop-state lock contention across priority tiers; production builds omit
those hooks. Warm C++20 resources before taking allocation measurements.

The application owns board startup, timing/resource observations, flash/debug
transport and deadlines. Read the stopped conformance/regression/C++20 reports through
`tools/test_protocol.py` and apply native-health checks too. Full physical
qualification covers Debug/Release endurance, exact warmed resource recovery,
stack headroom, FPU/preemption, independent timing, interrupts/cache, real wrap,
fault/reset paths and restoration of healthy ordinary firmware. Use exclusive
board/probe access. See [CI](ci.md) and the [support matrix](support-matrix.md).
