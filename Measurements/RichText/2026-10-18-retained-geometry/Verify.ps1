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
foreach($line in Get-Content -LiteralPath (Join-Path $Packet 'SHA256SUMS')){
 if($line -cnotmatch '^([A-F0-9]{64})  (.+)$'){throw 'Malformed seal.'}
 if(-not$sealed.Add($Matches[2])){throw 'Duplicate seal entry.'};Assert-Hash $Matches[2] $Matches[1]
}
foreach($file in Get-ChildItem -LiteralPath $Packet -Recurse -File){
 $relative=[IO.Path]::GetRelativePath($Packet,$file.FullName).Replace('\','/')
 if($relative -cne 'SHA256SUMS' -and -not$sealed.Contains($relative)){throw 'Unsealed extra file.'}
 if($file.Extension -in @('.exe','.dll','.pdb','.lib','.pyc') -or $relative -match '(^|/)__pycache__/'){throw 'Ordinary build products or incidental bytecode in reviewed packet.'}
}
$inventory=Read-Receipt 'inventory.receipt.txt'
if($inventory.sourceFingerprint -cne '4927C4EE251F2845B3BBD9E9534CF0B1C2C50A790A7C68345EAC3C38F9271B9C' -or
 $inventory.nativeSuiteCount -ne 57 -or $inventory.nativeSuites.Count -ne 57){throw 'Fixed source/suite inventory differs.'}
foreach($copy in $inventory.copies){Assert-Hash $copy.path $copy.sha256}
$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach($entry in $inventory.nativeSuites){
 if(-not$seen.Add($entry.suite+'|'+$entry.configuration)){throw 'Duplicate native suite/profile.'}
 $native=Read-Receipt "Native/$($entry.suite)-x64-$($entry.configuration).receipt.txt"
 $performance=Read-Receipt ('Performance/'+$entry.performance)
 $build=Read-Receipt ('Build/x64-'+$entry.configuration.Replace(' ','')+'-build.receipt.txt')
 if($native.exitCode -ne 0 -or $native.skips.Count -or $native.Contains('tests') -or $native.sha256 -cne $entry.sha256 -or
  $native.suite -cne $entry.suite -or $native.configuration -cne $entry.configuration -or $native.nativeArchitecture -cne 'X64'){throw 'Native success identity differs.'}
 if($performance.sourceFingerprint -cne $inventory.sourceFingerprint -or $performance.configuration -cne $entry.configuration -or
  $performance.executableSha256 -cne $native.performanceExecutableSha256 -or $performance.buildAttestationIdentity -cne $build.identity -or $build.outcome -cne 'PASSED'){throw 'Native/performance/build binding differs.'}
 if((ConvertTo-Json $native.performanceScenarios -Depth 16 -Compress) -cne (ConvertTo-Json $performance.scenarios -Depth 16 -Compress)){throw 'Original frame values differ.'}
}
$completion=Read-Receipt 'Raw/Final02/qualification-completion.receipt.txt'
if($completion.scheduled -ne 6 -or $completion.attempted -ne 6 -or $completion.completed -ne 6 -or $completion.scheduledOperations -ne 12 -or
 $completion.attemptedOperations -ne 12 -or $completion.operations.Count -ne 12 -or $completion.elapsedSeconds -gt 7200 -or
 @($completion.operations|Where-Object{$_.exitCode -ne 0 -or $_.timedOut -or -not$_.drained}).Count -or
 @($completion.profiles|Where-Object{$_.exitCode -ne 0 -or $_.elapsedSeconds -gt 1800}).Count){throw 'Final fixed matrix is incomplete or outside its bounds.'}
foreach($platform in @('x64','ARM64')){foreach($configuration in @('Release','Debug','ASanDebug')){
 if((Read-Receipt "Build/$platform-$configuration-build.receipt.txt").outcome -cne 'PASSED'){throw 'Required build missing.'}
}}
foreach($configuration in @('Debug','ASan Debug')){
 $stem='x64-'+$configuration.Replace(' ','-')
 $build=Read-Receipt ('Build/x64-'+$configuration.Replace(' ','')+'-build.receipt.txt')
 $native=Read-Receipt "Native/Embedded-x64-$configuration.receipt.txt"
 foreach($probe in @('layout-clear','geometry')){
  $record=Read-Receipt "Raw/Final02/$stem-$probe-probe.receipt.txt"
  if($record.exitCode -ne 0 -or $record.configuration -cne $configuration -or $record.sha256 -cne $native.sha256 -or
   $record.buildAttestationIdentity -cne $build.identity){throw 'Isolated sweep artifact/build binding differs.'}
 }
 $lines=Get-Content -LiteralPath (Join-Path $Packet "Raw/Final02/$stem-geometry-probe.log")
 $trace=@(Get-Content -LiteralPath (Join-Path $Packet "Raw/Final02/$stem-geometry-probe.err"))
 $cursor=0
 foreach($pair in @(@('^rich layout copy allocation-failure sweptRequests=(\d+)$','rich layout copy failure-point='),
  @('^rich document geometry allocation-failure whole=0 sweptRequests=(\d+)$','rich geometry failure-point='),
  @('^rich document geometry allocation-failure whole=1 sweptRequests=(\d+)$','rich geometry failure-point='))){
  $observed=@($lines|Where-Object{$_ -match $pair[0]})
  if($observed.Count -ne 1 -or $observed[0] -notmatch $pair[0]){throw 'Isolated sweep count missing.'};$count=[int]$Matches[1]
  if($count -le 0){throw 'Empty allocation schedule.'}
  for($index=0;$index -lt $count;++$index){if($cursor -ge $trace.Count -or $trace[$cursor++] -cne ($pair[1]+$index)){throw 'Allocation trace is not complete/contiguous.'}}
 }
 if($cursor -ne $trace.Count){throw 'Unexpected isolated diagnostic output.'}
 if(-not(Select-String -LiteralPath (Join-Path $Packet "Raw/Final02/$stem-layout-clear-probe.log") -SimpleMatch 'rich layout clear CppAllocations=0 CppRequestedBytes=0')){throw 'Nonallocating clear missing.'}
}
foreach($configuration in @('Release','Debug','ASan Debug')){
 $stem='x64-'+$configuration.Replace(' ','-');$rasterDirectory=Join-Path $Packet "Raw/Final02/$stem/Raster"
 $actual=@(Get-ChildItem -LiteralPath $rasterDirectory -Filter '*-actual.png');$expected=@(Get-ChildItem -LiteralPath $rasterDirectory -Filter '*-expected.png')
 if($actual.Count -ne 54 -or $expected.Count -ne 54){throw 'Final 54-pair WARP coverage differs.'}
 foreach($file in $actual){if((Get-FileHash $file.FullName).Hash -cne (Get-FileHash (Join-Path $rasterDirectory $file.Name.Replace('-actual.png','-expected.png'))).Hash){throw 'Final WARP pair differs.'}}
 $log=Get-Content -LiteralPath (Join-Path $Packet "Native/Embedded-x64-$configuration.log")
 foreach($line in @('rich layout clear CppAllocations=0 CppRequestedBytes=0','rich geometry unchanged repeats=4 lines=512 CppAllocations=0 CppRequestedBytes=0')){
  if($line -notin $log){throw 'Required zero-allocation observation missing.'}
 }
 $replacement=@($log|Where-Object{$_ -match '^rich geometry replacement lines=1024 edits=512 retainedCppBytes=(\d+) peakCppBytes=(\d+) releasedDelta=0$'})
 if($replacement.Count -ne 1 -or $replacement[0] -notmatch '^rich geometry replacement lines=1024 edits=512 retainedCppBytes=(\d+) peakCppBytes=(\d+) releasedDelta=0$' -or
  [long]$Matches[1] -gt 8388608 -or [long]$Matches[2] -gt 16777216 -or [long]$Matches[2] -lt [long]$Matches[1]){throw 'Retained/replacement peak budget differs.'}
 for($kind=0;$kind -lt 3;++$kind){
  $pattern='^rich geometry maximum kind='+$kind+' sourceUnits=65536 chunks=(\d+) retainedCppBytes=(\d+) releasedDelta=0$'
  $observed=@($log|Where-Object{$_ -match $pattern})
  if($observed.Count -ne 1 -or $observed[0] -notmatch $pattern -or [long]$Matches[1] -gt 128 -or [long]$Matches[2] -gt 25165824){throw 'Maximum-source resource budget differs.'}
 }
}
$failed=Read-Receipt 'Raw/Final/qualification-completion.receipt.txt'
if($failed.completed -ne 1 -or $failed.attempted -ne 2 -or $failed.attemptedOperations -ne 4 -or $failed.operations[-1].exitCode -ne 197){throw 'Original failed matrix was lost.'}
$stack=Get-Content -LiteralPath (Join-Path $Packet 'Raw/StackProbe/debug-before-probe.err') -Raw
if(-not$stack.Contains('rich geometry failure-point=173') -or -not$stack.Contains('Projection::Projection') -or -not$stack.Contains('JoinBlocks')){throw 'Original Debug fault stack missing.'}
$asan=Read-Receipt 'Native/AddressSanitizer-x64.receipt.txt'
if(-not$asan.detected -or $asan.exitCode -eq 0 -or -not(Select-String -LiteralPath (Join-Path $Packet 'Native/AddressSanitizer-x64.log') -SimpleMatch 'AddressSanitizer: heap-use-after-free')){throw 'Specific ASan runtime diagnosis missing.'}
$tooling=Read-Receipt 'Raw/ToolingFinal/tooling-completion.receipt.txt'
if($tooling.scheduled -ne 2 -or $tooling.attempted -ne 2 -or $tooling.elapsedSeconds -gt 900 -or
 @($tooling.results|Where-Object{$_.exitCode -ne 0 -or $_.timedOut -or -not$_.drained}).Count -or
 -not(Select-String -LiteralPath (Join-Path $Packet 'Raw/ToolingFinal/validation-full.log') -SimpleMatch 'All validation passed.')){throw 'Full validation/format evidence differs.'}
$start=Read-Receipt 'Raw/Final02/qualified-source.receipt.txt'
$end=Read-Receipt 'Raw/Final02/frozen-inputs-end.receipt.txt'
if($start.files.Count -ne 22 -or $end.fileCount -ne 22 -or $start.baseCommit -cne $end.baseCommit){throw 'Frozen input inventory differs.'}
Assert-Hash 'Raw/Final02/qualified-source.receipt.txt' $end.manifestSha256
Assert-Hash 'Raw/Final02/qualification-completion.receipt.txt' $end.matrixSha256
foreach($inputFile in $start.files){
 $relative='Raw/Final02/QualifiedSource/'+$inputFile.path
 if($relative.EndsWith('.json')){$relative=$relative.Substring(0,$relative.Length-5)+'.receipt.txt'}
 if($relative -match '\.(md|vcxproj|sln|props|targets|filters)$'){$relative+='.receipt.txt'}
 Assert-Hash $relative $inputFile.sha256
}
function Get-MeasuredMethod([string]$Relative){
 $text=[IO.File]::ReadAllText((Join-Path $Packet $Relative))
 $match=[regex]::Match($text,'void TestRichTextEditorRetainedPreparationFixedTrials\(')
 if(-not$match.Success){throw 'Measured method missing.'};$first=$text.IndexOf('{',$match.Index);$depth=0
 for($index=$first;$index -lt $text.Length;++$index){
  if($text[$index] -eq '{'){++$depth}elseif($text[$index] -eq '}'){if(--$depth -eq 0){return $text.Substring($match.Index,$index+1-$match.Index)}}
 }
 throw 'Unbalanced measured method.'
}
$trials=Read-Receipt 'Raw/Final02/native-trials.receipt.txt'
Assert-Hash 'Raw/Before/before-native.log' $trials.beforeLogSha256
Assert-Hash 'Native/EditorControls-x64-Release.log' $trials.afterLogSha256
$beforeMethod=Get-MeasuredMethod 'Raw/Before/Source/Tests/Controls/Scope.Tests.RichTextEditor.cpp'
$afterMethod=Get-MeasuredMethod 'Source/Tests/Controls/Scope.Tests.RichTextEditor.cpp'
if($beforeMethod -cne $afterMethod -or $trials.methodSha256 -cne '38F507301DAB9D0A8612D20365ECD09F856209E7611575084665E33DF76DDB91' -or
 [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($beforeMethod))) -cne $trials.methodSha256){throw 'Fixed native method changed.'}
foreach($side in @('before','after')){
 $log=if($side -ceq 'before'){'Raw/Before/before-native.log'}else{'Native/EditorControls-x64-Release.log'}
 $observations=@(foreach($line in Get-Content (Join-Path $Packet $log)){
  if($line -match '^rich retained preparation trial=(\d+) sourceUnits=26624 edits=128 initialMs=([\d.]+) editMs=([\d.]+)$'){
   @{trial=[int]$Matches[1];initialMilliseconds=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture);
    editMilliseconds=[double]::Parse($Matches[3],[Globalization.CultureInfo]::InvariantCulture)}
  }
 })
 if($observations.Count -ne 5 -or (@($observations.trial|Sort-Object)-join ',') -cne '0,1,2,3,4'){throw 'Native timing sample inventory differs.'}
 for($index=0;$index -lt 5;++$index){
  $original=$trials[$side].samples[$index]
  if($original.trial -ne $observations[$index].trial -or $original.initialMilliseconds -ne $observations[$index].initialMilliseconds -or
   $original.editMilliseconds -ne $observations[$index].editMilliseconds){throw 'Original native observation changed.'}
 }
 if($trials[$side].initialMedianMilliseconds -ne @($observations.initialMilliseconds|Sort-Object)[2] -or
  $trials[$side].editMedianMilliseconds -ne @($observations.editMilliseconds|Sort-Object)[2]){throw 'Native timing median arithmetic differs.'}
}
$beforeNative=Read-Receipt 'Raw/Before/before-native.receipt.txt'
$beforeRun=Read-Receipt 'Raw/Before/before-completion.receipt.txt'
if($beforeNative.sha256 -cne 'FA9D9BB50C398E15DCE4B0450FB47E48B5F48D39F334CAB9C2C859FDD1A2A373' -or $beforeNative.exitCode -ne 0 -or
 $beforeNative.skips.Count -or $beforeRun.exitCode -ne 0 -or $beforeRun.timedOut -or -not$beforeRun.drained -or $beforeRun.elapsedSeconds -gt 900){throw 'Original filtered native baseline differs.'}
Write-Host 'Verified guarded geometry: 57 native suite successes, six builds, complete failure sweeps, 162 WARP pairs and fixed resource budgets. Original failures remain retained; performance and remaining feature gates stay open.'
