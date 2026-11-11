# Real Git/content/binary fixtures; does not build or take desktop focus.
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repository=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $repository 'Tools/ScopedTesting.psm1') -Force
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
    Run-Case 'actual ignored engine bytes bind build and tooling evidence without changing iteration maps' {
        $product=$fixture+'-engine-input-project'
        [void][IO.Directory]::CreateDirectory($product)
        $initialized=& git -C $product init --quiet 2>&1
        if($LASTEXITCODE){throw "Owned product fixture initialization failed: $initialized"}
        Write-Fixture (Join-Path $product '.gitignore') ".build/`n.ci-engine/`n"
        Write-Fixture (Join-Path $product '.ci/project.json') ([IO.File]::ReadAllText((Join-Path $repository '.ci/project.json')))
        Write-Fixture (Join-Path $product 'runtime.cpp') 'synthetic source'
        $engine=Join-Path $product '.ci-engine'
        $cloneOutput=& git clone --quiet --no-hardlinks (Join-Path $repository '.ci-engine') $engine 2>&1
        if($LASTEXITCODE){throw "Owned engine fixture clone failed: $cloneOutput"}
        $mapBefore=Get-ScopedMapIdentity (Get-ScopedSourceMap $product)
        $toolingBefore=Get-ScopedSourceIdentity $product
        $buildBefore=Get-ScopedSourceIdentity $product -CompiledOnly
        Assert-Scope ((Get-ScopedSourceIdentity $product.ToUpperInvariant()) -ceq $toolingBefore) 'Windows path casing invalidated tooling evidence'
        Assert-Scope ((Get-ScopedSourceIdentity $product.ToUpperInvariant() -CompiledOnly) -ceq $buildBefore) 'Windows path casing invalidated build evidence'
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
            Assert-Scope ((Get-ScopedMapIdentity (Get-ScopedSourceMap $product)) -ceq $mapBefore) 'Engine identity polluted product iteration maps'
        }finally{[IO.File]::WriteAllBytes($policy,$original)}
        Assert-Scope ((Get-ScopedSourceIdentity $product) -ceq $toolingBefore) 'Restored exact engine did not recover its tooling identity'
        Assert-Scope ((Get-ScopedSourceIdentity $product -CompiledOnly) -ceq $buildBefore) 'Restored exact engine did not recover its build identity'

    }
    Invoke-FixtureGit @('init','-q')
    Invoke-FixtureGit @('config','user.name','Scoped testing fixture')
    Invoke-FixtureGit @('config','user.email','fixture@example.invalid')
    foreach($file in @('code.cpp','staged.cpp','deleted.cpp','renamed.cpp')) {Write-Fixture (Join-Path $fixture $file) 'initial'}
    Invoke-FixtureGit @('add','-A');Invoke-FixtureGit @('commit','-q','-m','fixture baseline')
    $baseline=(& git -C $fixture rev-parse HEAD).Trim()
    Write-Fixture (Join-Path $fixture 'committed.cpp') 'committed'
    Invoke-FixtureGit @('add','-A');Invoke-FixtureGit @('commit','-q','-m','fixture candidate')
    Write-Fixture (Join-Path $fixture 'staged.cpp') 'staged'
    Invoke-FixtureGit @('add','staged.cpp')
    # A working copy matching HEAD must not hide a different staged blob.
    Write-Fixture (Join-Path $fixture 'staged.cpp') 'initial'
    Write-Fixture (Join-Path $fixture 'code.cpp') 'working'
    Remove-Item -LiteralPath (Join-Path $fixture 'deleted.cpp')
    Move-Item -LiteralPath (Join-Path $fixture 'renamed.cpp') -Destination (Join-Path $fixture 'renamed-new.cpp')
    Write-Fixture (Join-Path $fixture 'untracked file.cpp') 'untracked'
    $manifest=Read-ScopedTestManifest $repository
    Run-Case 'all active native tests satisfy naming and inventory' {Assert-Scope ((Assert-ScopedTestNames $repository) -gt 0) 'Empty native inventory'}
    Run-Case 'batched scope patterns retain scalar, separator, Unicode and case behavior' {
        $matches=@(& (Get-Module ScopedTesting) {
            Test-ScopedPattern 'src/Controls/Editor.cpp' 'src/Controls/*.cpp'
            Test-ScopedPattern 'src/Controls/Nested/Editor.cpp' 'src/Controls/*.cpp'
            Test-ScopedPattern 'src/Controls/Nested/Editor.cpp' 'src/**/Editor.cpp'
            Test-ScopedPattern @('unrelated.cpp','SRC/Controls/É.cpp') 'src/Controls/?.cpp'
            Test-ScopedPattern @('unrelated.cpp','src/Other/Editor.cpp') 'src/Controls/*.cpp'
            Test-ScopedPattern @() '**'
        })
        Assert-Scope (($matches -join ',') -ceq 'True,False,True,True,False,False') 'Batched glob matching changed scope semantics'
    }
    Run-Case 'every scope rule matches a current repository input' {
        $unmatched=@(& (Get-Module ScopedTesting) {
            param($root,$rules)
            $paths=@(Get-ScopedTrackedPaths $root)
            foreach($rule in $rules) {if(-not (Test-ScopedPattern $paths $rule.pattern)) {$rule.pattern}}
        } $repository $manifest.rules)
        Assert-Scope ($unmatched.Count -eq 0) "Scope rules match no current input: $($unmatched -join ', ')"
    }
    Run-Case 'foreground source changes produce explicit interactive not-run obligations' {
        if ($manifest.repository -like '*/DxUi') {
            $rules=@($manifest.interactiveObligations)
            $paths=@(& (Get-Module ScopedTesting) {
                param($root,$rules)
                $tracked=@(Get-ScopedTrackedPaths $root)
                foreach($rule in $rules) { if(-not (Test-ScopedPattern $tracked $rule.pattern)) {$rule.pattern} }
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
    }
    Run-Case 'one PrePush entry point accounts for six profiles and revision-4 and revision-5 fixtures are wired into consumers' {
        if ($manifest.repository -like '*/DxUi') {
            $prePush=Join-Path $repository 'Test-PrePush.ps1'
            $tokens=$null;$errors=$null
            $ast=[Management.Automation.Language.Parser]::ParseFile($prePush,[ref]$tokens,[ref]$errors)
            Assert-Scope (@($errors).Count -eq 0) 'PrePush accounting script has parse errors'
            $profiles=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.HashtableAst]},$true))
            Assert-Scope ($profiles.Count -eq 6) 'PrePush does not enumerate all six platform/configuration profiles'
            $fixture=Join-Path $repository 'Tools/ConsumerApi/ConsumerApi.Tests.Revision4.cpp'
            $consumer=Join-Path $repository 'test-consumer.ps1'
            Assert-Scope ((Test-Path -LiteralPath $fixture) -and [IO.File]::ReadAllText($consumer).Contains('Tools/ConsumerApi/ConsumerApi.Tests.Revision4.cpp') -and [IO.File]::ReadAllText($consumer).Contains('ConsumerApi.Tests.Revision4.cpp')) 'Frozen public API fixture is not compiled by the exact-pin consumer gate'
            $fixture5=Join-Path $repository 'Tools/ConsumerApi/ConsumerApi.Tests.Revision5.cpp'
            Assert-Scope ((Test-Path -LiteralPath $fixture5) -and [IO.File]::ReadAllText($consumer).Contains('Tools/ConsumerApi/ConsumerApi.Tests.Revision5.cpp') -and [IO.File]::ReadAllText($consumer).Contains('ConsumerApi.Tests.Revision5.cpp') -and [IO.File]::ReadAllText($consumer).Contains('Samples/Shared/FailureHost.cpp')) 'Revision-5 fixture and application-owned host are not compiled by the consumer gate'
            Assert-Scope ([IO.File]::ReadAllText($consumer).Contains('DXUI_NATIVE_MODULE_FATAL executed=16 passed=16') -and [IO.File]::ReadAllText($consumer).Contains('DXUI_NATIVE_MODULE_NATIVE_ALLOWED executed=2 passed=2')) 'Consumer gate must require all independent module fatal and native caught-exception probes'
            Assert-Scope ([IO.File]::ReadAllText($consumer).Contains('Tests/Support/Support.Tests.DumpReader.h')) 'Consumer gate must copy the canonical dump-unwind oracle'
            $sdk=[IO.File]::ReadAllText((Join-Path $repository 'Tools/New-SdkPackage.ps1'))
            foreach($required in @('samples/Shared','FailureHost.cpp','DbgHelp.lib','foreach($unit in $sourceProfile.units)')) {
                Assert-Scope ($sdk.Contains($required)) "SDK package or independent source dependency check is missing $required"
            }
            $sourceProfiles=Import-Module (Join-Path $repository 'Tools/SdkSourceProfiles.psm1') -Force -PassThru
            $currentProfile=& $sourceProfiles {param($Root) Get-DxUiSdkSourceProfile -Root $Root -SourceCommit ('a'*40)} $repository
            Assert-Scope (($currentProfile.units -join '|') -ceq 'Main|FailureHost' -and $currentProfile.shared.Count -eq 2) 'Current API-5 SDK retains both source-owned units and their independent header checks'
            Assert-Scope ($sdk.Contains("Import-Module (Join-Path `$toolingRoot 'Tools/SdkNotices.psm1')") -and
                $sdk.Contains('Copy-DxUiSdkNotices -Root $root') -and
                [regex]::Matches($sdk,'Assert-DxUiSdkNotices -Root \$root').Count -eq 2) 'SDK package must stage and verify the canonical notices before and after extraction'
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
    }
    Run-Case 'active names require exact Tests spelling and reject historical inventory membership' {
        $naming=Join-Path $fixture 'naming'
        foreach($invalid in @('Tests/Scope.tests.Case.cpp','Tests/Scope.Tests.Case.cpp','Specs/TestRuns/Scope.Tests.Case.cpp')) {
            Write-Fixture (Join-Path $naming $invalid) 'invalid declared source'
            Write-Fixture (Join-Path $naming 'Tests/native-test-files.json') (ConvertTo-Json -InputObject @($invalid) -Compress)
            $message=''
            try {Assert-ScopedTestNames $naming | Out-Null} catch [System.Management.Automation.RuntimeException] {$message=$_.Exception.Message}
            Assert-Scope ($message -match 'Invalid/missing native test source:|Historical/external source cannot enter|Use a component context instead of placeholder Scope') "Invalid inventory accepted: $invalid ($message)"
        }
        $casePaths=@([IO.Directory]::GetFiles((Join-Path $naming 'Tests'),'Scope.*.Case.cpp') | ForEach-Object { [IO.Path]::GetFileName($_) })
        if($casePaths.Count -eq 2) {
            $tracked=@(& (Get-Module ScopedTesting) {param($root) Get-ScopedTrackedPaths $root} $fixture)
            foreach($name in @('Scope.Tests.Case.cpp','Scope.tests.Case.cpp')) {
                Assert-Scope ($tracked -ccontains "naming/Tests/$name") "Distinct Git path was lost during enumeration: naming/Tests/$name"
            }
        }
    }
    Run-Case 'Git path normalization is ordinal exact and preserves rename sides and Unicode' {
        $inputPaths=@('Tests/Scope.tests.Case.cpp','Tests/Scope.Tests.Case.cpp','Tests/Scope.Tests.Case.cpp',
            'renamed.cpp','renamed-new.cpp','Tests/Data/Größe.txt','Tests/Data/GRÖSSE.txt','Tests/Data/Größe.txt')
        $paths=@(& (Get-Module ScopedTesting) {param([string[]]$items) Get-ScopedOrdinalUniquePaths $items} $inputPaths)
        Assert-Scope ($paths.Count -eq 6) "Ordinal path normalization returned $($paths.Count) rows instead of six: $($paths -join ', ')"
        foreach($path in @('Tests/Scope.Tests.Case.cpp','Tests/Scope.tests.Case.cpp','renamed.cpp','renamed-new.cpp','Tests/Data/Größe.txt','Tests/Data/GRÖSSE.txt')) {
            Assert-Scope ($paths -ccontains $path) "Ordinal path normalization omitted '$path'"
        }
        for($index=1;$index -lt $paths.Count;$index++) {
            Assert-Scope ([StringComparer]::Ordinal.Compare($paths[$index-1],$paths[$index]) -lt 0) "Path rows are not strictly ordinal-sorted at $($paths[$index-1]) / $($paths[$index])"
        }
    }
    Run-Case 'committed, staged, working, deleted, both rename sides and spaced untracked paths count' {
        $paths=@(Get-ScopedChangedPaths $fixture $baseline)
        foreach($path in @('committed.cpp','staged.cpp','code.cpp','deleted.cpp','renamed.cpp','renamed-new.cpp','untracked file.cpp')) {Assert-Scope ($path -in $paths) "Missing impact: $path"}
    }
    Run-Case 'unmapped executable input widens to every scope' {$plan=Get-ScopedTestPlan $manifest @('Unknown/new.cpp');Assert-Scope $plan.full 'Fallback was narrowed'}
    if($manifest.repository -like '*/DxUi') {
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
        foreach($path in @('Specs/Testing/explanation.md','Plugins/Weather/README.md','.agents/skills/example/SKILL.md')) {
            $plan=Get-ScopedTestPlan $manifest @($path)
            Assert-Scope ($plan.scopes.Count -gt 0) 'Documentation validation omitted'
            Assert-Scope (@($manifest.scopes | Where-Object {$_.native -and $_.name -in $plan.scopes}).Count -eq 0) 'Prose unnecessarily selected native tests'
        }
        Assert-Scope ((Get-ScopedTestPlan $manifest @()).scopes.Count -eq 0) 'Empty changes selected tests'
    }
    Run-Case 'unmatched non-Markdown docs, measurement and change files select validators' {
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
        $path=if($manifest.repository -like '*/DxUi'){'src/Controls/DxUi.Tree.cpp'}else{'Plugins/Weather/Weather.cpp'}
        $plan=Get-ScopedTestPlan $manifest @($path)
        Assert-Scope ($plan.scopes.Count -gt 1 -and -not $plan.full) 'No focused integration fan-out'
        Assert-Scope ($plan.reasons[0].path -eq $path) 'Missing selection explanation'
        if($manifest.repository -like '*/DxUi') {
            $grid=Get-ScopedTestPlan $manifest @('src/Controls/DxUi.Grid.cpp')
            foreach($consumer in @('Grid','Tree','Theme','Animation','EditorControls','Tooltip','Embedded','WindowHost')) {Assert-Scope ($consumer -in $grid.scopes) "Grid caller omitted: $consumer"}
        } else {
            $systemData=Get-ScopedTestPlan $manifest @('Plugins/SystemData/SystemData.cpp')
            Assert-Scope ('SystemDataPhase0' -in $systemData.scopes) 'Referenced SystemData module did not select its phase-zero consumer'
        }
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
        $full=Join-Path $targetRoot '.build/reports/scoped-tests/x64-Release/build.json'
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
        $native=Get-ScopedRunIdentity $fixture x64 Debug Example
        foreach($path in @('README.md','.agents/skills/example/SKILL.md','Specs/Done/SourceImport/test-port.json')) {
            $before=Get-ScopedSourceIdentity $fixture
            Write-Fixture (Join-Path $fixture $path) 'changed validator input'
            Assert-Scope ((Get-ScopedSourceIdentity $fixture) -cne $before) "Validator input omitted: $path"
        }
        Assert-Scope ((Get-ScopedSourceIdentity $fixture -CompiledOnly) -ceq $compiled) 'Prose invalidated build attestation'
        Assert-Scope ((Get-ScopedRunIdentity $fixture x64 Debug Example) -ceq $native) 'Validator prose unnecessarily invalidated native success'
        Write-Fixture (Join-Path $fixture 'code.cpp') 'new implementation'
        Assert-Scope ((Get-ScopedSourceIdentity $fixture -CompiledOnly) -cne $compiled) 'Source mutation did not invalidate build attestation'
    }
    Run-Case 'iteration maps retain exact case, index metadata and byte-based invalidation' {
        $map=[Collections.Specialized.OrderedDictionary]::new([StringComparer]::Ordinal)
        $map.Add('Tests/Data/A.txt','A'*64);$map.Add('Tests/Data/a.txt','B'*64)
        $path=Join-Path $fixture '.build/reports/scoped-tests/x64-Debug/iteration.json'
        $index=Get-ScopedIndexMap $fixture
        Write-ScopedIteration $path $fixture x64 Debug 'environment' 'options' $map $index '' ''
        $state=Read-ScopedIteration $path $fixture x64 Debug 'environment' 'options'
        Assert-Scope ($null -ne $state -and $state.source.Count -eq 2 -and $state.source['Tests/Data/A.txt'] -cne $state.source['Tests/Data/a.txt']) 'JSON baseline collapsed distinct case-sensitive Git paths'
        Assert-Scope ($null -eq (Read-ScopedIteration $path $fixture x64 Debug 'environment' 'other-options')) 'Changed worker options reused a baseline'
        $state.source['Tests/Data/a.txt']='C'*64
        Assert-Scope (@(Get-ScopedMapDelta $map $state.source).Count -eq 1) 'Ordinal source delta lost one case variant'
        $beforeIndex=Get-ScopedIndexMap $fixture
        Invoke-FixtureGit @('update-index','--assume-unchanged','code.cpp')
        $afterIndex=Get-ScopedIndexMap $fixture
        Assert-Scope ('code.cpp' -cin @(Get-ScopedPendingIndexDelta $fixture $beforeIndex $afterIndex)) 'An index flag change was invisible'
        Invoke-FixtureGit @('update-index','--no-assume-unchanged','code.cpp')
        $state=Get-Content $path -Raw|ConvertFrom-Json -AsHashtable
        $state.completedUtc=[DateTime]::UtcNow.AddHours(-25).ToString('o')
        $state.digest=& (Get-Module ScopedTesting) {param($s) Get-ScopedIterationDigest $s} $state
        Write-Fixture $path ($state|ConvertTo-Json -Depth 8)
        Assert-Scope ($null -eq (Read-ScopedIteration $path $fixture x64 Debug 'environment' 'options')) 'Expired baseline was reusable'
        Write-Fixture $path '{invalid'
        Assert-Scope ($null -eq (Read-ScopedIteration $path $fixture x64 Debug 'environment' 'options')) 'Malformed baseline was reusable'
        $refused=$false
        try {Write-ScopedIteration (Join-Path $fixture '.build/foreign.json') $fixture x64 Debug 'environment' 'options' $map $index '' ''} catch {$refused=$true}
        Assert-Scope $refused 'Iteration write escaped its report root'
    }
    Run-Case 'profile lease excludes competing processes and permits nested and disjoint work' {
        $lease=Enter-ScopedProfileLease $fixture x64 Debug
        try {
            $nested=Enter-ScopedProfileLease $fixture x64 Debug
            Exit-ScopedProfileLease $nested
            $childPath=Join-Path $fixture '.build/lease-child.ps1'
            Write-Fixture $childPath @'
param($Module,$Root,$Configuration)
Import-Module $Module
try {$lease=Enter-ScopedProfileLease $Root x64 $Configuration} catch {exit 17}
Exit-ScopedProfileLease $lease
exit 0
'@
            & (Join-Path $PSHOME $(if($IsWindows){'pwsh.exe'}else{'pwsh'})) -NoProfile -File $childPath -Module (Join-Path $repository 'Tools/ScopedTesting.psm1') -Root $fixture -Configuration Debug
            Assert-Scope ($LASTEXITCODE -eq 17) 'Competing process acquired the same profile lease'
            & (Join-Path $PSHOME $(if($IsWindows){'pwsh.exe'}else{'pwsh'})) -NoProfile -File $childPath -Module (Join-Path $repository 'Tools/ScopedTesting.psm1') -Root $fixture -Configuration Release
            Assert-Scope ($LASTEXITCODE -eq 0) 'Disjoint profile was blocked'
            & (Join-Path $PSHOME $(if($IsWindows){'pwsh.exe'}else{'pwsh'})) -NoProfile -File $childPath -Module (Join-Path $repository 'Tools/ScopedTesting.psm1') -Root (Join-Path $fixture 'other-worktree') -Configuration Debug
            Assert-Scope ($LASTEXITCODE -eq 0) 'Disjoint worktree was blocked'
        } finally {Exit-ScopedProfileLease $lease}
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
    Run-Case 'tooling identity is stable and hashes installed Git and GitHub CLI binaries' {
        $first=Get-ScopedToolIdentity
        Assert-Scope ($first -match '^[0-9a-f]{64}$') 'Tool identity is not a SHA-256 digest'
        Assert-Scope ((Get-ScopedToolIdentity) -ceq $first) 'Unchanged tool identity was unstable'
        $oldPath=Get-EnvironmentSnapshot @('PATH')
        $environmentBefore=Get-ScopedEnvironmentIdentity
        $shim=Join-Path $fixture 'tool-shim'
        try {
            [void](New-Item -ItemType Directory -Path $shim -Force)
            if ($IsWindows) {
                [IO.File]::WriteAllText((Join-Path $shim 'gh.cmd'),"@echo off`r`necho gh version fixture`r`n")
            } else {
                $shimPath=Join-Path $shim 'gh'
                [IO.File]::WriteAllText($shimPath,"#!/bin/sh`necho gh version fixture`n")
                [IO.File]::SetUnixFileMode($shimPath,[IO.UnixFileMode]::UserRead -bor [IO.UnixFileMode]::UserWrite -bor [IO.UnixFileMode]::UserExecute)
            }
            $oldPathValue=if($oldPath['PATH'].exists){$oldPath['PATH'].value}else{''}
            Set-Item Env:PATH -Value "$shim$([IO.Path]::PathSeparator)$oldPathValue"
            Assert-Scope ((Get-ScopedToolIdentity) -cne $first) 'Replacing an installed tool binary did not invalidate tooling identity'
        } finally {Restore-EnvironmentSnapshot $oldPath}
        Assert-EnvironmentSnapshot $oldPath 'tool identity fixture'
        Assert-Scope ((Get-ScopedEnvironmentIdentity) -ceq $environmentBefore) 'Tool identity fixture changed the caller environment identity'
    }
    Run-Case 'deleted source identity is stable before staging, after staging, and after commit' {
        $deletedFixture=Join-Path $fixture 'deleted-source-fixture'
        [void](New-Item -ItemType Directory -Path $deletedFixture -Force)
        & git -C $deletedFixture init -q
        & git -C $deletedFixture config user.name 'Scoped testing fixture'
        & git -C $deletedFixture config user.email 'fixture@example.invalid'
        [IO.File]::WriteAllText((Join-Path $deletedFixture 'removed.cpp'),'initial',[Text.UTF8Encoding]::new($false))
        & git -C $deletedFixture add -A
        & git -C $deletedFixture commit -q -m 'fixture baseline'
        $beforeDeletion=Get-ScopedSourceIdentity $deletedFixture
        Remove-Item -LiteralPath (Join-Path $deletedFixture 'removed.cpp')
        $unstagedDeletion=Get-ScopedSourceIdentity $deletedFixture
        & git -C $deletedFixture add -u -- removed.cpp
        $stagedDeletion=Get-ScopedSourceIdentity $deletedFixture
        & git -C $deletedFixture commit -q -m 'fixture deletion'
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
        $before=Get-ScopedRunIdentity $fixture x64 Debug Example
        Assert-Scope ((Get-ScopedRunIdentity $fixture x64 Release Example) -cne $before) 'Configuration ignored'
        Assert-Scope ((Get-ScopedRunIdentity $fixture x64 Debug Other) -cne $before) 'Scope ignored'
        $options=Get-EnvironmentSnapshot @('ASAN_OPTIONS')
        $environmentBefore=Get-ScopedEnvironmentIdentity
        try {Set-Item Env:ASAN_OPTIONS -Value 'halt_on_error=1:fixture=changed';Assert-Scope ((Get-ScopedRunIdentity $fixture x64 Debug Example) -cne $before) 'Environment ignored'}
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
        $buildReceipt=Join-Path $fixture '.build/reports/scoped-tests/x64-Debug/build.json'
        $runReceipt=Join-Path $fixture '.build/reports/scoped-tests/x64-Debug/Example.json'
        $exe=Join-Path $fixture '.build/x64/Debug/suite.exe'
        $library=Join-Path $fixture '.build/x64/Debug/DxUi.lib'
        Write-Fixture $exe 'unchanged executable'
        Write-Fixture $library 'library before'
        $buildInput=Get-ScopedBuildInputIdentity $fixture x64
        $artifact=Get-ScopedArtifactIdentity $fixture x64 Debug
        $runIdentity=Get-ScopedRunIdentity $fixture x64 Debug Example
        $exeBefore=Get-FileHash -LiteralPath $exe -Algorithm SHA256
        Write-ScopedReceipt $buildReceipt (Get-ScopedDigest ($buildInput+"`n"+$artifact))
        Write-ScopedReceipt $runReceipt $runIdentity

        Write-Fixture $library 'library after'

        $exeAfter=Get-FileHash -LiteralPath $exe -Algorithm SHA256
        Assert-Scope ($exeBefore.Hash -ceq $exeAfter.Hash) 'Library mutation changed executable bytes'
        Assert-Scope ((Get-ScopedArtifactIdentity $fixture x64 Debug) -cne $artifact) 'Static library was absent from artifact identity'
        $currentBuildIdentity=Get-ScopedDigest ($buildInput+"`n"+(Get-ScopedArtifactIdentity $fixture x64 Debug))
        Assert-Scope (-not(Test-ScopedReceipt $buildReceipt $currentBuildIdentity)) 'Stale build attestation accepted a changed static library'
        Assert-Scope (-not(Test-ScopedReceipt $runReceipt (Get-ScopedRunIdentity $fixture x64 Debug Example))) 'Run receipt reused after static library mutation'
    }
    Run-Case 'installed dependency bytes bind build and run identities while output artifacts stay fixed' {
        $buildReceipt=Join-Path $fixture '.build/reports/scoped-tests/x64-Debug/build.json'
        $runReceipt=Join-Path $fixture '.build/reports/scoped-tests/x64-Debug/Example.json'
        $exe=Join-Path $fixture '.build/x64/Debug/suite.exe'
        $artifact=Get-ScopedArtifactIdentity $fixture x64 Debug
        $buildInput=Get-ScopedBuildInputIdentity $fixture x64
        $dependency=Get-ScopedDependencyIdentity $fixture x64
        $runIdentity=Get-ScopedRunIdentity $fixture x64 Debug Example
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
                Assert-Scope (-not(Test-ScopedReceipt $runReceipt (Get-ScopedRunIdentity $fixture x64 Debug Example))) "Stale native run receipt accepted dependency mutation: $path"
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
    Run-Case 'PR delegation binds clean committed bytes and rejects concurrent changes' {
        $delegate=Join-Path $fixture 'delegation'
        function Invoke-DelegateGit([string[]]$Arguments) {& git -C $delegate @Arguments *> $null;if($LASTEXITCODE){throw "Delegation fixture git failed: $Arguments"}}
        [void](New-Item -ItemType Directory -Path $delegate -Force)
        $workflow="name: fixture`non:`n  pull_request:`n"
        [void](New-Item -ItemType Directory -Path (Join-Path $delegate '.github/workflows') -Force)
        [IO.File]::WriteAllText((Join-Path $delegate '.github/workflows/ci.yml'),$workflow)
        [IO.File]::WriteAllText((Join-Path $delegate 'code.cpp'),'initial')
        Invoke-DelegateGit @('init','-q')
        Invoke-DelegateGit @('config','user.name','Delegation fixture')
        Invoke-DelegateGit @('config','user.email','fixture@example.invalid')
        Invoke-DelegateGit @('add','-A');Invoke-DelegateGit @('commit','-q','-m','baseline')
        Invoke-DelegateGit @('update-ref','refs/remotes/origin/main','HEAD')
        Invoke-DelegateGit @('checkout','-q','-b','stacked-base')
        [IO.File]::WriteAllText((Join-Path $delegate 'base-only.cpp'),'base change')
        Invoke-DelegateGit @('add','-A');Invoke-DelegateGit @('commit','-q','-m','stacked base')
        Invoke-DelegateGit @('update-ref','refs/remotes/origin/stacked-base','HEAD')
        $stackedBase=(& git -C $delegate rev-parse HEAD).Trim()
        Invoke-DelegateGit @('remote','add','origin','https://github.com/fixture/example.git')
        Invoke-DelegateGit @('checkout','-q','-b','feature')
        [IO.File]::WriteAllText((Join-Path $delegate 'code.cpp'),'candidate')
        Invoke-DelegateGit @('add','-A');Invoke-DelegateGit @('commit','-q','-m','candidate')
        $coverage=[pscustomobject]@{repository='fixture/example';defaultBranch='main';prWorkflowDigest=(Get-ScopedDigest $workflow);prCoverage=@([pscustomobject]@{platform='x64';configuration='Release';scopes=@('Example')})}
        $previousGh=Get-Item Function:\global:gh -ErrorAction SilentlyContinue
        $global:ScopedFixtureGhCalls=0;$global:ScopedFixtureGhMutation=''
        $global:ScopedFixtureGhRoot=$delegate
        $global:ScopedFixtureGhBaseName='stacked-base'
        $global:ScopedFixtureGhBaseOid=$stackedBase
        function global:gh {
            param([Parameter(ValueFromRemainingArguments=$true)][string[]]$FixtureGhArguments)
            $global:ScopedFixtureGhCalls++;$global:LASTEXITCODE=0
            if($global:ScopedFixtureGhMutation -eq 'untracked') {[IO.File]::WriteAllText((Join-Path $global:ScopedFixtureGhRoot 'during-api.cpp'),'changed')}
            if($global:ScopedFixtureGhMutation -eq 'commit') {
                [IO.File]::WriteAllText((Join-Path $global:ScopedFixtureGhRoot 'code.cpp'),'concurrent committed change')
                & git -C $global:ScopedFixtureGhRoot add code.cpp *> $null
                & git -C $global:ScopedFixtureGhRoot commit -q -m 'concurrent change' *> $null
            }
            if ($FixtureGhArguments[0] -eq 'pr') {
                $branch=(& git -C $global:ScopedFixtureGhRoot branch --show-current).Trim()
                $head=(& git -C $global:ScopedFixtureGhRoot rev-parse HEAD).Trim()
                $payload=@{state='OPEN';baseRefName=$global:ScopedFixtureGhBaseName;baseRefOid=$global:ScopedFixtureGhBaseOid;headRefName=$branch;headRefOid=$head} | ConvertTo-Json -Compress
                if($global:ScopedFixtureGhMutation -eq 'base') { & git -C $global:ScopedFixtureGhRoot update-ref "refs/remotes/origin/$global:ScopedFixtureGhBaseName" HEAD *> $null }
                return $payload
            }
            '{"state":"active"}'
        }
        try {
            Assert-Scope (@(Get-ScopedPrCoverage $delegate $coverage x64 Release) -contains 'Example') 'Clean committed candidate was not delegated'
            foreach($state in @('untracked','unstaged','staged')) {
                $path=Join-Path $delegate $(if($state -eq 'untracked'){'pending.cpp'}else{'code.cpp'})
                [IO.File]::WriteAllText($path,'pending')
                if($state -eq 'staged'){Invoke-DelegateGit @('add','code.cpp')}
                $calls=$global:ScopedFixtureGhCalls
                Assert-Scope (@(Get-ScopedPrCoverage $delegate $coverage x64 Release).Count -eq 0) "Dirty candidate delegated: $state"
                Assert-Scope ($global:ScopedFixtureGhCalls -eq $calls) 'Dirty candidate consulted CI before retaining obligations locally'
                if($state -eq 'untracked'){Remove-Item -LiteralPath $path}else{Invoke-DelegateGit @('restore','--staged','--worktree','--','code.cpp')}
            }
            Invoke-DelegateGit @('checkout','-q','main')
            Assert-Scope (@(Get-ScopedPrCoverage $delegate $coverage x64 Release).Count -eq 0) 'Default branch was delegated'
            Invoke-DelegateGit @('checkout','-q','feature')
            Invoke-DelegateGit @('checkout','-q','--detach')
            Assert-Scope (@(Get-ScopedPrCoverage $delegate $coverage x64 Release).Count -eq 0) 'Detached candidate was delegated'
            Invoke-DelegateGit @('checkout','-q','feature')
            Invoke-DelegateGit @('remote','set-url','origin','https://github.com/other/repository.git')
            Assert-Scope (@(Get-ScopedPrCoverage $delegate $coverage x64 Release).Count -eq 0) 'Foreign remote was delegated'
            Invoke-DelegateGit @('remote','set-url','origin','https://github.com/fixture/example.git')
            Invoke-DelegateGit @('update-ref','refs/remotes/origin/stacked-base','HEAD')
            Assert-Scope (@(Get-ScopedPrCoverage $delegate $coverage x64 Release).Count -eq 0) 'Stale local PR-base ref was delegated'
            Invoke-DelegateGit @('update-ref','refs/remotes/origin/stacked-base',$stackedBase)
            $global:ScopedFixtureGhMutation='untracked'
            Assert-Scope (@(Get-ScopedPrCoverage $delegate $coverage x64 Release).Count -eq 0) 'Mutation during API lookup was delegated'
            Remove-Item -LiteralPath (Join-Path $delegate 'during-api.cpp')
            $global:ScopedFixtureGhMutation='base'
            Assert-Scope (@(Get-ScopedPrCoverage $delegate $coverage x64 Release).Count -eq 0) 'Concurrent PR-base ref change was delegated'
            Invoke-DelegateGit @('update-ref','refs/remotes/origin/stacked-base',$stackedBase)
            $global:ScopedFixtureGhMutation='commit'
            Assert-Scope (@(Get-ScopedPrCoverage $delegate $coverage x64 Release).Count -eq 0) 'Concurrent committed candidate change was delegated'
            # The digest binds every file of the coverage contract and the coverage the manifest claims, not the workflow alone
            # (CR-31 of the 10 October review): a CI-only quarantine in a runner changes what CI covers.
            $global:ScopedFixtureGhMutation=''
            [IO.File]::WriteAllText((Join-Path $delegate 'runner.ps1'),'run every suite')
            Invoke-DelegateGit @('add','-A');Invoke-DelegateGit @('commit','-q','-m','runner')
            $contract=[pscustomobject]@{repository='fixture/example';defaultBranch='main';prContractFiles=@('.github/workflows/ci.yml','runner.ps1');prWorkflowDigest='';prCoverage=$coverage.prCoverage}
            $contract.prWorkflowDigest=Get-ScopedPrContractDigest $delegate $contract
            Assert-Scope ($contract.prWorkflowDigest -cne (Get-ScopedDigest $workflow)) 'The contract digest is the workflow digest'
            Assert-Scope (@(Get-ScopedPrCoverage $delegate $contract x64 Release) -contains 'Example') 'A clean reviewed contract was not delegated'
            [IO.File]::WriteAllText((Join-Path $delegate 'runner.ps1'),'run every suite but Example when CI runs it')
            Invoke-DelegateGit @('add','-A');Invoke-DelegateGit @('commit','-q','-m','quarantine')
            Assert-Scope (@(Get-ScopedPrCoverage $delegate $contract x64 Release).Count -eq 0) 'A changed contract script was delegated'
            [IO.File]::WriteAllText((Join-Path $delegate 'runner.ps1'),'run every suite')
            Invoke-DelegateGit @('add','-A');Invoke-DelegateGit @('commit','-q','-m','restore runner')
            Assert-Scope (@(Get-ScopedPrCoverage $delegate $contract x64 Release) -contains 'Example') 'The restored contract was not delegated'
            $claim=[pscustomobject]@{repository='fixture/example';defaultBranch='main';prContractFiles=$contract.prContractFiles;prWorkflowDigest=$contract.prWorkflowDigest;
                prCoverage=@([pscustomobject]@{platform='x64';configuration='Release';scopes=@('Example','Other')})}
            Assert-Scope (@(Get-ScopedPrCoverage $delegate $claim x64 Release).Count -eq 0) 'A widened coverage claim was delegated'
            $missing=[pscustomobject]@{repository='fixture/example';defaultBranch='main';prContractFiles=@('.github/workflows/ci.yml','runner.ps1','gone.ps1');prWorkflowDigest=$contract.prWorkflowDigest;prCoverage=$coverage.prCoverage}
            Assert-Scope (@(Get-ScopedPrCoverage $delegate $missing x64 Release).Count -eq 0) 'A missing contract file was delegated'
            $closure=[pscustomobject]@{repository='fixture/example';defaultBranch='main';prContractClosure=$true;prContractFiles=@('.github/workflows/ci.yml');prWorkflowDigest='';prCoverage=$coverage.prCoverage}
            $closure.prWorkflowDigest=Get-ScopedPrContractDigest $delegate $closure
            [IO.Directory]::CreateDirectory((Join-Path $delegate 'Tools')) | Out-Null
            [IO.File]::WriteAllText((Join-Path $delegate 'Tools/new-ci-script.ps1'),'new CI behavior')
            Assert-Scope ((Get-ScopedPrContractDigest $delegate $closure) -cne $closure.prWorkflowDigest) 'New CI tooling did not change the reviewed closure'
            Remove-Item -LiteralPath (Join-Path $delegate 'Tools/new-ci-script.ps1')
            $closure.prCoverage=@([pscustomobject]@{platform='x64';configuration='Release';scopes=@('Other')})
            Assert-Scope ((Get-ScopedPrContractDigest $delegate $closure) -cne $closure.prWorkflowDigest) 'Manifest behavior changed without changing the reviewed closure'
            $noWorkflow=[pscustomobject]@{repository='fixture/example';defaultBranch='main';prContractFiles=@('runner.ps1');prWorkflowDigest='';prCoverage=$coverage.prCoverage}
            $threw=$false;try{[void](Get-ScopedPrContractDigest $delegate $noWorkflow)}catch{$threw=$true}
            Assert-Scope $threw 'A contract without the workflow has a digest'
        } finally {
            if($previousGh){Set-Item Function:\global:gh -Value $previousGh.ScriptBlock}else{Remove-Item Function:\global:gh}
            Remove-Variable -Name ScopedFixtureGhCalls,ScopedFixtureGhMutation,ScopedFixtureGhRoot,ScopedFixtureGhBaseName,ScopedFixtureGhBaseOid -Scope Global
        }
    }

    Run-Case 'a profile outside actual PR coverage cannot be delegated' {Assert-Scope (@(Get-ScopedPrCoverage $repository $manifest Unknown Unknown).Count -eq 0) 'Unknown PR profile delegated'}
    Run-Case 'compiler-free PRs delegate no native or compiler-dependent tooling obligations' {
        foreach($platform in @('x64','ARM64')){
            foreach($configuration in @('Debug','Release','ASan Debug')){
                Assert-Scope (@(Get-ScopedPrCandidateScopes $repository $manifest $platform $configuration @('src/changed.cpp')).Count -eq 0) 'PR checks cannot satisfy native/tooling execution'
            }
        }
        Assert-Scope ($manifest.prCoverage.Count -eq 0) 'The manifest advertises compiler-dependent PR coverage'
    }
    Run-Case 'reviewed CI keeps PR coverage empty and normal native execution in the main adapter' {
        Assert-Scope ((@($manifest.prCoverage)).Count -eq 0) 'Native PR coverage must remain empty'
        $engine=Join-Path $repository '.ci-engine'
        Import-Module (Join-Path $engine 'Tools/Modules/Ci.Project.psm1')
        Import-Module (Join-Path $engine 'Tools/Modules/Ci.Policy.psm1')
        $project=Read-CiProject $repository
        Test-CiProjectConfiguration @{root=$repository;engineRoot=$engine;project=$project;development=$false}
        $adapter=[IO.File]::ReadAllText((Join-Path $repository 'Tools/Invoke-ProjectCi.ps1'))
        Assert-Scope ($adapter.Contains("Run 'test.ps1'") -and $adapter.Contains("'-Full','-SkipTooling','-SkipBuild'")) 'The normal batch lost Full native execution'
        Assert-Scope ($adapter.Contains('Invoke-ToolingTests.ps1')) 'Compiler-dependent tooling lost its main owner'
        Assert-Scope ($adapter.Contains('Get-CiSuiteEvidence')) 'Native suite execution must produce canonical fresh evidence'
        Assert-Scope (@(Get-ScopedPrCoverage $repository $manifest x64 Release).Count -eq 0) 'Native qualification was delegated to a static PR check'
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
    Run-Case 'public runner executes once, reuses success and never retains failed or mutated runs' {
        $sandbox=Join-Path $fixture 'runner'
        Write-Fixture (Join-Path $sandbox '.gitignore') '.build/'
        Copy-Item -LiteralPath (Join-Path $repository 'Test-Changes.ps1') -Destination (Join-Path $sandbox 'Test-Changes.ps1')
        $moduleRelative=if ($manifest.repository -like '*/DxUi') {'Tools/ScopedTesting.psm1'} else {'Build/ScopedTesting.psm1'}
        Write-Fixture (Join-Path $sandbox $moduleRelative) ([IO.File]::ReadAllText((Join-Path $repository $moduleRelative)))
        Write-Fixture (Join-Path $sandbox 'Tests/Example.Tests.Case.cpp') 'int example;'
        Write-Fixture (Join-Path $sandbox 'Specs/TestRuns/history/SelfTest/Old.SelfTest.cpp') 'immutable historical source;'
        Write-Fixture (Join-Path $sandbox 'Tests/Other.Tests.Case.cpp') 'int other;'
        Write-Fixture (Join-Path $sandbox 'Tests/native-test-files.json') '["Tests/Example.Tests.Case.cpp","Tests/Other.Tests.Case.cpp"]'
        Write-Fixture (Join-Path $sandbox 'Tests/test-scopes.json') '{"version":1,"defaultBranch":"main","repository":"fixture/example","scopes":[{"name":"Example","native":true,"reuse":true},{"name":"Other","native":true,"reuse":true}],"rules":[{"pattern":"Tests/Example.*","scopes":["Example"],"reason":"fixture Example"},{"pattern":"Tests/Other.*","scopes":["Other"],"reason":"fixture Other"}],"toolingCommands":[],"prCoverage":[],"prWorkflowDigest":"none","interactiveObligations":[{"pattern":"Tests/Example.*","suites":["ForegroundFixture"],"reason":"not executed by this fixture"}]}'
        # Isolated control-flow fixture only; production uses evaluated native toolchain bytes.
        Write-Fixture (Join-Path $sandbox 'Tools/PerformancePolicy.psm1') @'
function Get-PerformanceToolIdentities {
    param($Root,$Platform,$Configuration,$IdentityScope)
    $path=Join-Path $Root '.build/toolchain.txt'
    $identity=if(Test-Path $path){(Get-FileHash $path).Hash}else{'A'*64}
    return @{identityStatus='build-only';buildIdentityVersion=2;toolchainIdentity=$identity}
}
Export-ModuleMember -Function Get-PerformanceToolIdentities
'@
        Write-Fixture (Join-Path $sandbox 'Tools/Test-Changelog.ps1') 'param($Base) exit 0'
        Write-Fixture (Join-Path $sandbox 'build.ps1') @'
param($Platform,$Configuration,$Projects)
$output=Join-Path $PSScriptRoot ".build/$Platform/$Configuration"
New-Item -ItemType Directory -Path $output -Force|Out-Null
[IO.File]::WriteAllText((Join-Path $output 'example.exe'),'fixture binary')
[IO.File]::WriteAllText((Join-Path $output 'DxUi.lib'),'fixture library')
[IO.File]::WriteAllText((Join-Path $output 'DxUi.pdb'),'fixture library symbols')
foreach($name in @('DxUi.FoundationTests','DxUi.ControlTests','DxUi.EmbeddedTests','DxUi.InteractiveLease','DxUi.EmbeddedControls')) {
    foreach($extension in @('exe','pdb')){[IO.File]::WriteAllText((Join-Path $output "$name.$extension"),'fixture output')}
}
exit 0
'@
        Write-Fixture (Join-Path $sandbox 'test.ps1') @'
param($Platform,$Configuration,[switch]$SkipBuild,[string[]]$Suites,[switch]$SkipTooling,[int]$ParallelSuites=1,[switch]$Full)
[IO.File]::AppendAllText((Join-Path $PSScriptRoot '.build/calls.txt'),"execute`n")
[IO.File]::WriteAllText((Join-Path $PSScriptRoot '.build/parallel.txt'),[string]$ParallelSuites)
if(Test-Path (Join-Path $PSScriptRoot '.build/fail')){exit 1}
if(Test-Path (Join-Path $PSScriptRoot '.build/mutate')){[IO.File]::AppendAllText((Join-Path $PSScriptRoot 'Tests/Example.Tests.Case.cpp'),'changed')}
[void](New-Item -ItemType Directory -Path (Join-Path $PSScriptRoot '.build/reports') -Force)
$report=if(Test-Path (Join-Path $PSScriptRoot '.build/skips')){'{"exitCode":0,"skips":["fixture capability unavailable"]}'}else{'{"exitCode":0,"skips":[]}'}
$suffix=if($env:DXUI_MUTANT){'.instrumented'}else{''}
foreach($suite in $Suites){[IO.File]::WriteAllText((Join-Path $PSScriptRoot ".build/reports/$suite-$Platform-$Configuration$suffix.json"),$report)}
exit 0
'@
        & git -C $sandbox init -q
        if ($LASTEXITCODE) {throw 'Runner fixture git init failed'}
        & git -C $sandbox config user.name 'Scoped runner fixture'
        & git -C $sandbox config user.email 'fixture@example.invalid'
        & git -C $sandbox add -A
        & git -C $sandbox commit -q -m baseline
        if ($LASTEXITCODE) {throw 'Runner fixture baseline commit failed'}
        & git -C $sandbox update-ref refs/remotes/origin/main HEAD
        if ($LASTEXITCODE) {throw 'Runner fixture default ref failed'}
        $fixturePlatform=if([Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString() -eq 'Arm64') {'ARM64'} else {'x64'}
        Write-FixtureDependencies $sandbox $fixturePlatform
        function Invoke-PublicRunner([string[]]$Arguments,[switch]$NoExplicitScope,[switch]$Hosted) {
            $start=[Diagnostics.ProcessStartInfo]::new((Join-Path $PSHOME 'pwsh.exe'))
            if (-not(Test-Path $start.FileName)) {$start.FileName=Join-Path $PSHOME 'pwsh'}
            $start.UseShellExecute=$false;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
            # These recording fixtures model an ordinary local iteration even when the
            # enclosing test runs in CI. The separate Hosted control retains CI freshness.
            foreach($name in @('CI','GITHUB_ACTIONS')){[void]$start.Environment.Remove($name)}
            if($Hosted){$start.Environment['CI']='true';$start.Environment['GITHUB_ACTIONS']='true'}
            $runnerArguments=@('-NoProfile','-File',(Join-Path $sandbox 'Test-Changes.ps1'))
            if(-not $NoExplicitScope){$runnerArguments+=@('-Scopes','Example')}
            $runnerArguments+=@('-Platform',$fixturePlatform)+$Arguments
            foreach($argument in $runnerArguments){$start.ArgumentList.Add($argument)}
            $process=[Diagnostics.Process]::Start($start)
            try {$stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync();$process.WaitForExit();return [pscustomobject]@{Exit=$process.ExitCode;Text=$stdout.GetAwaiter().GetResult()+$stderr.GetAwaiter().GetResult()}}
            finally {$process.Dispose()}
        }
        $run=Invoke-PublicRunner @() -NoExplicitScope
        Assert-Scope ($run.Exit -eq 0 -and $run.Text.Contains('NO_WORK; repository NOT_EVALUATED') -and -not $run.Text.Contains('FULL_SCOPES_PASSED')) "Clean no-op was reported as a pass: $($run.Text)"
        $run=Invoke-PublicRunner @();Assert-Scope ($run.Exit -eq 0) "Initial execution failed: $($run.Text)"
        $run=Invoke-PublicRunner @('-SkipBuild');Assert-Scope ($run.Exit -eq 0 -and $run.Text.Contains('REUSED Example')) "Identical success was executed again: $($run.Text)"
        Assert-Scope (@(Get-Content (Join-Path $sandbox '.build/calls.txt')).Count -eq 1) 'Cached call executed runtime'
        $run=Invoke-PublicRunner @('-SkipBuild','-ParallelSuites','2')
        Assert-Scope ($run.Exit -eq 0 -and -not $run.Text.Contains('REUSED Example')) "Changed worker limit reused an incompatible result: $($run.Text)"
        Assert-Scope ((Get-Content (Join-Path $sandbox '.build/parallel.txt') -Raw) -ceq '2') 'Public runner did not forward the chosen worker limit'
        $run=Invoke-PublicRunner @('-SkipBuild','-ParallelSuites','2')
        Assert-Scope ($run.Exit -eq 0 -and $run.Text.Contains('REUSED Example')) 'Identical worker-limit result was not reusable'
        $receipt=Join-Path $sandbox ".build/reports/scoped-tests/$fixturePlatform-Debug/Example.json"
        Write-Fixture (Join-Path $sandbox '.build/fail') 'fail'
        $run=Invoke-PublicRunner @('-Force','-SkipBuild');Assert-Scope ($run.Exit -ne 0 -and -not(Test-Path $receipt)) 'Failure retained earlier reusable success'
        Remove-Item -LiteralPath (Join-Path $sandbox '.build/fail')
        $run=Invoke-PublicRunner @('-SkipBuild');Assert-Scope ($run.Exit -eq 0 -and -not $run.Text.Contains('REUSED Example')) 'Failed run was reused'
        $run=Invoke-PublicRunner @('-Force','-SkipBuild')
        Assert-Scope ($run.Exit -eq 0) "Clean forced rerun failed: $($run.Text)"
        $receipt=Join-Path $sandbox ".build/reports/scoped-tests/$fixturePlatform-Debug/Example.json"
        $previousMutant=Get-EnvironmentSnapshot @('DXUI_MUTANT')
        $environmentBeforeMutant=Get-ScopedEnvironmentIdentity
        try {
            Set-Item Env:DXUI_MUTANT -Value 'fixture-instrumentation'
            $run=Invoke-PublicRunner @('-Force','-SkipBuild')
            Assert-Scope ($run.Exit -eq 0 -and $run.Text.Contains('INSTRUMENTED_SCOPE_PASS') -and -not(Test-Path $receipt)) "Instrumented execution was not separately classified: $($run.Text)"
            Assert-Scope (Test-Path (Join-Path $sandbox ".build/reports/Example-$fixturePlatform-Debug.instrumented.json")) 'Instrumented suite report was not read from its separate path'
        } finally { Restore-EnvironmentSnapshot $previousMutant }
        Assert-EnvironmentSnapshot $previousMutant 'DXUI_MUTANT runner fixture'
        Assert-Scope ((Get-ScopedEnvironmentIdentity) -ceq $environmentBeforeMutant) 'DXUI_MUTANT runner fixture changed the caller environment identity'
        Write-Fixture (Join-Path $sandbox ".build/reports/Example-$fixturePlatform-Debug.json") '{"exitCode":0,"skips":["prior capability skip"]}'
        Write-Fixture (Join-Path $sandbox '.build/skips') 'skip'
        $run=Invoke-PublicRunner @('-SkipBuild')
        Assert-Scope ($run.Exit -eq 0 -and -not $run.Text.Contains('REUSED Example') -and $run.Text.Contains('PASSED_WITH_CAPABILITY_SKIPS') -and $run.Text.Contains('repository NOT_EVALUATED') -and -not(Test-Path $receipt)) 'Old or current capability-skipped suite was reported or retained as complete coverage'
        Remove-Item -LiteralPath (Join-Path $sandbox '.build/skips')
        # Default iteration consumes only its own successful affected lane, never Full/PrePush,
        # explicit scopes or cumulative foreground obligations.
        Write-Fixture (Join-Path $sandbox 'Tests/Example.Tests.Case.cpp') 'int exampleChanged;'
        $run=Invoke-PublicRunner @() -NoExplicitScope
        Assert-Scope ($run.Exit -eq 0 -and $run.Text.Contains('scopes: Example') -and $run.Text.Contains('INTERACTIVE_NOT_RUN')) "Initial affected iteration failed: $($run.Text)"
        $iteration=Join-Path $sandbox ".build/reports/scoped-tests/$fixturePlatform-Debug/iteration.json"
        Assert-Scope (Test-Path $iteration) 'Default success did not establish an iteration baseline'
        $count=@(Get-Content (Join-Path $sandbox '.build/calls.txt')).Count
        $run=Invoke-PublicRunner @() -NoExplicitScope
        Assert-Scope ($run.Exit -eq 0 -and $run.Text.Contains('NO_WORK') -and $run.Text.Contains('INTERACTIVE_NOT_RUN') -and @(Get-Content (Join-Path $sandbox '.build/calls.txt')).Count -eq $count) "Unchanged iteration repeated native execution or hid foreground debt: $($run.Text)"
        Write-Fixture (Join-Path $sandbox 'Tests/Other.Tests.Case.cpp') 'int otherChanged;'
        $run=Invoke-PublicRunner @() -NoExplicitScope
        Assert-Scope ($run.Exit -eq 0 -and $run.Text.Contains('scopes: Other') -and -not $run.Text.Contains('scopes: Example,')) "Later edit retested the earlier branch change: $($run.Text)"
        & git -C $sandbox add -- Tests/Other.Tests.Case.cpp
        if($LASTEXITCODE){throw 'Cannot stage the tested fixture bytes'}
        $run=Invoke-PublicRunner @() -NoExplicitScope
        Assert-Scope ($run.Exit -eq 0 -and $run.Text.Contains('NO_WORK')) "Staging tested bytes repeated native work: $($run.Text)"
        Write-Fixture (Join-Path $sandbox 'Tests/Other.Tests.Case.cpp') 'int differentStagedOther;'
        & git -C $sandbox add -- Tests/Other.Tests.Case.cpp
        if($LASTEXITCODE){throw 'Cannot stage distinct fixture bytes'}
        Write-Fixture (Join-Path $sandbox 'Tests/Other.Tests.Case.cpp') 'int otherChanged;'
        $run=Invoke-PublicRunner @() -NoExplicitScope
        Assert-Scope ($run.Exit -eq 0 -and $run.Text.Contains('scopes: Other') -and -not $run.Text.Contains('NO_WORK')) 'Working bytes concealed a changed staged input'
        $run=Invoke-PublicRunner @('-Explain','-BaseRef','origin/main') -NoExplicitScope
        Assert-Scope ($run.Exit -eq 0 -and $run.Text.Contains('scopes: Example, Other')) 'Explicit base silently used the mutable iteration baseline'
        foreach($mode in @('Full','PrePush')) {
            $run=Invoke-PublicRunner @('-Explain','-Mode',$mode) -NoExplicitScope
            Assert-Scope ($run.Exit -eq 0 -and $run.Text.Contains('scopes: Example, Other') -and $run.Text.Contains('INTERACTIVE_NOT_RUN')) "$mode lost cumulative obligations"
        }
        Write-Fixture (Join-Path $sandbox '.build/toolchain.txt') 'serviced SDK/compiler bytes'
        $run=Invoke-PublicRunner @('-Explain') -NoExplicitScope
        Assert-Scope ($run.Exit -eq 0 -and $run.Text.Contains('native context changed') -and $run.Text.Contains('scopes: Example, Other')) 'Changed toolchain retained earlier affected coverage'
        $run=Invoke-PublicRunner @() -NoExplicitScope
        Assert-Scope ($run.Exit -eq 0) "Changed toolchain fallback failed: $($run.Text)"
        Write-Fixture (Join-Path $sandbox '.build/fail') 'fail'
        $run=Invoke-PublicRunner @('-Force','-SkipBuild')
        Assert-Scope ($run.Exit -ne 0 -and -not(Test-Path $iteration)) 'Failed explicit rerun retained a default no-work baseline'
        Remove-Item -LiteralPath (Join-Path $sandbox '.build/fail')
        $run=Invoke-PublicRunner @() -NoExplicitScope
        Assert-Scope ($run.Exit -eq 0 -and -not $run.Text.Contains('NO_WORK')) 'Old baseline hid the latest same-source failure'
        Write-Fixture (Join-Path $sandbox '.build/mutate') 'mutate'
        $run=Invoke-PublicRunner @('-Force','-SkipBuild');Assert-Scope ($run.Exit -ne 0 -and -not(Test-Path $receipt)) 'Post-run mutation published success'
        Remove-Item -LiteralPath (Join-Path $sandbox '.build/mutate')
        $run=Invoke-PublicRunner @('-SkipBuild');Assert-Scope ($run.Exit -ne 0 -and $run.Text.Contains('SkipBuild refused')) 'Stale compiled input accepted'
        $parentCi=Get-EnvironmentSnapshot @('CI','GITHUB_ACTIONS')
        $run=Invoke-PublicRunner @() -NoExplicitScope
        Assert-Scope ($run.Exit -eq 0 -and (Test-Path $iteration)) "Local fixture could not establish the hosted-refusal control: $($run.Text)"
        $count=@(Get-Content (Join-Path $sandbox '.build/calls.txt')).Count
        $run=Invoke-PublicRunner @() -NoExplicitScope -Hosted
        Assert-Scope ($run.Exit -eq 0 -and -not $run.Text.Contains('NO_WORK') -and -not(Test-Path $iteration) -and
            @(Get-Content (Join-Path $sandbox '.build/calls.txt')).Count -gt $count) "CI reused or published a local iteration baseline: $($run.Text)"
        Assert-EnvironmentSnapshot $parentCi 'Local/hosted fixture process isolation'

        # Exercise the actual public ordering with independent tooling plus a later
        # failed native obligation; a passed tooling result must survive that failure.
        $fixtureManifest=Get-Content (Join-Path $sandbox 'Tests/test-scopes.json') -Raw|ConvertFrom-Json
        $fixtureManifest.scopes+= [pscustomobject]@{name='Tooling';native=$false;reuse=$true}
        $fixtureManifest.toolingCommands=@('Tools/Check-Fixture.ps1')
        Write-Fixture (Join-Path $sandbox 'Tests/test-scopes.json') ($fixtureManifest|ConvertTo-Json -Depth 8)
        Write-Fixture (Join-Path $sandbox 'Tools/Check-Fixture.ps1') @'
[IO.File]::AppendAllText((Join-Path $PSScriptRoot '../.build/tooling-calls.txt'),"tooling`n")
if(Test-Path (Join-Path $PSScriptRoot '../.build/tooling-fail')){throw 'Deliberate tooling failure'}
if(Test-Path (Join-Path $PSScriptRoot '../.build/tooling-mutate')){[IO.File]::AppendAllText((Join-Path $PSScriptRoot '../README.md'),'changed during tooling')}
'@
        Write-Fixture (Join-Path $sandbox '.build/fail') 'fail native after tooling'
        $run=Invoke-PublicRunner @('-Mode','Full') -NoExplicitScope
        $toolingReceipt=Join-Path $sandbox '.build/reports/scoped-tests/independent/Tooling.json'
        Assert-Scope ($run.Exit -ne 0 -and (Test-Path $toolingReceipt) -and -not(Test-Path $receipt) -and -not(Test-Path $iteration)) "Native failure discarded independent tooling or retained native success: $($run.Text)"
        $toolingBytes=[IO.File]::ReadAllText($toolingReceipt)
        Remove-Item -LiteralPath (Join-Path $sandbox '.build/fail')
        $run=Invoke-PublicRunner @('-Mode','Full') -NoExplicitScope
        Assert-Scope ($run.Exit -eq 0 -and $run.Text.Contains('REUSED Tooling') -and @(Get-Content (Join-Path $sandbox '.build/tooling-calls.txt')).Count -eq 1 -and
            [IO.File]::ReadAllText($toolingReceipt) -ceq $toolingBytes) "Continuation repeated or rewrote completed tooling: $($run.Text)"
        Write-Fixture (Join-Path $sandbox '.build/tooling-fail') 'fail tooling'
        $run=Invoke-PublicRunner @('-Mode','Full','-Force') -NoExplicitScope
        Assert-Scope ($run.Exit -ne 0 -and -not(Test-Path $toolingReceipt)) 'Failed tooling retained its earlier success'
        Remove-Item -LiteralPath (Join-Path $sandbox '.build/tooling-fail')
        Write-Fixture (Join-Path $sandbox '.build/tooling-mutate') 'mutate while tooling runs'
        $nativeCalls=@(Get-Content (Join-Path $sandbox '.build/calls.txt')).Count
        $run=Invoke-PublicRunner @('-Mode','Full') -NoExplicitScope
        Assert-Scope ($run.Exit -ne 0 -and -not(Test-Path $toolingReceipt) -and @(Get-Content (Join-Path $sandbox '.build/calls.txt')).Count -eq $nativeCalls) 'Tooling mutation published success or entered native execution'
        Remove-Item -LiteralPath (Join-Path $sandbox '.build/tooling-mutate')
        $savedInstrumentation=Get-EnvironmentSnapshot @('DXUI_MUTANT')
        try {
            Set-Item Env:DXUI_MUTANT -Value 'fixture-tooling-instrumentation'
            $run=Invoke-PublicRunner @('-Mode','Full') -NoExplicitScope
            Assert-Scope ($run.Exit -eq 0 -and -not(Test-Path $toolingReceipt)) "Instrumented tooling published reusable ordinary proof: $($run.Text)"
        } finally {Restore-EnvironmentSnapshot $savedInstrumentation}
        Write-Fixture (Join-Path $sandbox '.build/mutate') 'native changes source after tooling completed'
        $run=Invoke-PublicRunner @('-Mode','Full') -NoExplicitScope
        Assert-Scope ($run.Exit -ne 0 -and (Test-Path $toolingReceipt)) 'Later native mutation discarded the original independent tooling evidence or passed the batch'
        $originalToolingBytes=[IO.File]::ReadAllText($toolingReceipt)
        $toolingCalls=@(Get-Content (Join-Path $sandbox '.build/tooling-calls.txt')).Count
        Remove-Item -LiteralPath (Join-Path $sandbox '.build/mutate')
        $run=Invoke-PublicRunner @('-Mode','Full') -NoExplicitScope
        Assert-Scope ($run.Exit -eq 0 -and -not $run.Text.Contains('REUSED Tooling') -and @(Get-Content (Join-Path $sandbox '.build/tooling-calls.txt')).Count -eq ($toolingCalls+1) -and
            [IO.File]::ReadAllText($toolingReceipt) -cne $originalToolingBytes) "Later source mutation reused tooling for earlier inputs: $($run.Text)"
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
