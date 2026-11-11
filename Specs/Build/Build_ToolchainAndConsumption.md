# Toolchain and consumer integration

## Pinned engine inputs and paired diagnostic readers

Local source/build/runtime identities bind the actual clean `.ci-engine` at the
checkout's exact `.ci/project.json` pin through the shared source-input reader.
Keep engine identity outside product file/iteration maps; generated engine `.build`
output does not participate. Missing, dirty, foreign or wrong-pin inputs refuse reuse.

Paired diagnostics may pass an explicit `ToolingEngineRoot` from their common current
harness to `build.ps1` and `performance.ps1`. Historical sides retain their original
manifest and actual engine pin. The current reader is a separate independent clean
checkout at its own owner's exact pin; both raw engine identities enter build and
measurement identity before and after execution. External-reader keys differ from
the default qualification key. Production-only and external-source SDK builds reject
this diagnostic reader option before discovery or mutation.

Pair preparation restores only an absent side engine from the local clean harness
checkout, then checks out the side's original exact pin with Git long paths enabled.
Existing engines are verified and preserved; no dirty or wrong-pin checkout is reset.
Pre-CI historical sides acquire no engine. Freeze both engine identities across side
preparation, build, SkipBuild/preflight and every measurement. Record them per side;
changed inputs refuse the study. Never copy current reader code into an older engine
or advance a historical pin to make the comparison work. This supplies diagnostic
input provenance, not native/SDK/Full acceptance.

## Selected native target builds

`build.ps1 -Projects DxUi.FoundationTests,DxUi.EmbeddedTests` selects one owned project union instead of
the full solution. Project names use their catalog spelling; unknown names refuse. The union always includes
the static library. Evaluation checks the actual project references, output paths and target roles for the
selected profile, including imported/conditional references; a new or opaque dependency requires a reviewed
closure update before partial admission. The current owned targets have no app-local plugin/data deployment
beyond their executables, symbols, static library and ASan runtime. New runtime assets must extend the catalog,
staging and negative controls together. System graphics/Windows DLLs retain their environment/native gates.

Affected execution maps Foundation to its executable, Embedded and ordinary performance to EmbeddedTests,
other scopes to ControlTests, foreground/private lease scenarios to InteractiveLease, and ASan's failure probe
to FoundationTests. The EmbeddedTests benchmark is required even when it is not an explicitly selected suite.
Full and PrePush retain a full solution build request. Explicit direct test suite calls may build their union;
`test.ps1 -Full` remains full. Unknown source input still widens the affected plan conservatively.

Default-output selected test builds that include the benchmark retain canonical v2 source/toolchain/artifact
and intermediate verification. Stale inputs or unverified objects force Rebuild of the selected union; a warm
Build is allowed only with exact existing final and intermediate proof. Selected receipts bind the actual
target set, complete current compiled/dependency inputs and profile artifact bytes. They require every selected
EXE/PDB, the library and the selected ASan runtime. An exact full proof can cover a subset, and a proved selected
superset can cover a subset. A selected proof never grants a full build request. Missing assets, changed
targets/source/dependencies/outputs or failed builds cannot inherit an old success. Partial operations revoke
full build proof and publish no solution-wide receipt. Custom output roots and selections lacking the benchmark
remain diagnostic builds under the existing consumer-output contract and do not establish test-run attestation.

Build/test operations coordinate by resolved output root plus architecture/configuration, including nested
calls. Other profiles and output roots remain independent. Foreign output processes are still diagnosed and
never terminated. Failed default-output builds revoke full/selected build proof; failures before compilation
also cannot retain an affected no-work baseline. Existing package/production and native architecture gates remain.

Status: normative intended contract
Last reviewed: 2026-11-09

Failed relocated-consumer qualification retains its original compiler/linker logs
under the owning repository's `.build/reports`, where hosted workspace evidence
collects them. Before returning failure, preserve a non-overwriting, independently
member-verified archive of generated consumer projects/sources and its native
images/PDBs/dumps. Exclude the relocated SDK checkout, Git history and dependency
caches. Collection is bounded to 8,192 files and 2 GiB of source bytes. An archive
failure reports an additional diagnostic and leaves both original artifacts and
the original qualification failure intact; it never changes failure into success.

## Pinned-source SDK inputs

The current producer may build a separate clean immutable DxUi source with
`build.ps1 -Configuration Release -ProductionOnly -SourceRoot <source>`. The source checkout's solution,
default build outputs and dependency pins remain authoritative for its native bytes; build modules and guards
come from the invoking producer checkout. The feature supports only that Release library role, never custom
outputs, dirty source, selected unions or Full/test proof. The existing profile lease, owned-process/output
checks, complete before/after v2 identities and zero-warning/error acceptance still apply.

An explicit local developer build may instead add `-LocalUnpinnedSource` for an
actually dirty separate `SourceRoot`. It retains Release/ProductionOnly, default
producer outputs, reviewed source/API recipes, native input barriers, profile
leases and zero-warning diagnostics. Hosted execution, a missing SourceRoot and
clean source with this flag refuse before output discovery. The local route
records its exact dirty library inputs, revokes any earlier pinned-source build
descriptor and writes no pinned-source replacement. The dirty receipt can feed
`Tools/New-LocalSdkSnapshot.ps1`; ordinary pinned SDK packaging still rejects
dirty source. No native test, Full, application or production result is implied.

The producer tooling must itself be clean and committed. A separate input descriptor binds its repository,
commit and complete content identity, exact library record digest, actual command identity and original build
log hash. Any source-build failure revokes both records. SDK packaging with separate source requires this
descriptor, matching clean producer tooling and all normal SDK extraction, license, header, symbol and warning
negative controls. Recompute the producer identity after build and package operations. A local JSON descriptor
alone establishes no hosted producer trust, native suite pass or publication acceptance.

Only the exact API-4 pins 4cdf1ce52c7785446698e346b8aec058ed6a9283 and
f2e328a0a26a18ca66bf4b2aea77bf010c9515e1 have historical packaging recipes. The former predates SDK samples
and links/checks the public frame runtime with a current reviewed compatibility sample; this establishes its
bounded smoke, not later RichText/drag/SVG behavior. The latter retains its own richer source-owned sample.
Their exact original dependency-license sets are required; missing required files never select a smaller set.
API-5 source retains both FailureHost files and their independent header-closure checks. Unsupported source/API
profiles refuse. Dependency source pins never advance as part of this packaging route.

## Native metadata acquisition

### Local unpinned Release snapshot capture

`Tools/New-LocalSdkSnapshot.ps1 -ProducerRoot <DxUi checkout> -Platform x64|ARM64`
captures an existing current Release library/PDB build; it does not compile or
qualify an SDK. ProducerRoot may be dirty only when its exact state has matching
canonical library provenance. The tooling's engine must be clean and exactly
pinned. Local dirty tooling uses the explicitly distinct
`LocalUnpinnedProducerTooling` record from `Get-DxUiSdkToolingProvenance
-LocalCandidate`; normal SDK and pinned source-build paths retain their clean,
exact three-field descriptor and refuse this local record.

The command holds the canonical producer Release profile lease through fresh
source/API, complete native tool/profile/dependency/environment and library/PDB
checks. It captures headers, restored dependency headers, library, matching
compiler PDB, native policy/imports, the complete reviewed license set, original
library provenance and the canonical actual portable ABI/toolchain reference.
The evaluated Release compiler version must match the ABI reference. Complete
input/library barriers run before staging, after capture and before descriptor
publication. Source or library mutation fails with staging retained.

`SdkPayload.psm1` owns common byte staging for this capture and `New-SdkPackage.ps1`.
Its fresh destination must be under the producer's owned `.build`; occupied,
foreign and reparse paths refuse. It verifies the independently supplied library
and PDB hashes. Its integration props preserve the preceding SDK artifact byte
for byte, covered by a frozen fixture on both platform substitutions. All ordinary
SDK extraction, smoke, profile, symbol isolation and publication gates remain.
`SdkLibraryInputs.psm1` exports its existing `Assert-DxUiSdkOwnedPath` guard for
these callers rather than introducing another ownership algorithm.

Capture publishes the complete immutable payload through the exact pinned shared
engine's separate `LocalUnpinnedSdkSnapshot` storage role. The descriptor and
diagnostic staging are create-only, under `.build/local-sdk-capture`; the default
owned store is `.build/local-sdk-snapshots`. An explicit StoreRoot also stays
under the tooling checkout's `.build`. Initialization refuses nonempty unowned
storage. The descriptor binds store/key/original manifest hash, producer identity,
local tooling identity and an independently readable bootstrap record containing
tooling root, raw source bytes and actual pinned-engine bytes. A consumer must
verify that bootstrap before importing producer tooling. The producer lease is
released before returning any result to a consumer, including a downstream
pipeline; no consumer write lease is held while awaiting producer capture.

Hosted qualification/publication refuses this command before discovery. It emits
no test, native SDK, package or qualification verdict and changes no dependency
pin or ordinary consumer output. Application candidate linking, full current-input
reacquisition, isolated local-unpinned outputs/receipts and PrePush/package refusal
remain pending. Pinned readers do not accept this descriptor. Debug, ASan and
annotation variants retain their source paths.

### Immutable SDK byte publication and use

`New-SdkPackage.ps1` optionally publishes its exact staged SDK payload through `Tools/SdkInputStore.psm1`
with `-SdkStoreRoot` and an explicit/default repository owner. Store initialization is limited to an owned
`.build` descendant with the shared ownership marker; unowned nonempty roots and reparse paths refuse.
The SDK manifest binds its independently acquired strict input identity, source/API/profile and actual
library/compiler-PDB hashes. Every existing extracted native smoke, source/header/license, PDB isolation,
missing-PDB and warning control must pass before publication, and complete input identity must still match.
The complete bounded SDK payload inventory/byte digest is captured before creating the archive that is
smoke-tested. Publication requires that independently held digest to match every current staged byte,
including headers and integration policy, before the generic atomic-copy checks run. A late stage edit
cannot inherit the extracted archive's passed smoke.
The outer SDK receipt retains the full immutable store manifest SHA-256 separately from the stored entry.

`Open-DxUiSdkInputBytes` accepts that independently retained/trusted manifest SHA-256, reacquires complete
current pinned inputs and opens the shared reader lease. It verifies the full payload plus SDK source/API,
Release/MD identity, library/PDB and required public header/integration policy before returning bytes.
Consumers retain the lease through all compiler/linker use, independently admit their exact compatible
consumer profile and linker provenance, and close in finally. Close rechecks complete current input identity
and the immutable inventory/bytes. An input mutation or byte-integrity failure invalidates the operation;
failure still releases the reader lease and preserves diagnostics. A missing exact version supplies no bytes.

Only a clean matching shared-engine checkout at the producer's exact `.ci/project.json` pin may implement
storage. The byte snapshot reuses that exact pin's canonical payload catalog, including its bounds and
reparse refusal; the pin-bound private-helper integration is covered by actual store tests. Generic
publication itself establishes no native SDK or product test verdict; mutable receipt
flags cannot authenticate a hosted producer. Cross-repository artifacts still require independent workflow,
source/run/attempt and transport-digest verification. Debug, ASan, disabled annotations and unpinned local
candidates retain source fallback until their distinct profiles and taint/output boundaries are implemented.

`Tools/SdkCacheInputs.psm1` acquires the shared v1 `DependencySdkInputs` identity without compiling or
granting a result. It binds clean exact source/API and raw bytes, restored dependency inventory/bytes,
complete supported Release/MD/stdcpplatest/GL-LTCG/enabled-annotation profile, selected toolchain files and
compiler environment, runtime environment, and exact clean producer tooling. Source/tooling mutations
during acquisition refuse. Consumers must reacquire and compare the input key after use, retain immutable
payload leases, independently verify producer trust and output manifests, and fall back to source on a miss.
Acquiring a key alone establishes no cache hit, native SDK acceptance or product test result.

The SDK's separate semantic project projection binds project files, solution mapping, expanded imports and
targets, complete evaluated compiler/linker/shader profile, resolved native source membership and bytes.
MSBuild's checkout-derived `Directory`/`RootDir` metadata remains bound by existing v2 build/test receipts;
those receipts are unchanged and are never portable SDK proof. All existing unsupported-input/profile,
ignored-header, toolchain/dependency byte and before/after checks remain. System installation paths and
compiler environment are retained conservatively: an otherwise compatible differently installed toolchain
may miss the cache and build from source.

Native identity acquisition requests the entire discovered MSBuild property set in one evaluation-only
response-file query. This reduces repeated process/evaluation cost without narrowing the property, item,
import, toolchain or source-byte closure. Response files contain only validated distinct property names;
queries add no targets and must fail on nonzero execution, invalid JSON or omitted properties. Query scratch
is operation-owned beneath the caller's `.build/msbuild-queries`, refuses reparse/foreign paths and is removed
after each attempt. Independent old-batch and installed-project controls establish value/profile equivalence.
See [MSBuild evaluation](https://learn.microsoft.com/en-us/visualstudio/msbuild/evaluate-items-and-properties)
and [response files](https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild-response-files).

## MSVC 14.52 Release policy

All supported configurations require the installed default MSVC **14.52.x** family (`RequiredMsvcToolsVersion` in
`Directory.Build.props`), still using the `v145` platform toolset. Discovery does not hard-code an installation or
silently select a different family. Restore rejects a different default before vcpkg work; MSBuild rejects a different
selected `VCToolsVersion` before compilation. Receipts bind the exact patch, compiler and linker hashes; `/GL` archives
and their final links must use the same build tools, including minor updates.

Release translation units explicitly enable `/Gy` and `/GL`. Final executable/DLL links explicitly enable `/OPT:REF`
and full `/LTCG`, with ordinary incremental linking disabled. Debug and ASan Debug retain their existing diagnostic
optimization policy. Full LTCG is selected after project-local defaults; do not assume that the WPO checkbox alone
selects full rather than incremental LTCG. `Tools/tests/Test-ReleaseBuildPolicy.ps1` evaluates every actual native
project in all six profiles and rejects broken compiler/linker controls.

The static archive remains complete. Function elimination and cross-file optimization happen at each final EXE/DLL
link and cannot optimize across DLL boundaries. LTO is not a guarantee of smaller or faster output; keep matched
fixture/source/toolchain identities, frozen baseline images, actual image/section sizes, link/build duration and
runtime/resource observations. Different compiler policies are an intentional experiment, not reusable identical-profile
performance qualification. Report shared-host activity and retain failed/noisy attempts.

Consumers must forward the evaluated `WholeProgramOptimization` and exact `VCToolsVersion` through their project
references. A parent's locally evaluated property is not inherited by an independent archive project. The public
consumer targets forward both. An older adopted pin may be retained with the same explicit consumer-owned MSBuild
forwarding; include `releaseOptimizationPolicy=Gy-REF-GL-LTCG-full-v1` in that consumer's dependency identity so an
old policy's writable archive output is never reused. API pin changes remain separate adoption work.

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

The required matrix is Windows 10/11 for the embedded baseline, x64 and ARM64, Debug/Release/ASan Debug, Unicode, the current
shared VS 2026/v145/Windows SDK baseline and `stdcpplatest`. Exact SDK, dependency versions and supported capabilities
are recorded once in machine-owned build/lock files. Windows-11-only backdrop features remain optional WindowHost
capabilities; they must not introduce unconditional imports that break the RedXe Windows 10 baseline. OS-specific
text and accessibility paths require actual verification. All library, sample, test and generated consumer projects
map each of the six configurations explicitly. ASan Debug uses compiler AddressSanitizer instrumentation, the debug
DLL CRT, non-incremental linking and separate configuration outputs. The public consumer targets explicitly disable incremental linking for ASan Debug;
a conflicting global `LinkIncremental` override is refused before compilation. This applies independently of
repository-local project defaults and avoids MSVC's LNK4300 ASan/incremental-link conflict. Every standalone translation unit rejects
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

Consumers restore the pinned tree under `<root>\.build\dependencies\DxUi\source\<first-16-commit-hex>\`, which is
50 characters after the consumer's root. The lock and verified Git HEAD retain the complete 40-character commit;
the folder prefix is only a locator. Historical full-commit folders are retained. Git without long paths cannot create
a file whose full path exceeds 259 characters. The conservative existing guarantee remains: every tracked path is at
most 150 characters and consumer roots of up to 35 characters restore without Git long paths, including the older
full-commit layout. `validate-dependencies.ps1` rejects a longer
path, tracked or about to be, before it can reach a pin.

Consumers import `Build/DxUi.Consumer.props` / `.targets` and reference `src/DxUi.vcxproj`. They do
not maintain a second list of library `.cpp` files. Public header paths and output paths resolve from the imported
file/project, never an assumed application `SolutionDir`. Standalone `.build` outputs and consumer dependency outputs
are separate. Consumer outputs use `.build/dependencies/DxUi/<first-16-fingerprint-hex>/<platform>/<configuration>/`
beneath that consumer, retaining the complete SHA256 fingerprint in verified identity metadata. The fingerprint
includes source, toolchain, CRT and build flags. Parallel builds of both applications must
not share writable intermediates or vcpkg work trees.

Pin WIL and any other extracted dependency with the library; no implicit reuse of whatever headers are on the
developer's machine. Build validation detects conflicting public-header dependencies and incompatible CRT/STL flags.
Shipping static libraries means there is no `DxUi.dll` to stage; the existing `AVControl.dll` remains dynamically
loaded by RedXe as a plugin. Windows/system runtime dependencies and notices still require normal packaging checks.

Library CI builds and tests without either application checkout. RedXe restores its pinned revision and runs
its integration tests; `redxe-adapter` records that pin and the synthetic COM/POD adapters, not real IME/AT. A library update changes the consumer lock in a reviewed change with test evidence and a
rollback to the previous pin. Each consumer owns its tested pin; shared source ownership does not require
simultaneous releases of the two applications.

## Implemented build and consumption

`src/DxUi.vcxproj` produces the only archive, `DxUi.lib`. FoundationTests, ControlTests, EmbeddedTests and the
standalone EmbeddedControls executable all consume it. The test-only `DxUi.InteractiveLease.exe` (the desktop lease behind
`test.ps1 -Interactive`) is built beside them in every configuration and does not link it. The supported lock target is
`["DxUi"]`, API revision 5. The [failure-policy evidence](../../Measurements/FailurePolicy_2026-11-06/README.md)
qualifies the recorded x64 library scope at `f6791b5070efd1fa8dc86601bc9422ae9a321d2e`;
native ARM64 execution, formal performance-policy acceptance and product adoption remain separate.

Failure host services use protocol 2 in every independently linked module. Rebuild
the complete EXE/DLL closure; protocol-1 table sizes are refused. Retained UIA peers
and notification targets keep a module lifetime pin, so detach/uninstall must handle
`BUSY` until those objects and active entries finish. The checked lifecycle result
does not authorize unloading a module with retained provider state.

Run `vcpkg-install.ps1 -Platform x64` (or ARM64/All) before build. WIL and the vcpkg tool revision are pinned.
Restore coordinates by the canonical output root. Checkout selection and bootstrap take an exclusive tooling
lease; each installer retains a shared tooling lease until vcpkg exits. A current checkout is verified under
that read lease without waiting for another platform's install. Platform-private install, buildtree, package
and overlay mutations take a separate root/platform lease. Same-platform contenders wait; disjoint platforms
and output roots may proceed together. Initialization is rechecked after exclusive acquisition and before
returning the shared lease. Failure releases leases, retains evidence and never terminates another owner.
Stable lease files are retained; deleting them while readers exist would break coordination. A finite
15-minute acquisition bound reports the contested path without accepting partial restore state.
The manifest's ports builtin-baseline and vcpkg-tool.json's executable checkout are independent nonzero full Git
revisions. Restore uses each for its own role; metadata validation checks both immutable identities and never
requires equality or rewrites either pin. Their compatibility still requires actual source-bound native qualification.
`-OutputRoot` isolates restore and build work in a consumer-owned directory. The same absolute directory, including
its trailing separator, becomes `DxUiConsumerOutputRoot`; props locate its public WIL headers and the project
reference passes it as `DxUiOutputRoot`. No application checkout is needed. Missing restore and mismatched/dirty pins
fail with actionable diagnostics. Both tracked and untracked source changes invalidate a release pin.

vcpkg builds with the Visual Studio installation and default MSVC toolset that MSBuild compiles DxUi with, not with the
newest toolset it finds. The two differ when a newer toolset is installed beside the default and lacks a compiler for a
target (in the historical 14.51-default installation, the side-by-side 14.52 preview lacked an x64-hosted ARM64 compiler, so every fresh ARM64 restore
failed configuring `wil:arm64-windows`); for compiled vcpkg libraries a toolset newer than the one that links them is also
a link hazard. `vcpkg-install.ps1` takes the installation from `Get-DxUiVisualStudioInstallation` (`Tools/VisualStudio.psm1`),
the one discovery `build.ps1` also uses (`vswhere -latest -prerelease -requires Microsoft.Component.MSBuild`), and the
toolset from that installation's `VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt`; neither is hard-coded. A missing
or malformed version file fails before anything is cloned or run, naming the file and the repair. For each platform it then
writes the overlay triplet `<output root>\vcpkg-triplets\<platform>\<triplet>.cmake` (`Tools/VcpkgTriplet.psm1`): the pinned
vcpkg checkout's triplet, copied unchanged in its own line endings, then
`set(VCPKG_VISUAL_STUDIO_PATH "<installation>")` and `set(VCPKG_PLATFORM_TOOLSET_VERSION "<major.minor>")`, and passes
`--overlay-triplets=<that directory>` to `vcpkg install`. The file is rewritten only when its bytes change; the pin is
major.minor, so a patch update of the default toolset and an unchanged restore leave it, and its timestamp, alone.

A different triplet is a different package ABI hash, so the first restore after this change, and after a change of
installation, default major.minor or vcpkg pin, rebuilds the packages (WIL here); vcpkg's default per-user binary cache
then serves the result to any other output root whose triplet text is identical. The interface is unchanged (`-Platform`,
`-OutputRoot`): a consumer that runs the pinned checkout's `vcpkg-install.ps1`, from Windows PowerShell 5.1 or PowerShell 7,
gets the pin with its next pin update and needs no change. In an ARM64 restore on an x64 host vcpkg's host triplet,
`x64-windows`, keeps its stock triplet; the host-tool ports it builds (`vcpkg-cmake`) run during the restore and are not
linked into DxUi. A consumer's own installer for compiled libraries applies the same pins to every triplet it builds. It
can import both modules from the pinned checkout, as it does `Tools/ConsumerBuild.psm1`; `Update-DxUiVcpkgOverlayTriplet`
takes any triplet file as its source, such as a sanitizer overlay (one that names a file beside itself with
`CMAKE_CURRENT_LIST_DIR` needs that file beside the generated copy).

External consumers import `Build/DxUi.Consumer.props` after Microsoft.Cpp.props and `.targets` after
Microsoft.Cpp.targets. Set DxUiRoot, DxUiConsumerLockFile and DxUiConsumerOutputRoot before those imports.
Compile with Unicode, stdcpplatest, v145 and the matching /MDd or /MD runtime. The public example's source includes
only public DxUi headers and ordinary Windows/WIL headers; it never reaches src. Diagnostic hook availability is
fixed in Configuration.h and must not be overridden by consumers.
The relocated consumer fixture copies the sample's shared ComplexUi scene into its own sample tree and adds
that copied directory to its compilation include search. It must compile without private library source paths
or include directories inherited from the developer checkout.

`build.ps1` presents a DxUi banner and color-classifies streamed MSBuild diagnostics in interactive terminals while
preserving plain redirected output and a complete file log. It rejects only running executables in the selected
output directory and never terminates them. Its six fixed top-level phases (Visual Studio discovery, output-use
checks, dependency admission, pre-build attestation, MSBuild and post-build attestation) emit bounded BEGIN and
END/FAIL records with UTC and monotonic elapsed milliseconds. Consumer-owned output roots retain their existing
separate attestation contract, so the two default-output attestation phases apply only to default builds. Timing
records add no input paths or environment values and cannot replace operation output or its original failure;
these diagnostics are not native workload measurements or performance acceptance.

The canonical performance-build attestation admits a library C parser with explicit `CompileAsC` only when its
remaining evaluated compiler options match the C++ profile. It still binds each source's exact language and flags;
other heterogeneous settings fail closed as specified in [performance provenance](../Core/Core_PerformanceAndResources.md).

`test.ps1 -SkipBuild` also requires the complete scoped native test-source/dependency/artifact receipt, which default `build.ps1` revokes before compilation and publishes only after a successful stable build. A missing or stale receipt forces a Rebuild, including test edits whose timestamps did not change. `test.ps1 -SkipBuild` first requires the canonical current v2 attestation, inside the fixed SkipBuildGate timing
phase, before any native desktop probe, filter/watchdog self-check or suite. Source, dependency, resolved toolchain
or final-artifact drift refuses early; no fallback relabels or rebuilds an invalid receipt. The later native and
before/after measurement checks remain required. CI separates the official build and receipt-verified Full stages
under the finite [validation ceilings](../Testing/Testing_Validation.md); timing helpers and the build entrypoint
remain in the existing harness identity closure, and workflow delegation requires the exact reviewed workflow digest.
`test.ps1` runs all three test executables, splitting inherited control suites into independent runs with exit-code,
SHA256, native architecture and capability-skip receipts; `-Tests <Name>[,<Name>]` narrows each control-suite run to the
named tests and `-TestTimeout <seconds>` sets the deadline each control test may run before the runner's watchdog ends the run
with exit code 124; `-Interactive` runs the suites that need the real desktop under `DxUi.InteractiveLease.exe` (see the
[validation contract](../Testing/Testing_Validation.md)). `gallery.ps1` generates five themed control sheets, a
supplied-device example image and an HTML index. `DxUi.EmbeddedControls.exe` opens the live toggle/slider example;
`--output image.png` renders it headlessly through a sample-created WARP device. `--complex-ui` selects the independent
83-control/1,000-row scene shared with the benchmark. External-consumer validation copies both sample directories
and renders both modes; neither sample requires an application checkout or application services.
`test-consumer.ps1 -FixtureRoot <short-directory>` places its unique relocated fixture under a caller-selected base
when a deeply nested checkout would exceed Windows/vcpkg path limits. It does not replace or clean existing fixtures.
The same fixture independently links one executable and two DLLs to the pinned archive and verifies native
animation callbacks, module-owned window procedures and menu invocation without sharing DxUi C++ objects.

`docs/README.md` documents consumption and all controls; root README links it and the published gallery.
`gallery.ps1 -PublishDocs` publishes reviewed sheets and a generation receipt under `docs/gallery`, leaving runtime
logs in `.build`. The receipt's `sourceDirty` excludes `docs/gallery`, which the command itself replaces. Docs/gallery maintenance follows [Core_Documentation](../Core/Core_Documentation.md).
The formatting workflow uses `format.ps1` and the repository `.clang-format` on pushes and PRs, returning a patch
when changes are needed. Formatting is pinned to clang-format 22.1.3: `Tools/Install-ClangFormat.ps1` fetches the
Windows x64 wheel by the version and SHA-256 it pins, verifies it and extracts only the executable under
`.build/format`. `format.ps1` checks the version and accepts `-FormatterPath` or `DXUI_CLANG_FORMAT`, then that
install, before PATH/Visual Studio discovery, so a runner image update cannot silently change output.
Manual `apply_changes` mode commits formatting to the selected branch with ordinary push
semantics; it never force-pushes, runs on pull_request_target, or grants fork PRs a write token. Branch protection
still applies. Token-created commits require an explicit validation run or subsequent user push for fresh CI.
Repository tooling needs only PowerShell 7; no CI job installs another runtime. Each test invocation includes complex-UI FPS/memory,
and implementation acceptance requires matched before/after evidence under the performance contract.

RedXe already has an exact pin and synthetic preparation/input/text/UIA adapters; real IME/AT acceptance stays
with its AV owner. RedSalamander I19 has completed local x64 Debug/Release adoption on its consumer branch;
its master merge/publication handoff and user-deferred ARM64/ASan qualification remain separate. See
[the completed routing record](../Plans/Done/RedSalamanderMigration.md). Neither application's runtime changes
merely because this library builds.

Managed vcpkg clones enable Git long-path support in their own repository configuration so deeply relocated
consumer fixtures can check out tool sources. No user/global Git configuration is changed.

Public consumer helper headers are `DxUi/Typography.h`, `DxUi/FocusRestore.h`,
`DxUi/PointerInput.h`, `DxUi/AccessibilityTextUnits.h`, and `DxUi/NativeMenuInterop.h`.
They preserve neutral font, focus, pointer, Unicode-unit and HMENU bridging policies. Application
commands, theme conversion, resource loading and window-subclass policy stay in consumer adapters.
Private compatibility includes inside src only forward to the canonical public headers. Consumers
must not include src paths. The relocated fixture compiles each helper header independently.
It also compiles `DxUi/RichMath.h` and `DxUi/RichTextExport.h` independently, and the extracted SDK smoke
parses and exports a heading and emphasis using only the packaged headers and archive.
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

## Compatibility and the consumer interface

The user-facing [consumer upgrade guide](../../docs/upgrade.md) collects the 2 November 2026 delivery pin,
API 4 rebuild requirements, earlier API 3 migration, optional adapter adoption, validation and rollback.
This contract remains the authority for exact source/toolchain identity and compatibility; library acceptance
does not qualify a consumer product.

`apiRevision` in `capabilities.json` is the compatibility number a consumer lock pins, and `validate_consumer.ps1`
rejects a lock that names another, so a consumer adopts a new revision in the reviewed change that moves its pin.

Revision 5 requires an application-owned failure host before any scoped DxUi work or direct control construction.
Install process hooks on the application owner thread, register the host services explicitly with each independently
linked module outside loader callbacks, stop work and detach those modules before uninstalling on the same thread.
Direct stack or smart-pointer control construction requires an active failure scope; checked factory callers handle
its returned refusal. `MarkdownParser::Advance` can return `Failed` with `CapacityExceeded`; consumers preserve the
previous publication and handle that result. Consumers use the checked `Try...` forms for externally supplied
source, ranges, configuration, command, export and native layout/query inputs; invalid convenience calls can be
contract-fatal. `SemanticBuilder::Advance` and `PreparedDocumentLayout` also expose failed staging results with
their documented publication and HRESULT semantics. Callbacks and model/delegate methods must return expected
refusals through their documented results; an unexpected C++ exception is fatal. The [upgrade guide](../../docs/upgrade.md#port-checked-operations)
maps the changed operations. The frozen revision 4 and [revision 5](../../Tools/ConsumerApi/ConsumerApi.Tests.Revision5.cpp)
fixtures compile for every exact-pin consumer profile. The sample host is an application example, not a consumer
crash-infrastructure qualification. Each consumer's native, dump-context and product-specific adoption checks
remain with its owner; the recorded x64 library evidence does not qualify a consumer product.

The revision increments when a consumer built against the previous one could stop compiling, linking or behaving as before
without a change of its own:
- a public header, declaration, MSBuild property or file is removed or renamed;
- a public function, control or message whose meaning a consumer relies on changes, including a private value a consumer had
  copied;
- a consumer interface entry, or one of its parameters, is removed or renamed;
- a behavior now needs a call it did not need before.

Additions do not increment it. Instead of incrementing, a change may keep the old form working for one revision: an alias for
a renamed declaration, or a script or function that forwards to its replacement. The next increment removes it. Such an
alias carries no `[[deprecated]]` attribute, since consumers require zero-warning builds; the changelog names it and its
replacement. The changelog fragment of a change that increments the revision says so and lists what each consumer must
change.

The optional read-only parsed-snapshot APIs remain in revision 4. They add methods without changing the ordinary
source-synchronization/parser paths. `MarkdownSyntax` gains an immutable shared owner, so its C++ object layout
changes. This is source-coordinated adoption: rebuild with the exact matching headers, library, compiler, runtime
and configuration. API revision equality or an isolated layout probe does not permit mixing old/new objects or
establish stable binary interoperability. Performance comparisons across these pins must admit their exact
changed header closure and retain each side's independently rebuilt source/toolchain evidence.

The consumer interface is what a consumer may call or import from its pinned checkout. `capabilities.json` lists it under
`consumerInterface`:
- **Scripts**, with the parameters consumers pass: `vcpkg-install.ps1`, `validate-build-matrix.ps1` and
  `Tools/validate_consumer.ps1`.
- **Module functions**, with their parameters: `Get-DxUiConsumerBuildIdentity`, `Show-DxUiUpdateNotice`,
  `Get-DxUiVisualStudioInstallation`, `Get-DxUiDefaultToolset` and `Update-DxUiVcpkgOverlayTriplet`.
- **MSBuild files**: `Build/DxUi.Consumer.props` and `.targets`, `Build/DxUi.AddressSanitizer.targets` and `src/DxUi.vcxproj`.
- **Public headers**: the `include/DxUi` root.

`validate-dependencies.ps1` fails if any of the following holds:
- a listed script or module is missing;
- a listed module no longer exports a listed function;
- a listed script or function lost a listed parameter;
- a listed MSBuild file or the header root is missing;
- the revision is not a positive integer.

It reads scripts and modules from their parse trees and runs nothing. Names compare as PowerShell binds them, ignoring case,
and a new parameter is an addition. Every other repository tool, such as `build.ps1`, `test.ps1`, `Tools/Validation.psm1` and
the other validators, may change without a revision.

Revision 3 covers changes made at revision 2:
- `IDxGridModel`, `IDxGridDelegate`, `IDxTreeModel` and `IDxTreeDelegate` became `IGridModel`, `IGridDelegate`, `ITreeModel` and `ITreeDelegate`.
- The modal-loop entry point became `RunModalLoop`; render diagnostics became `IsRenderStageActiveForDebug` and `EmitRenderMutationBlockedForDebug`.
- In `DxUi::Typography`, `TypographySpec`, `GetDxUiTypographySpec`, `TypographyPerfEmitter`, `GetTypographyPerfEmitter`, `SetTypographyPerfEmitter`, `EmitTypographyPerfCounter`, `TypographyFontFamilyCacheEntry`, `TypographyTextFormatCacheEntry`, `GetTypographyMeasurementCacheMutex`, `GetTypographyFontFamilyCache`, `GetTypographyTextFormatCache`, `kTypographyFamilyCacheMissMetric` and `kTypographyTextFormatCacheMissMetric` became `Spec`, `GetSpec`, `PerfEmitter`, `GetPerfEmitter`, `SetPerfEmitter`, `EmitPerfCounter`, `FontFamilyCacheEntry`, `TextFormatCacheEntry`, `GetMeasurementCacheMutex`, `GetFontFamilyCache`, `GetTextFormatCache`, `kFamilyCacheMissMetric` and `kTextFormatCacheMissMetric`.
- The Python tools were removed; `validate-build-matrix.ps1` replaces the Python build-matrix validator.
- Private window messages are registered by name, and menu-bar hover is posted through `ContextMenu::PostMenuBarHover`.

Revision 4 advances because Grid selection behavior and tab extraction semantics changed in ways an existing consumer could observe. It includes the compatible migration APIs `Panel::TakeChild`, `TabControl::TakeTab`/`TakeChild`, independent Grid focus accessors and `RequestFocusRow`, Tree focused-item accessors and `RequestFocusVisibleItem`, focus notifications, and the `GridSelectionModel` anchor setter; it also records quoted TSV output behavior. Those additions alone would not require a revision bump. The frozen [`Revision4.cpp`](../../Tools/ConsumerApi/ConsumerApi.Tests.Revision4.cpp) consumer fixture compiles with every consumer profile; review it alongside public headers whenever the revision changes.

A consumer moving its pin to revision 4 sets `apiRevision` to 4 and reviews these changes:

- `Panel::AddChild` and `TabControl::AddTab` return null when a callback retires the panel or the new child while it
  is inserted; check the result before using it.
- `Panel::TakeChild(index)` leaves an empty slot, while `TabControl::TakeChild` and `TakeTab` remove the tab. Either
  returns empty when a focus or capture callback keeps the child in place: check it before giving it to `SetRoot` or
  `PageHost::SetPage`, since `SetRoot` with an empty pointer destroys the tree that still holds the child.
  `RemoveTab` keeps the tab in that case too.
- `ITreeDelegate::OnTreeSelectionChanged` reports a membership change to a nonempty selection, with its primary member;
  a move of the focus alone reports `OnTreeFocusedItemChanged`. Grid reports its focused row through
  `OnGridFocusedRowChanged`, apart from `OnGridSelectionChanged`.
- A described menu entry's UI Automation name is its label or `accessibleName`, and its secondary text is its
  HelpText: an `accessibleName` that repeated the secondary text now speaks it twice.
- `ContextMenuSessionCallbacks::flowDirection` sits between `rootVerticalPlacement` and `minRootWidthDip`; initialize
  the aggregate by member name.

The fixture also derives a consumer's Grid and Tree models and delegates, so a later pure virtual or a changed
signature of an overridden virtual stops it compiling.

`capabilities.json` declares the supported PowerShell versions for each consumer script and module. The published restore entry point `vcpkg-install.ps1` remains compatible with Windows PowerShell 5.1 and PowerShell 7. The consumer-facing `validate-build-matrix.ps1 -Root` runs under PowerShell 7 and resolves a relative root against the caller's working directory. `Tools/validate_consumer.ps1` and the shared `ConsumerBuild`, `ConsumerUpdate`, `VisualStudio` and `VcpkgTriplet` modules remain compatible with 5.1 and 7. Repository validation and scoped-iteration tools require PowerShell 7. The dependency validator parses each declared entry, rejects newly mandatory parameters unless declared, and rejects missing or unsupported PowerShell version promises.

## Advisory updates

Consumers call `Show-DxUiUpdateNotice -LockFile <exact-lock>` from `Tools/ConsumerUpdate.psm1`
once in their root restore/build invocation. It reads main, verifies ancestry, and recommends only
the exact main commit whose latest `qualify.yml` dispatch reported success. This advisory remains a hint,
not a replacement for source-bound native receipts and the consumer's own qualification. Same-pin builds are
quiet; pending/failed/unavailable validation keeps the pin selected; divergent/ahead pins require review.
The notice includes the comparison URL and lock to update. It is plain build output, never an MSBuild
warning, automatic edit, PR comment, push, or scheduling service. Private read access uses existing
GH_TOKEN/GITHUB_TOKEN or a bounded local gh credential lookup. The gh child has a two-second run
deadline; on timeout only that owned child is terminated and its exit is confirmed within one additional
second. Standard output and error drain concurrently and have a bounded drain wait; only a zero-exit,
fully drained result can supply the token. Each of at most three API requests has a two-second timeout.
Lookup failures are explicitly unavailable and cannot fail an otherwise valid fixed-pin build.
Authentication details are never logged.
The exit/drain fixture uses a native local command under the same two-second deadline as the native `gh`
helper. It does not conflate cold PowerShell interpreter startup with native helper work. The separate timeout
fixture still verifies owned-process termination and independent-process survival; failed fixtures retain their
synthetic result records through the shared fixture owner. No production deadline or pipe-drain budget changes.
The consumer still validates/restores its exact source pin independently and fails on a real pin error.

### CI toolchain preparation and audit evidence

Active Windows compilation/tooling workflows use the local setup-msvc composite action. It reads RequiredMsvcToolsVersion, uses the canonical `Tools/VisualStudio.psm1` discovery to verify the selected default compiler, linker, ASan target libraries, SDK and x64 LLVM scanner tools, and provisions Visual Studio 2026 Insiders only on GitHub-hosted runners when necessary. The Microsoft-signed bootstrapper runs under RUNNER_TEMP; existing installations are retained. A channel that no longer supplies 14.52 fails rather than accepting another compiler. Local -VerifyOnly is read-only; automatic local installation is refused.

The bootstrapper download has a 180-second request timeout. Launch the Windows GUI bootstrapper with `Start-Process -WindowStyle Hidden -PassThru` and its own `--wait`, then wait at most 30 minutes on that process handle. On timeout, request termination of that owned process and its descendants and wait at most 15 seconds for the owned parent; report termination failure explicitly. Read its actual `ExitCode`, never ambient `$LASTEXITCODE`. Preserve an install path containing spaces as one argument. Only exit codes 0 and 3010 permit rediscovery and full verification of the default toolchain that downstream builds will select. Retain bootstrapper stdout/stderr under the generated RUNNER_TEMP directory, print their tails and copy this invocation's detailed `dd_*.log` files from TEMP/RUNNER_TEMP even when the installer times out. The composite action uploads only these logs in a uniquely named artifact on success or failure; the downloaded executable is excluded. [Microsoft's waiting guidance](https://learn.microsoft.com/en-us/visualstudio/install/command-line-parameter-examples#using---wait) explains the bootstrapper wait. `Tools/tests/Test-MsvcSetup.ps1` exercises delayed and timed-out fake GUI installers and provider fixtures without downloading or installing software. A fixture pass does not establish actual channel/component availability; hosted installation and the subsequent native jobs need successful CI receipts.

[Full evaluated settings and differences](../../Measurements/Toolchain/2026-10-23-msvc1452/Settings.md) include both platform/configuration matrices and exact per-file metadata. [Microsoft installation guidance](https://learn.microsoft.com/en-us/cpp/overview/acquire-msvc) documents channel-specific acquisition. The published [runner image inventory](https://github.com/actions/runner-images/blob/main/images/windows/Windows2025-VS2026-Readme.md) does not establish that the required toolset is installed; runtime versions alone are also not compiler proof.

## Hosted repair qualification record (2026-10-27)

The source-bound [hosted MSVC native archive](../../Measurements/Toolchain/2026-10-27-hosted-msvc/README.md)
records PR #119 head `767dffc5` / Actions merge `38d9b21e`: all six actual builds/Full native jobs and eight consumer
qualifications passed, with independently replayed original ZIPs and completion streams. The record retains ARM64
initial-focus limitations and a separate local x64 Release Menu supplement. It does not establish blanket physical-input
or performance acceptance. Strict paired preflight matched after complete-catalog project staging. All original calculations replayed, but final publication refused the unqualified base policy; unstable controls and dual-judge disagreements also remain open. The external main merge has the same reviewed tree. A separate main x64 Debug NativeTextInput failure remains unattributed; this record does not replace or retag either cohort.
Provider runner-file hashes and exact source Git-blob hashes have explicitly different representation labels.

The [native policy, console and dependency-path contract](Build_ConsoleAndPaths.md) owns /Wall without /WX, zero-warning acceptance, Debug/ASan symbols, Release-only ICF, common terminal results and verified short dependency folders.

The relocatable Release SDK distribution and extracted-consumer proof are governed by Specs/Build/Build_UnifiedCI.md. Its exact compiler/LTCG/CRT identity and packaged build policy do not change the independently pinned source-consumer interface above.
