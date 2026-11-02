# Every script receives a fresh process: forced module imports and LASTEXITCODE
# cannot leak into another test. Output and failures are retained for every run.
function Invoke-DxUiToolingTestRoster {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string[]] $Scripts, [ValidateRange(1,3600)][int] $TimeoutSeconds = 1200)
    Set-StrictMode -Version Latest
    $ErrorActionPreference = 'Stop'
    $failed = [Collections.Generic.List[string]]::new()
    $shell = (Get-Process -Id $PID).Path
    $child = Join-Path $PSScriptRoot 'ToolingTestChild.ps1'
    foreach ($path in $Scripts) {
        $name = Split-Path -Leaf $path
        Write-Host "== $name"
        $process = $null
        try {
            $start = [Diagnostics.ProcessStartInfo]::new($shell)
            $start.UseShellExecute = $false
            $start.CreateNoWindow = $true
            $start.RedirectStandardOutput = $true
            $start.RedirectStandardError = $true
            foreach ($argument in @('-NoProfile','-File',$child,'-ScriptPath',$path)) { [void]$start.ArgumentList.Add($argument) }
            $process = [Diagnostics.Process]::Start($start)
            $stdout = $process.StandardOutput.ReadToEndAsync()
            $stderr = $process.StandardError.ReadToEndAsync()
            $timer = [Diagnostics.Stopwatch]::StartNew()
            while (-not $process.WaitForExit(1000) -and $timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {}
            if (-not $process.HasExited) {
                $process.Kill($true)
                if (-not $process.WaitForExit(5000)) { throw "Owned tooling child did not terminate: $name" }
                $failed.Add($name)
                Write-Host "$name timed out after $TimeoutSeconds seconds"
            } elseif ($process.ExitCode -ne 0) {
                $failed.Add($name)
                Write-Host "$name returned exit code $($process.ExitCode)"
            }
            if (-not $stdout.Wait(5000) -or -not $stderr.Wait(5000)) {
                $process.StandardOutput.Dispose(); $process.StandardError.Dispose()
                throw "Tooling child output did not close: $name"
            }
            $out = $stdout.GetAwaiter().GetResult()
            $err = $stderr.GetAwaiter().GetResult()
            if ($out) { Write-Host $out.TrimEnd() }
            if ($err) { Write-Host $err.TrimEnd() }
        } catch {
            if (-not $failed.Contains($name)) { $failed.Add($name) }
            Write-Host $_.Exception.Message
        } finally {
            if ($process) {
                if (-not $process.HasExited) { $process.Kill($true); [void]$process.WaitForExit(5000) }
                $process.Dispose()
            }
        }
    }
    if ($failed.Count) { throw "Tooling tests failed: $($failed -join ', ')" }
    $global:LASTEXITCODE = 0
}
Export-ModuleMember -Function Invoke-DxUiToolingTestRoster
