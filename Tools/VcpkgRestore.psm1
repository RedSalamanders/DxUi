# vcpkg's checkout/bootstrap is shared; its install trees are platform-private.
Set-StrictMode -Version Latest

function Open-DxUiVcpkgFileLease {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path,[switch]$Shared,
          [ValidateRange(0,3600)][int]$TimeoutSeconds=900)
    $elapsed=[Diagnostics.Stopwatch]::StartNew()
    $wait=[Threading.ManualResetEventSlim]::new($false)
    $announced=$false
    try {
        while($true){
            try {
                $access=if($Shared){[IO.FileAccess]::Read}else{[IO.FileAccess]::ReadWrite}
                $share=if($Shared){[IO.FileShare]::Read}else{[IO.FileShare]::None}
                # Keep the lock file's identity stable. Delete-on-close permits a
                # waiter to open a different inode while another reader holds it.
                return [IO.File]::Open($Path,[IO.FileMode]::OpenOrCreate,$access,$share)
            }catch [IO.IOException]{
                if(($_.Exception.HResult -band 0xffff) -notin @(32,33)){throw}
                if($elapsed.Elapsed.TotalSeconds -ge $TimeoutSeconds){throw "Timed out waiting for the vcpkg lease '$Path'; its owner was not terminated."}
                if(-not $announced){Write-Host "Waiting for the vcpkg lease '$Path'.";$announced=$true}
                [void]$wait.Wait(100)
            }
        }
    }finally{$wait.Dispose()}
}

function Enter-DxUiVcpkgToolReadLease {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$BuildRoot,
          [Parameter(Mandatory)][scriptblock]$IsCurrent,
          [Parameter(Mandatory)][scriptblock]$Initialize,
          [ValidateRange(0,3600)][int]$TimeoutSeconds=900)
    $path=Join-Path $BuildRoot '.vcpkg-tool.lock'
    while($true){
        $reader=Open-DxUiVcpkgFileLease -Path $path -Shared -TimeoutSeconds $TimeoutSeconds
        $accepted=$false
        try {
            if(& $IsCurrent){$accepted=$true;return $reader}
        }finally{if(-not $accepted){$reader.Dispose()}}
        $writer=Open-DxUiVcpkgFileLease -Path $path -TimeoutSeconds $TimeoutSeconds
        try {
            # Another initializer may have completed while this writer waited.
            if(-not (& $IsCurrent)){& $Initialize | Out-Host}
            if(-not (& $IsCurrent)){throw 'vcpkg initialization did not establish the requested pinned tooling.'}
        }finally{$writer.Dispose()}
        # Reacquire and verify under a shared lease before use. No unprotected
        # gap between a successful identity check and the caller's install.
    }
}

function Enter-DxUiVcpkgPlatformLease {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$BuildRoot,
          [Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform,
          [ValidateRange(0,3600)][int]$TimeoutSeconds=900)
    Open-DxUiVcpkgFileLease -Path (Join-Path $BuildRoot ('.vcpkg-install.'+$Platform.ToLowerInvariant()+'.lock')) -TimeoutSeconds $TimeoutSeconds
}

Export-ModuleMember -Function Enter-DxUiVcpkgToolReadLease,Enter-DxUiVcpkgPlatformLease
