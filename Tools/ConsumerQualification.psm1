# Consumer-owned progress is deliberately separate from fixed top-level build timing:
# it must retain the active conformance case when an external CI watchdog ends the process.
# It never substitutes for the final ExternalConsumer receipt.
Set-StrictMode -Version Latest
function Save-ConsumerQualificationProgress($Progress) {
    $temporary=$Progress.Path+'.pending-'+[guid]::NewGuid().ToString('N')
    [IO.File]::WriteAllText($temporary,($Progress.Record|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
    [IO.File]::Move($temporary,$Progress.Path,$true)
}
function New-ConsumerQualificationProgress {
    [CmdletBinding()]param([Parameter(Mandatory)][string]$Path,[Parameter(Mandatory)][hashtable]$Context)
    if(Test-Path -LiteralPath $Path){throw 'Consumer progress must have a fresh owned path.'}
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $Path))
    $progress=[pscustomobject]@{Path=$Path;Owner=$PID;Record=[ordered]@{schema=1;suite='ConsumerProgress';status='incomplete';startedUtc=[DateTime]::UtcNow.ToString('o');context=$Context;activePhase=$null;negativeChecks=0;phases=[Collections.Generic.List[object]]::new()}}
    Save-ConsumerQualificationProgress $progress
    return $progress
}
function Invoke-ConsumerQualificationPhase {
    [CmdletBinding()]param([Parameter(Mandatory)]$Progress,[Parameter(Mandatory)][string]$Name,[Parameter(Mandatory)][scriptblock]$Body,[switch]$NegativeCheck)
    if($Progress.Owner-ne $PID -or $Progress.Record.status-ne 'incomplete' -or $Progress.Record.activePhase){throw 'Consumer progress ownership/state mismatch.'}
    if(@($Progress.Record.phases|Where-Object name -CEQ $Name).Count){throw 'A consumer phase cannot be counted twice.'}
    $phase=[ordered]@{name=$Name;state='running';startedUtc=[DateTime]::UtcNow.ToString('o');elapsedMs=0;negativeCheck=[bool]$NegativeCheck}
    $Progress.Record.phases.Add($phase);$Progress.Record.activePhase=$Name
    Save-ConsumerQualificationProgress $Progress
    Write-Host "[DXUI_CONSUMER_PHASE] phase=$Name state=BEGIN"
    $timer=[Diagnostics.Stopwatch]::StartNew();$passed=$false
    try{& $Body;$passed=$true}
    finally{
        $timer.Stop();$phase.elapsedMs=$timer.Elapsed.TotalMilliseconds;$phase.state=if($passed){'passed'}else{'failed'}
        if($passed){$Progress.Record.activePhase=$null}
        if($passed -and $NegativeCheck){$Progress.Record.negativeChecks++}
        # Preserve the original operation exception if its diagnostic write also fails.
        if($passed){Save-ConsumerQualificationProgress $Progress}
        else{try{Save-ConsumerQualificationProgress $Progress}catch{Write-Warning "Consumer progress write failed: $($_.Exception.Message)"}}
        Write-Host "[DXUI_CONSUMER_PHASE] phase=$Name state=$($phase.state) elapsedMs=$($phase.elapsedMs.ToString('F3',[Globalization.CultureInfo]::InvariantCulture))"
    }
}
function Assert-ConsumerQualificationChecks {
    [CmdletBinding()]param([Parameter(Mandatory)]$Progress)
    if($Progress.Owner-ne $PID -or $Progress.Record.status-ne 'incomplete' -or $Progress.Record.activePhase -or $Progress.Record.negativeChecks-ne 10 -or @($Progress.Record.phases|Where-Object state -NE 'passed').Count){throw 'Consumer progress cannot complete before all ten negative checks pass.'}
}
function Complete-ConsumerQualificationProgress {
    [CmdletBinding()]param([Parameter(Mandatory)]$Progress,[Parameter(Mandatory)][string]$FinalReceipt)
    Assert-ConsumerQualificationChecks $Progress
    $receipt=Get-Content -LiteralPath $FinalReceipt -Raw|ConvertFrom-Json
    if($receipt.suite-ne 'ExternalConsumer' -or $receipt.exitCode-ne 0 -or $receipt.negativeChecks-ne 10 -or $receipt.nativeModuleChecks-ne 12 -or $receipt.commit-cne $Progress.Record.context.commit){throw 'A genuine matching completed consumer receipt is required.'}
    $Progress.Record.status='completed';$Progress.Record.finalReceiptSha256=(Get-FileHash -LiteralPath $FinalReceipt -Algorithm SHA256).Hash
    Save-ConsumerQualificationProgress $Progress
}
Export-ModuleMember -Function New-ConsumerQualificationProgress,Invoke-ConsumerQualificationPhase,Assert-ConsumerQualificationChecks,Complete-ConsumerQualificationProgress
