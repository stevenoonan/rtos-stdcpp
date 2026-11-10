#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Audit the selected RTOS kernel, CPU ABI and runtime providers in QEMU images."""
import argparse
import json
from pathlib import Path
import subprocess
import shlex

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', required=True, type=Path)
parser.add_argument('--toolchain', required=True, type=Path)
parser.add_argument('--machine', required=True, choices=['a9', 'm4', 'm7', 'm55'])
parser.add_argument('--backend', required=True, choices=['FreeRTOS', 'ThreadX'])
args = parser.parse_args()
commands = json.loads((args.build / 'compile_commands.json').read_text())
files = [Path(item['file']).name for item in commands]
cache = (args.build / 'CMakeCache.txt').read_text()
standard = 20 if 'RTOS_STDCPP_CXX_STANDARD:STRING=20' in cache else 17
root = Path(__file__).resolve().parents[1]
flags = json.loads((root / f'cmake/profiles/qemu-{args.machine}-gcc-14.3.1-cxx17.json').read_text())['cpu_flags']
tls_command = next(item['command'] for item in commands if Path(item['file']).name == 'tls.cpp')
custom_tls = '-DRTOS_STDCPP_TLS_CUSTOM_ALLOCATOR=1' in tls_command
hooks = {'rtos_stdcpp_tls_allocate', 'rtos_stdcpp_tls_deallocate'}
tls_object = next(args.build.glob('**/rtos_stdcpp.dir/src/toolchains/gcc/tls.cpp.obj'))
undefined = subprocess.check_output([str(args.toolchain / 'bin/arm-none-eabi-nm'), '--undefined-only', str(tls_object)], text=True)
undefined_names = {line.split()[-1] for line in undefined.splitlines() if line.split()}
if custom_tls and (not hooks <= undefined_names or {'malloc', 'free', 'pvPortMalloc', 'vPortFree'} & undefined_names):
    raise RuntimeError('Custom TLS runtime must use only the paired allocation hooks')
if not custom_tls and hooks & undefined_names:
    raise RuntimeError('Default TLS runtime unexpectedly requires custom hooks')
providers = ['thread.cpp', 'lifecycle.cpp', 'gthr_key.cpp', 'tls.cpp', 'cxx_guards.cpp', 'mutex.cc', 'condition_variable.cc', 'future.cc']
if args.backend == 'ThreadX':
    providers += ['tx_thread_create.c', 'tx_thread_schedule.S', 'tx_timer_interrupt.S', 'threadx_time.cpp']
else:
    providers += ['tasks.c', 'queue.c', 'list.c', 'heap_4.c', 'port.c', 'freertos_time.cpp']
    if args.machine == 'a9':
        providers += ['portASM.S', 'runtime.cpp']
    if args.machine == 'm55':
        providers += ['portasm.c']
if args.machine != 'a9':
    providers += ['newlib_malloc_lock.c', args.backend.lower() + '.c']
providers = list(dict.fromkeys(providers))
for name in providers:
    if files.count(name) != 1:
        raise RuntimeError('Provider missing or compiled multiple times: ' + name)
for item in commands:
    command = item['command']
    for flag in flags:
        if flag not in shlex.split(command):
            raise RuntimeError('Inconsistent CPU ABI: ' + item['file'])
    # Nano and full newlib have different struct _reent layouts. Kernel, adapter,
    # runtime and application must all see the same layout in TX_THREAD.
    if Path(item['file']).suffix != '.S' and '--specs=nano.specs' not in command:
        raise RuntimeError('Inconsistent newlib ABI: ' + item['file'])
    if Path(item['file']).suffix in ['.cpp', '.cc']:
        if ('gcc_threading_config.h' not in command or '-D_GLIBCXX_HAS_GTHREADS=1' not in command
                or f'-std=c++{standard}' not in shlex.split(command)):
            raise RuntimeError('Inconsistent gthread configuration: ' + item['file'])
    if args.machine == 'm55' and '-mcmse' not in command:
        raise RuntimeError('Missing secure M55 profile')
images = []
for elf in sorted(args.build.glob('*.elf')):
    content = elf.with_suffix('.map').read_text()
    archive = content.split('Discarded input sections')[0]
    for name in ['thread.o', 'mutex.o', 'condition_variable.o', 'guard.o', 'chrono.o',
                 'libc_a-lock.o', 'libc_a-mlock.o', 'emutls.o', 'atexit_thread.o']:
        if '(' + name + ')' in archive:
            raise RuntimeError('Stock runtime fallback: ' + name)
    symbols = subprocess.check_output([str(args.toolchain / 'bin/arm-none-eabi-nm'),
                                       '--defined-only', str(elf)], text=True)
    names = [line.split()[-1] for line in symbols.splitlines() if len(line.split()) >= 3]
    required = ['_exit', '__malloc_lock']
    if args.backend == 'ThreadX':
        required += ['_tx_thread_schedule', '_tx_thread_create', '_tx_timer_interrupt',
                     '_tx_execution_thread_enter', '_tx_execution_thread_exit']
    elif args.machine == 'a9':
        required += ['vTaskStartScheduler', 'FreeRTOS_IRQ_Handler', 'FreeRTOS_SWI_Handler', 'FreeRTOS_Tick_Handler']
    else:
        required += ['vTaskStartScheduler', 'PendSV_Handler', 'SVC_Handler', 'SysTick_Handler']
    for name in required:
        if names.count(name) != 1:
            raise RuntimeError('Missing or duplicate provider: ' + name)
    if elf.stem in ['conformance', 'qualification']:
        for name in ['__cxa_guard_acquire', '__cxa_guard_release']:
            if names.count(name) != 1:
                raise RuntimeError('Missing static guard provider: ' + name)
    if elf.stem in ['conformance', 'qualification', 'cxx20']:
        for name in ['__emutls_get_address', '__emutls_register_common', '__cxa_thread_atexit']:
            if names.count(name) != 1:
                raise RuntimeError('Missing TLS provider: ' + name)
        if custom_tls and any(names.count(hook) != 1 for hook in hooks):
            raise RuntimeError('Custom TLS provider missing or duplicated')
    images.append(elf.name)
standard20 = standard == 20
if len(images) != ((11 if standard20 else 10) + (2 if args.backend == 'FreeRTOS' else 0)):
    raise RuntimeError('Missing required images')
report = {'status': 'passed', 'machine': args.machine, 'backend': args.backend, 'single_kernel_and_runtime': True,
          'consistent_cpu_and_newlib_abi': True, 'no_stock_runtime_fallback': True,
          'tls_allocator': 'custom' if custom_tls else 'default', 'images': images}
(args.build / 'runtime_link.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
