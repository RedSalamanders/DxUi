# Consumer-owned progress is deliberately separate from fixed top-level build timing:
# it must retain the active conformance case when an external CI watchdog ends the process.
# It never substitutes for the final ExternalConsumer receipt.
Set-StrictMode -Version Latest

if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT -and -not ('TestRunEvidenceV2.Native' -as [type])) {
    $script:evidenceModule = Import-Module -Name (Join-Path $PSScriptRoot 'TestRunEvidence.psm1') -PassThru -Verbose:$false
    & $script:evidenceModule { Initialize-TestRunEvidenceNative }
}

function Get-ConsumerProgressFileSnapshot([string] $Path) {
    $handle = [TestRunEvidenceV2.Native]::Pin($Path, $false)
    try {
        $identity = [TestRunEvidenceV2.Native]::Identity($handle)
        $stream = [IO.FileStream]::new($handle, [IO.FileAccess]::Read, 4096, $false)
        try {
            $sha = [Security.Cryptography.SHA256]::Create()
            try {
                $hash = [Convert]::ToHexString($sha.ComputeHash($stream))
                return [pscustomobject]@{ Path = $Path; Identity = $identity; Length = $stream.Length; Sha256 = $hash }
            } finally { $sha.Dispose() }
        } finally { $stream.Dispose() }
    } finally { if (-not $handle.IsClosed) { $handle.Dispose() } }
}

function Test-ConsumerProgressFileSnapshotEqual($Expected, $Actual) {
    return $Expected.Path -ceq $Actual.Path -and $Expected.Identity -ceq $Actual.Identity -and
        $Expected.Length -eq $Actual.Length -and $Expected.Sha256 -ceq $Actual.Sha256
}

function Assert-ConsumerProgressFilesUnchanged($ExpectedTarget, $ExpectedTemporary, [int] $Code, [int] $Attempt, [Exception] $Failure) {
    try {
        $targetNow = Get-ConsumerProgressFileSnapshot $ExpectedTarget.Path
        $temporaryNow = Get-ConsumerProgressFileSnapshot $ExpectedTemporary.Path
    } catch {
        Write-Verbose "DXUI_CONSUMER_PUBLISH_RETRY_REFUSED code=$Code attempt=$Attempt reason=identity-read-failed"
        throw [IO.IOException]::new("Consumer journal state could not be verified after replace error $Code; refusing retry.", $Failure)
    }
    if (-not (Test-ConsumerProgressFileSnapshotEqual $ExpectedTarget $targetNow) -or
        -not (Test-ConsumerProgressFileSnapshotEqual $ExpectedTemporary $temporaryNow)) {
        Write-Verbose "DXUI_CONSUMER_PUBLISH_RETRY_REFUSED code=$Code attempt=$Attempt reason=identity-or-content-changed"
        throw [IO.IOException]::new("Consumer journal or pending identity/content changed after replace error $Code; refusing retry.", $Failure)
    }
}

function Get-ConsumerReplaceWin32Code([Exception] $Exception) {
    $unsigned = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int] $Exception.HResult), 0)
    if (($unsigned -shr 16) -ne 0x8007) { return $null }
    return [int] ($unsigned -band 0xFFFF)
}

function Save-ConsumerQualificationProgress($Progress,[switch]$Initial) {
    $temporary=$Progress.Path+'.pending-'+[guid]::NewGuid().ToString('N')
    [IO.File]::WriteAllText($temporary,($Progress.Record|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
    if($Initial){[IO.File]::Move($temporary,$Progress.Path);return}
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        [IO.File]::Replace($temporary,$Progress.Path,[System.Management.Automation.Language.NullString]::Value)
        return
    }

    $maxAttempts = 3
    $maxElapsed = [TimeSpan]::FromMilliseconds(250)
    $retryDelayMs = 25
    # Retry only Win32 sharing violation (32) and ReplaceFile's non-destructive 1175;
    # both paths must still resolve to the same file IDs and bytes before another call.
    $retryableCodes = @(32, 1175)
    $targetBefore = Get-ConsumerProgressFileSnapshot $Progress.Path
    $temporaryBefore = Get-ConsumerProgressFileSnapshot $temporary
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $previousFailure = $null
    $previousCode = 0
    for ($attempt = 1; $attempt -le $maxAttempts; $attempt++) {
        if ($attempt -gt 1 -and $timer.Elapsed -ge $maxElapsed) { throw $previousFailure }
        if ($attempt -gt 1) {
            Assert-ConsumerProgressFilesUnchanged $targetBefore $temporaryBefore $previousCode ($attempt - 1) $previousFailure
            if ($timer.Elapsed -ge $maxElapsed) {
                Write-Verbose "DXUI_CONSUMER_PUBLISH_RETRY_REFUSED code=$previousCode attempt=$($attempt - 1) reason=deadline-after-identity elapsedMs=$([int] $timer.Elapsed.TotalMilliseconds)"
                throw $previousFailure
            }
        }
        try {
            [IO.File]::Replace($temporary,$Progress.Path,[System.Management.Automation.Language.NullString]::Value)
            if ($attempt -gt 1) { Write-Verbose "DXUI_CONSUMER_PUBLISH_RETRY_SUCCESS attempt=$attempt max=$maxAttempts" }
            return
        } catch {
            $failure = $_.Exception.GetBaseException()
            $code = Get-ConsumerReplaceWin32Code $failure
            if ($code -notin $retryableCodes -or $attempt -ge $maxAttempts -or $timer.Elapsed -ge $maxElapsed) { throw $failure }
            Assert-ConsumerProgressFilesUnchanged $targetBefore $temporaryBefore $code $attempt $failure

            if (($timer.Elapsed + [TimeSpan]::FromMilliseconds($retryDelayMs)) -ge $maxElapsed) { throw $failure }
            $previousFailure = $failure
            $previousCode = $code
            Write-Verbose "DXUI_CONSUMER_PUBLISH_RETRY code=$code attempt=$attempt max=$maxAttempts elapsedMs=$([int] $timer.Elapsed.TotalMilliseconds)"
            Start-Sleep -Milliseconds $retryDelayMs
        }
    }
}
function New-ConsumerQualificationProgress {
    [CmdletBinding()]param([Parameter(Mandatory)][string]$Path,[Parameter(Mandatory)][hashtable]$Context)
    if(Test-Path -LiteralPath $Path){throw 'Consumer progress must have a fresh owned path.'}
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $Path))
    $progress=[pscustomobject]@{Path=$Path;Owner=$PID;Record=[ordered]@{schema=1;suite='ConsumerProgress';status='incomplete';startedUtc=[DateTime]::UtcNow.ToString('o');context=$Context;activePhase=$null;negativeChecks=0;phases=[Collections.Generic.List[object]]::new()}}
    Save-ConsumerQualificationProgress $progress -Initial
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
