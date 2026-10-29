# Standalone embedded compile examples

This project configures either backend with explicit vendor kernel headers and
no parent firmware project. It builds OBJECT targets for the adapter, common
public API probe and optional compiler runtime. ThreadX also compiles
[application.cpp](threadx/application.cpp), which initializes the runtime and
adapter, creates a native bootstrap thread, and joins a managed C++ worker.

Put your `arm-none-eabi` toolchain on PATH. These examples are tested with ST
GCC 14.3.1. FreeRTOS requires its source root and matching
Cortex-M55 port include directory:

```sh
cmake -S examples/compile-check -B /tmp/rtos-freertos -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-none-eabi.cmake" \
  -DRTOS_STDCPP_BACKEND=FreeRTOS \
  -DFREERTOS_KERNEL_ROOT=/absolute/path/to/freertos \
  -DFREERTOS_PORT_INCLUDE=/absolute/path/to/freertos/portable/GCC/ARM_CM55_NTZ/non_secure
cmake --build /tmp/rtos-freertos
ctest --test-dir /tmp/rtos-freertos --output-on-failure --no-tests=error
```

ThreadX requires a tree containing `common/inc` and `ports/cortex_m55/gnu/inc`:

```sh
cmake -S examples/compile-check -B /tmp/rtos-threadx -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-none-eabi.cmake" \
  -DRTOS_STDCPP_BACKEND=ThreadX -DTHREADX_ROOT=/absolute/path/to/threadx
cmake --build /tmp/rtos-threadx
ctest --test-dir /tmp/rtos-threadx --output-on-failure --no-tests=error
```

Commands run from the library root. Both default to C++17; select
`-DRTOS_STDCPP_PROFILE=stm32-gcc-14.3.1-cxx20` for C++20. Select Debug or Release
with `-DCMAKE_BUILD_TYPE`. Inputs are never fetched by CMake.

The native configuration headers are compile fixtures. To turn the ThreadX
bootstrap into a board application, provide the matching kernel/port objects,
startup, vectors, timer hook, linker layout, heap/syscalls/failure handlers and
`_tx_initialize_low_level`, then enter through `example_start()`. The example's
`tx_application_define()` owns adapter/runtime initialization. A final image must
link the adapter and compiler runtime OBJECT providers directly.

Compilation does not execute a scheduler or establish board qualification.
[Shared integration](../../docs/integration.md) and the
[FreeRTOS](../../docs/backends/freertos.md) /
[ThreadX](../../docs/backends/threadx.md) guides define the full consumer contract.
The [release checker](../../docs/testing.md#release-checks) compiles both examples
in isolated copies and verifies their selected providers and headers.
