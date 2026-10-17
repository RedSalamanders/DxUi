[CmdletBinding()] param([string] $SourceRoot, [string] $Reports)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $SourceRoot 'Tools/PerformanceProfiling.psm1') -Force
Import-Module (Join-Path $SourceRoot 'Tools/PerformancePolicy.psm1')
function Read-Object([string] $Path) {
    if (-not (Test-Path -LiteralPath $Path) -and $Path.EndsWith('.json',[StringComparison]::Ordinal)) { $Path=$Path.Substring(0,$Path.Length-5)+'.receipt.txt' }
    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -AsHashtable -Depth 32
}
function Assert-Hash([string] $Path,[string] $Expected) {
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -cne $Expected) { throw "Byte binding differs: $Path" }
}
function Median([double[]] $Values) {
    $ordered=@($Values | Sort-Object); $middle=[int][Math]::Floor($ordered.Count/2)
    if ($ordered.Count % 2) { return $ordered[$middle] }; return ($ordered[$middle-1]+$ordered[$middle])/2
}
$seal=Join-Path $PSScriptRoot 'SHA256SUMS'
if (Test-Path $seal) {
    foreach ($line in Get-Content -LiteralPath $seal) {
        if ($line -cnotmatch '^([A-F0-9]{64})  (.+)$') { throw 'Malformed packet seal.' }
        $relative=$Matches[2]; $expected=$Matches[1]
        $target=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot $relative))
        if (-not $target.StartsWith(([IO.Path]::GetFullPath($PSScriptRoot)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)) { throw 'Packet seal escapes its root.' }
        Assert-Hash $target $expected
    }
}
$plan=Read-Object (Join-Path $Reports 'plan.json')
$done=Read-Object (Join-Path $Reports 'completion.json')
if ($done.scheduled -ne 8 -or $done.attempted -ne 8 -or $done.results.Count -ne 8 -or $done.elapsedSeconds -gt 900) { throw 'Incomplete fixed pilot.' }
foreach ($key in $plan.anchors.A.profilingInputs.Keys) { Assert-Hash (Join-Path $SourceRoot $key) $plan.anchors.A.profilingInputs[$key] }
$rows=[Collections.Generic.List[object]]::new(); $captures=[Collections.Generic.HashSet[string]]::new()
for ($slot=0; $slot -lt 8; $slot++) {
    $result=$done.results[$slot]; $assignment=$plan.schedule[$slot]
    if ($result.index -ne $slot+1 -or $result.role -cne $assignment.role -or $result.timers -cne $assignment.timers -or
        $result.timedOut -or $result.exitCode -ne 0 -or -not $result.identityMatched -or $result.identityError) { throw 'Pilot schedule/result mismatch.' }
    $path=Join-Path $Reports $result.report; $report=Read-Object $path
    # Archived packets rename only raw/child .json suffixes to .receipt.txt; bytes remain exact.
    $rawPath=$path+'.raw.json'; $childPath=$path+'.child.json'
    if (-not (Test-Path $rawPath)) { $rawPath=$path+'.raw.receipt.txt'; $childPath=$path+'.child.receipt.txt' }
    $native=Read-Object $rawPath; $child=Read-Object $childPath
    Assert-Hash $rawPath $report.rawReportSha256; Assert-Hash $childPath $report.childMetadataSha256
    Assert-Hash ($path+'.log') $report.logSha256; Assert-Hash ($path+'.png') $report.captureSha256
    [void]$captures.Add($report.captureSha256)
    foreach ($key in $native.Keys) {
        if ((ConvertTo-Json $native[$key] -Depth 32 -Compress) -cne (ConvertTo-Json $report[$key] -Depth 32 -Compress)) { throw "Native field differs after stamp: $key" }
    }
    $state=[ordered]@{}; foreach ($key in $plan.anchors[$result.role].Keys) { $state[$key]=$report[$key] }
    Assert-PerformanceMeasurementState -Before $plan.anchors[$result.role] -After $state
    if ($child.exitCode -ne 0 -or $child.timedOut -or $child.terminationError -or $child.drainError -or
        $child.arguments[0] -cne '--benchmark-complex-ui-boundaries' -or $child.arguments[2] -cne $result.timers) { throw 'Owned-child result differs.' }
    $null=Assert-BoundaryReport -Receipt $native -Timers ($result.timers -ceq 'On') -Configuration Release -ProcessId $child.processId
    $scale=1000.0/$native.clock.ticksPerSecond
    for ($window=0; $window -lt 8; $window++) {
        $probe=$native.windows[$window]
        $frames=@($probe.frames | Sort-Object { $_.qpc[8]-$_.qpc[0] })
        $tail=$frames[37].qpc # Same existing 38th-ranked frame, not independently sorted subintervals.
        $row=[ordered]@{process=$slot+1;role=$result.role;timers=$result.timers;window=$window;
            control=$(if ($probe.clear) { if ($probe.composite) {'full'} else {'clear'} } elseif ($probe.composite) {'composite'} else {'readback'});
            frameP50Ms=($frames[19].qpc[8]-$frames[19].qpc[0])*$scale;frameP95Ms=($tail[8]-$tail[0])*$scale;
            mainCycles=$probe.cpuAfter.main.cycles-$probe.cpuBefore.main.cycles;processCycles=$probe.cpuAfter.process.cycles-$probe.cpuBefore.process.cycles}
        if ($result.timers -ceq 'On') {
            $names=@('prepare','bind','clear','composite','copy','map','unmap','remainder')
            for ($edge=0; $edge -lt 8; $edge++) { $row[$names[$edge]+'Ms']=($tail[$edge+1]-$tail[$edge])*$scale }
            $row['mapSharePercent']=100.0*($tail[6]-$tail[5])/($tail[8]-$tail[0])
        }
        $rows.Add($row)
    }
}
$groups=@(foreach ($role in @('A','B')) { foreach ($timers in @('Off','On')) { foreach ($control in @('full','clear','composite','readback')) {
    $values=@($rows | Where-Object {$_.role -ceq $role -and $_.timers -ceq $timers -and $_.control -ceq $control})
    $item=[ordered]@{role=$role;timers=$timers;control=$control;windows=$values.Count;medianWindowP95Ms=(Median @($values.frameP95Ms))}
    if ($timers -ceq 'On') { $item['medianSameFrameMapSharePercent']=Median @($values.mapSharePercent) }
    $item
} } })
@{classification='diagnostic-only';processes=8;frames=2560;windows=$rows.ToArray();groups=$groups;
    uniqueCaptureHashes=$captures.Count;elapsedSeconds=$done.elapsedSeconds;acceptance='not-established';
    scope='Window p95 uses sorted index 37 of 40; subintervals use that same frame. Fresh timers-off/on two processes per side are a bounded observer pilot, not an isolation or convergence proof. CPU call intervals include backend work/waits/preemption; no ETW scheduler or GPU-duration attribution.'} |
    ConvertTo-Json -Depth 16
