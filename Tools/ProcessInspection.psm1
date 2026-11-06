# Read-only process inspection for exact build-output ownership. A process name
# is only a discovery filter; it never establishes ownership or permits killing.
function Open-DxUiObservedProcess([int]$ProcessId) {
    [Diagnostics.Process]::GetProcessById($ProcessId)
}
function Resolve-DxUiObservedProcessPath {
    <#
    .SYNOPSIS
    Resolves a CIM process image, tolerating an observation of a process that exited.
    .PARAMETER Observation
    A local Win32_Process observation with ProcessId and ExecutablePath properties.
    .PARAMETER DisplayName
    Diagnostic label only; never used as evidence of process ownership.
    .OUTPUTS
    Original image path, or no output for a confirmed exited process.
    .NOTES
    Reads process state only. An inaccessible live or uncertain process fails closed.
    The direct process instance is disposed; no process is ever terminated.
    #>
    [CmdletBinding()]
    param([Parameter(Mandatory)]$Observation,[string]$DisplayName='native executable')
    if ($Observation.ExecutablePath) { return [string]$Observation.ExecutablePath }
    $processId=[int]$Observation.ProcessId
    if ($processId -le 0) { throw "Cannot inspect running $DisplayName with an invalid PID; no process was terminated." }
    try { $live=Open-DxUiObservedProcess $processId }
    catch [ArgumentException] { return } # The local PID no longer exists.
    $path=$null
    $exited=$false
    $failure=$null
    try {
        try {
            $exited=$live.HasExited
            if ($exited -isnot [bool]) { throw [InvalidOperationException]::new('Process liveness is unavailable.') }
            if (-not $exited) {
                $path=$live.MainModule.FileName
                $exited=$live.HasExited
                if ($exited -isnot [bool]) { throw [InvalidOperationException]::new('Process liveness is unavailable.') }
            }
        } catch [System.Exception] {
            $path=$null
            $failure=$_.Exception
            # A module query can race termination. Only confirmed exit permits
            # ignoring the observation; failed liveness inspection still refuses.
            try { $exited=$live.HasExited; if ($exited -isnot [bool]) { $exited=$false } } catch [System.Exception] { $exited=$false }
        }
        if ($exited) { return }
        if (-not $path) {
            throw [InvalidOperationException]::new("Cannot inspect running $DisplayName (PID $processId); no process was terminated.",$failure)
        }
        return [string]$path
    } finally { $live.Dispose() }
}
Export-ModuleMember -Function Resolve-DxUiObservedProcessPath
