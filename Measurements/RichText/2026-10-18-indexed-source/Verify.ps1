[CmdletBinding()] param([string] $SourceRoot,[string] $Packet=$PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $SourceRoot 'Tools/PerformanceProfiling.psm1') -Force
function Read-Receipt([string] $Path){return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -AsHashtable -Depth 32}
function Assert-Hash([string] $Path,[string] $Hash){if((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -cne $Hash){throw "Byte binding differs: $Path"}}
$seal=Join-Path $Packet 'SHA256SUMS'
if(Test-Path $seal){foreach($line in Get-Content -LiteralPath $seal){
    if($line -cnotmatch '^([A-F0-9]{64})  (.+)$'){throw 'Malformed seal.'}
    $hash=$Matches[1];$target=[IO.Path]::GetFullPath((Join-Path $Packet $Matches[2]))
    if(-not $target.StartsWith(([IO.Path]::GetFullPath($Packet)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)){throw 'Seal escapes packet.'}
    Assert-Hash $target $hash
}}
$inventory=Read-Receipt (Join-Path $Packet 'inventory.receipt.txt')
if($inventory.nativeSuiteCount -ne 57 -or $inventory.nativeSuites.Count -ne 57 -or $inventory.linkedCanonicalReports -ne 4){throw 'Native inventory differs.'}
$source=$inventory.sourceFingerprint
foreach($entry in $inventory.nativeSuites){
    $native=Read-Receipt (Join-Path $Packet "Native/$($entry.suite)-x64-$($entry.configuration).receipt.txt")
    if($native.exitCode -ne 0 -or $native.skips.Count -or $native.Contains('tests') -or $native.sha256 -cne $entry.sha256){throw 'Native success identity differs.'}
    $performance=Read-Receipt (Join-Path $Packet ('Performance/'+$entry.performance))
    if($performance.sourceFingerprint -cne $source -or $performance.configuration -cne $entry.configuration -or
        $performance.executableSha256 -cne $native.performanceExecutableSha256){throw 'Native/performance binding differs.'}
    if((ConvertTo-Json $native.performanceScenarios -Depth 16 -Compress) -cne (ConvertTo-Json $performance.scenarios -Depth 16 -Compress)){throw 'Native frame values differ.'}
    $build=Read-Receipt (Join-Path $Packet ('Build/x64-'+$entry.configuration.Replace(' ','')+'.receipt.txt'))
    if($build.outcome -cne 'PASSED' -or $build.identity -cne $performance.buildAttestationIdentity){throw 'Original successful build identity differs.'}
}
$done=Read-Receipt (Join-Path $Packet 'Raw/qualification-completion.receipt.txt')
if($done.scheduled -ne 6 -or $done.attempted -ne 6 -or $done.elapsedSeconds -gt 3600 -or
    @($done.results | Where-Object {$_.exitCode -ne 0 -or $_.timedOut -or -not $_.drained}).Count){throw 'Incomplete fixed build/native matrix.'}
$probe=Read-Receipt (Join-Path $Packet 'Native/AddressSanitizer-x64.receipt.txt')
if(-not $probe.detected -or $probe.exitCode -eq 0 -or -not (Select-String -LiteralPath (Join-Path $Packet 'Native/AddressSanitizer-x64.log') -SimpleMatch 'AddressSanitizer: heap-use-after-free')){throw 'Specific sanitizer diagnosis is missing.'}
$captures=[Collections.Generic.HashSet[string]]::new()
foreach($configuration in @('Release','Debug','ASan Debug')){foreach($timers in @('Off','On')){
    $stem=$configuration.Replace(' ','-')+'-'+$timers+'.json';$path=Join-Path $Packet ('Diagnostics/'+$stem)
    $report=Read-Receipt $path;$raw=Read-Receipt ($path+'.raw.receipt.txt');$child=Read-Receipt ($path+'.child.receipt.txt')
    Assert-Hash ($path+'.raw.receipt.txt') $report.rawReportSha256;Assert-Hash ($path+'.child.receipt.txt') $report.childMetadataSha256
    Assert-Hash ($path+'.log') $report.logSha256;Assert-Hash ($path+'.png') $report.captureSha256
    foreach($key in $raw.Keys){if((ConvertTo-Json $raw[$key] -Depth 32 -Compress) -cne (ConvertTo-Json $report[$key] -Depth 32 -Compress)){throw 'Diagnostic native fields changed during stamping.'}}
    foreach($key in $report.profilingInputs.Keys){Assert-Hash (Join-Path $SourceRoot $key) $report.profilingInputs[$key]}
    if($report.sourceFingerprint -cne $source -or $child.exitCode -ne 0 -or $child.timedOut -or $child.terminationError -or $child.drainError){throw 'Current-profile diagnostic binding differs.'}
    $null=Assert-BoundaryReport -Receipt $raw -Timers ($timers -ceq 'On') -Configuration $configuration -ProcessId $child.processId
    [void]$captures.Add($report.captureSha256)
}}
$diagnostics=Read-Receipt (Join-Path $Packet 'Diagnostics/completion.receipt.txt')
if($diagnostics.scheduled -ne 6 -or $diagnostics.attempted -ne 6 -or $diagnostics.elapsedSeconds -gt 900 -or
    @($diagnostics.results | Where-Object {$_.exitCode -ne 0 -or $_.timedOut -or -not $_.drained}).Count){throw 'Incomplete native branch/profile diagnostics.'}
Write-Host "Verified 57 native suites, four canonical reports, six builds and six current diagnostic profiles; $($captures.Count) populated capture hash."
Write-Host 'This is functional/byte evidence; performance acceptance and physical/native-platform gates remain open.'

