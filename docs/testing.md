# Running the tests

The single registry is [tests/catalog.json](../tests/catalog.json): stable case
IDs, suites, explicit requirements and reasons for deferred cases. CMake generates
only the C++ descriptors from it, outside the source tree. Cases use the same
Release-safe assertion/report support on the host and embedded targets.

## Native host checks

With CMake 3.22+, Ninja, Python 3 and GNU host C++ with UBSan:

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug --no-tests=error
cmake --preset host-release
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

The `--layer` value records `host`, `compile_link`, `emulator` or `hardware`;
it identifies the transport/evidence layer, not an automatic support claim.
A hardware runner must apply both the common result validation and its own
native-health checks and timeout. Board transports belong to their integrations.

## Emulator handoff

The common `tests/support/embedded_main.cpp` writes the report and exits with its
result. For A9, this calls the inherited `SYS_EXIT_EXTENDED` semihost mechanism;
if that mechanism returns, the platform waits and the outer deadline fails.
After separately qualifying an A9 compiler/kernel/image, its execution command is:

```sh
python3 tools/run_tests.py --suite cxx17 --layer emulator --timeout 30 -- \
  qemu-system-arm -M vexpress-a9 -m 128M -nographic \
  -semihosting-config enable=on,target=native -kernel /absolute/path/to/qualified-test.elf
```

The platform can use `platforms/qemu-vexpress-a9/test_platform.cpp` for the
support hooks. Neither an A9 image nor QEMU is qualified by the host process
fixtures. Actual emulator build/execution, dependencies and UART/exit behavior
must be qualified in Step 6. The RISC-V source is historical with no active runner.
