[CmdletBinding()] param([string] $PacketRoot = $PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$manifest = Join-Path $PacketRoot 'SHA256SUMS'
if (Test-Path -LiteralPath $manifest -PathType Leaf) {
    foreach ($line in Get-Content -LiteralPath $manifest) {
        if ($line -notmatch '^([A-Fa-f0-9]{64})  (.+)$') { throw "Malformed packet hash: $line" }
        if ((Get-FileHash -LiteralPath (Join-Path $PacketRoot $Matches[2]) -Algorithm SHA256).Hash -cne $Matches[1].ToUpperInvariant()) {
            throw "Packet hash mismatch: $($Matches[2])"
        }
    }
}
$plan = Get-Content -LiteralPath (Join-Path $PacketRoot 'plan.receipt.txt') -Raw | ConvertFrom-Json
$completion = Get-Content -LiteralPath (Join-Path $PacketRoot 'completion.receipt.txt') -Raw | ConvertFrom-Json
if ($plan.reportBudget -ne 11 -or $completion.scheduled -ne 11 -or $completion.attempted -ne 11 -or
    $completion.results.Count -ne 11 -or -not $completion.identityMatched -or $completion.identityError) { throw 'Incomplete or changed UIA inventory.' }
$runs = @(for ($index = 0; $index -lt 11; $index++) {
    $result = $completion.results[$index]
    $metadata = Get-Content -LiteralPath (Join-Path $PacketRoot $result.receipt) -Raw | ConvertFrom-Json
    $logPath = Join-Path $PacketRoot $result.log
    if ($result.index -ne $index + 1 -or $result.exitCode -ne 0 -or $result.timedOut -or $result.terminationError -or $result.drainError -or
        $metadata.exitCode -ne 0 -or $metadata.executableSha256 -cne $plan.executableSha256 -or
        (Get-FileHash -LiteralPath $logPath -Algorithm SHA256).Hash -cne $metadata.logSha256) { throw "Invalid UIA run: $($result.log)" }
    $lines = Get-Content -LiteralPath $logPath
    $steps = @(foreach ($line in $lines) {
        if ($line -match '\[UIA timeline\] step=(\d+) mark=(\d+) cutoff=(\d+) cutoffTick=(\d+) expected=(\d+) expectedArrived=(\d+) action=(\d+)\.\.(\d+)') {
            [ordered]@{ step=[int]$Matches[1]; mark=[int]$Matches[2]; cutoff=[int]$Matches[3]; cutoffTick=[long]$Matches[4];
                expected=[int]$Matches[5]; expectedArrived=([int]$Matches[6] -eq 1); actionStarted=[long]$Matches[7]; actionCompleted=[long]$Matches[8] }
        }
    })
    if ($steps.Count -ne 16 -or ($steps.step -join ',') -cne ((1..16) -join ',') -or
        $steps[14].expected -ne 0 -or $steps[14].mark -ne $steps[14].cutoff -or @($steps | Where-Object { -not $_.expectedArrived }).Count) {
        throw "Missing selection-step evidence: $($result.log)"
    }
    $unchangedProvider = @($lines | Where-Object { $_ -match '\[UIA provider attempts\] step=15 phase=after counts=(\d+)\.\.(\d+) stepFirst=(\d+) actionFirst=(\d+) capacity=1024 overflow=0' })
    if ($unchangedProvider.Count -ne 1 -or $unchangedProvider[0] -notmatch 'counts=(\d+)\.\.(\d+) stepFirst=(\d+) actionFirst=(\d+)' -or
        $Matches[1] -cne $Matches[2] -or $Matches[2] -cne $Matches[3] -or $Matches[3] -cne $Matches[4]) {
        throw "Unchanged republish made a provider attempt: $($result.log)"
    }
    if (@($lines | Where-Object { $_ -match '\[UIA stream\].*quiet=0|\[UIA arrivals\].*(overflow=[1-9]|failed=[1-9])|\[UIA provider attempts\].*overflow=1' }).Count) {
        throw "UIA diagnostic timeout/loss: $($result.log)"
    }
    $arrivals = @(foreach ($line in $lines) {
        if ($line -match '\[UIA arrival\] seq=(\d+) index=(\d+) entered=(\d+) completed=(\d+)') {
            [ordered]@{ sequence=[long]$Matches[1]; eventIndex=[int]$Matches[2]; entered=[long]$Matches[3]; completed=[long]$Matches[4]; callbackMs=([long]$Matches[4]-[long]$Matches[3]) }
        }
    })
    if (-not $arrivals.Count -or @($arrivals | Where-Object callbackMs -lt 0).Count) { throw "Invalid arrival timeline: $($result.log)" }
    [ordered]@{ file=$result.log; kind=$result.kind; processId=$metadata.processId; steps=$steps; arrivals=$arrivals.Count;
        maximumRecordedCallbackMs=($arrivals.callbackMs | Measure-Object -Maximum).Maximum;
        unchangedClientEvents=0; unchangedProviderAttempts=0; diagnosticLoss=0 }
})
$analysis = [ordered]@{ purpose='bounded-uia-selection-investigation'; runs=$runs; wallSeconds=$completion.wallSeconds;
    originalFailureCause='unproven'; limits='The original archived failure is preserved. Successful repetitions establish these observations only. A quiet interval does not rule out later repeats; provider attempts do not prove delivery or event identity.' }
[IO.File]::WriteAllText((Join-Path $PacketRoot 'analysis.txt'), ((ConvertTo-Json $analysis -Depth 12) + "`n"), [Text.UTF8Encoding]::new($false))
Write-Host 'Verified eleven fixed UIA runs and unchanged-selection assertions; original failure cause remains unproven.'
