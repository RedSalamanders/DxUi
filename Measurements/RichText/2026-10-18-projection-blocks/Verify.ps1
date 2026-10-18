[CmdletBinding()] param([string]$Packet=$PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
function Read-Receipt([string]$Relative){Get-Content -LiteralPath (Join-Path $Packet $Relative) -Raw|ConvertFrom-Json -AsHashtable -Depth 64}
function Assert-Hash([string]$Relative,[string]$Expected){
 $path=[IO.Path]::GetFullPath((Join-Path $Packet $Relative))
 if(-not$path.StartsWith(([IO.Path]::GetFullPath($Packet)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)){throw 'Seal escapes packet.'}
 if((Get-FileHash -LiteralPath $path).Hash -cne $Expected){throw "SHA-256 differs: $Relative"}
}
foreach($line in Get-Content -LiteralPath (Join-Path $Packet 'SHA256SUMS')){
 if($line -cnotmatch '^([A-F0-9]{64})  (.+)$'){throw 'Malformed seal.'}
 Assert-Hash $Matches[2] $Matches[1]
}
$inventory=Read-Receipt 'inventory.receipt.txt'
if($inventory.nativeSuiteCount -ne 57 -or $inventory.nativeSuites.Count -ne 57 -or $inventory.canonicalReportCount -ne 6 -or
 $inventory.sourceFingerprint -cne '574862352141C18EDBC27BEB54037E53B2566E8D8663891511F087872965B9F0'){throw 'Fixed packet inventory differs.'}
foreach($copy in $inventory.copies){Assert-Hash $copy.path $copy.sha256}
foreach($entry in $inventory.nativeSuites){
 $native=Read-Receipt "Native/$($entry.suite)-x64-$($entry.configuration).receipt.txt"
 $performance=Read-Receipt ('Performance/'+$entry.performance)
 $build=Read-Receipt ('Build/x64-'+$entry.configuration.Replace(' ','')+'-build.receipt.txt')
 if($native.exitCode -ne 0 -or $native.skips.Count -or $native.Contains('tests') -or $native.sha256 -cne $entry.sha256 -or
  $native.suite -cne $entry.suite -or $native.configuration -cne $entry.configuration -or $native.nativeArchitecture -cne 'X64'){throw 'Native success identity differs.'}
 if($performance.sourceFingerprint -cne $inventory.sourceFingerprint -or $performance.configuration -cne $entry.configuration -or
  $performance.executableSha256 -cne $native.performanceExecutableSha256 -or $performance.buildAttestationIdentity -cne $build.identity -or $build.outcome -cne 'PASSED'){throw 'Native/performance/build binding differs.'}
 if((ConvertTo-Json $native.performanceScenarios -Depth 16 -Compress) -cne (ConvertTo-Json $performance.scenarios -Depth 16 -Compress)){throw 'Original frame values differ.'}
}
$completion=Read-Receipt 'Raw/qualification-completion.receipt.txt'
if($completion.scheduled -ne 6 -or $completion.attempted -ne 6 -or $completion.completed -ne 6 -or $completion.scheduledOperations -ne 12 -or
 $completion.attemptedOperations -ne 12 -or $completion.operations.Count -ne 12 -or $completion.elapsedSeconds -gt 3600 -or
 @($completion.operations|Where-Object{$_.exitCode -ne 0 -or $_.timedOut -or -not$_.drained}).Count -or
 @($completion.profiles|Where-Object{$_.exitCode -ne 0 -or $_.elapsedSeconds -gt 900}).Count){throw 'Final six-profile/twelve-operation matrix differs.'}
foreach($platform in @('x64','ARM64')){foreach($configuration in @('Release','Debug','ASanDebug')){
 if((Read-Receipt "Build/$platform-$configuration-build.receipt.txt").outcome -cne 'PASSED'){throw 'Required build missing.'}
}}
foreach($configuration in @('Debug','ASan Debug')){
 $stem='x64-'+$configuration.Replace(' ','-')
 $build=Read-Receipt ('Build/x64-'+$configuration.Replace(' ','')+'-build.receipt.txt')
 $native=Read-Receipt "Native/Embedded-x64-$configuration.receipt.txt"
 foreach($probe in @('snapshot-slice','projection')){
  $record=Read-Receipt "Raw/$stem-$probe-probe.receipt.txt"
  if($record.exitCode -ne 0 -or $record.configuration -cne $configuration -or $record.sha256 -cne $native.sha256 -or
   $record.buildAttestationIdentity -cne $build.identity){throw 'Final isolated probe binding differs.'}
  $log=Get-Content -LiteralPath (Join-Path $Packet "Raw/$stem-$probe-probe.log")
  $pattern=if($probe -ceq 'projection'){'^rich projection allocation-failure sweptRequests=(\d+)$'}else{'^rich snapshot slice allocation-failure sweptRequests=(\d+)$'}
  $matchesFound=@($log|Where-Object{$_ -match $pattern})
  if($matchesFound.Count -ne 1 -or $matchesFound[0] -notmatch $pattern){throw 'Isolated sweep count missing.'}
  $count=[int]$Matches[1]
  $trace=@(Get-Content -LiteralPath (Join-Path $Packet "Raw/$stem-$probe-probe.err"))
  if($count -le 0 -or $trace.Count -ne $count){throw 'Isolated trace count differs.'}
  for($index=0;$index -lt $count;++$index){
   $prefix=if($probe -ceq 'projection'){'rich projection failure-point='}else{'rich snapshot slice failure-point='}
   if($trace[$index] -cne ($prefix+$index)){throw 'Isolated failure points are not contiguous.'}
  }
 }
}
$negative=Read-Receipt 'Raw/debug-before-completion.receipt.txt'
$negativeBuild=Read-Receipt 'Raw/debug-before-build.receipt.txt'
$negativeProbe=Read-Receipt 'Raw/debug-before-probe.receipt.txt'
if($negative.attempted -ne 2 -or $negative.scheduled -ne 2 -or $negative.elapsedSeconds -gt 900 -or
 $negative.results[0].exitCode -ne 0 -or $negative.results[1].exitCode -ne 197 -or
 @($negative.results|Where-Object{$_.timedOut -or -not$_.drained}).Count -or $negativeBuild.outcome -cne 'PASSED' -or
 $negativeProbe.buildAttestationIdentity -cne $negativeBuild.identity -or $negativeProbe.exitCode -ne 197 -or
 $negativeProbe.sha256 -cne '3251C459ADD4606CD982BE07D28FFE280A6043AA97999995637833D7F6096038'){throw 'Original Debug negative diagnostic differs.'}
$errorLines=Get-Content -LiteralPath (Join-Path $Packet 'Raw/debug-before-probe.err')
if($errorLines.Count -ne 2 -or $errorLines[0] -cne 'rich snapshot slice failure-point=0' -or
 $errorLines[1] -cne 'rich preparation probe: unexpected noexcept termination'){throw 'Original noexcept point-zero failure missing.'}
foreach($pair in @(@('Raw/','C988499564EDE702B5A357BCB4483648B8B68E5618032ED751195AAFB6AF8075'),
 @('Raw/BeforeRefined/','1367502BA9E4CA2E4F3CD47DD18A2DD1DA0D95EEB5E94B00099B64E932020ACD'))){
 $process=Read-Receipt ($pair[0]+'before-completion.receipt.txt')
 $native=Read-Receipt ($pair[0]+'before-native.receipt.txt')
 $report=Read-Receipt ('Performance/'+(Split-Path $native.performanceReport -Leaf))
 if($process.exitCode -ne 1 -or $process.timedOut -or -not$process.drained -or $process.elapsedSeconds -gt 600 -or
  $native.exitCode -ne 1 -or $report.sourceFingerprint -cne $pair[1]){throw 'Original capability baseline differs.'}
 if(-not(Select-String -LiteralPath (Join-Path $Packet ($pair[0]+'before-native.log')) -SimpleMatch 'FAILED: single-line edit rebuilds its line and two neighbors rather than the complete source')){throw 'Original reuse failure missing.'}
}
foreach($scope in @('EditorControls','Embedded','Accessibility')){
 $native=Read-Receipt "Raw/FirstCandidate/$scope.receipt.txt"
 $report=Read-Receipt ('Performance/'+(Split-Path $native.performanceReport -Leaf))
 if($native.exitCode -ne 0 -or $native.skips.Count -or $report.sourceFingerprint -cne '84AF430147BB108BA7207B280BE280F942B4299A072599DD48EFB0B741AA74D9'){throw 'Preliminary Release outcome differs.'}
}
function Get-Method([string]$Path,[string]$Name){
 $text=[IO.File]::ReadAllText($Path);$match=[regex]::Match($text,'void '+[regex]::Escape($Name)+'\(')
 if(-not$match.Success){throw "Method missing: $Name"}
 $first=$text.IndexOf('{',$match.Index);$depth=0
 for($index=$first;$index -lt $text.Length;++$index){
  if($text[$index] -eq '{'){++$depth}
  elseif($text[$index] -eq '}'){if(--$depth -eq 0){return $text.Substring($match.Index,$index+1-$match.Index)}}
 }
 throw 'Unbalanced method.'
}
foreach($method in (Read-Receipt 'Raw/unchanged-measured-methods.receipt.txt')){
 foreach($prefix in @('Raw/BeforeRefined/BeforeSource/','Source/')){
  $body=Get-Method (Join-Path $Packet ($prefix+$method.file)) $method.method
  if([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($body))) -cne $method.sha256){throw 'A measured method changed.'}
 }
}
foreach($configuration in @('Release','Debug','ASan Debug')){
 $lines=Get-Content -LiteralPath (Join-Path $Packet "Native/Embedded-x64-$configuration.log")
 if(-not($lines -contains 'rich projection mappings repeats=4 lines=512 CppAllocations=0 CppRequestedBytes=0')){throw 'Mapping/unchanged zero-allocation observation missing.'}
 foreach($pattern in @('^rich snapshot slice allocation-failure sweptRequests=[1-9][0-9]*$','^rich projection allocation-failure sweptRequests=[1-9][0-9]*$')){
  if(-not($lines -match $pattern)){throw 'Ordinary source/projection failure sweep missing.'}
 }
 foreach($fixture in @(@('^rich projection retained lines=1024 edits=512 retainedCppBytes=(\d+) releasedDelta=0$',4194304),
  @('^rich projection maximum kind=0 sourceUnits=65536 retainedCppBytes=(\d+) releasedDelta=0$',16777216),
  @('^rich projection maximum kind=1 sourceUnits=65536 retainedCppBytes=(\d+) releasedDelta=0$',16777216))){
  $observed=@($lines|Where-Object{$_ -match $fixture[0]})
  if($observed.Count -ne 1 -or $observed[0] -notmatch $fixture[0] -or [long]$Matches[1] -gt $fixture[1]){throw 'Retained/released projection budget differs.'}
 }
 $editor=Join-Path $Packet "Native/EditorControls-x64-$configuration.log"
 foreach($name in @('TestRichProjectionWholeSourceFixedTrials','TestRichProjectionBlockFixedTrials','TestRichProjectionBlockFullEquivalence',
  'TestRichProjectionBlockReuseAndLineJoins','TestRichProjectionBlockUnicodeEditOracle','TestRichProjectionBlockFallbackModesAndLifetime',
  'TestRichProjectionBlockMaximumSource','TestRichProjectionBlockBranchesAndRetirement')){
  if(-not(Select-String -LiteralPath $editor -SimpleMatch "[DONE] $name")){throw 'Projection behavior oracle missing.'}
 }
}
$tooling=Read-Receipt 'Raw/tooling-completion.receipt.txt'
if($tooling.attempted -ne 2 -or $tooling.scheduled -ne 2 -or $tooling.elapsedSeconds -gt 900 -or
 @($tooling.results|Where-Object{$_.exitCode -ne 0 -or $_.timedOut -or -not$_.drained}).Count -or
 -not(Select-String -LiteralPath (Join-Path $Packet 'Raw/validation-full.log') -SimpleMatch 'All validation passed.')){throw 'Full tooling/format qualification differs.'}
$probe=Read-Receipt 'Native/AddressSanitizer-x64.receipt.txt'
if(-not$probe.detected -or $probe.exitCode -eq 0 -or -not(Select-String -LiteralPath (Join-Path $Packet 'Native/AddressSanitizer-x64.log') -SimpleMatch 'AddressSanitizer: heap-use-after-free')){throw 'Specific ASan runtime diagnosis missing.'}
Write-Host 'Verified 57 final native successes, six builds, four successful isolated sweeps, original capability and noexcept failures, unchanged methods and resource bounds.'
Write-Host 'Operation timing samples are descriptive; retained geometry/shared scheduling, physical/platform gates and frame-performance HOLD remain open.'
