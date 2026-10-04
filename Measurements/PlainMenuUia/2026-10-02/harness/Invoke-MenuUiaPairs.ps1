<#
.SYNOPSIS Runs the MenuUiaCost fixture of two DxUi.ControlTests.exe builds as interleaved A, B, B, A sets.
.DESCRIPTION Each run is one process, started and awaited here, with --no-activate: the fixture never takes the
foreground, the keyboard focus or the pointer. A run's standard output (one JSON object per line) goes to
<Label>.jsonl and its standard error to <Label>.err.log in OutputDirectory; runs.tsv lists every run in order with its
executable's SHA-256, the name of the directory that holds it, and the exit code. A failing run stops the set.
.PARAMETER Baseline The baseline (A) executable.
.PARAMETER Candidate The candidate (B) executable.
.PARAMETER Sets How many A, B, B, A passes to run (three give six runs per side).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $Baseline,
    [Parameter(Mandatory)][string] $Candidate,
    [Parameter(Mandatory)][string] $OutputDirectory,
    [ValidateRange(1, 20)][int] $Sets = 3
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$executables = @{ A = (Resolve-Path -LiteralPath $Baseline).Path; B = (Resolve-Path -LiteralPath $Candidate).Path }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$hashes = @{}
foreach ($side in 'A', 'B') { $hashes[$side] = (Get-FileHash -Algorithm SHA256 -LiteralPath $executables[$side]).Hash }
$index = Join-Path $OutputDirectory 'runs.tsv'
if (-not (Test-Path -LiteralPath $index)) { "label`tside`tstartedUtc`tseconds`texitCode`tsha256`tbuild" | Set-Content -LiteralPath $index -Encoding utf8 }
$counts = @{ A = @(Get-ChildItem -LiteralPath $OutputDirectory -Filter 'A*.jsonl').Count; B = @(Get-ChildItem -LiteralPath $OutputDirectory -Filter 'B*.jsonl').Count }
for ($set = 0; $set -lt $Sets; $set++) {
    foreach ($side in 'A', 'B', 'B', 'A') {
        $counts[$side]++
        $label = "$side$($counts[$side])"
        $output = Join-Path $OutputDirectory "$label.jsonl"
        $errors = Join-Path $OutputDirectory "$label.err.log"
        $started = [DateTime]::UtcNow
        & $executables[$side] '--suite=MenuUiaCost' '--no-activate' 1> $output 2> $errors
        $exitCode = $LASTEXITCODE
        $seconds = ([DateTime]::UtcNow - $started).TotalSeconds
        "$label`t$side`t$($started.ToString('o'))`t$([Math]::Round($seconds, 1))`t$exitCode`t$($hashes[$side])`t$(Split-Path (Split-Path $executables[$side]) -Leaf)" | Add-Content -LiteralPath $index -Encoding utf8
        Write-Host ("{0}: exit {1} in {2:N1} s" -f $label, $exitCode, $seconds)
        if ($exitCode -ne 0) { throw "Run $label failed with exit code $exitCode; see $errors" }
    }
}
