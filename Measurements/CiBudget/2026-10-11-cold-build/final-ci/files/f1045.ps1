<# Independent read-only audit of the raw native benchmark reports linked by a completed CI review. #>
[CmdletBinding()] param([Parameter(Mandatory)][string] $ReviewReceipt)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
function Require([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
$externalRoot = [IO.Path]::GetFullPath('C:/Users/eric/.codex/tmp/dpf-s0-s2-merge')
$ReviewReceipt = [IO.Path]::GetFullPath($ReviewReceipt)
Require ($ReviewReceipt.StartsWith($externalRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) 'Review must belong to the task-owned external packet.'
$review = Get-Content -Raw -LiteralPath $ReviewReceipt | ConvertFrom-Json -AsHashtable
Require ($review['native'].Count -eq 128) 'A complete native inventory is required for this independent audit.'
$collectionRoot = [IO.Path]::GetDirectoryName($ReviewReceipt)
function Assert-Seal {
    foreach ($line in [IO.File]::ReadAllLines((Join-Path $collectionRoot 'SHA256SUMS'))) {
        Require ($line -cmatch '^([a-f0-9]{64})  (.+)$') 'Malformed collection seal.'
        $expected = $Matches[1]; $relative = $Matches[2]
        $path = [IO.Path]::GetFullPath((Join-Path $collectionRoot $relative))
        Require ($path.StartsWith($collectionRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) 'Sealed path escapes collection.'
        Require ([string]::Equals((Get-FileHash -LiteralPath $path).Hash, $expected, [StringComparison]::OrdinalIgnoreCase)) "Sealed bytes changed: $relative"
    }
}
Assert-Seal
$records = [Collections.Generic.List[object]]::new()
$paths = @($review['native'] | ForEach-Object { $_['performance'] } | Sort-Object -Unique)
foreach ($path in $paths) {
    $path = [IO.Path]::GetFullPath($path)
    Require ($path.StartsWith($collectionRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) 'Native benchmark escapes its collection.'
    $value = Get-Content -Raw -LiteralPath $path | ConvertFrom-Json -AsHashtable
    $configuration = [string]$value['configuration']; $platform = [string]$value['platform']
    Require ($configuration -cin @('Debug','Release','ASan Debug') -and $platform -cin @('x64','ARM64')) 'Unexpected native profile.'
    $ceiling = if ($configuration -ceq 'Release') { 64 } else { 320 }
    $expected = [ordered]@{ fixture='dxui-complex-ui-v2'; renderer='WARP'; width=1280; height=720; dpi=96; controls=83; modelRows=1000; framesPerRound=40; roundCount=5; dirtyAllocationCeilingPerFrame=$ceiling; sourceCommit=$review['testedMerge']; sourceDirty=$false }
    foreach ($key in $expected.Keys) {
        Require ($value.Contains($key) -and (ConvertTo-Json -InputObject $value[$key] -Compress) -ceq (ConvertTo-Json -InputObject $expected[$key] -Compress)) "Raw native fixture differs: $platform/$configuration/$key"
    }
    Require ([string]::Equals([string]$value['nativeArchitecture'],$platform,[StringComparison]::OrdinalIgnoreCase)) 'Native benchmark architecture differs from its profile.'
    Require ($value['hiddenPreparations'] -eq 0 -and $value['hiddenComposites'] -eq 0) 'Native hidden-work budget failed.'
    $phases = @($value['scenarios'])
    Require ($phases.Count -eq 2 -and (@($phases | ForEach-Object { $_['name'] } | Sort-Object) -join ',') -ceq 'clean,dirty') 'Native clean/dirty inventory is incomplete or repeated.'
    $peakDirty = 0
    foreach ($phase in $phases) {
        Require (@($phase['rounds']).Count -eq 5) 'A native phase lacks its five raw rounds.'
        foreach ($round in $phase['rounds']) {
            $count = $round['cppAllocations']
            Require (($count -is [int] -or $count -is [long]) -and $count -ge 0) 'Native allocation count must be an actual nonnegative integer.'
            Require ($round['composeAllocations'] -eq 0) 'Native composition allocation budget failed.'
            if ($phase['name'] -ceq 'clean') { Require ($count -eq 0) 'Native clean whole-frame allocation budget failed.' }
            else { Require ($count -le $ceiling * 40) 'Native dirty whole-frame allocation budget failed.'; $peakDirty = [Math]::Max($peakDirty,[double]$count) }
        }
    }
    $records.Add([ordered]@{ platform=$platform; configuration=$configuration; file=$path; sha256=(Get-FileHash -LiteralPath $path).Hash; rounds=10; dirtyCeilingPerFrame=$ceiling; maximumDirtyPerRound=$peakDirty; cleanCppAllocations=0; composeAllocations=0; hiddenPreparations=0; hiddenComposites=0 })
}
$profiles = @($records | ForEach-Object { "$($_['platform'])/$($_['configuration'])" } | Sort-Object -Unique)
Require ($profiles.Count -eq 6) 'Native raw budgets do not cover all six profiles.'
Assert-Seal
$result = [ordered]@{ version=1; status='RAW_NATIVE_ABSOLUTE_BUDGETS_VERIFIED'; runId=$review['runId']; head=$review['head']; testedMerge=$review['testedMerge']; reviewReceiptSha256=(Get-FileHash -LiteralPath $ReviewReceipt).Hash; reports=$records.Count; rounds=10*$records.Count; profiles=$profiles; rawReports=$records.ToArray(); reviewedUtc=[DateTime]::UtcNow.ToString('o'); driverSha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash }
$output = Join-Path $collectionRoot ("native-raw-budget-review-$([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))-$([guid]::NewGuid().ToString('N')).receipt.txt")
[IO.File]::WriteAllText($output,(($result | ConvertTo-Json -Depth 20) + "`n"),[Text.UTF8Encoding]::new($false))
Write-Host "RAW_NATIVE_ABSOLUTE_BUDGETS_VERIFIED: $($records.Count) reports; $($result.rounds) rounds; $output"
