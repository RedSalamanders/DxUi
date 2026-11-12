<# .SYNOPSIS Exercises SDK identity acquisition and mutation/refusal boundaries with synthetic inputs, without native compilation or cache acceptance. #>
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
$module=Import-Module (Join-Path $root 'Tools/SdkCacheInputs.psm1') -Force -PassThru
Invoke-FixtureCase 'public SDK acquisition uses every canonical guard and returns only strict input fields' {
    param($fixture)
    & $module {
        param($r)
        $script:fixtureRoot=$r;$script:calls=[Collections.Generic.List[string]]::new()
        $script:source=@{repository='RedSalamanders/DxUi';commit=('a'*40);contentIdentity=('b'*64)}
        $script:tooling=@{repository='RedSalamanders/DxUi';commit=('c'*40);contentIdentity=('d'*64)}
        $script:tools=@{identityStatus='build-only';buildIdentityVersion=2;sdkInputIdentity=('e'*64);toolchainIdentity=('f'*64);resolvedBuildIdentity=('0'*64)}
        $script:dependency='1'*64;$script:mutate=$false;$script:rejectProfile=$false;$script:sourceReads=0
        function script:Get-DxUiSdkToolingProvenance {param($Root) $script:calls.Add('provenance');if($Root -ceq $script:fixtureRoot){$script:sourceReads++;if($script:mutate -and $script:sourceReads -gt 1){$script:source.contentIdentity='2'*64};return $script:source.Clone()};return $script:tooling.Clone()}
        function script:Get-DxUiSdkSourceProfile {param($Root,$SourceCommit) $script:calls.Add('source-profile');return @{apiRevision=4}}
        function script:Assert-DxUiSdkReleaseProfile {param($Root,$Platform) $script:calls.Add('release-profile');if($script:rejectProfile){throw 'controlled incompatible profile'}}
        function script:Get-PerformanceToolIdentities {param($Root,$Platform,$Configuration,$IdentityScope) $script:calls.Add('complete-native-inputs');if($Configuration -cne 'Release' -or $IdentityScope -cne 'Build'){throw 'Wrong native acquisition role'};return $script:tools.Clone()}
        function script:Get-ScopedDependencyIdentity {param($Root,$Platform) $script:calls.Add('complete-dependency-bytes');return $script:dependency}
    } $fixture
    $identity=Get-DxUiSdkCacheInputIdentity -Root $fixture -Platform x64
    Assert-Equal 17 $identity.Count 'strict v1 input schema has no verdict or checkout-root fields'
    Assert-Equal 'DependencySdkInputs' $identity.role 'role is dependency input only'
    $calls=& $module {$script:calls.ToArray()}
    Assert-Equal 4 @($calls|Where-Object{$_ -ceq 'provenance'}).Count 'source/tooling have before and after barriers'
    foreach($name in @('source-profile','release-profile','complete-native-inputs','complete-dependency-bytes')){Assert-Equal 1 @($calls|Where-Object{$_ -ceq $name}).Count "canonical $name runs"}
    $schema=Join-Path $root '.ci-engine/Schemas/sdk-cache-identity.schema.json'
    Assert-True (Test-Json -Json ($identity|ConvertTo-Json -Compress) -SchemaFile $schema) 'shared store independently accepts strict identity'
}
Invoke-FixtureCase 'each actual SDK input family invalidates while old checkout-bound receipt identity stays separate' {
    param($fixture)
    $baseline=& $module {
        $script:sourceReads=0
        New-DxUiSdkCacheInputIdentity $script:source $script:tooling $script:tools x64 4 $script:dependency ('3'*64)
    }
    foreach($family in @('source-commit','source-bytes','tooling-commit','tooling-bytes','native-inputs','toolchain','dependency','environment','api','platform')){
        $changed=& $module {
            param($family)
            $s=$script:source.Clone();$p=$script:tooling.Clone();$t=$script:tools.Clone();$d=$script:dependency;$e='3'*64;$a=4;$platform='x64'
            switch($family){
                source-commit{$s.commit='4'*40};source-bytes{$s.contentIdentity='4'*64};tooling-commit{$p.commit='4'*40};tooling-bytes{$p.contentIdentity='4'*64}
                native-inputs{$t.sdkInputIdentity='4'*64};toolchain{$t.toolchainIdentity='4'*64};dependency{$d='4'*64};environment{$e='4'*64};api{$a=5};platform{$platform='ARM64'}
            }
            New-DxUiSdkCacheInputIdentity $s $p $t $platform $a $d $e
        } $family
        Assert-True (($baseline|ConvertTo-Json -Compress) -cne ($changed|ConvertTo-Json -Compress)) "SDK identity binds $family"
    }
    $same=& $module {$t=$script:tools.Clone();$t.resolvedBuildIdentity='9'*64;New-DxUiSdkCacheInputIdentity $script:source $script:tooling $t x64 4 $script:dependency ('3'*64)}
    Assert-Equal ($baseline|ConvertTo-Json -Compress) ($same|ConvertTo-Json -Compress) 'SDK projection does not adopt old checkout-bound build/test proof'
}
Invoke-FixtureCase 'missing canonical identity and unsupported roles cannot become SDK inputs' {
    param($fixture)
    foreach($mode in @('unverifiable','old-version','missing-profile','missing-toolchain','invalid-source','invalid-api','invalid-platform')){
        Assert-Throws {& $module {
            param($mode)
            $s=$script:source.Clone();$t=$script:tools.Clone();$a=4;$p='x64'
            switch($mode){unverifiable{$t.identityStatus='identity-unverifiable'};old-version{$t.buildIdentityVersion=1};missing-profile{$t.sdkInputIdentity=''};missing-toolchain{$t.toolchainIdentity=''};invalid-source{$s.commit='main'};invalid-api{$a=0};invalid-platform{$p='x86'}}
            New-DxUiSdkCacheInputIdentity $s $script:tooling $t $p $a $script:dependency ('3'*64)
        } $mode} "unsupported acquisition refuses: $mode"
    }
}
Invoke-FixtureCase 'public acquisition propagates profile refusal and source mutation instead of creating a usable key' {
    param($fixture)
    & $module {param($r) $script:fixtureRoot=$r;$script:sourceReads=0;$script:rejectProfile=$true} $fixture
    Assert-Throws {Get-DxUiSdkCacheInputIdentity -Root $fixture -Platform x64} 'profile refusal is fatal to reuse'
    & $module {$script:rejectProfile=$false;$script:mutate=$true;$script:sourceReads=0}
    Assert-Throws {Get-DxUiSdkCacheInputIdentity -Root $fixture -Platform x64} 'source mutation invalidates acquisition'
}
Remove-Module $module
Complete-TestRun 'SDK cache input acquisition'
