<# .SYNOPSIS Records continuous host observations alongside a benchmark; never judges or filters benchmark samples. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $OutputPath,
    [ValidateRange(100,60000)][int] $IntervalMilliseconds = 1000,
    [ValidateRange(1,20000)][int] $MaximumSamples = 15000
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not $IsWindows) { throw 'Benchmark host observations require Windows.' }
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
$readyPath = $OutputPath + '.ready'
$stopPath = $OutputPath + '.stop'
$completionPath = $OutputPath + '.completion.json'
foreach ($path in @($OutputPath,$readyPath,$stopPath,$completionPath)) {
    if (Test-Path -LiteralPath $path) { throw "Preserve the existing host observation attempt: $path" }
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null

# GetSystemTimes includes idle time in kernel time. Preserve the original counters,
# rather than calculating a percentage over a nominal sampling interval. Process
# snapshots are observations at different instants, not scheduler or GPU traces.
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace DxUiBenchmarkHost {
    public static class Clock {
        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool GetSystemTimes(out ulong idle, out ulong kernel, out ulong user);
    }
}
'@
$writer = [IO.StreamWriter]::new($OutputPath,$false,[Text.UTF8Encoding]::new($false))
$writer.AutoFlush = $true
$count = 0
$reason = 'failed'
$errorText = ''
$started = [DateTime]::UtcNow
try {
    while ($count -lt $MaximumSamples) {
        $qpcStart = [Diagnostics.Stopwatch]::GetTimestamp()
        $idle = $kernel = $user = [ulong]0
        if (-not [DxUiBenchmarkHost.Clock]::GetSystemTimes([ref]$idle,[ref]$kernel,[ref]$user)) {
            throw [ComponentModel.Win32Exception]::new([Runtime.InteropServices.Marshal]::GetLastWin32Error())
        }
        $rows = [Collections.Generic.List[object]]::new()
        foreach ($process in [Diagnostics.Process]::GetProcesses()) {
            try {
                # Explicit getters surface access/exit errors; PowerShell's property
                # adapter can otherwise suppress a failing getter and return null.
                $cpuTime = $process.get_TotalProcessorTime()
                $rows.Add([ordered]@{ pid=$process.Id; name=$process.get_ProcessName();
                    cpuTicks=$cpuTime.Ticks; privateBytes=$process.get_PrivateMemorySize64();
                    workingSetBytes=$process.get_WorkingSet64(); status='observed' })
            } catch [ComponentModel.Win32Exception] {
                $rows.Add(@{ pid=$process.Id; status='inaccessible-or-exited' })
            } catch [InvalidOperationException] {
                $rows.Add(@{ pid=$process.Id; status='exited' })
            } finally { $process.Dispose() }
        }
        $writer.WriteLine((ConvertTo-Json -InputObject ([ordered]@{
            sequence=$count; utc=[DateTime]::UtcNow.ToString('o'); qpcStart=$qpcStart;
            qpcEnd=[Diagnostics.Stopwatch]::GetTimestamp(); qpcFrequency=[Diagnostics.Stopwatch]::Frequency;
            idle100ns=$idle; kernelIncludingIdle100ns=$kernel; user100ns=$user;
            observerPid=$PID; processes=$rows.ToArray()
        }) -Depth 5 -Compress))
        $count++
        if ($count -eq 1) { [IO.File]::WriteAllText($readyPath,$started.ToString('o')) }
        if (Test-Path -LiteralPath $stopPath) { $reason = 'stop-requested'; break }
        Start-Sleep -Milliseconds $IntervalMilliseconds
    }
    if ($reason -ne 'stop-requested') { throw 'Host observation sample budget exhausted; preserve this incomplete attempt.' }
} catch {
    $errorText = $_.Exception.Message
    throw
} finally {
    $writer.Dispose()
    [ordered]@{ startedUtc=$started.ToString('o'); completedUtc=[DateTime]::UtcNow.ToString('o');
        reason=$reason; error=$errorText; samples=$count; requestedIntervalMilliseconds=$IntervalMilliseconds;
        observerPid=$PID; sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash;
        limitation='Sample gaps and collection costs must be inspected; no dedicated-host, scheduler-delay or GPU-duration claim.'
    } | ConvertTo-Json | Set-Content -LiteralPath $completionPath -Encoding utf8
}
