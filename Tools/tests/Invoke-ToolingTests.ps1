<# .SYNOPSIS Runs the repository tooling tests: validators, performance comparison and the vcpkg toolset pin. No native build is needed. #>
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$failed = [Collections.Generic.List[string]]::new()
foreach ($name in @('Test-Validation.ps1', 'Test-Docs.ps1', 'Test-TestPort.ps1', 'Test-BuildMatrix.ps1', 'Test-PerformanceComparison.ps1', 'Test-PairedRun.ps1', 'Test-BenchmarkGate.ps1', 'Test-NativeScope.ps1', 'Test-SuiteFailure.ps1', 'Test-VcpkgTriplet.ps1', 'Test-InteractiveMode.ps1', 'Test-ConsumerUpdate.ps1', 'Test-ScopedTesting.ps1')) {
    Write-Host "== $name"
    try { & (Join-Path $PSScriptRoot $name) } catch { $failed.Add($name); Write-Host $_.Exception.Message }
}
if ($failed.Count) { throw "Tooling tests failed: $($failed -join ', ')" }
Write-Host 'All tooling tests passed.'
exit 0
