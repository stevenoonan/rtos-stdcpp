# Migrating from FreeRTOS_cpp11

This fork is now maintained independently as `rtos-stdcpp`, with C++17 as the
minimum. Obtain authorized read access to the private
`https://github.com/stevenoonan/rtos-stdcpp.git` repository. Record the exact
accepted revision in the integrating application; `rtos-stdcpp` is the maintenance
branch, not an instruction to update dependencies automatically.

## Checkout, build and header changes

| Historical location or use | Current interface |
|---|---|
| `third_party/FreeRTOS_cpp11` application checkout | Rename to an application-selected path such as `third_party/rtos-stdcpp`; update `.gitmodules`, gitlink and owned build references together |
| `FreeRTOS/cpp11_gcc` | Backend in `src/backends/freertos`, GCC overrides in `src/toolchains/gcc`, public extensions in `include/rtos_stdcpp` |
| Hand-listed adapter/runtime objects or inherited sample CMake | `add_subdirectory` plus direct `rtos-stdcpp::rtos-stdcpp` linkage and shared `rtos-stdcpp::config` |
| `libstdc++_gcc/v10`, `v11`, `v13` | Exact selected GCC runtime in `third_party/libstdcxx/stm32-gcc-14.3.1-cxx17`; no nearest-version fallback |
| `test`, `tests`, `lib_test_*` | Shared cases in `tests`; platform-specific startup/runners in `platforms` |
| `sys_common` | Unselected legacy allocation/syscall test helpers under `tests/support`; production policy belongs to the application |
| Flat extension includes | Prefer `<rtos_stdcpp/thread_with_attributes.h>`, `<rtos_stdcpp/freertos_thread_attributes.h>`, `<rtos_stdcpp/freertos_time.h>` |

Existing `free_rtos_std`, `SetSystemClockTime` and flat extension include spellings
are retained. Internal backend/GCC headers are not a stable application API.
The attribute helper's stack sizes are words, not bytes. Inherited C++20 symbols
or experimental sources do not establish C++20 support.

The `-Dk64frdmevk` / `-Darmca9` sample selection interface, K64F and LM3S811
samples and inherited compiler workflows are retired. The maintained standalone
A9 entry point is `platforms/qemu-vexpress-a9`; RISC-V source is historical and
unsupported. Use [testing](testing.md), not commands from the inherited README.

## Required rebuild and review

Rebuild every affected C++ object after changing native types, GCC hooks or common
configuration. Canonical thread IDs now occupy one `uintptr_t` word; CV/clock
native interfaces and GCC feature settings also differ from earlier revisions.
Do not link objects built against stock or historical gthread headers with this
adapter. Clean separate build directories are preferable to reusing old caches.

Confirm exactly one kernel/port/heap and the intended thread/clock/mutex/CV
providers in the final image. Remove manual copies of selected runtime sources.
Apply the same nano newlib ABI settings to affected C and C++ targets and the
matching libraries at link time. Keep startup, newlib locks, guards, allocation
bounds and fatal policy under application ownership. See [integration](integration.md).

Preserve per-file notices and immutable provenance when moving sources. The
component license index replaces the former every-file inventory; updating a
README no longer requires changing source hashes. Retained dependencies still
have their targeted manifests. See [licensing](licensing.md).

Run host checks, the standalone compile fixture, qualified A9 execution and the
application's firmware/hardware acceptance before accepting a migrated baseline.
A neutral checkout can run the host and A9 suites without any application tree.
The final release-baseline milestone additionally repeats full physical-board
qualification and fresh application checkout/regeneration checks.
