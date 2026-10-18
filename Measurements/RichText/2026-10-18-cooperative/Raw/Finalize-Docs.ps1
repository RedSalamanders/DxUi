[CmdletBinding()]param([string]$Root)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$work=Join-Path $Root '.build/reports/rich-cooperative-20261018'
$final=Join-Path $work 'Final'
$matrix=Get-Content -LiteralPath (Join-Path $final 'qualification-completion.receipt.txt') -Raw|ConvertFrom-Json
$tooling=Get-Content -LiteralPath (Join-Path $final 'tooling-completion.receipt.txt') -Raw|ConvertFrom-Json
if($matrix.completed -ne 6 -or $matrix.attemptedOperations -ne 14 -or
 @($matrix.operations|Where-Object {$_.exitCode -ne 0 -or $_.timedOut -or -not$_.drained}).Count -or
 $tooling.attempted -ne 2 -or @($tooling.results|Where-Object {$_.exitCode -ne 0 -or $_.timedOut -or -not$_.drained}).Count){
 throw 'Do not close documentation before the fixed qualification passes.'
}
$frozen=Get-Content -LiteralPath (Join-Path $final 'qualified-source.receipt.txt') -Raw|ConvertFrom-Json
foreach($entry in $frozen.files){
 if($entry.path -match '^(src/|include/|Tests/|Tools/)' -and (Get-FileHash -LiteralPath (Join-Path $Root $entry.path)).Hash -cne $entry.sha256){
  throw 'Qualified code/test input changed before documentation closeout.'
 }
}
function Replace-Exact([string]$Relative,[string]$Before,[string]$After){
 $path=Join-Path $Root $Relative
 $text=[IO.File]::ReadAllText($path)
 if(-not$text.Contains($Before)){throw "Documentation anchor absent: $Relative"}
 [IO.File]::WriteAllText($path,$text.Replace($Before,$After))
}
Replace-Exact 'Specs/Core/Core_PerformanceAndResources.md' 'Cooperative rich preparation candidate (qualification in progress) retains at' 'Cooperative rich preparation retains at'
Replace-Exact 'Specs/UI/UI_ControlsAndLayout.md' 'Cooperative preparation candidate (qualification in progress): content owners' 'Cooperative preparation: content owners'
Replace-Exact 'Specs/Rendering/Rendering_EmbeddedD3D11.md' 'Cooperative content candidate (qualification in progress): Prepare returns' 'Cooperative content preparation: Prepare returns'
Replace-Exact 'docs/rich-text.md' 'Cooperative preparation is being qualified.' 'Cooperative preparation is implemented and configuration-qualified.'
Replace-Exact 'docs/rich-text.md' 'parser claim follows. Final configuration qualification remains active.' 'parser claim follows. All ordinary x64 Debug/Release/ASan suites and all six build profiles pass; native ARM64 runtime and physical IME/AT remain separate.'
$guide=Join-Path $Root 'docs/rich-text.md'
[IO.File]::AppendAllText($guide,"`nThe [cooperative preparation record](../Measurements/RichText/2026-10-18-cooperative/README.md) retains native/build/pixel/fault/resource evidence, all original failed/interrupted attempts and pinned Ghidra/parser references. This closes the scheduling component; the full engine and original frame-performance HOLD remain open.`n")
$resources=Join-Path $Root 'Specs/Core/Core_PerformanceAndResources.md'
[IO.File]::AppendAllText($resources,"`nThe [cooperative preparation record](../../Measurements/RichText/2026-10-18-cooperative/README.md) qualifies 57 ordinary x64 scopes, six builds and 324 exact WARP pairs without changing the existing ceilings or oracles. Work-unit bounds do not establish quiet-host frame acceptance, hard latency or large-document capacity.`n")
$plan=Join-Path $Root 'Specs/Plans/WIP/RichTextEditor_2026-10-17.md'
$text=[IO.File]::ReadAllText($plan)
$marker='### Cooperative preparation implementation, 2026-10-18'
$offset=$text.IndexOf($marker,[StringComparison]::Ordinal)
if($offset -lt 0){throw 'Cooperative plan component absent.'}
$component=@'
### Cooperative preparation implementation, 2026-10-18

- [x] Stage cancellable document geometry with separate SDK-layout and block/
  placement budgets; preserve atomic source/geometry publication on failure.
- [x] Borrow ControlHost's live-tree traversal and native/application wakeups;
  coalesce one pending request without a control-owned worker/timer/queue.
- [x] Keep queries non-draining, retain source/IME transactions while pending
  and reject obsolete source/mode/binding/width/typography/resource results.
- [x] Cover budgets, unchanged reuse, compaction/whole fallback, cancellation,
  failure/retry, rapid typing, stale IME tokens, callback retirement, two views,
  native/embedded UIA and hide/zero extent/device/typography replacement.
- [x] Complete required native/build/resource qualification, validators and
  pinned formatter; preserve original attempts and reconcile specs/docs.

The core exposes BeginPreparation/AdvancePreparation/CancelPreparation. Direct
core Prepare remains a compatibility drain of those same phases. The control
uses owner turns (default four SDK layouts and 4,096 block/placement steps per
visible owner), never input/paint/geometry query drains. Aggregate host work
depends on owner count. Begin's v0 projection/index pass, joins, one physical
paragraph and whole compatibility SDK calls remain indivisible. Pending logical
commands may project source without DirectWrite. No hard millisecond, complete
parser or large-document guarantee follows.

Each owner holds one current pending request; immutable geometry can share the
published graph. Complete key checks precede acknowledgement, and successful
pixels precede text-service/UIA geometry publication. One outstanding application
notification is distinct from dirty pixels; failure has no self-retry loop.
Source typing, selection/history and revision-checked IME preview/cancel remain
available during pending geometry. Visual movement/scroll/hit/bounds wait for
current layout. Native WM_PAINT and embedded application continuations are both
tested. Embedded views acquire no HWND/device/swap chain/worker/timer.

Native window hide and either zero-size dimension explicitly retire content,
including unchanged-width/zero-height transitions. WM_SETREDRAW's asymmetric hide
retains the swap chain. Source/selection/history survive restoration. The initial
embedded first-Prepare notification convention is preserved. Reentrant Prepare
is rejected; scheduling callbacks remain enqueue signals under the embedded
ownership/reentrancy contract.

Qualification uses source fingerprint
4FF99EDD4EF7A76C5C346D5BBA3DD99A167C049987256265C548D8778C322A57.
All 57 ordinary x64 Debug/Release/ASan scopes pass with zero capability skips;
six isolated Debug/ASan failure probes and all six root build profiles pass.
ARM64 is cross-build evidence, not native execution. All 324 actual/reference
WARP PNG pairs are byte-identical, with unchanged 0.125-DIP geometry tolerance.
Copy/chunk/whole failure sweeps cover 22/338/471 observed points in Debug/ASan;
existing zero-allocation, 24-MiB source-fixture, 8/16-MiB retained/peak and zero-
released-delta oracles pass. SDK-private heaps are excluded from C++ counts.
The fixed 14-operation matrix finishes in __MATRIX_SECONDS__ seconds; full five-
validator/tooling validation and formatter 22.1.3 finish in __TOOLING_SECONDS__
seconds. Evidence sealing/replay and final document checks are separate closeout
records in the [cooperative packet](../../../Measurements/RichText/2026-10-18-cooperative/README.md).

| Attempt | Observed result retained |
| --- | --- |
| CoreFirst | Core-only affected Release passes in 349.381 s; no later host qualification implied. |
| HostFirst | Protected lifetime-access compile failure, 375.262 s overall; original source/full logs retained. |
| HostSecond | Provenance refuses shortened MSBuild failure markers, 20.325 s; intermediate trees/hash mappings preserved, no policy changes. |
| HostThird | Build passes; old visual-navigation drain assumption and extra initial notification fail, 510.074 s. Original predicates retained. |
| HostFourth | Three affected Release suites pass, 557.366 s; subsequent native lifecycle gap remains unqualified by it. |
| FirstMatrix | Debug 19/19 and 108 exact pairs pass; owned ASan build intentionally stopped after hide/zero-height audit, exit -1, 854.246 s. Not a compiler failure/timeout or final-source qualification. |
| HostFifth | Four affected Release suites pass, 545.276 s, including the native lifecycle fix and WindowHost. |
| Final | Fresh frozen lifecycle-fixed source passes all native/build/resource obligations; earlier source results are not substituted. |

All failed/interrupted receipts, snapshots and private artifact hashes remain
preserved. The fixed timing method body is unchanged but its test-only preparation
helper closure changes; earlier trial observations cannot be pooled as an
unchanged-fixture causal comparator. Canonical five benchmark inputs, judges,
thresholds, tolerances, baselines, device policy and consumer pin remain unchanged.
The original frame-performance HOLD remains open.

Actual read-only Ghidra MCP calls retain eight recalculation routines from the
saved Office reference SHA 7f810b35cd2f818b70475d865ef743860b40cefe463e60d2f1c2de5bd2c6aa41.
Active/open user32 state and the saved program remain unchanged. Pending/step/
completion/retirement inform the owned design; private ABI/flags/timer/thread
assumptions are not copied. Raw warnings and exact exports remain in the packet.

The requested [MD4C](https://github.com/mity/md4c),
[Discount](https://github.com/Orc/discount) and
[cmark-gfm](https://github.com/github/cmark-gfm) public README/API/license inputs
are pinned by commit/URL/hash in the packet and discussed in the rich-text guide.
Callbacks, compile/output separation, AST/source positions and extension APIs
inform later work; abort/feed/finish do not prove resumable parsing. No parser
dependency or copied implementation is adopted.

This component is complete; the overall WIP plan stays active. Full lossless
CommonMark/GFM and semantic transactions, math/Mermaid/footnotes/front matter,
source-visible pending/error presentation, resumable parsing/long paragraphs,
capacity/latency, physical IME/AT, agreed focus-taking tests, native ARM64 execution,
consumer adoption and final performance acceptance remain explicit later gates.
Docs/gallery review retains the existing assets after review of the unchanged
catalog/example/styles and small one-turn gallery fixture. Independent exact
raster/geometry oracles validate the completed control; no baseline is rebaselined.
'@
$culture=[Globalization.CultureInfo]::InvariantCulture
$component=$component.Replace('__MATRIX_SECONDS__',$matrix.elapsedSeconds.ToString('F3',$culture)).Replace('__TOOLING_SECONDS__',$tooling.elapsedSeconds.ToString('F3',$culture))
[IO.File]::WriteAllText($plan,$text.Substring(0,$offset)+$component+"`n")
[IO.File]::WriteAllText((Join-Path $Root 'Changes/2026-10-18-cooperative-rich-preparation.md'),"- Add cooperative Unicode rich-text preparation through shared native/embedded owner turns, with cancellable/coalesced requests, stale-key rejection and pending-safe source/IME/UIA behavior.`n  Cover native hide/zero-size lifecycle, failure/resource/pixel scenarios and all required build profiles; update contracts, usage and preserved evidence. Full grammar and performance acceptance remain separate.`n")
@{classification='Post-qualification prose closeout only; compiled code/test bytes match the frozen manifest';matrixSeconds=$matrix.elapsedSeconds;toolingSeconds=$tooling.elapsedSeconds;completedUtc=[datetime]::UtcNow.ToString('o')}|
 ConvertTo-Json|Set-Content -LiteralPath (Join-Path $work 'documentation-closeout.receipt.txt')
Write-Host 'Cooperative component docs reconciled; overall WIP and performance HOLD remain open.'
