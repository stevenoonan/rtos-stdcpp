#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Require an armed ASan diagnostic, rather than accepting an arbitrary crash."""
import argparse
import os
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--probe', required=True)
args = parser.parse_args()
env = os.environ.copy()
env['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1:exitcode=91'
result = subprocess.run([args.probe], env=env, text=True, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, timeout=10)
if (result.returncode != 91
        or result.stdout.splitlines().count('RTOS_STDCPP_ARMED asan_use_after_free') != 1
        or 'ERROR: AddressSanitizer: heap-use-after-free' not in result.stdout):
    raise SystemExit('ASan negative control failed:\n' + result.stdout)
print('ASan detected the deliberately armed heap use-after-free')
