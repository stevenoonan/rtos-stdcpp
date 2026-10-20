#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Execute one resettable A9 image; require a strict report or armed failure."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
sys.dont_write_bytecode = True
from test_protocol import parse

FAILURES = {'throwing_new': 89, 'kernel_alloc': 87, 'joinable': 89, 'recursive_guard': 86}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', required=True)
    parser.add_argument('--image', required=True)
    kind = parser.add_mutually_exclusive_group(required=True)
    kind.add_argument('--suite', choices=['cxx17', 'cxx20', 'a9', 'regression'])
    kind.add_argument('--failure', choices=FAILURES)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    command = [args.qemu, '-M', 'vexpress-a9', '-cpu', 'cortex-a9', '-smp', '1', '-m', '128M',
               '-nographic', '-monitor', 'none', '-nic', 'none', '-semihosting-config', 'enable=on,target=native', '-kernel', args.image]
    report = {'status': 'failed', 'layer': 'emulator', 'command': command}
    env = os.environ.copy(); env['QEMU_AUDIO_DRV'] = 'none'
    try:
        process = subprocess.Popen(command, env=env, text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        try:
            console, _ = process.communicate(timeout=25)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            console, _ = process.communicate()
            report['console'] = console
            raise RuntimeError('Emulator timed out')
        report.update(console=console, returncode=process.returncode)
        if args.failure:
            marker = 'RTOS_STDCPP_ARMED ' + args.failure
            if process.returncode != FAILURES[args.failure] or console.splitlines().count(marker) != 1:
                raise RuntimeError('Expected armed failure did not occur')
            report['expected_failure'] = args.failure
        else:
            if process.returncode != 0:
                raise RuntimeError('Emulator exited unsuccessfully')
            report['report'] = parse(console, args.suite)
        report['status'] = 'passed'
    except (OSError, ValueError, RuntimeError) as error:
        report['error'] = str(error)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return int(report['status'] != 'passed')

if __name__ == '__main__':
    sys.exit(main())
