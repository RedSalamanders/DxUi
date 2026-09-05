# Toolchain and consumer integration

Status: normative intended contract
Last reviewed: 2026-09-05

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

The initial matrix is Windows 10/11 for the embedded baseline, x64 and ARM64, Debug and Release, Unicode, the current
shared VS 2026/v145/Windows SDK baseline and `stdcpplatest`. Exact SDK, dependency versions and supported capabilities
are recorded once in machine-owned build/lock files. Windows-11-only backdrop features remain optional WindowHost
capabilities; they must not introduce unconditional imports that break the RedXe Windows 10 baseline. OS-specific
text and accessibility paths require actual verification. Preserve existing ASan configurations where supported as
additional tests, not replacements for the four required builds.

Each consumer adds a machine-readable `Dependencies/DxUi.lock.json` with source repository identity, exact commit,
required API revision, enabled targets and dependency/toolchain fingerprint. The actual remote and first commit are
filled in during bootstrap; this RFC invents neither. No floating `main`, branch name, `latest`, or silently accepted
dirty sibling checkout is a release dependency.

`DxUiRoot` defaults to a sibling checkout and supports an explicit absolute override. A consumer restore entrypoint
can populate an isolated pinned checkout for CI using its configured repository identity. It never resets, checks
out, cleans or overwrites the developer's existing sibling checkout. A missing root or mismatched revision fails
with an actionable message. An explicit development override may use edited source, but must record its fingerprint
and mark the result non-release; clean release validation requires the lock match.

Consumers import `Build/DxUi.Consumer.props` / `.targets` and reference the selected DxUi `.vcxproj` targets. They do
not maintain a second list of library `.cpp` files. Public header paths and output paths resolve from the imported
file/project, never an assumed application `SolutionDir`. Standalone `.build` outputs and consumer dependency outputs
are separate. Consumer outputs use `.build/dependencies/DxUi/<fingerprint>/<platform>/<configuration>/` beneath that
consumer; the fingerprint includes source, toolchain, CRT and build flags. Parallel builds of both applications must
not share writable intermediates or vcpkg work trees.

Pin WIL and any other extracted dependency with the library; no implicit reuse of whatever headers are on the
developer's machine. Build validation detects conflicting public-header dependencies and incompatible CRT/STL flags.
Shipping static libraries means there is no `DxUi.dll` to stage; the existing `AVControl.dll` remains dynamically
loaded by RedXe as a plugin. Windows/system runtime dependencies and notices still require normal packaging checks.

Library CI builds and tests without either application checkout. RedXe CI restores its pinned revision and runs
its integration tests. A library update changes the consumer lock in a reviewed change with test evidence and a
rollback to the previous pin. RedSalamander may stay on the old in-tree implementation and later its own pin; shared
source ownership does not require simultaneous releases of the two applications.

## Bootstrap implementation

Only `DxUi.Foundation.lib` is currently built. It has no WIL dependency and does not advertise control or embedded
support. `DxUi.Controls`, `DxUi.Embedded`, `DxUi.Win32Services` and `DxUi.Win32Host` are the required later targets.
`build.ps1` builds the foundation solution; it accepts an explicit output root and chooses independent intermediates.
The current consumer props/targets accept only the Foundation target and verify a full revision lock before build.
No RedXe runtime uses the library yet. Consumer lock creation waits for the first tested integration revision.
