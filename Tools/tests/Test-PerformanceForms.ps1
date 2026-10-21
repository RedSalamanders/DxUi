# Execute the public forms runner in isolated PowerShell children. The executable and build/identity helpers are
# portable function/script shims; these cases do not compile, render, benchmark, or provide qualification evidence.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$fixturePlatform = if ([Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString() -eq 'Arm64') { 'ARM64' } else { 'x64' }

function New-FormFrame([int] $Width, [bool] $Validation) {
    return [ordered]@{
        width=$Width; validation=$Validation; layoutAndPrepareMs=0.5; prepareMs=0.4; compositeMs=0.3; completedFrameMs=1.0
        layoutAndPrepareThreadCycles=100; compositeThreadCycles=60; measures=0; preparations=0; surfaceAllocations=0; surfaceBytes=1024
        cppAllocations=0; cppAllocationBytes=0; plainCppLiveDeltaBytes=-4; compositeCppAllocations=0; compositeCppAllocationBytes=0
    }
}
function New-FormMemory {
    return [ordered]@{ privateBytes=1024; workingSetBytes=2048; peakWorkingSetBytes=3072 }
}
function New-FormStatistics {
    return [ordered]@{ preparations=20; composites=120; surfaceBytes=1024; surfaceAllocations=2; replacementPeakBytes=2048; cachedBrushes=4; cachedTextFormats=2 }
}
function New-FormReport([uint32] $Seed = 20261010, [int] $Blocks = 1, [string] $Configuration = 'Release') {
    $schedule = @(for ($block = 0; $block -lt $Blocks; $block++) { if ($block % 2) { 'BAAB' } else { 'ABBA' } })
    $profiles = @(
        [ordered]@{ name='standard-ltr-above'; width=640; narrowWidth=520; height=720; dpi=96; columns=2; density='standard'; flow='ltr'; labels='above' },
        [ordered]@{ name='compact-rtl-beside'; width=720; narrowWidth=600; height=1440; dpi=144; columns=1; density='compact'; flow='rtl'; labels='beside' }
    )
    foreach ($profile in $profiles) {
        $profile['preTimingParity'] = [ordered]@{ states=5; leafHierarchy=$true; metadata=$true; bounds=$true; pixels=$true }
        $profile['runs'] = @(for ($block = 0; $block -lt $Blocks; $block++) {
            for ($slot = 0; $slot -lt 4; $slot++) {
                $phases = @(foreach ($phase in @('unchanged','width','validation')) {
                    [ordered]@{
                        phase=$phase
                        frames=@(for ($frame = 0; $frame -lt 40; $frame++) {
                            $width = if ($phase -eq 'width' -and $frame % 2 -eq 0) { $profile['narrowWidth'] } else { $profile['width'] }
                            New-FormFrame $width ($phase -eq 'validation' -and $frame % 2 -eq 0)
                        })
                    }
                })
                [ordered]@{
                    block=$block; slot=$slot; variant=$(if ($schedule[$block][$slot] -ceq 'A') { 'manual' } else { 'form' })
                    constructionCppAllocations=16; constructionCppAllocationBytes=1024; constructionPlainCppLiveDeltaBytes=512
                    initial=(New-FormFrame $profile['width'] $false)
                    stages=@(foreach ($stage in @('beforeConstruction','afterWarmup','afterUnchanged','afterWidth','afterValidation')) {
                        [ordered]@{ stage=$stage; plainCppLiveDeltaBytes=256; process=(New-FormMemory) }
                    })
                    afterRetirement=[ordered]@{ plainCppLiveDeltaBytes=-4; process=(New-FormMemory) }
                    finalStatistics=(New-FormStatistics); phases=$phases
                }
            }
        })
        $profile['lifecycle'] = @(foreach ($variant in @('manual','form')) {
            [ordered]@{
                variant=$variant
                stages=@(foreach ($stage in @('visible','hidden','shown','generationInvalidated','generationRecovered','hiddenGenerationReplaced',
                        'hiddenGenerationRecovered','rootRetired','detached','destroyed')) {
                    [ordered]@{ stage=$stage; measures=24; cppAllocations=4; cppAllocationBytes=128; plainCppLiveDeltaBytes=-4
                        statistics=(New-FormStatistics); process=(New-FormMemory) }
                })
            }
        })
    }
    return [ordered]@{
        schema=1; fixture='dxui-form-layout-cost-v1'; classification='diagnostic-within-candidate'; renderer='WARP'; compiler=194435217
        pointerBits=64; iteratorDebugLevel=$(if ($Configuration -eq 'Release') { 0 } else { 2 }); fields=8; warmupFrames=12; framesPerPhase=40
        seed=$Seed; blocks=$Blocks; scheduleProtocol='mt19937-independent-order-v1'; schedule=$schedule
        allocationScope='Synthetic fixture counter scope'; identityScope='Synthetic raw report requires accompanying provenance'
        deviceRecoveryScope='Synthetic generation-recovery scope'; timingScope='Synthetic explicit timing interval scope'; profiles=$profiles
    }
}
function Set-FormNativeReport([string] $Root, $Report) {
    Set-FixtureFile $Root 'native-report.txt' ((ConvertTo-Json -InputObject $Report -Depth 20 -Compress) + "`n")
}
function New-FormRunnerFixture([string] $Root, [uint32] $Seed = 20261010, [int] $Blocks = 1, [string] $Configuration = 'Release') {
    Copy-Item -LiteralPath (Join-Path $repository 'performance-forms.ps1') -Destination (Join-Path $Root 'performance-forms.ps1')
    Set-FixtureFile $Root 'Tests/Embedded/Embedded.Tests.FormLayoutBenchmark.h' 'synthetic fixture header'
    Set-FixtureFile $Root 'Tests/Embedded/Embedded.Tests.Embedded.cpp' 'synthetic minimal dispatcher'
    Set-FixtureFile $Root 'src/Layout.cpp' 'synthetic library input'
    Set-FixtureFile $Root 'dependency.txt' 'synthetic dependency input'
    Set-FixtureFile $Root ".build/$fixturePlatform/$Configuration/DxUi.EmbeddedTests.exe" 'synthetic executable bytes; a function shim runs instead'
    Set-FixtureFile $Root 'mode.txt' 'valid'
    Set-FixtureFile $Root 'attestation-version.txt' '2'
    Set-FixtureFile $Root 'Tools/PerformancePolicy.psm1' @'
Set-StrictMode -Version Latest
$script:StateCount = 0
function Get-PerformanceMeasurementState {
    param($Root, $Platform, $Configuration)
    $script:StateCount++
    [IO.File]::AppendAllText((Join-Path $Root 'calls.txt'), "state:$script:StateCount`n")
    if ($script:StateCount -eq 2 -and [IO.File]::ReadAllText((Join-Path $Root 'mode.txt')) -eq 'raw-drift') {
        [IO.File]::AppendAllText($global:FormFixtureOutput, ' ')
        [IO.File]::AppendAllText((Join-Path $Root 'native-report.txt'), ' ')
    }
    $hash = { param($Name) (Get-FileHash -LiteralPath (Join-Path $Root $Name) -Algorithm SHA256).Hash }
    return [ordered]@{
        platform=$Platform; configuration=$Configuration; nativeArchitecture=[Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
        sourceCommit=('a' * 40); sourceDirty=$true; sourceFingerprint=(& $hash 'src/Layout.cpp'); executableSha256=(& $hash ".build/$Platform/$Configuration/DxUi.EmbeddedTests.exe")
        buildIdentityVersion=[int][IO.File]::ReadAllText((Join-Path $Root 'attestation-version.txt')); resolvedBuildIdentity=('b' * 64)
        buildInputIdentity=(& $hash 'src/Layout.cpp'); buildArtifactIdentity=(& $hash ".build/$Platform/$Configuration/DxUi.EmbeddedTests.exe"); buildAttestationIdentity=('c' * 64)
        identityScope='full'; identityStatus='verifiable'; toolchainIdentity=('d' * 64); harnessIdentity=('e' * 64)
        dependencyIdentity=(& $hash 'dependency.txt'); environmentIdentity=('f' * 64)
    }
}
function Assert-PerformanceBuildAttestation {
    param($Root, $Platform, $Configuration, $State)
    [IO.File]::AppendAllText((Join-Path $Root 'calls.txt'), "attest:$script:StateCount`n")
    if ($State['buildIdentityVersion'] -ne 2 -or (Test-Path -LiteralPath (Join-Path $Root 'reject-attestation.txt'))) { throw 'Synthetic current v2 attestation rejected.' }
}
function Assert-PerformanceMeasurementState {
    param($Before, $After)
    [IO.File]::AppendAllText((Join-Path $global:FormFixtureRoot 'calls.txt'), "compare`n")
    foreach ($key in $Before.Keys) {
        if ((ConvertTo-Json -InputObject $Before[$key] -Depth 8 -Compress) -cne (ConvertTo-Json -InputObject $After[$key] -Depth 8 -Compress)) {
            throw "Synthetic $key changed during execution."
        }
    }
}
Export-ModuleMember -Function Get-PerformanceMeasurementState, Assert-PerformanceBuildAttestation, Assert-PerformanceMeasurementState
'@
    Set-FixtureFile $Root 'build.ps1' @'
param($Platform, $Configuration)
[IO.File]::AppendAllText((Join-Path $PSScriptRoot 'calls.txt'), "build:${Platform}:${Configuration}`n")
if ([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'mode.txt')) -eq 'build-failure') { $global:LASTEXITCODE = 13; return }
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'attestation-version.txt'), '2')
$global:LASTEXITCODE = 0
'@
    Set-FixtureFile $Root 'Invoke-Runner.ps1' @'
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$global:FormFixtureRoot = $PSScriptRoot
$arguments = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'arguments.json')) | ConvertFrom-Json -AsHashtable
$platform = if ($arguments.Contains('Platform')) { $arguments['Platform'] } elseif ([Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString() -eq 'Arm64') { 'ARM64' } else { 'x64' }
$configuration = if ($arguments.Contains('Configuration')) { $arguments['Configuration'] } else { 'Release' }
$exe = Join-Path $PSScriptRoot ".build/$platform/$configuration/DxUi.EmbeddedTests.exe"
Set-Item -Path ('Function:global:' + $exe) -Value {
    param($Flag, $Output, $Seed, $Blocks)
    $root = $global:FormFixtureRoot
    $global:FormFixtureOutput = $Output
    [IO.File]::AppendAllText((Join-Path $root 'calls.txt'), "native:${Flag}:${Seed}:${Blocks}`n")
    $mode = [IO.File]::ReadAllText((Join-Path $root 'mode.txt'))
    $global:LASTEXITCODE = 0
    if ($mode -eq 'missing') { return }
    [IO.File]::WriteAllBytes($Output, [IO.File]::ReadAllBytes((Join-Path $root 'native-report.txt')))
    $mutations = @{
        'input-drift'='src/Layout.cpp'; 'fixture-drift'='Tests/Embedded/Embedded.Tests.FormLayoutBenchmark.h'
        'dispatcher-drift'='Tests/Embedded/Embedded.Tests.Embedded.cpp'; 'runner-drift'='performance-forms.ps1'
        'dependency-drift'='dependency.txt'; 'artifact-drift'=".build/$platform/$configuration/DxUi.EmbeddedTests.exe"
    }
    if ($mutations.ContainsKey($mode)) { [IO.File]::AppendAllText((Join-Path $root $mutations[$mode]), "`nchanged") }
    if ($mode -eq 'exit-failure') { $global:LASTEXITCODE = 23 }
    if ($mode -eq 'after-attestation-failure') { [IO.File]::WriteAllText((Join-Path $root 'reject-attestation.txt'), 'reject') }
}.GetNewClosure()
try { & (Join-Path $PSScriptRoot 'performance-forms.ps1') @arguments; exit 0 }
catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }
'@
    Set-FormNativeReport $Root (New-FormReport -Seed $Seed -Blocks $Blocks -Configuration $Configuration)
}
function Invoke-FormRunner([string] $Root, [hashtable] $Arguments) {
    if (-not $Arguments.ContainsKey('Platform')) { $Arguments['Platform'] = $fixturePlatform }
    Set-FixtureJson $Root 'arguments.json' $Arguments
    $start = [Diagnostics.ProcessStartInfo]::new((Join-Path $PSHOME $(if ($IsWindows) { 'pwsh.exe' } else { 'pwsh' })))
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in @('-NoProfile','-File',(Join-Path $Root 'Invoke-Runner.ps1'))) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        return [pscustomobject]@{ Exit=$process.ExitCode; Text=$stdout.GetAwaiter().GetResult()+$stderr.GetAwaiter().GetResult() }
    } finally { $process.Dispose() }
}
function Assert-InvalidNativeReport([string] $Root, [string] $Output, $Run, [string] $Message) {
    Assert-True ($Run.Exit -ne 0) "$Message was accepted: $($Run.Text)"
    Assert-True (-not (Test-Path -LiteralPath $Output)) "$Message left a success report"
    Assert-True (-not (Test-Path -LiteralPath ($Output + '.raw.json'))) "$Message left a valid raw sidecar"
    $invalid = @(Get-ChildItem -LiteralPath (Split-Path -Parent $Output) -File | Where-Object { $_.Name.StartsWith([IO.Path]::GetFileName($Output) + '.') -and $_.Name.EndsWith('.invalid.json') })
    Assert-Equal 1 $invalid.Count "$Message invalid raw report count"
    Assert-Equal ((Get-FileHash -LiteralPath (Join-Path $Root 'native-report.txt') -Algorithm SHA256).Hash) ((Get-FileHash -LiteralPath $invalid[0].FullName -Algorithm SHA256).Hash) "$Message did not preserve exact native bytes"
    Assert-True $Run.Text.Contains($Message) "Failure did not explain $Message`: $($Run.Text)"
}

Invoke-FixtureCase 'a valid SkipBuild diagnostic binds exact extra inputs, retains raw bytes and checks both identities' {
    param($root)
    New-FormRunnerFixture $root
    $output = Join-Path $root 'report with spaces.json'
    $run = Invoke-FormRunner $root @{ OutputPath=$output; Blocks=1; SkipBuild=$true }
    Assert-Equal 0 $run.Exit "valid diagnostic failed: $($run.Text)"
    Assert-Equal "state:1`nattest:1`nnative:--benchmark-form-layout:20261010:1`nstate:2`ncompare`nattest:2`n" ([IO.File]::ReadAllText((Join-Path $root 'calls.txt'))) 'before/after helper order and native CLI'
    $receipt = Get-FixtureJson $root 'report with spaces.json'
    Assert-Equal 'diagnostic-within-candidate' $receipt.classification 'classification'
    Assert-True $receipt.measurement.Contains('Candidate-only Manual/Form') 'within-candidate explanation'
    Assert-True $receipt.measurement.Contains('No baseline/candidate regression or policy qualification verdict') 'qualification is not implied'
    Assert-True (-not $receipt.Contains('verdict') -and -not $receipt.Contains('gateVerdict')) 'diagnostic contains a gate verdict'
    Assert-Equal 2 ([int]$receipt.buildIdentityVersion) 'v2 identity retained'
    Assert-Equal 'full' $receipt.identityScope 'measurement identity scope'
    Assert-Equal 'Synthetic raw report requires accompanying provenance' $receipt.nativeIdentityScope 'raw scope retained'
    Assert-True $receipt.benchmarkInputsScope.Contains('historical ComplexUi closure') 'historical helper closure is not relabeled as the form scenario'
    Assert-True $receipt.buildSkipped 'SkipBuild recorded'
    Assert-Equal $output'.raw.json' $receipt.rawReportPath 'raw path'
    $nativeHash = (Get-FileHash -LiteralPath (Join-Path $root 'native-report.txt') -Algorithm SHA256).Hash
    Assert-Equal $nativeHash (Get-FileHash -LiteralPath $receipt.rawReportPath -Algorithm SHA256).Hash 'exact raw bytes retained'
    Assert-Equal $nativeHash $receipt.rawReportSha256 'raw hash'
    foreach ($path in @('Tests/Embedded/Embedded.Tests.FormLayoutBenchmark.h','Tests/Embedded/Embedded.Tests.Embedded.cpp','performance-forms.ps1')) {
        Assert-Equal (Get-FileHash -LiteralPath (Join-Path $root $path) -Algorithm SHA256).Hash $receipt.formBenchmarkInputs[$path] "exact extra input $path"
    }
    Assert-True $run.Text.Contains('no gate or policy qualification verdict') 'public output distinguishes diagnostic'
}

Invoke-FixtureCase 'default seed and blocks build through the public v2 path and use a distinct forms report' {
    param($root)
    New-FormRunnerFixture $root -Blocks 12
    Set-FixtureFile $root 'attestation-version.txt' '1'
    $run = Invoke-FormRunner $root @{}
    Assert-Equal 0 $run.Exit "default diagnostic failed: $($run.Text)"
    $report = Get-FixtureJson $root ".build/reports/Performance-Forms-$fixturePlatform-Release.json"
    Assert-Equal 12 ([int]$report.blocks) 'default blocks'
    Assert-Equal 20261010 ([int]$report.seed) 'default seed'
    Assert-True (-not $report.buildSkipped) 'default build is not marked skipped'
    Assert-True ([IO.File]::ReadAllText((Join-Path $root 'calls.txt')).StartsWith("build:${fixturePlatform}:Release`nstate:1`nattest:1`n")) 'build occurs before both identity checks'
}

Invoke-FixtureCase 'explicit uint32 seed and ASan Debug configuration reach the native CLI without a native build' {
    param($root)
    New-FormRunnerFixture $root -Seed 4294967295 -Configuration 'ASan Debug'
    $output = Join-Path $root 'report.json'
    $run = Invoke-FormRunner $root @{ OutputPath=$output; Seed='4294967295'; Blocks=1; Configuration='ASan Debug'; SkipBuild=$true }
    Assert-Equal 0 $run.Exit "explicit diagnostic failed: $($run.Text)"
    $report = Get-FixtureJson $root 'report.json'
    Assert-Equal 'ASan Debug' $report.configuration 'configuration'
    Assert-True ([IO.File]::ReadAllText((Join-Path $root 'calls.txt')).Contains('native:--benchmark-form-layout:4294967295:1')) 'seed upper bound forwarded exactly'
}

Invoke-FixtureCase 'an unsupported native flag cannot restamp a prior success when it produces no report' {
    param($root)
    New-FormRunnerFixture $root
    $output = Join-Path $root 'report.json'
    Set-FixtureFile $root 'report.json' 'previous augmented report'
    Set-FixtureFile $root 'report.json.raw.json' 'previous native bytes'
    Set-FixtureFile $root 'mode.txt' 'missing'
    $run = Invoke-FormRunner $root @{ OutputPath=$output; Blocks=1; SkipBuild=$true }
    Assert-True ($run.Exit -ne 0 -and $run.Text.Contains('wrote no report')) "missing report accepted: $($run.Text)"
    Assert-True (-not (Test-Path -LiteralPath $output) -and -not (Test-Path -LiteralPath ($output + '.raw.json'))) 'prior success remains at a current path'
    $stale = @(Get-ChildItem -LiteralPath $root -File -Filter '*.stale.json')
    Assert-Equal 2 $stale.Count 'prior report and raw sidecar archived'
    Assert-Contains @($stale | ForEach-Object { [IO.File]::ReadAllText($_.FullName) }) 'previous augmented report' 'previous report retained'
    Assert-Contains @($stale | ForEach-Object { [IO.File]::ReadAllText($_.FullName) }) 'previous native bytes' 'previous raw retained'
    Assert-True (-not ([IO.File]::ReadAllText((Join-Path $root 'calls.txt')).Contains('state:2'))) 'missing native output did not complete identity publication'
}

foreach ($case in @(
    @{ Name='malformed JSON'; Error='JSON'; Mutate={ param($root,$report) Set-FixtureFile $root 'native-report.txt' '{unfinished' } },
    @{ Name='null JSON'; Error='must be an object'; Mutate={ param($root,$report) Set-FixtureFile $root 'native-report.txt' 'null' } },
    @{ Name='wrong fixture'; Error='report.fixture'; Mutate={ param($root,$report) $report.fixture='dxui-complex-ui-v1'; Set-FormNativeReport $root $report } },
    @{ Name='wrong schema'; Error='report.schema'; Mutate={ param($root,$report) $report.schema=2; Set-FormNativeReport $root $report } },
    @{ Name='false parity'; Error='preTimingParity.pixels'; Mutate={ param($root,$report) $report.profiles[1].preTimingParity.pixels=$false; Set-FormNativeReport $root $report } },
    @{ Name='missing parity state'; Error='preTimingParity.states'; Mutate={ param($root,$report) $report.profiles[0].preTimingParity.Remove('states'); Set-FormNativeReport $root $report } },
    @{ Name='missing timing scope'; Error='report.timingScope'; Mutate={ param($root,$report) $report.Remove('timingScope'); Set-FormNativeReport $root $report } },
    @{ Name='truncated runs'; Error='runs must contain exactly'; Mutate={ param($root,$report) $report.profiles[0].runs=@($report.profiles[0].runs[0..2]); Set-FormNativeReport $root $report } },
    @{ Name='duplicate run'; Error='runs[1].slot'; Mutate={ param($root,$report) $report.profiles[0].runs[1]=$report.profiles[0].runs[0]; Set-FormNativeReport $root $report } },
    @{ Name='incorrect slot variant'; Error='runs[1].variant'; Mutate={ param($root,$report) $report.profiles[0].runs[1].variant='manual'; Set-FormNativeReport $root $report } },
    @{ Name='unknown schedule'; Error='unknown schedule'; Mutate={ param($root,$report) $report.schedule[0]='AABB'; Set-FormNativeReport $root $report } },
    @{ Name='wrong seed'; Error='report.seed'; Mutate={ param($root,$report) $report.seed=20261011; Set-FormNativeReport $root $report } },
    @{ Name='wrong block count'; Error='report.blocks'; Mutate={ param($root,$report) $report.blocks=12; Set-FormNativeReport $root $report } },
    @{ Name='missing profile'; Error='profiles must contain exactly'; Mutate={ param($root,$report) $report.profiles=@($report.profiles[0]); Set-FormNativeReport $root $report } },
    @{ Name='changed profile DPI'; Error='profiles[1].dpi'; Mutate={ param($root,$report) $report.profiles[1].dpi=96; Set-FormNativeReport $root $report } },
    @{ Name='missing phase frames'; Error='frames must contain exactly'; Mutate={ param($root,$report) $report.profiles[0].runs[0].phases[0].frames=@(); Set-FormNativeReport $root $report } },
    @{ Name='missing frame counter'; Error='compositeThreadCycles must be numeric'; Mutate={ param($root,$report) $report.profiles[0].runs[0].phases[0].frames[0].Remove('compositeThreadCycles'); Set-FormNativeReport $root $report } },
    @{ Name='wrong width phase state'; Error='frames[0].width'; Mutate={ param($root,$report) $report.profiles[0].runs[0].phases[1].frames[0].width=640; Set-FormNativeReport $root $report } },
    @{ Name='wrong validation phase state'; Error='frames[0].validation'; Mutate={ param($root,$report) $report.profiles[0].runs[0].phases[2].frames[0].validation=$false; Set-FormNativeReport $root $report } },
    @{ Name='negative duration'; Error='numeric domain'; Mutate={ param($root,$report) $report.profiles[0].runs[0].initial.prepareMs=-1; Set-FormNativeReport $root $report } },
    @{ Name='missing lifecycle stage'; Error='stages must contain exactly'; Mutate={ param($root,$report) $report.profiles[0].lifecycle[1].stages=@(); Set-FormNativeReport $root $report } }
)) {
    Invoke-FixtureCase "invalid native $($case.Name) is preserved without success publication" {
        param($root)
        New-FormRunnerFixture $root
        & $case.Mutate $root (New-FormReport)
        $output = Join-Path $root 'report.json'
        $run = Invoke-FormRunner $root @{ OutputPath=$output; Blocks=1; SkipBuild=$true }
        Assert-InvalidNativeReport $root $output $run $case.Error
    }
}

foreach ($mode in @('input-drift','fixture-drift','dispatcher-drift','runner-drift','dependency-drift','artifact-drift','raw-drift','after-attestation-failure','exit-failure')) {
    Invoke-FixtureCase "$mode preserves exact raw output and refuses a current diagnostic" {
        param($root)
        New-FormRunnerFixture $root
        Set-FixtureFile $root 'mode.txt' $mode
        $output = Join-Path $root 'report.json'
        $run = Invoke-FormRunner $root @{ OutputPath=$output; Blocks=1; SkipBuild=$true }
        $message = switch ($mode) { 'exit-failure' { 'exit code 23' }; 'after-attestation-failure' { 'attestation rejected' }; 'raw-drift' { 'report changed while it was being read' }; default { 'changed during execution' } }
        Assert-InvalidNativeReport $root $output $run $message
    }
}

foreach ($failure in @('old-attestation','build-failure','missing-executable','missing-identity-helper')) {
    Invoke-FixtureCase "$failure revokes prior success and refuses native execution" {
        param($root)
        New-FormRunnerFixture $root
        $output = Join-Path $root 'report.json'
        Set-FixtureFile $root 'report.json' 'previous success'
        $arguments = @{ OutputPath=$output; Blocks=1; SkipBuild=$true }
        switch ($failure) {
            'old-attestation' { Set-FixtureFile $root 'attestation-version.txt' '1' }
            'build-failure' { Set-FixtureFile $root 'mode.txt' 'build-failure'; $arguments.Remove('SkipBuild') }
            'missing-executable' { Remove-Item -LiteralPath (Join-Path $root ".build/$fixturePlatform/Release/DxUi.EmbeddedTests.exe") }
            'missing-identity-helper' { Remove-Item -LiteralPath (Join-Path $root 'Tools/PerformancePolicy.psm1') }
        }
        $run = Invoke-FormRunner $root $arguments
        Assert-True ($run.Exit -ne 0) "$failure was accepted"
        Assert-True (-not (Test-Path -LiteralPath $output)) "$failure retained stale success"
        Assert-Equal 1 @(Get-ChildItem -LiteralPath $root -File -Filter '*.stale.json').Count "$failure prior report preservation"
        if (Test-Path -LiteralPath (Join-Path $root 'calls.txt')) { Assert-True (-not ([IO.File]::ReadAllText((Join-Path $root 'calls.txt')).Contains('native:'))) "$failure reached native execution" }
    }
}

foreach ($arguments in @(
    @{ Seed='-1' }, @{ Seed='4294967296' }, @{ Seed='1.5' }, @{ Blocks=0 }, @{ Blocks=65 }, @{ Blocks='1.5' },
    @{ Configuration='Profile' }, @{ Platform='Win32' }, @{ Baseline='previous.json' }, @{ OutputPath='would-overwrite.ps1' }
)) {
    Invoke-FixtureCase "CLI rejects $($arguments.Keys -join ',')=$($arguments.Values -join ',') before build or native work" {
        param($root)
        New-FormRunnerFixture $root
        $run = Invoke-FormRunner $root $arguments
        Assert-True ($run.Exit -ne 0) "invalid CLI was accepted: $($run.Text)"
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $root 'calls.txt'))) 'invalid CLI reached build/measurement/native work'
    }
}

Invoke-FixtureCase 'the runner rejects emulated architecture before build or native execution' {
    param($root)
    New-FormRunnerFixture $root
    $other = if ($fixturePlatform -eq 'x64') { 'ARM64' } else { 'x64' }
    $run = Invoke-FormRunner $root @{ Platform=$other; Blocks=1; SkipBuild=$true }
    Assert-True ($run.Exit -ne 0 -and $run.Text.Contains('native execution matching')) "emulated architecture accepted: $($run.Text)"
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $root 'calls.txt'))) 'emulated architecture reached build/measurement/native work'
}

Complete-TestRun 'Performance forms runner'
