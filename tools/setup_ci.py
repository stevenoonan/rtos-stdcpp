#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Explicit checksum-verified setup for the qualified hosted/local CI tools."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import stat
import sys
import urllib.request
import zipfile
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]

def fetch(url, path, checksum):
    if not path.exists():
        temporary = path.with_suffix(path.suffix + '.partial')
        with urllib.request.urlopen(url, timeout=120) as response, temporary.open('wb') as stream:
            while chunk := response.read(1024 * 1024):
                stream.write(chunk)
        temporary.replace(path)
    if hashlib.sha256(path.read_bytes()).hexdigest() != checksum:
        raise RuntimeError('Dependency checksum mismatch: ' + path.name)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kind', required=True, choices=['a9'])
    parser.add_argument('--destination', required=True, type=Path)
    args = parser.parse_args()
    destination = args.destination.resolve()
    if destination == ROOT or ROOT in destination.parents:
        parser.error('Prepared tools must stay outside the library tree')
    packages = destination / 'packages'; packages.mkdir(parents=True, exist_ok=True)
    lock = json.loads((ROOT / 'cmake/profiles/st-gcc-14.3.1-download.json').read_text())
    archive = packages / 'st-gcc.bundle'
    fetch(lock['url'], archive, lock['sha256'])
    toolchain = destination / 'toolchain'; toolchain.mkdir(exist_ok=True)
    with zipfile.ZipFile(archive) as bundle:
        for item in bundle.infolist():
            path = toolchain / item.filename
            if not path.resolve().is_relative_to(toolchain):
                raise RuntimeError('Unexpected bundle path')
            if stat.S_ISLNK(item.external_attr >> 16):
                target = bundle.read(item).decode()
                if not (path.parent / target).resolve().is_relative_to(toolchain):
                    raise RuntimeError('Unexpected bundle symlink')
                if path.is_symlink() or path.exists():
                    path.unlink()
                path.parent.mkdir(parents=True, exist_ok=True)
                path.symlink_to(target)
                continue
            bundle.extract(item, toolchain)
            if not item.is_dir():
                path.chmod((item.external_attr >> 16) & 0o777 or 0o644)
    subprocess.run([sys.executable, str(ROOT / 'tools/check_profile.py'), '--profile', 'qemu-a9-gcc-14.3.1-cxx17',
                    '--compiler', str(toolchain / 'bin/arm-none-eabi-g++'), '--c-compiler', str(toolchain / 'bin/arm-none-eabi-gcc')], check=True)
    subprocess.run([sys.executable, str(ROOT / 'tools/setup_qemu.py'), '--destination', str(destination / 'qemu')], check=True)
    print('Prepared ' + args.kind + ' dependencies at ' + str(destination))

if __name__ == '__main__':
    main()
