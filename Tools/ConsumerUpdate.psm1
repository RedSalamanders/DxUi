# Shared, read-only advisory used once by a consumer's root restore/build entrypoint.
Set-StrictMode -Version Latest

function Get-DxUiUpdateNotice {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $LockFile,
        [Parameter(Mandatory)][scriptblock] $Request
    )
    try {
        $pin = Get-Content -LiteralPath $LockFile -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
        if ($pin.repository -cne 'https://github.com/RedSalamanders/DxUi' -or $pin.commit -cnotmatch '^[0-9a-f]{40}$') {
            throw 'Invalid canonical pin.'
        }
        $head = & $Request 'commits/main'
        $candidate = [string]$head.sha
        if ($candidate -cnotmatch '^[0-9a-f]{40}$') { throw 'Invalid upstream identity.' }
        if ($candidate -ceq $pin.commit) { return $null }
        $comparison = & $Request "compare/$($pin.commit)...$candidate"
        if ($comparison.status -eq 'behind') { return "DxUi pin is ahead of main; review $LockFile before any change." }
        if ($comparison.status -ne 'ahead') { return "DxUi pin and main diverge; review $LockFile before any change." }
        $runs = & $Request "actions/workflows/ci.yml/runs?head_sha=$candidate&branch=main&event=push&per_page=1"
        $latest = @($runs.workflow_runs)
        if ($latest.Count -ne 1 -or $latest[0].head_sha -cne $candidate -or
            $latest[0].status -ne 'completed' -or $latest[0].conclusion -ne 'success') {
            return "DxUi main $($candidate.Substring(0, 12)) has no successful completed validation; keep pinned $($pin.commit.Substring(0, 12))."
        }
        return "DxUi update available: pinned $($pin.commit.Substring(0, 12)), available $($candidate.Substring(0, 12)). https://github.com/RedSalamanders/DxUi/compare/$($pin.commit)...$candidate . Update $LockFile on a branch and run the product regressions."
    } catch [System.Exception] {
        # Advisory failure never changes the separate fail-closed source-restore result.
        # Do not echo transport exceptions: private-repository authentication must stay private.
        return 'DxUi update check unavailable; the exact pinned dependency remains selected.'
    }
}

function Show-DxUiUpdateNotice {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $LockFile)

    $token = $env:GH_TOKEN
    if (-not $token) { $token = $env:GITHUB_TOKEN }
    if (-not $token) {
        $gh = Get-Command gh -ErrorAction SilentlyContinue
        if ($gh) {
            # Bound this optional credential lookup too; terminate only our own helper on timeout.
            $start = [Diagnostics.ProcessStartInfo]::new()
            $start.FileName = $gh.Source
            $start.Arguments = 'auth token --hostname github.com'
            $start.UseShellExecute = $false
            $start.CreateNoWindow = $true
            $start.RedirectStandardOutput = $true
            $start.RedirectStandardError = $true
            $process = [Diagnostics.Process]::new()
            $process.StartInfo = $start
            try {
                if ($process.Start()) {
                    if ($process.WaitForExit(2000)) {
                        if ($process.ExitCode -eq 0) { $token = $process.StandardOutput.ReadToEnd().Trim() }
                    } else { $process.Kill() }
                }
            } catch [System.Exception] {
                # Missing/locked credentials are an advisory lookup failure, never a restore failure.
            } finally { $process.Dispose() }
        }
    }
    $headers = @{ Accept = 'application/vnd.github+json'; 'X-GitHub-Api-Version' = '2022-11-28' }
    if ($token) { $headers.Authorization = "Bearer $token" }
    $request = {
        param([string] $route)
        Invoke-RestMethod -Uri "https://api.github.com/repos/RedSalamanders/DxUi/$route" -Headers $headers -TimeoutSec 2 -ErrorAction Stop
    }.GetNewClosure()
    $notice = Get-DxUiUpdateNotice -LockFile $LockFile -Request $request
    if ($notice) { Write-Host "$notice (Checked $([DateTime]::UtcNow.ToString('u')))" }
}

Export-ModuleMember -Function Get-DxUiUpdateNotice, Show-DxUiUpdateNotice
