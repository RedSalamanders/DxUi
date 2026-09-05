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
required API revision, enabled targets and dependency/toolchain fingerprint. The repository is
`https://github.com/RedSalamanders/DxUI.git`, with default branch `main`; consumers pin a tested full commit. No floating `main`, branch name, `latest`, or silently accepted
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

## Implemented build and consumption

`src/DxUi.vcxproj` produces the only archive, `DxUi.lib`. FoundationTests, ControlTests, EmbeddedTests and the
standalone EmbeddedControls executable all consume it. The supported lock target is `["DxUi"]`, API revision 2.

Run `vcpkg-install.ps1 -Platform x64` (or ARM64/All) before build. WIL and the vcpkg tool revision are pinned.
`-OutputRoot` isolates restore and build work in a consumer-owned directory. The same absolute directory, including
its trailing separator, becomes `DxUiConsumerOutputRoot`; props locate its public WIL headers and the project
reference passes it as `DxUiOutputRoot`. No application checkout is needed. Missing restore and mismatched/dirty pins
fail with actionable diagnostics. Both tracked and untracked source changes invalidate a release pin.

External consumers import `Build/DxUi.Consumer.props` after Microsoft.Cpp.props and `.targets` after
Microsoft.Cpp.targets. Set DxUiRoot, DxUiConsumerLockFile and DxUiConsumerOutputRoot before those imports.
Compile with Unicode, stdcpplatest, v145 and the matching /MDd or /MD runtime. The public example's source includes
only public DxUi headers and ordinary Windows/WIL headers; it never reaches src. Diagnostic hook availability is
fixed in Configuration.h and must not be overridden by consumers.

`build.ps1` rejects only running executables in the selected output directory and never terminates them.
`test.ps1` runs all three test executables, splitting inherited control suites into independent runs with exit-code,
SHA256, native architecture and capability-skip receipts. `gallery.ps1` generates five themed control sheets, a
supplied-device example image and an HTML index. `DxUi.EmbeddedControls.exe` opens the live toggle/slider example;
`--output image.png` renders it headlessly through an application-created WARP device.

`docs/README.md` documents consumption and all controls; root README links it and the published gallery.
`gallery.ps1 -PublishDocs` publishes reviewed sheets and a generation receipt under `docs/gallery`, leaving runtime
logs in `.build`. Docs/gallery maintenance follows [Core_Documentation](../Core/Core_Documentation.md).
The formatting workflow uses `format.ps1` and the repository `.clang-format` on pushes and PRs, returning a patch
when changes are needed. Manual `apply_changes` mode commits formatting to the selected branch with ordinary push
semantics; it never force-pushes, runs on pull_request_target, or grants fork PRs a write token. Branch protection
still applies. Token-created commits require an explicit validation run or subsequent user push for fresh CI.
Native CI installs Python for performance receipt validation. Each test invocation includes complex-UI FPS/memory,
and implementation acceptance requires matched before/after evidence under the performance contract.

RedXe still needs its first release pin and preparation/input/text/UIA bridge. RedSalamander's application migration
remains on hold. Neither application's runtime changes merely because this library builds.
