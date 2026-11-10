# FreeRTOS and ThreadX Cortex-M QEMU fixture

This test platform runs both backends with their actual pinned kernels and GNU
ports. It links the adapter and optional compiler/newlib runtime into resettable
images without STM32 HAL or application dependencies.

| Core | QEMU machine | FreeRTOS port | ThreadX port | SysTick clock |
| --- | --- | --- | --- | --- |
| M4 | `mps2-an386` | `ARM_CM4F` | `cortex_m4/gnu` | 25 MHz |
| M7 | `mps2-an500` | `ARM_CM7/r0p1` | `cortex_m7/gnu` | 25 MHz |
| M55 | `mps3-an547` | `ARM_CM55_NTZ/non_secure` | `cortex_m55/gnu` | 32 MHz |

All select hard float and 1 kHz ticks. M55 runs secure-only with TrustZone
transitions, MVE, MPU, PAC and BTI disabled. FreeRTOS 11.2.0 and ThreadX 6.4.1
are pinned in [kernel inputs](../../cmake/profiles/embedded-kernel-inputs.json).

Both share startup/data initialization, semihost transport, linker layout,
qualification cases, failure programs, image registration and runtime audits.
Backend files supply scheduler/bootstrap setup, exception vectors and kernel
hooks. A native bootstrap starts managed C++ main. ThreadX uses execution-change
hooks and a TCB extension; FreeRTOS uses its newlib integration and TLS slot 0.
Kernel, adapter, runtime and application consistently use newlib-nano.

The bounded C++ arena is 2 MiB. FreeRTOS has an additional 1 MiB kernel heap.
ThreadX adapter allocations use the C++ arena. M4/M7 data RAM starts at
`0x20000000`; M55 uses `0x21000000`. The linker reserves 16 KiB for MSP and
rejects native ELF TLS sections. No network backend is attached.

[Testing commands](../../docs/testing.md#cortex-m-execution) run all 96 variants
or select a backend/core/language/configuration/allocator/tick subset.
Individual CMake builds require the matching toolchain file,
`RTOS_STDCPP_TOOLCHAIN_ROOT`, `RTOS_STDCPP_CORTEX_M`, `RTOS_STDCPP_BACKEND`,
`RTOS_STDCPP_QEMU` and the selected `THREADX_KERNEL_ROOT` or
`FREERTOS_KERNEL_ROOT`. Configuration never downloads dependencies.

The [parity contract](../../docs/testing.md#backend-parity) defines common and
backend-specific coverage. The shared runner performs 64 stress rounds, detailed
clock/sleep checks, startup and interrupt masking checks, exact resource recovery
and FP register preservation across concurrent priority tiers. Seven separate
fatal images require armed failures. FreeRTOS adds notification-index isolation
and separate kernel-heap OOM coverage. Custom TLS hooks use tagged paired
allocations and verify recovery; observed stacks require 128 unused words.

Qualification uses 90/100-second emulator/CTest deadlines; other images use
25/35 seconds. Crashes, timeouts, malformed/missing reports, unapproved skips
and unexpected survival fail. These Arm MPS models do not qualify STM32
peripherals, physical interrupt latency or cache behavior. The
[A9 fixture](../qemu-vexpress-a9/README.md) remains supplemental FreeRTOS coverage.
