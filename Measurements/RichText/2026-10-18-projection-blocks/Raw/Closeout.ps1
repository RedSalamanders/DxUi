[CmdletBinding()] param([string]$Root)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$work=Join-Path $Root '.build/reports/rich-incremental-preparation-20261018'
$completion=Get-Content -LiteralPath (Join-Path $work 'qualification-completion.receipt.txt') -Raw|ConvertFrom-Json
$tooling=Get-Content -LiteralPath (Join-Path $work 'tooling-completion.receipt.txt') -Raw|ConvertFrom-Json
if($completion.completed -ne 6 -or $completion.attemptedOperations -ne 12 -or @($completion.operations|Where-Object{$_.exitCode -ne 0 -or $_.timedOut -or -not$_.drained}).Count -or
 $tooling.attempted -ne 2 -or @($tooling.results|Where-Object{$_.exitCode -ne 0 -or $_.timedOut -or -not$_.drained}).Count){throw 'Complete the frozen qualification/tooling matrix first.'}
function Observation([string]$Configuration,[string]$Pattern){
 $lines=Get-Content -LiteralPath (Join-Path $Root ".build/logs/test-Embedded-x64-$Configuration.log")
 $found=@($lines|Where-Object{$_ -match $Pattern})
 if($found.Count -ne 1 -or $found[0] -notmatch $Pattern){throw 'Original resource observation missing.'}
 return [long]$Matches[1]
}
$debugObservations=@();$asanObservations=@()
foreach($configuration in @('Debug','ASan Debug')){
 $observed=@((Observation $configuration '^rich projection retained lines=1024 edits=512 retainedCppBytes=(\d+) releasedDelta=0$'),
  (Observation $configuration '^rich projection maximum kind=0 sourceUnits=65536 retainedCppBytes=(\d+) releasedDelta=0$'),
  (Observation $configuration '^rich projection maximum kind=1 sourceUnits=65536 retainedCppBytes=(\d+) releasedDelta=0$'))
 if($configuration -ceq 'Debug'){$debugObservations=$observed}else{$asanObservations=$observed}
}
if(($debugObservations -join ',') -cne ($asanObservations -join ',')){throw 'Debug/ASan resource values differ; split the explanation columns rather than assuming equality.'}
$elapsed=$completion.elapsedSeconds.ToString('F3',[Globalization.CultureInfo]::InvariantCulture)
$text=[IO.File]::ReadAllText((Join-Path $work 'README.Draft.md'))
$matrix="All **57 final ordinary x64 suite runs** pass with zero native skips across Release, Debug and ASan Debug. All six x64/ARM64 build profiles pass. The fixed schedule completes six profiles / twelve owned operations in $elapsed seconds within 3,600 overall / 900 per profile; Debug/ASan each pass both isolated sweeps before their ordinary suites. ARM64 is cross-build only. The isolated ASan runtime probe actually diagnoses heap-use-after-free; its expected nonzero result is separate from suite success."
$text=$text.Replace('FINAL_MATRIX_PLACEHOLDER',$matrix).Replace('DEBUG_RETAINED',$debugObservations[0].ToString('N0',[Globalization.CultureInfo]::InvariantCulture)).Replace('DEBUG_NEWLINES',$debugObservations[1].ToString('N0',[Globalization.CultureInfo]::InvariantCulture)).Replace('DEBUG_UNIQUE',$debugObservations[2].ToString('N0',[Globalization.CultureInfo]::InvariantCulture))
[IO.File]::WriteAllText((Join-Path $work 'README.Final.md'),$text)
$path=Join-Path $Root 'Specs/Plans/WIP/RichTextEditor_2026-10-17.md'
$text=[IO.File]::ReadAllText($path)
$text=$text.Replace('- [ ] Use those intervals to reuse unchanged projection/paragraph geometry.',
 "- [x] Add the bounded v0 line-block projection service with exact mapping, reuse,`r`n  retirement, failure and retention qualification.`r`n- [ ] Integrate retained paragraph geometry with the qualified projection service.")
$text+=@'

### Qualified v0 projection blocks

The [projection-block packet](../../../Measurements/RichText/2026-10-18-projection-blocks/README.md)
seals library fingerprint 574862352141C18EDBC27BEB54037E53B2566E8D8663891511F087872965B9F0.
All 57 ordinary x64 suites pass with zero native skips, all six build profiles
pass, and all four final isolated Debug/ASan allocation diagnostics return zero.
Full tooling, five validators and pinned formatting pass. Eight new behavior
cases preserve exact text, source units, styles and every ACP/affinity across
edits, line joins/splits, equal-revision branches, reset/expiry and retirement.

Snapshot slicing now propagates Debug allocation failure instead of terminating
inside its empty string default constructor. The original point-zero exit 197
is retained independently; the final slice sweeps cover one Release / two Debug
and ASan allocations. Complete projection sweeps cover 47 / 95 / 95 points and
prove prior-state preservation followed by successful retry. Four mapping/no-op
sweeps allocate zero C++ bytes. Each editing/maximum-source fixture releases its
tracked delta to zero and remains under its unchanged 4 MiB / 16 MiB limits.

Both refined-before/final operation methods remain byte-identical. Descriptive
block initial/edit medians are 1.1589/114.419 ms before and 1.2675/1.9308 after;
initial preparation is slower in this sample. Rebuilt source units across 128
edits fall from 3,921,152 to 11,520, with 130,688 outer slots reused. Flat Project
comparisons and the original unrefined draft are also retained. There are no
same-binary controls, quiet-host proof or frame/timing acceptance claims.

This completes only the supporting projection component. Retained geometry,
shared/cooperative host preparation, the full pinned Markdown/extension engine,
physical input/AT and native ARM64 execution remain open. Keep this plan WIP.
Review the current shared GraphicsDevice and owner preparation callback before
adding host services. DirectWrite formats may not be thread-safe; no assumption
of arbitrary-thread geometry follows merely from a shared factory. The control
still prepares full layout and no consumer pin or gallery input changes here.
'@
$text+="`r`nThe six-profile/twelve-operation schedule completes in $elapsed seconds under its declared limits.`r`n"
[IO.File]::WriteAllText($path,$text)
[IO.File]::AppendAllText((Join-Path $Root 'docs/rich-text.md'),@'

The [projection-block qualification](../Measurements/RichText/2026-10-18-projection-blocks/README.md)
retains the original reuse and Debug noexcept failures, 57 final native suite
successes, six builds, allocation/retention observations and unchanged operation
methods. Snapshot slicing now propagates allocation failure. Valid block mapping
and unchanged preparation allocate zero C++ bytes. The retained editor's geometry,
shared scheduling and complete syntax profiles remain separate plan gates; the
descriptive operation improvements do not resolve frame-performance acceptance.
'@)
Write-Host 'Reconciled final projection evidence and plan/docs without changing compiled inputs.'
