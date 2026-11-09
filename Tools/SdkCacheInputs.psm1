# Acquisition of immutable SDK input identity; no cache mutation, compilation or validation verdict.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$script:performance=Import-Module (Join-Path $PSScriptRoot 'PerformancePolicy.psm1') -PassThru
$script:scoped=Import-Module (Join-Path $PSScriptRoot 'ScopedTesting.psm1') -PassThru
Import-Module (Join-Path $PSScriptRoot 'SdkSourceProfiles.psm1')
Import-Module (Join-Path $PSScriptRoot 'SdkLibraryInputs.psm1')

function New-DxUiSdkCacheInputIdentity {
    param([Collections.IDictionary]$Source,[Collections.IDictionary]$Tooling,[Collections.IDictionary]$Tools,
        [string]$Platform,[int]$ApiRevision,[string]$Dependency,[string]$Environment)
    if($Tools.identityStatus -cne 'build-only' -or $Tools['buildIdentityVersion'] -ne 2){throw 'SDK inputs require the complete canonical native build acquisition.'}
    foreach($value in @($Source.contentIdentity,$Tooling.contentIdentity,$Tools.sdkInputIdentity,$Tools.toolchainIdentity,$Dependency,$Environment)){
        if([string]$value -cnotmatch '^[A-Fa-f0-9]{64}$'){throw 'SDK inputs require complete source, profile, dependency, toolchain, tooling and environment hashes.'}
    }
    if($Source.repository -cne 'RedSalamanders/DxUi' -or $Tooling.repository -cne 'RedSalamanders/DxUi' -or
        $Source.commit -cnotmatch '^[a-f0-9]{40}$' -or $Tooling.commit -cnotmatch '^[a-f0-9]{40}$' -or
        $ApiRevision -lt 1 -or $ApiRevision -gt 65535 -or $Platform -cnotin @('x64','ARM64')){throw 'Unsupported SDK input source/tooling/profile.'}
    return [ordered]@{schemaVersion=1;role='DependencySdkInputs';sourceRepository='RedSalamanders/DxUi';sourceCommit=$Source.commit;
        sourceBytes=$Source.contentIdentity.ToLowerInvariant();apiRevision=$ApiRevision;platform=$Platform;configuration='Release';runtimeFamily='MD';
        annotationPolicy='enabled';languageStandard='stdcpplatest';optimizationPolicy='GL-LTCG-full';
        toolchainIdentity=$Tools.toolchainIdentity.ToLowerInvariant();compilerProfileIdentity=$Tools.sdkInputIdentity.ToLowerInvariant();
        dependencyIdentity=$Dependency.ToLowerInvariant();integrationPolicyIdentity=(Get-ScopedDigest ('dxui-sdk-input-policy-v1'+"`n"+($Tooling|ConvertTo-Json -Compress)));
        environmentIdentity=$Environment.ToLowerInvariant()}
}

function Get-DxUiSdkCacheInputIdentity {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform)
    $toolingRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
    $tooling=Get-DxUiSdkToolingProvenance -Root $toolingRoot
    $source=Get-DxUiSdkToolingProvenance -Root $Root
    $profile=Get-DxUiSdkSourceProfile -Root $Root -SourceCommit $source.commit
    Assert-DxUiSdkReleaseProfile -Root $Root -Platform $Platform
    $tools=Get-PerformanceToolIdentities -Root $Root -Platform $Platform -Configuration Release -IdentityScope Build
    $dependency=Get-ScopedDependencyIdentity -Root $Root -Platform $Platform
    # Machine name and graphics state belong to test evidence, not dependency compilation. Selected tools and all
    # compiler environment overrides (including PATH) are already byte-bound by the canonical toolchain identity.
    $environment=Get-ScopedDigest (@('dxui-sdk-build-environment-v1',[Environment]::OSVersion.VersionString,
        [Runtime.InteropServices.RuntimeInformation]::OSArchitecture,[Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture,
        [Environment]::Version.ToString(),$PSVersionTable.PSVersion.ToString()) -join "`n")
    $identity=New-DxUiSdkCacheInputIdentity -Source $source -Tooling $tooling -Tools $tools -Platform $Platform -ApiRevision $profile.apiRevision -Dependency $dependency -Environment $environment
    $sourceAfter=Get-DxUiSdkToolingProvenance -Root $Root
    $toolingAfter=Get-DxUiSdkToolingProvenance -Root $toolingRoot
    if(($source|ConvertTo-Json -Compress) -cne ($sourceAfter|ConvertTo-Json -Compress) -or
        ($tooling|ConvertTo-Json -Compress) -cne ($toolingAfter|ConvertTo-Json -Compress)){throw 'SDK source or producer tooling changed during input acquisition.'}
    return $identity
}

Export-ModuleMember -Function Get-DxUiSdkCacheInputIdentity
