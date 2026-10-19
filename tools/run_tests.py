#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run a test process (native or emulator) with a deadline and strict results."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
sys.dont_write_bytecode = True
from test_protocol import parse


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite', required=True)
    parser.add_argument('--layer', required=True, choices=['host', 'compile_link', 'emulator', 'hardware'])
    parser.add_argument('--timeout', type=float, default=20)
    parser.add_argument('--output', type=Path)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not command or not 0 < args.timeout <= 3600:
        parser.error('Supply a command and a finite positive timeout')
    report = {'status': 'failed', 'layer': args.layer, 'command': command}
    try:
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True, start_new_session=True)
        try:
            output, _ = process.communicate(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            output, _ = process.communicate()
            report['console'] = output
            raise RuntimeError('Test process timed out')
        report['returncode'] = process.returncode
        report['console'] = output
        if process.returncode != 0:
            raise RuntimeError('Test process exited unsuccessfully: ' + str(process.returncode))
        report['report'] = parse(output, args.suite)
        report['status'] = 'passed'
    except (OSError, RuntimeError, ValueError) as error:
        report['error'] = str(error)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    sys.exit(main())
