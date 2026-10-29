#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check that standalone A9 images link the expected runtime providers."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode = True

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path)
    parser.add_argument('--toolchain', type=Path)
    args = parser.parse_args()
    if not args.build:
        return
    commands = json.loads((args.build / 'compile_commands.json').read_text())
    cache = (args.build / 'CMakeCache.txt').read_text()
    standard = 20 if 'RTOS_STDCPP_CXX_STANDARD:STRING=20' in cache else 17
    files = [Path(x['file']).name for x in commands]
    for name in ['tasks.c', 'queue.c', 'list.c', 'event_groups.c', 'heap_4.c', 'port.c', 'portASM.S', 'thread.cpp', 'gthr_key.cpp', 'freertos_time.cpp', 'tls.cpp', 'mutex.cc', 'condition_variable.cc', 'future.cc']:
        if files.count(name) != 1:
            raise RuntimeError('Provider compiled multiple times or missing: ' + name)
    tls_command = next(x['command'] for x in commands if Path(x['file']).name == 'tls.cpp')
    custom_tls = '-DRTOS_STDCPP_TLS_CUSTOM_ALLOCATOR=1' in tls_command
    tls_object = next(args.build.glob('**/rtos_stdcpp.dir/src/toolchains/gcc/tls.cpp.obj'))
    undefined = subprocess.check_output([str(args.toolchain / 'bin/arm-none-eabi-nm'),
                                         '--undefined-only', str(tls_object)], text=True)
    undefined_names = {line.split()[-1] for line in undefined.splitlines() if line.split()}
    hooks = {'rtos_stdcpp_tls_allocate', 'rtos_stdcpp_tls_deallocate'}
    if custom_tls:
        if not hooks <= undefined_names or {'pvPortMalloc', 'vPortFree'} & undefined_names:
            raise RuntimeError('Custom TLS runtime must use only the paired allocation hooks')
    elif hooks & undefined_names:
        raise RuntimeError('Default TLS runtime unexpectedly requires custom allocation hooks')
    for item in commands:
        command = item['command']
        if Path(item['file']).suffix in ['.cpp', '.cc']:
            if f'-std=c++{standard}' not in command or 'gcc_threading_config.h' not in command or '-D_GLIBCXX_HAS_GTHREADS=1' not in command:
                raise RuntimeError('Inconsistent C++ ABI/configuration: ' + item['file'])
        for flag in ['-mcpu=cortex-a9', '-mfpu=vfpv3', '-mfloat-abi=hard', '-marm']:
            if flag not in command:
                raise RuntimeError('Wrong CPU profile')
    images = []
    for elf in sorted(args.build.glob('*.elf')):
        content = elf.with_suffix('.map').read_text()
        archive = content.split('Discarded input sections')[0]
        forbidden = ['thread.o', 'mutex.o', 'condition_variable.o', 'guard.o', 'chrono.o', 'libc_a-lock.o', 'libc_a-mlock.o', 'emutls.o', 'atexit_thread.o']
        for name in forbidden:
            if '(' + name + ')' in archive:
                raise RuntimeError('Stock runtime fallback: ' + name)
        symbols = subprocess.check_output([str(args.toolchain / 'bin/arm-none-eabi-nm'), '--defined-only', str(elf)], text=True).splitlines()
        names = [line.split()[-1] for line in symbols if len(line.split()) >= 3]
        for name in ['vTaskStartScheduler', 'FreeRTOS_IRQ_Handler', 'FreeRTOS_SWI_Handler', 'FreeRTOS_Tick_Handler', '_exit', '__malloc_lock']:
            if names.count(name) != 1:
                raise RuntimeError('Missing or duplicate provider: ' + name)
        # Guard/clock providers can be discarded only in images that never use them.
        if elf.stem in ['conformance', 'qualification']:
            for name in ['__cxa_guard_acquire', '__cxa_guard_release']:
                if names.count(name) != 1:
                    raise RuntimeError('Guard provider missing')
        if elf.stem in ['conformance', 'qualification', 'cxx20']:
            for name in ['__emutls_get_address', '__emutls_register_common', '__cxa_thread_atexit']:
                if names.count(name) != 1:
                    raise RuntimeError('TLS provider missing: ' + name)
            if 'rtos_stdcpp.dir/src/toolchains/gcc/tls.cpp.obj' not in content:
                raise RuntimeError('TLS runtime object missing from link map')
            if custom_tls and any(names.count(hook) != 1 for hook in hooks):
                raise RuntimeError('Custom TLS allocation hook provider missing or duplicated')
        images.append(elf.name)
    if len(images) != (12 if standard == 20 else 11):
        raise RuntimeError('Missing required images')
    report = {'status': 'passed', 'layer': 'compile_link',
              'single_kernel_port_heap': True, 'no_stock_thread_guard_clock_lock_fallback': True,
              'tls_allocator': 'custom' if custom_tls else 'freertos', 'images': images}
    (args.build / 'runtime_link.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
