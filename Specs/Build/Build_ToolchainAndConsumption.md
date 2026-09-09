# Toolchain and consumer integration

Status: normative intended contract
Last reviewed: 2026-09-06

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

The required matrix is Windows 10/11 for the embedded baseline, x64 and ARM64, Debug/Release/ASan Debug, Unicode, the current
shared VS 2026/v145/Windows SDK baseline and `stdcpplatest`. Exact SDK, dependency versions and supported capabilities
are recorded once in machine-owned build/lock files. Windows-11-only backdrop features remain optional WindowHost
capabilities; they must not introduce unconditional imports that break the RedXe Windows 10 baseline. OS-specific
text and accessibility paths require actual verification. All library, sample, test and generated consumer projects
map each of the six configurations explicitly. ASan Debug uses compiler AddressSanitizer instrumentation, the debug
DLL CRT, non-incremental linking and separate configuration outputs. Every standalone translation unit rejects
missing instrumentation. `test.ps1` requires a specific heap-use-after-free diagnosis from an isolated intentional
fault before accepting an ASAN run; ordinary crashes or an unsanitized Debug executable fail that probe. MSBuild
stages the matching sanitizer runtime next to executables. Native ARM64 execution is required for runtime evidence;
cross-compilation from x64 is supported by [MSVC ARM64 ASAN](https://devblogs.microsoft.com/cppblog/introducing-msvc-addresssanitizer-for-arm64-targets/).

Each consumer adds a machine-readable `Dependencies/DxUi.lock.json` with source repository identity, exact commit,
required API revision, enabled targets and dependency/toolchain fingerprint. The repository is
`https://github.com/RedSalamanders/DxUi.git`, with default branch `main`; consumers pin a tested full commit. No floating `main`, branch name, `latest`, or silently accepted
dirty sibling checkout is a release dependency.

`DxUiRoot` defaults to a sibling checkout and supports an explicit absolute override. A consumer restore entrypoint
can populate an isolated pinned checkout for CI using its configured repository identity. It never resets, checks
out, cleans or overwrites the developer's existing sibling checkout. A missing root or mismatched revision fails
with an actionable message. An explicit development override may use edited source, but must record its fingerprint
and mark the result non-release; clean release validation requires the lock match.

Consumers import `Build/DxUi.Consumer.props` / `.targets` and reference `src/DxUi.vcxproj`. They do
not maintain a second list of library `.cpp` files. Public header paths and output paths resolve from the imported
file/project, never an assumed application `SolutionDir`. Standalone `.build` outputs and consumer dependency outputs
are separate. Consumer outputs use `.build/dependencies/DxUi/<fingerprint>/<platform>/<configuration>/` beneath that
consumer; the fingerprint includes source, toolchain, CRT and build flags. Parallel builds of both applications must
not share writable intermediates or vcpkg work trees.

Pin WIL and any other extracted dependency with the library; no implicit reuse of whatever headers are on the
developer's machine. Build validation detects conflicting public-header dependencies and incompatible CRT/STL flags.
Shipping static libraries means there is no `DxUi.dll` to stage; the existing `AVControl.dll` remains dynamically
loaded by RedXe as a plugin. Windows/system runtime dependencies and notices still require normal packaging checks.

Library CI builds and tests without either application checkout. RedXe restores its pinned revision and runs
its integration tests; `redxe-adapter` records that pin and the synthetic COM/POD adapters, not real IME/AT. A library update changes the consumer lock in a reviewed change with test evidence and a
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

`build.ps1` presents a DxUi banner and color-classifies streamed MSBuild diagnostics in interactive terminals while
preserving plain redirected output and a complete file log. It rejects only running executables in the selected
output directory and never terminates them.
`test.ps1` runs all three test executables, splitting inherited control suites into independent runs with exit-code,
SHA256, native architecture and capability-skip receipts. `gallery.ps1` generates five themed control sheets, a
supplied-device example image and an HTML index. `DxUi.EmbeddedControls.exe` opens the live toggle/slider example;
`--output image.png` renders it headlessly through a sample-created WARP device. `--complex-ui` selects the independent
83-control/1,000-row scene shared with the benchmark. External-consumer validation copies both sample directories
and renders both modes; neither sample requires an application checkout or application services.
`test-consumer.ps1 -FixtureRoot <short-directory>` places its unique relocated fixture under a caller-selected base
when a deeply nested checkout would exceed Windows/vcpkg path limits. It does not replace or clean existing fixtures.

`docs/README.md` documents consumption and all controls; root README links it and the published gallery.
`gallery.ps1 -PublishDocs` publishes reviewed sheets and a generation receipt under `docs/gallery`, leaving runtime
logs in `.build`. Docs/gallery maintenance follows [Core_Documentation](../Core/Core_Documentation.md).
The formatting workflow uses `format.ps1` and the repository `.clang-format` on pushes and PRs, returning a patch
when changes are needed. Formatting is pinned to clang-format 22.1.3; CI installs the Windows x64 wheel using the
version and SHA256 in `Tools/requirements-format.txt`. `format.ps1` checks the version and accepts `-FormatterPath`
or `DXUI_CLANG_FORMAT` before PATH/Visual Studio discovery, so a runner image update cannot silently change output.
Manual `apply_changes` mode commits formatting to the selected branch with ordinary push
semantics; it never force-pushes, runs on pull_request_target, or grants fork PRs a write token. Branch protection
still applies. Token-created commits require an explicit validation run or subsequent user push for fresh CI.
Native CI installs Python for performance receipt validation. Each test invocation includes complex-UI FPS/memory,
and implementation acceptance requires matched before/after evidence under the performance contract.

RedXe already has an exact pin and synthetic preparation/input/text/UIA adapters; real IME/AT acceptance stays
with its AV owner. RedSalamander adoption is active under its I19 consumer plan. Neither application's runtime
changes merely because this library builds.

Managed vcpkg clones enable Git long-path support in their own repository configuration so deeply relocated
consumer fixtures can check out tool sources. No user/global Git configuration is changed.

Public consumer helper headers are `DxUi/Typography.h`, `DxUi/FocusRestore.h`,
`DxUi/PointerInput.h`, `DxUi/AccessibilityTextUnits.h`, and `DxUi/NativeMenuInterop.h`.
They preserve neutral font, focus, pointer, Unicode-unit and HMENU bridging policies. Application
commands, theme conversion, resource loading and window-subclass policy stay in consumer adapters.
Private compatibility includes inside src only forward to the canonical public headers. Consumers
must not include src paths. The relocated fixture compiles each helper header independently.
Native menu host function bodies remain in `src/Controls/NativeMenuInterop.cpp` and link from the archive;
the public header contains declarations and layout, with no extra wrapper allocation.
Private diagnostics, secure wiping, posted payloads and window-message names remain in the DxUi
namespace so static consumers cannot coalesce their own same-named application helper definitions.

`DxUiDisableStlAnnotations=true` is the explicit ASAN compatibility setting for consumers that
link ordinary prebuilt C++ dependencies (currently RedSalamander). It defines `_DISABLE_STL_ANNOTATION`
in both application and archive translation units and disables only STL container-overflow annotations;
heap/stack instrumentation and the required detection probe remain active. The default is false.
The consumer output fingerprint includes this setting. The relocated ASAN fixture exercises both
policies; never mix their artifacts or silently substitute ordinary Debug for ASan Debug.
`Get-DxUiConsumerBuildIdentity` in `Tools/ConsumerBuild.psm1` evaluates the selected MSBuild toolchain
before compilation. Its fingerprint includes the exact source/API, target platform, compiler host,
toolset, compiler/linker/MSBuild hashes, SDK version and Windows/header/import-library hashes, debug/release
DLL-CRT family and STL annotation policy. Configuration remains a separate output directory below it.
Consumers record the identity beside their restored props and supply the expected toolset/compiler/SDK/host
properties; mismatches fail before compilation. A different compiler installation or ASAN container policy
cannot reuse the same writable archive output directory. Pin validation remains a separate clean-source check.

ASAN outputs stage the selected MSVC toolset's architecture-matching runtime beside the executable.
`Build/DxUi.AddressSanitizer.targets` supplies ARM64 staging omitted by MSBuild's built-in x86/x64
copy target and fails if that runtime is missing. The standalone archive stages once for its common
output directory; external consumers stage for their own output. Synthetic MSBuild staging tests
do not replace the required native sanitizer detection probe and regression suite.

## Advisory updates

Consumers call `Show-DxUiUpdateNotice -LockFile <exact-lock>` from `Tools/ConsumerUpdate.psm1`
once in their root restore/build invocation. It reads main, verifies ancestry, and recommends only
the exact main commit whose latest push run of `ci.yml` completed successfully. Same-pin builds are
quiet; pending/failed/unavailable validation keeps the pin selected; divergent/ahead pins require review.
The notice includes the comparison URL and lock to update. It is plain build output, never an MSBuild
warning, automatic edit, PR comment, push, or scheduling service. Private read access uses existing
GH_TOKEN/GITHUB_TOKEN or a bounded local gh credential lookup. Each of at most three API requests has
a two-second timeout; credential lookup has a two-second timeout. Lookup failures are explicitly
unavailable and cannot fail an otherwise valid fixed-pin build. Authentication details are never logged.
The consumer still validates/restores its exact source pin independently and fails on a real pin error.
