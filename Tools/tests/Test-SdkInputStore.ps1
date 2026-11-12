<# .SYNOPSIS Tests SDK payload/store integration with synthetic bytes. Does not qualify a native SDK or product. #>
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
$storeModule=Import-Module (Join-Path $root 'Tools/SdkInputStore.psm1') -Force -PassThru
$engine=Import-Module (Join-Path $root '.ci-engine/Tools/Modules/Ci.SdkCache.psm1') -PassThru
$identity=[ordered]@{schemaVersion=1;role='DependencySdkInputs';sourceRepository='RedSalamanders/DxUi';sourceCommit=('a'*40);sourceBytes=('b'*64);apiRevision=4;
    platform='x64';configuration='Release';runtimeFamily='MD';annotationPolicy='enabled';languageStandard='stdcpplatest';optimizationPolicy='GL-LTCG-full';
    toolchainIdentity=('c'*64);compilerProfileIdentity=('d'*64);dependencyIdentity=('e'*64);integrationPolicyIdentity=('f'*64);environmentIdentity=('0'*64)}
& $storeModule {param($i) $script:fixtureIdentity=$i;function script:Get-DxUiSdkCacheInputIdentity {param($Root,$Platform) return $script:fixtureIdentity}} $identity
function New-SdkStoreFixture([string]$Fixture){
    & git -C $Fixture init -q
    if($LASTEXITCODE){throw 'Owned store fixture Git initialization failed.'}
    $payload=Join-Path $Fixture 'payload';$store=Join-Path $Fixture '.build/sdk-cache'
    foreach($file in @('include/DxUi/DxUi.h','DxUi.Sdk.props','build/NativeBuildPolicy.targets','lib/x64/Release/DxUi.lib','lib/x64/Release/DxUi.pdb')){Set-FixtureFile $payload $file ('synthetic input '+$file)}
    $manifest=[ordered]@{schemaVersion=1;kind='sdk';source=$identity.sourceCommit;apiRevision=4;platform='x64';configuration='Release';runtime='MD';testsIncluded=$false;sdkCacheIdentity=$identity;
        librarySha256=(Get-FileHash (Join-Path $payload 'lib/x64/Release/DxUi.lib')).Hash.ToLowerInvariant();libraryPdbSha256=(Get-FileHash (Join-Path $payload 'lib/x64/Release/DxUi.pdb')).Hash.ToLowerInvariant()}
    Set-FixtureJson $payload sdk-manifest.json $manifest
    return @{payload=$payload;store=$store;manifest=$manifest;digest=(Get-DxUiSdkInputPayloadDigest -PayloadRoot $payload)}
}
Invoke-FixtureCase 'verified SDK bytes stay leased through input recheck; exact misses supply no bytes or verdict' {
    param($fixture)
    $f=New-SdkStoreFixture $fixture
    $published=Publish-DxUiSdkInputBytes -StoreRoot $f.store -StoreRepositoryRoot $fixture -PayloadRoot $f.payload -Identity $identity -ExpectedPayloadDigest (Get-DxUiSdkInputPayloadDigest -PayloadRoot $f.payload)
    $opened=Open-DxUiSdkInputBytes -Root $fixture -Platform x64 -StoreRoot $f.store -ManifestSha256 $published.ManifestSha256
    Assert-True ($null -ne $opened -and (Test-Path (Join-Path $opened.SdkRoot 'DxUi.Sdk.props'))) 'verified payload is available'
    Assert-Throws {Move-CiSdkCacheEntryToQuarantine -StoreRoot $f.store -Identity $identity -ManifestSha256 $published.ManifestSha256} 'writer cannot move an active SDK reader'
    Close-DxUiSdkInputBytes $opened
    Assert-Throws {Close-DxUiSdkInputBytes $opened} 'closed readers cannot qualify another operation'
    $miss=Open-DxUiSdkInputBytes -Root $fixture -Platform x64 -StoreRoot $f.store -ManifestSha256 ('1'*64)
    Assert-True ($null -eq $miss) 'an exact version miss returns no input bytes'
}
Invoke-FixtureCase 'source/profile/symbol/public-policy mismatches refuse SDK publication' {
    param($fixture)
    $f=New-SdkStoreFixture $fixture
    foreach($field in @('source','platform','runtime','testsIncluded','libraryPdbSha256')){
        $changed=Copy-JsonValue $f.manifest
        $changed[$field]=switch($field){source{'2'*40};platform{'ARM64'};runtime{'MT'};testsIncluded{$true};default{'2'*64}}
        Set-FixtureJson $f.payload sdk-manifest.json $changed
        Assert-Throws {Publish-DxUiSdkInputBytes -StoreRoot $f.store -StoreRepositoryRoot $fixture -PayloadRoot $f.payload -Identity $identity -ExpectedPayloadDigest (Get-DxUiSdkInputPayloadDigest -PayloadRoot $f.payload)} "unsupported payload $field refuses"
    }
    Set-FixtureJson $f.payload sdk-manifest.json $f.manifest
    [IO.File]::Move((Join-Path $f.payload 'DxUi.Sdk.props'),(Join-Path $f.payload 'preserved-policy.txt'))
    Assert-Throws {Publish-DxUiSdkInputBytes -StoreRoot $f.store -StoreRepositoryRoot $fixture -PayloadRoot $f.payload -Identity $identity -ExpectedPayloadDigest (Get-DxUiSdkInputPayloadDigest -PayloadRoot $f.payload)} 'missing public policy refuses'
    Assert-True (-not(Test-Path $f.store)) 'rejected payload does not initialize a store'
}
Invoke-FixtureCase 'stage bytes changed after the independent pre-archive snapshot cannot be published' {
    param($fixture)
    $f=New-SdkStoreFixture $fixture
    $header=Join-Path $f.payload 'include/DxUi/DxUi.h';$time=[IO.File]::GetLastWriteTimeUtc($header)
    $bytes=[IO.File]::ReadAllBytes($header);$bytes[0]=$bytes[0] -bxor 1;[IO.File]::WriteAllBytes($header,$bytes);[IO.File]::SetLastWriteTimeUtc($header,$time)
    Assert-Throws {Publish-DxUiSdkInputBytes -StoreRoot $f.store -StoreRepositoryRoot $fixture -PayloadRoot $f.payload -Identity $identity -ExpectedPayloadDigest $f.digest} 'late header change cannot inherit the original SDK smoke'
    Assert-True (-not(Test-Path $f.store)) 'unqualified changed stage did not create a cache entry'
}
Invoke-FixtureCase 'same-size/mtime byte changes fail open without returning SDK inputs' {
    param($fixture)
    $f=New-SdkStoreFixture $fixture
    $p=Publish-DxUiSdkInputBytes -StoreRoot $f.store -StoreRepositoryRoot $fixture -PayloadRoot $f.payload -Identity $identity -ExpectedPayloadDigest (Get-DxUiSdkInputPayloadDigest -PayloadRoot $f.payload)
    $file=Join-Path $p.Payload 'lib/x64/Release/DxUi.lib';$time=[IO.File]::GetLastWriteTimeUtc($file);$bytes=[IO.File]::ReadAllBytes($file);$bytes[0]=$bytes[0] -bxor 1;[IO.File]::WriteAllBytes($file,$bytes);[IO.File]::SetLastWriteTimeUtc($file,$time)
    Assert-Throws {Open-DxUiSdkInputBytes -Root $fixture -Platform x64 -StoreRoot $f.store -ManifestSha256 $p.ManifestSha256} 'raw byte corruption cannot become a hit'
}
Invoke-FixtureCase 'changed source at close preserves failure and releases the immutable reader lease' {
    param($fixture)
    $f=New-SdkStoreFixture $fixture
    $p=Publish-DxUiSdkInputBytes -StoreRoot $f.store -StoreRepositoryRoot $fixture -PayloadRoot $f.payload -Identity $identity -ExpectedPayloadDigest (Get-DxUiSdkInputPayloadDigest -PayloadRoot $f.payload)
    $opened=Open-DxUiSdkInputBytes -Root $fixture -Platform x64 -StoreRoot $f.store -ManifestSha256 $p.ManifestSha256
    $changed=Copy-JsonValue $identity;$changed.sourceBytes='9'*64
    & $storeModule {param($i) $script:fixtureIdentity=$i} $changed
    try{Assert-Throws {Close-DxUiSdkInputBytes $opened} 'source mutation fails use acceptance'}finally{& $storeModule {param($i) $script:fixtureIdentity=$i} $identity}
    $moved=Move-CiSdkCacheEntryToQuarantine -StoreRoot $f.store -Identity $identity -ManifestSha256 $p.ManifestSha256
    Assert-True (Test-Path $moved) 'failed close released its reader lease without deleting bytes'
}
Remove-Module $storeModule
Complete-TestRun 'SDK input byte store'
