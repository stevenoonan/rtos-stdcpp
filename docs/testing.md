# Current local checks

From this repository, with CMake 3.22+, Ninja and GNU host C++ with UBSan:

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug --no-tests=error
cmake --preset host-release
cmake --build --preset host-release
ctest --preset host-release --no-tests=error
```

Presets put generated files in `../build/rtos-stdcpp/`, outside the source
checkout, so source and license inventories stay free of build artifacts.

Each configuration runs two C++17 arithmetic tests, at 100 and 1000 Hz. Checks
abort on failure even in Release, use UBSan and have a CTest timeout. Stubs are
private to these host targets; they must never reach embedded compilation.

To check the embedded target independently, put the exact audited toolchain's
`bin` directory on PATH and supply local dependency paths:

```sh
cmake -S examples/compile-check -B ../build/rtos-stdcpp/compile-check -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-none-eabi.cmake" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DFREERTOS_KERNEL_ROOT=/absolute/path/to/kernel \
  -DFREERTOS_PORT_INCLUDE=/absolute/path/to/kernel/portable/GCC/ARM_CM55_NTZ/non_secure
cmake --build ../build/rtos-stdcpp/compile-check
```

The compile fixture has no startup, linked kernel or runtime execution. The
application integration supplies those and verifies its final link map: exactly
one kernel/heap/port/handler set, explicit adapter and matching mutex/CV providers,
and no stock single-thread clock/guard/thread/newlib-lock providers. Hardware
regressions and platform health checks remain application-owned.

The moved lifecycle regression retains its explicit result bitmask. Inherited
header cases and legacy runners still contain print-only checks and unsupported
future/C++20 cases; they are not enabled or counted as passing qualification.
Step 5 will provide a shared reliable runner; Step 6 will expand the matrix and
qualify an emulator. QEMU execution is not part of the current Step 4 build.
