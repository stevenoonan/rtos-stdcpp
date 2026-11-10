# Testing

Run commands from the library root. Use CMake 3.22+, Ninja and Python 3. The host
matrix uses Ubuntu 24.04 x86_64, GCC 13 and Clang 18 with libstdc++, UBSan and ASan+UBSan.
Install prerequisites explicitly:

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends gcc-13 g++-13 clang-18 libclang-rt-18-dev cmake ninja-build qemu-system-arm
```

## Host

```sh
cmake --preset host-debug -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13
cmake --build --preset host-debug
ctest --preset host-debug --no-tests=error
```

Use `host-release` for Release. Build output goes to `../build/rtos-stdcpp`.
Set `-DRTOS_STDCPP_CXX_STANDARD=20` for C++20; use a separate build directory
when keeping both modes. Each UBSan C++17 configuration has twelve CTest
entries; C++20 adds its concurrency suite. These cover both backends' deadline
arithmetic at two tick rates, FreeRTOS tick extension, thread identity/allocator
and backend lifecycle fixtures, standard-library reference cases, runner controls
and matrix orchestration/artifact selection controls.
Set `-DRTOS_STDCPP_HOST_SANITIZER=address` in a separate build directory for
ASan+UBSan with leak detection; CI sets `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`.
This adds a CTest negative control that requires a heap-use-after-free diagnostic
and the exact expected exit status. The default `undefined` lane remains enabled.
Host threads use the host runtime; they do not exercise either embedded kernel.

## Embedded compile checks

The [standalone fixture](../examples/compile-check/README.md) selects either
backend using explicit vendor kernel headers. It compiles the public C++ API
and the optional compiler runtime without any board project or HAL dependency.
ThreadX also compiles a bootstrap example that initializes the adapter/runtime
and starts a managed standard C++ worker.

The fixture README owns the exact configure/build/test commands for both
backends. Select `RTOS_STDCPP_BACKEND`, supply that kernel's include tree, and
choose the C++17 or C++20 profile. No inputs are downloaded by CMake.
Compilation does not produce a final board image or run a scheduler;
applications still verify their ELF/map and physical behavior.

## Release checks

Run the standalone build checks with a compiler installation and the vendor
kernel trees you intend to support:

```sh
python3 tools/check_standalone.py \
  --toolchain-root /absolute/path/to/toolchain \
  --freertos-kernel-root /absolute/path/to/freertos \
  --freertos-port-include /absolute/path/to/freertos/portable/GCC/ARM_CM55_NTZ/non_secure \
  --threadx-root /absolute/path/to/threadx \
  --output /absolute/path/to/new-release-results
```

The checker copies library sources and required vendor headers into the result
directory, then configures/builds/tests only those isolated copies. By default it
checks GCC host C++17/C++20 in Debug/Release and both embedded backends in both
modes/configurations with default and custom TLS allocation. It verifies runtime
provider selection, header selection and TLS ABI symbols, and rejects invalid
configuration.
Vendor inputs and toolchains are explicit: no check downloads dependencies.

Use `--backend FreeRTOS` or `--backend ThreadX` for one backend; only that
backend's input arguments are required. `--standard 17|20`,
`--config Debug|Release` and `--skip-host` select a smaller check. The result
location must be new, empty and outside the library. Host checks default to
`gcc-13`/`g++-13`, overrideable with `--host-cc` and `--host-cxx`.

CI uses the public vendor revisions recorded in
[embedded kernel inputs](../cmake/profiles/embedded-kernel-inputs.json).
`tools/setup_ci.py --kind embedded --destination /absolute/path/to/tools`
explicitly downloads/prepares the qualified compiler; unlike `--kind qemu`, it
omits emulator setup. Builds and release checks use the prepared inputs offline.

## Host and QEMU matrix

Prepare tools and choose a new empty result directory outside the source tree:

```sh
python3 tools/setup_ci.py --kind qemu --destination /absolute/path/to/tools
python3 tools/run_matrix.py \
  --toolchain-root /absolute/path/to/tools/toolchain \
  --qemu-root /absolute/path/to/tools/qemu --clang \
  --output /absolute/path/to/results
```

Setup verifies and extracts the exact ST compiler archive and QEMU packages.
Cached downloads are verified again; matrix execution uses no network. The system
QEMU package supplies shared dependencies, while runs use the prepared binary.
Install it with `--no-install-recommends` to omit optional GUI, audio and extra
storage packages from the headless test environment.

The full matrix tests C++17 and C++20 with GCC/Clang Debug/Release and FreeRTOS A9
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

A9 runs the shared conformance, lifecycle and qualification suites, FreeRTOS
notification isolation, eight isolated fatal cases and runtime link checks. TLS failures cover startup/native-task access,
allocation exhaustion and size overflow. Shared TLS cases run in every stress
round and require exact resource recovery. Expected failures require the armed
marker and exact exit status; a crash or hang is a failure. Each emulator process has a 25-second
deadline, with an outer CTest limit. See the [platform contract](../platforms/qemu-vexpress-a9/README.md).

## Cortex-M execution

Prepare the same compiler/QEMU tools above, then check out the public kernels at
the `FreeRTOS-QEMU` and `ThreadX-QEMU` commits in
[embedded kernel inputs](../cmake/profiles/embedded-kernel-inputs.json).
The runner verifies their commits and rejects tracked changes. The pinned paths
are also sufficient for sparse checkouts using Git's cone mode, which includes
the FreeRTOS root C files automatically.

```sh
python3 tools/run_cortex_m_matrix.py \
  --toolchain-root /absolute/path/to/tools/toolchain \
  --qemu-root /absolute/path/to/tools/qemu \
  --freertos-root /absolute/path/to/pinned-freertos \
  --threadx-root /absolute/path/to/pinned-threadx \
  --output /absolute/path/to/new-cortex-m-results
python3 tools/run_matrix.py --environment host --host-compiler both \
  --sanitizer address --output /absolute/path/to/new-asan-results
```

The Cortex-M command runs all 96 backend/core/language/configuration/allocator/tick
variants. Select `--backend FreeRTOS|ThreadX`, `--core m4|m7|m55`,
`--standard 17|20`, `--config Debug|Release`, `--tls-allocator default|custom`, or
`--tick-wrap normal|wrap` to narrow it. Only selected kernels' paths are required.
Each ThreadX C++17 build has 11 CTest entries; FreeRTOS has 13 because of its two
backend-specific checks. C++20 adds one entry to each. The actual kernel, port,
adapter and optional runtime providers execute in the emulator. Logs, JUnit and
reports remain in the output directory. See the
[platform contract](../platforms/qemu-cortex-m/README.md).

## Backend parity

Both backends run exactly the same required shared cases and seven fatal
scenarios on each M4/M7/M55 model. Shared qualification has six required cases:

| Case | Common requirement |
| --- | --- |
| `runtime.startup` | Allocation and guarded local-static initialization from startup constructors |
| `runtime.interrupt_masking` | Kernel ticks remain fixed while a hardware counter advances across three tick periods |
| `time.clocks_sleep` | Real 32-bit wrap, monotonic extension, negative/zero/subtick sleeps, sleep-until and wall-clock adjustments |
| `runtime.stress` | Four C++17 warm-up rounds, then 64 shared C++17 and, when selected, C++20 rounds with exact resource recovery |
| `runtime.resources` | Warmed heap/thread/TLS recovery, 128 further joined/detached workers, destructors, aligned/nothrow allocation and OOM |
| `runtime.fpu_context` | Distinct FP register contents survive tick interrupts and preemption across priority tiers |

Resource hooks measure newlib usage, live kernel tasks and custom TLS blocks;
FreeRTOS also measures its separate kernel heap. Host arithmetic checks use the
same deadline-rounding oracle and boundary/random vectors for each backend at
100 Hz and 1 kHz. Tick-extension arithmetic is FreeRTOS-specific: ThreadX
maintains a 64-bit counter directly; runtime wrap checks still run on both. Cortex-M probes keep S0-S31
live without compiler spills. Supplemental A9 probes keep D0-D31 live. MPS
interrupt masking uses the SysTick hardware COUNTFLAG while delivery is masked;
A9 uses its independent global timer. Stack observations require 128 free words.

FreeRTOS adds a required notification-index-isolation suite and an armed
kernel-heap allocation failure. ThreadX has no notification-array equivalent and
uses the same arena for kernel adapter and C++ allocations, so the common
allocation/TLS failure scenarios cover its allocator policy. These are documented
backend differences, not skipped common cases. C++17 retains its four approved
feature skips equally on both backends.

Qualification images have a 90-second emulator deadline and 100-second CTest
limit; other images have 25/35-second limits. Each fatal image must print its
exact armed marker and exit with the expected status; arbitrary crashes and
unexpected survival fail. The common runtime audit checks provider uniqueness,
stock fallback exclusion, CPU/newlib ABI flags and TLS allocator selection.

## Shared reports and hardware

[The catalog](../tests/catalog.json) defines case IDs, requirements and approved
skips. The C++17 suite requires 12 passes and 4 feature skips; the shared backend
lifecycle suite requires one pass. The C++20 suite requires eight passes and no
skips. Assertions stay active in Release. Every required case must run checks,
and failures remain sticky across repeated rounds.

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
This shared support can be used with either selected backend; the lifecycle and emulator qualification runners build on the shared conformance cases.
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
