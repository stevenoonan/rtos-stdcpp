#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run FreeRTOS and ThreadX on pinned QEMU M4/M7/M55 models using prepared offline inputs."""
import argparse
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from matrix_support import ROOT, MatrixResults, verify_compiler, verify_kernel, verify_qemu


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain-root', required=True, type=Path)
    parser.add_argument('--qemu-root', required=True, type=Path)
    parser.add_argument('--threadx-root', type=Path)
    parser.add_argument('--freertos-root', type=Path)
    parser.add_argument('--backend', choices=['FreeRTOS', 'ThreadX', 'both'], default='both')
    parser.add_argument('--core', choices=['m4', 'm7', 'm55', 'all'], default='all')
    parser.add_argument('--standard', choices=['17', '20', 'both'], default='both')
    parser.add_argument('--config', choices=['Debug', 'Release', 'both'], default='both')
    parser.add_argument('--tls-allocator', choices=['default', 'custom', 'both'], default='both')
    parser.add_argument('--tick-wrap', choices=['normal', 'wrap', 'both'], default='both')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    selected = ['FreeRTOS', 'ThreadX'] if args.backend == 'both' else [args.backend]
    kernels = {'FreeRTOS': args.freertos_root, 'ThreadX': args.threadx_root}
    if any(not kernels[backend] for backend in selected):
        parser.error('Supply --freertos-root and/or --threadx-root for the selected backends')
    try:
        results = MatrixResults(args.output, backends=selected, kernel_commits={})
    except (OSError, ValueError) as error:
        parser.error(str(error))
    run = results.run
    output = results.output
    report = results.report

    def choices(selected, all_values):
        return all_values if selected in ['all', 'both'] else [selected]

    try:
        report['compiler_version'] = verify_compiler(args.toolchain_root)
        qemu, report['qemu_binary_sha256'] = verify_qemu(args.qemu_root)
        for backend in selected:
            kernel = kernels[backend].resolve()
            report['kernel_commits'][backend] = verify_kernel(kernel, backend)
            for core in choices(args.core, ['m4', 'm7', 'm55']):
                for standard in choices(args.standard, ['17', '20']):
                    for config in choices(args.config, ['Debug', 'Release']):
                        for allocator in choices(args.tls_allocator, ['default', 'custom']):
                            for wrap in choices(args.tick_wrap, ['normal', 'wrap']):
                                name = f'{backend.lower()}-{core}-cxx{standard}-{config.lower()}-{allocator}-{wrap}'
                                build = output / name
                                run(name + '-configure', ['cmake', '-S', ROOT / 'platforms/qemu-cortex-m', '-B', build, '-G', 'Ninja',
                                    '-DCMAKE_BUILD_TYPE=' + config,
                                    '-DCMAKE_TOOLCHAIN_FILE=' + str(ROOT / 'cmake/toolchains/qemu-cortex-m.cmake'),
                                    '-DRTOS_STDCPP_TOOLCHAIN_ROOT=' + str(args.toolchain_root.resolve()),
                                    '-DRTOS_STDCPP_BACKEND=' + backend,
                                    '-D' + ('THREADX_KERNEL_ROOT' if backend == 'ThreadX' else 'FREERTOS_KERNEL_ROOT') + '=' + str(kernel), '-DRTOS_STDCPP_CORTEX_M=' + core,
                                    '-DRTOS_STDCPP_QEMU=' + str(qemu), '-DRTOS_STDCPP_CXX_STANDARD=' + standard,
                                    '-DRTOS_STDCPP_TLS_CUSTOM_ALLOCATOR=' + ('ON' if allocator == 'custom' else 'OFF'),
                                    '-DRTOS_STDCPP_M_TICK_WRAP=' + ('ON' if wrap == 'wrap' else 'OFF')], 'configuration')
                                run(name + '-build', ['cmake', '--build', build, '--parallel', '8'], 'compile_link')
                                run(name + '-test', ['ctest', '--test-dir', build, '--output-on-failure', '--no-tests=error',
                                                    '--output-junit', build / 'junit.xml'], 'emulator_and_link')
        return results.finish()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        return results.finish(error)


if __name__ == '__main__':
    sys.exit(main())
