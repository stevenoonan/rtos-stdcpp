# Qualified C++17 matrix

The Step 6 matrix has executed evidence in Debug and Release. C++17 is the
minimum. Requiring a newer language mode does not qualify its concurrency APIs.
Exact byte locks are in `cmake/profiles`; version numbers alone are insufficient.

| Environment | Exact compiler/runtime and dependencies | Evidence and status |
|---|---|---|
| Linux x86_64 GCC | Ubuntu GCC 13 (patch updates allowed), system libstdc++, C++17, UBSan; maintained distro patches allowed | Executed Debug/Release: real helper arithmetic at 100/1000 Hz, portable reference cases and strict runner controls |
| Linux x86_64 Clang | Ubuntu Clang 18 (patch updates allowed), same system libstdc++, C++17, UBSan; host reference profile | Executed Debug/Release with the same cases; host reference only |
| STM32N6570-DK | `stm32-gcc-14.3.1-cxx17`; exact ST GCC 14.3.1, Cortex-M55 hard float, newlib-nano; FreeRTOS 11.2.0 from X-CUBE-FREERTOS 1.6.0 | Executed Debug/Release by the integrating application: final image provenance, shared cases, sustained resource/stack/timing checks, failures/resets and real tick wrap. Board tooling/evidence remain application-owned |
| Standalone M55 compile fixture | Same exact embedded compiler; externally supplied kernel/port headers | Build-only: public API and all five providers compile. No scheduler or linked-image claim from this fixture |
| QEMU vexpress-a9, one Cortex-A9 | `qemu-a9-gcc-14.3.1-cxx17`; same compiler distribution, separate `thumb/v7-a+fp/hard` multilib; bundled pinned 10.4.3 kernel/ARM_CA9 port; QEMU 8.2.2 Ubuntu package profile | Executed Debug/Release, normal/near-wrap tick start: scheduler cases, lifecycle regression, startup/clocks/notification isolation, repeated recovery and isolated fatal runs; final provider checks |
| Additional GCC release, embedded Clang, libc++, RISC-V, SMP | No qualified runtime/kernel/port selection | Unsupported; historical source is not build or execution evidence |
| C++20 concurrency, future/async, shared_mutex, atomic wait/unsupported-width fallback | Require separate implementation/runtime qualification | Deferred; four explicit C++17 suite skips, no passing support claim |

The shared C++17 suite requires **11 passes and 4 approved skips**. It covers
thread ownership, moved callable/arguments and IDs/hashes; mutex contention,
recursive/timed locks; predicate notification/timeout races, explicit predicate
rechecks, condition_variable_any; call_once, local statics and exit notification.
Exit-notification resources stay alive through a join; detached lifetimes are
covered separately by lifecycle cases. The FreeRTOS regression requires one
identity/TCB-reuse pass. Each arithmetic run requires two passes. The A9 platform
suite requires four more passes, with 64 shared stress rounds after warm-up and
64 allocation/thread recovery cycles, no permitted resource drift, at least 128
free stack words, bounded waits and a real wrap variant. The timer observations
are scheduler/virtual-machine gates, not physical-board timing guarantees.

No exceptions or RTTI are enabled for either embedded profile. The A9 image
explicitly supplies recoverable nothrow/aligned allocation and terminating
throwing-new, joinable-destructor, kernel-OOM and recursive-guard policy. Failure
policy and its providers remain application/platform responsibilities.

Language thread_local, additional gthread keys, ISR thread operations, external
deletion of managed tasks, and reuse of reserved TLS slot 0 or notification index
0 remain outside the contract. Run thread services after scheduler startup and
allow idle reclamation. Startup is single threaded; the A9 startup probe checks
allocation/static initialization, not concurrent startup operations.

`tools/run_matrix.py` verifies the exact prepared host/toolchain/QEMU inputs and
writes commands, Git commit/dirty state, compiler versions, JUnit output,
per-image reports and final ELF hashes. Routine runs are offline. Other QEMU
versions, multilibs or host package revisions require a reviewed profile update
and new execution evidence. Host std::thread and Clang use the host runtime;
neither qualifies the FreeRTOS backend or embedded Clang.

The hosted [CI workflow](ci.md) uses these same profiles and commands, including
explicit package setup. Server-side
merge enforcement remains pending by the owner's explicit choice.
