<# .SYNOPSIS Runs the repository tooling tests: validators, performance comparison and the vcpkg toolset pin. No native build is needed. #>
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$failed = [Collections.Generic.List[string]]::new()
Write-Host '== Test-ConsumerBuild.ps1'
try {
    # This aggregate retains its repository-workspace defaults. Local governed
    # callers run the standalone control with their own explicit existing parent.
    $consumerParent = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../.build/test-artifacts/consumer-build-capture'))
    [void][IO.Directory]::CreateDirectory($consumerParent)
    & (Join-Path $PSScriptRoot 'Test-ConsumerBuild.ps1') -FixtureParent $consumerParent *> (Join-Path $consumerParent 'aggregate-private.log')
} catch { $failed.Add('Test-ConsumerBuild.ps1'); Write-Host 'Consumer process controls failed; raw details remain in the private workspace log.' }
Write-Host '== Test-PairedProjectProfiles.ps1'
try { & (Join-Path $PSScriptRoot 'Test-PairedProjectProfiles.ps1') } catch { $failed.Add('Test-PairedProjectProfiles.ps1'); Write-Host $_.Exception.Message }
Write-Host '== Test-ConsumerQualification.ps1'
try { & (Join-Path $PSScriptRoot 'Test-ConsumerQualification.ps1') } catch { $failed.Add('Test-ConsumerQualification.ps1'); Write-Host $_.Exception.Message }
foreach($name in @('Test-BuildConsole.ps1','Test-MsvcSetupEntrypoints.ps1','Test-ReleaseBuildPolicy.ps1','Test-RunEvidence.ps1','Test-Validation.ps1','Test-Docs.ps1','Test-TestPort.ps1','Test-BuildMatrix.ps1','Test-PerformanceComparison.ps1','Test-PairedRun.ps1','Test-PerformancePolicy.ps1','Test-PerformanceForms.ps1','Test-PerformanceProfiling.ps1','Test-BenchmarkGate.ps1','Test-BenchmarkHost.ps1','Test-CiGate.ps1','Test-CapabilitySkipPolicy.ps1','Test-NativeScope.ps1','Test-SuiteFailure.ps1','Test-VcpkgTriplet.ps1','Test-InteractiveMode.ps1','Test-ConsumerUpdate.ps1','Test-ScopedTesting.ps1','Test-UnicodeGraphemeData.ps1','Test-MarkdownData.ps1','Test-CodeOwners.ps1')) {
    Write-Host "== $name"
    try { & (Join-Path $PSScriptRoot $name) } catch { $failed.Add($name); Write-Host $_.Exception.Message }
}
if ($failed.Count) { throw "Tooling tests failed: $($failed -join ', ')" }
Write-Host 'All tooling tests passed.'
exit 0
