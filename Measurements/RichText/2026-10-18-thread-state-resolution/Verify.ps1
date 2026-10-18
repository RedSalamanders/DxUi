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
foreach($copy in $inventory.copies){Assert-Hash $copy.path $copy.sha256}
$plan=Read-Receipt 'Raw/Controls/plan.receipt.txt'
$completion=Read-Receipt 'Raw/Controls/completion.receipt.txt'
$analysis=Read-Receipt 'Raw/Controls/analysis.receipt.txt'
if($plan.anchor.sourceFingerprint -cne '4927C4EE251F2845B3BBD9E9534CF0B1C2C50A790A7C68345EAC3C38F9271B9C' -or
 $plan.anchor.benchmarkSha256 -cne 'E9DE41251A9186D4EC25F8178C4398E6E80C14E80FF948251067B1465549047E'){throw 'Declared source/canonical fixture identity differs.'}
if($completion.scheduled -ne 4 -or $completion.attempted -ne 4 -or $completion.elapsedSeconds -gt 600 -or $completion.overallBudgetExceeded -or
 $analysis.cases.Count -ne 4 -or ($completion.cases.observer -join ',') -cne 'Off,On,On,Off'){throw 'Fixed observer schedule incomplete/out of bounds.'}
foreach($source in $plan.sourceFiles){Assert-Hash ('Source/'+$source.name) $source.sha256}
foreach($case in $completion.cases){
 if($case.exitCode -ne 0 -or $case.timedOut -or -not$case.identityAndNativeValidationPassed -or
  $case.child.elapsedSeconds -gt 120 -or $case.child.drainError -or $case.child.terminationError){throw 'Original owned child failed.'}
 $path='Raw/Controls/'+$case.nativeReport;Assert-Hash $path $case.nativeSha256;Assert-Hash ($path+'.png') $case.captureSha256
 $native=Read-Receipt $path;$stem=[IO.Path]::GetFileNameWithoutExtension($case.nativeReport).Replace('.native.receipt','')
 $states=Read-Receipt ('Raw/Controls/'+$stem+'.states.receipt.txt')
 if($native.fixture -cne 'dxui-complex-ui-boundaries-v1' -or $native.boundaryTimers -ne $true -or $native.processId -ne $case.child.processId -or
  $native.probeFrames -ne 320 -or $native.warmFrames -ne 200 -or $native.windows.Count -ne 8 -or
  $native.hiddenPreparations -ne 0 -or $native.hiddenComposites -ne 0 -or $native.hiddenSurfaceBytes -ne 0){throw 'Native fixture/no-op identity differs.'}
 foreach($window in $native.windows){
  if($window.frames.Count -ne 40){throw 'Native window frame count differs.'}
  foreach($frame in $window.frames){
   if($frame.cppAllocations -ne 0 -or $frame.qpc.Count -ne 9){throw 'Frame allocation/edge count differs.'}
   for($edge=1;$edge -lt 9;++$edge){if([long]$frame.qpc[$edge] -lt [long]$frame.qpc[$edge-1]){throw 'Native frame QPC is not monotone.'}}
  }
 }
 if($case.observer -ceq 'On'){
  $main=@($states.firstObservedCreations|Where-Object threadId -EQ $native.mainThreadId)
  if(-not$case.mainIdentityVerified -or $main.Count -ne 1 -or [long]$main[0].creation100ns -ne [long]$native.mainThreadCreation100ns -or
   @($states.samples).Count -le 0 -or @($states.samples).Count -ne $case.sampleCount){throw 'Observed main identity/sample inventory differs.'}
 }elseif($case.sampleCount -ne 0 -or @($states.samples).Count){throw 'Disabled observer collected snapshots.'}
 $entry=@($analysis.cases|Where-Object index -EQ $case.index)
 if($entry.Count -ne 1 -or $entry[0].nativeSha256 -cne $case.nativeSha256 -or $entry[0].snapshotCount -ne $case.sampleCount){throw 'Analysis/native identity differs.'}
 Assert-Hash ('Raw/Controls/'+$stem+'.states.receipt.txt') $entry[0].stateReportSha256
}
# Replay only pure analysis into a caller-private directory, never mutate the packet.
$replay=Join-Path ([IO.Path]::GetTempPath()) ('dxui-thread-state-replay-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $replay|Out-Null
foreach($file in Get-ChildItem (Join-Path $Packet 'Raw/Controls') -File){Copy-Item -LiteralPath $file.FullName -Destination $replay}
& (Join-Path $Packet 'Source/Analyze-StateControls.ps1') -OutputDirectory $replay

$original=Read-Receipt 'Raw/Controls/analysis.receipt.txt'
$replayed=Get-Content (Join-Path $replay 'analysis.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable -Depth 64
Add-Type -AssemblyName System.Text.Json
$originalJson=[System.Text.Json.Nodes.JsonNode]::Parse((ConvertTo-Json $original.cases -Depth 32 -Compress))
$replayedJson=[System.Text.Json.Nodes.JsonNode]::Parse((ConvertTo-Json $replayed.cases -Depth 32 -Compress))
if(-not[System.Text.Json.Nodes.JsonNode]::DeepEquals($originalJson,$replayedJson)){throw 'Analysis arithmetic changed.'}
Write-Host 'Verified four same-binary observer controls, 1280 native frames, owned main-thread identity, source/seal bindings and analysis replay.'
Write-Host 'Counts are unweighted observations. Scheduler-event, ready-time, GPU-duration, baseline/candidate and acceptance claims remain unavailable.'
Write-Host "Private replay retained: $replay"
