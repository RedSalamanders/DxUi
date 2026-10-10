<#
.SYNOPSIS
Runs affected test scopes, reusing identical successful local results.
.DESCRIPTION
The default compares committed work since the local merge base plus staged, unstaged,
deleted, renamed and untracked files. Unknown inputs select full coverage. Full selects
every noninteractive scope. PrePush delegates matching scopes only to an enabled and
reviewed candidate PR workflow; remaining obligations execute locally. CI always uses Force.
.PARAMETER Mode
Affected (default), Full, or PrePush. Partial/delegated coverage is labeled explicitly.
.PARAMETER BaseRef
Local comparison ref; defaults to the repository's remote default branch. Never fetches.
.PARAMETER Scopes
Explicit component scopes for focused iteration; incompatible with Full and PrePush.
.PARAMETER Configuration
Debug, Release or ASan Debug.
.PARAMETER Platform
x64 or ARM64. Runtime tests require matching native architecture.
.PARAMETER Explain
Prints the selected scopes, changed paths and reasons without building or testing.
.PARAMETER Force
Executes even when identical successful local evidence exists.
.PARAMETER SkipBuild
Uses only artifacts with a matching scoped-runner build attestation. Changed compiled source,
installed dependencies or artifact bytes fail before tests; run once without this switch to establish attestation.
.OUTPUTS
Console plan and local receipts under .build/reports/scoped-tests. Empty work is NOT_EVALUATED; interactive obligations
are explicitly INTERACTIVE_NOT_RUN. Selected/delegated work never establishes repository success.
.NOTES
Requires PowerShell 7 and Git; build toolchain for native work, gh for PR delegation.
Builds selected profile, runs repository-owned tests and writes bounded local receipts.
Does not activate the person's desktop. Uses the canonical build/test entrypoints.
.EXAMPLE
./Test-Changes.ps1 -Explain
.EXAMPLE
./Test-Changes.ps1 -Scopes Tree -Configuration Debug
.EXAMPLE
./Test-Changes.ps1 -Mode PrePush -Configuration Release
#>
[CmdletBinding()]
param(
    [ValidateSet('Affected','Full','PrePush')][string] $Mode='Affected',
    [string] $BaseRef='',
    [string[]] $Scopes=@(),
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration='Debug',
    [ValidateSet('x64','ARM64')][string] $Platform='x64',
    [switch] $Explain,
    [switch] $Force,
    [switch] $SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$modulePath=Join-Path $root 'Tools/ScopedTesting.psm1'
Import-Module $modulePath -Force
$manifest=Read-ScopedTestManifest $root
if ($Scopes.Count -and $Mode -ne 'Affected') {throw '-Scopes is only valid for focused Affected iteration.'}
if (-not $BaseRef) {$BaseRef='origin/'+$manifest.defaultBranch}
Write-Host "Native test naming: $(Assert-ScopedTestNames $root) files checked."
$changed=if ($Mode -eq 'Affected' -and -not $Scopes.Count) {@(Get-ScopedChangedPaths $root $BaseRef)} else {@()}
$plan=Get-ScopedTestPlan -Manifest $manifest -ChangedPaths $changed -Scopes $Scopes -Full:($Mode -ne 'Affected')
$interactive=@(Get-ScopedInteractiveObligations -Manifest $manifest -ChangedPaths $changed -Full:($Mode -ne 'Affected'))
Write-Host "Test plan: $Mode ($Platform $Configuration); scopes: $($plan.scopes -join ', ')"
foreach ($reason in $plan.reasons) {Write-Host "  $($reason.path): $($reason.reason) => $($reason.scopes -join ', ')"}
foreach ($obligation in $interactive) { Write-Host "INTERACTIVE_NOT_RUN: $($obligation.suite) — $($obligation.reason) [$($obligation.pattern)]" }
$instrumentation = @(Get-ScopedInstrumentation)
if ($instrumentation.Count) { Write-Host "INSTRUMENTED RUN (never a qualification receipt): $($instrumentation -join ', ')" }

$deferred=@(if ($Mode -eq 'PrePush') {Get-ScopedPrCoverage $root $manifest $Platform $Configuration})
if ($deferred.Count) {Write-Host "DEFERRED_CI (forthcoming PR, pending success): $($deferred -join ', ')"}
if ($Explain) {exit 0}
$selected=@($manifest.scopes | Where-Object {$_.name -in $plan.scopes -and $_.name -notin $deferred})
if (-not $selected.Count) {
    if ($deferred.Count) {Write-Host "NO_LOCAL_WORK; CI_PENDING ($($deferred -join ', '))"}
    else {Write-Host 'NO_WORK; repository NOT_EVALUATED'}
    exit 0
}
$native=@($selected | Where-Object {$_.native})
$reports=Join-Path $root ".build/reports/scoped-tests/$Platform-$($Configuration -replace ' ','')"
if ($native.Count) {
    $architecture=[Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
    if (($Platform -eq 'ARM64' -and $architecture -ne 'Arm64') -or ($Platform -eq 'x64' -and $architecture -ne 'X64')) {throw 'Native runtime testing requires the selected host architecture.'}
    $buildInputBefore=Get-ScopedBuildInputIdentity $root $Platform
    $attestation=Join-Path $reports 'build.json'
    if ($SkipBuild) {
        $artifact=Get-ScopedArtifactIdentity $root $Platform $Configuration
        if (-not (Test-ScopedReceipt $attestation (Get-ScopedDigest ($buildInputBefore+"`n"+$artifact)))) {throw 'SkipBuild refused: source/dependency/build identity changed or no attestation exists. Run without -SkipBuild.'}
    } else {
        & (Join-Path $root 'build.ps1') -Platform $Platform -Configuration $Configuration
        if ($LASTEXITCODE -ne 0) {throw "Build failed with exit $LASTEXITCODE."}
        # Nested entrypoints import their own modules; restore this caller's surface.
        Import-Module $modulePath -Force
        if ((Get-ScopedBuildInputIdentity $root $Platform) -cne $buildInputBefore) {throw 'Compiled source or installed dependencies changed during the build; no attestation was published.'}
        $artifact=Get-ScopedArtifactIdentity $root $Platform $Configuration
        Write-ScopedReceipt $attestation (Get-ScopedDigest ($buildInputBefore+"`n"+$artifact))
    }
}
$pending=[Collections.Generic.List[object]]::new()
$identities=@{}
$commonNative=if ($native.Count) {
    Get-ScopedDigest (@([IO.Path]::GetFullPath($root),$Platform,$Configuration,'(common)','',$buildInputBefore,$artifact,(Get-ScopedEnvironmentIdentity)) -join "`n")
} else {''}
$commonTooling=Get-ScopedDigest ((Get-ScopedSourceIdentity $root)+"`n"+(Get-ScopedEnvironmentIdentity)+"`n"+(Get-ScopedToolIdentity))
function Get-ScopeReceiptPath($Scope) {if ($Scope.native) {Join-Path $reports ($Scope.name+'.json')} else {Join-Path $root ('.build/reports/scoped-tests/independent/'+$Scope.name+'.json')}}
foreach ($scope in $selected) {
    $identity=Get-ScopedDigest ($(if ($scope.native) {$commonNative} else {$commonTooling})+"`n"+$scope.name)
    $identities[$scope.name]=$identity
    $receipt=Get-ScopeReceiptPath $scope
    $canReuse=-not $instrumentation.Count -and -not $Force -and $scope.reuse -and (Test-ScopedReceipt $receipt $identity)
    if ($canReuse -and $scope.native) {
        $priorReport=Join-Path $root ".build/reports/$($scope.name)-$Platform-$Configuration.json"
        try {
            if (-not (Test-Path -LiteralPath $priorReport -PathType Leaf)) {$canReuse=$false}
            else {
                $priorResult=Get-Content -LiteralPath $priorReport -Raw | ConvertFrom-Json
                $canReuse=$priorResult.exitCode -eq 0 -and @($priorResult.skips).Count -eq 0
            }
        } catch { $canReuse=$false }
    }
    if ($canReuse) {Write-Host "REUSED $($scope.name) (identical successful local evidence)"}
    else {
        # A failed or interrupted rerun must never leave an earlier success reusable.
        if (Test-Path -LiteralPath $receipt) {Remove-Item -LiteralPath $receipt}
        $pending.Add($scope)
    }
}
if ($pending.Count) {
    $runtime=@($pending | Where-Object {$_.native} | ForEach-Object {$_.name})
    $tooling=@($pending | Where-Object {-not $_.native})
    if ($tooling.Count) {
        foreach ($command in $manifest.toolingCommands) {
            $global:LASTEXITCODE=0
            & (Join-Path $root $command)
            if (-not $? -or $LASTEXITCODE -ne 0) {throw "Tooling failed: $command (exit $LASTEXITCODE)."}
        }
    }
    if ($runtime.Count) {
        & (Join-Path $root 'test.ps1') -Platform $Platform -Configuration $Configuration -SkipBuild -Suites $runtime -SkipTooling
        if ($LASTEXITCODE -ne 0) {throw "Selected runtime tests failed with exit $LASTEXITCODE."}
    }
    Import-Module $modulePath -Force
    $afterNative=if ($runtime.Count) {Get-ScopedRunIdentity $root $Platform $Configuration '(common)'} else {$commonNative}
    $afterTooling=Get-ScopedDigest ((Get-ScopedSourceIdentity $root)+"`n"+(Get-ScopedEnvironmentIdentity)+"`n"+(Get-ScopedToolIdentity))
    foreach ($scope in $pending) {
        $after=Get-ScopedDigest ($(if ($scope.native) {$afterNative} else {$afterTooling})+"`n"+$scope.name)
        if ($after -cne $identities[$scope.name]) {throw "Inputs changed while $($scope.name) ran; no reusable success was published."}
    }
    $environmentReduced=[Collections.Generic.List[string]]::new()
    foreach ($scope in $pending) {
        if (-not $scope.native) {
            if ($scope.reuse -and -not $instrumentation.Count) {Write-ScopedReceipt (Get-ScopeReceiptPath $scope) $identities[$scope.name]}
            continue
        }
        $instrumentationSuffix=if ($instrumentation.Count) {'.instrumented'} else {''}
        $report=Join-Path $root ".build/reports/$($scope.name)-$Platform-$Configuration$instrumentationSuffix.json"
        if (-not (Test-Path -LiteralPath $report -PathType Leaf)) {throw "Missing suite report; cannot establish reusable success for $($scope.name)."}
        $suiteResult=Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
        if ($suiteResult.exitCode -ne 0) {throw "Suite report does not show a successful exit for $($scope.name)."}
        if (@($suiteResult.skips).Count) {
            $environmentReduced.Add($scope.name)
            Write-Host "PASSED_WITH_CAPABILITY_SKIPS $($scope.name) (not reusable)"
            continue
        }
        if ($scope.reuse -and -not $instrumentation.Count) {Write-ScopedReceipt (Get-ScopeReceiptPath $scope) $identities[$scope.name]}
    }
} else { $environmentReduced=[Collections.Generic.List[string]]::new() }
$coverage=if ($instrumentation.Count) {'INSTRUMENTED_SCOPE_PASS; qualification NOT_EVALUATED; no receipt written'} elseif ($deferred.Count -and $environmentReduced.Count) {'LOCAL_SCOPES_PASSED_WITH_CAPABILITY_SKIPS; CI_PENDING; repository NOT_EVALUATED'} elseif ($deferred.Count) {'LOCAL_OBLIGATIONS_PASSED; CI_PENDING'} elseif ($environmentReduced.Count) {'SELECTED_PASSED_WITH_CAPABILITY_SKIPS; repository NOT_EVALUATED'} elseif ($plan.full) {'FULL_SCOPES_PASSED; repository NOT_EVALUATED'} else {'SELECTED_PASSED; repository NOT_EVALUATED'}
Write-Host $coverage -ForegroundColor Green
exit 0
