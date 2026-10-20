# Licensing and dependencies

The [component notices](../THIRD_PARTY_NOTICES.md) and
[index](../LICENSES/components.json) identify the code currently delivered.
Project code uses the root [MIT license](../LICENSE); FreeRTOS, Arm and GCC files
retain their own terms and notices. Include the applicable license texts and
copyright notices when distributing sources. The GCC exception's terms are in
[COPYING.RUNTIME](../third_party/libstdcxx/stm32-gcc-14.3.1-cxx17/COPYING.RUNTIME).

Source provenance belongs in the dependency manifests:

- [Selected GCC runtime](../third_party/libstdcxx/stm32-gcc-14.3.1-cxx17/SOURCES.json).
- [Bundled kernel](../third_party/freertos-kernel/SOURCES.json) and
  [A9 kernel selection](../cmake/profiles/qemu-a9-kernel.json).
- [ST compiler archive](../cmake/profiles/st-gcc-14.3.1-download.json) and
  [QEMU packages](../cmake/profiles/qemu-8.2.2-ubuntu24-x86_64.json).

To update a dependency, select an immutable revision, review its license and
source changes, then check native ABI, headers, multilib and runtime providers.
Update the affected manifests/notices and run the required
[qualification checks](testing.md). Applications must also validate their own
firmware and hardware integration. Preserve original file notices and record any
local changes to vendor code.

Git tracks project-owned files. Only external dependency identities need
checksums; ordinary code/documentation edits do not require checksum inventories.
