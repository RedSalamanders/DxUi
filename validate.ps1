<#
.SYNOPSIS Runs every repository validator and the tooling tests, and reports every failure before it fails.
.DESCRIPTION
The one validation entry point: validate-skills.ps1, validate-specs.ps1, validate-dependencies.ps1,
validate-test-port.ps1 and validate-build-matrix.ps1, then Tools/tests/Invoke-ToolingTests.ps1. A failing step does not
stop the others; the summary lists each step and the script fails when any did. Each validator also runs alone.
format.ps1 -Check stays separate because it needs the pinned clang-format. CI's validation job runs this script;
test.ps1 runs the tooling tests beside its native suites.
#>
[CmdletBinding()] param()
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Tools/Validation.psm1') -Force
$results = @(Invoke-DxUiValidation -Root $PSScriptRoot)
Write-Host ''
Write-Host 'Validation summary:'
foreach ($result in $results) { Write-Host ('  {0,-6} {1}' -f $(if ($result.Passed) { 'ok' } else { 'FAILED' }), $result.Step) }
$failed = @($results | Where-Object { -not $_.Passed })
if ($failed.Count) { throw "Validation failed: $(($failed | ForEach-Object { $_.Step }) -join ', ')" }
Write-Host 'All validation passed.'
