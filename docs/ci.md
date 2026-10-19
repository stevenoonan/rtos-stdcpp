# C++17 continuous integration

**Rollout status (2026-10-19):** implementation and local validation are ready.
The owner requested keeping these changes local. No hosted run or remote configuration is claimed. Hosted execution is required before Step 7 can be
marked fully complete; server-side merge enforcement is separately pending by
the owner's explicit choice.

The library's `C++17 qualification` workflow uses only GitHub-hosted Ubuntu 24.04
x86_64 runners. Pushes, pull requests and manual dispatch execute
four host jobs (GCC/Clang, Debug/Release), two A9 jobs (Debug/Release, both ordinary
and real tick-wrap variants), then the aggregate `Required C++17 checks` job.
Every required dependency must succeed; failed, skipped, cancelled or missing
profiles fail the aggregate. There are 52 CTest entries across the matrix.

Actions are pinned to reviewed commit IDs, repository permissions are read-only,
and checkout credentials are not persisted. No untrusted library PR executes on
a self-hosted runner. Concurrent superseded runs are cancelled. Host/A9 jobs have
15/20-minute deadlines; individual tests retain their own process/CTest limits.
There is no pull_request_target workflow or required job path filter.

## Explicit setup and local reproduction

`tools/setup_ci.py` prepares checksum-pinned Ubuntu GCC/Clang/sanitizer packages
or the exact ST GCC bundle and QEMU packages. It extracts into a selected directory
without sudo and retains the compiler bundle's license notices. The official ST
bundle is checked against the existing embedded byte lock after extraction.
IDE descriptor bundles are unnecessary for command-line compiler execution.
Package download URLs/hashes are recorded in `cmake/profiles`; no moving release
or branch tip is fetched by a build. CI caches only downloaded package bytes and
verifies them again on every setup. A bad/stale cache fails verification.

Run the same commands locally, with all tools/results outside the source tree:

```sh
python3 tools/setup_ci.py --kind host --destination /absolute/path/to/host-tools
python3 tools/run_matrix.py --environment host --host-compiler both \
  --host-root /absolute/path/to/host-tools/host --output /absolute/path/to/host-results
python3 tools/setup_ci.py --kind a9 --destination /absolute/path/to/a9-tools
python3 tools/run_matrix.py --environment a9 \
  --toolchain-root /absolute/path/to/a9-tools/toolchain \
  --qemu-root /absolute/path/to/a9-tools/qemu --output /absolute/path/to/a9-results
```

`--config Debug` or `--config Release` selects one configuration; host compiler
selection accepts gcc, clang or both. Omitting selectors retains the full Step 6
matrix interface. Prepared runs use no network. The host prefix fixes compiler,
C++ headers/runtime and sanitizer packages; the surrounding Ubuntu system is not
claimed to be hermetic. CMake/Ninja and system dependencies must be available.
Reports retain compiler/source/dependency identities and the tested Git commit.

## Reports and artifacts

Artifacts contain command logs, JUnit/CTest results, structured reports, compile
commands, final image/provider evidence and failed build output. They are uploaded
on successful and failed runs, retained for seven days, and named by
profile/configuration/commit. Missing result files fail the upload step. Setup
failures remain observable in the job console even when no result directory exists.
No repository name-policy variable or source/artifact name scanning is required.

## Merge and physical-board obligations

The aggregate is the stable check to require on the maintained branch. Repository
administrators must enable required status checks/branch rules separately; a green
workflow alone does not prevent a direct push. The current private repository's
account does not provide GitHub Pro branch protection. The owner explicitly chose
to leave server-side merge enforcement pending while retaining private visibility.
No claim of enforced branch protection is made.

Physical-board builds and qualification remain the integrating application's
responsibility, against its exact library gitlink. Its runner needs the audited
compiler, signing/flashing tools, provisioned board/probe and exclusive access.
Run its sustained Debug/Release, failure/reset, tick-wrap and restored-firmware
checks for qualification; a hosted emulator pass does not discharge that duty.
The library workflow registers no self-hosted runner and requires no consumer
checkout or consumer-specific CI credential.
