#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Explicit userspace setup of the checksum-pinned Ubuntu 24.04 x86_64 QEMU."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--destination', required=True, type=Path)
args = parser.parse_args()
lock = json.loads((ROOT / 'cmake/profiles/qemu-8.2.2-ubuntu24-x86_64.json').read_text())
destination = args.destination.resolve()
# Keep downloads/build products outside the source/license inventory.
if destination == ROOT or ROOT in destination.parents:
    parser.error('Select a destination outside the library source tree')
packages = destination / 'packages'; packages.mkdir(parents=True, exist_ok=True)
sysroot = destination / 'root'; sysroot.mkdir(exist_ok=True)
for item in lock['packages']:
    package = packages / Path(item['Filename']).name
    if not package.exists():
        with urllib.request.urlopen(lock['base_url'] + item['Filename'], timeout=60) as response:
            package.write_bytes(response.read())
    if hashlib.sha256(package.read_bytes()).hexdigest() != item['SHA256']:
        raise RuntimeError('Package checksum mismatch: ' + str(package))
    subprocess.run(['dpkg-deb', '-x', str(package), str(sysroot)], check=True)
binary = sysroot / 'usr/bin/qemu-system-arm'
if hashlib.sha256(binary.read_bytes()).hexdigest() != lock['binary_sha256']:
    raise RuntimeError('QEMU binary checksum mismatch')
wrapper = destination / 'qemu-system-arm'
wrapper.write_text('#!/bin/sh\nexport LD_LIBRARY_PATH=' + shlex.quote(str(sysroot / 'usr/lib/x86_64-linux-gnu')) + '\nexec ' + shlex.quote(str(binary)) + ' "$@"\n')
wrapper.chmod(0o755)
version = subprocess.check_output([str(wrapper), '--version'], text=True).splitlines()[0]
if version != lock['version']:
    raise RuntimeError('QEMU version mismatch')
print(wrapper)
