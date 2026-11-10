<# .SYNOPSIS Exercises real file-sharing lease, initialization and platform isolation boundaries without native/network work. #>
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../VcpkgRestore.psm1') -Force
function Refuses([scriptblock]$Body){$failed=$false;try{& $Body|Out-Null}catch{$failed=$true};Assert-True $failed 'Expected a competing lease to refuse.'}
Invoke-FixtureCase 'cold initialization happens once; current readers share protection' {
    param($root)
    $state=@{current=$false;initializations=0}
    $check={$state.current};$initialize={$state.initializations++;$state.current=$true;'bootstrap stdout must not become a lease'}
    $first=Enter-DxUiVcpkgToolReadLease -BuildRoot $root -IsCurrent $check -Initialize $initialize -TimeoutSeconds 0
    $second=$null
    try{
        Assert-True ($first -is [IO.FileStream]) 'Initializer output contaminated the returned lease.'
        $second=Enter-DxUiVcpkgToolReadLease -BuildRoot $root -IsCurrent $check -Initialize $initialize -TimeoutSeconds 0
        Assert-True ($state.initializations -eq 1) 'Current readers repeated bootstrap.'
        Refuses { [IO.File]::Open((Join-Path $root '.vcpkg-tool.lock'),[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None) }
        Refuses { Enter-DxUiVcpkgToolReadLease -BuildRoot $root -IsCurrent {$false} -Initialize {throw 'Mutation entered while readers held tooling.'} -TimeoutSeconds 0 }
    }finally{if($second){$second.Dispose()};$first.Dispose()}
    $exclusive=[IO.File]::Open((Join-Path $root '.vcpkg-tool.lock'),[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    $exclusive.Dispose()
}
Invoke-FixtureCase 'initialization and verification failures release every lease' {
    param($root)
    Refuses {Enter-DxUiVcpkgToolReadLease -BuildRoot $root -IsCurrent {$false} -Initialize {throw 'controlled initialization failure'} -TimeoutSeconds 0}
    Refuses {Enter-DxUiVcpkgToolReadLease -BuildRoot $root -IsCurrent {$false} -Initialize {} -TimeoutSeconds 0}
    Refuses {Enter-DxUiVcpkgToolReadLease -BuildRoot $root -IsCurrent {throw 'controlled verification failure'} -Initialize {} -TimeoutSeconds 0}
    $reader=Enter-DxUiVcpkgToolReadLease -BuildRoot $root -IsCurrent {$true} -Initialize {throw 'Must not initialize'} -TimeoutSeconds 0
    $reader.Dispose()
}
Invoke-FixtureCase 'platform installs isolate same-platform contenders and allow disjoint roots/platforms' {
    param($root)
    $other=Join-Path $root 'other';[void][IO.Directory]::CreateDirectory($other)
    $x64=Enter-DxUiVcpkgPlatformLease -BuildRoot $root -Platform x64 -TimeoutSeconds 0
    $arm=$null;$independent=$null
    try{
        Refuses {Enter-DxUiVcpkgPlatformLease -BuildRoot $root -Platform x64 -TimeoutSeconds 0}
        $arm=Enter-DxUiVcpkgPlatformLease -BuildRoot $root -Platform ARM64 -TimeoutSeconds 0
        $independent=Enter-DxUiVcpkgPlatformLease -BuildRoot $other -Platform x64 -TimeoutSeconds 0
    }finally{if($independent){$independent.Dispose()};if($arm){$arm.Dispose()};$x64.Dispose()}
    Assert-True (Test-Path -LiteralPath (Join-Path $root '.vcpkg-install.x64.lock')) 'A released lease removed its stable identity.'
}
Complete-TestRun 'vcpkg restore coordination'
