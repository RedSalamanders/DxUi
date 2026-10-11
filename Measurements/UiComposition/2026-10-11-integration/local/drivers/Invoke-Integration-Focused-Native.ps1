[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Debug','Release','ASan Debug')][string]$Configuration,
      [Parameter(Mandatory)][string]$Folder)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$tests = @(Get-Content -LiteralPath (Join-Path $root 'Tests/Controls/Scope.Tests.FormAccessibility.h') | ForEach-Object {
    if ($_ -cmatch '^void (TestFormAccessibility\w+)\(\);$') { $Matches[1] }
}) + @('TestUiaQuiescenceReportsTimeoutInsteadOfAcceptingAnActiveStream',
       'TestUiaQuiescenceRestartsItsIntervalForNewArrivalsIncludingTheBoundary',
       'TestUiaQuietIntervalDoesNotAttributeALaterRepeatToItsOriginalStep',
       'TestUiaArrivalDiagnosticsPreserveEventEqualityAndPerStepDeduplication')
if ($tests.Count -ne 14 -or @($tests | Sort-Object -Unique).Count -ne 14) { throw 'The focused form/quiet-boundary inventory changed.' }
$log = Join-Path $Folder 'form-runner.log'
try {
    & (Join-Path $root 'test.ps1') -Platform x64 -Configuration $Configuration -SkipBuild -SkipTooling -Suites Accessibility -Tests $tests *> $log
    exit $LASTEXITCODE
} catch {
    $_ | Out-File -LiteralPath $log -Append -Encoding utf8
    exit 1
}
