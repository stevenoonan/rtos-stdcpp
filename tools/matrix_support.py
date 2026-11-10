# SPDX-License-Identifier: MIT
"""Shared offline input verification and evidence capture for CI matrix runners."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def verify_qemu(directory):
    directory = directory.resolve()
    lock = json.loads((ROOT / 'cmake/profiles/qemu-8.2.2-ubuntu24-x86_64.json').read_text())
    executable = directory / 'qemu-system-arm'
    digest = sha(directory / 'root/usr/bin/qemu-system-arm')
    if digest != lock['binary_sha256']:
        raise RuntimeError('QEMU does not match the pinned profile')
    version = subprocess.check_output([str(executable), '--version'], text=True).splitlines()[0]
    if version != lock['version']:
        raise RuntimeError('QEMU does not match the pinned profile')
    return executable, digest


def verify_compiler(directory):
    compiler = directory.resolve() / 'bin/arm-none-eabi-g++'
    version = subprocess.check_output([str(compiler), '-dumpfullversion'], text=True).strip()
    target = subprocess.check_output([str(compiler), '-dumpmachine'], text=True).strip()
    if version != '14.3.1' or target != 'arm-none-eabi':
        raise RuntimeError('Compiler does not match the selected profile')
    return subprocess.check_output([str(compiler), '--version'], text=True).splitlines()[0]


def verify_kernel(directory, backend):
    pins = json.loads((ROOT / 'cmake/profiles/embedded-kernel-inputs.json').read_text())
    revision = subprocess.check_output(['git', '-C', str(directory), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != pins[backend + '-QEMU']['commit']:
        raise RuntimeError(backend + ' does not match the pinned official kernel')
    subprocess.run(['git', '-C', str(directory), 'diff', '--exit-code', 'HEAD'],
                   check=True, stdout=subprocess.DEVNULL)
    return revision


class MatrixResults:
    def __init__(self, output, **metadata):
        self.output = output.resolve()
        if self.output == ROOT or ROOT in self.output.parents or self.output in ROOT.parents:
            raise ValueError('Use an output outside the library tree')
        self.output.mkdir(parents=True, exist_ok=True)
        if any(self.output.iterdir()):
            raise ValueError('Use an empty output directory to preserve earlier evidence')
        self.report = {'status': 'running', 'started_utc': datetime.now(timezone.utc).isoformat(),
                       'network_operations': False, 'checks': [], **metadata}
        try:
            self.report['library_commit'] = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
            self.report['library_status'] = subprocess.check_output(['git', '-C', str(ROOT), 'status', '--short'], text=True)
        except subprocess.SubprocessError:
            self.report['library_commit'] = 'source archive; Git revision unavailable'
        self.save()

    def save(self):
        (self.output / 'results.json').write_text(json.dumps(self.report, indent=2) + '\n')

    def run(self, name, command, layer):
        command = list(map(str, command))
        log = self.output / (name + '.log')
        timed_out = False
        with log.open('w') as stream:
            try:
                result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, timeout=240)
                returncode = result.returncode
            except subprocess.TimeoutExpired:
                returncode = None
                timed_out = True
        self.report['checks'].append({'name': name, 'layer': layer, 'command': command,
            'log': log.name, 'returncode': returncode, 'timed_out': timed_out,
            'status': 'passed' if returncode == 0 else 'failed'})
        self.save()
        print(name + ': ' + self.report['checks'][-1]['status'], flush=True)
        if returncode != 0:
            raise RuntimeError('Check failed; inspect ' + str(log))

    def finish(self, error=None):
        self.report['status'] = 'failed' if error else 'passed'
        if error:
            self.report['error'] = str(error)
        self.report['finished_utc'] = datetime.now(timezone.utc).isoformat()
        self.save()
        return int(error is not None)
