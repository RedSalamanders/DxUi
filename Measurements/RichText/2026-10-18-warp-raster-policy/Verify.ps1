[CmdletBinding()]param([string]$Packet=$PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$Packet=[IO.Path]::GetFullPath($Packet)
function Read-Receipt([string]$Relative){Get-Content -LiteralPath (Join-Path $Packet $Relative) -Raw|ConvertFrom-Json -AsHashtable -Depth 64}
function Assert-Hash([string]$Relative,[string]$Expected){
 $path=[IO.Path]::GetFullPath((Join-Path $Packet $Relative))
 if(-not$path.StartsWith(($Packet+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)){throw 'Seal path escapes packet.'}
 if((Get-FileHash -LiteralPath $path).Hash -cne $Expected){throw "SHA-256 differs: $Relative"}
}
$sealed=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($line in Get-Content (Join-Path $Packet 'SHA256SUMS')){
 if($line -cnotmatch '^([A-F0-9]{64})  (.+)$'){throw 'Malformed seal.'}
 if(-not$sealed.Add($Matches[2])){throw 'Duplicate seal entry.'};Assert-Hash $Matches[2] $Matches[1]
}
foreach($file in Get-ChildItem -LiteralPath $Packet -Recurse -File){
 $relative=[IO.Path]::GetRelativePath($Packet,$file.FullName).Replace('\','/')
 if($relative -cne 'SHA256SUMS' -and -not$sealed.Contains($relative)){throw 'Unsealed extra file.'}
 if($file.Extension -in @('.exe','.dll','.pdb','.lib','.pyc')){throw 'Build artifact or incidental bytecode in reviewed packet.'}
}
$inventory=Read-Receipt 'inventory.receipt.txt'
Add-Type -AssemblyName System.Text.Json
foreach($copy in $inventory.copies){Assert-Hash $copy.path $copy.sha256}
function Work-Path([string]$Relative){
 $matches=@($inventory.copies|Where-Object workPath -CEQ $Relative)
 if(-not$matches.Count){throw "Archived work input missing: $Relative"}
 return $matches[0].path
}
function Work-Receipt([string]$Relative){Read-Receipt (Work-Path $Relative)}
$matrix=Work-Receipt 'Final/qualification-completion.receipt.txt'
$frozen=Work-Receipt 'Final/qualified-source.receipt.txt'
$end=Work-Receipt 'Final/frozen-inputs-end.receipt.txt'
if($matrix.completed -ne 6 -or $matrix.scheduled -ne 6 -or $matrix.attemptedOperations -ne 14 -or $matrix.operations.Count -ne 14 -or
 $matrix.elapsedSeconds -gt 7200 -or $end.fileCount -ne $frozen.files.Count){throw 'Frozen qualification inventory differs.'}
Assert-Hash (Work-Path 'Final/qualified-source.receipt.txt') $end.manifestSha256
Assert-Hash (Work-Path 'Final/qualification-completion.receipt.txt') $end.matrixSha256
foreach($operation in $matrix.operations){
 if($operation.exitCode -ne 0 -or $operation.timedOut -or -not$operation.drained){throw 'Qualification operation failed.'}
}
foreach($file in $frozen.files){Assert-Hash (Work-Path ('Final/QualifiedSource/'+$file.path)) $file.sha256}
if($inventory.nativeSuiteCount -ne 57 -or $inventory.nativeSuites.Count -ne 57){throw 'Native suite count differs.'}
$nativeKeys=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach($entry in $inventory.nativeSuites){
 if(-not$nativeKeys.Add($entry.configuration+'/'+$entry.suite)){throw 'Duplicate native qualification receipt.'}
 $native=Read-Receipt ('Native/'+$entry.suite+'-x64-'+$entry.configuration+'.receipt.txt')
 if($native.suite -cne $entry.suite -or $native.configuration -cne $entry.configuration -or $native.platform -cne 'x64' -or
  $native.nativeArchitecture -cne 'X64' -or $native.exitCode -ne 0 -or $native.skips.Count -or $native.Contains('tests') -or
  $native.sha256 -cne $entry.sha256){throw 'Ordinary native receipt differs or has skips/selection.'}
 $canonicalCopies=@($inventory.copies|Where-Object { [IO.Path]::GetFileName($_.source.Replace('\','/')) -ceq $entry.performance })
 if(-not$canonicalCopies.Count){throw 'Native canonical diagnostic missing.'}
 $canonical=Read-Receipt $canonicalCopies[0].path
 if($canonical.sourceFingerprint -cne '4927C4EE251F2845B3BBD9E9534CF0B1C2C50A790A7C68345EAC3C38F9271B9C' -or
  $canonical.configuration -cne $entry.configuration -or $canonical.executableSha256 -cne $native.performanceExecutableSha256){throw 'Native/canonical identity differs.'}
 $nativeValues=[Text.Json.Nodes.JsonNode]::Parse((ConvertTo-Json $native.performanceScenarios -Depth 48 -Compress))
 $canonicalValues=[Text.Json.Nodes.JsonNode]::Parse((ConvertTo-Json $canonical.scenarios -Depth 48 -Compress))
 if(-not[Text.Json.Nodes.JsonNode]::DeepEquals($nativeValues,$canonicalValues)){throw 'Native original canonical values differ.'}
}
foreach($configuration in @('Debug','Release','ASan Debug')){
 if(@($inventory.nativeSuites|Where-Object configuration -CEQ $configuration).Count -ne 19){throw 'Native profile scope inventory differs.'}
 foreach($platform in @('x64','ARM64')){
  $build=Read-Receipt ('Build/'+$platform+'-'+$configuration.Replace(' ','')+'-build.receipt.txt')
  if($build.outcome -cne 'PASSED' -or -not$build.identity){throw 'Root profile build did not pass.'}
 }
}
foreach($profile in @('x64-Debug','x64-Release','x64-ASan-Debug')){
 $configuration=$profile.Substring(4).Replace('-',' ')
 $log=[IO.File]::ReadAllText((Join-Path $Packet ('Native/EditorControls-x64-'+$configuration+'.log')))
 if($log.Contains('[START] TestRichTextEditorRetainedPreparationFixedTrials') -or $log.Contains('rich retained preparation trial=')){
  throw 'Repeated timing leaked into an ordinary suite.'
 }
}
$tooling=Work-Receipt 'ToolingFinal/tooling-completion.receipt.txt'
if($tooling.scheduled -ne 2 -or $tooling.attempted -ne 2 -or $tooling.elapsedSeconds -gt 900){throw 'Tooling inventory differs.'}
foreach($step in $tooling.results){if($step.exitCode -ne 0 -or $step.timedOut -or -not$step.drained){throw 'Tooling operation failed.'}}
$cli=Work-Receipt 'Cli/cli-refusal.receipt.txt'
if($cli.scheduled -ne 4 -or $cli.attempted -ne 4){throw 'Native refusal inventory differs.'}
foreach($case in $cli.cases){
 if($case.exitCode -ne 1 -or $case.timedOut -or -not$case.drained -or -not$case.expectedMessage -or -not$case.noNativeReportOrCapture){throw 'Invalid native request was accepted.'}
}
$plan=Work-Receipt 'Controls/plan.receipt.txt'
$completion=Work-Receipt 'Controls/completion.receipt.txt'
$analysis=Work-Receipt 'Controls/analysis.receipt.txt'
foreach($path in $plan.anchor.profilingInputs.Keys){Assert-Hash ('Source/'+$path) $plan.anchor.profilingInputs[$path]}
Import-Module (Join-Path $Packet 'Source/Tools/PerformanceProfiling.psm1') -Force
if($plan.anchor.sourceFingerprint -cne '4927C4EE251F2845B3BBD9E9534CF0B1C2C50A790A7C68345EAC3C38F9271B9C' -or
 $plan.anchor.benchmarkSha256 -cne 'E9DE41251A9186D4EC25F8178C4398E6E80C14E80FF948251067B1465549047E'){throw 'Source/canonical fixture identity differs.'}
if($completion.scheduled -ne 4 -or $completion.attempted -ne 4 -or $completion.elapsedSeconds -gt 900 -or $analysis.cases.Count -ne 4 -or
 ($completion.cases.mode -join ',') -cne 'Default,CallingThread,CallingThread,Default'){throw 'Fixed device-policy schedule differs.'}
foreach($case in $completion.cases){
 if($case.exitCode -ne 0 -or $case.timedOut -or -not$case.drained -or -not$case.validationPassed){throw 'Original policy-control child failed.'}
 $relative='Controls/'+$case.report
 Assert-Hash (Work-Path ($relative+'.raw.json')) $case.rawSha256
 Assert-Hash (Work-Path ($relative+'.png')) $case.captureSha256
 if($case.captureSha256 -cne '6DF197E22101D1A71DAE8DFF1354B5B357D16E7E58D6EED74EA85BD14F9000BD'){throw 'Populated capture differs.'}
 $native=Work-Receipt ($relative+'.raw.json');$wrapper=Work-Receipt $relative;$child=Work-Receipt ($relative+'.child.json')
 [void](Assert-BoundaryReport -Receipt $native -Timers $true -WarpRasterMode $case.mode -Configuration Release -ProcessId $case.nativePid)
 $flags=if($case.mode -ceq 'Default'){32}else{40}
 if($native.schema -ne 2 -or $native.fixture -cne 'dxui-complex-ui-boundaries-v2' -or $native.warpRasterMode -cne $case.mode -or
  $native.requestedDeviceFlags -ne $flags -or $native.deviceCreationFlags -ne $flags -or $native.processId -ne $case.nativePid -or
  $native.windows.Count -ne 8 -or $native.probeFrames -ne 320 -or $native.warmFrames -ne 200 -or -not$native.boundaryTimers -or
  $native.hiddenPreparations -ne 0 -or $native.hiddenComposites -ne 0 -or $native.hiddenSurfaceBytes -ne 0){throw 'Native policy fixture differs.'}
 if($child.exitCode -ne 0 -or $child.timedOut -or $child.terminationError -or $child.drainError -or
  ([datetime]$child.completedUtc-[datetime]$child.startedUtc).TotalSeconds -gt 125 -or
  $child.processId -ne $case.nativePid -or $wrapper.rawReportSha256 -cne $case.rawSha256 -or $wrapper.captureSha256 -cne $case.captureSha256){throw 'Owned child/source-byte binding differs.'}
 foreach($key in $plan.anchor.Keys){
  $expected=[Text.Json.Nodes.JsonNode]::Parse((ConvertTo-Json $plan.anchor[$key] -Depth 48 -Compress))
  $actual=[Text.Json.Nodes.JsonNode]::Parse((ConvertTo-Json $wrapper[$key] -Depth 48 -Compress))
  if(-not[Text.Json.Nodes.JsonNode]::DeepEquals($expected,$actual)){throw "Measurement state changed: $key"}
 }
 $previous=[long]0
 for($index=0;$index -lt 8;++$index){
  $window=$native.windows[$index];$kind=if($index -lt 4){$index}else{7-$index}
  if($window.clear -ne ($kind -in @(0,1)) -or $window.composite -ne ($kind -in @(0,2)) -or
   $window.startedQpc -lt $previous -or $window.endedQpc -le $window.startedQpc -or $window.frames.Count -ne 40){throw 'Native window invariant differs.'}
  $previous=[long]$window.startedQpc
  foreach($frame in $window.frames){
   if($frame.cppAllocations -ne 0 -or $frame.qpc.Count -ne 9 -or $frame.qpc[8] -le $frame.qpc[0]){throw 'Native frame invariant differs.'}
   foreach($tick in $frame.qpc){if([long]$tick -lt $previous -or [long]$tick -gt [long]$window.endedQpc){throw 'Native QPC order differs.'};$previous=[long]$tick}
  }
  $previous=[long]$window.endedQpc
 }
}
$legacy=Work-Receipt 'Legacy/legacy.json.raw.json'
$legacyBinding=Work-Receipt 'Legacy/legacy-binding.receipt.txt'
[void](Assert-BoundaryReport -Receipt $legacy -Timers $true -Configuration Release -ProcessId $legacyBinding.nativePid)
if($legacyBinding.schema -ne 1 -or $legacyBinding.nativeExitCode -ne 0 -or $legacyBinding.nativeArguments.Count -ne 3 -or
 $legacyBinding.nativeArguments[2] -cne 'On' -or $legacyBinding.captureSha256 -cne '6DF197E22101D1A71DAE8DFF1354B5B357D16E7E58D6EED74EA85BD14F9000BD'){
 throw 'Legacy schema/argument/capture behavior differs.'
}
Assert-Hash (Work-Path 'Legacy/legacy.json.raw.json') $legacyBinding.rawSha256
Assert-Hash (Work-Path 'Legacy/legacy.json.png') $legacyBinding.captureSha256
$trials=Work-Receipt 'Trials/native-trials.receipt.txt'
$trialCompletion=Work-Receipt 'Trials/completion.receipt.txt'
$selected=Work-Receipt 'Trials/selected-native.receipt.txt'
$trialPlan=Work-Receipt 'Trials/plan.receipt.txt'
$trialRecovery=Work-Receipt 'Trials/collector-recovery.receipt.txt'
Assert-Hash (Work-Path 'Trials/initial-collector-source.ps1.receipt.txt') $trialPlan.sourceSha256
if($trialRecovery.initialCollectorExitCode -ne 1 -or $trialRecovery.originalRootChildExitCode -ne 0 -or
 $trialRecovery.recoveryExitCode -ne 0 -or $trialRecovery.nativeRerun){throw 'Original collector failure or no-rerun recovery was misrepresented.'}
if($trialCompletion.exitCode -ne 0 -or $trialCompletion.timedOut -or -not$trialCompletion.drained -or $trialCompletion.elapsedSeconds -gt 660 -or
 $selected.exitCode -ne 0 -or $selected.skips.Count -or $selected.tests.Count -ne 1 -or
 $selected.tests[0] -cne 'TestRichTextEditorRetainedPreparationFixedTrials' -or
 $trials.methodSha256 -cne '38F507301DAB9D0A8612D20365ECD09F856209E7611575084665E33DF76DDB91' -or
 $trials.after.samples.Count -ne 5 -or (@($trials.after.samples.trial|Sort-Object)-join ',') -cne '0,1,2,3,4'){
 throw 'Explicit unchanged timing diagnostic differs.'
}
Assert-Hash (Work-Path 'Trials/selected-native.log') $trials.afterLogSha256
# Restore only sealed controls into a fresh caller-private directory. Native
# binaries, build scripts and profiling processes are never executed on replay.
$replay=Join-Path ([IO.Path]::GetTempPath()) ('dxui-warp-replay-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $replay|Out-Null
foreach($copy in $inventory.copies|Where-Object { $_.workPath -and $_.workPath.StartsWith('Controls/') }){
 $relative=$copy.workPath.Substring(9);$destination=[IO.Path]::GetFullPath((Join-Path $replay $relative))
 if(-not$destination.StartsWith(($replay+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)){throw 'Replay path escapes destination.'}
 New-Item -ItemType Directory -Path (Split-Path $destination) -Force|Out-Null
 Copy-Item -LiteralPath (Join-Path $Packet $copy.path) -Destination $destination
}
& (Join-Path $Packet (Work-Path 'Analyze-WarpPolicies.ps1')) -OutputDirectory $replay
$replayed=Get-Content (Join-Path $replay 'analysis.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable -Depth 64
$originalJson=[Text.Json.Nodes.JsonNode]::Parse((ConvertTo-Json $analysis.cases -Depth 48 -Compress))
$replayedJson=[Text.Json.Nodes.JsonNode]::Parse((ConvertTo-Json $replayed.cases -Depth 48 -Compress))
if(-not[Text.Json.Nodes.JsonNode]::DeepEquals($originalJson,$replayedJson)){throw 'Pure analysis arithmetic differs.'}
Write-Host 'Verified six profiles, 57 ordinary native successes, opt-in policy controls, exact captures, frozen inputs, CLI refusals and pure analysis replay.'
Write-Host 'Device execution policy is an intervention; scheduler latency and performance acceptance remain unestablished.'
Write-Host "Private replay retained: $replay"
