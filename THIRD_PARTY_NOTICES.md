# Component notices

rtos-stdcpp is independently maintained from Piotr Grygorczuk's
[FreeRTOS_cpp11](https://github.com/grygorek/FreeRTOS_cpp11). Preserve copyright,
permission and license notices in distributed files.

| Component | Location | Terms |
|---|---|---|
| Adapter, extensions, tests and project tooling | `include`, `src`, `tests`, `tools`, build files and project-authored platform code | [MIT](LICENSE); retain per-file notices and the [original project notice](LICENSES/FreeRTOS_cpp11-original.txt) |
| FreeRTOS 10.4.3 kernel | `third_party/freertos-kernel` | [MIT](third_party/freertos-kernel/LICENSE), with Amazon and other file notices |
| Arm CMSIS and Cortex-A9 startup/system support | `platforms/qemu-vexpress-a9` | [Apache-2.0](LICENSES/Apache-2.0.txt) where identified by file notices |
| Selected ST GCC runtime sources | `third_party/libstdcxx/stm32-gcc-14.3.1-cxx17` | [GPLv3](LICENSES/GPL-3.0-or-later.txt) with the [GCC Runtime Library Exception](LICENSES/GCC-exception-3.1.txt) |

File notices take precedence over the project MIT default.

## GCC runtime sources

The bundled `mutex.cc`, `condition_variable.cc` and `future.cc` come from
[`src/gcc/libstdc++-v3/src/c++11` in GNU Tools for STM32 14.3.rel1](https://github.com/STMicroelectronics/gnu-tools-for-stm32/tree/b8cdff71b827478c8510c70b3a15d2b50c759255/src/gcc/libstdc++-v3/src/c++11),
commit `b8cdff71b827478c8510c70b3a15d2b50c759255`, without local changes.
Both C++17 and C++20 profiles use these files. The accompanying `COPYING3` and
`COPYING.RUNTIME` texts come from the same revision's `src/gcc` directory.

The C++20 [`stop_token` header](third_party/libstdcxx/stm32-gcc-14.3.1-cxx20/stop_token)
comes from the same distribution's
[`src/gcc/libstdc++-v3/include/std/stop_token`](https://github.com/STMicroelectronics/gnu-tools-for-stm32/blob/b8cdff71b827478c8510c70b3a15d2b50c759255/src/gcc/libstdc++-v3/include/std/stop_token).
Local changes select `vTaskDelay(1)` for FreeRTOS or `tx_thread_sleep(1)` for
ThreadX during stop-state contention, and add optional `RTOS_STDCPP_TEST_STOP_LOCK`
callbacks for acquisition/contention tests. These sources use GPLv3 with the GCC
Runtime Library Exception.

## FreeRTOS test kernel

The FreeRTOS 10.4.3 kernel bundled for the A9 tests comes from
[`FreeRTOS/Source` in FreeRTOS_cpp11](https://github.com/stevenoonan/FreeRTOS_cpp11/tree/0dc4317a96fdbd5c8fa5211c9a387f6f0fe069ba/FreeRTOS/Source),
commit `0dc4317a96fdbd5c8fa5211c9a387f6f0fe069ba`. It retains its MIT license and
per-file copyright notices.

Applications supply their own kernel/port, board code and runtime policy.
See [licensing](docs/licensing.md) for component licenses.
