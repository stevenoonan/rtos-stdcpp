#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reproduce the qualified host/A9 C++17 matrix using prepared offline inputs."""
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
from check_profile import verify
ROOT = Path(__file__).resolve().parents[1]

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain-root', type=Path)
    parser.add_argument('--qemu-root', type=Path, help='Directory produced by setup_qemu.py')
    parser.add_argument('--clang', action='store_true', help='Also require the pinned optional Clang host profile')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--environment', choices=['all', 'host', 'a9'], default='all')
    parser.add_argument('--host-compiler', choices=['gcc', 'clang', 'both'])
    parser.add_argument('--host-root', type=Path, help='Pinned host prefix from setup_ci.py')
    parser.add_argument('--config', choices=['both', 'Debug', 'Release'], default='both')
    args = parser.parse_args()
    if args.environment != 'host' and (not args.toolchain_root or not args.qemu_root):
        parser.error('A9 execution requires --toolchain-root and --qemu-root')
    configs = ['Debug', 'Release'] if args.config == 'both' else [args.config]
    if args.host_root:
        args.host_root = args.host_root.resolve()
        os.environ['PATH'] = str(args.host_root / 'usr/bin') + os.pathsep + os.environ['PATH']
        os.environ['LD_LIBRARY_PATH'] = str(args.host_root / 'usr/lib/x86_64-linux-gnu')
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
              'network_operations': False, 'language': 'c++17', 'checks': [],
              'source_files_sha256': {p.relative_to(ROOT).as_posix(): sha(p) for p in sorted(ROOT.rglob('*')) if p.is_file() and '.git' not in p.relative_to(ROOT).parts}}
    try:
        report['library_commit'] = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
        report['library_status'] = subprocess.check_output(['git', '-C', str(ROOT), 'status', '--short'], text=True)
    except subprocess.SubprocessError:
        report['library_commit'] = 'source archive; use source hashes'
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
                item = lock[kind]; path = Path(shutil.which(item['command']) or item['command']).absolute()
                if sha(path) != item['sha256'] or subprocess.check_output([str(path), '--version'], text=True).splitlines()[0] != item['version']:
                    raise RuntimeError('Host compiler differs from pinned profile: ' + profile)
                compilers[kind] = str(path)
            flags = ['--gcc-install-dir=' + str(args.host_root / 'usr/lib/gcc/x86_64-linux-gnu/13')] if args.host_root and profile == 'clang' else []
            library = Path(subprocess.check_output([compilers['cxx'], *flags, '-print-file-name=libstdc++.so'], text=True).strip()).resolve()
            if sha(library) != lock['libstdcxx_sha256']:
                raise RuntimeError('Host libstdc++ differs from pinned profile')
            for config in configs:
                name = profile + '-' + config.lower(); build = output / name
                run(name + '-configure', ['cmake', '-S', ROOT, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + config,
                    '-DCMAKE_C_COMPILER=' + compilers['cc'], '-DCMAKE_CXX_COMPILER=' + compilers['cxx'], '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                    '-DRTOS_STDCPP_BUILD_LIBRARY=OFF', '-DRTOS_STDCPP_BUILD_HOST_TESTS=ON', '-DCMAKE_CXX_FLAGS=' + ' '.join(flags)], 'configuration')
                run(name + '-build', ['cmake', '--build', build, '--parallel', '8'], 'compile_link')
                run(name + '-test', ['ctest', '--test-dir', build, '--output-on-failure', '--no-tests=error', '--output-junit', build / 'junit.xml'], 'host')
        if args.environment != 'host':
            qemu_lock = json.loads((ROOT / 'cmake/profiles/qemu-8.2.2-ubuntu24-x86_64.json').read_text())
            qemu = args.qemu_root.resolve() / 'qemu-system-arm'
            binary = args.qemu_root.resolve() / 'root/usr/bin/qemu-system-arm'
            if sha(binary) != qemu_lock['binary_sha256'] or subprocess.check_output([str(qemu), '--version'], text=True).splitlines()[0] != qemu_lock['version']:
                raise RuntimeError('QEMU does not match the selected profile')
            report['qemu_binary_sha256'] = sha(binary)
            # Record the actual shared dependency closure in addition to package locks.
            env = os.environ.copy(); env['LD_LIBRARY_PATH'] = str(args.qemu_root.resolve() / 'root/usr/lib/x86_64-linux-gnu')
            dependencies = subprocess.check_output(['ldd', str(binary)], text=True, env=env)
            if 'not found' in dependencies:
                raise RuntimeError('QEMU runtime dependency is missing')
            report['qemu_shared_dependencies_sha256'] = {line.split('=>', 1)[1].split()[0]: sha(line.split('=>', 1)[1].split()[0]) for line in dependencies.splitlines() if '=>' in line and line.split('=>', 1)[1].strip().startswith('/')}
            for config in configs:
                for wrap in [False, True]:
                    name = 'a9-' + config.lower() + ('-wrap' if wrap else ''); build = output / name
                    run(name + '-configure', ['cmake', '-S', ROOT / 'platforms/qemu-vexpress-a9', '-B', build, '-G', 'Ninja',
                        '-DCMAKE_BUILD_TYPE=' + config, '-DCMAKE_TOOLCHAIN_FILE=' + str(ROOT / 'cmake/toolchains/qemu-a9.cmake'),
                        '-DRTOS_STDCPP_TOOLCHAIN_ROOT=' + str(args.toolchain_root.resolve()), '-DRTOS_STDCPP_QEMU=' + str(qemu),
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
