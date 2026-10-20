#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify the pinned kernel snapshot and standalone image provider provenance."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
sys.dont_write_bytecode = True
from check_profile import verify
ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path)
    parser.add_argument('--toolchain', type=Path)
    args = parser.parse_args()
    lock = json.loads((ROOT / 'cmake/profiles/qemu-a9-kernel.json').read_text())
    verify(ROOT / 'third_party/freertos-kernel', lock['files'])
    if not args.build:
        return
    commands = json.loads((args.build / 'compile_commands.json').read_text())
    cache = (args.build / 'CMakeCache.txt').read_text()
    standard = 20 if 'RTOS_STDCPP_CXX_STANDARD:STRING=20' in cache else 17
    files = [Path(x['file']).name for x in commands]
    for name in ['tasks.c', 'queue.c', 'list.c', 'event_groups.c', 'heap_4.c', 'port.c', 'portASM.S', 'thread.cpp', 'gthr_key.cpp', 'freertos_time.cpp', 'mutex.cc', 'condition_variable.cc']:
        if files.count(name) != 1:
            raise RuntimeError('Provider compiled multiple times or missing: ' + name)
    for item in commands:
        command = item['command']
        if Path(item['file']).suffix in ['.cpp', '.cc']:
            if f'-std=c++{standard}' not in command or 'gcc_threading_config.h' not in command or '-D_GLIBCXX_HAS_GTHREADS=1' not in command:
                raise RuntimeError('Inconsistent C++ ABI/configuration: ' + item['file'])
        for flag in ['-mcpu=cortex-a9', '-mfpu=vfpv3', '-mfloat-abi=hard', '-marm']:
            if flag not in command:
                raise RuntimeError('Wrong CPU profile')
    images = {}
    for elf in sorted(args.build.glob('*.elf')):
        content = elf.with_suffix('.map').read_text()
        archive = content.split('Discarded input sections')[0]
        forbidden = ['thread.o', 'mutex.o', 'condition_variable.o', 'guard.o', 'chrono.o', 'libc_a-lock.o', 'libc_a-mlock.o']
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
        images[elf.name] = hashlib.sha256(elf.read_bytes()).hexdigest()
    if len(images) != (8 if standard == 20 else 7):
        raise RuntimeError('Missing required images')
    report = {'status': 'passed', 'layer': 'compile_link', 'kernel_manifest_sha256': hashlib.sha256((ROOT / 'cmake/profiles/qemu-a9-kernel.json').read_bytes()).hexdigest(),
              'single_kernel_port_heap': True, 'no_stock_thread_guard_clock_lock_fallback': True, 'images_sha256': images}
    (args.build / 'provenance.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
