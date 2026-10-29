# Optional Cortex-M compiler runtime

Start with [shared integration](integration.md) and select
[FreeRTOS](backends/freertos.md#allocation-and-compiler-runtime) or
[ThreadX](backends/threadx.md#allocation-and-compiler-runtime) for native runtime
requirements. This page owns the optional provider interface and shared hooks.

`cmake/runtime.cmake` has no board or HAL dependency. It supplies
newlib allocation locks, blocking C++ static guards and nothrow allocation
helpers for the embedded GCC/newlib profiles. It does not reserve a heap or
implement `_sbrk`, `abort`, kernel entry or board fault capture. Its interrupt
check currently requires an ARM M profile; the standalone A9 platform supplies
its own runtime instead.

## Provider selection

```cmake
include(path/to/rtos-stdcpp/cmake/runtime.cmake)
rtos_stdcpp_add_runtime(compiler_runtime BACKEND "${RTOS_STDCPP_BACKEND}"
    CONFIG_TARGET firmware_configuration)
target_link_libraries(application PRIVATE compiler_runtime)
```

| Argument | Contract |
| --- | --- |
| `BACKEND` | `FreeRTOS`, `ThreadX` or `NONE`; native selection must match the application's kernel |
| `CONFIG_TARGET` | Existing target exporting native configuration/headers, newlib headers and the same compiler ABI options as `rtos-stdcpp::config` |
| `PROVIDER_TARGET` | Required existing consumer provider when `BACKEND NONE` is selected |

Link the OBJECT target directly to the executable so weak compiler/libc hooks
cannot prevent overrides from being included. Call
`rtos_stdcpp_runtime_initialize()` before starting managed workers. Native locking,
newlib context switching and cleanup requirements live in the two backend guides:

- [FreeRTOS runtime requirements](backends/freertos.md#allocation-and-compiler-runtime).
- [ThreadX runtime requirements](backends/threadx.md#allocation-and-compiler-runtime).

## Custom runtime provider

`BACKEND NONE PROVIDER_TARGET custom_runtime` compiles the common compiler
helpers and requires the consumer target to supply initialization, allocation
lock/unlock and guard lock/unlock functions from `<rtos_stdcpp/runtime.h>`.
Guard locks must allow scheduling and recursive ownership. Allocation locks must
serialize newlib operations and support nesting. No native implementation is
selected, and missing providers fail configuration. `NONE` applies only to this
helper module; the primary threading library supports FreeRTOS or ThreadX.

## Failure and measurement hooks

The weak `rtos_stdcpp_runtime_fail(message, file, line)` handler calls `abort()`.
Override it when `abort()` itself uses runtime facilities. It must never return.
The optional weak cycle hook returns zero by default; a consumer can read its
cycle counter to populate `rtos_stdcpp_newlib_max_lock_cycles` measurements.
Both hooks are supplied through the consumer runtime interface.

## Linker checks

The optional `src/runtime/runtime_assertions.ld` supplemental script rejects
native ELF TLS and runtime data overlapping the consumer's `_sstack` MSP
boundary. The consumer owns the memory map and must provide those sections and
symbols when using this script.
