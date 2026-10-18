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
if($inventory.nativeSuiteCount -ne 57 -or $inventory.nativeSuites.Count -ne 57 -or $inventory.canonicalReportCount -ne 7){throw 'Fixed packet inventory differs.'}
if($inventory.sourceFingerprint -in @('6B5ED058F3E2B3E5C2EB479A7EEDE48D615CBAAB3B98DEDE229F41B355DF1B71','E1556863AF507211818803A7BA5CD190538CBDE5777B9D424151BB7F5AB8BA40')){throw 'Final packet uses an obsolete source.'}
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
$recovery=Read-Receipt 'Raw/Recovery/qualification-completion.receipt.txt'
if($recovery.scheduled -ne 6 -or $recovery.attempted -ne 6 -or $recovery.elapsedSeconds -gt 3600 -or
 @($recovery.results|Where-Object{$_.exitCode -ne 0 -or $_.timedOut -or -not $_.drained}).Count){throw 'Final recovery matrix is incomplete or out of budget.'}
foreach($platform in @('x64','ARM64')){foreach($configuration in @('Release','Debug','ASanDebug')){
 if((Read-Receipt "Build/$platform-$configuration-build.receipt.txt").outcome -cne 'PASSED'){throw 'Required build missing.'}
}}
$initial=Read-Receipt 'Raw/qualification-completion.receipt.txt'
if($initial.scheduled -ne 6 -or $initial.attempted -ne 2 -or $initial.results[0].exitCode -ne 0 -or $initial.results[1].exitCode -ne 1 -or
 @($initial.results|Where-Object{$_.timedOut -or -not $_.drained}).Count){throw 'Original failed matrix outcomes differ.'}
$intermediateFiles=@(Get-ChildItem -LiteralPath (Join-Path $Packet 'Raw/Intermediate/Native') -Filter '*.receipt.txt')
if($intermediateFiles.Count -ne 38){throw 'Intermediate native outcome inventory differs.'}
$failed=0
foreach($file in $intermediateFiles){
 $native=Get-Content -LiteralPath $file.FullName -Raw|ConvertFrom-Json -AsHashtable -Depth 64
 $performance=Read-Receipt ('Performance/'+(Split-Path $native.performanceReport -Leaf))
 if($performance.sourceFingerprint -cne 'E1556863AF507211818803A7BA5CD190538CBDE5777B9D424151BB7F5AB8BA40' -or $native.skips.Count -or $native.Contains('tests')){throw 'Intermediate source/scope differs.'}
 if($native.exitCode -ne 0){
  ++$failed
  if($native.suite -cne 'Embedded' -or $native.configuration -cne 'Debug' -or $native.exitCode -ne 1){throw 'Unexpected intermediate failure.'}
 }
}
if($failed -ne 1 -or -not(Select-String -LiteralPath (Join-Path $Packet 'Raw/Intermediate/Native/Embedded-x64-Debug.log') -SimpleMatch 'rich same-content edit CppAllocations=2 CppRequestedBytes=32')){throw 'Original Debug proxy failure is missing.'}
$before=Read-Receipt 'Raw/before-completion.receipt.txt'
if($before.scheduled -ne 3 -or $before.attempted -ne 3 -or $before.elapsedSeconds -gt 1200 -or
 $before.results[0].exitCode -ne 0 -or $before.results[1].exitCode -ne 1 -or $before.results[2].exitCode -ne 1 -or
 @($before.results|Where-Object{$_.timedOut -or -not $_.drained}).Count){throw 'Original before outcomes differ.'}
$beforeNative=Read-Receipt 'Raw/Before/edit-read-single-step/native.receipt.txt'
$beforeReport=Read-Receipt ('Performance/'+(Split-Path $beforeNative.performanceReport -Leaf))
if($beforeReport.sourceFingerprint -cne '6B5ED058F3E2B3E5C2EB479A7EEDE48D615CBAAB3B98DEDE229F41B355DF1B71' -or $beforeNative.exitCode -ne 0 -or $beforeNative.skips.Count){throw 'Original root baseline differs.'}
foreach($slot in @('retained','same-content')){
 $process=Read-Receipt "Raw/Before/$slot/process.receipt.txt"
 if($process.exitCode -ne 1 -or $process.sha256 -cne $beforeReport.executableSha256 -or $process.buildAttestationIdentity -cne $beforeReport.buildAttestationIdentity){throw 'Isolated before executable/build binding differs.'}
}
if(-not(Select-String -LiteralPath (Join-Path $Packet 'Raw/Before/retained/runner.log') -SimpleMatch 'retainedCppBytes=40235301 releasedDelta=0') -or
 -not(Select-String -LiteralPath (Join-Path $Packet 'Raw/Before/same-content/runner.log') -SimpleMatch 'CppAllocations=24 CppRequestedBytes=518630')){throw 'Original storage failures missing.'}
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
 foreach($prefix in @('Raw/BeforeSource/','Source/')){
  $body=Get-Method (Join-Path $Packet ($prefix+$method.file)) $method.method
  $hash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($body)))
  if($hash -cne $method.sha256){throw 'A measured oracle/budget method changed.'}
 }
}
foreach($configuration in @('Release','Debug','ASan Debug')){
 $embedded=Join-Path $Packet "Native/Embedded-x64-$configuration.log"
 $lines=Get-Content -LiteralPath $embedded
 $storage=@($lines|Where-Object{$_ -match '^rich fragmented storage units=12288 edits=4096 retainedCppBytes=(\d+) releasedDelta=0$'})
 if($storage.Count -ne 1 -or $storage[0] -notmatch 'retainedCppBytes=(\d+)' -or [long]$Matches[1] -gt 4194304){throw 'Final retained-byte bound differs.'}
 if(-not($lines -like '*rich same-content edit CppAllocations=0 CppRequestedBytes=0*')){throw 'Final no-op allocation observation missing.'}
 foreach($operation in 0..4){
  if(-not($lines -match "^rich source allocation-failure operation=$operation sweptRequests=[1-9][0-9]*$")){throw 'Allocation-failure operation sweep missing.'}
 }
 $editor=Join-Path $Packet "Native/EditorControls-x64-$configuration.log"
 foreach($name in @('TestRichDocumentPersistentSplitMergeOracle','TestRichDocumentMaximumFragmentationAndRepeatedRootCollapse','TestRichDocumentEmptyMovedSnapshotsAndCrossNodeJoins','TestRichDocumentBorrowedInputLifetime','TestRichDocumentConcurrentSnapshotReads','TestUnicode17BulkCharacterMovementOracle')){
  if(-not(Select-String -LiteralPath $editor -SimpleMatch "[DONE] $name")){throw 'Final behavior oracle missing.'}
 }
}
$probe=Read-Receipt 'Native/AddressSanitizer-x64.receipt.txt'
if(-not $probe.detected -or $probe.exitCode -eq 0 -or -not(Select-String -LiteralPath (Join-Path $Packet 'Native/AddressSanitizer-x64.log') -SimpleMatch 'AddressSanitizer: heap-use-after-free')){throw 'Specific sanitizer diagnosis missing.'}
Write-Host 'Verified 57 final native successes, six builds, original resource and Debug failures, unchanged measured methods, storage bounds and failure sweeps.'
Write-Host 'Edit/read/single-step timings remain descriptive; frame performance acceptance and physical/native-platform gates remain open.'
