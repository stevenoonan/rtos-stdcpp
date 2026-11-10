// SPDX-License-Identifier: MIT
#include "console.h"
#include "platform.h"
#include "runner.h"
#include "qualification_platform.h"
#include <rtos_stdcpp/runtime.h>
#include "thread_gthread.h"
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

namespace {
uintptr_t semihost(uintptr_t operation, const void* argument) {
    register uintptr_t result asm("r0") = operation;
    register const void* data asm("r1") = argument;
    asm volatile("bkpt 0xab" : "+r"(result) : "r"(data) : "memory");
    return result;
}

volatile uint32_t& reg(uintptr_t address) {
    return *reinterpret_cast<volatile uint32_t*>(address);
}

alignas(8) unsigned char heap[2 * 1024 * 1024];
size_t heap_used;
} // namespace

void print(const char* text) {
    semihost(4, text);
}

void print(unsigned number) {
    char text[9];
    for (unsigned i = 0; i < 8; ++i) {
        text[i] = "0123456789abcdef"[(number >> ((7 - i) * 4)) & 15];
    }
    text[8] = 0;
    print(text);
}

extern "C" [[noreturn]] void _exit(int status) {
    const uintptr_t arguments[]{0x20026, static_cast<uintptr_t>(status)};
    semihost(0x20, arguments);
    for (;;) {
        asm volatile("wfi");
    }
}

extern "C" [[noreturn]] void abort() noexcept {
    _exit(89);
}

extern "C" [[noreturn]] void platform_fault(const uint32_t* stack) {
    print("RTOS_STDCPP_FAULT pc=");
    print(stack[6]);
    print(" cfsr=");
    print(reg(0xe000ed28));
    print("\n");
    _exit(88);
}

extern "C" [[noreturn]] void rtos_stdcpp_runtime_fail(const char* expression, const char* file,
                                                      unsigned line) {
    print("RTOS_STDCPP_RUNTIME_FAILURE ");
    print(expression);
    print(" ");
    print(file);
    print(":");
    print(line);
    print("\n");
    _exit(86);
}

extern "C" void* _sbrk(ptrdiff_t amount) {
    if (amount < 0 || static_cast<size_t>(amount) > sizeof(heap) - heap_used) {
        errno = ENOMEM;
        return reinterpret_cast<void*>(-1);
    }
    void* result = heap + heap_used;
    heap_used += amount;
    return result;
}

extern "C" int _write(int, const char* buffer, int length) {
    for (int i = 0; i < length; ++i) {
        char byte[]{buffer[i], 0};
        print(byte);
    }
    return length;
}

extern "C" int _read(int, char*, int) {
    errno = ENOSYS;
    return -1;
}

extern "C" int _close(int) {
    return -1;
}

extern "C" int _fstat(int, struct stat* value) {
    value->st_mode = S_IFCHR;
    return 0;
}

extern "C" int _isatty(int) {
    return 1;
}

extern "C" int _lseek(int, int, int) {
    return 0;
}

extern "C" int _getpid() {
    return 1;
}

extern "C" int _kill(int, int) {
    errno = EINVAL;
    return -1;
}

extern "C" void _init() {}

extern "C" void _fini() {}

void* __dso_handle = &__dso_handle;
extern "C" void __libc_init_array();
extern "C" unsigned char _sidata[], _sdata[], _edata[], _sbss[], _ebss[], _estack[];
[[noreturn]] void start_scheduler();

extern "C" [[noreturn]] void platform_start() {
    for (auto *out = _sdata, *in = _sidata; out < _edata;) {
        *out++ = *in++;
    }
    for (auto* out = _sbss; out < _ebss;) {
        *out++ = 0;
    }
    reg(0xe000ed88) |= 0x00f00000;
    asm volatile("dsb\nisb" ::: "memory");
    __libc_init_array();
    start_scheduler();
}

extern "C" {
volatile uint32_t rtos_test_tick_count;
volatile unsigned rtos_test_idle_count;
}

namespace rtos_test::platform {
[[noreturn]] void fatal(const char* file, unsigned line) {
    print(file);
    print(":");
    print(line);
    print("\n");
    std::_Exit(1);
}
} // namespace rtos_test::platform

namespace rtos_test::qualification {
bool masked_tick_probe() {
    rtos_stdcpp::critical_section guard;
#ifdef RTOS_STDCPP_BACKEND_THREADX
    const auto before = tx_time_get();
#else
    const auto before = xTaskGetTickCount();
#endif
    // COUNTFLAG is set by the hardware counter even while delivery of the
    // SysTick exception is masked. Reading CTRL clears only COUNTFLAG.
    (void)reg(0xe000e010);
    unsigned wraps = 0;
    while (wraps < 3) {
        if (reg(0xe000e010) & (1U << 16)) {
            ++wraps;
        }
    }
#ifdef RTOS_STDCPP_BACKEND_THREADX
    return tx_time_get() == before;
#else
    return xTaskGetTickCount() == before;
#endif
}
} // namespace rtos_test::qualification
