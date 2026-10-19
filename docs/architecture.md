# Layout and ownership

| Location | Responsibility |
|---|---|
| `include/rtos_stdcpp` | Application extensions; existing C++ symbols retained |
| `src/backends/freertos` | Task lifetime, native synchronization, TLS keys and clock implementation |
| `src/toolchains/gcc` | GCC hook-header override and common C++ ABI configuration |
| `cmake/profiles` | Exact audited compiler, headers, multilib and runtime selection |
| `third_party/libstdcxx/stm32-gcc-14.3.1-cxx17` | Sole authoritative selection of two unmodified GCC runtime sources, their licenses and manifest |
| `tests/unit` | Standalone host arithmetic oracle and minimal type-only kernel stubs |
| `tests/regression` | Reliable scheduled-task lifecycle regression shared with integrations |
| `tests/conformance/legacy` | Inherited cases retained for Step 5 runner consolidation; not a qualified suite |
| `tests/support` | Legacy runners, helpers and explicitly unselected test allocation/syscall support |
| `platforms` | Inherited A9 and RISC-V startup, console and configuration material; no active emulator build |
| `third_party/freertos-kernel` | Unmodified inherited 10.4.3 dependency snapshot; source pin/hashes in SOURCES.json |

The old `FreeRTOS/cpp11_gcc`, `test`, `sys_common` and `lib_test_*` trees are
replaced by these responsibilities. The inherited kernel is not silently
upgraded or selected by the maintained build. It remains pinned to the original
import until a standalone platform and replacement dependency are qualified.
No exact upstream kernel commit is inferred from its version label.

The primary CMake target is an object library to preserve explicit provider
linking. It owns three backend translation units and the selected GCC mutex/CV
implementations. An independent configuration interface carries native-type and
newlib ABI requirements without pulling objects back into kernel targets.
Applications retain kernel/port/heap ownership and runtime memory/failure policy.
There is no general backend framework, automatic kernel fetch, or default demo.

Legacy syscall stubs and allocation wrappers have been split by responsibility.
Legacy new/delete overrides remain separate and optional by explicit source
selection only. None is linked by the maintained target or qualified application.
They are historical test policies, not supported production allocation contracts.
Inventory records preserve all inherited notices and the original file provenance.
