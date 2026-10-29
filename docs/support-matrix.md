# Supported language modes and platforms

C++17 is the minimum. C++17 and C++20 are tested separately; C++11/C++14
compatibility is not maintained.
Embedded profiles disable exceptions and RTTI and select newlib-nano
headers and libraries through `rtos-stdcpp::config`.

## Backend and profile selection

| Backend | Profile family | Configuration contract |
| --- | --- | --- |
| FreeRTOS | `stm32-gcc-14.3.1-cxx17` / `cxx20`, `qemu-a9-gcc-14.3.1-cxx17` / `cxx20` | [FreeRTOS integration](backends/freertos.md) |
| ThreadX | `stm32-gcc-14.3.1-cxx17` / `cxx20` | [ThreadX integration](backends/threadx.md) |

The CMake profile/backend combinations above are distinct from executed evidence
below. The standalone compile fixture checks both backends. The QEMU runner selects
FreeRTOS; ThreadX execution uses a consumer-owned board runner. Both backends share the
[integration contract](integration.md) and language-TLS implementation.

## Executed and build-only profiles

| Environment | Compiler, runtime and dependencies | Evidence |
|---|---|---|
| Ubuntu 24.04 x86_64 GCC | GCC 13, system libstdc++, UBSan; distro patch updates permitted | Debug/Release host arithmetic, standard-library reference cases and runner controls |
| Ubuntu 24.04 x86_64 Clang | Clang 18 with GCC 13's C++ headers/libstdc++, UBSan; distro patch updates permitted | Same host checks; no embedded Clang claim |
| FreeRTOS Cortex-M55 board integration | `stm32-gcc-14.3.1-cxx17`, ST GCC 14.3.1, hard float, `thumb/v8-m.main+dp/hard`; FreeRTOS 11.2.0 / X-CUBE-FREERTOS 1.6.0 | Application-owned Debug/Release final-image and sustained physical-board qualification, real wrap and failure/reset tests |
| ThreadX STM32N6570-DK integration | `stm32-gcc-14.3.1-cxx20`, same ST compiler/multilib; consumer-pinned STM32CubeN6 ThreadX 6.4 Cortex-M55 port | Consumer-owned Debug/Release C++17/C++20 shared suites, tick wrap, FPU preemption, lwIP primitives and repeated resource recovery; broader board/failure/network qualification is separate |
| Standalone FreeRTOS/ThreadX M55 compile fixtures | Same compiler/multilib with explicit vendor kernel/port headers | C++17/C++20 Debug/Release public APIs, native bootstrap (ThreadX), compiler runtime and TLS allocation policies compile; no scheduler execution or final executable |
| QEMU vexpress-a9, one Cortex-A9 | `qemu-a9-gcc-14.3.1-cxx17`, same ST compiler distribution, `thumb/v7-a+fp/hard` selected with ARM/VFPv3 flags; bundled 10.4.3 kernel/ARM_CA9 port; selected QEMU 8.2.2 packages | Debug/Release, ordinary and near-wrap starts; shared cases, lifecycle/platform suites, isolated failures and final-image provenance |

The tested embedded compiler is GNU Tools for STM32
`14.3.rel1.20251027-0700`, GCC `14.3.1 20250623`. CMake requires GCC 14.3.1
for C and C++ without checking the vendor banner or file hashes.
CPU flags are in the [M55](../cmake/profiles/stm32-gcc-14.3.1-cxx17.json) and
[A9](../cmake/profiles/qemu-a9-gcc-14.3.1-cxx17.json) profiles. Both select the same
three unmodified [GCC runtime sources](../third_party/libstdcxx/stm32-gcc-14.3.1-cxx17/SOURCES.json).
The setup scripts pin the downloaded compiler archive and QEMU packages. The
bundled FreeRTOS kernel's exact upstream commit is not established by its version
label. Consumers record the exact kernel/port source identity for their images.

C++20 selects `stm32-gcc-14.3.1-cxx20` or `qemu-a9-gcc-14.3.1-cxx20`. These
reuse the C++17 profiles' CPU flags and GCC runtime sources and select the
[stop-token override](../third_party/libstdcxx/stm32-gcc-14.3.1-cxx20/SOURCES.json).
The [CI matrix](ci.md) runs both language modes across host and A9 profiles. Board
commands and physical measurements belong to the integrating application.

## Feature boundary

| Facility | Qualified behavior or limit |
|---|---|
| `std::thread` / attributes | Callable/argument ownership, moves, join/detach before/after completion, IDs/hashes; worker attributes observed on embedded platforms |
| Mutexes | Ordinary, recursive, timed and recursive-timed operations, contention and standard lock wrappers |
| Condition variables | Predicate wait/notify-one/all, timed races, explicit predicate rechecks, `condition_variable_any`; FreeRTOS notification index 0 reserved |
| Once and local statics | Concurrent `call_once`; thread-safe function-local static initialization with platform guard provider |
| Thread exit notification | Existing single gthread key/callback path, after language TLS destructors; referenced objects must outlive notification |
| Language `thread_local` | Managed workers: per-thread initialization, extended alignment, cross-TU identity, reverse-order destructors and heap recovery; GCC emulated TLS |
| Clocks and sleep | Separate monotonic/wall clocks, duration/deadline rounding, zero/negative sleeps, kernel tick wrap |
| Atomic facilities | Lock-free integral/pointer wait/notify and `atomic_flag`; non-lock-free fallbacks excluded |
| C++20 threads | `jthread`, stop-source/token/callback ownership, moves, cancellation, stop-and-join destruction and attributes |
| C++20 synchronization | Stop-aware `condition_variable_any`, binary/counting semaphores, latch and multi-phase barrier with completion/drop |
| Promise/future, async and shared mutexes | Matching GCC future provider is linked; consumer STM32 checks cover promise/future completion and timeouts. General future/async and shared mutex qualification remain outside the shared suite |

The C++20 suite requires **8 passes without skips**. The shared C++17 suite
requires **12 passes and 4 approved skips**. The skip
reasons are authoritative in [the catalog](../tests/catalog.json). The FreeRTOS
lifecycle suite requires one named regression pass; each arithmetic run requires
two passes. These suite counts are distinct from the number of CTest entries.
The A9 platform suite requires four passes, including a tick-interrupt masking
probe across three hardware timer periods, 64 shared stress rounds
and 64 allocation/thread recovery cycles after warm-up, exact resource recovery,
at least 128 free stack words and a real kernel-overflow observation in the wrap
variant. Emulator timing is not a physical-board latency guarantee.

## Context, failures and exclusions

General threading/synchronization operations require running thread context.
Startup allowances are backend-specific: see
[FreeRTOS startup](backends/freertos.md#startup-and-interrupts) and
[ThreadX startup](backends/threadx.md#startup-and-interrupts). A consumer may
support allocation/static initialization there, but not concurrent RTOS-backed
constructors or blocking operations. Language TLS access before scheduler
startup or in native workers, native ELF TLS, arbitrary extra gthread keys,
native-worker exit callbacks, ISR thread operations and external deletion of
managed workers are unsupported.

FreeRTOS reserves TLS pointer slot 0 and notification index 0 and needs idle
cleanup opportunities. ThreadX reserves its TCB extension and needs the reaper
to run. Attribute-helper callable/argument construction follows the
[FreeRTOS](backends/freertos.md#thread-attributes) or
[ThreadX](backends/threadx.md#thread-attributes) synchronization contract.

Under the no-exceptions profiles, errors on throwing paths terminate through the
platform policy. Returning null for explicit nothrow/aligned allocation requires
the platform's qualified provider; it does not promise recoverable `std::thread`
construction failure. A9 resets separately for throwing-new, kernel-OOM,
joinable-thread destruction, recursive-static-guard failures, TLS context misuse,
TLS allocation exhaustion and TLS size overflow. Exact armed
markers/statuses distinguish expected failures from arbitrary crashes.

Additional GCC versions, new kernel/port combinations, embedded Clang, libc++,
RISC-V, SMP and additional C++20 facilities require separate qualification. A host or compile-only check does not establish embedded support.

## Changing or adding a profile

Review compiler configuration, native layouts, header feature paths, multilib,
newlib ABI, selected runtime symbols and kernel/port behavior. Record source and
license provenance and update only the affected dependency manifests. Rebuild all
affected objects, verify final providers, and execute Debug/Release shared,
platform, failure and wrap tests before claiming support. Board profiles also
need physical timing/resource/FPU/interrupt qualification. Host distro patch
updates at the supported majors do not require refreshing binary hashes; their
actual versions are recorded by [the matrix runner](testing.md).
