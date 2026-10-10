Set-StrictMode -Version Latest

function Get-DxUiCapabilitySkipEntries {
    [CmdletBinding()] param([Parameter(Mandatory)][string] $LogPath, [Parameter(Mandatory)][string] $Suite)
    # Reimporting with Force from this module removes the caller's exported reporting functions.
    Import-Module (Join-Path $PSScriptRoot 'SuiteFailure.psm1')
    $entries = [Collections.Generic.List[object]]::new()
    $items = Get-SuiteSkips -LogPath $LogPath
    foreach ($item in $items) {
        $parts = $item -split ': ', 2
        if ($parts.Count -ne 2) { throw "Malformed capability skip entry: $item" }
        $entries.Add([pscustomobject]@{ suite=$Suite; test=$parts[0]; reason=$parts[1] })
    }
    return $entries.ToArray()
}

function Get-DxUiUnexpectedCapabilitySkips {
    [CmdletBinding()] param([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][string] $Platform,
        [Parameter(Mandatory)][string] $Configuration, [Parameter(Mandatory)][AllowEmptyCollection()][object[]] $Skips)
    $profile = "$Platform|$Configuration"
    $manifest = Get-Content -LiteralPath (Join-Path $Root 'Tests/capability-skip-allowlist.json') -Raw | ConvertFrom-Json
    if ($manifest.version -ne 1 -or $null -eq $manifest.allowances) { throw 'Invalid capability skip allowlist.' }
    $lane = if ($env:RUNNER_ENVIRONMENT -ceq 'github-hosted' -and $env:RUNNER_OS -ceq 'Windows') { 'github-hosted-windows' } else { '' }
    $allowed = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($entry in $manifest.allowances) {
        if ($entry.lane -ceq $lane -and $profile -cin @($entry.profiles)) {
            [void]$allowed.Add("$($entry.suite)|$($entry.test)|$($entry.reason)")
        }
    }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $unexpected = [Collections.Generic.List[string]]::new()
    foreach ($skip in $Skips) {
        $key = "$($skip.suite)|$($skip.test)|$($skip.reason)"
        if (-not $seen.Add($key)) { $unexpected.Add("duplicate: $key") }
        elseif (-not $allowed.Contains($key)) { $unexpected.Add($key) }
    }
    return $unexpected.ToArray()
}

Export-ModuleMember -Function Get-DxUiCapabilitySkipEntries, Get-DxUiUnexpectedCapabilitySkips
