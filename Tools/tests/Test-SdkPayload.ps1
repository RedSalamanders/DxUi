<# .SYNOPSIS Tests shared SDK payload staging with synthetic libraries and a frozen pre-refactor integration contract. No native SDK qualification. #>
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $root 'Tools/SdkPayload.psm1') -Force
function New-PayloadFixture([string]$Fixture,[string]$Platform){
    $null=& git -C $Fixture init -q
    if($LASTEXITCODE){throw 'Fixture Git initialization failed.'}
    $triplet=if($Platform -ceq 'ARM64'){'arm64-windows'}else{'x64-windows'}
    foreach($file in @('include/DxUi/DxUi.h','Build/NativeBuildPolicy.targets',".build/$Platform/Release/DxUi.lib",".build/$Platform/Release/DxUi.pdb",
        ".build/vcpkg_installed/$Platform/$triplet/include/wil/resource.h",".build/vcpkg_installed/$Platform/$triplet/share/wil/copyright",'LICENSE.txt')){
        Set-FixtureFile $Fixture $file ('synthetic staging control '+$file)
    }
    Set-FixtureFile $Fixture 'THIRD-PARTY-NOTICES.md' '[WIL](docs/licenses/WIL.txt) and [source](vcpkg.json)'
    return @{
        Root=$Fixture;Stage=(Join-Path $Fixture '.build/stage');Platform=$Platform;
        SourceCommit='4cdf1ce52c7785446698e346b8aec058ed6a9283';CompilerVersion='14.52.36430';
        ExpectedLibraryHash=(Get-FileHash (Join-Path $Fixture ".build/$Platform/Release/DxUi.lib")).Hash.ToLowerInvariant();
        ExpectedPdbHash=(Get-FileHash (Join-Path $Fixture ".build/$Platform/Release/DxUi.pdb")).Hash.ToLowerInvariant()
    }
}
foreach($platform in @('x64','ARM64')){
    Invoke-FixtureCase "shared $platform payload preserves headers, symbols, licenses and exact original integration props" {
        param($fixture)
        $arguments=New-PayloadFixture $fixture $platform
        $payload=New-DxUiSdkPayload @arguments
        $golden=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'fixtures/SdkPayload/SDK.props.txt')).Replace('@PLATFORM@',$platform).Replace('@COMPILER@',$arguments.CompilerVersion)
        $actual=[IO.File]::ReadAllText((Join-Path $payload.Path 'DxUi.Sdk.props'))
        Assert-True ($actual -ceq $golden) 'Integration props are byte-identical to the frozen pre-refactor artifact'
        foreach($file in @('include/DxUi/DxUi.h','include/wil/resource.h',"lib/$platform/Release/DxUi.lib","lib/$platform/Release/DxUi.pdb",'build/NativeBuildPolicy.targets')){
            Assert-True (Test-Path -LiteralPath (Join-Path $payload.Path $file) -PathType Leaf) "complete payload contains $file"
        }
        Assert-True ($payload.Licenses.Count -eq 3) 'the exact reviewed API-4 dependency license set is retained'
        Assert-True ((Get-FileHash (Join-Path $payload.Path "lib/$platform/Release/DxUi.lib")).Hash.ToLowerInvariant() -ceq $arguments.ExpectedLibraryHash) 'library bytes match the independently acquired hash'
        Assert-True ((Get-FileHash (Join-Path $payload.Path "lib/$platform/Release/DxUi.pdb")).Hash.ToLowerInvariant() -ceq $arguments.ExpectedPdbHash) 'compiler PDB bytes match the independently acquired hash'
    }
}
Invoke-FixtureCase 'payload staging refuses foreign and occupied destinations before replacing data' {
    param($fixture)
    $arguments=New-PayloadFixture $fixture x64
    $arguments.Stage=Join-Path $fixture 'outside-build'
    Assert-Throws {New-DxUiSdkPayload @arguments} 'outside .build refuses'
    Assert-True (-not(Test-Path -LiteralPath $arguments.Stage)) 'refused destination was not created'
    $arguments.Stage=Join-Path $fixture '.build/occupied'
    Set-FixtureFile $arguments.Stage keep.txt 'existing data'
    Assert-Throws {New-DxUiSdkPayload @arguments} 'occupied destination refuses'
    Assert-True ([IO.File]::ReadAllText((Join-Path $arguments.Stage 'keep.txt')) -ceq 'existing data') 'existing bytes were preserved'
}
Invoke-FixtureCase 'payload staging retains a failed attempt and refuses symbol/hash mismatch' {
    param($fixture)
    $arguments=New-PayloadFixture $fixture x64
    $arguments.ExpectedPdbHash='0'*64
    Assert-Throws {New-DxUiSdkPayload @arguments} 'wrong compiler PDB bytes refuse'
    Assert-True (Test-Path -LiteralPath (Join-Path $arguments.Stage 'lib/x64/Release/DxUi.pdb')) 'failed staging remains available for diagnosis'
}
Complete-TestRun -Suite 'SDK payload'
$global:LASTEXITCODE=0
