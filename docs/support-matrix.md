# Supported language modes and platforms

C++17 is the minimum. C++17 and C++20 are tested separately; C++11/C++14
compatibility is not maintained.
Both embedded profiles disable exceptions and RTTI and use newlib-nano.

## Executed and build-only profiles

| Environment | Compiler, runtime and dependencies | Evidence |
|---|---|---|
| Ubuntu 24.04 x86_64 GCC | GCC 13, system libstdc++, UBSan; distro patch updates permitted | Debug/Release host arithmetic, standard-library reference cases and runner controls |
| Ubuntu 24.04 x86_64 Clang | Clang 18 with GCC 13's C++ headers/libstdc++, UBSan; distro patch updates permitted | Same host checks; no embedded Clang claim |
| Cortex-M55 board integration | `stm32-gcc-14.3.1-cxx17`, ST GCC 14.3.1, hard float, `thumb/v8-m.main+dp/hard`; FreeRTOS 11.2.0 / X-CUBE-FREERTOS 1.6.0 | Application-owned Debug/Release final-image and sustained physical-board qualification, real wrap and failure/reset tests |
| Standalone M55 compile fixture | Same compiler/multilib with externally supplied matching kernel/port headers | Public APIs and seven provider objects compile; no scheduler execution or final executable |
| QEMU vexpress-a9, one Cortex-A9 | `qemu-a9-gcc-14.3.1-cxx17`, same ST compiler distribution, `thumb/v7-a+fp/hard` selected with ARM/VFPv3 flags; bundled 10.4.3 kernel/ARM_CA9 port; selected QEMU 8.2.2 packages | Debug/Release, ordinary and near-wrap starts; shared cases, lifecycle/platform suites, isolated failures and final-image provenance |

The exact embedded compiler identity is GNU Tools for STM32
`14.3.rel1.20251027-0700`, GCC `14.3.1 20250623`. Header/archive/compiler identities
and CPU flags are in the [M55](../cmake/profiles/stm32-gcc-14.3.1-cxx17.json) and
[A9](../cmake/profiles/qemu-a9-gcc-14.3.1-cxx17.json) profiles. Both select the same
three unmodified [GCC runtime sources](../third_party/libstdcxx/stm32-gcc-14.3.1-cxx17/SOURCES.json).
QEMU's binary/packages and selected kernel bytes are pinned separately. The
kernel's exact upstream commit is not established by its version label.

C++20 selects `stm32-gcc-14.3.1-cxx20` or `qemu-a9-gcc-14.3.1-cxx20`. These
reuse the C++17 profiles' compiler/runtime identities and select the
[stop-token override](../third_party/libstdcxx/stm32-gcc-14.3.1-cxx20/SOURCES.json).
The [CI matrix](ci.md) runs both language modes across host and A9 profiles. Board
commands and physical measurements belong to the integrating application.

## Feature boundary

| Facility | Qualified behavior or limit |
|---|---|
| `std::thread` / attributes | Callable/argument ownership, moves, join/detach before/after completion, IDs/hashes; worker attributes observed on embedded platforms |
| Mutexes | Ordinary, recursive, timed and recursive-timed operations, contention and standard lock wrappers |
| Condition variables | Predicate wait/notify-one/all, timed races, explicit predicate rechecks, `condition_variable_any`; index 0 reserved |
| Once and local statics | Concurrent `call_once`; thread-safe function-local static initialization with platform guard provider |
| Thread exit notification | Existing single gthread key/callback path, after language TLS destructors; referenced objects must outlive notification |
| Language `thread_local` | Managed workers: per-thread initialization, extended alignment, cross-TU identity, reverse-order destructors and heap recovery; GCC emulated TLS |
| Clocks and sleep | Separate monotonic/wall clocks, duration/deadline rounding, zero/negative sleeps, kernel tick wrap |
| Atomic facilities | Lock-free integral/pointer wait/notify and `atomic_flag`; non-lock-free fallbacks excluded |
| C++20 threads | `jthread`, stop-source/token/callback ownership, moves, cancellation, stop-and-join destruction and attributes |
| C++20 synchronization | Stop-aware `condition_variable_any`, binary/counting semaphores, latch and multi-phase barrier with completion/drop |
| Future/async and shared mutexes | Not qualified |

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

General threading/synchronization operations require running task context. The
M55 application additionally qualifies ordinary mutex construction and
uncontended operations during single-threaded startup, followed by task-context
reuse. Other startup facilities remain application-owned; a platform may
support allocation/static initialization there,
but not concurrent RTOS-backed constructors or blocking operations. Language
TLS access before scheduler startup or in native tasks, native ELF TLS, arbitrary
extra gthread keys, native-task exit callbacks, ISR thread operations, external
deletion of managed tasks, and reuse of TLS pointer
slot 0 or notification index 0 are unsupported. Idle must receive CPU time.
Attributes-helper callable copy/move constructors must not block or yield.

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
