# Carrying consumer process capture into version6

Status: integration guide only. **This fix has not been applied to or qualified on version6.**

This guide describes the API4 maintenance change based on
`4cdf1ce52c7785446698e346b8aec058ed6a9283`, on branch
`ej/dxui-api4-consumer-capture-20261115`. Use the eventual reviewed maintenance
commit as the source of the patch; the base commit alone does not contain the fix.
The [toolchain and consumption contract](Build_ToolchainAndConsumption.md) and
[validation contract](../Testing/Testing_Validation.md) remain authoritative.

## Identify the destination before applying anything

The observed cooking branch was `ej/dxui-performance-motion-impl-63dac3`, at
`9574105be2fdeb768079dda27217217e2ada25c8`, with a clean worktree on
2026-11-15 at 10:31 UTC. Its active plan was
`Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md`.
**Its actual `capabilities.json` declared API revision 5, not 6.** Here, version6
is the user's intended release label, not an instruction to change `apiRevision`.
Re-read the destination's current capabilities, instructions, helper catalog,
consumer interface and active plan before porting; these facts are a snapshot.

That branch already had unified CI: `.ci/project.json` named the SDK adapter
`Tools/Invoke-ProjectCi.ps1` and shared engine
`766fd7e01aeef2e77ba18df83b9928516155014e`. Its VisualStudio module also exported
`Get-DxUiNativeMSBuild`, in addition to installation/default-toolset discovery.
Preserve those destination features. Do not replace whole destination files with
their API4 versions, copy the legacy API4 `ci.yml`, restore an old workflow digest,
or adopt API4 capabilities/headers merely to obtain this process fix.

## Problem and intended boundary

A background PowerShell host can invoke a console application through its native
command processor even when that host was itself started with no-window flags.
The parent's flags do not impose the same policy on each later native launch.
The correction explicitly captures the producer-owned background launches below.
It does not identify the creator of a historical foreign terminal, grant authority
to dismiss one, prove that no console-server process exists, or establish a
performance improvement. Vendor bootstrap/vcpkg/compiler descendants retain their
own launch behavior; a captured producer boundary is not proof about every descendant.

## Port the behavior, keeping the destination's interfaces

Search the destination's canonical process helpers first. Reuse one only if it
retains the consumer's PowerShell 5.1/7, standalone-module, stream, ownership and
error contracts. The maintenance implementation keeps one private process core
inside `Tools/ConsumerBuild.psm1`; it needs no companion module, consumer repository,
shared CI checkout, desktop helper or new public export. Co-versioned producer-owned
modules/scripts invoke that core in its module scope. External consumers continue
to use only the declared public interface.

| File / symbols | Behavior to carry forward |
| --- | --- |
| `Tools/ConsumerBuild.psm1`: `ConvertTo-DxUiConsumerArgument`, `Start-DxUiConsumerQueryProcess`, `Invoke-DxUiConsumerNativeQuery` | Windows argument quoting; prelaunch no-window/redirected-stream checks; concurrent separate metadata capture; live installer streaming; explicit child exit and owned-root disposal. |
| Same module: `Write-DxUiConsumerProcessLine`, `Test-DxUiConsumerInteractiveConsole`, `Test-DxUiConsumerFatalException` | Stream output without polluting the returned exit/result; distinguish a genuine interactive console; preserve direct and wrapped fatal exceptions and a known primary failure. |
| Same module: `Get-DxUiConsumerBuildIdentity` | Capture Git HEAD and MSBuild property evaluation. Retain the public parameters `DxUiRoot`, `MSBuildPath`, `Platform`, `DisableStlAnnotations` and its existing result shape. |
| `Tools/VisualStudio.psm1`: `Get-DxUiVisualStudioInstallation` | Capture VSwhere while preserving installation selection and default-toolset policy. Keep all newer destination functions/exports, including native MSBuild host selection. |
| `vcpkg-install.ps1` | Capture background clone, origin/status/HEAD, conditional fetch/checkout, bootstrap and install. Keep the existing interactive native route, platform/output-root parameters, tool/repository pins, triplet and explicit exit checks. |
| `Tools/validate_consumer.ps1` | Capture HEAD/status before enforcing the same exact commit, clean checkout, API revision and single-target lock checks. It is reached through `ValidateDxUiConsumerPin` in `Build/DxUi.Consumer.targets`. |
| `Tools/ScopedTesting.psm1`: `Invoke-ScopedGit` | Set no-window creation on the existing redirected Git process without changing path discovery, decoding, arguments or reuse rules. |

The identity calculation must not change as a side effect. Preserve ordered fields,
property validation, compiler-host selection, file hashes, JSON serialization and
SHA-256 calculation for identical inputs. A newly committed source naturally changes
the returned `commit` and fingerprint; do not force the old fingerprint or reuse its
writable output directory. Preserve the destination's current API revision and C++
headers, source, ABI and capabilities. This guide does not request an API6 migration.

## Implementation traps to retain in the port

- Consumer-facing files must parse and run under Windows PowerShell 5.1 and
  PowerShell 7. Keep ASCII source where the existing no-BOM/ANSI compatibility
  contract requires it. Do not introduce `ProcessStartInfo.ArgumentList`,
  `Process.Kill(bool)` or PowerShell-7-only syntax into that portable core.
- Use `UseShellExecute=false`, `CreateNoWindow=true` and both redirected streams
  for background children. Quote empty arguments, embedded quotes, spaces and
  backslashes according to Windows executable argument rules. Keep the selected
  filesystem working directory separate from command text and preserve the host's
  native decoding policy. Stderr must never become a host value or JSON stdout.
- Start both pipe reads before waiting. Metadata retains separate output/error
  results. Streaming processes line tasks on the caller thread and forwards output
  while the child is alive; it must not buffer the whole installation or add an
  idle polling timer. Preserve the actual exit code and installer
  `$LASTEXITCODE` behavior, including failures.
- A module imported with `-PassThru` must be assigned privately. Do not leak its
  `PSModuleInfo`, progress text or callback return values into a public discovery
  result or the numeric exit result. Do not reimport with `-Force` over active
  test interception on every probe.
- An inherited `WT_SESSION` or similar marker is not proof of an interactive
  console. Background/no-console callers remain captured. Ordinary verified
  interactive installer calls keep native inheritance; do not alter the
  destination's ordinary interactive build-console defaults.
- The background batch route admits only the fixed producer-owned
  `bootstrap-vcpkg.bat -disableMetrics` command through owned `cmd.exe` with
  `/d /v:off /s /c`. Select its checkout using the separate working directory;
  do not interpolate a caller path or arbitrary arguments into shell syntax.
  Executable quoting and `cmd` command parsing are different contracts.
- Cleanup owns the exact process it started, not arbitrary same-name processes or
  an independently inferred process tree. Preserve a known primary failure when
  secondary cleanup fails, using nonterminating fixed-content diagnostics.
  Unwrap fatal exceptions with `GetBaseException` at cleanup and interactive
  fallback boundaries; direct or wrapped OOM/stack-overflow/access-violation
  exceptions must propagate. Do not log exception text containing private paths.

## Controls to transplant and run

Port `Tools/tests/Test-ConsumerBuild.ps1` into the destination's current tooling
test layout and registration. Use the destination's governed captured runner to
launch each supported host; a bare PowerShell host invocation can itself enter the
native-command console path from a headless parent.

The following is a producer-development example, usable only after the reviewed
private core and test port exist. It invokes the co-versioned core in its assigned
module scope; **this is not a public consumer API**. Run from the destination
checkout and supply existing owned fixtures and trusted absolute executable paths:

```powershell
# Existing caller-owned fixture parent; this does not initialize a drive root.
$producerRoot = (Get-Location).ProviderPath
$fixtureParent = '<existing owned fixture directory>'
$msbuildPath = '<trusted native MSBuild.exe>'
$engines = @('<trusted absolute powershell.exe>', '<trusted absolute pwsh.exe>')
$testScript = Join-Path $producerRoot 'Tools/tests/Test-ConsumerBuild.ps1'
$captureModule = Import-Module (Join-Path $producerRoot 'Tools/ConsumerBuild.psm1') -PassThru -Scope Local -ErrorAction Stop
$testArguments = @('-NoProfile', '-NonInteractive', '-File', $testScript,
    '-FixtureParent', $fixtureParent, '-MSBuildPath', $msbuildPath)
foreach ($enginePath in $engines) {
    if (-not [IO.Path]::IsPathRooted($enginePath) -or
        -not (Test-Path -LiteralPath $enginePath -PathType Leaf)) {
        throw 'Supply a trusted absolute supported-host executable path.'
    }
    $exitCode = & $captureModule {
        param($Engine, $Values, $Directory)
        Invoke-DxUiConsumerNativeQuery -FilePath $Engine -Arguments $Values -WorkingDirectory $Directory -Stream
    } $enginePath $testArguments $producerRoot
    if ($exitCode -ne 0) { throw 'Supported-host consumer capture controls failed; retain that case evidence.' }
}
```

Without an explicit trusted MSBuild path, actual MSBuild evaluation is
`NOT_EVALUATED`, not a pass. Run the two shells sequentially against independently
owned retained case directories. Do not run an unsafe bare-process baseline.

The standalone test host must bind its own standard Utility module before importing
TestSupport or the producer. A cross-host parent can leave PowerShell 7 module
search state in Windows PowerShell 5.1; one retained diagnostic observed the
PowerShell 7 Utility manifest loaded there without `Get-FileHash`. The earlier
load sequence was not captured. The test-only bootstrap explicitly imports the
running host's `$PSHOME` Utility manifest and verifies its actual callable. A
manifest can legally re-export a function from a nested script module: the surfaced
command's module wrapper and the export's module need not be the same object.
Retain top-manifest authority, require the same nonnull `ScriptBlock` plus the
trusted native definition for functions, and compare actual `ImplementingType`
identity for cmdlets. Reject a different callable even when its name or wrapper
looks right. Retain these private predicates in `host-bootstrap-binding.json`
before refusing setup and bind that record through `callable_binding_sha256` in
`host-bootstrap.json`. These observations do not establish how an earlier host
loaded its modules. Keep the paths and bindings private. Its public
`host_utility` projection contains only the validated version, manifest digest,
command kind and verified-owner flag; the private summary binds the raw record
with `host_bootstrap_sha256`. Carry this setup into the destination's isolated test
host, not into the shipped SDK. Do not change `PSModulePath`, production hashing
or the consumer identity algorithm to repair a test-host module mismatch.

Record the exact successfully started process object before querying fallible
creation/handle metadata. If observation then fails, retain that start and dispose
only its owned process while preserving the primary failure and fatal semantics.
On Unix, `Process.SafeHandle` is supported but represents a runtime wait object,
not a Windows process-instance lease. A short child may exit before uncached
creation metadata is available; report that outcome explicitly without inventing
a timestamp or reopening a PID. Retain positive creation/handle assertions on
Windows and actual positive creation evidence for the controlled long-lived child.
Exercise both already-exited and still-live observation failures deterministically.

The test observer must invoke the exact owned `Diagnostics.Process` `StartTime`,
`SafeHandle` and `HasExited` getter methods explicitly. PowerShell's property
binder can suppress ordinary CLR or ScriptProperty getter failures into null;
property syntax alone does not preserve the error being tested. Unwrap the method
failure with `GetBaseException`, validate a positive `DateTime`, held `SafeHandle`
and actual boolean exit result, and reject null or wrong-type facts. Only the
known `InvalidOperationException`/`Win32Exception` creation errors on Unix plus a
verified boolean true exit result permit the unavailable-creation outcome.
Windows, live-child and unrelated errors remain failures; fatal errors propagate.
Keep controlled reader seams inside the existing test
case; they must not manufacture native starts or change the shipped process core.

Public control projection v2 adds only the closed `setup.phase`, `setup.type` and
signed 32-bit `setup.code` fields. Keep phases and exception categories allowlisted;
never publish an arbitrary exception type, message, path or stack there. Preserve
the detailed primary failure privately in `control-error.json`, alongside the raw
start observations and existing summary/manifest hash bindings. The earlier Linux
failure's exact exception remains unproved: a reachable post-start metadata race
is not evidence that Unix wait handles are unsupported or that it caused that run.

The review/qualification packet must retain these controls and their limits:

1. Standalone copied-module import, unchanged public export/parameters and a frozen
   same-input identity oracle. Cover selected host architectures, annotation policy,
   required properties, malformed JSON, missing files and valid-looking stdout
   accompanied by a nonzero exit.
2. Actual safe argv/Unicode/empty/quote/trailing-backslash cases, separate large
   stdout/stderr pipe pressure, and live output observed before child exit.
   Require safe PSI before launch, actual exit/result integrity and an actual
   no-secondary-window observation. Console-server association counts are diagnostic,
   not a requirement to be zero.
3. The fixed batch command in an owned working directory containing shell
   metacharacters and Unicode: correct marker/output, actual nonzero exit and no
   path expansion/injection. Keep all paths inside the approved fixture tree.
4. Direct and wrapped fatal controls, ordinary refusal, primary plus secondary
   cleanup failure under `WarningPreference=Stop`, and cleanup without a prior
   failure. Preserve exact ownership and environment restoration. Use injected
   exception objects, not actual resource exhaustion.
5. Caller/source guards for the complete producer-owned chain, including VSwhere,
   installer conditional branches, consumer lock validation and scoped Git.
   Keep Git-fixture real module/type/path/export-owner checks distinct from the
   actual recorded launch-routing observations; Pester proxy metadata alone is
   not routing proof.

## Integrate with version6 qualification and delivery

- Update the destination's owning build spec, helper catalog/help, active plan,
  change fragment, tooling registration and required dependency closure. A fixture
  that copies VisualStudio must include its co-versioned ConsumerBuild dependency.
  Keep ConsumerBuild itself independently importable.
- Include the co-versioned process backend in the destination's applicable paired
  build/restore relevance and Full harness identity. Port the import-closure guard
  and actual-selector mutation/omission controls so a backend change invalidates
  reuse. The actual common build-harness overlay must also carry the co-versioned
  installer, discovery and process backend onto both sides; a hash-only addition
  cannot make different restore implementations comparable. Keep workload/fixture
  identity separate from that build overlay, and preserve the real baseline, each
  side's library source/project, dependency pins, judge and strict preflight equality.
  Cover missing-backend refusal, copy/rollback and library preservation using the
  destination's actual selector. Do not copy an API4 path list or workflow digest
  over destination policy.
- Adapt CI tests to the destination's current unified adapter and scoped/evidence
  contracts. Add always-run-on-failure retention/upload for only the owned control
  result directory. Publish closed-field digests/counts and limitations, not raw
  machine paths, command lines, environment values, credentials or desktop contents.
  Refresh any required workflow/runner digest from the destination's actual final
  bytes; never copy the API4 legacy digest or weaken required checks.
- Inspect the actual affected and PrePush plans. Component controls do not replace
  required zero-warning native builds, native ARM64 execution, consumer restore/build
  tests or the destination's remaining qualification. No scoped pass closes a
  separate foreground gate. Use existing leases and agreed time for any required
  focus-taking work; this fix grants no desktop permission.
- Retain the tested source, shell/toolchain identities, hashes, case counts,
  failures and explicit unexecuted fields. The API4 maintenance result, any historical
  consumer run and a future version6 commit are separate evidence identities.
  Re-run qualification required by the final destination source and caller contracts.
- Only after reviewed source-bound producer evidence exists should each consumer
  explicitly adopt the published full commit and rebuild with matching headers,
  library, runtime and configuration. Old SDK artifacts cannot be relabelled as a
  new producer commit. Keep version6's SDK distribution and current pin rules intact.

Until that destination work and its applicable gates actually finish, record the
carry-forward as **not applied / not qualified on version6**. Neither this document
nor the API4 backport marks the performance/motion plan complete.
