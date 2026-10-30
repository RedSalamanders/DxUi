[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
$scriptPath = Join-Path $PSScriptRoot '../Watch-BenchmarkHost.ps1'

Invoke-TestCase 'hosted study admits continuous observations and retains original native outputs' {
    $workflow = Get-Content (Join-Path $PSScriptRoot '../../.github/workflows/benchmark.yml') -Raw
    $measure = $workflow.Substring($workflow.IndexOf('      - id: measure'),$workflow.IndexOf('      - id: verdict')-$workflow.IndexOf('      - id: measure'))
    Assert-True ($measure.IndexOf("Test-Path (`$hostPath + '.ready')") -lt $measure.IndexOf('./performance-paired.ps1 @pairedArgs')) 'ready evidence precedes measurement'
    Assert-True ($measure.Contains('finally {') -and $measure.Contains("(`$hostPath + '.stop')") -and $measure.Contains("`$observer.State -ne 'Completed'")) 'all outcomes stop and check the owned observer'
    foreach ($side in @('baseline','candidate')) {
        foreach ($extension in @('exe','lib','pdb','dll')) {
            Assert-True ($workflow.Contains(".build/paired/*/$side/.build/x64/Release/*.$extension")) "original $side $extension retained"
        }
        Assert-True ($workflow.Contains(".build/paired/*/$side/.build/reports/**")) "$side attestations retained"
    }
    Assert-True ($workflow.Contains('.build/host-observations/**')) 'continuous evidence retained'
}

function Start-Observer([string] $Path, [int] $Limit=100) {
    $start = [Diagnostics.ProcessStartInfo]::new((Get-Command pwsh).Source)
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($arg in @('-NoProfile','-NonInteractive','-File',$scriptPath,'-OutputPath',$Path,
        '-IntervalMilliseconds','100','-MaximumSamples',"$Limit")) { $start.ArgumentList.Add($arg) }
    return [Diagnostics.Process]::Start($start)
}

Invoke-FixtureCase 'host observations refuse to overwrite original evidence' {
    param($root)
    $path = Join-Path $root 'observations.jsonl'
    [IO.File]::WriteAllText($path,'original evidence')
    $process = Start-Observer $path
    try {
        Assert-True ($process.WaitForExit(30000)) 'refusal is bounded'
        Assert-True ($process.ExitCode -ne 0) 'existing evidence fails closed'
        Assert-Equal 'original evidence' ([IO.File]::ReadAllText($path)) 'evidence survives refusal'
    } finally { if (-not $process.HasExited) { $process.Kill($true); $process.WaitForExit() }; $process.Dispose() }
}

if ($IsWindows) {
    Invoke-FixtureCase 'continuous Windows observations retain counters, collection costs and cooperative completion' {
        param($root)
        $path = Join-Path $root 'observations.jsonl'
        $process = Start-Observer $path
        try {
            $deadline = [DateTime]::UtcNow.AddSeconds(30)
            do { Start-Sleep -Milliseconds 100 } while (-not (Test-Path ($path+'.ready')) -and -not $process.HasExited -and [DateTime]::UtcNow -lt $deadline)
            Assert-True (Test-Path ($path+'.ready')) 'observer starts before benchmark admission'
            Start-Sleep -Milliseconds 350
            [IO.File]::WriteAllText($path+'.stop','test completed')
            Assert-True ($process.WaitForExit(10000)) 'owned observer stops cooperatively'
            Assert-Equal 0 $process.ExitCode 'successful completion'
            $samples = @(Get-Content -LiteralPath $path | ForEach-Object { $_ | ConvertFrom-Json })
            Assert-True ($samples.Count -ge 2) 'continuous samples are retained'
            foreach ($sample in $samples) {
                Assert-True ($sample.qpcEnd -ge $sample.qpcStart) 'collection interval is retained'
                Assert-True ($sample.kernelIncludingIdle100ns -ge $sample.idle100ns) 'native kernel counter includes idle'
                Assert-True (@($sample.processes | Where-Object { $_.pid -eq $sample.observerPid -and $_.status -eq 'observed' }).Count -eq 1) 'observer CPU and memory are visible'
            }
            $done = Get-Content ($path+'.completion.json') -Raw | ConvertFrom-Json
            Assert-Equal 'stop-requested' $done.reason 'no sample-budget completion is disguised as success'
            Assert-Equal $samples.Count ([int]$done.samples) 'completion binds every sample'
            Assert-Equal (Get-FileHash $path -Algorithm SHA256).Hash $done.sha256 'original sample bytes are bound'
        } finally { if (-not $process.HasExited) { $process.Kill($true); $process.WaitForExit() }; $process.Dispose() }
    }
    Invoke-FixtureCase 'observation budget exhaustion retains raw samples and fails admission' {
        param($root)
        $path = Join-Path $root 'observations.jsonl'
        $process = Start-Observer $path 2
        try {
            Assert-True ($process.WaitForExit(30000)) 'budget is bounded'
            Assert-True ($process.ExitCode -ne 0) 'exhausted observer cannot qualify continuity'
            $done = Get-Content ($path+'.completion.json') -Raw | ConvertFrom-Json
            Assert-Equal 2 ([int]$done.samples) 'all pre-failure samples survive'
            Assert-True (-not [string]::IsNullOrWhiteSpace($done.error)) 'failure is explicit'
        } finally { if (-not $process.HasExited) { $process.Kill($true); $process.WaitForExit() }; $process.Dispose() }
    }
} else {
    Invoke-FixtureCase 'unsupported host fails before writing synthetic observations' {
        param($root)
        $path = Join-Path $root 'observations.jsonl'
        $process = Start-Observer $path
        try {
            Assert-True ($process.WaitForExit(30000)) 'platform refusal is bounded'
            Assert-True ($process.ExitCode -ne 0) 'native Windows counters cannot be fabricated'
            Assert-True (-not (Test-Path $path)) 'no synthetic evidence is written'
        } finally { if (-not $process.HasExited) { $process.Kill($true); $process.WaitForExit() }; $process.Dispose() }
    }
}
Complete-TestRun 'Benchmark host observations'
