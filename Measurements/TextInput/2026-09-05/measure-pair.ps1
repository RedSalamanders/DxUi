param([ValidateSet('Debug','Release')][string]$Configuration, [string]$Label='first')
$ErrorActionPreference='Stop'
$baselineRoot='Z:/src/DxUi-worktrees/av-input-baseline'
$candidateRoot='Z:/src/DxUi-worktrees/av-high-contrast'
$baselineReport=Join-Path $baselineRoot ".build/reports/TextInput-v2-baseline-$Label-$Configuration.json"
$candidateReport=Join-Path $candidateRoot ".build/reports/TextInput-v2-candidate-$Label-$Configuration.json"
if ((Test-Path -LiteralPath $baselineReport) -or (Test-Path -LiteralPath $candidateReport)) {throw 'Never overwrite a retained pair'}
$measurementProcess=[Diagnostics.Process]::GetCurrentProcess()
$oldAffinity=$measurementProcess.ProcessorAffinity
try {
    $measurementProcess.ProcessorAffinity=[IntPtr]0xFFFF
    Write-Output 'Only this measurement process and its children use affinity 0xFFFF; no other process or power policy changes.'
    & (Join-Path $baselineRoot 'performance.ps1') -Configuration $Configuration -SkipBuild -OutputPath $baselineReport
    & (Join-Path $candidateRoot 'performance.ps1') -Configuration $Configuration -SkipBuild -OutputPath $candidateReport -Baseline $baselineReport
} finally {
    $measurementProcess.ProcessorAffinity=$oldAffinity
    $measurementProcess.Dispose()
}
