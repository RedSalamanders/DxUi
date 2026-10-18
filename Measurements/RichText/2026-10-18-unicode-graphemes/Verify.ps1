[CmdletBinding()] param([string] $Packet = $PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
function Read-Receipt([string] $Relative) { Get-Content -LiteralPath (Join-Path $Packet $Relative) -Raw | ConvertFrom-Json -AsHashtable -Depth 32 }
function Assert-Hash([string] $Relative, [string] $Expected) {
    $path = [IO.Path]::GetFullPath((Join-Path $Packet $Relative))
    if (-not $path.StartsWith(([IO.Path]::GetFullPath($Packet) + [IO.Path]::DirectorySeparatorChar), [StringComparison]::OrdinalIgnoreCase)) { throw 'Seal path escapes packet.' }
    if ((Get-FileHash -LiteralPath $path).Hash -cne $Expected) { throw "SHA-256 differs: $Relative" }
}
foreach ($line in Get-Content -LiteralPath (Join-Path $Packet 'SHA256SUMS')) {
    if ($line -cnotmatch '^([A-F0-9]{64})  (.+)$') { throw 'Malformed packet seal.' }
    Assert-Hash $Matches[2] $Matches[1]
}
$inventory = Read-Receipt 'inventory.receipt.txt'
if ($inventory.nativeSuiteCount -ne 57 -or $inventory.nativeSuites.Count -ne 57 -or $inventory.canonicalReportCount -ne 5) { throw 'Fixed evidence inventory differs.' }
foreach ($copy in $inventory.copies) { Assert-Hash $copy.path $copy.sha256 }
foreach ($entry in $inventory.nativeSuites) {
    $stem = "$($entry.suite)-x64-$($entry.configuration)"
    $native = Read-Receipt "Native/$stem.receipt.txt"
    $performance = Read-Receipt ('Performance/' + $entry.performance)
    $build = Read-Receipt ('Build/x64-' + $entry.configuration.Replace(' ','') + '-build.receipt.txt')
    if ($native.exitCode -ne 0 -or $native.skips.Count -or $native.Contains('tests') -or $native.sha256 -cne $entry.sha256 -or
        $native.suite -cne $entry.suite -or $native.configuration -cne $entry.configuration -or $native.nativeArchitecture -cne 'X64') { throw 'Complete native suite identity differs.' }
    if ($performance.sourceFingerprint -cne $inventory.sourceFingerprint -or $performance.configuration -cne $entry.configuration -or
        $performance.executableSha256 -cne $native.performanceExecutableSha256 -or $performance.buildAttestationIdentity -cne $build.identity -or $build.outcome -cne 'PASSED') { throw 'Native/performance/build binding differs.' }
    if ((ConvertTo-Json $native.performanceScenarios -Depth 16 -Compress) -cne (ConvertTo-Json $performance.scenarios -Depth 16 -Compress)) { throw 'Original native frame fields differ.' }
}
$matrix = Read-Receipt 'Raw/qualification-completion.receipt.txt'
if ($matrix.scheduled -ne 6 -or $matrix.attempted -ne 5 -or $matrix.elapsedSeconds -gt 3600) { throw 'Original matrix inventory differs.' }
$refused = @($matrix.results | Where-Object { $_.exitCode -ne 0 -or $_.timedOut -or -not $_.drained })
if ($refused.Count -ne 1 -or $refused[0].configuration -cne 'Release' -or $refused[0].platform -cne 'ARM64' -or $refused[0].exitCode -ne 1 -or $refused[0].timedOut -or -not $refused[0].drained) { throw 'Original refusal record differs.' }
if (-not (Select-String -LiteralPath (Join-Path $Packet 'Raw/ARM64-Release.err') -SimpleMatch 'buildInputIdentity changed during build')) { throw 'Original input-mutation refusal missing.' }
$recovery = Read-Receipt 'Raw/recovery/qualification-completion.receipt.txt'
if ($recovery.scheduled -ne 2 -or $recovery.attempted -ne 2 -or $recovery.elapsedSeconds -gt 900 -or
    @($recovery.results | Where-Object { $_.exitCode -ne 0 -or $_.timedOut -or -not $_.drained }).Count) { throw 'Frozen recovery builds incomplete.' }
$binding = Read-Receipt 'Raw/restored-build-input.receipt.txt'
if (-not $binding.matchesOriginalX64Input -or $binding.restoredBuildInputIdentity -cne $binding.originalBuildInputIdentity) { throw 'Restored inputs do not match the successful native runs.' }
foreach ($platform in @('x64','ARM64')) { foreach ($configuration in @('Release','Debug','ASanDebug')) {
    if ((Read-Receipt "Build/$platform-$configuration-build.receipt.txt").outcome -cne 'PASSED') { throw 'A required cross/profile build did not pass.' }
}}
$probe = Read-Receipt 'Native/AddressSanitizer-x64.receipt.txt'
if (-not $probe.detected -or $probe.exitCode -eq 0 -or -not (Select-String -LiteralPath (Join-Path $Packet 'Native/AddressSanitizer-x64.log') -SimpleMatch 'AddressSanitizer: heap-use-after-free')) { throw 'Specific sanitizer diagnosis missing.' }
$before = Read-Receipt 'Raw/before-source.receipt.txt'
Assert-Hash 'Generated/Scope.Tests.UnicodeGraphemeCorpus.h' $before['Tests/Controls/Scope.Tests.UnicodeGraphemeCorpus.h']
$red = Read-Receipt 'Raw/before-corpus-native.receipt.txt'
if ($red.exitCode -ne 1 -or $red.tests -cnotcontains 'TestUnicode17DefaultGraphemeCorpus') { throw 'Original corpus failure missing.' }
foreach ($pair in @(@('Raw/before-corpus-native.log','Unicode17 vector=1 offset=0 next=2/1'), @('Raw/before-utf8-native.log','decoderCppRequestedBytes=131127'))) {
    if (-not (Select-String -LiteralPath (Join-Path $Packet $pair[0]) -SimpleMatch $pair[1])) { throw 'Original failing predicate missing.' }
}
foreach ($configuration in @('Release','Debug','ASan Debug')) {
    $embedded = Join-Path $Packet "Native/Embedded-x64-$configuration.log"
    foreach ($needle in @('Unicode17 forward walk units=49152 elements=16384 CppAllocations=0 CppRequestedBytes=0',
        'rich UTF8 rejection inputBytes=65537 decoderCppRequestedBytes=0', 'rich UTF8 rejection inputBytes=196609 decoderCppRequestedBytes=0')) {
        if (-not (Select-String -LiteralPath $embedded -SimpleMatch $needle)) { throw "Native resource observation missing: $configuration / $needle" }
    }
    foreach ($pair in @(@('EditorControls','[DONE] TestUnicode17DefaultGraphemeCorpus'), @('TextField','[DONE] TestMaskedTextFieldUnicode17SourceMapping'),
        @('Accessibility','[DONE] TestRichTextAccessibilityWindowUnicode17Commands'), @('Accessibility','[DONE] TestRichTextAccessibilityEmbeddedUnicode17Commands'))) {
        if (-not (Select-String -LiteralPath (Join-Path $Packet "Native/$($pair[0])-x64-$configuration.log") -SimpleMatch $pair[1])) { throw 'Current native behavior case missing.' }
    }
}
& (Join-Path $Packet 'Generator/Generate-UnicodeGraphemeData.ps1') -InputDirectory (Join-Path $Packet 'Inputs') -OutputDirectory (Join-Path $Packet 'Generated') -Check
Write-Host 'Verified pinned corpus generation, red predicates, 57 native success/log bindings, six builds, specific ASan diagnosis and three resource profiles.'
Write-Host 'Performance acceptance, physical input/AT, native ARM64 execution and the remaining rich engine are still unqualified.'
