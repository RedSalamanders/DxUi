[CmdletBinding()] param([string] $Packet=$PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
function Read-Receipt([string] $Relative){Get-Content -LiteralPath (Join-Path $Packet $Relative) -Raw|ConvertFrom-Json -AsHashtable -Depth 64}
function Assert-Hash([string] $Relative,[string] $Expected){
 $target=[IO.Path]::GetFullPath((Join-Path $Packet $Relative))
 if(-not $target.StartsWith(([IO.Path]::GetFullPath($Packet)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)){throw 'Seal escapes packet.'}
 if((Get-FileHash -LiteralPath $target).Hash -cne $Expected){throw "SHA-256 differs: $Relative"}
}
foreach($line in Get-Content -LiteralPath (Join-Path $Packet 'SHA256SUMS')){
 if($line -cnotmatch '^([A-F0-9]{64})  (.+)$'){throw 'Malformed seal.'}
 Assert-Hash $Matches[2] $Matches[1]
}
$inventory=Read-Receipt 'inventory.receipt.txt'
if($inventory.nativeSuiteCount -ne 57 -or $inventory.nativeSuites.Count -ne 57 -or $inventory.canonicalReportCount -ne 6 -or
 $inventory.sourceFingerprint -cne '3FEED65870EA98787EAC461CCB7AA6BA1CD9E5BC73C21684AB650B873517BAC4'){throw 'Fixed packet inventory differs.'}
foreach($copy in $inventory.copies){Assert-Hash $copy.path $copy.sha256}
foreach($entry in $inventory.nativeSuites){
 $stem="$($entry.suite)-x64-$($entry.configuration)"
 $native=Read-Receipt "Native/$stem.receipt.txt"
 $performance=Read-Receipt ('Performance/'+$entry.performance)
 $build=Read-Receipt ('Build/x64-'+$entry.configuration.Replace(' ','')+'-build.receipt.txt')
 if($native.exitCode -ne 0 -or $native.skips.Count -or $native.Contains('tests') -or $native.sha256 -cne $entry.sha256 -or
    $native.suite -cne $entry.suite -or $native.configuration -cne $entry.configuration -or $native.nativeArchitecture -cne 'X64'){throw 'Native success identity differs.'}
 if($performance.sourceFingerprint -cne $inventory.sourceFingerprint -or $performance.configuration -cne $entry.configuration -or
    $performance.executableSha256 -cne $native.performanceExecutableSha256 -or $performance.buildAttestationIdentity -cne $build.identity -or $build.outcome -cne 'PASSED'){throw 'Native/performance/build binding differs.'}
 if((ConvertTo-Json $native.performanceScenarios -Depth 16 -Compress) -cne (ConvertTo-Json $performance.scenarios -Depth 16 -Compress)){throw 'Original frame values differ.'}
}
$completion=Read-Receipt 'Raw/qualification-completion.receipt.txt'
if($completion.scheduled -ne 6 -or $completion.attempted -ne 6 -or $completion.elapsedSeconds -gt 3600 -or
 @($completion.results|Where-Object{$_.exitCode -ne 0 -or $_.timedOut -or -not $_.drained}).Count){throw 'Final matrix is incomplete or out of budget.'}
$tooling=Read-Receipt 'Raw/tooling-completion.receipt.txt'
if($tooling.scheduled -ne 2 -or $tooling.attempted -ne 2 -or $tooling.elapsedSeconds -gt 900 -or
 @($tooling.results|Where-Object{$_.exitCode -ne 0 -or $_.timedOut -or -not $_.drained}).Count -or
 -not(Select-String -LiteralPath (Join-Path $Packet 'Raw/validation-full.log') -SimpleMatch 'All validation passed.')){throw 'Full tooling/format qualification is incomplete.'}
foreach($platform in @('x64','ARM64')){foreach($configuration in @('Release','Debug','ASanDebug')){
 if((Read-Receipt "Build/$platform-$configuration-build.receipt.txt").outcome -cne 'PASSED'){throw 'Required build missing.'}
}}
$before=Read-Receipt 'Raw/before-native.receipt.txt'
$beforeReport=Read-Receipt ('Performance/'+(Split-Path $before.performanceReport -Leaf))
$guard=Read-Receipt 'Raw/before-guard-completion.receipt.txt'
$guardPlan=Read-Receipt 'Raw/before-guard-plan.receipt.txt'
if($before.exitCode -ne 1 -or $beforeReport.sourceFingerprint -cne '8FF24243850627EEAFE981A0031CEE873A7E02274BD9BA38667C546E6AE0A98F' -or
 (Read-Receipt 'Raw/before-completion.receipt.txt').exitCode -ne 1 -or $guard.timedOut -or $guard.elapsedSeconds -gt 600 -or $null -ne $guard.exitCode -or
 $guard.processId -ne $guardPlan.processId -or [datetime]$guardPlan.attachedUtc -le [datetime]$guardPlan.creationUtc){throw 'Original expected failure or attached-guard evidence differs.'}
if(-not(Select-String -LiteralPath (Join-Path $Packet 'Raw/before-native.log') -SimpleMatch 'FAILED: recent source edits retain bounded change metadata')){throw 'Baseline fallback failure missing.'}
$failed=0
foreach($scope in @('EditorControls','Embedded','Accessibility')){
 $native=Read-Receipt "Raw/FirstCandidate/$scope.receipt.txt"
 $report=Read-Receipt ('Performance/'+(Split-Path $native.performanceReport -Leaf))
 if($report.sourceFingerprint -cne '043AF8F51B0A9CAD86E392D952E82164ACAC69B3E7A8629179BF3A1D5940F8A3' -or $native.skips.Count -or $native.Contains('tests')){throw 'First candidate identity differs.'}
 if($native.exitCode -ne 0){++$failed;if($scope -cne 'Embedded' -or $native.exitCode -ne 1){throw 'Unexpected first candidate failure.'}}
}
if($failed -ne 1 -or -not(Select-String -LiteralPath (Join-Path $Packet 'Raw/FirstCandidate/Embedded.log') -SimpleMatch 'FAIL: numeric change metadata survives retired transient previews without creating history')){throw 'Localized preview failure missing.'}
function Get-Method([string]$File,[string]$Name){
 $text=[IO.File]::ReadAllText($File)
 $match=[regex]::Match($text,'(?:static )?void '+[regex]::Escape($Name)+'\(')
 if(-not $match.Success){throw "Method missing: $Name"}
 $first=$text.IndexOf('{',$match.Index);$depth=0
 for($index=$first;$index -lt $text.Length;++$index){
  if($text[$index] -eq '{'){++$depth}
  elseif($text[$index] -eq '}'){if(--$depth -eq 0){return $text.Substring($match.Index,$index+1-$match.Index)}}
 }
 throw 'Unbalanced method span.'
}
foreach($method in (Read-Receipt 'Raw/unchanged-measured-methods.receipt.txt')){
 foreach($prefix in @($method.beforePrefix,'Source/')){
  $body=Get-Method (Join-Path $Packet ($prefix+$method.file)) $method.method
  $hash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($body)))
  if($hash -cne $method.sha256){throw 'A measured oracle/budget method changed.'}
 }
}
foreach($configuration in @('Release','Debug','ASan Debug')){
 $lines=Get-Content -LiteralPath (Join-Path $Packet "Native/Embedded-x64-$configuration.log")
 foreach($observation in @('rich same-content edit CppAllocations=0 CppRequestedBytes=0','rich source change queries=512 previews=64 CppAllocations=0 CppRequestedBytes=0')){
  if(-not($lines -contains $observation)){throw 'Zero-allocation observation missing.'}
 }
 $storage=@($lines|Where-Object{$_ -match '^rich transient metadata units=8192 previews=4096 retainedCppBytes=(\d+) releasedDelta=0$'})
 if($storage.Count -ne 1 -or $storage[0] -notmatch 'retainedCppBytes=(\d+)' -or [long]$Matches[1] -gt 262144){throw 'Transient retained-byte budget differs.'}
 $storage=@($lines|Where-Object{$_ -match '^rich fragmented storage units=12288 edits=4096 retainedCppBytes=(\d+) releasedDelta=0$'})
 if($storage.Count -ne 1 -or $storage[0] -notmatch 'retainedCppBytes=(\d+)' -or [long]$Matches[1] -gt 4194304){throw 'History retained-byte budget differs.'}
 if(-not($lines -match '^rich source page-rollover sweptRequests=[1-9][0-9]*$')){throw 'Page rollover sweep missing.'}
 foreach($operation in 0..4){if(-not($lines -match "^rich source allocation-failure operation=$operation sweptRequests=[1-9][0-9]*$")){throw 'Allocation failure sweep missing.'}}
 $editor=Join-Path $Packet "Native/EditorControls-x64-$configuration.log"
 foreach($name in @('SingleTransactions','LocalizedTransientPreviews','JoinedCrlfBounds','UndoRedoAndBranches','TransientPagesAndExpiry','IndependentUnicodeOracle','ConcurrentEmptyAndMovedValues')){
  if(-not(Select-String -LiteralPath $editor -SimpleMatch "[DONE] TestRichSourceChange$name")){throw 'Source-change behavior oracle missing.'}
 }
}
$probe=Read-Receipt 'Native/AddressSanitizer-x64.receipt.txt'
if(-not $probe.detected -or $probe.exitCode -eq 0 -or -not(Select-String -LiteralPath (Join-Path $Packet 'Native/AddressSanitizer-x64.log') -SimpleMatch 'AddressSanitizer: heap-use-after-free')){throw 'Specific sanitizer diagnosis missing.'}
Write-Host 'Verified 57 final native successes, six builds, both original failures, unchanged measured methods, bounded transient storage and allocation failure sweeps.'
Write-Host 'The baseline supervisor attached after launch; it did not capture an exit code. Starter/native receipts record expected exit 1. Frame acceptance remains HOLD.'
