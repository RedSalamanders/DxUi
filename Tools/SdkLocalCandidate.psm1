# Explicit local Release candidate capture. No qualification or package/publication receipt is produced.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$script:scoped=Import-Module (Join-Path $PSScriptRoot 'ScopedTesting.psm1') -PassThru
$script:cacheInputs=Import-Module (Join-Path $PSScriptRoot 'SdkCacheInputs.psm1') -PassThru
Import-Module (Join-Path $PSScriptRoot 'SdkLibraryInputs.psm1')
$script:inputStore=Import-Module (Join-Path $PSScriptRoot 'SdkInputStore.psm1') -PassThru
Import-Module (Join-Path $PSScriptRoot 'SdkPayload.psm1')
Import-Module (Join-Path $PSScriptRoot 'VisualStudio.psm1')
Import-Module (Join-Path $PSScriptRoot 'SdkSourceProfiles.psm1')
Import-Module (Join-Path $PSScriptRoot 'PerformancePolicy.psm1')

function Get-DxUiLocalSnapshotInputs {
    param([string]$Root,[string]$Platform,[object]$SourceReader)
    $toolingRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
    $tooling=Get-DxUiSdkToolingProvenance -Root $toolingRoot -LocalCandidate
    $head=(& $script:scoped {param($r) Invoke-ScopedGit $r @('rev-parse','--verify','HEAD')} $Root).Trim()
    $raw=& $SourceReader {param($r) Get-CiCheckoutContentIdentity $r} $Root
    $bootstrap=& $SourceReader {param($r)
        [ordered]@{root=$r;sourceBytes=(Get-CiCheckoutContentIdentity $r);engineInputsIdentity=(Get-CiPinnedEngineInputsIdentity $r)}
    } $toolingRoot
    $source=[ordered]@{repository='RedSalamanders/DxUi';commit=$head;contentIdentity=$raw}
    $api=(Get-Content -LiteralPath (Join-Path $Root 'capabilities.json') -Raw|ConvertFrom-Json).apiRevision
    if(($api -isnot [long] -and $api -isnot [int]) -or $api -lt 1 -or $api -gt 65535){throw 'Local SDK capture requires the actual supported API revision.'}
    Assert-DxUiSdkReleaseProfile $Root $Platform
    $tools=Get-PerformanceToolIdentities -Root $Root -Platform $Platform -Configuration Release -IdentityScope Build
    $dependency=Get-ScopedDependencyIdentity -Root $Root -Platform $Platform
    $environment=Get-ScopedDigest (@('dxui-sdk-build-environment-v1',[Environment]::OSVersion.VersionString,
        [Runtime.InteropServices.RuntimeInformation]::OSArchitecture,[Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture,
        [Environment]::Version.ToString(),$PSVersionTable.PSVersion.ToString()) -join "`n")
    $inputs=& $script:cacheInputs {param($s,$t,$v,$p,$a,$d,$e)
        New-DxUiSdkCacheInputIdentity -Source $s -Tooling $t -Tools $v -Platform $p -ApiRevision $a -Dependency $d -Environment $e
    } $source $tooling $tools $Platform $api $dependency $environment
    $policy=& $script:cacheInputs {$script:performance}
    $compiler=& $policy {param($r,$p)
        Import-Module (Join-Path $PSScriptRoot 'VisualStudio.psm1')
        $installation=Get-DxUiVisualStudioInstallation
        $metadata=Get-PerformanceProjectMetadataCore -MSBuildPath (Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe') `
            -ProjectPath (Join-Path $r 'src/DxUi.vcxproj') -Configuration Release -Platform $p `
            -OutputRoot ((Join-Path $r '.build')+[IO.Path]::DirectorySeparatorChar) -Root $r -Role library
        return [string]$metadata.properties.VCToolsVersion
    } $Root $Platform
    return [ordered]@{inputs=$inputs;tooling=$tooling;toolingBootstrap=$bootstrap;compilerVersion=$compiler}
}

function New-DxUiLocalSdkSnapshot {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$ProducerRoot,[Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform,
        [string]$StoreRoot='')
    if($env:GITHUB_ACTIONS -ceq 'true'){throw 'Local unpinned SDK capture is unavailable to hosted qualification and publication.'}
    $toolingRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
    $root=[IO.Path]::GetFullPath($ProducerRoot).TrimEnd('\','/')
    # Verify the tooling engine before importing the separate local-only storage seam.
    $storage=& $script:inputStore {Import-DxUiSdkStoreEngine}
    $engineRoot=Join-Path $toolingRoot '.ci-engine'
    $snapshotModule=Import-Module (Join-Path $engineRoot 'Tools/Modules/Ci.LocalSdkSnapshot.psm1') -PassThru -ErrorAction Stop
    $sourceReader=Import-Module (Join-Path $engineRoot 'Tools/Modules/Ci.SourceInputs.psm1') -PassThru -ErrorAction Stop
    if(-not $StoreRoot){$StoreRoot=Join-Path $toolingRoot '.build/local-sdk-snapshots'}
    $store=Assert-DxUiSdkOwnedPath $toolingRoot $StoreRoot
    # Only the producer is held here. Consumers acquire their own write lease after capture returns.
    $lease=Enter-ScopedProfileLease $root $Platform Release
    $result=$null
    try{
        $before=Get-DxUiLocalSnapshotInputs $root $Platform $sourceReader
        $library=Assert-DxUiSdkLibraryBuild $root $Platform
        $files=@{};foreach($file in $library.files){$files[$file.path]=$file}
        $operation=Join-Path $toolingRoot ('.build/local-sdk-capture/'+[guid]::NewGuid().ToString('N'))
        $stage=Assert-DxUiSdkOwnedPath $toolingRoot (Join-Path $operation 'payload')
        # The reusable payload helper stages beneath its source owner. Keep a producer-owned
        # stage when capturing an independently edited checkout; storage copies it immutably.
        if(-not $root.Equals($toolingRoot,[StringComparison]::OrdinalIgnoreCase)){
            $stage=Assert-DxUiSdkOwnedPath $root (Join-Path $root ('.build/local-sdk-capture/'+[guid]::NewGuid().ToString('N')+'/payload'))
        }
        $payload=New-DxUiSdkPayload -Root $root -Stage $stage -Platform $Platform -SourceCommit $before.inputs.sourceCommit `
            -CompilerVersion $before.compilerVersion -ExpectedLibraryHash $files['DxUi.lib'].sha256 -ExpectedPdbHash $files['DxUi.pdb'].sha256
        $provenance=Join-Path $stage 'provenance/library-inputs.json'
        [void][IO.Directory]::CreateDirectory((Split-Path $provenance))
        [IO.File]::WriteAllText($provenance,($library|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
        $abi=Import-Module (Join-Path $engineRoot 'Tools/Modules/Ci.SdkAbi.psm1') -PassThru
        $installation=Get-DxUiVisualStudioInstallation
        $abiReference=& $abi {param($t,$m,$p,$a) Get-CiSdkAbiReference -ToolingRoot $t -MSBuildPath $m -ProjectPath $p -Platform $a} `
            $toolingRoot (Get-DxUiNativeMSBuild -Installation $installation) (Join-Path $root 'src/DxUi.vcxproj') $Platform
        if($abiReference.compiler.vcToolsVersion -cne $before.compilerVersion){throw 'Local SDK ABI reference disagrees with the actual evaluated Release compiler.'}
        [IO.File]::WriteAllText((Join-Path $stage 'provenance/toolchain-reference.json'),($abiReference|ConvertTo-Json -Depth 14),[Text.UTF8Encoding]::new($false))
        $catalog=@(& $storage {param($p) Get-CiSdkPayloadFiles $p} $stage)
        $bindings=[ordered]@{
            headers=@($catalog.path|Where-Object {$_ -clike 'include/*'});
            libraries=@("lib/$Platform/Release/DxUi.lib");symbols=@("lib/$Platform/Release/DxUi.pdb");
            integration=@('DxUi.Sdk.props','build/NativeBuildPolicy.targets');
            licenses=@($payload.Licenses.path);buildProvenance=@('provenance/library-inputs.json','provenance/toolchain-reference.json')
        }
        $identity=[ordered]@{schemaVersion=1;role='LocalUnpinnedSdkSnapshot';producerRoot=$root;inputs=$before.inputs;bindings=$bindings}
        $after=Get-DxUiLocalSnapshotInputs $root $Platform $sourceReader
        $libraryAfter=Assert-DxUiSdkLibraryBuild $root $Platform
        if(($before|ConvertTo-Json -Depth 12 -Compress) -cne ($after|ConvertTo-Json -Depth 12 -Compress) -or
            ($library|ConvertTo-Json -Depth 12 -Compress) -cne ($libraryAfter|ConvertTo-Json -Depth 12 -Compress)){throw 'Producer source/tooling/profile/library changed during local capture; staging was retained.'}
        & $storage {param($r,$s) Initialize-CiSdkStore $r $s} $toolingRoot $store
        $entry=& $snapshotModule {param($s,$i,$p) Publish-CiLocalSdkSnapshot $s $i $p} $store $identity $stage
        $final=Get-DxUiLocalSnapshotInputs $root $Platform $sourceReader
        $null=Assert-DxUiSdkLibraryBuild $root $Platform
        if(($before|ConvertTo-Json -Depth 12 -Compress) -cne ($final|ConvertTo-Json -Depth 12 -Compress)){throw 'Producer inputs changed before descriptor publication.'}
        $descriptor=[ordered]@{schemaVersion=1;role='LocalUnpinnedSdkSnapshot';storeRoot=$store;identity=$entry.Manifest.identity;
            manifestSha256=$entry.ManifestSha256;key=$entry.Key;producerTooling=$before.tooling;toolingBootstrap=$before.toolingBootstrap}
        $path=Assert-DxUiSdkOwnedPath $toolingRoot (Join-Path $operation 'local-unpinned-snapshot.json')
        [void][IO.Directory]::CreateDirectory((Split-Path $path))
        $stream=[IO.File]::Open($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
        try{$bytes=[Text.UTF8Encoding]::new($false).GetBytes(($descriptor|ConvertTo-Json -Depth 14));$stream.Write($bytes);$stream.Flush($true)}finally{$stream.Dispose()}
        $result=[pscustomobject]@{Descriptor=$path;Key=$entry.Key;ManifestSha256=$entry.ManifestSha256;SdkRoot=$entry.Payload;Role='LocalUnpinnedSdkSnapshot'}
    }finally{Exit-ScopedProfileLease $lease}
    return $result
}

Export-ModuleMember -Function New-DxUiLocalSdkSnapshot
