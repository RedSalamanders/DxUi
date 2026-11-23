<#
.SYNOPSIS Convert a full frame timing receipt (raw run reports embedded) into the retained form that is committed.
.DESCRIPTION Recomputes every run summary from its embedded raw report and the judgement from the summaries, and refuses
when either differs from what the full receipt holds (Tools/FrameTiming.psm1, ConvertTo-FrameTimingRetainedReceipt).
Each raw report's size is read from the report file the receipt names, when that file still exists with the recorded
SHA-256; otherwise the size stays null. The input is never modified. The output is compact UTF-8 JSON without a BOM.
.EXAMPLE ./Tools/Convert-FrameTimingReceipt.ps1 -Path .build/reports/frame-timing/raw/<run>/receipt-full.json -OutputPath Measurements/.../receipt.json
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $Path,
    [Parameter(Mandatory)][string] $OutputPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'FrameTiming.psm1') -Force
$Path = [IO.Path]::GetFullPath($Path)
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
if ($Path -eq $OutputPath) { throw 'The retained receipt must not overwrite the full receipt.' }
$receipt = [IO.File]::ReadAllText($Path, [Text.UTF8Encoding]::new($false, $true)) | ConvertFrom-Json -AsHashtable -Depth 60
$sizes = @{}
foreach ($run in @($receipt['sessions'] | ForEach-Object { $_['runs'] })) {
    $report = [string]$run['report']
    if ($report -and (Test-Path -LiteralPath $report -PathType Leaf) -and (Get-FileHash -LiteralPath $report -Algorithm SHA256).Hash -ceq [string]$run['reportSha256']) {
        $sizes[[string]$run['reportSha256']] = (Get-Item -LiteralPath $report).Length
    }
}
$retained = ConvertTo-FrameTimingRetainedReceipt -Receipt $receipt -ReportSizes $sizes
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null
[IO.File]::WriteAllText($OutputPath, (ConvertTo-Json -InputObject $retained -Depth 40 -Compress) + "`n", [Text.UTF8Encoding]::new($false))
$runs = @($retained['sessions'] | ForEach-Object { $_['runs'] })
Write-Host ("Retained frame timing receipt: {0} ({1:N0} bytes from {2:N0}); {3} of {4} raw report sizes found." -f $OutputPath,
    (Get-Item -LiteralPath $OutputPath).Length, (Get-Item -LiteralPath $Path).Length, @($runs | Where-Object { $null -ne $_['reportSizeBytes'] }).Count, $runs.Count)
