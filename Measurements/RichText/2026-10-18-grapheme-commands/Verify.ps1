[CmdletBinding()] param([string] $Packet=$PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
function Read-Receipt([string] $Relative){Get-Content -LiteralPath (Join-Path $Packet $Relative) -Raw|ConvertFrom-Json -AsHashtable -Depth 32}
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
if($inventory.nativeSuiteCount -ne 57 -or $inventory.nativeSuites.Count -ne 57 -or $inventory.canonicalReportCount -ne 7){throw 'Fixed inventory differs.'}
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
$matrix=Read-Receipt 'Raw/qualification-completion.receipt.txt'
if($matrix.scheduled -ne 6 -or $matrix.attempted -ne 6 -or $matrix.elapsedSeconds -gt 3600 -or
    @($matrix.results|Where-Object{$_.exitCode -ne 0 -or $_.timedOut -or -not $_.drained}).Count){throw 'Incomplete or out-of-budget matrix.'}
foreach($platform in @('x64','ARM64')){foreach($configuration in @('Release','Debug','ASanDebug')){
    if((Read-Receipt "Build/$platform-$configuration-build.receipt.txt").outcome -cne 'PASSED'){throw 'Required build missing.'}
}}
$before=Read-Receipt 'Raw/before-completion.receipt.txt'
if($before.scheduled -ne 3 -or $before.attempted -ne 3 -or $before.elapsedSeconds -gt 1200 -or
    @($before.results|Where-Object{$_.exitCode -ne 1 -or $_.timedOut -or -not $_.drained}).Count){throw 'Original before-run schedule differs.'}
foreach($slot in @('oracle-trials-legacy','rich-native','rich-embedded')){
    $receipt=Read-Receipt "Raw/Before/$slot/native.receipt.txt"
    if($receipt.exitCode -ne 1 -or $receipt.skips.Count -or $receipt.sha256 -cne 'E226DE8C45CB301D31726D951E4902F983D527FC80F77FBA8561FA842B1B717A' -or -not $receipt.Contains('tests')){throw 'Original filtered failure differs.'}
    $report=Read-Receipt ('Performance/'+(Split-Path $receipt.performanceReport -Leaf))
    if($report.sourceFingerprint -cne '55DE3A71D32382F2DE7DC23D3A7E4EED6A3CDE40F93684C45FF83880F47E9C85' -or $report.executableSha256 -cne $receipt.performanceExecutableSha256){throw 'Before failure/benchmark binding differs.'}
}
if(-not(Select-String -LiteralPath (Join-Path $Packet 'Raw/Before/oracle-trials-legacy/native.log') -SimpleMatch 'resultUnits=3 validUtf16=0')){throw 'Original invalid UTF16 predicate missing.'}
foreach($slot in @('rich-native','rich-embedded')){
    if(-not(Select-String -LiteralPath (Join-Path $Packet "Raw/Before/$slot/native.log") -SimpleMatch 'FAILED: collapsed rich deletion removes the complete containing grapheme')){throw 'Original rich failure predicate missing.'}
}
$methods=Read-Receipt 'Raw/unchanged-oracle-methods.receipt.txt'
$old=[IO.File]::ReadAllText((Join-Path $Packet 'Raw/BeforeTests/Scope.Tests.TextElementMovement.cpp'))
$new=[IO.File]::ReadAllText((Join-Path $Packet 'Source/Tests/Controls/Scope.Tests.TextElementMovement.cpp'))
foreach($pair in @(@('TestUnicode17BulkCharacterMovementOracle','TestTextElementBulkMovementFixedTrials'),@('TestTextElementBulkMovementFixedTrials','TextFieldInteriorDeletion'))){
    foreach($text in @($old,$new)){
        $first=$text.IndexOf('void '+$pair[0]+'(');$last=$text.IndexOf('void '+$pair[1]+'(')
        if($first -lt 0 -or $last -le $first){throw 'Oracle method span missing.'}
        $hash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($text.Substring($first,$last-$first))))
        if($hash -cne $methods[$pair[0]]){throw 'The measured oracle method changed.'}
    }
}
foreach($configuration in @('Release','Debug','ASan Debug')){
    $embedded=Join-Path $Packet "Native/Embedded-x64-$configuration.log"
    if(-not(Select-String -LiteralPath $embedded -SimpleMatch 'Unicode17 bulk commands units=49152 elements=16384 CppAllocations=0 CppRequestedBytes=0')){throw 'Resource observation missing.'}
    foreach($name in @('TestUnicode17BulkCharacterMovementOracle','TestUnicode17CollapsedDeletionCorpus','TestTextElementBulkMovementMaximumSourceAndMalformedBounds','TestTextFieldInteriorAcpDeletion','TestMultilineFieldInteriorAcpDeletion','TestComboBoxInteriorAcpDeletion')){
        if(-not(Select-String -LiteralPath (Join-Path $Packet "Native/EditorControls-x64-$configuration.log") -SimpleMatch "[DONE] $name")){throw 'Current corpus/legacy behavior case missing.'}
    }
    foreach($name in @('WindowInteriorAcpDeletion','EmbeddedInteriorAcpDeletion','WindowBulkCharacterCommands','EmbeddedBulkCharacterCommands')){
        if(-not(Select-String -LiteralPath (Join-Path $Packet "Native/Accessibility-x64-$configuration.log") -SimpleMatch "[DONE] TestRichTextAccessibility$name")){throw 'Current native/embedded provider case missing.'}
    }
}
$analysis=Read-Receipt 'Raw/trial-analysis.receipt.txt'
foreach($pair in @(@('before','Raw/Before/oracle-trials-legacy/native.log'),@('after','Native/EditorControls-x64-Release.log'))){
    $trials=@()
    foreach($line in Get-Content -LiteralPath (Join-Path $Packet $pair[1])){
        if($line -match '^UIA bulk trial=(\d+) sourceUnits=(\d+) elements=(\d+) forwardMs=([0-9.]+) backwardMs=([0-9.]+)$'){
            $trials+=@{trial=[int]$Matches[1];units=[int]$Matches[2];elements=[int]$Matches[3];
                forwardMs=[double]::Parse($Matches[4],[Globalization.CultureInfo]::InvariantCulture);
                backwardMs=[double]::Parse($Matches[5],[Globalization.CultureInfo]::InvariantCulture)}
        }
    }
    if($trials.Count -ne 5 -or $analysis[$pair[0]].Count -ne 5){throw 'Fixed trial count differs.'}
    for($index=0;$index -lt 5;++$index){foreach($key in @('trial','units','elements','forwardMs','backwardMs')){
        if($trials[$index][$key] -ne $analysis[$pair[0]][$index][$key]){throw 'Derived trial value differs from the original log.'}
    }}
    if(@($trials.forwardMs|Sort-Object)[2] -ne $analysis[$pair[0]+'ForwardMedianMs'] -or
       @($trials.backwardMs|Sort-Object)[2] -ne $analysis[$pair[0]+'BackwardMedianMs']){throw 'Derived trial median differs.'}
}
$probe=Read-Receipt 'Native/AddressSanitizer-x64.receipt.txt'
if(-not $probe.detected -or $probe.exitCode -eq 0 -or -not(Select-String -LiteralPath (Join-Path $Packet 'Native/AddressSanitizer-x64.log') -SimpleMatch 'AddressSanitizer: heap-use-after-free')){throw 'Specific sanitizer diagnosis missing.'}
Write-Host 'Verified 57 native success bindings, six builds, three original failures, unchanged oracle bodies, maximal native/embedded commands and zero-allocation observations.'
Write-Host 'Timing trials remain descriptive; Default performance acceptance and physical/native-platform gates remain open.'
