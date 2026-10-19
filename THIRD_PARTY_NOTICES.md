# rtos-stdcpp source notices and dependency ownership

This independently maintained project began as Piotr Grygorczuk's
[FreeRTOS_cpp11](https://github.com/grygorek/FreeRTOS_cpp11), based on commit
`4805aaf62744926d7e6200e5ff1852ce27841383`. Its maintained repository is
[stevenoonan/rtos-stdcpp](https://github.com/stevenoonan/rtos-stdcpp), on branch
`rtos-stdcpp`. Historical source URLs below identify the original import rather
than the current checkout layout. The root [LICENSE](LICENSE)
covers the project-authored code; it does not relicense third-party components.
Preserve individual copyright and permission notices when distributing sources.

## Retained components

| Component | Location | Applicable terms and provenance |
|---|---|---|
| Adapter, attributes, clocks, test/helper code and build scripts | `include/`, `src/`, `tests/`, build files and project-authored platform files | MIT. Piotr Grygorczuk's per-file notices include 2018–2025; retain their exact years/text. The original root notice is preserved in [LICENSES/FreeRTOS_cpp11-original.txt](LICENSES/FreeRTOS_cpp11-original.txt). |
| Bundled FreeRTOS 10.4.3 | `third_party/freertos-kernel/` | MIT, Copyright (C) 2020 Amazon.com, Inc. or its affiliates. Keep [kernel LICENSE](third_party/freertos-kernel/LICENSE) and source notices. This is historical standalone-test material; integrating applications supply their own qualified kernel/port. |
| Arm CMSIS and Cortex-A9 startup/system support | `platforms/qemu-vexpress-a9/cmsis/` and its Arm startup/system/memory support files | Apache-2.0. Arm Limited copyright years vary from 2009–2019; preserve each file's notice. Full text: [LICENSES/Apache-2.0.txt](LICENSES/Apache-2.0.txt). These retained source files are not currently qualified standalone builds. |

[LICENSES/inventory.json](LICENSES/inventory.json) identifies retained files,
hashes, license expressions, source revisions/URLs, and local changes. Files
without their own header use the original repository MIT notice where no
third-party origin/notice overrides it; this does not extend to the removed NXP
or Luminary inputs. New Step 2 documentation and audit metadata are MIT-licensed,
Copyright (c) 2026 rtos-stdcpp contributors. Full license-document copies retain
the copying terms printed in those documents.

No separate upstream NOTICE file was found in the inherited Arm source subset.
This index preserves the notices actually supplied; it does not assert a
specific CMSIS package tag that the inherited import did not record.

## Runtime selection and ownership

The currently qualified profile is STM32 GCC 14.3.1 / C++17. The library delivers the selected runtime sources, full license texts and
[authoritative source manifest](third_party/libstdcxx/stm32-gcc-14.3.1-cxx17/SOURCES.json):

- Source repository: `https://github.com/STMicroelectronics/gnu-tools-for-stm32`.
- Immutable commit: `b8cdff71b827478c8510c70b3a15d2b50c759255`.
- Selected implementation: `libstdc++-v3/src/c++11/mutex.cc` and
  `condition_variable.cc`, byte-identical to their recorded source URLs.
- Terms: GPL-3.0-or-later WITH GCC-exception-3.1. Copies of the full texts are
  also available here as [GPLv3](LICENSES/GPL-3.0-or-later.txt) and
  [GCC exception](LICENSES/GCC-exception-3.1.txt).
- There are no local changes to these runtime sources. They remain separate
  from the MIT adapter. The CMake object target compiles them exactly once.

The GCC exception permits eligible compiled combinations with independent
application modules; distribution of GCC source or an independent runtime
library still follows its applicable terms. It does not apply merely because
a file sits beside COPYING.RUNTIME. See the
[GCC licensing documentation](https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html).

The library owns reusable adapter changes. Each integrating application owns its selected
kernel/port, board configuration, newlib locks, ARM guards, arenas, failure policy,
and build/flash/debug checks until a separately reviewed extraction. There is no
qualified standalone kernel/toolchain profile yet. Future profiles require exact
versions, source/license manifests, and executed qualification; there is no
fallback assigning arbitrary GCC versions to a nearby snapshot.

Application-specific names, checkout layouts, build targets, and hardware-runner
details belong in the integrating application's repository. Library integration
documentation describes configurable requirements and interfaces. Transition
notes must be replaced by the standalone delivery contract before release.

## Retired components and libatomic provenance

The Step 2 audit retired `libstdc++_gcc/v10`, `v11`, and `v13`, their four sample
build scripts, and the inherited compiler workflows. The reusable root build selects only the exact audited runtime profile. The unsupported
K64F and LM3S811 samples were also removed. Retained A9/RISC-V platform source material
can inform later maintained platforms, but has no active standalone build.

All removed files remain recoverable at commit
`0dc4317a96fdbd5c8fa5211c9a387f6f0fe069ba`. Their immutable URLs, original byte
hashes, and removal reasons are in inventory.json; removal does not change the
terms of historical copies.

The two retired libatomic.c files had the following verified chain:

- `v11/libatomic.c` was introduced by upstream commit
  `b9ad2e6281d42c7e534c510fc139581a30620a41` and was unchanged since that import.
- `v13/libatomic.c` was introduced by
  `ba5245429a998df8581332c4459d86a5640e4145`. It differs from v11 only by a GCC
  diagnostic push/ignored infinite-recursion warning/pop.
- Both state Copyright (C) 2010, 2011 Free Software Foundation, Inc. and
  GPL version 3 or later. Neither grants the runtime exception. Their precise
  original GCC revision was not recorded by the import and was not established
  by this audit; directory labels are not proof of GCC-release provenance.
- Neither was compiled by the qualified STM32 profile. They are retired rather
  than relabeled or enabled. Reintroducing a fallback requires independently
  verified origin, licensing, architecture behavior, and tests.

The removed LM3S811 tree included Luminary Micro code restricted to its devices,
its Software License Agreement, and prebuilt vendor libraries. The removed NXP
sample had a BSD-3-Clause startup file but generated Code Red/NXP linker scripts
with copyright statements and no license grant in the retained import. Those
files are not covered by a new MIT declaration. They were retired with their
unsupported consumers; historical copies retain their original notices.
