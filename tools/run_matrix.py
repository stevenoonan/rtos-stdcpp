#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reproduce the qualified host/A9 C++17/C++20 matrix using prepared offline inputs."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain-root', type=Path)
    parser.add_argument('--qemu-root', type=Path, help='Directory produced by setup_qemu.py')
    parser.add_argument('--standard', choices=['17', '20', 'both'], default='both')
    parser.add_argument('--clang', action='store_true', help='Also require the optional Clang host profile')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--environment', choices=['all', 'host', 'a9'], default='all')
    parser.add_argument('--host-compiler', choices=['gcc', 'clang', 'both'])
    parser.add_argument('--config', choices=['both', 'Debug', 'Release'], default='both')
    parser.add_argument('--tls-allocator', choices=['freertos', 'custom', 'both'], default='both',
                        help='A9 TLS allocation policies to exercise (default: both)')
    args = parser.parse_args()
    if args.environment != 'host' and (not args.toolchain_root or not args.qemu_root):
        parser.error('A9 execution requires --toolchain-root and --qemu-root')
    standards = [17, 20] if args.standard == 'both' else [int(args.standard)]
    configs = ['Debug', 'Release'] if args.config == 'both' else [args.config]
    tls_allocators = ['freertos', 'custom'] if args.tls_allocator == 'both' else [args.tls_allocator]
    selected_host = args.host_compiler or ('both' if args.clang else 'gcc')
    hosts = ['gcc', 'clang'] if selected_host == 'both' else [selected_host]
    if args.environment == 'a9':
        hosts = []
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        parser.error('Use an output outside the library tree')
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error('Use an empty output directory to preserve earlier evidence')
    report = {'status': 'running', 'started_utc': datetime.now(timezone.utc).isoformat(),
              'network_operations': False, 'languages': ['c++' + str(x) for x in standards],
              'tls_allocators': tls_allocators if args.environment != 'host' else [], 'checks': [], 'host_compilers': {}}
    try:
        report['library_commit'] = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
        report['library_status'] = subprocess.check_output(['git', '-C', str(ROOT), 'status', '--short'], text=True)
    except subprocess.SubprocessError:
        report['library_commit'] = 'source archive; Git revision unavailable'
    def save():
        (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    def run(name, command, layer):
        command = list(map(str, command)); log = output / (name + '.log')
        with log.open('w') as stream:
            result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, timeout=180)
        report['checks'].append({'name': name, 'layer': layer, 'command': command, 'log': log.name, 'returncode': result.returncode, 'status': 'passed' if result.returncode == 0 else 'failed'})
        save(); print(name + ': ' + report['checks'][-1]['status'], flush=True)
        if result.returncode:
            raise RuntimeError('Check failed; inspect ' + str(log))
    try:
        host_lock = json.loads((ROOT / 'cmake/profiles/host-linux-x86_64.json').read_text())
        for profile in hosts:
            lock = host_lock['profiles'][profile]
            compilers = {}
            for kind in ['cc', 'cxx']:
                command = lock[kind]
                found = shutil.which(command)
                if not found:
                    raise RuntimeError('Required host compiler is missing: ' + command)
                path = Path(found).absolute()
                version = subprocess.check_output([str(path), '-dumpversion'], text=True).strip()
                if int(version.split('.')[0]) != lock['major']:
                    raise RuntimeError('Unexpected host compiler major version: ' + version)
                compilers[kind] = str(path)
            report['host_compilers'][profile] = subprocess.check_output([compilers['cxx'], '--version'], text=True).splitlines()[0]
            flags = ['--gcc-install-dir=/usr/lib/gcc/x86_64-linux-gnu/13'] if profile == 'clang' else []
            for standard in standards:
                for config in configs:
                    name = profile + '-cxx' + str(standard) + '-' + config.lower(); build = output / name
                    run(name + '-configure', ['cmake', '-S', ROOT, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + config,
                        '-DCMAKE_C_COMPILER=' + compilers['cc'], '-DCMAKE_CXX_COMPILER=' + compilers['cxx'], '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                        '-DRTOS_STDCPP_CXX_STANDARD=' + str(standard), '-DRTOS_STDCPP_BUILD_LIBRARY=OFF', '-DRTOS_STDCPP_BUILD_HOST_TESTS=ON', '-DCMAKE_CXX_FLAGS=' + ' '.join(flags)], 'configuration')
                    run(name + '-build', ['cmake', '--build', build, '--parallel', '8'], 'compile_link')
                    run(name + '-test', ['ctest', '--test-dir', build, '--output-on-failure', '--no-tests=error', '--output-junit', build / 'junit.xml'], 'host')
        if args.environment != 'host':
            qemu_lock = json.loads((ROOT / 'cmake/profiles/qemu-8.2.2-ubuntu24-x86_64.json').read_text())
            qemu = args.qemu_root.resolve() / 'qemu-system-arm'
            binary = args.qemu_root.resolve() / 'root/usr/bin/qemu-system-arm'
            if sha(binary) != qemu_lock['binary_sha256'] or subprocess.check_output([str(qemu), '--version'], text=True).splitlines()[0] != qemu_lock['version']:
                raise RuntimeError('QEMU does not match the selected profile')
            report['qemu_binary_sha256'] = sha(binary)
            for standard in standards:
                for config in configs:
                    for allocator in tls_allocators:
                        for wrap in [False, True]:
                            name = ('a9-cxx' + str(standard) + '-' + config.lower()
                                    + ('-custom-tls' if allocator == 'custom' else '') + ('-wrap' if wrap else ''))
                            build = output / name
                            run(name + '-configure', ['cmake', '-S', ROOT / 'platforms/qemu-vexpress-a9', '-B', build, '-G', 'Ninja',
                                '-DCMAKE_BUILD_TYPE=' + config, '-DCMAKE_TOOLCHAIN_FILE=' + str(ROOT / 'cmake/toolchains/qemu-a9.cmake'),
                                '-DRTOS_STDCPP_TOOLCHAIN_ROOT=' + str(args.toolchain_root.resolve()), '-DRTOS_STDCPP_QEMU=' + str(qemu),
                                '-DRTOS_STDCPP_CXX_STANDARD=' + str(standard),
                                '-DRTOS_STDCPP_TLS_CUSTOM_ALLOCATOR=' + ('ON' if allocator == 'custom' else 'OFF'),
                                '-DRTOS_STDCPP_A9_TICK_WRAP=' + ('ON' if wrap else 'OFF')], 'configuration')
                            run(name + '-build', ['cmake', '--build', build, '--parallel', '8'], 'compile_link')
                            run(name + '-test', ['ctest', '--test-dir', build, '--output-on-failure', '--no-tests=error', '--output-junit', build / 'junit.xml'], 'emulator_and_link')
        report['status'] = 'passed'
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        report.update(status='failed', error=str(error)); print(str(error), file=sys.stderr)
    report['finished_utc'] = datetime.now(timezone.utc).isoformat(); save()
    return int(report['status'] != 'passed')

if __name__ == '__main__':
    sys.exit(main())
