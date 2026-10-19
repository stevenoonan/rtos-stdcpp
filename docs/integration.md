# Integrating rtos-stdcpp

The maintained embedded profile is `stm32-gcc-14.3.1-cxx17`: the exact GNU Tools
for STM32 release recorded in [the profile lock](../cmake/profiles/stm32-gcc-14.3.1-cxx17.json),
Cortex-M55 hard-float multilib, newlib-nano, C++17, exceptions and RTTI disabled.
Other compilers, architectures, libraries and C++20 features require separate
qualification. Sources and licenses for the two selected libstdc++ implementation
files are delivered in this checkout, with their immutable source manifest.

Define an existing target supplying the selected FreeRTOS kernel, port headers
and application `FreeRTOSConfig.h`, then configure the library:

```cmake
# application_freertos already exists and supplies configuration/include paths.
set(RTOS_STDCPP_PROFILE stm32-gcc-14.3.1-cxx17)
set(RTOS_STDCPP_FREERTOS_TARGET application_freertos)
add_subdirectory(path/to/rtos-stdcpp)
target_link_libraries(application_freertos INTERFACE rtos-stdcpp::config)
target_link_libraries(firmware PRIVATE rtos-stdcpp::rtos-stdcpp application_freertos)
set_target_properties(firmware PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
```

Use `PUBLIC` instead of `INTERFACE` when the supplied target itself compiles the
kernel. Apply `rtos-stdcpp::config` to every other affected target: C++ objects
must agree on the GCC hook headers and native types, and C/newlib users must
agree on the nano reentrancy ABI. Configuration usage requirements deliberately
contain no objects or dependency on the supplied kernel target. Linking the
primary object-library target directly into the final executable supplies all
five providers exactly once; an interface-only link does not deliver objects.
For additional interface wrappers, explicitly forward `$<TARGET_OBJECTS:rtos_stdcpp>`
or keep the direct final link shown above. Do not compile these sources separately.

The library sets target usage requirements, not global compiler/linker flags.
The selected profile includes Cortex-M55 CPU flags, nano C/C++ header selection,
GCC hook precedence, thread-safe static initialization, and disabled exceptions/RTTI.
The application supplies its executable's linker script and matching nano library
link selection (`--specs=nano.specs`), startup, kernel, heap, port, handlers,
newlib locks, C++ guards, bounded arenas and failure policy. The library does not
add kernel sources, allocator overrides, syscall stubs, tests or demos by default.
Do not supply competing stock or custom thread, clock, mutex or CV providers.

Required kernel settings include newlib task reentrancy, at least one TLS slot
(slot 0 reserved for the adapter), task notifications (index 0 reserved), mutexes,
recursive mutexes, dynamic allocation, and a 32-bit tick counter with a frequency
that divides one billion. Enable scheduler-state, delay, delete, suspend and task
priority APIs used by the backend. Run thread operations after scheduler startup;
allow idle-task deletion cleanup. Inspect the backend for the full API surface
when qualifying another configuration. The application owns interrupt priority,
FPU, cache, TrustZone and memory-map choices.

Public extensions are available as `<rtos_stdcpp/thread_with_attributes.h>`,
`<rtos_stdcpp/freertos_thread_attributes.h>` and `<rtos_stdcpp/freertos_time.h>`.
Existing flat header spellings and `free_rtos_std` / `SetSystemClockTime` symbols
are retained during this layout migration. Internal backend and GCC headers
remain visible only because libstdc++ needs them; they are not stable extension APIs.

The reusable [compile example](../examples/compile-check/CMakeLists.txt) needs
only this checkout, the audited compiler, and externally supplied kernel/port
headers. Its configuration is a compile-only fixture, not an executable platform.
Normal configure/build operations verify local hashes and never fetch dependencies.
