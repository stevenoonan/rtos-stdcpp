#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rtos-stdcpp contributors
"""Check component notices and selected runtime provenance offline."""

import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
LICENSE_TEXT_SOURCES = {
    "LICENSES/GCC-exception-3.1.txt": "third_party/libstdcxx/stm32-gcc-14.3.1-cxx17/COPYING.RUNTIME",
    "LICENSES/GPL-3.0-or-later.txt": "third_party/libstdcxx/stm32-gcc-14.3.1-cxx17/COPYING3",
}
APACHE_LICENSE_SHA256 = "b40930bbcf80744c86c46a12bc9da056641d722716c378f5659b9e555ef833e1"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check(library):
    inventory = json.loads((library / "LICENSES/components.json").read_text())
    allowed = {"MIT", "Apache-2.0", "LicenseRef-Verbatim-License-Document",
               "GPL-3.0-or-later WITH GCC-exception-3.1"}
    for component in inventory["components"]:
        require(component["license_expression"] in allowed, "Unexpected component license")
        for name in component["paths"]:
            require((library / name).exists(), "Missing licensed component: " + name)
        if "notice" in component:
            require((library / component["notice"]).is_file(), "Missing component notice")
    for name, original in LICENSE_TEXT_SOURCES.items():
        require((library / name).read_bytes() == (library / original).read_bytes(), "License copy differs: " + name)
    require(digest(library / "LICENSES/Apache-2.0.txt") == APACHE_LICENSE_SHA256,
            "Apache license text changed")
    kernel = library / "third_party/freertos-kernel"
    kernel_manifest = json.loads((kernel / "SOURCES.json").read_text())
    for item in kernel_manifest["files"]:
        require(digest(kernel / item["path"]) == item["sha256"],
                "Kernel bytes changed: " + item["path"])
    require("Permission is hereby granted" in (library / "LICENSE").read_text(), "Missing root MIT text")
    require("Copyright 2018-2023 Piotr Grygorczuk" in (library / "LICENSE").read_text(),
            "Original root copyright missing")

    runtime = library / "third_party/libstdcxx/stm32-gcc-14.3.1-cxx17"
    manifest = json.loads((runtime / "SOURCES.json").read_text())
    runtime_files = {path.relative_to(runtime).as_posix() for path in runtime.rglob("*") if path.is_file()}
    require(runtime_files == {item["path"] for item in manifest["files"]} | {"SOURCES.json"},
            "Runtime directory differs from the authoritative source selection")
    require(manifest["profile"] == "stm32-gcc-14.3.1-cxx17", "Unexpected qualified runtime profile")
    selected = [item for item in manifest["files"] if item["path"].endswith(".cc")]
    require({item["path"] for item in selected} ==
            {"libstdc++-v3/src/c++11/mutex.cc", "libstdc++-v3/src/c++11/condition_variable.cc",
             "libstdc++-v3/src/c++11/future.cc"},
            "Qualified runtime implementation selection changed")
    for item in manifest["files"]:
        require(digest(runtime / item["path"]) == item["sha256"], "Runtime bytes changed: " + item["path"])
        require(item["local_modifications"] == [], "Unexpected local runtime modification")
        require(manifest["commit"] in item["url"], "Runtime URL is not pinned to the recorded commit")
    for item in selected:
        require(item["license_expression"] == "GPL-3.0-or-later WITH GCC-exception-3.1",
                "Runtime source incorrectly licensed: " + item["path"])
        require("GCC Runtime Library Exception" in (runtime / item["path"]).read_text(),
                "Runtime exception file notice missing")

    override = library / "third_party/libstdcxx/stm32-gcc-14.3.1-cxx20"
    selected = json.loads((override / "SOURCES.json").read_text())
    for item in selected["files"]:
        require(digest(override / item["path"]) == item["sha256"], "C++20 runtime bytes changed")
        require(item["license_expression"] == "GPL-3.0-or-later WITH GCC-exception-3.1", "Wrong C++20 runtime license")
        require("GCC Runtime Library Exception" in (override / item["path"]).read_text(), "Missing C++20 runtime notice")
        require(bool(item["local_modifications"]), "Missing C++20 runtime modification record")
    original = (library / "LICENSES/FreeRTOS_cpp11-original.txt").read_text()
    require("Copyright 2018-2023 Piotr Grygorczuk" in original and
            "Permission is hereby granted" in original, "Original MIT notice missing")
    return {"status": "passed", "licensed_components": len(inventory["components"]),
            "qualified_runtime_profile": manifest["profile"],
            "original_notice_preserved": True, "base_runtime_sources_unmodified": True, "cxx20_override_verified": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        report = check(args.library.resolve())
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        print(f"License/provenance check failed: {error}", file=sys.stderr)
        return 1
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
