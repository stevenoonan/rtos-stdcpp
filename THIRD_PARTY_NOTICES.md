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

The [component index](LICENSES/components.json) records scope. File notices take
precedence over the project MIT default; the repository is not uniformly MIT.

The two selected runtime files are `mutex.cc` and `condition_variable.cc`, from
ST source commit `b8cdff71b827478c8510c70b3a15d2b50c759255`, without local changes.
Their [source manifest](third_party/libstdcxx/stm32-gcc-14.3.1-cxx17/SOURCES.json)
contains immutable URLs, checksums and licenses. Both embedded profiles use this
selection.

The bundled kernel's [manifest](third_party/freertos-kernel/SOURCES.json) identifies
its source snapshot. The exact upstream kernel commit and CMSIS package tag are
not established; version labels alone do not identify those revisions.

Applications supply and document their own kernel/port, board code and runtime
policy. See [licensing](docs/licensing.md) for dependency updates.
