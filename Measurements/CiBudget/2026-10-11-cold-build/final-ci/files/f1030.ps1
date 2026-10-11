[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$taskRoot = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$taskHelper = Join-Path $PSScriptRoot 'Review-PrCiEvidence.ps1'
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($taskHelper,[ref]$tokens,[ref]$errors)
if (@($errors).Count) { throw 'Review helper parser errors.' }
foreach ($name in @('Require','Assert-Sha','Assert-PairedRawBudgetAndFixture','Bind-PairedRoleReceipt')) {
    $definition = @($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $name},$true))
    if ($definition.Count -ne 1) { throw "Cannot extract helper $name." }
    . ([scriptblock]::Create($definition[0].Extent.Text))
}
Import-Module (Join-Path $taskRoot 'Tools/PairedRun.psm1') -Force
$historical = Join-Path $taskRoot 'Measurements/Performance/2026-10-10-aa-v2'
$summary = Get-Content -Raw (Join-Path $historical 'summary.receipt.txt') | ConvertFrom-Json -AsHashtable
$paths = @(Get-PairedHarness) + @('build.ps1','Tools/ScopedTesting.psm1','Tools/Validation.psm1','Tools/Changelog.psm1','Tests/Embedded/BenchmarkMain.h','Tests/Embedded/ComplexUiBenchmark.h','Tests/Support/HeapDiagnostic.h')
$states = @{}; $count=0
foreach ($scenario in $summary['scenarios']) {
    foreach ($step in $summary['steps']) {
        $name = [string]$step['Name']
        $value = Get-Content -Raw (Join-Path $historical "$($scenario['scenario'])-$name.json") | ConvertFrom-Json -AsHashtable
        Bind-PairedRoleReceipt $summary $scenario $name $value $states $paths
        Assert-PairedRawBudgetAndFixture $value ([string]$scenario['scenario'])
        $count++
    }
}
if ($count -ne 144 -or $states.Count -ne 2) { throw 'Historical compatibility audit is incomplete.' }
[ordered]@{ checks=$count; scheduledRoleIdentities=$states; helperSha256=(Get-FileHash $taskHelper).Hash; purpose='Compatibility of external helper with immutable historical A/A receipts only; not current candidate qualification'; completedUtc=[DateTime]::UtcNow.ToString('o') } | ConvertTo-Json -Depth 7 | Set-Content (Join-Path $PSScriptRoot 'historical-role-audit.receipt.txt')
Write-Output 'HISTORICAL_ROLE_BUDGET_COMPATIBILITY: 144 reports passed'
