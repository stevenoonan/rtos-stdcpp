#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check an isolated source copy using explicit compiler and kernel inputs."""

import argparse
from collections import Counter
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--toolchain-root', type=Path, help='GCC installation with bin/arm-none-eabi-g++')
    parser.add_argument('--backend', choices=['FreeRTOS', 'ThreadX', 'both'], default='both')
    parser.add_argument('--standard', choices=['17', '20', 'both'], default='both')
    parser.add_argument('--config', choices=['Debug', 'Release', 'both'], default='both')
    parser.add_argument('--freertos-kernel-root', type=Path)
    parser.add_argument('--freertos-port-include', type=Path)
    parser.add_argument('--threadx-root', type=Path)
    parser.add_argument('--skip-host', action='store_true', help='Run embedded/release checks without host tests')
    parser.add_argument('--host-cc', default='gcc-13')
    parser.add_argument('--host-cxx', default='g++-13')
    args = parser.parse_args()

    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents or output in ROOT.parents:
        parser.error('Use a new output directory outside the library tree')
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error('Use a new empty output directory')

    backends = ['FreeRTOS', 'ThreadX'] if args.backend == 'both' else [args.backend]
    standards = ['17', '20'] if args.standard == 'both' else [args.standard]
    configs = ['Debug', 'Release'] if args.config == 'both' else [args.config]
    if 'FreeRTOS' in backends and not (args.freertos_kernel_root and args.freertos_port_include):
        parser.error('FreeRTOS checks require --freertos-kernel-root and --freertos-port-include')
    if 'ThreadX' in backends and not args.threadx_root:
        parser.error('ThreadX checks require --threadx-root')

    env = os.environ.copy()
    if args.toolchain_root:
        env['PATH'] = str(args.toolchain_root.resolve() / 'bin') + os.pathsep + env.get('PATH', '')
    compiler = shutil.which('arm-none-eabi-g++', path=env.get('PATH'))
    if not compiler:
        parser.error('Put GCC 14.3.1 on PATH or supply --toolchain-root')
    nm = Path(compiler).parent / 'arm-none-eabi-nm'
    report = {'status': 'running', 'network_operations': False,
              'started_utc': datetime.now(timezone.utc).isoformat(), 'checks': []}

    def save():
        (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')

    def run(name, command, expected=None, directory=output):
        command = [str(value) for value in command]
        result = subprocess.run(command, cwd=directory, env=env, capture_output=True,
                                text=True, timeout=180)
        log = result.stdout + result.stderr
        (output / (name + '.log')).write_text(log)
        passed = result.returncode == 0 if expected is None else (
            result.returncode != 0 and expected in ' '.join(log.split()))
        report['checks'].append({'name': name, 'command': command, 'returncode': result.returncode,
                                 'expected_rejection': expected, 'status': 'passed' if passed else 'failed'})
        save()
        print(name + ': ' + report['checks'][-1]['status'], flush=True)
        require(passed, name + ' failed; inspect ' + str(output / (name + '.log')))
        return log

    try:
        library = output / 'library'
        for path in ROOT.rglob('*'):
            if path.is_symlink():
                require(path.resolve().is_relative_to(ROOT), 'Source symlink escapes library: ' + str(path))
        shutil.copytree(ROOT, library, symlinks=True,
                        ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
        dependencies = output / 'dependencies'
        inputs = {}
        if 'FreeRTOS' in backends:
            kernel = dependencies / 'freertos'
            shutil.copytree(args.freertos_kernel_root.resolve() / 'include', kernel / 'include')
            port = kernel / 'port'
            shutil.copytree(args.freertos_port_include.resolve(), port)
            inputs['FreeRTOS'] = ['-DFREERTOS_KERNEL_ROOT=' + str(kernel),
                                  '-DFREERTOS_PORT_INCLUDE=' + str(port)]
        if 'ThreadX' in backends:
            kernel = dependencies / 'threadx'
            for relative in ['common/inc', 'ports/cortex_m55/gnu/inc']:
                shutil.copytree(args.threadx_root.resolve() / relative, kernel / relative)
            inputs['ThreadX'] = ['-DTHREADX_ROOT=' + str(kernel)]

        run('provenance', [sys.executable, library / 'tools/check_licenses.py'])
        if not args.skip_host:
            for standard in standards:
                for config in configs:
                    name = f'host-cxx{standard}-{config.lower()}'
                    build = output / name
                    run(name + '-configure', ['cmake', '-S', library, '-B', build, '-G', 'Ninja',
                        '-DCMAKE_C_COMPILER=' + args.host_cc, '-DCMAKE_CXX_COMPILER=' + args.host_cxx,
                        '-DCMAKE_BUILD_TYPE=' + config, '-DRTOS_STDCPP_BUILD_LIBRARY=OFF',
                        '-DRTOS_STDCPP_BUILD_HOST_TESTS=ON', '-DRTOS_STDCPP_CXX_STANDARD=' + standard])
                    run(name + '-build', ['cmake', '--build', build, '--parallel', '4'])
                    run(name + '-test', ['ctest', '--test-dir', build, '--output-on-failure', '--no-tests=error'])

        for backend in backends:
            common = ['cmake', '-S', library / 'examples/compile-check', '-G', 'Ninja',
                      '-DRTOS_STDCPP_BACKEND=' + backend, *inputs[backend],
                      '-DCMAKE_TOOLCHAIN_FILE=' + str(library / 'cmake/toolchains/arm-none-eabi.cmake')]
            for standard in standards:
                for config in configs:
                    for custom in [False, True]:
                        name = backend.lower() + f'-cxx{standard}-{config.lower()}' + ('-custom-tls' if custom else '')
                        build = output / name
                        run(name + '-configure', [*common, '-B', build,
                            '-DCMAKE_BUILD_TYPE=' + config, '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                            '-DRTOS_STDCPP_PROFILE=stm32-gcc-14.3.1-cxx' + standard,
                            '-DRTOS_STDCPP_TLS_CUSTOM_ALLOCATOR=' + ('ON' if custom else 'OFF')])
                        run(name + '-build', ['cmake', '--build', build, '--parallel', '4'])
                        run(name + '-ctest', ['ctest', '--test-dir', build, '-L', 'compile_link',
                            '--output-on-failure', '--no-tests=error'])
                        commands = json.loads((build / 'compile_commands.json').read_text())
                        counts = Counter(Path(entry['file']).name for entry in commands)
                        native = ['lifecycle.cpp', 'threadx_time.cpp', 'threadx.c', 'application.cpp'] if backend == 'ThreadX' else [
                            'tls_context.cpp', 'freertos_time.cpp', 'freertos.c']
                        providers = ['thread.cpp', 'gthr_key.cpp', 'tls.cpp', 'mutex.cc',
                                     'condition_variable.cc', 'future.cc', 'public_api.cpp',
                                     'newlib_malloc_lock.c', 'cxx_guards.cpp', 'cxx_nothrow.cpp', 'hooks.c', *native]
                        require(counts == Counter(providers), 'Missing, duplicate or unselected provider in ' + name)
                        for entry in commands:
                            require(Path(entry['file']).resolve().is_relative_to(library),
                                    'Compiled source escapes isolated library')
                            flags = shlex.split(entry['command'])
                            if Path(entry['file']).suffix in ['.cpp', '.cc']:
                                require('-std=c++' + standard in flags, 'Wrong language mode')
                                require('-include' + str(library / 'src/toolchains/gcc/gcc_threading_config.h') in flags,
                                        'Missing ABI configuration')
                        deps = run(name + '-headers', ['ninja', '-C', build, '-t', 'deps'])
                        hook = str(library / 'src/toolchains/gcc/bits/gthr-default.h')
                        require(hook in deps, 'Missing gthread hook')
                        for line in deps.splitlines():
                            if line.strip().endswith('bits/gthr-default.h'):
                                require(line.strip() == hook, 'Stock gthread hook selected')
                        if standard == '20':
                            require(str(library / 'third_party/libstdcxx/stm32-gcc-14.3.1-cxx20/stop_token') in deps,
                                    'Missing qualified stop-token header')
                        providers = [path for path in build.glob('**/rtos_stdcpp.dir/src/toolchains/gcc/tls.cpp.*')
                                     if path.suffix in ['.o', '.obj']]
                        require(len(providers) == 1, 'Expected one TLS ABI provider')
                        symbols = run(name + '-tls-symbols', [nm, '--defined-only', providers[0]])
                        names = [line.split()[-1] for line in symbols.splitlines() if len(line.split()) >= 3]
                        for symbol in ['__emutls_get_address', '__emutls_register_common', '__cxa_thread_atexit']:
                            require(names.count(symbol) == 1, 'Missing or duplicate TLS ABI provider: ' + symbol)
                        if custom:
                            symbols = run(name + '-tls-hooks', [nm, '--undefined-only', providers[0]])
                            names = {line.split()[-1] for line in symbols.splitlines() if line.split()}
                            require({'rtos_stdcpp_tls_allocate', 'rtos_stdcpp_tls_deallocate'} <= names,
                                    'Custom TLS allocation hooks missing')
                            require(not {'pvPortMalloc', 'vPortFree', 'malloc', 'free'} & names,
                                    'Custom TLS still references a default allocator')
                        cache = (build / 'CMakeCache.txt').read_text().splitlines()
                        require('CMAKE_C_FLAGS:STRING=' in cache and 'CMAKE_CXX_FLAGS:STRING=' in cache,
                                'Library replaced global compiler flags')
            run(backend.lower() + '-reject-language-conflict', [*common, '-B', output / (backend + '-bad-language'),
                '-DRTOS_STDCPP_PROFILE=stm32-gcc-14.3.1-cxx20', '-DRTOS_STDCPP_CXX_STANDARD=17'],
                'conflicts with RTOS_STDCPP_PROFILE')
            native_cc = shutil.which('cc')
            native_cxx = shutil.which('c++')
            require(native_cc and native_cxx, 'Native compilers are required for rejection checks')
            run(backend.lower() + '-reject-host-compiler', [*common,
                '-B', output / (backend + '-bad-compiler'),
                '-DCMAKE_C_COMPILER=' + native_cc, '-DCMAKE_CXX_COMPILER=' + native_cxx],
                'requires GCC 14.3.1 for both C and C++')

        run('reject-language-mode', ['cmake', '-S', library, '-B', output / 'bad-language',
            '-DRTOS_STDCPP_BUILD_LIBRARY=OFF', '-DRTOS_STDCPP_CXX_STANDARD=23'], 'must be 17 or 20')
        run('reject-unknown-profile', ['cmake', '-S', library, '-B', output / 'bad-profile',
            '-DRTOS_STDCPP_PROFILE=unqualified'], 'Select an exact qualified RTOS_STDCPP_PROFILE')
        run('reject-missing-provider', ['cmake', '-S', library, '-B', output / 'bad-provider',
            '-DRTOS_STDCPP_PROFILE=stm32-gcc-14.3.1-cxx17'], 'must name an existing configuration target')

        for relative, expected in [
            ('third_party/libstdcxx/stm32-gcc-14.3.1-cxx17/libstdc++-v3/src/c++11/mutex.cc', 'Runtime bytes changed:'),
            ('third_party/libstdcxx/stm32-gcc-14.3.1-cxx17/libstdc++-v3/src/c++11/future.cc', 'Runtime bytes changed:'),
            ('third_party/libstdcxx/stm32-gcc-14.3.1-cxx20/stop_token', 'C++20 runtime bytes changed'),
            ('third_party/freertos-kernel/tasks.c', 'Kernel bytes changed:'),
            ('LICENSES/Apache-2.0.txt', 'Apache license text changed'),
        ]:
            selected = library / relative
            original = selected.read_bytes()
            try:
                selected.write_bytes(original + b'\n/* modified dependency fixture */\n')
                run('reject-modified-' + selected.name, [sys.executable, library / 'tools/check_licenses.py'], expected)
            finally:
                selected.write_bytes(original)
        report.update(status='passed', standalone_without_parent_build=True,
                      runtime_providers_verified=True, custom_tls_hooks_verified=True)
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        report.update(status='failed', error=str(error))
    report['finished_utc'] = datetime.now(timezone.utc).isoformat()
    save()
    print(report['status'] + ': ' + str(output / 'results.json'), flush=True)
    return int(report['status'] != 'passed')


if __name__ == '__main__':
    sys.exit(main())
