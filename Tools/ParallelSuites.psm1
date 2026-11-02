Set-StrictMode -Version Latest

function Copy-DxUiSuiteLogText([string] $Path, [IO.StreamWriter] $Writer) {
    $Writer.Flush()
    $offset = $Writer.BaseStream.Position
    $utf8 = [Text.UTF8Encoding]::new($false, $true)
    $encoding = $utf8
    while ($true) {
        $reader = [IO.StreamReader]::new($Path, $encoding, $true)
        $last = -1
        try {
            $buffer = [char[]]::new(4096)
            while (($count = $reader.Read($buffer, 0, $buffer.Length)) -gt 0) {
                $Writer.Write($buffer, 0, $count)
                $last = [int]$buffer[$count - 1]
            }
            $Writer.Flush()
            return $last
        } catch [Text.DecoderFallbackException] {
            if (-not [object]::ReferenceEquals($encoding, $utf8) -or -not [Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Windows)) { throw }
            $Writer.Flush()
            $Writer.BaseStream.SetLength($offset)
            $Writer.BaseStream.Position = $offset
            $encoding = [Text.Encoding]::GetEncoding([Globalization.CultureInfo]::CurrentCulture.TextInfo.OEMCodePage)
        } finally { $reader.Dispose() }
    }
}

# Only launches the caller's explicit children. Each pipe drains directly to a
# separate file so a noisy suite cannot fill a pipe or retain its output in RAM.
function Invoke-DxUiParallelSuites {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][object[]] $Runs,
        [Parameter(Mandatory)][ValidateRange(1,8)][int] $Limit,
        [Parameter(Mandatory)][string] $WorkingDirectory
    )
    $active = [Collections.Generic.List[object]]::new()
    $results = [Collections.Generic.List[object]]::new()
    $next = 0
    try {
        while ($next -lt $Runs.Count -or $active.Count) {
            while ($next -lt $Runs.Count -and $active.Count -lt $Limit) {
                $run = $Runs[$next++]
                $start = [Diagnostics.ProcessStartInfo]::new([string]$run.Executable)
                $start.UseShellExecute = $false
                $start.CreateNoWindow = $true
                $start.WorkingDirectory = $WorkingDirectory
                $start.RedirectStandardOutput = $true
                $start.RedirectStandardError = $true
                foreach ($argument in $run.Arguments) { $start.ArgumentList.Add([string]$argument) }
                $stdoutPath = "$($run.Log).stdout"
                $stderrPath = "$($run.Log).stderr"
                $stdout = [IO.File]::Open($stdoutPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::Read)
                $stderr = $null
                $process = $null
                try {
                    $stderr = [IO.File]::Open($stderrPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::Read)
                    $process = [Diagnostics.Process]::Start($start)
                    $outCopy = $process.StandardOutput.BaseStream.CopyToAsync($stdout)
                    $errCopy = $process.StandardError.BaseStream.CopyToAsync($stderr)
                    $active.Add([pscustomobject]@{ Run=$run; Process=$process; Exit=$process.WaitForExitAsync();
                            Stdout=$stdout; Stderr=$stderr; OutCopy=$outCopy; ErrCopy=$errCopy; OutPath=$stdoutPath; ErrPath=$stderrPath })
                } catch {
                    if ($process) { if (-not $process.HasExited) { $process.Kill(); [void]$process.WaitForExit(5000) }; $process.Dispose() }
                    if ($stderr) { $stderr.Dispose() }
                    $stdout.Dispose()
                    throw
                }
            }
            $tasks = [Threading.Tasks.Task[]]@($active | ForEach-Object { $_.Exit })
            $completed = [Threading.Tasks.Task]::WhenAny($tasks).GetAwaiter().GetResult()
            $item = @($active | Where-Object { [object]::ReferenceEquals($_.Exit, $completed) })[0]
            [void]$item.Exit.GetAwaiter().GetResult()
            $completedUtc = [DateTime]::UtcNow.ToString('o')
            $drain = [Threading.Tasks.Task]::WhenAll([Threading.Tasks.Task[]]@($item.OutCopy, $item.ErrCopy))
            $done = [Threading.Tasks.Task]::WhenAny($drain, [Threading.Tasks.Task]::Delay(10000)).GetAwaiter().GetResult()
            if (-not [object]::ReferenceEquals($done, $drain)) { throw "Suite $($item.Run.Name) exited but its output pipes did not close." }
            [void]$drain.GetAwaiter().GetResult()
            $item.Stdout.Dispose(); $item.Stderr.Dispose()
            $destination = [IO.File]::Open($item.Run.Log, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::Read)
            $writer = [IO.StreamWriter]::new($destination, [Text.UTF8Encoding]::new($false), 4096, $true)
            try {
                $last = Copy-DxUiSuiteLogText -Path $item.OutPath -Writer $writer
                if ((Get-Item -LiteralPath $item.ErrPath).Length -gt 0 -and $last -ge 0 -and $last -ne 10 -and $last -ne 13) { $writer.Write("`n") }
                [void](Copy-DxUiSuiteLogText -Path $item.ErrPath -Writer $writer)
            } finally { $writer.Dispose(); $destination.Dispose() }
            $results.Add([pscustomobject]@{ Name=$item.Run.Name; ExitCode=$item.Process.ExitCode; CompletedUtc=$completedUtc })
            $item.Process.Dispose()
            [void]$active.Remove($item)
        }
        return $results.ToArray()
    } finally {
        $cleanupProblems = [Collections.Generic.List[string]]::new()
        foreach ($item in $active) {
            try {
                if (-not $item.Process.HasExited) {
                    $item.Process.Kill()
                    if (-not $item.Process.WaitForExit(5000)) { $cleanupProblems.Add("$($item.Run.Name) did not terminate") }
                }
            } catch { $cleanupProblems.Add("$($item.Run.Name): $($_.Exception.Message)") }
            finally { $item.Stdout.Dispose(); $item.Stderr.Dispose(); $item.Process.Dispose() }
        }
        if ($cleanupProblems.Count) { throw "Owned suite cleanup failed: $($cleanupProblems -join '; ')" }
    }
}
Export-ModuleMember -Function Invoke-DxUiParallelSuites
