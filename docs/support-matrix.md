# Current support boundary

| Layer | Compiler / mode | Dependency / platform | Status |
|---|---|---|---|
| Embedded integration | Exact ST GCC 14.3.1 profile, explicit C++17, no exceptions/RTTI | FreeRTOS 11.2.0 from X-CUBE-FREERTOS 1.6.0; Cortex-M55, newlib-nano, hard float | Existing application qualification baseline; application owns board execution evidence |
| Standalone host arithmetic | GNU 13.3.0, explicit C++17, UBSan, Debug and Release | Type-only kernel stubs, 100/1000 Hz | Deadline rounding/saturation and tick extension only; no native-thread runtime claim |
| Host reference/harness | GNU 13.3.0, explicit C++17, UBSan, Debug and Release | Native host threads and synthetic process faults | Shared case reference run and strict runner qualification; no embedded scheduling/attributes claim |
| Standalone embedded compile | Exact ST GCC 14.3.1 profile, explicit C++17 | Externally supplied 11.2.0 Cortex-M55 kernel/port headers; generic compile fixture | Public API and all five providers compile; no linked/running platform claim |
| A9 emulator | Not selected | Inherited A9 platform and 10.4.3 dependency | Unqualified; no active build |
| RISC-V emulator | Not selected | Inherited platform/dependency | Historical source only; no active build |
| Other compilers, libc++, C++20 | Not selected | Requires matching runtime/dependencies | Unsupported until separately qualified |

Shared cases and the named lifecycle regression also execute through the existing
embedded integration, with the common strict report validator and application
health checks. Deferred features are explicitly skipped.

This is the Step 5 support boundary, not the expanded Step 6 matrix. A host pass
does not qualify embedded synchronization. CMake rejects other embedded profile
names and nonmatching compiler/header/library bytes; a newer GCC version is not
assigned to old snapshots. No GCC v10/v11/v13 runtime copies remain.
