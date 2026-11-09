# Immutable dependency bytes only. Producer trust and native/product qualification are separate obligations.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'SdkCacheInputs.psm1')

function Import-DxUiSdkStoreEngine {
    param([string]$EngineRoot)
    $toolingRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
    if(-not $EngineRoot){$EngineRoot=Join-Path $toolingRoot '.ci-engine'}
    $pin=Get-Content (Join-Path $toolingRoot '.ci/project.json') -Raw|ConvertFrom-Json
    $head=(& git -C $EngineRoot rev-parse --verify HEAD).Trim()
    if($LASTEXITCODE -or $pin.engine.repository -cne 'RedSalamanders/.github' -or $head -cne $pin.engine.ref){throw 'SDK store engine must match the exact producer-tooling pin.'}
    $dirty=@(& git -C $EngineRoot status --porcelain --untracked-files=all)
    if($LASTEXITCODE -or $dirty.Count){throw 'SDK store engine must be clean.'}
    return Import-Module (Join-Path $EngineRoot 'Tools/Modules/Ci.SdkCache.psm1') -PassThru
}

function Assert-DxUiSdkInputPayload {
    param([string]$Payload,[Collections.IDictionary]$Identity,[object]$Engine)
    $path=Join-Path $Payload 'sdk-manifest.json'
    if(-not(Test-Path $path -PathType Leaf) -or (Get-Item $path).Length -gt 64KB){throw 'SDK input payload has no bounded manifest.'}
    $manifest=Get-Content $path -Raw|ConvertFrom-Json -AsHashtable
    $actual=& $Engine {param($i) Get-CiSdkCacheKey $i} $manifest.sdkCacheIdentity
    $expected=& $Engine {param($i) Get-CiSdkCacheKey $i} $Identity
    if($actual -cne $expected -or $manifest.schemaVersion -ne 1 -or $manifest.kind -cne 'sdk' -or
        $manifest.source -cne $Identity.sourceCommit -or $manifest.apiRevision -ne $Identity.apiRevision -or
        $manifest.platform -cne $Identity.platform -or $manifest.configuration -cne 'Release' -or
        $manifest.runtime -cne 'MD' -or $manifest.testsIncluded -isnot [bool] -or $manifest.testsIncluded){throw 'SDK payload source/profile/input identity mismatch.'}
    foreach($relative in @('include/DxUi/DxUi.h','DxUi.Sdk.props','build/NativeBuildPolicy.targets')){
        if(-not(Test-Path (Join-Path $Payload $relative) -PathType Leaf)){throw 'SDK input public header/integration policy is incomplete.'}
    }
    foreach($pair in @(@('DxUi.lib','librarySha256'),@('DxUi.pdb','libraryPdbSha256'))){
        $file=Join-Path $Payload ("lib/$($Identity.platform)/Release/"+$pair[0])
        if($manifest[$pair[1]] -cnotmatch '^[a-f0-9]{64}$' -or -not(Test-Path $file -PathType Leaf) -or
            (Get-FileHash $file).Hash.ToLowerInvariant() -cne $manifest[$pair[1]]){throw 'SDK input library/compiler-PDB bytes do not match their manifest.'}
    }
}

function Get-DxUiSdkInputPayloadDigest {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$PayloadRoot,[string]$EngineRoot='')
    $engine=Import-DxUiSdkStoreEngine $EngineRoot
    # Reuse the complete bounded/reparse-safe byte catalog from the exact pinned storage implementation.
    # Its private helper is deliberately pin-bound here; the integration tests exercise this actual closure.
    return & $engine {param($p)
        $files=@(Get-CiSdkPayloadFiles $p)
        Get-CiSha256 ('dxui-sdk-payload-v1'+"`n"+($files|ConvertTo-Json -Depth 5 -Compress))
    } $PayloadRoot
}

function Publish-DxUiSdkInputBytes {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$StoreRoot,[Parameter(Mandatory)][string]$StoreRepositoryRoot,
        [Parameter(Mandatory)][string]$PayloadRoot,[Parameter(Mandatory)][Collections.IDictionary]$Identity,
        [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedPayloadDigest,[string]$EngineRoot='')
    $engine=Import-DxUiSdkStoreEngine $EngineRoot
    if((Get-DxUiSdkInputPayloadDigest -PayloadRoot $PayloadRoot -EngineRoot $EngineRoot) -cne $ExpectedPayloadDigest){throw 'SDK payload changed after the independently held qualification snapshot.'}
    Assert-DxUiSdkInputPayload $PayloadRoot $Identity $engine
    # The caller independently acquired this identity and completed native SDK gates. This function authenticates
    # no mutable receipt and grants no verdict; generic publication verifies/copies every payload byte atomically.
    & $engine {param($r,$s) Initialize-CiSdkStore -RepositoryRoot $r -StoreRoot $s} $StoreRepositoryRoot $StoreRoot
    return & $engine {param($s,$i,$p) Publish-CiSdkCacheEntry -StoreRoot $s -Identity $i -PayloadRoot $p} $StoreRoot $Identity $PayloadRoot
}

function Open-DxUiSdkInputBytes {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform,
        [Parameter(Mandatory)][string]$StoreRoot,[Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{64}$')][string]$ManifestSha256,
        [string]$EngineRoot='')
    $engine=Import-DxUiSdkStoreEngine $EngineRoot
    $identity=Get-DxUiSdkCacheInputIdentity -Root $Root -Platform $Platform
    $entry=& $engine {param($s,$i,$h) Open-CiSdkCacheEntry -StoreRoot $s -Identity $i -ManifestSha256 $h} $StoreRoot $identity $ManifestSha256
    if($null -eq $entry){return $null}
    try{
        Assert-DxUiSdkInputPayload $entry.Payload $identity $engine
        return [pscustomobject]@{Root=$Root;Platform=$Platform;Identity=$identity;Entry=$entry;Engine=$engine;SdkRoot=$entry.Payload;Closed=$false}
    }catch{
        $failure=$_
        try{& $engine {param($e) Close-CiSdkCacheEntry $e} $entry}catch{Write-Warning ('SDK input integrity close also failed: '+$_.Exception.Message)}
        throw $failure
    }
}

function Close-DxUiSdkInputBytes {
    [CmdletBinding()]
    param([Parameter(Mandatory)]$InputBytes)
    if($InputBytes.Closed){throw 'SDK input lease is already closed.'}
    $failure=$null
    try{
        $after=Get-DxUiSdkCacheInputIdentity -Root $InputBytes.Root -Platform $InputBytes.Platform
        $beforeKey=& $InputBytes.Engine {param($i) Get-CiSdkCacheKey $i} $InputBytes.Identity
        $afterKey=& $InputBytes.Engine {param($i) Get-CiSdkCacheKey $i} $after
        if($beforeKey -cne $afterKey){throw 'SDK source, profile, tooling or environment changed during use.'}
    }catch{$failure=$_}
    finally{
        try{& $InputBytes.Engine {param($e) Close-CiSdkCacheEntry $e} $InputBytes.Entry}
        catch{if($null -eq $failure){$failure=$_}else{Write-Warning ('SDK input integrity close also failed: '+$_.Exception.Message)}}
        finally{$InputBytes.Closed=$true}
    }
    if($null -ne $failure){throw $failure}
}

Export-ModuleMember -Function Get-DxUiSdkInputPayloadDigest,Publish-DxUiSdkInputBytes,Open-DxUiSdkInputBytes,Close-DxUiSdkInputBytes
