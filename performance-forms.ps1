<#
.SYNOPSIS Measure Manual/Form layout costs within this candidate using the opt-in WARP fixture.
.DESCRIPTION The report is a candidate-only diagnostic. It provides no baseline/candidate regression or policy qualification verdict.
The exact native JSON is retained beside the report as .raw.json; malformed or changed-input output is retained as .invalid.json.
.PARAMETER Seed The native fixture's uint32 schedule seed, default 20261010.
.PARAMETER Blocks Independent ABBA/BAAB blocks per profile, from 1 through 64; default 12.
.PARAMETER SkipBuild Reuse only a successful v2 build attested against its current complete inputs and artifacts.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Release',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [string] $OutputPath = '',
    [ValidatePattern('^[0-9]+$')][string] $Seed = '20261010',
    [ValidatePattern('^(?:[1-9]|[1-5][0-9]|6[0-4])$')][string] $Blocks = '12',
    [switch] $SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$seedValue = [uint32]0
if (-not [uint32]::TryParse($Seed, [Globalization.NumberStyles]::None, [Globalization.CultureInfo]::InvariantCulture, [ref]$seedValue)) {
    throw 'The form-layout seed must be a uint32 decimal integer (0 through 4294967295).'
}
$blockCount = [int]$Blocks
$native = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
if (($Platform -eq 'ARM64' -and $native -ne 'Arm64') -or ($Platform -eq 'x64' -and $native -ne 'X64')) {
    throw 'Form-layout diagnostics require native execution matching the requested architecture.'
}
if (-not $OutputPath) { $OutputPath = Join-Path $PSScriptRoot ".build/reports/Performance-Forms-$Platform-$Configuration.json" }
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
if ([IO.Path]::GetExtension($OutputPath) -ine '.json') { throw 'The form-layout output path must name a JSON report.' }
$rawPath = $OutputPath + '.raw.json'
$exe = Join-Path $PSScriptRoot ".build/$Platform/$Configuration/DxUi.EmbeddedTests.exe"

function Assert-FormObject($Value, [string] $Context) {
    if ($Value -isnot [Collections.IDictionary]) { throw "Invalid form-layout report: $Context must be an object." }
}
function Assert-FormExact([Collections.IDictionary] $Value, [string] $Name, $Expected, [string] $Context) {
    if (-not $Value.Contains($Name) -or
        (ConvertTo-Json -InputObject $Value[$Name] -Compress) -cne (ConvertTo-Json -InputObject $Expected -Compress)) {
        throw "Invalid form-layout report: $Context.$Name does not match the native fixture."
    }
}
function Assert-FormArray([Collections.IDictionary] $Value, [string] $Name, [int] $Count, [string] $Context) {
    if (-not $Value.Contains($Name) -or $Value[$Name] -isnot [array] -or $Value[$Name].Count -ne $Count) {
        throw "Invalid form-layout report: $Context.$Name must contain exactly $Count entries."
    }
}
function Assert-FormNumber([Collections.IDictionary] $Value, [string] $Name, [string] $Context, [switch] $Integer, [switch] $Signed) {
    if (-not $Value.Contains($Name) -or $null -eq $Value[$Name] -or
        [Type]::GetTypeCode($Value[$Name].GetType()) -notin @('Byte','SByte','Int16','UInt16','Int32','UInt32','Int64','UInt64','Single','Double','Decimal')) {
        throw "Invalid form-layout report: $Context.$Name must be numeric."
    }
    $number = [double]$Value[$Name]
    if (-not [double]::IsFinite($number) -or (-not $Signed -and $number -lt 0) -or ($Integer -and [Math]::Truncate($number) -ne $number)) {
        throw "Invalid form-layout report: $Context.$Name is outside its numeric domain."
    }
}
function Assert-FormMemory($Value, [string] $Context) {
    Assert-FormObject $Value $Context
    foreach ($name in @('privateBytes','workingSetBytes','peakWorkingSetBytes')) { Assert-FormNumber $Value $name $Context -Integer }
}
function Assert-FormStatistics($Value, [string] $Context) {
    Assert-FormObject $Value $Context
    foreach ($name in @('preparations','composites','surfaceBytes','surfaceAllocations','replacementPeakBytes','cachedBrushes','cachedTextFormats')) {
        Assert-FormNumber $Value $name $Context -Integer
    }
}
function Assert-FormFrame($Value, [string] $Context, [int] $Width, [bool] $Validation) {
    Assert-FormObject $Value $Context
    Assert-FormExact $Value 'width' $Width $Context
    Assert-FormExact $Value 'validation' $Validation $Context
    foreach ($name in @('layoutAndPrepareMs','prepareMs','compositeMs','completedFrameMs')) { Assert-FormNumber $Value $name $Context }
    foreach ($name in @('layoutAndPrepareThreadCycles','compositeThreadCycles','measures','preparations','surfaceAllocations','surfaceBytes',
            'cppAllocations','cppAllocationBytes','compositeCppAllocations','compositeCppAllocationBytes')) {
        Assert-FormNumber $Value $name $Context -Integer
    }
    Assert-FormNumber $Value 'plainCppLiveDeltaBytes' $Context -Integer -Signed
    if ($Value['layoutAndPrepareMs'] -lt $Value['prepareMs'] -or $Value['completedFrameMs'] -lt $Value['layoutAndPrepareMs'] -or
        $Value['completedFrameMs'] -lt $Value['compositeMs']) { throw "Invalid form-layout report: $Context has inconsistent timing intervals." }
}
function Assert-FormReport($Receipt) {
    Assert-FormObject $Receipt 'report'
    foreach ($entry in @(@('schema',1), @('fixture','dxui-form-layout-cost-v1'), @('classification','diagnostic-within-candidate'),
            @('renderer','WARP'), @('pointerBits',64), @('fields',8), @('warmupFrames',12), @('framesPerPhase',40),
            @('seed',$seedValue), @('blocks',$blockCount), @('scheduleProtocol','mt19937-independent-order-v1'))) {
        Assert-FormExact $Receipt $entry[0] $entry[1] 'report'
    }
    Assert-FormNumber $Receipt 'compiler' 'report' -Integer
    if ($Receipt['compiler'] -le 0) { throw 'Invalid form-layout report: compiler identity is absent.' }
    $iteratorLevel = if ($Configuration -eq 'Release') { 0 } else { 2 }
    Assert-FormExact $Receipt 'iteratorDebugLevel' $iteratorLevel 'report'
    foreach ($name in @('allocationScope','identityScope','deviceRecoveryScope','timingScope')) {
        if (-not $Receipt.Contains($name) -or $Receipt[$name] -isnot [string] -or [string]::IsNullOrWhiteSpace($Receipt[$name])) {
            throw "Invalid form-layout report: report.$name is absent."
        }
    }
    Assert-FormArray $Receipt 'schedule' $blockCount 'report'
    foreach ($order in $Receipt['schedule']) {
        if ($order -isnot [string] -or $order -cnotin @('ABBA','BAAB')) { throw 'Invalid form-layout report: unknown schedule order.' }
    }
    $profiles = @(
        [ordered]@{ name='standard-ltr-above'; width=640; narrowWidth=520; height=720; dpi=96; columns=2; density='standard'; flow='ltr'; labels='above' },
        [ordered]@{ name='compact-rtl-beside'; width=720; narrowWidth=600; height=1440; dpi=144; columns=1; density='compact'; flow='rtl'; labels='beside' }
    )
    $phaseNames = @('unchanged','width','validation')
    $stageNames = @('beforeConstruction','afterWarmup','afterUnchanged','afterWidth','afterValidation')
    $lifecycleNames = @('visible','hidden','shown','generationInvalidated','generationRecovered','hiddenGenerationReplaced',
        'hiddenGenerationRecovered','rootRetired','detached','destroyed')
    Assert-FormArray $Receipt 'profiles' $profiles.Count 'report'
    for ($profileIndex = 0; $profileIndex -lt $profiles.Count; $profileIndex++) {
        $expected = $profiles[$profileIndex]
        $profile = $Receipt['profiles'][$profileIndex]
        $context = "report.profiles[$profileIndex]"
        Assert-FormObject $profile $context
        foreach ($name in $expected.Keys) { Assert-FormExact $profile $name $expected[$name] $context }
        if (-not $profile.Contains('preTimingParity')) { throw "Invalid form-layout report: $context.preTimingParity is absent." }
        Assert-FormObject $profile['preTimingParity'] "$context.preTimingParity"
        Assert-FormExact $profile['preTimingParity'] 'states' 5 "$context.preTimingParity"
        foreach ($name in @('leafHierarchy','metadata','bounds','pixels')) { Assert-FormExact $profile['preTimingParity'] $name $true "$context.preTimingParity" }
        Assert-FormArray $profile 'runs' ($blockCount * 4) $context
        for ($index = 0; $index -lt $profile['runs'].Count; $index++) {
            $run = $profile['runs'][$index]
            $runContext = "$context.runs[$index]"
            $block = [int][Math]::Floor($index / 4)
            $slot = $index % 4
            $variant = if ($Receipt['schedule'][$block][$slot] -ceq 'A') { 'manual' } else { 'form' }
            Assert-FormObject $run $runContext
            Assert-FormExact $run 'block' $block $runContext
            Assert-FormExact $run 'slot' $slot $runContext
            Assert-FormExact $run 'variant' $variant $runContext
            foreach ($name in @('constructionCppAllocations','constructionCppAllocationBytes')) { Assert-FormNumber $run $name $runContext -Integer }
            Assert-FormNumber $run 'constructionPlainCppLiveDeltaBytes' $runContext -Integer -Signed
            if (-not $run.Contains('initial')) { throw "Invalid form-layout report: $runContext.initial is absent." }
            Assert-FormFrame $run['initial'] "$runContext.initial" $expected['width'] $false
            Assert-FormArray $run 'stages' $stageNames.Count $runContext
            for ($stageIndex = 0; $stageIndex -lt $stageNames.Count; $stageIndex++) {
                $stage = $run['stages'][$stageIndex]
                $stageContext = "$runContext.stages[$stageIndex]"
                Assert-FormObject $stage $stageContext
                Assert-FormExact $stage 'stage' $stageNames[$stageIndex] $stageContext
                Assert-FormNumber $stage 'plainCppLiveDeltaBytes' $stageContext -Integer -Signed
                if (-not $stage.Contains('process')) { throw "Invalid form-layout report: $stageContext.process is absent." }
                Assert-FormMemory $stage['process'] "$stageContext.process"
            }
            if (-not $run.Contains('afterRetirement')) { throw "Invalid form-layout report: $runContext.afterRetirement is absent." }
            Assert-FormObject $run['afterRetirement'] "$runContext.afterRetirement"
            Assert-FormNumber $run['afterRetirement'] 'plainCppLiveDeltaBytes' "$runContext.afterRetirement" -Integer -Signed
            Assert-FormMemory $run['afterRetirement']['process'] "$runContext.afterRetirement.process"
            Assert-FormStatistics $run['finalStatistics'] "$runContext.finalStatistics"
            Assert-FormArray $run 'phases' $phaseNames.Count $runContext
            for ($phaseIndex = 0; $phaseIndex -lt $phaseNames.Count; $phaseIndex++) {
                $phase = $run['phases'][$phaseIndex]
                $phaseContext = "$runContext.phases[$phaseIndex]"
                Assert-FormObject $phase $phaseContext
                Assert-FormExact $phase 'phase' $phaseNames[$phaseIndex] $phaseContext
                Assert-FormArray $phase 'frames' 40 $phaseContext
                for ($frameIndex = 0; $frameIndex -lt 40; $frameIndex++) {
                    $width = if ($phaseIndex -eq 1 -and $frameIndex % 2 -eq 0) { $expected['narrowWidth'] } else { $expected['width'] }
                    Assert-FormFrame $phase['frames'][$frameIndex] "$phaseContext.frames[$frameIndex]" $width ($phaseIndex -eq 2 -and $frameIndex % 2 -eq 0)
                }
            }
        }
        Assert-FormArray $profile 'lifecycle' 2 $context
        for ($variantIndex = 0; $variantIndex -lt 2; $variantIndex++) {
            $lifecycle = $profile['lifecycle'][$variantIndex]
            $lifecycleContext = "$context.lifecycle[$variantIndex]"
            Assert-FormObject $lifecycle $lifecycleContext
            Assert-FormExact $lifecycle 'variant' @('manual','form')[$variantIndex] $lifecycleContext
            Assert-FormArray $lifecycle 'stages' $lifecycleNames.Count $lifecycleContext
            for ($stageIndex = 0; $stageIndex -lt $lifecycleNames.Count; $stageIndex++) {
                $stage = $lifecycle['stages'][$stageIndex]
                $stageContext = "$lifecycleContext.stages[$stageIndex]"
                Assert-FormObject $stage $stageContext
                Assert-FormExact $stage 'stage' $lifecycleNames[$stageIndex] $stageContext
                foreach ($name in @('measures','cppAllocations','cppAllocationBytes')) { Assert-FormNumber $stage $name $stageContext -Integer }
                Assert-FormNumber $stage 'plainCppLiveDeltaBytes' $stageContext -Integer -Signed
                Assert-FormStatistics $stage['statistics'] "$stageContext.statistics"
                Assert-FormMemory $stage['process'] "$stageContext.process"
            }
        }
    }
}
function Get-FormMeasurementState {
    $state = Get-PerformanceMeasurementState -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration
    $inputs = [ordered]@{}
    foreach ($path in @('Tests/Embedded/Embedded.Tests.FormLayoutBenchmark.h','Tests/Embedded/Embedded.Tests.Embedded.cpp','performance-forms.ps1')) {
        $inputs[$path] = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $path) -Algorithm SHA256).Hash
    }
    $state['formBenchmarkInputs'] = $inputs
    return $state
}

New-Item -ItemType Directory -Path (Split-Path -Parent $OutputPath) -Force | Out-Null
# Revoke the requested paths before a build or identity check can fail. Keep earlier bytes explicitly stale.
foreach ($path in @($OutputPath,$rawPath)) {
    if (Test-Path -LiteralPath $path -PathType Leaf) { Move-Item -LiteralPath $path -Destination ($path + '.' + [guid]::NewGuid().ToString('N') + '.stale.json') }
}
Import-Module (Join-Path $PSScriptRoot 'Tools/PerformancePolicy.psm1') -Force
if (-not $SkipBuild) {
    $global:LASTEXITCODE = 0
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform
    if ($LASTEXITCODE -ne 0) { throw "Form-layout diagnostic build failed with exit code $LASTEXITCODE." }
}
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "The form-layout benchmark executable is missing: $exe" }
Push-Location $PSScriptRoot
try {
    $before = Get-FormMeasurementState
    Assert-PerformanceBuildAttestation -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration -State $before
    $stampPath = $OutputPath + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
    $rawStampPath = $rawPath + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
    try {
        $global:LASTEXITCODE = 0
        & $exe '--benchmark-form-layout' $OutputPath $seedValue.ToString([Globalization.CultureInfo]::InvariantCulture) $blockCount
        if ($LASTEXITCODE -ne 0) { throw "Form-layout benchmark failed with exit code $LASTEXITCODE. Executable: $exe" }
        if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) { throw "The form-layout benchmark wrote no report: $OutputPath. Executable: $exe" }
        $completedUtc = [DateTime]::UtcNow.ToString('o')
        $rawBytes = [IO.File]::ReadAllBytes($OutputPath)
        $rawSha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($rawBytes))
        $after = Get-FormMeasurementState
        Assert-PerformanceMeasurementState -Before $before -After $after
        Assert-PerformanceBuildAttestation -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration -State $after
        $receipt = [Text.UTF8Encoding]::new($false, $true).GetString($rawBytes) | ConvertFrom-Json -AsHashtable -Depth 20
        Assert-FormReport $receipt
        if ((Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash -cne $rawSha256) { throw 'The native form-layout report changed while it was being read.' }
        $receipt['nativeIdentityScope'] = $receipt['identityScope']
        foreach ($key in $before.Keys) { $receipt[$key] = $before[$key] }
        $receipt['buildSkipped'] = [bool]$SkipBuild
        $receipt['completedUtc'] = $completedUtc
        $receipt['workloadOwner'] = 'DxUi'
        $receipt['benchmarkInputsScope'] = 'benchmarkInputs and benchmarkSha256 retain the shared helper historical ComplexUi closure; formBenchmarkInputs binds this Form layout fixture, dispatcher and runner.'
        $receipt['rawReportPath'] = $rawPath
        $receipt['rawReportSha256'] = $rawSha256
        $receipt['measurement'] = 'Candidate-only Manual/Form layout diagnostic. No baseline/candidate regression or policy qualification verdict. Allocation calls/bytes include aligned new; plain live-byte deltas exclude aligned blocks. All UI-thread C++ counters exclude COM/driver heaps.'
        [IO.File]::WriteAllText($stampPath, (ConvertTo-Json -InputObject $receipt -Depth 20) + "`n", [Text.UTF8Encoding]::new($false))
        # Publish the same immutable bytes that were parsed, rather than re-reading a mutable native path.
        [IO.File]::WriteAllBytes($rawStampPath, $rawBytes)
        Move-Item -LiteralPath $rawStampPath -Destination $rawPath
        Move-Item -LiteralPath $stampPath -Destination $OutputPath -Force
    } catch {
        # Native bytes remain untouched until validated publication, including malformed JSON and failed identity checks.
        foreach ($path in @($OutputPath,$rawPath)) {
            if (Test-Path -LiteralPath $path -PathType Leaf) { Move-Item -LiteralPath $path -Destination ($path + '.' + [guid]::NewGuid().ToString('N') + '.invalid.json') }
        }
        foreach ($path in @($stampPath,$rawStampPath)) {
            if (Test-Path -LiteralPath $path -PathType Leaf) { Remove-Item -LiteralPath $path }
        }
        throw
    }
    Write-Host "Form-layout diagnostic report: $OutputPath"
    Write-Host "Exact native report: $rawPath"
    Write-Host 'Candidate-only Manual/Form diagnostic; no gate or policy qualification verdict.'
} finally { Pop-Location }
