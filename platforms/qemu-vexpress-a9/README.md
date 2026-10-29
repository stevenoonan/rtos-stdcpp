# Standalone A9 test platform

The selected image runs the shared C++17 and C++20 cases against the actual FreeRTOS adapter on
QEMU `vexpress-a9`, one Cortex-A9 CPU, 128 MiB RAM. Debug and Release, including
an accelerated real kernel tick-wrap configuration, are tested. See
[testing](../../docs/testing.md) for setup and commands and the
[support matrix](../../docs/support-matrix.md) for the exact scope.

The selected ST GCC 14.3.1 distribution supplies the separate A9
`thumb/v7-a+fp/hard` multilib. A9 code uses ARM instructions, VFPv3 and hard float.
CMake selects nano headers and libraries.
Set `RTOS_STDCPP_CXX_STANDARD=17` (default) or `20` at configure time. The GCC
mutex, condition-variable and future sources are shared with the Cortex-M55
profiles.

The tests use the bundled FreeRTOS 10.4.3 kernel. No dependency is fetched by
configure/build/test.

This platform owns its startup, linker script, allocator arenas, failure policy,
newlib scheduler locks, static guards and semihost exit. These are optional test
platform choices, not providers injected into applications by the library target.
The kernel heap is 512 KiB; the bounded C++ arena is 256 KiB. Main has 4096 stack
words, default workers 512, C++20 test workers 1024, and the stack workload 1536. Every observed worker
and main stack must retain at least 128 words. Resource gates require exact
recovery after idle opportunities; this is a single-core profile.

`-DRTOS_STDCPP_TLS_CUSTOM_ALLOCATOR=ON` builds the public allocation hooks from
`tls_allocator.cpp`. This test provider uses the C++/newlib arena with tagged
allocations and counts live blocks. Qualification requires actual hook usage,
paired deallocation and zero live TLS blocks after worker cleanup. The default
matrix executes both allocator configurations; the ordinary platform default
continues to use the FreeRTOS heap.

UART0 uses the PL011 flag register at offset 0x18 and TXFF bit 5. The selected
[QEMU PL011 model](https://github.com/qemu/qemu/blob/v8.2.2/hw/char/pl011.c)
and [board map](https://github.com/qemu/qemu/blob/v8.2.2/hw/arm/vexpress.c)
are the platform references. The
[global timer model](https://github.com/qemu/qemu/blob/v8.2.2/hw/timer/a9gtimer.c)
uses a 10 ns counter period with prescaler zero, so the platform programs 100 MHz
and a 1000 Hz RTOS tick. The tick PPI is explicitly assigned the port's lowest
usable priority (logical 30, GIC byte 0xf0), below the API-call threshold
(logical 18, 0x90). The startup qualification verifies that the tick stays masked across three
hardware timer periods inside a critical section. All tasks preserve a full FPU context. The pinned port
omits the memset declaration in that configuration; its C translation unit
force-includes string.h, preserving the dependency source bytes.

Reset sets CPU modes/stacks and branches to an ordinary C++ function before
performing runtime initialization. This keeps runtime initialization in an ordinary C++ context.
SYS_EXIT_EXTENDED carries the process status. A missing exit, exception loop,
crash, malformed report or missing required case fails the outer deadline/gate.
Expected failures require both the pre-fault marker and the exact exit status;
each runs in a fresh emulator process.

For interactive debugging, use the same machine options as tools/run_qemu.py,
add `-s -S`, and attach arm-none-eabi-gdb to localhost:1234. Keep manual debug
sessions separate from automated deadline-based evidence.
