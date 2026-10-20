# Continuous integration

Pushes, pull requests and manual dispatch run the C++17/C++20 matrix on GitHub-hosted
Ubuntu 24.04:

- GCC 13 and Clang 18, each in Debug and Release.
- QEMU A9 Debug and Release, each with ordinary and tick-wrap configurations.
- `Required C++ checks`, which succeeds only when all profiles succeed.

Each language mode has separate jobs and artifacts. Failed, skipped or cancelled required jobs fail the
aggregate. Server-side merge enforcement is not configured.

Host jobs install maintained Ubuntu compiler/sanitizer packages. A9 setup verifies
the exact ST compiler archive and selected QEMU packages; only downloaded archives
are cached. Builds/tests use prepared inputs without fetching dependencies.
[Testing](testing.md) gives the same commands used locally.

Actions are pinned, permissions are read-only and checkout credentials are not
persisted. Host/A9 jobs have 15/20-minute deadlines. Logs, JUnit, reports and build
artifacts are uploaded after success or failure, with seven-day retention.

Physical-board checks belong to the integrating application. Its runner needs
the qualified tools, exclusive board/probe access and its own build/flash/reset
commands. Untrusted PRs must not run on that machine. Emulator results do not
replace physical timing, interrupt, FPU, cache or resource qualification.
