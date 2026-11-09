# Source-owned SDK assets and reviewed compatibility recipes; no build or qualification occurs on import.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$script:scoped=Import-Module (Join-Path $PSScriptRoot 'ScopedTesting.psm1') -PassThru

function Get-DxUiSdkToolingProvenance {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root)
    $head=(& $script:scoped {param($r) Invoke-ScopedGit $r @('rev-parse','--verify','HEAD')} $Root).Trim()
    $dirty=(& $script:scoped {param($r) Invoke-ScopedGit $r @('status','--porcelain','--untracked-files=all')} $Root).Trim()
    if($head -cnotmatch '^[a-f0-9]{40}$' -or $dirty){throw 'SDK production requires clean committed producer tooling.'}
    return [ordered]@{repository='RedSalamanders/DxUi';commit=$head;contentIdentity=(Get-ScopedSourceIdentity -Root $Root)}
}

function Get-DxUiSdkSourceProfile {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{40}$')][string]$SourceCommit)
    $api=[int](Get-Content -LiteralPath (Join-Path $Root 'capabilities.json') -Raw|ConvertFrom-Json).apiRevision
    $main=Join-Path $Root 'Samples/SdkSmoke/Main.cpp'
    $shared=@()
    if($SourceCommit -ceq '4cdf1ce52c7785446698e346b8aec058ed6a9283'){
        if($api -ne 4){throw 'Legacy basic SDK recipe requires the reviewed API-4 source.'}
        $name='api4-basic-2026-10-29-v1'
        $main=Join-Path $PSScriptRoot '../Samples/SdkSmoke/LegacyApi4Basic.cpp'
    }elseif($SourceCommit -ceq 'f2e328a0a26a18ca66bf4b2aea77bf010c9515e1'){
        if($api -ne 4){throw 'Legacy rich SDK recipe requires the reviewed API-4 source.'}
        $name='api4-source-owned-2026-11-02-v1'
    }else{
        if($api -ne 5){throw 'No reviewed SDK packaging recipe exists for this source/API.'}
        $name='api5-source-owned-v1'
        $shared=@('FailureHost.h','FailureHost.cpp'|ForEach-Object{Join-Path $Root ('Samples/Shared/'+$_)})
    }
    foreach($path in @($main)+$shared){
        if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw 'SDK recipe requires every declared source asset.'}
        for($cursor=[IO.Path]::GetFullPath($path);$cursor;$cursor=Split-Path -Parent $cursor){
            if((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'SDK source assets cannot traverse reparse paths.'}
        }
    }
    return [ordered]@{name=$name;apiRevision=$api;main=[IO.Path]::GetFullPath($main);shared=$shared;
        units=$(if($shared.Count){@('Main','FailureHost')}else{@('Main')})}
}

Export-ModuleMember -Function Get-DxUiSdkToolingProvenance,Get-DxUiSdkSourceProfile
