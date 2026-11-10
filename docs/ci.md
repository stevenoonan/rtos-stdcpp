# Continuous integration

Pushes, pull requests and manual dispatch run the C++17/C++20 matrix on
GitHub-hosted Ubuntu 24.04:

- GCC 13 and Clang 18, each in Debug/Release with UBSan and ASan+UBSan lanes.
- FreeRTOS and ThreadX on the same QEMU M4/M7/M55 models, each in
  Debug/Release with default/custom TLS allocation and ordinary/near-wrap ticks.
- Supplemental FreeRTOS A9 execution with the same language, configuration,
  allocator and tick variants.
- Standalone FreeRTOS and ThreadX compile/release checks, including optional
  runtime providers and both TLS allocation policies.
- `Required C++ checks`, which requires every host, Cortex-M, A9 and standalone job.

There are 16 host jobs, 24 Cortex-M jobs, four A9 jobs and two standalone jobs.
Each Cortex-M job executes four allocator/tick variants: 96 configurations total,
48 per backend. Artifacts include logs, JUnit and per-image reports after success
or failure, with seven-day retention. Skipped/cancelled jobs fail the aggregate.
Server-side merge enforcement is not configured.

Both backends execute the same shared C++17/C++20 suites, lifecycle regression,
qualification runner and seven fatal scenarios. Qualification covers 64 stress
rounds, clock adjustment and sleep boundaries, startup constructors, interrupt
masking, exact resource recovery, TLS destructors, aligned/nothrow allocation and
live FP registers across preemption. FreeRTOS additionally tests notification
index isolation and its separate kernel-heap OOM hook. ThreadX uses one shared
newlib arena and has no notification-array equivalent. See the
[parity contract](testing.md#backend-parity).

The fixtures share image/test registration in
[cmake/qemu-tests.cmake](../cmake/qemu-tests.cmake), test implementations under
[tests/qualification](../tests/qualification), and the runtime audit in
[tools/check_qemu.py](../tools/check_qemu.py). Matrix drivers share input
verification and evidence capture through
[tools/matrix_support.py](../tools/matrix_support.py). Emulator jobs use the same
[setup action](../.github/actions/setup-qemu/action.yml) for compiler/QEMU cache,
checksums and runtime packages. Host negative controls check the orchestration's
failure/timeout handling and rejection of incorrect inputs. ASan lanes enable
leak detection and require an armed use-after-free diagnostic.

Actions and kernel inputs are pinned, permissions are read-only and checkout
credentials are not persisted. Cortex-M uses official FreeRTOS 11.2.0 and ThreadX
6.4.1 kernels; A9 retains its bundled FreeRTOS 10.4.3 port. Pins live in
[embedded kernel inputs](../cmake/profiles/embedded-kernel-inputs.json). Host jobs
have 15-minute deadlines; emulator and standalone jobs have 20-minute deadlines.
Setup downloads and verifies dependencies explicitly; builds/tests run offline.

Physical-board checks belong to the integrating application and need exclusive
board/probe access. Untrusted PRs must not run on that machine. Emulation does
not qualify physical interrupt latency, cache behavior or STM32 peripherals.
M55 uses secure single mode; TrustZone transitions and MVE remain outside scope.
The supplemental A9 fixture is FreeRTOS-only: the existing ThreadX integration
and optional runtime support Cortex-M, and adding an A-profile backend/runtime
is outside this matrix. This does not reduce Cortex-M backend parity.
