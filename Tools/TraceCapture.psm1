# Captures the DxUi trace provider with the inbox Windows tools (logman, tracerpt). See docs/performance.md, "Trace provider".
# Every external command runs through a runner script block (default: the real executable), so the tooling tests drive the
# whole flow with fakes and no session rights.
Set-StrictMode -Version Latest

$script:ProviderName = 'DxUi'
$script:ProviderGuid = '{89b12dac-c17b-5934-baa0-35b9f94990e6}'
# Input, frame and wake keywords; every event is written at level 4 (information).
$script:Keywords = '0x7'
$script:Level = '0x4'

function Get-DxUiTraceProvider {
    [CmdletBinding()] param()
    [pscustomobject]@{ Name = $script:ProviderName; Guid = $script:ProviderGuid; Keywords = $script:Keywords; Level = $script:Level }
}

function Invoke-DxUiTraceTool {
    [CmdletBinding()] param([Parameter(Mandatory)][string] $Tool, [Parameter(Mandatory)][string[]] $Arguments)
    $output = & $Tool @Arguments 2>&1 | Out-String
    [pscustomobject]@{ ExitCode = $LASTEXITCODE; Output = $output }
}

function Assert-DxUiTraceToolResult([object] $Result, [string] $What) {
    if ($Result.ExitCode -ne 0) { throw "$What failed (exit $($Result.ExitCode)): $($Result.Output.Trim())" }
}

function Assert-DxUiTraceSessionName([string] $SessionName) {
    if ($SessionName -notmatch '^[A-Za-z0-9_.-]{1,64}$') { throw "Invalid trace session name '$SessionName': use letters, digits, '.', '_' or '-'." }
}

# Starts a file-backed ETW session with the DxUi provider enabled. The session's clock is QueryPerformanceCounter, the clock
# of every event's Qpc field.
function Start-DxUiTrace {
    [CmdletBinding()] param(
        [Parameter(Mandatory)][string] $EtlPath,
        [string] $SessionName = 'DxUiTrace',
        [scriptblock] $Runner = { param($tool, $arguments) Invoke-DxUiTraceTool -Tool $tool -Arguments $arguments })
    Assert-DxUiTraceSessionName $SessionName
    $full = [IO.Path]::GetFullPath($EtlPath)
    if ([IO.Path]::GetPathRoot($full) -eq [IO.Path]::GetDirectoryName($full) -or -not [IO.Path]::GetDirectoryName($full)) {
        throw "Refusing to write a trace at a drive root: $full"
    }
    if (-not $full.EndsWith('.etl', [StringComparison]::OrdinalIgnoreCase)) { throw "The trace file must end in .etl: $full" }
    New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($full)) | Out-Null
    $arguments = @('start', $SessionName, '-ets', '-p', $script:ProviderGuid, $script:Keywords, $script:Level, '-o', $full, '-ct', 'perf')
    $result = & $Runner 'logman.exe' $arguments
    if ($result.ExitCode -ne 0 -and $result.Output -match '(?i)access is denied') {
        throw "Starting the ETW session '$SessionName' was refused: this desktop session lacks the right to trace (run elevated or as a member of Performance Log Users)."
    }
    Assert-DxUiTraceToolResult $result "Starting the ETW session '$SessionName'"
    [pscustomobject]@{ SessionName = $SessionName; EtlPath = $full }
}

# Stops the session and, unless told otherwise, decodes the trace to XML beside it with tracerpt.
function Stop-DxUiTrace {
    [CmdletBinding()] param(
        [Parameter(Mandatory)][string] $EtlPath,
        [string] $SessionName = 'DxUiTrace',
        [switch] $NoDecode,
        [scriptblock] $Runner = { param($tool, $arguments) Invoke-DxUiTraceTool -Tool $tool -Arguments $arguments })
    Assert-DxUiTraceSessionName $SessionName
    $full = [IO.Path]::GetFullPath($EtlPath)
    Assert-DxUiTraceToolResult (& $Runner 'logman.exe' @('stop', $SessionName, '-ets')) "Stopping the ETW session '$SessionName'"
    $xml = $null
    if (-not $NoDecode) {
        $xml = [IO.Path]::ChangeExtension($full, '.xml')
        Assert-DxUiTraceToolResult (& $Runner 'tracerpt.exe' @($full, '-o', $xml, '-of', 'XML', '-y')) "Decoding '$full'"
    }
    [pscustomobject]@{ SessionName = $SessionName; EtlPath = $full; XmlPath = $xml }
}

Export-ModuleMember -Function Get-DxUiTraceProvider, Invoke-DxUiTraceTool, Start-DxUiTrace, Stop-DxUiTrace
