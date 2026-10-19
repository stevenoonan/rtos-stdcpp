# Running the tests

The single registry is [tests/catalog.json](../tests/catalog.json): stable case
IDs, suites, explicit requirements and reasons for deferred cases. CMake generates
only the C++ descriptors from it, outside the source tree. Cases use the same
Release-safe assertion/report support on the host and embedded targets.

## Native host checks

The supported host environment is Ubuntu 24.04 x86_64, CMake 3.22+, Ninja,
Python 3 and GCC 13 / Clang 18 with libstdc++ and UBSan. Explicit setup (network
and package-manager privileges required) is:

```sh
sudo apt-get update
sudo apt-get install -y gcc-13 g++-13 clang-18 libclang-rt-18-dev cmake ninja-build qemu-system-arm
```

The system QEMU package supplies maintained shared dependencies; A9 runs use the
separately prepared checksum-selected QEMU binary. After setup, run commands from
the library root. For the quick native checks:

```sh
cmake --preset host-debug -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13
cmake --build --preset host-debug
ctest --preset host-debug --no-tests=error
cmake --preset host-release -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13
cmake --build --preset host-release
ctest --preset host-release --no-tests=error
```

Presets generate files in `../build/rtos-stdcpp/`. Each configuration has five
CTest entries, labeled `host` plus `arithmetic`, `reference` or `harness`:

- The actual backend time arithmetic at 100 and 1000 Hz: deadline edges,
  saturation, 100,000 randomized rounding inputs and 100,000 wrap/extension inputs
  per tick rate. Only these targets see the type-only kernel stubs.
- A standard-library reference run of the shared C++17 cases. This uses native
  host threads, not the embedded backend. Logical priority/stack requests are
  ignored on the host; no embedded scheduling or stack-size support is inferred.
- Runner negative controls, with normal and optimized Python. Their compiled
  fixture exercises a real assertion failure hidden behind a zero process exit,
  a sticky failure across repeat runs, a timeout, a crash, premature zero exit,
  failure after a valid report, incomplete/missing cases and forged counts.
  Additional controls reject duplicate/unknown or skipped required cases,
  missing checks, invalid field types, truncated/duplicate console reports,
  an `OK`-only message, and truncated debugger memory.

The active C++17 cases are lifecycle/moves/detach, mutex contention,
notification/timeout races, exit notifications, recursive/timed mutexes,
condition_variable_any, concurrent call_once, and a move/stack workload. Embedded
platform hooks additionally exercise native worker attributes. Future/async,
shared mutexes, unsupported atomics and C++20 synchronization are explicit skips
with registry reasons; their sources are in `tests/deferred/` and are not built.

## Embedded compile and integration

Put the audited compiler's bin directory on PATH and provide local kernel/port
headers. Nothing is downloaded:

```sh
cmake -S examples/compile-check -B ../build/rtos-stdcpp/compile-check -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-none-eabi.cmake" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DFREERTOS_KERNEL_ROOT=/absolute/path/to/kernel \
  -DFREERTOS_PORT_INCLUDE=/absolute/path/to/kernel/portable/GCC/ARM_CM55_NTZ/non_secure
cmake --build ../build/rtos-stdcpp/compile-check
ctest --test-dir ../build/rtos-stdcpp/compile-check -L compile_link --output-on-failure --no-tests=error
```

The `compile_link` label identifies build checks. This fixture compiles the public
API and all five providers; it has no linked kernel/startup or runtime claim.
Applications continue to verify their final ELF/map and singleton providers.

An embedded integration sets `RTOS_STDCPP_BUILD_TEST_SUPPORT=ON` before
`add_subdirectory`, then links `rtos-stdcpp::test-support` directly alongside the
primary library. It supplies the functions in `tests/support/platform.h`: native
worker attributes, idle opportunities, milliseconds, stack observation and fatal
handling. Shared cases contain no board configuration or direct kernel calls.
The lifecycle identity regression uses a separate FreeRTOS test-support boundary
and retains its ordinary-build result bitmask. With test support enabled it also
records the named `regression` suite.

`run_suite` runs all registered C++17 cases once. Stress integrations may reset
once, call `run_shared_round` repeatedly, call `run_inherited_cases` once, then
finish. Every required case must execute at least one assertion. Check/failure
counts accumulate and a failed case stays failed across subsequent rounds.
The runner is serial at the case level; worker assertions are atomic. All worker
checks must finish before ending their case. `TEST_ASSERT`/`TEST_EQ` record
failures in Release; `TEST_REQUIRE` additionally invokes the platform fatal hook
when continuing would be unsafe. Calling checks outside an active case is fatal.

Applications own board startup, cache/interrupt/FPU health, memory policy,
resource accounting, flash/debug access and hardware deadlines. Reuse the case
sources; do not copy them into an application runner. Hardware transports read
`rtos_stdcpp_conformance_report` and `rtos_stdcpp_regression_report` after a
quiescent result stop and validate them through `tools/test_protocol.py`.
No particular consumer checkout or hardware tool is assumed by this repository.

## Reports and process gates

`tools/run_tests.py` runs one process with an outer deadline, requires exit zero,
and validates exactly one `RTOS_STDCPP_REPORT` JSON line. It verifies every case
ID/state, nonzero assertion counts, aggregate counts, registry-approved skips,
and completion. A final `OK` string, a good report followed by a fault, or a zero
exit with incomplete output fails. It kills the process group on timeout. The
same validator accepts the fixed uint32_t memory layout from a debugger.
Successful output includes pass/fail/skip counts, requirements and skip reasons.
For example, after the host Debug build:

```sh
python3 tools/run_tests.py --suite cxx17 --layer host --timeout 20 \
  --output ../build/rtos-stdcpp/host-reference.json \
  -- ../build/rtos-stdcpp/host-debug/tests/cxx17_reference
```

The shared C++17 report requires 11 passes and 4 catalog-approved skips. Those
feature skips are not missing CTest jobs. A9 expected-failure images instead use
`tools/run_qemu.py --failure` and require both the armed marker and exact status;
an arbitrary nonzero exit is a failure, not an expected success.

The `--layer` value records `host`, `compile_link`, `emulator` or `hardware`;
it identifies the transport/evidence layer, not an automatic support claim.
A hardware runner must apply both the common result validation and its own
native-health checks and timeout. Board transports belong to their integrations.

## Qualified host and A9 matrix

Use the prerequisites above and prepare the exact embedded tools explicitly.
Choose tool and empty result directories outside the source tree; these example
paths must be replaced by local absolute paths:

```sh
python3 tools/setup_ci.py --kind a9 --destination /absolute/path/to/prepared
python3 tools/run_matrix.py \
  --toolchain-root /absolute/path/to/prepared/toolchain \
  --qemu-root /absolute/path/to/prepared/qemu --clang \
  --output /absolute/path/to/new-matrix-results
```

Setup downloads the official checksum-pinned ST bundle and selected QEMU Ubuntu
packages, preserving compiler permissions/symlinks and licenses. It extracts
without sudo; remaining system dependencies must already be installed. Cached
archive/package bytes are verified again on repeat setup. Do not substitute a
nearby compiler distribution. The optional ST IDE descriptor bundle is not needed.

To select only host jobs, use `--environment host --host-compiler both` with
`--output` and omit embedded tool paths. For A9 only use `--environment a9` with
the prepared paths. `--config Debug` or `--config Release` selects one build mode.
If the exact compiler is already installed, `tools/setup_qemu.py --destination
/absolute/path/to/prepared/qemu` prepares only QEMU, and `--toolchain-root` can
refer to the existing compiler. Embedded Clang is unsupported.

The matrix records the Git commit/dirty state, actual host compiler versions,
commands/logs, JUnit and per-image results. It verifies embedded dependency
identities and the QEMU binary/version; it does not hash every project source or
system shared library. Ordinary runs are offline. Embedded dependency updates
require review and qualification; host distro patch updates are permitted.

With --clang, this runs four host compiler/configuration combinations (five CTest
entries each) and four A9 combinations (Debug/Release, normal/wrap; eight CTest
entries each): **52 CTest entries**. Without --clang it runs 42. The matrix rejects
missing prepared tools and every required test failure. It never downloads
inputs. Each configuration records its command logs and JUnit report. A9 has
three success images (shared conformance, lifecycle regression, platform
qualification), four separately reset failure images, and one link-provenance
check. Each emulator process has a 25-second deadline and its CTest entry has an
outer 35-second limit. Expected failures require an armed marker and the exact status, including
OOM, joinable-thread destruction and recursive static initialization.

To build/run one A9 configuration directly:

```sh
cmake -S platforms/qemu-vexpress-a9 -B /absolute/path/to/a9-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/qemu-a9.cmake" \
  -DRTOS_STDCPP_TOOLCHAIN_ROOT=/absolute/path/to/audited/toolchain \
  -DRTOS_STDCPP_QEMU=/absolute/path/to/prepared/qemu/qemu-system-arm
cmake --build /absolute/path/to/a9-build
ctest --test-dir /absolute/path/to/a9-build --output-on-failure --no-tests=error
```

Add -DRTOS_STDCPP_A9_TICK_WRAP=ON in a separate build directory to start the
actual kernel 30 ticks before 32-bit wrap. The platform suite proves the kernel
overflow count increments and steady time advances across it. Its resource gate
warms lazy state, executes 64 shared rounds with exact heap recovery, then 64
allocation/thread cycles, nothrow OOM, notification-index isolation and stack
headroom. Sleep checks include sub-tick rounding, zero/negative durations, a
100 ms sleep, monotonic deadlines and wall-clock steps. Saturation/very long
relative-time arithmetic is checked on the host without waiting for that duration.
The STM32 integration supplies sustained physical timing/resource/failure and
wrap evidence independently; its commands belong in that application checkout.

See the [A9 platform contract](../platforms/qemu-vexpress-a9/README.md) and
[support matrix](support-matrix.md). RISC-V has historical sources and no active
runner. A compile-only fixture or successful host run does not qualify a scheduler.

The [CI contract](ci.md) documents hosted jobs, explicit dependency setup, merge
enforcement and physical-board obligations.

## Physical-board qualification

The library does not assume a particular board/probe or consumer checkout.
An integrating application documents its build/flash/reset commands and board
runner. Enable the shared test-support target only in acceptance firmware and
validate its stopped reports with `tools/test_protocol.py`, alongside native
health observations. Qualify Debug/Release sustained resource recovery, idle
reclamation, measured stack headroom, independent timing, FPU/preemption,
interrupt/cache configuration, real tick wrap and isolated failure/reset paths.
Restore and smoke-test ordinary firmware afterward. A host or emulator pass does
not replace these physical checks. The C++17 release-baseline milestone repeats
full board acceptance; documentation-only edits do not require a new endurance run.
