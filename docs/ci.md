# C++17 continuous integration

Server-side merge enforcement remains pending by the owner's explicit choice.
The workflow provides a stable aggregate check for future branch rules.

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

Host CI installs supported GCC 13, Clang 18 and UBSan packages from Ubuntu
24.04 repositories. Distro patch updates are allowed; executable and system
library hashes are not locked. Clang selects GCC 13's C++ headers/runtime.
Reports record compiler versions and the Git commit/working-tree state.

For A9, `tools/setup_ci.py` extracts the exact checksum-verified ST GCC bundle and
QEMU packages outside the source tree. It preserves bundle permissions, symlinks
and license texts, then validates the ABI-sensitive embedded profile. CI caches
only downloaded archives/packages and verifies them again during setup. The ST
IDE descriptor bundle is unnecessary. System runtime dependencies use maintained
Ubuntu packages rather than individual shared-library byte locks.

On Ubuntu 24.04, install prerequisites explicitly, then use the same runner as CI:

```sh
sudo apt-get update
sudo apt-get install -y gcc-13 g++-13 clang-18 libclang-rt-18-dev cmake ninja-build qemu-system-arm
python3 tools/run_matrix.py --environment host --host-compiler both \
  --output /absolute/path/to/host-results
python3 tools/setup_ci.py --kind a9 --destination /absolute/path/to/a9-tools
python3 tools/run_matrix.py --environment a9 \
  --toolchain-root /absolute/path/to/a9-tools/toolchain \
  --qemu-root /absolute/path/to/a9-tools/qemu --output /absolute/path/to/a9-results
```

`--config Debug` or `--config Release` selects one configuration; host compiler
selection accepts gcc, clang or both. Omitting environment selectors retains the
full host/A9 interface (`--clang` also includes Clang). Tool/result directories
must stay outside the source tree. After explicit dependency preparation, matrix
execution uses no network. The Ubuntu environment is not a hermetic OS image.

## Reports and artifacts

Artifacts contain command logs, JUnit/CTest results, structured reports, compile
commands, final image/provider evidence and failed build output. They are uploaded
on successful and failed runs, retained for seven days, and named by
profile/configuration/commit. Missing result files fail the upload step. Setup
failures remain observable in the job console even when no result directory exists.

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
