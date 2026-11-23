# Real Git/content/binary fixtures; does not build or take desktop focus.
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repository=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $repository 'Tools/ScopedTesting.psm1')
$nativeEvidence = Import-Module (Join-Path $repository 'Tools/DxUiBuildEvidence.psm1') -PassThru
$passed=0
function Assert-Scope([bool]$Condition,[string]$Message) {if(-not $Condition){throw $Message}}
function Run-Case([string]$Name,[scriptblock]$Action) {& $Action;$script:passed++;Write-Host "PASS $Name"}
function Write-Fixture([string]$Path,[string]$Content) {[void](New-Item -ItemType Directory -Path (Split-Path $Path) -Force);[IO.File]::WriteAllText($Path,$Content,[Text.UTF8Encoding]::new($false))}
function Get-EnvironmentSnapshot([string[]]$Names) {
    $snapshot=[ordered]@{}
    foreach($name in $Names) {
        $entry=Get-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue
        $snapshot[$name]=[pscustomobject]@{exists=($null -ne $entry);value=if($null -ne $entry){[string]$entry.Value}else{$null}}
    }
    return $snapshot
}
function Restore-EnvironmentSnapshot([Collections.IDictionary]$Snapshot) {
    foreach($name in $Snapshot.Keys) {
        $entry=$Snapshot[$name]
        if($entry.exists) {Set-Item -LiteralPath "Env:$name" -Value ([string]$entry.value)}
        else {Remove-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue}
    }
}
function Assert-EnvironmentSnapshot([Collections.IDictionary]$Snapshot,[string]$Context) {
    foreach($name in $Snapshot.Keys) {
        $expected=$Snapshot[$name]
        $actual=Get-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue
        Assert-Scope (($null -ne $actual) -eq [bool]$expected.exists) "$Context changed presence of $name"
        if($expected.exists) {Assert-Scope ([string]$actual.Value -ceq [string]$expected.value) "$Context changed value of $name"}
    }
}
function Write-FixtureDependencies([string]$Root,[string]$Platform='x64') {
    $triplet=if($Platform -eq 'ARM64'){'arm64-windows'}else{'x64-windows'}
    $install=Join-Path $Root ".build/vcpkg_installed/$Platform"
    Write-Fixture (Join-Path $install "$triplet/include/wil/resource.h") 'fixture WIL header'
    Write-Fixture (Join-Path $install "$triplet/share/wil/vcpkg_abi_info.txt") 'fixture ABI'
    Write-Fixture (Join-Path $install "$triplet/lib/wil.lib") 'fixture installed library'
    Write-Fixture (Join-Path $install 'vcpkg/status') "Package: wil`nVersion: 1.0`nAbi: fixture-abi`n"
    Write-Fixture (Join-Path $install "vcpkg/info/wil_1.0_$triplet.list") 'fixture package file inventory'
}
function Invoke-FixtureGit([string[]]$Arguments) {& git -C $fixture @Arguments *> $null;if($LASTEXITCODE){throw "Fixture git failed: $Arguments"}}
$fixture=Join-Path $repository ('.build/ToolTests/ScopedTesting-'+[guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $fixture -Force)
try {
    function Get-NativeRunIdentity([string] $Root,[string] $Platform,[string] $Configuration,[string] $Scope) {
        & $nativeEvidence { param($r,$p,$c,$s) Get-ScopedRunIdentity $r $p $c $s } $Root $Platform $Configuration $Scope
    }
    Run-Case 'actual ignored engine bytes bind build and tooling evidence without changing iteration maps' {
        $product=Join-Path $fixture 'engine-input-project'
        [void][IO.Directory]::CreateDirectory($product)
        $initialized=& git -C $product init --quiet 2>&1
        if($LASTEXITCODE){throw "Owned product fixture initialization failed: $initialized"}
        Write-Fixture (Join-Path $product '.gitignore') ".build/`n.ci-engine/`n"
        Write-Fixture (Join-Path $product '.ci/project.json') ([IO.File]::ReadAllText((Join-Path $repository '.ci/project.json')))
        Write-Fixture (Join-Path $product 'runtime.cpp') 'synthetic source'
        $engine=Join-Path $product '.ci-engine'
        $cloneOutput=& git clone --quiet --no-hardlinks (Join-Path $repository '.ci-engine') $engine 2>&1
        if($LASTEXITCODE){throw "Owned engine fixture clone failed: $cloneOutput"}
        $mapBefore=Get-ScopedMapIdentity ((& $nativeEvidence {param($root) Get-ScopedSourceMap $root} $product))
        $toolingBefore=Get-ScopedSourceIdentity $product
        $buildBefore=Get-ScopedSourceIdentity $product -CompiledOnly
        if([OperatingSystem]::IsWindows()){
            Assert-Scope ((Get-ScopedSourceIdentity $product.ToUpperInvariant()) -ceq $toolingBefore) 'Windows path casing invalidated tooling evidence'
            Assert-Scope ((Get-ScopedSourceIdentity $product.ToUpperInvariant() -CompiledOnly) -ceq $buildBefore) 'Windows path casing invalidated build evidence'
        }
        Write-Fixture (Join-Path $engine '.build/fixture-output.json') '{"generated":true}'
        Assert-Scope ((Get-ScopedSourceIdentity $product) -ceq $toolingBefore) 'Generated engine output invalidated tooling'
        Assert-Scope ((Get-ScopedSourceIdentity $product -CompiledOnly) -ceq $buildBefore) 'Generated engine output invalidated build evidence'
        $policy=Join-Path $engine 'Tools/Modules/Ci.Policy.psm1'
        $original=[IO.File]::ReadAllBytes($policy)
        try{
            [IO.File]::AppendAllText($policy,"`n# Synthetic changed engine implementation.`n")
            foreach($compiled in @($false,$true)){
                $refusal=''
                try{$null=Get-ScopedSourceIdentity $product -CompiledOnly:$compiled}catch [System.Management.Automation.RuntimeException]{$refusal=$_.Exception.Message}
                Assert-Scope ($refusal.Contains('Local engine checkout must be clean')) 'Modified ignored engine retained reusable build/tooling evidence'
            }
            Assert-Scope ((Get-ScopedMapIdentity ((& $nativeEvidence {param($root) Get-ScopedSourceMap $root} $product))) -ceq $mapBefore) 'Engine identity polluted product iteration maps'
        }finally{[IO.File]::WriteAllBytes($policy,$original)}
        Assert-Scope ((Get-ScopedSourceIdentity $product) -ceq $toolingBefore) 'Restored exact engine did not recover its tooling identity'
        Assert-Scope ((Get-ScopedSourceIdentity $product -CompiledOnly) -ceq $buildBefore) 'Restored exact engine did not recover its build identity'

    }
    Invoke-FixtureGit @('init','-q')
    Write-Fixture (Join-Path $fixture '.git/info/exclude') "`n.build/`nengine-input-project/`n"
    Write-Fixture (Join-Path $fixture 'code.cpp') 'initial'
    $manifest=Read-ScopedTestManifest $repository
    Run-Case 'all active native tests satisfy naming and inventory' {Assert-Scope ((Assert-ScopedTestNames $repository) -gt 0) 'Empty native inventory'}
    Run-Case 'every scope rule matches a current repository input' {
        $unmatched=@(& (Get-Module DxUiBuildEvidence) {
            param($root,$rules)
            $paths=@(Get-ScopedTrackedPaths $root)
            foreach($rule in $rules) {if(-not (@($paths | Where-Object { Test-ScopedPattern $_ $rule.pattern }).Count)) {$rule.pattern}}
        } $repository $manifest.rules)
        Assert-Scope ($unmatched.Count -eq 0) "Scope rules match no current input: $($unmatched -join ', ')"
    }
    Run-Case 'foreground source changes produce explicit interactive not-run obligations' {
        $rules=@($manifest.interactiveObligations)
        $paths=@(& (Get-Module DxUiBuildEvidence) {
            param($root,$rules)
            $tracked=@(Get-ScopedTrackedPaths $root)
            foreach($rule in $rules) { if(-not (@($tracked | Where-Object { Test-ScopedPattern $_ $rule.pattern }).Count)) {$rule.pattern} }
        } $repository $rules)
        Assert-Scope ($paths.Count -eq 0) "Interactive-obligation rules match no current input: $($paths -join ', ')"
        $menu=@(Get-ScopedInteractiveObligations -Manifest $manifest -ChangedPaths @('Tests/Controls/DxUi.Tests.Menu.cpp'))
        Assert-Scope ($menu.Count -eq 1 -and $menu[0].suite -eq 'Menu') 'Menu source change lost its explicit desktop obligation'
        # The shared paths the interactive suites run through owe them too (CR-30 of the 10 October review).
        foreach ($shared in @(@{ Path='src/Controls/DxUi.WindowHost.cpp'; Suites='Menu,NativeTextInput' }, @{ Path='src/Controls/DxUi.TextStoreACP.cpp'; Suites='NativeTextInput' },
                @{ Path='src/Controls/DxUi.Accessibility.cpp'; Suites='Menu,NativeTextInput' })) {
            $owed=@(Get-ScopedInteractiveObligations -Manifest $manifest -ChangedPaths @($shared.Path) | ForEach-Object suite) -join ','
            Assert-Scope ($owed -eq $shared.Suites) "$($shared.Path) owes $($shared.Suites), not '$owed'"
        }
        $patterns=@($rules | ForEach-Object { "$($_.pattern)|$(@($_.suites) -join ',')" })
        Assert-Scope (@($patterns | Sort-Object -Unique).Count -eq $patterns.Count) 'Interactive-obligation rules repeat a pattern and its suites'
        $full=@(Get-ScopedInteractiveObligations -Manifest $manifest -ChangedPaths @() -Full)
        Assert-Scope (@($full | Where-Object suite -eq 'NativeTextInput').Count -gt 0) 'Full local work hid the separate native text-input gate'
    }
    Run-Case 'revision-4, revision-5 and revision-6 ABI fixtures stay wired into the consumer gate' {
        $fixture=Join-Path $repository 'Tools/ConsumerApi/ConsumerApi.Tests.Revision4.cpp'
        $consumer=Join-Path $repository 'test-consumer.ps1'
        Assert-Scope ((Test-Path -LiteralPath $fixture) -and [IO.File]::ReadAllText($consumer).Contains('Tools/ConsumerApi/ConsumerApi.Tests.Revision4.cpp') -and [IO.File]::ReadAllText($consumer).Contains('ConsumerApi.Tests.Revision4.cpp')) 'Frozen public API fixture is not compiled by the exact-pin consumer gate'
        $fixture5=Join-Path $repository 'Tools/ConsumerApi/ConsumerApi.Tests.Revision5.cpp'
        Assert-Scope ((Test-Path -LiteralPath $fixture5) -and [IO.File]::ReadAllText($consumer).Contains('Tools/ConsumerApi/ConsumerApi.Tests.Revision5.cpp') -and [IO.File]::ReadAllText($consumer).Contains('ConsumerApi.Tests.Revision5.cpp') -and [IO.File]::ReadAllText($consumer).Contains('Samples/Shared/FailureHost.cpp')) 'Revision-5 fixture and application-owned host are not compiled by the consumer gate'
        $fixture6=Join-Path $repository 'Tools/ConsumerApi/ConsumerApi.Tests.Revision6.cpp'
        Assert-Scope ((Test-Path -LiteralPath $fixture6) -and [IO.File]::ReadAllText($consumer).Contains('Tools/ConsumerApi/ConsumerApi.Tests.Revision6.cpp') -and [IO.File]::ReadAllText($consumer).Contains('<ClCompile Include="ConsumerApi.Tests.Revision6.cpp" />')) 'Revision-6 fixture is not compiled by the consumer gate'
        Assert-Scope ([IO.File]::ReadAllText($consumer).Contains('DXUI_NATIVE_MODULE_FATAL executed=16 passed=16') -and [IO.File]::ReadAllText($consumer).Contains('DXUI_NATIVE_MODULE_NATIVE_ALLOWED executed=2 passed=2')) 'Consumer gate must require all independent module fatal and native caught-exception probes'
        Assert-Scope ([IO.File]::ReadAllText($consumer).Contains('Tests/Support/Support.Tests.DumpReader.h')) 'Consumer gate must copy the canonical dump-unwind oracle'
        $sdk=[IO.File]::ReadAllText((Join-Path $repository 'Tools/New-SdkPackage.ps1'))
        foreach($required in @('samples/Shared','FailureHost.cpp','DbgHelp.lib','foreach($unit in $sourceProfile.units)')) {
            Assert-Scope ($sdk.Contains($required)) "SDK package or independent source dependency check is missing $required"
        }
        $sourceProfiles=Import-Module (Join-Path $repository 'Tools/SdkSourceProfiles.psm1') -Force -PassThru
        $currentProfile=& $sourceProfiles {param($Root) Get-DxUiSdkSourceProfile -Root $Root -SourceCommit ('a'*40)} $repository
        Assert-Scope (($currentProfile.units -join '|') -ceq 'Main|FailureHost' -and $currentProfile.shared.Count -eq 2) 'Current API-5 SDK retains both source-owned units and their independent header checks'
        $sdkPayload=[IO.File]::ReadAllText((Join-Path $repository 'Tools/SdkPayload.psm1'))
        Assert-Scope ($sdk.Contains("Import-Module (Join-Path `$toolingRoot 'Tools/SdkNotices.psm1')") -and
            $sdk.Contains("Import-Module (Join-Path `$toolingRoot 'Tools/SdkPayload.psm1')") -and
            $sdk.Contains('New-DxUiSdkPayload -Root $root -Stage $stage -Platform $Platform -SourceCommit $sourceCommit') -and
            $sdkPayload.Contains('Copy-DxUiSdkNotices -Root $root') -and
            [regex]::Matches($sdkPayload,'Assert-DxUiSdkNotices -Root \$root').Count -eq 1 -and
            [regex]::Matches($sdk,'Assert-DxUiSdkNotices -Root \$root').Count -eq 1) 'SDK package must stage and verify the canonical notices before and after extraction'
        $notices=Import-Module (Join-Path $repository 'Tools/SdkNotices.psm1') -Force -PassThru
        $catalog=@(& $notices { Get-NoticeFiles })
        foreach($required in @(
            @{Source='ThirdParty/libfyaml/0.9.4/LICENSE';Name='libfyaml-LICENSE.txt'},
            @{Source='ThirdParty/libfyaml/0.9.4/LICENSE-xxHash.txt';Name='libfyaml-xxHash-LICENSE.txt'})) {
            $catalogMatches=@($catalog | Where-Object { $_.Source -ceq $required.Source -and $_.Name -ceq $required.Name })
            Assert-Scope ($catalogMatches.Count -eq 1 -and (Test-Path -LiteralPath (Join-Path $repository $required.Source) -PathType Leaf)) "SDK notice catalog is missing or duplicates $($required.Source)"
        }
        $headerLoop=[regex]::Match([IO.File]::ReadAllText($consumer),'(?s)\$headerUnits\s*=\s*foreach\s*\(.*?\)\s*\{')
        Assert-Scope ($headerLoop.Success -and $headerLoop.Value.Contains("'RichMath'")) 'Exact-pin consumer must compile RichMath as an independent public header'
    }
    Run-Case 'unmapped executable input widens to every scope' {$plan=Get-ScopedTestPlan $manifest @('Unknown/new.cpp');Assert-Scope $plan.full 'Fallback was narrowed'}
    Run-Case 'native rich grammar geometry and shared fixtures retain accessibility coverage' {
        foreach($path in @('include/DxUi/RichMath.h','include/DxUi/RichSemanticDocument.h','include/DxUi/RichTextExport.h',
                          'src/Controls/DxUi.RichSemanticDocument.cpp','src/Controls/DxUi.RichTextExport.cpp','Tests/Support/Support.Tests.RichTextExport.h',
                          'src/Controls/DxUi.NativeMath.cpp','src/Controls/DxUi.PreparedMath.h','src/Controls/DxUi.NativeFlowchart.cpp',
                          'src/Controls/DxUi.NativeFlowchartLayout.h','src/Controls/DxUi.PreparedFlowchart.cpp','src/Controls/DxUi.SemanticExtensions.h',
                          'src/Controls/DxUi.RichTextScriptRenderer.h','src/Controls/DxUi.RichBidi.h',
                          'Tests/Support/Support.Tests.RichSemanticEditor.h','Tests/Support/Support.Tests.MarkdownBlocks.h',
                          'Tests/Support/Support.Tests.FlowchartLayout.h','Tests/Support/Support.Tests.PreparedFlowchart.h',
                          'Tests/Support/Support.Tests.NativeMathSupportRows.h')) {
            $plan=Get-ScopedTestPlan $manifest @($path)
            Assert-Scope (-not $plan.full) "Rich presentation input unexpectedly fell through to an unmapped full plan: $path"
            foreach($scope in @('EditorControls','Embedded','Accessibility')) {
                Assert-Scope ($scope -in $plan.scopes) "Rich presentation consumer omitted for ${path}: $scope"
            }
        }
    }
    Run-Case 'executable schema changes are never mistaken for prose' {$plan=Get-ScopedTestPlan $manifest @('Specs/Unmapped.schema.json');Assert-Scope $plan.full 'Schema was ignored'}
    Run-Case 'runtime Markdown data is not documentation-only coverage' {
        foreach($path in @('Tests/Data/semantic.md','Tests/Fixtures/README.md','src/Controls/unmapped-fixture.md')) {
            $plan=Get-ScopedTestPlan $manifest @($path)
            Assert-Scope (@($manifest.scopes | Where-Object {$_.native -and $_.name -in $plan.scopes}).Count -gt 0) "Runtime Markdown lost native coverage: $path"
            $before=Get-ScopedSourceIdentity $fixture -CompiledOnly
            Write-Fixture (Join-Path $fixture $path) 'runtime markdown data'
            Assert-Scope ((Get-ScopedSourceIdentity $fixture -CompiledOnly) -cne $before) "Runtime Markdown was absent from compiled identity: $path"
        }
    }
    Run-Case 'prose selects only independent validators, and empty changes select nothing' {
        foreach($path in @('Specs/Testing/explanation.md','docs/example.md','.agents/skills/example/SKILL.md')) {
            $plan=Get-ScopedTestPlan $manifest @($path)
            Assert-Scope ($plan.scopes.Count -gt 0) 'Documentation validation omitted'
            Assert-Scope (@($manifest.scopes | Where-Object {$_.native -and $_.name -in $plan.scopes}).Count -eq 0) 'Prose unnecessarily selected native tests'
        }
        Assert-Scope ((Get-ScopedTestPlan $manifest @()).scopes.Count -eq 0) 'Empty changes selected tests'
    }
    Run-Case 'unmatched non-Markdown docs, measurements and changelog files select validators' {
        foreach($path in @('docs/new-index.json','Measurements/new-receipt.json','Changes/2026-10-08-note.txt')) {
            $plan=Get-ScopedTestPlan $manifest @($path)
            Assert-Scope ('Tooling' -in $plan.scopes) "Validator scope omitted for $path"
            Assert-Scope (@($manifest.scopes | Where-Object {$_.native -and $_.name -in $plan.scopes}).Count -eq 0) "Non-Markdown prose selected native tests for $path"
        }
    }
    Run-Case 'invalid explicit scope and nonrelative paths fail before execution' {
        foreach($action in @({Get-ScopedTestPlan -Manifest $manifest -ChangedPaths @() -Scopes @('Typo')},{Get-ScopedTestPlan -Manifest $manifest -ChangedPaths @('../code.cpp')})) {
            $threw=$false;try{&$action|Out-Null}catch [System.Management.Automation.RuntimeException]{$threw=$true};Assert-Scope $threw 'Invalid selector accepted'
        }
    }
    Run-Case 'selection explains every changed path and accumulates integration consumers' {
        $path='src/Controls/DxUi.Tree.cpp'
        $plan=Get-ScopedTestPlan $manifest @($path)
        Assert-Scope ($plan.scopes.Count -gt 1 -and -not $plan.full) 'No focused integration fan-out'
        Assert-Scope ($plan.reasons[0].path -eq $path) 'Missing selection explanation'
        $grid=Get-ScopedTestPlan $manifest @('src/Controls/DxUi.Grid.cpp')
        foreach($consumer in @('Grid','Tree','Theme','Animation','EditorControls','Tooltip','Embedded','WindowHost')) {Assert-Scope ($consumer -in $grid.scopes) "Grid caller omitted: $consumer"}
    }
    Run-Case 'native runner tooling fixtures select the native suites they execute' {
        foreach($case in @(
            [pscustomobject]@{Path='Tools/tests/Test-TestFilter.ps1';Native='Grid'},
            [pscustomobject]@{Path='Tools/tests/Test-TestWatchdog.ps1';Native='Grid'},
            [pscustomobject]@{Path='Tools/tests/Test-InteractiveLease.ps1';Native='InteractiveLease'}
        )) {
            $plan=Get-ScopedTestPlan $manifest @($case.Path)
            Assert-Scope ('Tooling' -in $plan.scopes) "Tooling omitted for $($case.Path)"
            Assert-Scope ($case.Native -in $plan.scopes) "$($case.Native) omitted for $($case.Path)"
            Assert-Scope (-not $plan.full) "$($case.Path) widened to every scope"
        }
    }
    Run-Case 'ComboBox implementation selects NewControls for TagPicker coverage' {
        $plan=Get-ScopedTestPlan $manifest @('src/Controls/DxUi.ComboBox.cpp')
        Assert-Scope ('ComboBox' -in $plan.scopes) 'Owning ComboBox suite omitted'
        Assert-Scope ('NewControls' -in $plan.scopes) 'TagPicker NewControls suite omitted'
    }
    Run-Case 'shared tooling fixture helpers select every native integration consumer' {
        $plan=Get-ScopedTestPlan $manifest @('Tools/tests/TestSupport.psm1')
        foreach($scope in @('Tooling','Grid','InteractiveLease')) {Assert-Scope ($scope -in $plan.scopes) "Shared fixture consumer omitted: $scope"}
        Assert-Scope (-not $plan.full) 'Shared fixture helper widened to unrelated native suites'
    }
    Run-Case 'only intact identical success can be reused' {
        $path=Join-Path $fixture 'evidence/pass.json';$key=Get-ScopedDigest 'identity'
        Assert-Scope (-not(Test-ScopedReceipt $path $key)) 'Missing receipt reused'
        Write-ScopedReceipt $path $key
        Assert-Scope (Test-ScopedReceipt $path $key) 'Identical success not reusable'
        Assert-Scope (-not(Test-ScopedReceipt $path (Get-ScopedDigest 'other'))) 'Changed identity reused'
        Write-Fixture $path '{broken'
        Assert-Scope (-not(Test-ScopedReceipt $path $key)) 'Corrupt receipt reused'
        Write-ScopedReceipt $path $key
        $content=Get-Content $path -Raw | ConvertFrom-Json;$content.outcome='FAILED';Write-Fixture $path ($content|ConvertTo-Json)
        Assert-Scope (-not(Test-ScopedReceipt $path $key)) 'Failed result reused'
    }
    foreach($profile in @('Debug','Release')) {Write-Fixture (Join-Path $fixture ".build/x64/$profile/suite.exe") 'binary';Write-Fixture (Join-Path $fixture ".build/x64/$profile/Plugins/runtime.dll") 'dependency'}
    Write-FixtureDependencies $fixture
    Run-Case 'selected build union preserves benchmark, sanitizer and desktop helper dependencies' {
        $foundation=@(Get-ScopedRequiredBuildProjects -Suites Foundation -Configuration Release)
        Assert-Scope (($foundation -join ',') -ceq 'DxUi,DxUi.FoundationTests,DxUi.EmbeddedTests') 'Foundation selected unrelated targets or lost its benchmark dependency'
        $control=@(Get-ScopedRequiredBuildProjects -Suites Grid -Configuration Release)
        Assert-Scope (($control -join ',') -ceq 'DxUi,DxUi.ControlTests,DxUi.EmbeddedTests') 'Control target union is incomplete'
        Assert-Scope ('DxUi.InteractiveLease' -cin @(Get-ScopedRequiredBuildProjects -Suites Menu -Configuration Release)) 'Foreground selection lost the native lease'
        Assert-Scope ('DxUi.FoundationTests' -cin @(Get-ScopedRequiredBuildProjects -Suites Grid -Configuration 'ASan Debug')) 'Sanitizer selection lost its real failure probe'
        Assert-Scope (@(Get-ScopedRequiredBuildProjects -Suites Foundation -Configuration Release -Full).Count -eq 6) 'Full became a partial build request'
        $refused=$false;try{Resolve-ScopedBuildProjects @('invented-target')|Out-Null}catch{$refused=$true}
        Assert-Scope $refused 'Unknown native project was admitted'
    }
    Run-Case 'selected build receipts cover actual unions and never grant full solution coverage' {
        $targetRoot=Join-Path $fixture '.build/selected-receipt-fixture'
        foreach($project in (Get-ScopedBuildProjectCatalog).Keys) {
            $names=if($project -ceq 'DxUi'){@('DxUi.lib','DxUi.pdb')}else{@("$project.exe","$project.pdb")}
            foreach($name in $names){Write-Fixture (Join-Path $targetRoot ".build/x64/Release/$name") 'fixture native output'}
        }
        $projects=@(Get-ScopedRequiredBuildProjects -Suites Foundation -Configuration Release)
        Write-ScopedTargetBuildReceipt $targetRoot x64 Release $projects 'inputs' 'artifacts'
        Assert-Scope (Test-ScopedBuildCoverage $targetRoot x64 Release $projects 'inputs' 'artifacts') 'Matching selected proof was refused'
        Assert-Scope (Test-ScopedBuildCoverage $targetRoot x64 Release @('DxUi','DxUi.EmbeddedTests') 'inputs' 'artifacts') 'Selected superset could not satisfy a subset'
        Assert-Scope (-not(Test-ScopedBuildCoverage $targetRoot x64 Release @('DxUi','DxUi.ControlTests','DxUi.EmbeddedTests') 'inputs' 'artifacts')) 'Foundation proof granted unbuilt control coverage'
        Assert-Scope (-not(Test-ScopedBuildCoverage $targetRoot x64 Release $projects 'changed-inputs' 'artifacts')) 'Changed source/dependencies reused a selected build'
        Assert-Scope (-not(Test-ScopedBuildCoverage $targetRoot x64 Release $projects 'inputs' 'changed-artifacts')) 'Changed output bytes reused a selected build'
        $all=@((Get-ScopedBuildProjectCatalog).Keys)
        Assert-Scope (-not(Test-ScopedBuildCoverage $targetRoot x64 Release $all 'inputs' 'artifacts' -Full)) 'A partial proof granted Full coverage'
        $full=Join-Path $targetRoot '.build/reports/scoped-tests/x64-Release/native-build.json'
        Write-ScopedReceipt $full (Get-ScopedDigest ("inputs`nartifacts"))
        Assert-Scope (Test-ScopedBuildCoverage $targetRoot x64 Release $projects 'inputs' 'artifacts') 'Exact full evidence could not satisfy a subset'
        Remove-Item -LiteralPath (Join-Path $targetRoot '.build/x64/Release/DxUi.EmbeddedTests.pdb')
        $refused=$false;try{Test-ScopedBuildCoverage $targetRoot x64 Release $projects 'inputs' 'artifacts'|Out-Null}catch{$refused=$true}
        Assert-Scope $refused 'Missing selected symbols were admitted'
        Remove-ScopedBuildReceipts $targetRoot x64 Release
        Assert-Scope (-not(Test-Path $full) -and @(Get-ChildItem (Join-Path (Split-Path $full) 'build-targets') -Filter '*.json').Count -eq 0) 'Build revocation retained an older selected or full pass'
    }
    Run-Case 'tooling identities include documentation, skills and source-origin mappings' {
        $compiled=Get-ScopedSourceIdentity $fixture -CompiledOnly
        $native=Get-NativeRunIdentity $fixture x64 Debug Example
        foreach($path in @('README.md','.agents/skills/example/SKILL.md','Specs/SourceImport/test-port.json')) {
            $before=Get-ScopedSourceIdentity $fixture
            Write-Fixture (Join-Path $fixture $path) 'changed validator input'
            Assert-Scope ((Get-ScopedSourceIdentity $fixture) -cne $before) "Validator input omitted: $path"
        }
        Assert-Scope ((Get-ScopedSourceIdentity $fixture -CompiledOnly) -ceq $compiled) 'Prose invalidated build attestation'
        Assert-Scope ((Get-NativeRunIdentity $fixture x64 Debug Example) -ceq $native) 'Validator prose unnecessarily invalidated native success'
        Write-Fixture (Join-Path $fixture 'code.cpp') 'new implementation'
        Assert-Scope ((Get-ScopedSourceIdentity $fixture -CompiledOnly) -cne $compiled) 'Source mutation did not invalidate build attestation'
    }
    Run-Case 'UTF-8 Git paths remain in full and compiled source identities' {
        $path=Join-Path $fixture 'Tests/Data/Größe.txt'
        Write-Fixture $path 'first revision'
        Invoke-FixtureGit @('add','--','Tests/Data/Größe.txt')
        $fullBefore=Get-ScopedSourceIdentity $fixture
        $compiledBefore=Get-ScopedSourceIdentity $fixture -CompiledOnly
        Write-Fixture $path 'second revision'
        Assert-Scope ((Get-ScopedSourceIdentity $fixture) -cne $fullBefore) 'UTF-8 validator input did not invalidate identity'
        Assert-Scope ((Get-ScopedSourceIdentity $fixture -CompiledOnly) -cne $compiledBefore) 'UTF-8 compiled input did not invalidate identity'
    }
    Run-Case 'deleted source identity is stable before staging, after staging, and after commit' {
        $deletedFixture=Join-Path $fixture 'deleted-source-fixture'
        [void](New-Item -ItemType Directory -Path $deletedFixture -Force)
        & git -C $deletedFixture init -q
        & git -C $deletedFixture config user.name 'Scoped testing fixture'
        & git -C $deletedFixture config user.email 'fixture@example.invalid'
        [IO.File]::WriteAllText((Join-Path $deletedFixture 'removed.cpp'),'initial',[Text.UTF8Encoding]::new($false))
        & git -C $deletedFixture add -A
        & git -C $deletedFixture -c commit.gpgsign=false commit -q -m 'fixture baseline'
        $beforeDeletion=Get-ScopedSourceIdentity $deletedFixture
        Remove-Item -LiteralPath (Join-Path $deletedFixture 'removed.cpp')
        $unstagedDeletion=Get-ScopedSourceIdentity $deletedFixture
        & git -C $deletedFixture add -u -- removed.cpp
        $stagedDeletion=Get-ScopedSourceIdentity $deletedFixture
        & git -C $deletedFixture -c commit.gpgsign=false commit -q -m 'fixture deletion'
        $committedDeletion=Get-ScopedSourceIdentity $deletedFixture
        Assert-Scope ($unstagedDeletion -ceq $stagedDeletion) 'Staging a deletion changed working-tree content identity'
        Assert-Scope ($stagedDeletion -ceq $committedDeletion) 'Committing a deletion changed working-tree content identity'
        Assert-Scope ($beforeDeletion -cne $committedDeletion) 'Deletion did not change source content identity'
    }
    Run-Case 'uncommon executable input extensions invalidate build attestation' {
        $before=Get-ScopedSourceIdentity $fixture -CompiledOnly
        Write-Fixture (Join-Path $fixture 'include/Fixture.inl') 'changed inline implementation'
        Assert-Scope ((Get-ScopedSourceIdentity $fixture -CompiledOnly) -cne $before) 'Inline implementation was absent from build identity'
    }
    Run-Case 'runtime DLL mutation invalidates even with unchanged executable' {
        $before=Get-ScopedArtifactIdentity $fixture x64 Debug
        Write-Fixture (Join-Path $fixture '.build/x64/Debug/Plugins/runtime.dll') 'newdependency'
        Assert-Scope ((Get-ScopedArtifactIdentity $fixture x64 Debug) -cne $before) 'DLL mutation reused'
    }
    Run-Case 'configuration, scope and sanitizer environment differ' {
        $before=Get-NativeRunIdentity $fixture x64 Debug Example
        Assert-Scope ((Get-NativeRunIdentity $fixture x64 Release Example) -cne $before) 'Configuration ignored'
        Assert-Scope ((Get-NativeRunIdentity $fixture x64 Debug Other) -cne $before) 'Scope ignored'
        $options=Get-EnvironmentSnapshot @('ASAN_OPTIONS')
        $environmentBefore=Get-ScopedEnvironmentIdentity
        try {Set-Item Env:ASAN_OPTIONS -Value 'halt_on_error=1:fixture=changed';Assert-Scope ((Get-NativeRunIdentity $fixture x64 Debug Example) -cne $before) 'Environment ignored'}
        finally {Restore-EnvironmentSnapshot $options}
        Assert-EnvironmentSnapshot $options 'ASAN_OPTIONS fixture'
        Assert-Scope ((Get-ScopedEnvironmentIdentity) -ceq $environmentBefore) 'ASAN_OPTIONS fixture changed the caller environment identity'
    }
    Run-Case 'runtime instrumentation changes identity and cannot be misclassified as ordinary work' {
        $names=@('DXUI_GRAPH_PERF','DXUI_PERF_JSONL_PATH','DXUI_MUTANT','DXUI_ONLY_FRENCH')
        $old=Get-EnvironmentSnapshot $names
        $environmentBefore=Get-ScopedEnvironmentIdentity
        try {
            foreach($name in $names){Remove-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue}
            $ordinary=Get-ScopedEnvironmentIdentity
            foreach($pair in @(@('DXUI_GRAPH_PERF','1'),@('DXUI_PERF_JSONL_PATH','C:\private\perf.jsonl'),@('DXUI_MUTANT','SetRoot'),@('DXUI_ONLY_FRENCH','1'))){
                Set-Item -LiteralPath "Env:$($pair[0])" -Value $pair[1]
                Assert-Scope ((Get-ScopedEnvironmentIdentity) -cne $ordinary) "$($pair[0]) changes environment identity"
                Assert-Scope ($pair[0] -in (Get-ScopedInstrumentation)) "$($pair[0]) is marked instrumented"
                Remove-Item -LiteralPath "Env:$($pair[0])" -ErrorAction SilentlyContinue
            }
        } finally {Restore-EnvironmentSnapshot $old}
        Assert-EnvironmentSnapshot $old 'Instrumentation fixture'
        Assert-Scope ((Get-ScopedEnvironmentIdentity) -ceq $environmentBefore) 'Instrumentation fixture changed the caller environment identity'
    }
    Run-Case 'skip-build attestation rejects changed source and executable bytes' {
        $path=Join-Path $fixture 'evidence/build.json'
        $buildInput=Get-ScopedBuildInputIdentity $fixture x64;$binary=Get-ScopedArtifactIdentity $fixture x64 Debug
        Write-ScopedReceipt $path (Get-ScopedDigest ($buildInput+"`n"+$binary))
        Write-Fixture (Join-Path $fixture '.build/x64/Debug/suite.exe') 'differentbinary'
        Assert-Scope (-not(Test-ScopedReceipt $path (Get-ScopedDigest ($buildInput+"`n"+(Get-ScopedArtifactIdentity $fixture x64 Debug))))) 'Stale executable accepted'
        Write-Fixture (Join-Path $fixture 'code.cpp') 'another implementation'
        Assert-Scope (-not(Test-ScopedReceipt $path (Get-ScopedDigest ((Get-ScopedBuildInputIdentity $fixture x64)+"`n"+$binary)))) 'Stale source accepted'
    }
    Run-Case 'library-only mutation invalidates build and run attestation while executable bytes stay fixed' {
        $buildReceipt=Join-Path $fixture '.build/reports/scoped-tests/x64-Debug/native-build.json'
        $runReceipt=Join-Path $fixture '.build/reports/scoped-tests/x64-Debug/Example.json'
        $exe=Join-Path $fixture '.build/x64/Debug/suite.exe'
        $library=Join-Path $fixture '.build/x64/Debug/DxUi.lib'
        Write-Fixture $exe 'unchanged executable'
        Write-Fixture $library 'library before'
        $buildInput=Get-ScopedBuildInputIdentity $fixture x64
        $artifact=Get-ScopedArtifactIdentity $fixture x64 Debug
        $runIdentity=Get-NativeRunIdentity $fixture x64 Debug Example
        $exeBefore=Get-FileHash -LiteralPath $exe -Algorithm SHA256
        Write-ScopedReceipt $buildReceipt (Get-ScopedDigest ($buildInput+"`n"+$artifact))
        Write-ScopedReceipt $runReceipt $runIdentity

        Write-Fixture $library 'library after'

        $exeAfter=Get-FileHash -LiteralPath $exe -Algorithm SHA256
        Assert-Scope ($exeBefore.Hash -ceq $exeAfter.Hash) 'Library mutation changed executable bytes'
        Assert-Scope ((Get-ScopedArtifactIdentity $fixture x64 Debug) -cne $artifact) 'Static library was absent from artifact identity'
        $currentBuildIdentity=Get-ScopedDigest ($buildInput+"`n"+(Get-ScopedArtifactIdentity $fixture x64 Debug))
        Assert-Scope (-not(Test-ScopedReceipt $buildReceipt $currentBuildIdentity)) 'Stale build attestation accepted a changed static library'
        Assert-Scope (-not(Test-ScopedReceipt $runReceipt (Get-NativeRunIdentity $fixture x64 Debug Example))) 'Run receipt reused after static library mutation'
    }
    Run-Case 'installed dependency bytes bind build and run identities while output artifacts stay fixed' {
        $buildReceipt=Join-Path $fixture '.build/reports/scoped-tests/x64-Debug/native-build.json'
        $runReceipt=Join-Path $fixture '.build/reports/scoped-tests/x64-Debug/Example.json'
        $exe=Join-Path $fixture '.build/x64/Debug/suite.exe'
        $artifact=Get-ScopedArtifactIdentity $fixture x64 Debug
        $buildInput=Get-ScopedBuildInputIdentity $fixture x64
        $dependency=Get-ScopedDependencyIdentity $fixture x64
        $runIdentity=Get-NativeRunIdentity $fixture x64 Debug Example
        $exeBefore=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
        Assert-Scope ($buildInput -ceq (Get-ScopedBuildInputIdentity $fixture x64) -and $dependency -ceq (Get-ScopedDependencyIdentity $fixture x64)) 'Unchanged installed dependency bytes produced an unstable identity'
        Write-ScopedReceipt $buildReceipt (Get-ScopedDigest ($buildInput+"`n"+$artifact))
        Write-ScopedReceipt $runReceipt $runIdentity

        foreach($path in @(
            '.build/vcpkg_installed/x64/x64-windows/include/wil/resource.h',
            '.build/vcpkg_installed/x64/x64-windows/lib/wil.lib',
            '.build/vcpkg_installed/x64/x64-windows/share/wil/vcpkg_abi_info.txt',
            '.build/vcpkg_installed/x64/vcpkg/status',
            '.build/vcpkg_installed/x64/vcpkg/info/wil_1.0_x64-windows.list')) {
            $old=Get-Content -LiteralPath (Join-Path $fixture $path) -Raw
            try {
                Write-Fixture (Join-Path $fixture $path) ($old+"changed`n")
                $changedInput=Get-ScopedBuildInputIdentity $fixture x64
                Assert-Scope ($changedInput -cne $buildInput) "Installed dependency mutation did not change build input identity: $path"
                Assert-Scope ((Get-ScopedArtifactIdentity $fixture x64 Debug) -ceq $artifact) "Dependency mutation changed output artifact identity: $path"
                Assert-Scope (-not(Test-ScopedReceipt $buildReceipt (Get-ScopedDigest ($changedInput+"`n"+$artifact)))) "Stale SkipBuild attestation accepted dependency mutation: $path"
                Assert-Scope (-not(Test-ScopedReceipt $runReceipt (Get-NativeRunIdentity $fixture x64 Debug Example))) "Stale native run receipt accepted dependency mutation: $path"
            } finally { Write-Fixture (Join-Path $fixture $path) $old }
            Assert-Scope ((Get-ScopedBuildInputIdentity $fixture x64) -ceq $buildInput) "Dependency fixture did not restore its original bytes: $path"
        }
        Assert-Scope ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ceq $exeBefore) 'Dependency mutations changed the executable fixture'
    }
    Run-Case 'dependency identity fails closed when required vcpkg metadata is absent' {
        foreach($path in @(
            '.build/vcpkg_installed/x64/x64-windows/include/wil/resource.h',
            '.build/vcpkg_installed/x64/x64-windows/share/wil/vcpkg_abi_info.txt',
            '.build/vcpkg_installed/x64/vcpkg/status',
            '.build/vcpkg_installed/x64/vcpkg/info/wil_1.0_x64-windows.list')) {
            $full=Join-Path $fixture $path
            $saved=Get-Content -LiteralPath $full -Raw
            Remove-Item -LiteralPath $full
            $threw=$false
            try { [void](Get-ScopedDependencyIdentity $fixture x64) } catch [System.Management.Automation.RuntimeException] { $threw=$true }
            finally { Write-Fixture $full $saved }
            Assert-Scope $threw "Missing installed dependency input yielded a reusable identity: $path"
        }
    }
    Run-Case 'dependency identities isolate the selected platform' {
        Write-FixtureDependencies $fixture ARM64
        $x64Before=Get-ScopedDependencyIdentity $fixture x64
        $armBefore=Get-ScopedDependencyIdentity $fixture ARM64
        $armHeader=Join-Path $fixture '.build/vcpkg_installed/ARM64/arm64-windows/include/wil/resource.h'
        Write-Fixture $armHeader 'changed ARM64 header only'
        Assert-Scope ((Get-ScopedDependencyIdentity $fixture ARM64) -cne $armBefore) 'ARM64 dependency mutation did not change its identity'
        Assert-Scope ((Get-ScopedDependencyIdentity $fixture x64) -ceq $x64Before) 'ARM64-only mutation invalidated x64 dependency evidence'
    }
    Run-Case 'reviewed main adapter retains exact native and ABI gates' {
        $engine=Join-Path $repository '.ci-engine'
        Import-Module (Join-Path $engine 'Tools/Modules/Ci.Project.psm1')
        Import-Module (Join-Path $engine 'Tools/Modules/Ci.Policy.psm1')
        $project=Read-CiProject $repository
        Test-CiProjectConfiguration @{root=$repository;engineRoot=$engine;project=$project;development=$false}
        $adapter=[IO.File]::ReadAllText((Join-Path $repository 'Tools/Invoke-ProjectCi.ps1'))
        Assert-Scope ($adapter.Contains("Run 'test.ps1'") -and $adapter.Contains("'-Full','-SkipTooling','-SkipBuild'")) 'The normal batch lost Full native execution'
        Assert-Scope ($adapter.Contains('Invoke-ToolingTests.ps1')) 'Compiler-dependent tooling lost its main owner'
        Assert-Scope ($adapter.Contains('Get-CiSuiteEvidence')) 'Native suite execution must produce canonical fresh evidence'
        foreach($retained in @("Run 'test-consumer.ps1'","'-DisableStlAnnotations'","MenuResourceScaling,MenuTextLayoutResources","Run 'gallery.ps1'")){
            Assert-Scope ($adapter.Contains($retained)) "The main/manual adapter lost $retained"
        }
        $errors=$null;$tokens=$null;$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $repository 'test.ps1'),[ref]$tokens,[ref]$errors)
        Assert-Scope (@($errors).Count -eq 0) 'Native entrypoint has parse errors'
        $defaults=($ast.ParamBlock.Parameters|Where-Object {$_.Name.VariablePath.UserPath -eq 'Suites'}).DefaultValue.Extent.Text
        foreach($scope in $manifest.scopes){Assert-Scope ($defaults.Contains("'"+$scope.name+"'") -or -not $scope.native) "Normal native defaults miss $($scope.name)"}
    }
    Run-Case 'native preflight reuse binds runner, native sources, artifacts, environment and expiry' {
        $preflightRoot=Join-Path $fixture 'preflight'
        Write-Fixture (Join-Path $preflightRoot '.gitignore') '.build/'
        Write-Fixture (Join-Path $preflightRoot 'Tests/Harness.Tests.Watchdog.cpp') 'native watchdog v1'
        foreach($name in @('Test-TestFilter.ps1','Test-TestWatchdog.ps1')) {
            Write-Fixture (Join-Path $preflightRoot "Tools/tests/$name") @'
param($Platform,$Configuration)
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
[IO.File]::AppendAllText((Join-Path $root '.build/preflight-calls.txt'),"$($MyInvocation.MyCommand.Name)`n")
if(Test-Path (Join-Path $root '.build/fail')){exit 7}
if(Test-Path (Join-Path $root '.build/mutate')){[IO.File]::AppendAllText((Join-Path $root 'Tests/Harness.Tests.Watchdog.cpp'),'changed')}
exit 0
'@
        }
        & git -C $preflightRoot init -q
        if($LASTEXITCODE){throw 'Preflight fixture git init failed'}
        Write-FixtureDependencies $preflightRoot x64
        Write-Fixture (Join-Path $preflightRoot '.build/x64/Debug/DxUi.ControlTests.exe') 'fixture control binary'
        $environment=Get-EnvironmentSnapshot @('CI','GITHUB_ACTIONS','DXUI_MUTANT')
        try {
            foreach($name in @('CI','GITHUB_ACTIONS','DXUI_MUTANT')) {Remove-Item "Env:$name" -ErrorAction SilentlyContinue}
            function Invoke-Preflight([switch]$Force) {Invoke-ScopedHarnessPreflight -Root $preflightRoot -Platform x64 -Configuration Debug -Force:$Force}
            function Assert-PreflightCalls([int]$Count) {Assert-Scope (@(Get-Content (Join-Path $preflightRoot '.build/preflight-calls.txt')).Count -eq $Count) "Expected $Count preflight child invocations"}
            $receipt=Join-Path $preflightRoot '.build/reports/scoped-tests/x64-Debug/harness.json'
            Invoke-Preflight; Invoke-Preflight; Assert-PreflightCalls 2
            # Changed native harness outside Tools invalidates even with preserved size/time.
            $source=Get-Item (Join-Path $preflightRoot 'Tests/Harness.Tests.Watchdog.cpp')
            $stamp=$source.LastWriteTimeUtc
            [IO.File]::WriteAllText($source.FullName,'native watchdog v2');$source.LastWriteTimeUtc=$stamp
            Invoke-Preflight; Assert-PreflightCalls 4
            Write-Fixture (Join-Path $preflightRoot '.build/x64/Debug/DxUi.ControlTests.exe') 'changed fixture control binary'
            Invoke-Preflight; Assert-PreflightCalls 6
            Write-Fixture (Join-Path $preflightRoot 'Tools/tests/new-detector.ps1') '# changed detector closure'
            Invoke-Preflight; Assert-PreflightCalls 8
            Set-Item Env:CI true
            Invoke-Preflight; Invoke-Preflight; Assert-PreflightCalls 12
            Remove-Item Env:CI
            # A validly digested old receipt still cannot waive current verification.
            $stored=Get-Content $receipt -Raw|ConvertFrom-Json
            $stored.completedUtc=[DateTime]::UtcNow.AddHours(-25).ToString('o')
            $stored.digest=Get-ScopedDigest ($stored.identity+"`n"+$stored.completedUtc+"`nPASSED")
            $stored|ConvertTo-Json|Set-Content $receipt
            Invoke-Preflight; Assert-PreflightCalls 14
            Write-Fixture (Join-Path $preflightRoot '.build/fail') 'fail'
            $refused=$false;try{Invoke-Preflight -Force}catch{$refused=$true}
            Assert-Scope ($refused -and -not(Test-Path $receipt)) 'Failure retained reusable preflight success'
            Remove-Item (Join-Path $preflightRoot '.build/fail')
            Write-Fixture (Join-Path $preflightRoot '.build/mutate') 'mutate'
            $refused=$false;try{Invoke-Preflight -Force}catch{$refused=$true}
            Assert-Scope ($refused -and -not(Test-Path $receipt)) 'Mutation during verification retained success'
            Remove-Item (Join-Path $preflightRoot '.build/mutate')
            Set-Item Env:DXUI_MUTANT fixture
            Invoke-Preflight
            Assert-Scope (-not(Test-Path $receipt)) 'Instrumented preflight created qualification evidence'
        } finally {Restore-EnvironmentSnapshot $environment}
    }
    Run-Case 'actual hosted Release invocation selects two ordinary workers and retains the exact Full profile obligations' {
        $path=Join-Path $repository 'Tools/Invoke-ProjectCi.ps1'
        $tokens=$null;$errors=$null
        $text=[IO.File]::ReadAllText($path)
        $ast=[Management.Automation.Language.Parser]::ParseInput($text,[ref]$tokens,[ref]$errors)
        Assert-Scope ($errors.Count -eq 0) 'Hosted adapter no longer parses'
        $start=@($ast.EndBlock.Statements|Where-Object {$_ -is [Management.Automation.Language.AssignmentStatementAst] -and $_.Left.Extent.Text -ceq '$normalTestArguments'})
        $end=@($ast.EndBlock.Statements|Where-Object {$_.Extent.Text -ceq 'Run ''test.ps1'' $normalTestArguments'})
        Assert-Scope ($start.Count -eq 1 -and $end.Count -eq 1) 'Hosted normal test route is not uniquely identified'
        $fragment=$text.Substring($start[0].Extent.StartOffset,$end[0].Extent.EndOffset-$start[0].Extent.StartOffset)
        foreach($configuration in @('Release','Debug','ASan Debug')){
            foreach($platform in @('x64','ARM64')){
                $actual=@(& {
                    param($body,$configuration,$platform)
                    $profile=@('-Platform',$platform,'-Configuration',$configuration)
                    function Run([string]$Script,[string[]]$CommandArguments){[pscustomobject]@{script=$Script;arguments=$CommandArguments}}
                    & ([scriptblock]::Create($body))
                } $fragment $configuration $platform)
                $expected=@('-Platform',$platform,'-Configuration',$configuration,'-Full','-SkipTooling','-SkipBuild')
                if($configuration -ceq 'Release'){$expected+=@('-ParallelSuites','2')}
                Assert-Scope ($actual.Count -eq 1 -and $actual[0].script -ceq 'test.ps1' -and
                    ($actual[0].arguments -join [char]0) -ceq ($expected -join [char]0)) 'Actual native adapter changed the declared profile/coverage or unsupported parallel default'
            }
        }
    }
    Write-Host "Scoped testing: $passed cases passed."
} finally {
    $resolved=[IO.Path]::GetFullPath($fixture);$allowed=[IO.Path]::GetFullPath((Join-Path $repository '.build/ToolTests'))+[IO.Path]::DirectorySeparatorChar
    if(-not $resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Fixture cleanup escaped its owned root.'}
    if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
exit 0
