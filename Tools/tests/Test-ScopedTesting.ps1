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
    Run-Case 'every scope rule matches a current repository input' {
        $unmatched=@(& (Get-Module ScopedTesting) {
            param($root,$rules)
            $paths=@(Get-ScopedTrackedPaths $root)
            foreach($rule in $rules) {if(-not @($paths | Where-Object {Test-ScopedPattern $_ $rule.pattern}).Count) {$rule.pattern}}
        } $repository $manifest.rules)
        Assert-Scope ($unmatched.Count -eq 0) "Scope rules match no current input: $($unmatched -join ', ')"
    }
    Run-Case 'foreground source changes produce explicit interactive not-run obligations' {
        if ($manifest.repository -like '*/DxUi') {
            $rules=@($manifest.interactiveObligations)
            $paths=@(& (Get-Module ScopedTesting) {
                param($root,$rules)
                $tracked=@(Get-ScopedTrackedPaths $root)
                foreach($rule in $rules) { if(-not @($tracked | Where-Object {Test-ScopedPattern $_ $rule.pattern}).Count) {$rule.pattern} }
            } $repository $rules)
            Assert-Scope ($paths.Count -eq 0) "Interactive-obligation rules match no current input: $($paths -join ', ')"
            $menu=@(Get-ScopedInteractiveObligations -Manifest $manifest -ChangedPaths @('Tests/Controls/DxUi.Tests.Menu.cpp'))
            Assert-Scope ($menu.Count -eq 1 -and $menu[0].suite -eq 'Menu') 'Menu source change lost its explicit desktop obligation'
            $full=@(Get-ScopedInteractiveObligations -Manifest $manifest -ChangedPaths @() -Full)
            Assert-Scope (@($full | Where-Object suite -eq 'NativeTextInput').Count -gt 0) 'Full local work hid the separate native text-input gate'
        }
    }
    Run-Case 'one PrePush entry point accounts for six profiles and the revision-4 fixture is wired into consumers' {
        if ($manifest.repository -like '*/DxUi') {
            $prePush=Join-Path $repository 'Test-PrePush.ps1'
            $tokens=$null;$errors=$null
            $ast=[Management.Automation.Language.Parser]::ParseFile($prePush,[ref]$tokens,[ref]$errors)
            Assert-Scope (@($errors).Count -eq 0) 'PrePush accounting script has parse errors'
            $profiles=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.HashtableAst]},$true))
            Assert-Scope ($profiles.Count -eq 6) 'PrePush does not enumerate all six platform/configuration profiles'
            $fixture=Join-Path $repository 'Tools/ConsumerApi/Revision4.cpp'
            $consumer=Join-Path $repository 'test-consumer.ps1'
            Assert-Scope ((Test-Path -LiteralPath $fixture) -and [IO.File]::ReadAllText($consumer).Contains('Tools/ConsumerApi/Revision4.cpp') -and [IO.File]::ReadAllText($consumer).Contains('ConsumerApiRevision4.cpp')) 'Frozen public API fixture is not compiled by the exact-pin consumer gate'
        }
    }
    Run-Case 'active names require exact Tests spelling and reject historical inventory membership' {
        $naming=Join-Path $fixture 'naming'
        foreach($invalid in @('Tests/Scope.tests.Case.cpp','Specs/TestRuns/Scope.Tests.Case.cpp')) {
            Write-Fixture (Join-Path $naming $invalid) 'invalid declared source'
            Write-Fixture (Join-Path $naming 'Tests/native-test-files.json') (ConvertTo-Json -InputObject @($invalid) -Compress)
            $message=''
            try {Assert-ScopedTestNames $naming | Out-Null} catch [System.Management.Automation.RuntimeException] {$message=$_.Exception.Message}
            Assert-Scope ($message -match 'Invalid/missing native test source:|Historical/external source cannot enter') "Invalid inventory accepted: $invalid ($message)"
        }
    }
    Run-Case 'committed, staged, working, deleted, both rename sides and spaced untracked paths count' {
        $paths=@(Get-ScopedChangedPaths $fixture $baseline)
        foreach($path in @('committed.cpp','staged.cpp','code.cpp','deleted.cpp','renamed.cpp','renamed-new.cpp','untracked file.cpp')) {Assert-Scope ($path -in $paths) "Missing impact: $path"}
    }
    Run-Case 'unmapped executable input widens to every scope' {$plan=Get-ScopedTestPlan $manifest @('Unknown/new.cpp');Assert-Scope $plan.full 'Fallback was narrowed'}
    Run-Case 'executable schema changes are never mistaken for prose' {$plan=Get-ScopedTestPlan $manifest @('Specs/Unmapped.schema.json');Assert-Scope $plan.full 'Schema was ignored'}
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
        } finally {
            if($previousGh){Set-Item Function:\global:gh -Value $previousGh.ScriptBlock}else{Remove-Item Function:\global:gh}
            Remove-Variable -Name ScopedFixtureGhCalls,ScopedFixtureGhMutation,ScopedFixtureGhRoot,ScopedFixtureGhBaseName,ScopedFixtureGhBaseOid -Scope Global
        }
    }

    Run-Case 'a profile outside actual PR coverage cannot be delegated' {Assert-Scope (@(Get-ScopedPrCoverage $repository $manifest Unknown Unknown).Count -eq 0) 'Unknown PR profile delegated'}
    Run-Case 'PR coverage accounts for conditional native jobs without an API dependency' {
        $profile=$manifest.prCoverage[0]
        $documentation=@(Get-ScopedPrCandidateScopes $repository $manifest $profile.platform $profile.configuration @('README.md'))
        $code=@(Get-ScopedPrCandidateScopes $repository $manifest $profile.platform $profile.configuration @('src/Controls/DxUi.Grid.cpp'))
        if($manifest.repository -like '*/DxUi') {
            Assert-Scope ($documentation.Count -eq 1 -and $documentation[0] -eq 'Tooling') 'Skipped native PR jobs were counted as coverage'
            Assert-Scope ($code.Count -eq $manifest.scopes.Count) 'Native PR coverage was narrowed for code edits'
        } else {
            Assert-Scope ($documentation.Count -eq $manifest.scopes.Count -and $code.Count -eq $manifest.scopes.Count) 'Unconditional RedXe PR gate was incorrectly narrowed'
        }
    }
    Run-Case 'reviewed CI preserves full native coverage and separates independent tooling' {
        $workflow=[IO.File]::ReadAllText((Join-Path $repository '.github/workflows/ci.yml')) -replace "`r`n","`n"
        Assert-Scope ((Get-ScopedDigest $workflow) -ceq $manifest.prWorkflowDigest) 'CI contract digest is stale'
        Assert-Scope ($workflow -match 'test.ps1 -Full -SkipTooling') 'CI accidentally uses affected iteration or repeats independent tooling'
        $errors=$null;$tokens=$null;$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $repository 'test.ps1'),[ref]$tokens,[ref]$errors)
        Assert-Scope (@($errors).Count -eq 0) 'Native entrypoint has parse errors'
        $defaults=($ast.ParamBlock.Parameters | Where-Object {$_.Name.VariablePath.UserPath -eq 'Suites'}).DefaultValue.Extent.Text
        foreach($scope in $manifest.scopes){
            Assert-Scope ($defaults.Contains("'$($scope.name)'") -or -not $scope.native) "CI native default misses $($scope.name)"
        }
        foreach($profile in $manifest.prCoverage){foreach($scope in $profile.scopes){Assert-Scope ($scope -in $manifest.scopes.name) "PR profile declares nonexistent scope $scope"}}
        if($manifest.repository -like '*/DxUi') {
            Assert-Scope ($manifest.prCoverage.Count -eq 6 -and $workflow.Contains('MenuResourceScaling,MenuTextLayoutResources')) 'DxUi profile/resource coverage was narrowed'
            Assert-Scope ($workflow -match '(?m)^      - name: Qualify the relocated public consumer\r?\n        if: \$\{\{ !cancelled\(\) \}\}') 'Native failure hides independent consumer qualification'
            Assert-Scope ($workflow -match '(?m)^      - name: Qualify the consumer with STL annotations disabled\r?\n        if: \$\{\{ !cancelled\(\) && matrix.configuration == ''ASan Debug'' \}\}') 'Native or first consumer failure hides the ASan annotation variant'
            Assert-Scope ($workflow.Contains('run: ./validate.ps1')) 'Portable tooling gate missing'
            Assert-Scope ($workflow.Contains('windows-tooling:') -and ([regex]::Matches($workflow,'run: ./validate.ps1')).Count -eq 2) 'One qualification per Windows/Linux host was not retained'
            Assert-Scope (([regex]::Matches($workflow,'run: ./Tools/tests/Test-AsanRuntime.ps1')).Count -eq 1) 'Runtime staging fixture is repeated or missing'
            Assert-Scope ($workflow.Contains('ci-gate:') -and $workflow.Contains('needs: [validation, windows-tooling, format, native-scope, native, paired-benchmark]')) 'The aggregate gate does not require every qualification job'
            Assert-Scope ($workflow -match '(?m)^  native-scope:\r?\n    runs-on: ubuntu-24\.04\r?\n    timeout-minutes: 5\r?\n    outputs:') 'Native skip output is not a job-level classifier result'
        } else {
            Assert-Scope ($manifest.prCoverage.Count -eq 1 -and $manifest.prCoverage[0].configuration -eq 'Release' -and $manifest.prCoverage[0].platform -eq 'x64') 'RedXe PR coverage overclaimed'
            Assert-Scope ($workflow.Contains('./Tests/BuildProcessTests/Invoke-ToolingTests.ps1')) 'Independent tooling job missing'
        }
        $digest=$manifest.prWorkflowDigest
        try {$manifest.prWorkflowDigest='stale';$profile=$manifest.prCoverage[0];Assert-Scope (@(Get-ScopedPrCoverage $repository $manifest $profile.platform $profile.configuration).Count -eq 0) 'Modified workflow was delegated'}
        finally {$manifest.prWorkflowDigest=$digest}
    }
    Run-Case 'public runner executes once, reuses success and never retains failed or mutated runs' {
        $sandbox=Join-Path $fixture 'runner'
        Write-Fixture (Join-Path $sandbox '.gitignore') '.build/'
        Copy-Item -LiteralPath (Join-Path $repository 'Test-Changes.ps1') -Destination (Join-Path $sandbox 'Test-Changes.ps1')
        $moduleRelative=if ($manifest.repository -like '*/DxUi') {'Tools/ScopedTesting.psm1'} else {'Build/ScopedTesting.psm1'}
        Write-Fixture (Join-Path $sandbox $moduleRelative) ([IO.File]::ReadAllText((Join-Path $repository $moduleRelative)))
        Write-Fixture (Join-Path $sandbox 'Tests/Example.Tests.Case.cpp') 'int example;'
        Write-Fixture (Join-Path $sandbox 'Specs/TestRuns/history/SelfTest/Old.SelfTest.cpp') 'immutable historical source;'
        Write-Fixture (Join-Path $sandbox 'Tests/native-test-files.json') '["Tests/Example.Tests.Case.cpp"]'
        Write-Fixture (Join-Path $sandbox 'Tests/test-scopes.json') '{"version":1,"defaultBranch":"main","repository":"fixture/example","scopes":[{"name":"Example","native":true,"reuse":true}],"rules":[],"toolingCommands":[],"prCoverage":[],"prWorkflowDigest":"none"}'
        Write-Fixture (Join-Path $sandbox 'build.ps1') @'
param($Platform,$Configuration)
$output=Join-Path $PSScriptRoot ".build/$Platform/$Configuration"
New-Item -ItemType Directory -Path $output -Force|Out-Null
[IO.File]::WriteAllText((Join-Path $output 'example.exe'),'fixture binary')
exit 0
'@
        Write-Fixture (Join-Path $sandbox 'test.ps1') @'
param($Platform,$Configuration,[switch]$SkipBuild,[string[]]$Suites,[switch]$SkipTooling)
[IO.File]::AppendAllText((Join-Path $PSScriptRoot '.build/calls.txt'),"execute`n")
if(Test-Path (Join-Path $PSScriptRoot '.build/fail')){exit 1}
if(Test-Path (Join-Path $PSScriptRoot '.build/mutate')){[IO.File]::AppendAllText((Join-Path $PSScriptRoot 'Tests/Example.Tests.Case.cpp'),'changed')}
[void](New-Item -ItemType Directory -Path (Join-Path $PSScriptRoot '.build/reports') -Force)
$report=if(Test-Path (Join-Path $PSScriptRoot '.build/skips')){'{"exitCode":0,"skips":["fixture capability unavailable"]}'}else{'{"exitCode":0,"skips":[]}'}
$suffix=if($env:DXUI_MUTANT){'.instrumented'}else{''}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot ".build/reports/Example-$Platform-$Configuration$suffix.json"),$report)
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
        function Invoke-PublicRunner([string[]]$Arguments,[switch]$NoExplicitScope) {
            $start=[Diagnostics.ProcessStartInfo]::new((Join-Path $PSHOME 'pwsh.exe'))
            if (-not(Test-Path $start.FileName)) {$start.FileName=Join-Path $PSHOME 'pwsh'}
            $start.UseShellExecute=$false;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
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
        Write-Fixture (Join-Path $sandbox '.build/mutate') 'mutate'
        $run=Invoke-PublicRunner @('-Force','-SkipBuild');Assert-Scope ($run.Exit -ne 0 -and -not(Test-Path $receipt)) 'Post-run mutation published success'
        Remove-Item -LiteralPath (Join-Path $sandbox '.build/mutate')
        $run=Invoke-PublicRunner @('-SkipBuild');Assert-Scope ($run.Exit -ne 0 -and $run.Text.Contains('SkipBuild refused')) 'Stale compiled input accepted'
    }
    Write-Host "Scoped testing: $passed cases passed."
} finally {
    $resolved=[IO.Path]::GetFullPath($fixture);$allowed=[IO.Path]::GetFullPath((Join-Path $repository '.build/ToolTests'))+[IO.Path]::DirectorySeparatorChar
    if(-not $resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Fixture cleanup escaped its owned root.'}
    if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
exit 0
