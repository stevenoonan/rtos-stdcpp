# Release notes

## 0.1.0 C++17 baseline — 2026-10-19

rtos-stdcpp is maintained independently from Piotr Grygorczuk's FreeRTOS_cpp11.
The first maintained baseline requires C++17 and preserves original attribution,
component notices and history. C++20 concurrency remains a separate milestone.

### Integration and layout

- Reusable CMake object target `rtos-stdcpp::rtos-stdcpp` delivers the three
  backend units and two matching GCC runtime sources exactly once.
  `rtos-stdcpp::config` carries object-free native ABI/include requirements.
- Public extensions use `<rtos_stdcpp/...>`; existing flat includes and
  `free_rtos_std` / `SetSystemClockTime` symbols remain available.
- Backend, GCC hooks, shared tests, platform runners and pinned dependencies
  have separate ownership. Applications supply their kernel/port and runtime
  policy; the bundled kernel is selected only by the standalone A9 tests.
- Obsolete GCC runtime snapshots and K64F/LM3S811 sample builds are retired.
  Inherited RISC-V and C++20 material is explicitly unqualified/historical.

### Behavior and qualification scope

Managed threads use shared completion state and canonical one-word IDs. Callable
ownership, moves, join/detach and ID hashing no longer depend on reclaimed TCBs.
CV waiters handle notification/timeout races; monotonic clocks, wall time and
rounded/saturated waits have separate implementations. The A9 tick uses a priority
masked by kernel critical sections; a hardware-counter probe checks that behavior.

The supported profiles are ST GCC 14.3.1 / newlib-nano on Cortex-M55 with an
application-provided qualified kernel, and standalone QEMU vexpress-a9 with its
selected 10.4.3 kernel snapshot. GCC 13 / Clang 18 on Ubuntu 24.04 exercise host
reference/arithmetic behavior with UBSan. Debug/Release host/A9 qualification
contains 52 CTest entries, including actual kernel wrap and isolated fatal paths.
Host execution does not validate the FreeRTOS backend. Physical-board evidence,
startup, resource/stack/timing limits and flash/reset policy belong to each board
integration. See the [support matrix](docs/support-matrix.md) and
[test commands](docs/testing.md) for the exact scope and pass criteria.

### Restrictions and migration

Both embedded profiles disable exceptions/RTTI. Applications own newlib locks,
static guards, allocation bounds, fatal handling, startup and memory/interrupt
policy. Throwing failures terminate; recoverable nothrow allocation requires a
qualified platform provider. Thread operations require running task context.
Language `thread_local`, extra gthread keys, native-task exit callbacks, ISR
operations and external deletion of managed tasks remain unsupported. Preserve
TLS pointer slot 0 and notification index 0; allow idle reclamation.

Future/async, shared mutexes, unsupported atomic fallbacks, C++20 facilities,
embedded Clang/libc++, RISC-V, SMP and other compiler/kernel combinations are not
qualified. Rebuild all affected C++ objects when migrating native types or common
configuration. Use the [migration](docs/migration.md), [integration](docs/integration.md)
and [licensing](docs/licensing.md) guides; the inherited README is historical.

### Delivery and rollback

The source baseline uses annotated tag `v0.1.0-cxx17`. An application records its
accepted library commit/gitlink and its own qualification results. Keep the prior
accepted revision for rollback; tag creation does not update any application's
pin. Source archives include the component licenses/manifests. No board image,
consumer configuration or generated application build is part of the source
release. Creating the local tag/archive does not publish a hosted release.

CI uses read-only GitHub-hosted jobs and a stable aggregate check. Server-side
merge enforcement remains deferred. Project-owned files use Git review, not
per-file hash inventories; downloaded tools and ABI-sensitive dependencies retain
targeted identity checks.
