#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify the exact embedded toolchain and selected runtime bytes, offline."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PROFILE = "stm32-gcc-14.3.1-cxx17"


def verify(base, items):
    for item in items:
        path = base / item["path"]
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != item["sha256"]:
            raise RuntimeError("Audited dependency differs or is missing: " + str(path))


def load_profile(name):
    lock = json.loads((ROOT / "cmake/profiles" / (name + ".json")).read_text())
    if "extends" in lock:
        base = json.loads((ROOT / "cmake/profiles" / (lock["extends"] + ".json")).read_text())
        base.setdefault("runtime_profile", lock["extends"])
        lock = {**base, **lock}
    lock.setdefault("language_standard", 17)
    return lock


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--c-compiler", required=True)
    parser.add_argument("--profile", default=PROFILE, choices=[PROFILE, "qemu-a9-gcc-14.3.1-cxx17", "stm32-gcc-14.3.1-cxx20", "qemu-a9-gcc-14.3.1-cxx20"])
    args = parser.parse_args()
    try:
        lock = load_profile(args.profile)
        cxx = Path(shutil.which(args.compiler) or args.compiler).resolve()
        cc = Path(shutil.which(args.c_compiler) or args.c_compiler).resolve()
        toolchain = cxx.parent.parent
        if cc != (toolchain / "bin/arm-none-eabi-gcc").resolve():
            raise RuntimeError("C and C++ compilers must belong to the same audited toolchain")
        version = subprocess.check_output([cxx, "--version"], text=True).splitlines()[0]
        if version != lock["compiler_id"]:
            raise RuntimeError("Compiler identity does not match the selected profile")
        verify(toolchain, lock["files"])
        multilib = subprocess.check_output([cxx, *lock["cpu_flags"], "-print-multi-directory"], text=True).strip()
        if multilib != lock["multilib"]:
            raise RuntimeError("Compiler multilib does not match the selected profile")
        runtime_profile = lock.get("runtime_profile", args.profile)
        runtime = ROOT / "third_party/libstdcxx" / runtime_profile
        manifest = json.loads((runtime / "SOURCES.json").read_text())
        if manifest["profile"] != runtime_profile:
            raise RuntimeError("Runtime source manifest selects another profile")
        verify(runtime, manifest["files"])
        if "header_override" in lock:
            override = ROOT / lock["header_override"]
            selected = json.loads((override / "SOURCES.json").read_text())["files"]
            verify(override, selected)
            source = toolchain / "arm-none-eabi/include/c++/14.3.1/stop_token"
            if hashlib.sha256(source.read_bytes()).hexdigest() != selected[0]["source_sha256"]:
                raise RuntimeError("Installed GCC stop_token differs from the selected source")
        print(f"rtos-stdcpp exact GCC 14.3.1 / newlib-nano / C++{lock['language_standard']} profile verified offline")
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
