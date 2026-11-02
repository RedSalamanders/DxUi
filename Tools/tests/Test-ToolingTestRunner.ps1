<# .SYNOPSIS Verifies tooling roster exit, exception and stale-status handling. #>
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../ToolingTestRunner.psm1') -Force
Invoke-FixtureCase 'nonzero exit and throw are aggregated while later scripts still run' {
    param($root)
    Set-FixtureFile $root 'exit.ps1' '& (Get-Process -Id $PID).Path -NoProfile -Command ''exit 7'''
    Set-FixtureFile $root 'direct-exit.ps1' 'exit 9'
    Set-FixtureFile $root 'throw.ps1' 'throw "synthetic throw"'
    Set-FixtureFile $root 'later.ps1' '[IO.File]::WriteAllText((Join-Path $PSScriptRoot "later.txt"), "ran")'
    $failure = $null
    try { Invoke-DxUiToolingTestRoster @((Join-Path $root 'exit.ps1'), (Join-Path $root 'direct-exit.ps1'), (Join-Path $root 'throw.ps1'), (Join-Path $root 'later.ps1')) }
    catch { $failure = $_.Exception.Message }
    Assert-True ($failure -match 'exit\.ps1' -and $failure -match 'direct-exit\.ps1' -and $failure -match 'throw\.ps1' -and $failure -notmatch 'later\.ps1') 'both failures reported, later script not misreported'
    Assert-True (Test-Path (Join-Path $root 'later.txt')) 'later script ran after both failures'
}
Invoke-FixtureCase 'stale ambient and previous child exit status do not contaminate a clean script' {
    param($root)
    Set-FixtureFile $root 'clean.ps1' '$value = 42'
    $global:LASTEXITCODE = 19
    Invoke-DxUiToolingTestRoster @((Join-Path $root 'clean.ps1'))
    Assert-Equal 0 $global:LASTEXITCODE 'successful roster clears stale status'
}
Invoke-FixtureCase 'timeout stops only the owned child and continues the roster' {
    param($root)
    Set-FixtureFile $root 'slow.ps1' 'Start-Sleep -Seconds 30'
    Set-FixtureFile $root 'later.ps1' '[IO.File]::WriteAllText((Join-Path $PSScriptRoot "after-timeout.txt"), "ran")'
    $failure = $null
    try { Invoke-DxUiToolingTestRoster -Scripts @((Join-Path $root 'slow.ps1'), (Join-Path $root 'later.ps1')) -TimeoutSeconds 1 }
    catch { $failure = $_.Exception.Message }
    Assert-True ($failure -match 'slow\.ps1' -and (Test-Path (Join-Path $root 'after-timeout.txt'))) 'timeout fails the child and preserves the remaining roster'
}
Invoke-FixtureCase 'forced test support imports stay isolated between scripts' {
    param($root)
    $module = (Join-Path $PSScriptRoot 'TestSupport.psm1').Replace("'", "''")
    $script = "Import-Module '$module' -Force`nInvoke-FixtureCase 'child fixture' { param(`$childRoot) Set-FixtureFile `$childRoot 'proof.txt' 'ok' }`nComplete-TestRun 'child'`n"
    Set-FixtureFile $root 'first.ps1' $script
    Set-FixtureFile $root 'second.ps1' $script
    Invoke-DxUiToolingTestRoster @((Join-Path $root 'first.ps1'), (Join-Path $root 'second.ps1'))
    Assert-Equal 0 $global:LASTEXITCODE 'successive module imports both complete'
}
Complete-TestRun 'Tooling roster'
