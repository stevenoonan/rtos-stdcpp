# Licensing and dependencies

The [component notices](../THIRD_PARTY_NOTICES.md) and
[index](../LICENSES/components.json) identify the code currently delivered.
Project code uses the root [MIT license](../LICENSE); FreeRTOS, Arm and GCC files
retain their own terms and notices. Include the applicable license texts and
copyright notices when distributing sources. The GCC exception's terms are in
[COPYING.RUNTIME](../third_party/libstdcxx/stm32-gcc-14.3.1-cxx17/COPYING.RUNTIME).

Both backend implementations live in this library. The bundled kernel is
FreeRTOS for the standalone A9 platform; ThreadX kernel sources are supplied by
the consumer. Record and distribute the selected external kernel's applicable
source/license notices with the integrating firmware.

Dependency manifests record source origins and licenses:

- [Selected GCC runtime](../third_party/libstdcxx/stm32-gcc-14.3.1-cxx17/SOURCES.json).
- [Modified C++20 stop-token header](../third_party/libstdcxx/stm32-gcc-14.3.1-cxx20/SOURCES.json), under GPLv3 with GCC Runtime Library Exception; preserve its modification record.
- [Bundled kernel](../third_party/freertos-kernel/SOURCES.json).
- [ST compiler archive](../cmake/profiles/st-gcc-14.3.1-download.json) and
  [QEMU packages](../cmake/profiles/qemu-8.2.2-ubuntu24-x86_64.json).

To update a dependency, select an immutable revision, review its license and
source changes, then check native ABI, headers, multilib and runtime providers.
Update the affected manifests/notices and run the required
[qualification checks](testing.md). Applications must also validate their own
firmware and hardware integration. Preserve original file notices and record any
local changes to vendor code.

Git commits and tags identify the source tree. Ordinary builds do not verify
compiler or source-file checksums, and local edits do not require checksum
inventory updates. The explicit tool setup scripts verify downloaded archives
and packages before extraction.

Run `python3 tools/check_licenses.py` from the library root to verify the delivered
license texts and recorded FreeRTOS/GCC source hashes offline. The checker uses
only files in this repository. The isolated [release checks](testing.md#release-checks)
include modified-dependency fixtures to verify failures are detected.
