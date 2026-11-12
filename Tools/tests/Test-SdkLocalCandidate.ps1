<# .SYNOPSIS Tests local SDK capture admission and orchestration without native compilation or SDK acceptance. #>
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
$module=Import-Module (Join-Path $root 'Tools/SdkLocalCandidate.psm1') -Force -PassThru
Invoke-TestCase 'local snapshot entrypoint refuses hosted use before producer or engine acquisition' {
    $previous=[Environment]::GetEnvironmentVariable('GITHUB_ACTIONS','Process')
    try{
        $env:GITHUB_ACTIONS='true'
        $refused=$false
        try{New-DxUiLocalSdkSnapshot -ProducerRoot 'missing-producer-must-not-be-read' -Platform x64|Out-Null}
        catch{$refused=$_.Exception.Message -clike '*Local unpinned SDK capture is unavailable to hosted qualification and publication.*'}
        Assert-True $refused 'hosted refusal occurs before nonexistent producer/engine discovery'
    }finally{[Environment]::SetEnvironmentVariable('GITHUB_ACTIONS',$previous,'Process')}
}
Invoke-FixtureCase 'dirty tooling has a distinct raw identity and cannot become pinned source-build provenance' {
    param($fixture)
    $null=& git -C $fixture init -q -b main
    if($LASTEXITCODE){throw 'Fixture Git initialization failed.'}
    Set-FixtureFile $fixture '.gitignore' '.build/'
    Set-FixtureFile $fixture 'source.cpp' 'int value = 0;'
    $null=& git -C $fixture add --all
    $null=& git -C $fixture -c user.name=Fixture -c user.email=fixture@example.invalid -c commit.gpgsign=false -c core.hooksPath=.empty-hooks commit -qm 'Fixture baseline'
    if($LASTEXITCODE){throw 'Fixture baseline commit failed.'}
    $clean=& $module {param($r)Get-DxUiSdkToolingProvenance $r} $fixture
    Assert-Equal 3 $clean.Count 'the original clean pinned descriptor shape remains exact'
    Set-FixtureFile $fixture 'source.cpp' 'int value = 1;'
    Assert-Throws {& $module {param($r)Get-DxUiSdkToolingProvenance $r} $fixture} 'default pinned provenance still refuses dirty tooling'
    $local=& $module {param($r)Get-DxUiSdkToolingProvenance -Root $r -LocalCandidate} $fixture
    Assert-Equal 4 $local.Count 'local provenance has a separate shape'
    Assert-Equal 'LocalUnpinnedProducerTooling' $local.role 'dirty tooling remains explicitly local'
    Assert-True ($local.contentIdentity -cne $clean.contentIdentity) 'the actual dirty bytes change the identity'
    $libraryModule=Get-Module SdkLibraryInputs
    Assert-Throws {& $libraryModule {param($t)Assert-DxUiSdkProducerTooling $t} $local} 'local tooling cannot enter a pinned source-build descriptor'
}

# Exercise the actual public capture/storage/lease path. Only native profile/library/ABI
# acquisition is a recording boundary: these fixture files are deliberately not C++ artifacts.
$payloadModule=Import-Module (Join-Path $root 'Tools/SdkPayload.psm1') -PassThru
$abiModule=Import-Module (Join-Path $root '.ci-engine/Tools/Modules/Ci.SdkAbi.psm1') -PassThru
& $abiModule {
    function script:Get-CiSdkAbiReference {param($ToolingRoot,$MSBuildPath,$ProjectPath,$Platform)
        if($MSBuildPath -cne 'C:/synthetic-native-selected/MSBuild.exe'){throw 'Local capture bypassed native SDK MSBuild selection.'}
        return [ordered]@{schemaVersion=2;role='PortableSdkToolchainReference';compiler=@{vcToolsVersion='14.52.36430'};fixtureOnly=$true}
    }
}
$scoped=& $module {$script:scoped}
$exitBody=& $scoped {${function:Exit-ScopedProfileLease}}
& $module {
    param($payload,$exitBody)
    $script:payloadFixtureModule=$payload;$script:fixtureExit=$exitBody
    function script:Get-DxUiLocalSnapshotInputs {
        param($Root,$Platform,$SourceReader)
        $script:probe.inputReads++
        $head=(& git -C $Root rev-parse HEAD).Trim()
        $raw=& $SourceReader {param($r)Get-CiCheckoutContentIdentity $r} $Root
        $inputs=[ordered]@{schemaVersion=1;role='DependencySdkInputs';sourceRepository='RedSalamanders/DxUi';sourceCommit=$head;sourceBytes=$raw;apiRevision=5;
            platform=$Platform;configuration='Release';runtimeFamily='MD';annotationPolicy='enabled';languageStandard='stdcpplatest';optimizationPolicy='GL-LTCG-full';
            toolchainIdentity=('c'*64);compilerProfileIdentity=('d'*64);dependencyIdentity=('e'*64);integrationPolicyIdentity=('f'*64);environmentIdentity=('0'*64)}
        return [ordered]@{inputs=$inputs;tooling=@{repository='RedSalamanders/DxUi';commit=$head;contentIdentity=$raw;role='LocalUnpinnedProducerTooling'};
            toolingBootstrap=@{root=$Root;sourceBytes=$raw;engineInputsIdentity=('2'*64)};compilerVersion='14.52.36430'}
    }
    function script:Assert-DxUiSdkLibraryBuild {param($Root,$Platform)
        $script:probe.libraryReads++
        if($script:probe.rejectLibrary){throw 'Controlled stale or missing library provenance.'}
        return $script:probe.library
    }
    function script:Get-DxUiVisualStudioInstallation {return 'C:/synthetic-not-invoked-VisualStudio'}
    function script:Get-DxUiNativeMSBuild {param($Installation) return 'C:/synthetic-native-selected/MSBuild.exe'}
    function script:New-DxUiSdkPayload {
        [CmdletBinding()]param($Root,$Stage,$Platform,$SourceCommit,$CompilerVersion,$ExpectedLibraryHash,$ExpectedPdbHash)
        $arguments=$PSBoundParameters
        $script:probe.stage=$Stage
        $payload=& $script:payloadFixtureModule {param($b)New-DxUiSdkPayload @b} $arguments
        if($script:probe.mutate){[IO.File]::AppendAllText((Join-Path $Root 'source.cpp'),' changed while capturing')}
        return $payload
    }
    function script:Exit-ScopedProfileLease {param($Lease)& $script:fixtureExit $Lease;$script:probe.released=$true}
} $payloadModule $exitBody

foreach($scenario in @('capture','source-mutation','stale-library')){
    Invoke-FixtureCase "actual local capture $scenario preserves role, source barriers and lease order" {
        param($fixture)
        $null=& git -C $fixture init -q -b main
        foreach($file in @('include/DxUi/DxUi.h','Build/NativeBuildPolicy.targets','.build/x64/Release/DxUi.lib','.build/x64/Release/DxUi.pdb',
            '.build/vcpkg_installed/x64/x64-windows/include/wil/resource.h','.build/vcpkg_installed/x64/x64-windows/share/wil/copyright','source.cpp')){
            Set-FixtureFile $fixture $file ('synthetic capture control '+$file)
        }
        Set-FixtureFile $fixture '.gitignore' '.build/'
        $notices=Import-Module (Join-Path $root 'Tools/SdkNotices.psm1') -PassThru
        $noticeFiles=& $notices {Get-NoticeFiles}
        foreach($notice in $noticeFiles){
            if($notice.ContainsKey('Installed')){continue}
            $target=Join-Path $fixture $notice.Source
            [void][IO.Directory]::CreateDirectory((Split-Path $target))
            [IO.File]::Copy((Join-Path $root $notice.Source),$target,$false)
        }
        [IO.File]::Copy((Join-Path $root 'THIRD-PARTY-NOTICES.md'),(Join-Path $fixture 'THIRD-PARTY-NOTICES.md'),$false)
        $null=& git -C $fixture add --all
        $null=& git -C $fixture -c user.name=Fixture -c user.email=fixture@example.invalid -c commit.gpgsign=false -c core.hooksPath=.empty-hooks commit -qm 'Synthetic producer'
        if($LASTEXITCODE){throw 'Synthetic producer commit failed.'}
        [IO.File]::AppendAllText((Join-Path $fixture 'source.cpp'),' dirty built state')
        $files=@(foreach($name in @('DxUi.lib','DxUi.pdb')){
            $path=Join-Path $fixture ('.build/x64/Release/'+$name)
            @{path=$name;length=(Get-Item $path).Length;sha256=(Get-FileHash $path).Hash.ToLowerInvariant()}
        })
        $probe=@{released=$false;mutate=($scenario -ceq 'source-mutation');rejectLibrary=($scenario -ceq 'stale-library');inputReads=0;libraryReads=0;stage='';
            library=@{schemaVersion=1;role='ReleaseLibraryBuildInputs';files=$files;inputState=@{fixtureOnly=$true};digest=('1'*64)}}
        & $module {param($p)$script:probe=$p} $probe
        $store=Join-Path $root ('.build/local-sdk-fixture-store/'+[guid]::NewGuid().ToString('N'))
        $saved=[Environment]::GetEnvironmentVariable('GITHUB_ACTIONS','Process')
        try{
            $env:GITHUB_ACTIONS='false'
            if($scenario -ceq 'capture'){
                $result=New-DxUiLocalSdkSnapshot -ProducerRoot $fixture -Platform x64 -StoreRoot $store|ForEach-Object {
                    Assert-True $probe.released 'the producer lease is released before a downstream consumer receives output'
                    $_
                }
                $descriptor=Get-Content -LiteralPath $result.Descriptor -Raw|ConvertFrom-Json
                Assert-Equal 'LocalUnpinnedSdkSnapshot' $descriptor.role 'descriptor remains explicitly unpinned'
                Assert-Equal 'LocalUnpinnedProducerTooling' $descriptor.producerTooling.role 'tooling taint is retained'
                Assert-Equal 3 $probe.inputReads 'before, after capture and final complete input barriers run'
                Assert-Equal 3 $probe.libraryReads 'library provenance is rechecked through descriptor publication'
                Assert-True (Test-Path -LiteralPath (Join-Path $result.SdkRoot 'provenance/toolchain-reference.json')) 'the captured ABI reference is stored with the payload'
                Assert-True (-not $descriptor.PSObject.Properties['passed']) 'no test or qualification verdict is emitted'
            }else{
                Assert-Throws {New-DxUiLocalSdkSnapshot -ProducerRoot $fixture -Platform x64 -StoreRoot $store} 'changed source or stale library refuses capture'
                Assert-True $probe.released 'a failed capture releases the producer lease'
                Assert-True (-not(Test-Path -LiteralPath $store)) 'failed capture publishes no store entry or descriptor'
                if($scenario -ceq 'source-mutation'){Assert-True (Test-Path -LiteralPath $probe.stage) 'failed payload staging is preserved'}
            }
        }finally{[Environment]::SetEnvironmentVariable('GITHUB_ACTIONS',$saved,'Process')}
    }
}
Complete-TestRun -Suite 'Local SDK capture'
$global:LASTEXITCODE=0
