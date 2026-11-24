<#
.SYNOPSIS Runs every repository validator and the tooling tests, and reports every failure before it fails.
.DESCRIPTION
The one validation entry point: validate-skills.ps1, validate-specs.ps1, validate-dependencies.ps1,
validate-test-port.ps1 and validate-build-matrix.ps1, then Tools/tests/Invoke-ToolingTests.ps1. A failing step does not
stop the others; the summary lists each step and the script fails when any did. Each validator also runs alone.
format.ps1 -Check runs the pinned clang-format. Static CI uses this script's CompilerFree mode.
test.ps1 runs the tooling tests beside its native suites.
.PARAMETER CompilerFree
Run the existing Static checks using the pinned engine and the provided comparison base.
#>
[CmdletBinding(DefaultParameterSetName='Complete')]
param(
    [Parameter(Mandatory,ParameterSetName='CompilerFree')][switch]$CompilerFree,
    [Parameter(ParameterSetName='CompilerFree')][string]$EngineRoot=(Join-Path $PSScriptRoot '.ci-engine'),
    [Parameter(ParameterSetName='CompilerFree')][string]$Base='origin/main',
    [Parameter(ParameterSetName='CompilerFree')][ValidateSet('x64','ARM64')][string]$Platform='x64',
    [Parameter(ParameterSetName='CompilerFree')][switch]$DevelopmentEngine
)
$ErrorActionPreference = 'Stop'
if ($CompilerFree) {
    $priorBuildGuard = $env:CI_NATIVE_BUILDS_DISABLED
    try {
        $env:CI_NATIVE_BUILDS_DISABLED = 'true'
        $EngineRoot = [IO.Path]::GetFullPath($EngineRoot)
        Import-Module (Join-Path $EngineRoot 'Tools/Modules/Ci.Policy.psm1') -ErrorAction Stop
        Import-Module (Join-Path $EngineRoot 'Tools/Modules/Ci.Project.psm1') -ErrorAction Stop
        $context = New-CiProjectContext -Root $PSScriptRoot -EngineRoot $EngineRoot -Phase Static -Platform $Platform -DevelopmentEngine:$DevelopmentEngine
        Test-CiProjectConfiguration $context
        & (Join-Path $EngineRoot 'Tools/Test-Workflows.ps1') -Root $PSScriptRoot
        $baseCommit = Resolve-CiCommit $PSScriptRoot $Base
        Invoke-CiProjectScript -Context $context -Script 'Tools/Test-Changelog.ps1' -Arguments @('-Base',$baseCommit)
        foreach ($script in 'validate-skills.ps1','validate-specs.ps1','validate-dependencies.ps1','validate-test-port.ps1','validate-build-matrix.ps1','Tools/tests/Test-ScopedTesting.ps1') {
            Invoke-CiProjectScript -Context $context -Script $script
        }
    } finally {
        $env:CI_NATIVE_BUILDS_DISABLED = $priorBuildGuard
    }
    return
}

Import-Module (Join-Path $PSScriptRoot 'Tools/Validation.psm1') -Force
$results = @(Invoke-DxUiValidation -Root $PSScriptRoot)
Write-Host ''
Write-Host 'Validation summary:'
foreach ($result in $results) { Write-Host ('  {0,-6} {1}' -f $(if ($result.Passed) { 'ok' } else { 'FAILED' }), $result.Step) }
$failed = @($results | Where-Object { -not $_.Passed })
if ($failed.Count) { throw "Validation failed: $(($failed | ForEach-Object { $_.Step }) -join ', ')" }
Write-Host 'All validation passed.'
