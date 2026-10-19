# Licensing, provenance and dependency updates

rtos-stdcpp is maintained independently from Piotr Grygorczuk's FreeRTOS_cpp11.
The upstream base is `4805aaf62744926d7e6200e5ff1852ce27841383`; the pre-reorganization
import is `0dc4317a96fdbd5c8fa5211c9a387f6f0fe069ba`. The maintained repository is
[stevenoonan/rtos-stdcpp](https://github.com/stevenoonan/rtos-stdcpp), branch
`rtos-stdcpp`. Original authorship and per-file copyright years remain intact.

## Component records

| Component | License and authoritative record |
|---|---|
| Adapter, extensions, tests and project-authored tooling/docs | [MIT](../LICENSE); retain the [original full notice](../LICENSES/FreeRTOS_cpp11-original.txt) and file notices |
| Bundled kernel | MIT and per-file notices; [kernel license](../third_party/freertos-kernel/LICENSE) and [import manifest](../third_party/freertos-kernel/SOURCES.json) |
| Arm CMSIS/A9 support | Apache-2.0 with individual Arm notices; [full license](../LICENSES/Apache-2.0.txt) |
| Selected GCC runtime sources | GPL-3.0-or-later WITH GCC-exception-3.1; [SOURCES.json](../third_party/libstdcxx/stm32-gcc-14.3.1-cxx17/SOURCES.json), `COPYING3`, `COPYING.RUNTIME` and source-file notices |

The [component index](../LICENSES/inventory.json) and
[third-party notices](../THIRD_PARTY_NOTICES.md) define scope. The root MIT license
does not relicense GCC, Arm or other vendor material. Preserve the applicable
license texts and notices with distributions; the GCC exception's actual terms
are in its full text. Private hosting does not change component licenses.
The separate integrating application records its own kernel/port and board
component notices.

Only `mutex.cc` and `condition_variable.cc` are selected from ST's
`gnu-tools-for-stm32` source commit `b8cdff71b827478c8510c70b3a15d2b50c759255`.
They have no local modifications and are shared by both embedded profiles.
There is no selected future runtime or libatomic fallback. The retired
libatomic sources did not grant the runtime exception; see the retained audit
summary in the notice index before considering any reintroduction.

The kernel import manifest records the inherited snapshot, not an invented
upstream kernel SHA. The [A9 kernel selection](../cmake/profiles/qemu-a9-kernel.json)
checks that snapshot. The [ST bundle lock](../cmake/profiles/st-gcc-14.3.1-download.json)
and [QEMU lock](../cmake/profiles/qemu-8.2.2-ubuntu24-x86_64.json) identify explicitly
downloaded tools. These dependency checks remain separate from project code.

## Maintaining the records

Ordinary edits to project code/docs use Git review and meaningful tests. Do not
create a per-file checksum inventory or refresh historical audit reports. The
component index's historical-audit reference preserves the detailed import and
retired-file evidence in Git; it is not a source-tree freeze.

For a dependency change:

1. Select an immutable source revision or official release archive. Review the
   actual file licenses, notices, origin and any upstream/local changes.
2. Review ABI/header/multilib and runtime-provider compatibility. A nearby
   version label is not a compatible profile by itself.
3. Update the affected dependency source/download manifests and notice records.
   Preserve original notices and record local modifications explicitly. If a
   dependency source changes, inspect that change before replacing its checksum.
4. Run the [profile qualification](support-matrix.md#changing-or-adding-a-profile)
   and [test commands](testing.md), including final provider and failure checks.
   Record the tested commit/results and any limits; do not claim support from a
   compile-only run.
5. An application updates its exact library gitlink after reviewing those
   results, then repeats its own firmware/hardware acceptance as appropriate.
   Retain the previous accepted revision for rollback.

Host distro patch updates at GCC 13 / Clang 18 use maintained Ubuntu packages,
not executable/shared-library hash locks. Embedded ABI inputs and explicitly
downloaded compiler/emulator archives retain targeted checksums.
