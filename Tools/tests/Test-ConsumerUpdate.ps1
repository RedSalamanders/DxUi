# Deterministic advisory tests: no network, credentials, lock mutation or product checkout.
[CmdletBinding()] param()
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../ConsumerUpdate.psm1') -Force
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('DxUi-update-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
try {
    $lock = Join-Path $fixture 'DxUi.lock.json'
    $old = 'a' * 40
    $new = 'b' * 40
    @{repository='https://github.com/RedSalamanders/DxUi';commit=$old;apiRevision=2;targets=@('DxUi')} | ConvertTo-Json | Set-Content $lock
    $before = (Get-FileHash $lock).Hash
    foreach ($case in @('same','green','pending','failed','absent','divergent','ahead','offline','wrong-run','malformed')) {
        $calls = [Collections.Generic.List[string]]::new()
        $request = {
            param($route)
            $calls.Add($route)
            if ($case -eq 'offline') { throw 'private transport error must not escape' }
            if ($route -eq 'commits/main') {
                return @{sha=$(if ($case -eq 'same') {$old} elseif ($case -eq 'malformed') {'bad'} else {$new})}
            }
            if ($route.StartsWith('compare/')) {
                return @{status=$(if ($case -eq 'divergent') {'diverged'} elseif ($case -eq 'ahead') {'behind'} else {'ahead'})}
            }
            if ($case -eq 'absent') { return @{workflow_runs=@()} }
            return @{workflow_runs=@(@{
                head_sha=$(if($case -eq 'wrong-run'){$old}else{$new})
                status=$(if($case -eq 'pending'){'in_progress'}else{'completed'})
                conclusion=$(if($case -eq 'failed'){'failure'}else{'success'})
            })}
        }.GetNewClosure()
        $notice = Get-DxUiUpdateNotice -LockFile $lock -Request $request
        $expected = switch ($case) {
            same {''} green {'update available'} divergent {'diverge'} ahead {'ahead of main'}
            offline {'unavailable'} malformed {'unavailable'} default {'no successful completed validation'}
        }
        if (($case -eq 'same' -and $null -ne $notice) -or ($case -ne 'same' -and $notice -notlike "*$expected*")) {
            throw "Wrong advisory for ${case}: $notice"
        }
        if (@($notice).Count -gt 1 -or $calls.Count -gt 3 -or ($case -eq 'same' -and $calls.Count -ne 1)) { throw "Repeated lookup/notice for $case" }
        if ((Get-FileHash $lock).Hash -ne $before) { throw 'Advisory modified the exact pin.' }
        Write-Host "PASS DxUi update advisory: $case"
    }
} finally {
    # The only removed path is the exact unique directory created by this invocation.
    $resolved = [IO.Path]::GetFullPath($fixture)
    $parent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($parent, [StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($resolved) -notlike 'DxUi-update-*') { throw 'Unsafe fixture cleanup path.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
