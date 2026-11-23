<# .SYNOPSIS Captures the DxUi trace provider (input received, frame requested, prepare, present or composite, wakes) on
this machine with the inbox Windows tools.
.DESCRIPTION
Start begins a file-backed ETW session for the provider "DxUi" {89b12dac-c17b-5934-baa0-35b9f94990e6}; run the application
and reproduce the stall; Stop ends the session and decodes it to XML beside the .etl with tracerpt. The .etl also opens in
Windows Performance Analyzer. Starting a session needs the right to trace: an elevated prompt or membership in Performance
Log Users. The event schema is in docs/performance.md, "Trace provider".
.EXAMPLE
.\Tools\Capture-DxUiTrace.ps1 -Start
.\Tools\Capture-DxUiTrace.ps1 -Stop
#>
[CmdletBinding(DefaultParameterSetName = 'Start')] param(
    [Parameter(Mandatory, ParameterSetName = 'Start')][switch] $Start,
    [Parameter(Mandatory, ParameterSetName = 'Stop')][switch] $Stop,
    [string] $OutputPath = '',
    [string] $SessionName = 'DxUiTrace',
    [Parameter(ParameterSetName = 'Stop')][switch] $NoDecode
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TraceCapture.psm1') -Force
if (-not $OutputPath) { $OutputPath = Join-Path $PSScriptRoot "../.build/traces/$SessionName.etl" }
if ($Start) {
    $session = Start-DxUiTrace -EtlPath $OutputPath -SessionName $SessionName
    Write-Host "Tracing DxUi into $($session.EtlPath). Reproduce the issue, then run: .\Tools\Capture-DxUiTrace.ps1 -Stop -SessionName $SessionName -OutputPath '$($session.EtlPath)'"
} else {
    $result = Stop-DxUiTrace -EtlPath $OutputPath -SessionName $SessionName -NoDecode:$NoDecode
    Write-Host "Trace: $($result.EtlPath)"
    if ($result.XmlPath) { Write-Host "Decoded events: $($result.XmlPath)" }
}
exit 0
