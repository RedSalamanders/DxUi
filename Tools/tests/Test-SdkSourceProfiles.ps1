<# .SYNOPSIS Tests historical SDK source recipes and public source-build refusal without native compilation or qualification. #>
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $root 'Tools/SdkSourceProfiles.psm1')
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
function Asset([string]$Root,[string]$Path,[string]$Text){$full=Join-Path $Root $Path;[void][IO.Directory]::CreateDirectory((Split-Path $full));[IO.File]::WriteAllText($full,$Text)}
Invoke-FixtureCase 'only reviewed old sources select explicit API-4 recipes and API-5 retains its failure host' {
    param($fixture)
    Asset $fixture capabilities.json '{"apiRevision":4}'
    $basic=Get-DxUiSdkSourceProfile $fixture -SourceCommit 4cdf1ce52c7785446698e346b8aec058ed6a9283
    Assert-Equal 'api4-basic-2026-10-29-v1' $basic.name 'the oldest source has a named bounded recipe'
    Assert-Equal 0 $basic.shared.Count 'API-4 does not import API-5 failure-host code'
    Assert-Throws {Get-DxUiSdkSourceProfile $fixture -SourceCommit f2e328a0a26a18ca66bf4b2aea77bf010c9515e1} 'a missing declared source sample refuses'
    Asset $fixture Samples/SdkSmoke/Main.cpp '// fixture source sample'
    $rich=Get-DxUiSdkSourceProfile $fixture -SourceCommit f2e328a0a26a18ca66bf4b2aea77bf010c9515e1
    Assert-Equal 'api4-source-owned-2026-11-02-v1' $rich.name 'the rich API-4 source keeps its own sample'
    Assert-Throws {Get-DxUiSdkSourceProfile $fixture -SourceCommit ('a'*40)} 'unknown API-4 source has no implicit recipe'
    Asset $fixture capabilities.json '{"apiRevision":5}'
    Assert-Throws {Get-DxUiSdkSourceProfile $fixture -SourceCommit 4cdf1ce52c7785446698e346b8aec058ed6a9283} 'a known pin cannot be relabelled as another API'
    Asset $fixture Samples/Shared/FailureHost.h '// fixture host header'
    Assert-Throws {Get-DxUiSdkSourceProfile $fixture -SourceCommit ('a'*40)} 'incomplete API-5 host refuses'
    Asset $fixture Samples/Shared/FailureHost.cpp '// fixture host source'
    $current=Get-DxUiSdkSourceProfile $fixture -SourceCommit ('a'*40)
    Assert-Equal 2 $current.shared.Count 'API-5 ships both original host sources'
    Assert-Equal 'Main|FailureHost' ($current.units -join '|') 'both units require header-closure proof'
}
Invoke-FixtureCase 'actual source-build entrypoint rejects unsupported roles and dirty source before discovery or output' {
    param($fixture)
    $shell=(Get-Process -Id $PID).Path
    $script=Join-Path $root 'build.ps1'
    foreach($extra in @(@('-Configuration','Release'),@('-Configuration','Debug','-ProductionOnly'),
        @('-Configuration','Release','-ProductionOnly','-OutputRoot',(Join-Path $fixture 'foreign')),
        @('-Configuration','Release','-ProductionOnly','-Projects','DxUi'))){
        $text=@(& $shell -NoProfile -File $script -SourceRoot $fixture @extra 2>&1) -join "`n"
        Assert-True ($LASTEXITCODE -ne 0) 'unsupported source role refuses'
        Assert-True ($text -match 'SourceRoot supports only|ProductionOnly requires Release|separate build roles') 'role refusal precedes compiler discovery'
        Assert-True (-not(Test-Path (Join-Path $fixture '.build'))) 'refusal created no source build tree'
    }
    Asset $fixture DxUi.sln 'fixture, never evaluated'
    & git -C $fixture init -q -b main
    if($LASTEXITCODE){throw 'Fixture Git init failed.'}
    $text=@(& $shell -NoProfile -File $script -SourceRoot $fixture -Configuration Release -ProductionOnly 2>&1) -join "`n"
    Assert-True ($LASTEXITCODE -ne 0 -and $text -match 'clean committed source') 'uncommitted source refuses before discovery'
    Assert-True (-not(Test-Path (Join-Path $fixture '.build'))) 'dirty source created no build outputs'
}
Invoke-FixtureCase 'explicit local source entrypoint refuses hosted, missing, clean and unsupported source roles before outputs' {
    param($fixture)
    $script=Join-Path $root 'build.ps1';$shell=(Get-Process -Id $PID).Path
    $native=$env:CI_NATIVE_BUILDS_DISABLED;$hosted=$env:GITHUB_ACTIONS
    try{
        $env:CI_NATIVE_BUILDS_DISABLED='';$env:GITHUB_ACTIONS=''
        $text=@(& $shell -NoProfile -File $script -LocalUnpinnedSource -Configuration Release -ProductionOnly 2>&1)-join "`n"
        Assert-True ($LASTEXITCODE -ne 0 -and $text -match 'explicit dirty local SourceRoot') 'local source cannot be implicit'
        $env:GITHUB_ACTIONS='true'
        $text=@(& $shell -NoProfile -File $script -LocalUnpinnedSource -SourceRoot $fixture -Configuration Release -ProductionOnly 2>&1)-join "`n"
        Assert-True ($LASTEXITCODE -ne 0 -and $text -match 'hosted execution refuses') 'hosted requests refuse before source discovery'
        $env:GITHUB_ACTIONS=''
        Asset $fixture DxUi.sln 'fixture, never compiled'
        & git -C $fixture init -q -b main
        & git -C $fixture -c user.name=Fixture -c user.email=fixture@example.invalid -c commit.gpgsign=false add --all
        & git -C $fixture -c user.name=Fixture -c user.email=fixture@example.invalid -c commit.gpgsign=false commit -qm baseline
        if($LASTEXITCODE){throw 'Local source fixture initialization failed.'}
        $text=@(& $shell -NoProfile -File $script -LocalUnpinnedSource -SourceRoot $fixture -Configuration Release -ProductionOnly 2>&1)-join "`n"
        Assert-True ($LASTEXITCODE -ne 0 -and $text -match 'requires actual dirty source') 'clean local source does not impersonate a dirty role'
        [IO.File]::AppendAllText((Join-Path $fixture 'DxUi.sln'),' dirty')
        foreach($extra in @(@('-Configuration','Debug','-ProductionOnly'),@('-Configuration','Release'),@('-Configuration','Release','-ProductionOnly','-OutputRoot',(Join-Path $fixture 'foreign')))){
            $text=@(& $shell -NoProfile -File $script -LocalUnpinnedSource -SourceRoot $fixture @extra 2>&1)-join "`n"
            Assert-True ($LASTEXITCODE -ne 0 -and $text -match 'SourceRoot supports only|ProductionOnly requires Release') 'local source flag does not broaden native roles'
        }
        Assert-True (-not(Test-Path (Join-Path $fixture '.build'))) 'early local refusals created outputs'
        $tokens=$null;$errors=$null
        $ast=[Management.Automation.Language.Parser]::ParseFile($script,[ref]$tokens,[ref]$errors)
        $sourceGate=@($ast.EndBlock.Statements|Where-Object {$_ -is [Management.Automation.Language.IfStatementAst] -and $_.Clauses[0].Item1.Extent.Text -ceq '$SourceRoot'})[0]
        $SourceRoot=$fixture;$ProductionOnly=$true;$Configuration='Release';$OutputRoot='';$LocalUnpinnedSource=$true
        . ([scriptblock]::Create($sourceGate.Extent.Text))
        Assert-True ($sourceDirty -and $buildRoot -ceq $fixture) 'the actual source prelude did not admit the explicitly dirty local checkout'
        Assert-True (-not(Test-Path (Join-Path $fixture '.build'))) 'source admission discovered a compiler or created output'
    }finally{$env:CI_NATIVE_BUILDS_DISABLED=$native;$env:GITHUB_ACTIONS=$hosted}
}
Invoke-FixtureCase 'actual local source completion never publishes pinned provenance and retains dirty input classification' {
    param($fixture)
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $root 'build.ps1'),[ref]$tokens,[ref]$errors)
    $completion=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.IfStatementAst] -and $n.Clauses[0].Item1.Extent.Text -ceq '$SourceRoot' -and $n.Extent.Text.Contains('Complete-DxUiSdkSourceBuildRecord')},$true))
    Assert-Equal 1 $completion.Count 'the original completion branch is exercised'
    function Get-DxUiSdkToolingProvenance {param($Root,[switch]$LocalCandidate) return @{role=$(if($LocalCandidate){'local'}else{'pinned'})}}
    function Complete-DxUiSdkSourceBuildRecord {param($Root,$Platform,$Tooling,$LibraryRecord,$Arguments,$LogPath) $script:sourceRecordWrites++}
    $branch=[scriptblock]::Create($completion[0].Extent.Text)
    $SourceRoot=$fixture;$buildRoot=$fixture;$Platform='x64';$arguments=@('synthetic');$log=Join-Path $fixture 'not-written.log'
    $script:sourceRecordWrites=0;$LocalUnpinnedSource=$true;$producerToolingBefore=@{role='local'}
    $sdkLibraryRecord=@{inputState=@{sourceDirty=$true}}
    . $branch
    Assert-Equal 0 $script:sourceRecordWrites 'local dirty completion cannot emit a pinned-source descriptor'
    $sdkLibraryRecord.inputState.sourceDirty=$false
    Assert-Throws {. $branch} 'a local build cannot lose its explicit dirty source classification'
    Assert-Equal 0 $script:sourceRecordWrites 'a refused local completion cannot emit a pinned descriptor'
    $LocalUnpinnedSource=$false;$producerToolingBefore=@{role='pinned'}
    . $branch
    Assert-Equal 1 $script:sourceRecordWrites 'the ordinary clean source route still invokes the original pinned record writer'
}
Invoke-FixtureCase 'historical reader selection binds both subjects and preserves current and explicit-reader refusals' {
    param($fixture)
    # Execute the original selector at a recording boundary. The shared engine's real byte/pin
    # controls remain separate; these cases verify which reader and subjects it actually invokes.
    $scoped=Join-Path $root 'Tools/ScopedTesting.psm1'
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile($scoped,[ref]$tokens,[ref]$errors)
    $definitions=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq 'Get-ScopedSourceIdentity'},$true))
    Assert-Equal 1 $definitions.Count 'one canonical source selector exists'
    $owner=Join-Path $fixture 'tooling';$subject=Join-Path $fixture 'source'
    Asset $subject '.ci/project.json' '{}'
    Asset $owner '.ci-engine/Tools/Modules/Ci.SourceInputs.psm1' 'recording marker, never imported'
    $prefix=@'
Set-StrictMode -Version Latest
$script:imports=[Collections.Generic.List[string]]::new()
$script:subjects=[Collections.Generic.List[string]]::new()
$script:bad=''
function Get-ScopedSourceMap {param($Root,[switch]$CompiledOnly) return @{source='bytes'}}
function Get-ScopedMapIdentity {param($Map) return 'raw-source'}
function Get-ScopedDigest {param($Text) return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($Text)))}
function Import-Module {param($Name,$ErrorAction)
 if(-not [IO.File]::Exists($Name)){throw 'READER_UNAVAILABLE'}
 $script:imports.Add([IO.Path]::GetFullPath($Name))
}
function Get-CiPinnedEngineInputsIdentity {param($Root,$EngineRoot)
 if([IO.Path]::GetFullPath((Join-Path $Root '.ci-engine')) -cne [IO.Path]::GetFullPath($EngineRoot)){throw 'SUBJECT_REDIRECTED'}
 if($Root -ceq $script:bad){throw 'ENGINE_REFUSED'}
 $script:subjects.Add([IO.Path]::GetFullPath($Root))
 return ('engine:'+ [IO.Path]::GetFullPath($Root))
}
function Reset-Fixture {param($Bad='') $script:bad=$Bad;$script:imports.Clear();$script:subjects.Clear()}
function Get-Fixture {return @{imports=$script:imports.ToArray();subjects=$script:subjects.ToArray()}}
'@
    $modulePath=Join-Path $owner 'Tools/IdentityProbe.psm1'
    Asset $owner 'Tools/IdentityProbe.psm1' ($prefix+[char]10+$definitions[0].Extent.Text+[char]10+'Export-ModuleMember -Function Get-ScopedSourceIdentity,Reset-Fixture,Get-Fixture,Get-ScopedDigest')
    $probe=Import-Module $modulePath -PassThru
    try{
        $legacy=& $probe {param($r) Get-ScopedSourceIdentity $r} $subject
        $state=& $probe {Get-Fixture}
        Assert-Equal 2 $state.subjects.Count 'legacy selection authenticates both original and tooling engines'
        Assert-Equal ([IO.Path]::GetFullPath($subject)) $state.subjects[0] 'original engine is never substituted'
        Assert-Equal ([IO.Path]::GetFullPath($owner)) $state.subjects[1] 'current tooling engine is separately authenticated'
        Assert-Equal ([IO.Path]::GetFullPath((Join-Path $owner '.ci-engine/Tools/Modules/Ci.SourceInputs.psm1'))) $state.imports[0] 'reader code comes from the tooling owner'
        & $probe {param($r) Reset-Fixture $r} $owner
        Assert-Throws {& $probe {param($r) Get-ScopedSourceIdentity $r} $subject} 'wrong or dirty tooling engine cannot supply a reader'
        & $probe {param($r) Reset-Fixture $r} $subject
        Assert-Throws {& $probe {param($r) Get-ScopedSourceIdentity $r} $subject} 'wrong or dirty historical engine is not bypassed'
        & $probe {Reset-Fixture}
        Assert-Throws {& $probe {param($r,$e) Get-ScopedSourceIdentity $r -ToolingEngineRoot $e} $subject (Join-Path $fixture 'missing/.ci-engine')} 'an explicit absent reader has no fallback'
        Asset $subject '.ci-engine/Tools/Modules/Ci.SourceInputs.psm1' 'current recording marker'
        $current=& $probe {param($r) Get-ScopedSourceIdentity $r} $subject
        $state=& $probe {Get-Fixture}
        Assert-Equal 1 $state.subjects.Count 'current ordinary source retains its original one-engine identity'
        Assert-True ($legacy -cne $current) 'legacy two-engine and current default identities remain distinct'
    }finally{Remove-Module $probe -Force}
}
Complete-TestRun 'SDK source profiles'
