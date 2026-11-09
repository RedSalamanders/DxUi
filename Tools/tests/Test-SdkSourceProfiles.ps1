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
Complete-TestRun 'SDK source profiles'
