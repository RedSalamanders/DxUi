[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $AnalyzerPath,
    [Parameter(Mandatory)][string] $OutputDirectory,
    [string] $PacketSource = (Join-Path $PSScriptRoot 'reproduction'),
    [ValidateSet('Strengthened','Legacy')][string] $ExpectedBehavior = 'Strengthened'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$AnalyzerPath = [IO.Path]::GetFullPath($AnalyzerPath)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$PacketSource = [IO.Path]::GetFullPath($PacketSource)
if (-not (Test-Path -LiteralPath $AnalyzerPath -PathType Leaf) -or [IO.Path]::GetExtension($AnalyzerPath) -cne '.ps1') {
    throw 'Supply an existing PowerShell AnalyzerPath explicitly.'
}
if (-not (Test-Path -LiteralPath $PacketSource -PathType Container)) { throw 'Supply an existing PacketSource directory.' }
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Replay output must be fresh; prior cases are preserved.' }
foreach ($protectedDirectory in @($PacketSource, [IO.Path]::GetDirectoryName($AnalyzerPath))) {
    $protectedPath = $protectedDirectory.TrimEnd([char[]]'\/')
    $protectedPrefix = $protectedPath + [IO.Path]::DirectorySeparatorChar
    if ($OutputDirectory.Equals($protectedPath, [StringComparison]::OrdinalIgnoreCase) -or
        $OutputDirectory.StartsWith($protectedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Replay output must be outside the original packet and analyzer directory.'
    }
}
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$encoding = [Text.UTF8Encoding]::new($false)
$sourceFiles = @('plan.receipt.txt','completion.receipt.txt') + @(for ($index=1; $index -le 11; $index++) {
    $kind = if ($index -eq 11) { 'preceding-tree-prefix' } else { 'isolated' }
    $stem = 'uia-{0:d2}-{1}' -f $index, $kind
    "$stem.receipt.txt"; "$stem.log"
})
$sourceHashes = [ordered]@{}
foreach ($name in $sourceFiles) {
    $sourceHashes[$name] = (Get-FileHash -LiteralPath (Join-Path $PacketSource $name) -Algorithm SHA256).Hash
}
$analyzerHash = (Get-FileHash -LiteralPath $AnalyzerPath -Algorithm SHA256).Hash

function Change-Receipt([string] $CaseRoot, [int] $Run, [scriptblock] $Change) {
    $kind = if ($Run -eq 11) { 'preceding-tree-prefix' } else { 'isolated' }
    $path = Join-Path $CaseRoot ('uia-{0:d2}-{1}.receipt.txt' -f $Run,$kind)
    $receipt = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    & $Change $receipt
    [IO.File]::WriteAllText($path, ((ConvertTo-Json $receipt -Depth 32) + [char]10), $encoding)
}
function Change-Log([string] $CaseRoot, [int] $Run, [scriptblock] $Change, [bool] $Rebind = $true) {
    $kind = if ($Run -eq 11) { 'preceding-tree-prefix' } else { 'isolated' }
    $path = Join-Path $CaseRoot ('uia-{0:d2}-{1}.log' -f $Run,$kind)
    $lines = @(Get-Content -LiteralPath $path -Encoding UTF8)
    $changed = @(& $Change $lines)
    [IO.File]::WriteAllText($path, (($changed -join ([char]10)) + [char]10), $encoding)
    if ($Rebind) {
        $newHash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        Change-Receipt $CaseRoot $Run { param($receipt) $receipt.logSha256 = $newHash }
    }
}

$cases = @(
    [ordered]@{ name='original-packet'; accepted=$true; reason=''; mutate={ param($caseRoot) } },
    [ordered]@{ name='valid-entry-order-late-arrivals-and-in-flight'; accepted=$true; reason=''; mutate={
        param($caseRoot)
        Change-Log $caseRoot 1 {
            param($lines)
            foreach ($line in $lines) {
                if ($line -match '^\s*\[UIA arrival\] seq=([12]) index=') {
                    $sequence = 3-[int]$Matches[1]
                    $line = $line -replace 'seq=[12] index=', "seq=$sequence index="
                }
                $line
            }
        }
        Change-Log $caseRoot 1 {
            param($lines)
            $active = $false; $inserted = $false; $priorTick = 0L; $beforeTick = 0L; $ownerThread = 0L
            foreach ($line in $lines) {
                if ($line -match '^\s*\[UIA timeline\] step=1 .*cutoffTick=(\d+) ') { $priorTick = [long]$Matches[1] }
                if ($line -match '^\s*\[UIA provider attempt\].*thread=(\d+)$' -and $ownerThread -eq 0) { $ownerThread = [long]$Matches[1] }
                if ($line -match '^\s*\[UIA timeline\] begin step=2 ') { $active = $true }
                if ($line -match '^\s*\[UIA timeline\] step=2 .*action=(\d+)\.\.\d+$') { $beforeTick = [long]$Matches[1]-1 }
                if ($active -and $line -match '^\s*\[UIA provider attempts\] step=(\d+).*counts=(\d+)\.\.(\d+) stepFirst=(\d+) actionFirst=(\d+) ') {
                    $step = [int]$Matches[1]; $first = [int]$Matches[2]; $last = [int]$Matches[3]+2
                    $stepFirst = [int]$Matches[4]+2; $actionFirst = [int]$Matches[5]+2
                    if ($step -eq 2) { $stepFirst = $first+1 } else { $first += 2 }
                    $line = $line -replace 'counts=\d+\.\.\d+ stepFirst=\d+ actionFirst=\d+', "counts=$first..$last stepFirst=$stepFirst actionFirst=$actionFirst"
                } elseif ($active -and $line -match '^\s*\[UIA provider attempt\] seq=(\d+) ') {
                    $sequence = [int]$Matches[1]+2
                    $line = $line -replace 'seq=\d+ ', "seq=$sequence "
                }
                $line
                if ($active -and -not $inserted -and $line -match '^\s*\[UIA provider attempts\] step=2 ') {
                    "    [UIA provider attempt] seq=3 step=1 tick=$priorTick phase=after thread=$ownerThread"
                    "    [UIA provider attempt] seq=4 step=2 tick=$beforeTick phase=before thread=$ownerThread"
                    $inserted = $true
                }
            }
            if (-not $inserted) { throw 'Replay fixture missed the step2 provider window.' }
        }
        Change-Log $caseRoot 2 {
            param($lines)
            $active = $false; $lateTick = 0L; $inserted = $false
            foreach ($line in $lines) {
                if ($line -match '^\s*\[UIA timeline\] begin step=15 ') { $active = $true }
                if ($active) {
                    if ($line -match '^\s*\[UIA timeline\] (?:begin |tail )?step=') {
                        $line = [Regex]::Replace($line, '(mark|cutoff)=(\d+)', {
                            param($m) $m.Groups[1].Value + '=' + ([int]$m.Groups[2].Value+1)
                        })
                    } elseif ($line -match '^\s*\[UIA stream\] phase=before .*ticks=(\d+)\.\.\d+.*counts=(\d+)\.\.(\d+)$' -and -not $inserted) {
                        $lateTick = [long]$Matches[1]
                        $first = [int]$Matches[2]; $last = [int]$Matches[3]+1
                        $line = $line -replace 'counts=\d+\.\.\d+', "counts=$first..$last"
                    } elseif ($line -match '^\s*\[UIA stream\].*counts=(\d+)\.\.(\d+)$') {
                        $first = [int]$Matches[1]+1; $last = [int]$Matches[2]+1
                        $line = $line -replace 'counts=\d+\.\.\d+', "counts=$first..$last"
                    } elseif ($line -match '^\s*\[UIA arrivals\]') {
                        $line = [Regex]::Replace($line, '(entries|completed)=(\d+)', {
                            param($m) $m.Groups[1].Value + '=' + ([int]$m.Groups[2].Value+1)
                        })
                    } elseif ($line -match '^\s*\[UIA arrival\] seq=(\d+) index=(\d+) ') {
                        $sequence = [int]$Matches[1]+1; $eventIndex = [int]$Matches[2]+1
                        $line = $line -replace 'seq=\d+ index=\d+ ', "seq=$sequence index=$eventIndex "
                    }
                }
                $line
                if ($active -and -not $inserted -and $line -match '^\s*\[UIA arrivals\]') {
                    "    [UIA arrival] seq=149 index=148 entered=$lateTick completed=$lateTick [event 20012 from DataItem 'Ligne 3']"
                    $inserted = $true
                }
            }
            if (-not $inserted) { throw 'Replay fixture missed the step15 snapshot.' }
        }
        Change-Log $caseRoot 3 {
            param($lines)
            $tail = $false; $changed = $false
            foreach ($line in $lines) {
                if ($line -match '^\s*\[UIA timeline\] tail ') { $tail = $true }
                if ($tail -and $line -match '^\s*\[UIA arrivals\] entries=(\d+) ') {
                    $entries = [long]$Matches[1]+1
                    $line = $line -replace 'entries=\d+ ', "entries=$entries "
                    $changed = $true
                }
                $line
            }
            if (-not $changed) { throw 'Replay fixture missed the tail snapshot.' }
        }
    }},
    [ordered]@{ name='child-timeout-disagrees-with-completion'; accepted=$false; reason='UIA_CHILD_TIMEOUT'; mutate={
        param($caseRoot) Change-Receipt $caseRoot 1 { param($receipt) $receipt.timedOut = $true }
    }},
    [ordered]@{ name='child-drain-error-disagrees-with-completion'; accepted=$false; reason='UIA_CHILD_DRAIN'; mutate={
        param($caseRoot) Change-Receipt $caseRoot 1 { param($receipt) $receipt.drainError = 'synthetic replay drain failure' }
    }},
    [ordered]@{ name='child-pipe-truncation'; accepted=$false; reason='UIA_CHILD_TRUNCATION'; mutate={
        param($caseRoot) Change-Receipt $caseRoot 1 { param($receipt) $receipt.stdoutTruncated = $true; $receipt.stderrTruncated = $true }
    }},
    [ordered]@{ name='wrong-prefix-arguments'; accepted=$false; reason='UIA_NATIVE_ARGUMENTS'; mutate={
        param($caseRoot) Change-Receipt $caseRoot 11 {
            param($receipt)
            $receipt.arguments = @('--suite=Accessibility','--no-activate','--test=TestSingleGridWindowRowAndCellEventsReachAClientSubscribedToTheWindow','--test-timeout=60')
        }
    }},
    [ordered]@{ name='missing-native-completion'; accepted=$false; reason='UIA_NATIVE_INVENTORY'; mutate={
        param($caseRoot) Change-Log $caseRoot 1 {
            param($lines)
            $lines | Where-Object { $_ -notmatch '^\s*\[DONE\] TestSingleGridWindowRowAndCellEventsReachAClientSubscribedToTheWindow ' }
        }
    }},
    [ordered]@{ name='wrong-step-expected-count'; accepted=$false; reason='UIA_STEP_EXPECTATION'; mutate={
        param($caseRoot) Change-Log $caseRoot 1 {
            param($lines)
            foreach ($line in $lines) {
                if ($line -match '^\s*\[UIA timeline\] (?:begin )?step=1 ') { $line = $line -replace 'expected=2', 'expected=3' }
                $line
            }
        }
    }},
    [ordered]@{ name='duplicate-callback-sequence'; accepted=$false; reason='UIA_CALLBACK_SEQUENCE'; mutate={
        param($caseRoot) Change-Log $caseRoot 1 {
            param($lines)
            foreach ($line in $lines) {
                [Regex]::Replace($line, '(\[UIA arrival\] )seq=1 index=', {
                    param($m) $m.Groups[1].Value + 'seq=2 index='
                })
            }
        }
    }},
    [ordered]@{ name='callback-completion-after-cutoff'; accepted=$false; reason='UIA_CALLBACK_CUTOFF'; mutate={
        param($caseRoot) Change-Log $caseRoot 1 {
            param($lines)
            $cutoff = 0L; $changed = $false
            foreach ($line in $lines) {
                if ($line -match '^\s*\[UIA timeline\] step=1 .*cutoffTick=(\d+) ') { $cutoff = [long]$Matches[1]+1 }
                if (-not $changed -and $line -match '^\s*\[UIA arrival\]') {
                    $line = $line -replace 'completed=\d+ ', "completed=$cutoff "
                    $changed = $true
                }
                $line
            }
        }
    }},
    [ordered]@{ name='quiet-interval-too-short'; accepted=$false; reason='UIA_QUIET_DURATION'; mutate={
        param($caseRoot) Change-Log $caseRoot 1 {
            param($lines)
            $changed = $false
            foreach ($line in $lines) {
                if (-not $changed -and $line -match '^\s*\[UIA stream\] phase=before .*ticks=(\d+)\.\.\d+') {
                    $first = [long]$Matches[1]; $last = $first+499
                    $line = $line -replace 'ticks=\d+\.\.\d+', "ticks=$first..$last"
                    $changed = $true
                }
                $line
            }
        }
    }},
    [ordered]@{ name='quiet-deadline-overrun'; accepted=$false; reason='UIA_QUIET_DEADLINE'; mutate={
        param($caseRoot) Change-Log $caseRoot 1 {
            param($lines)
            $changed = $false
            foreach ($line in $lines) {
                if (-not $changed -and $line -match '^\s*\[UIA stream\] phase=before .*ticks=\d+\.\.(\d+)') {
                    $last = [long]$Matches[1]; $first = $last-10001
                    $line = $line -replace 'ticks=\d+\.\.\d+', "ticks=$first..$last"
                    $line = $line -replace 'lastArrival=\d+', "lastArrival=$first"
                    $changed = $true
                }
                $line
            }
        }
    }},
    [ordered]@{ name='missing-stream-record'; accepted=$false; reason='UIA_STREAM_INVENTORY'; mutate={
        param($caseRoot) Change-Log $caseRoot 1 {
            param($lines)
            $removed = $false
            foreach ($line in $lines) {
                if (-not $removed -and $line -match '^\s*\[UIA stream\] phase=before ') { $removed = $true; continue }
                $line
            }
        }
    }},
    [ordered]@{ name='missing-provider-record'; accepted=$false; reason='UIA_PROVIDER_SEQUENCE'; mutate={
        param($caseRoot) Change-Log $caseRoot 1 {
            param($lines)
            $lines | Where-Object { $_ -notmatch '^\s*\[UIA provider attempt\] seq=1 ' }
        }
    }},
    [ordered]@{ name='unbound-log-byte-change'; accepted=$false; reason='UIA_LOG_HASH'; mutate={
        param($caseRoot) Change-Log $caseRoot 1 {
            param($lines) $lines; '[source-only replay: deliberately unbound log bytes]'
        } $false
    }}
)
if ($cases.Count -ne 15) { throw 'The replay inventory must be exactly fifteen cases.' }
$results = [Collections.Generic.List[object]]::new()
for ($index=0; $index -lt $cases.Count; $index++) {
    $case = $cases[$index]
    $caseDirectory = Join-Path $OutputDirectory ('{0:d2}-{1}' -f ($index+1), $case.name)
    New-Item -ItemType Directory -Path $caseDirectory | Out-Null
    foreach ($name in $sourceFiles) { Copy-Item -LiteralPath (Join-Path $PacketSource $name) -Destination (Join-Path $caseDirectory $name) }
    [IO.File]::WriteAllText((Join-Path $caseDirectory 'replay-description.receipt.txt'),
        ((ConvertTo-Json ([ordered]@{ purpose='synthetic-source-only-analyzer-replay'; case=$case.name; sourcePacket=$PacketSource;
            analyzer=$AnalyzerPath; analyzerSha256=$analyzerHash; expectedAccepted=$case.accepted; expectedReason=$case.reason }) -Depth 8) + [char]10), $encoding)
    & $case.mutate $caseDirectory
    # Seal each labeled synthetic fixture after mutation. The unbound-log case retains the stale
    # log hash inside its receipt, while this fixture manifest correctly binds all changed bytes.
    $manifestLines = @(foreach ($name in ($sourceFiles + @('replay-description.receipt.txt'))) {
        '{0}  {1}' -f (Get-FileHash -LiteralPath (Join-Path $caseDirectory $name) -Algorithm SHA256).Hash, $name
    })
    [IO.File]::WriteAllText((Join-Path $caseDirectory 'SHA256SUMS'), (($manifestLines -join ([char]10)) + [char]10), $encoding)
    $accepted = $false; $errorText = ''
    try {
        & $AnalyzerPath -PacketRoot $caseDirectory *> (Join-Path $caseDirectory 'analyzer.log')
        $accepted = $true
    } catch { $errorText = $_.Exception.Message }
    $acceptanceMatches = $accepted -eq $case.accepted
    $strictMatches = $acceptanceMatches
    if (-not $accepted -and -not $case.accepted) { $strictMatches = $errorText -match ('^\['+[Regex]::Escape($case.reason)+'\] ') }
    $legacyAccepted = $index -ne 14
    $legacyMatches = $accepted -eq $legacyAccepted
    if (-not $legacyAccepted -and -not $accepted) { $legacyMatches = $errorText -match '^Invalid UIA run:' }
    $observations = $null
    if ($accepted -and $index -eq 1 -and $ExpectedBehavior -ceq 'Strengthened') {
        $analysis = Get-Content -LiteralPath (Join-Path $caseDirectory 'analysis.txt') -Raw | ConvertFrom-Json
        $observations = [ordered]@{ betweenStepClientEvents=$analysis.runs[1].betweenStepClientEvents;
            unchangedClientEvents=$analysis.runs[1].unchangedClientEvents; inFlightCallbacks=$analysis.runs[2].inFlightCallbacks;
            timelineComplete=$analysis.runs[2].callbackTimelineComplete; diagnosticLoss=$analysis.runs[2].diagnosticLoss;
            priorStepProviderAttempts=$analysis.runs[0].betweenStepProviderAttempts;
            beforeActionProviderAttempts=$analysis.runs[0].beforeActionProviderAttempts; providerAttempts=$analysis.runs[0].providerAttempts }
        $strictMatches = $strictMatches -and $observations.betweenStepClientEvents -eq 1 -and $observations.unchangedClientEvents -eq 0 -and
            $observations.inFlightCallbacks -eq 1 -and -not $observations.timelineComplete -and $observations.diagnosticLoss -eq 0 -and
            $observations.priorStepProviderAttempts -eq 1 -and $observations.beforeActionProviderAttempts -eq 1 -and $observations.providerAttempts -eq 78
        if (-not $strictMatches) { $errorText = '[UIA_REPLAY_OBSERVATIONS] positive observation fields do not match the injected evidence.' }
    }
    $profileMatches = if ($ExpectedBehavior -ceq 'Strengthened') { $strictMatches } else { $legacyMatches }
    $results.Add([ordered]@{ index=$index+1; case=$case.name; directory=$caseDirectory; accepted=$accepted;
        expectedAccepted=$case.accepted; expectedReason=$case.reason; error=$errorText; acceptanceMatches=$acceptanceMatches; strictContractMatches=$strictMatches;
        expectedProfileMatches=$profileMatches; observations=$observations })
    Write-Host ('{0:d2} {1}: accepted={2}; acceptance={3}; strict={4}; profile={5}; {6}' -f ($index+1),$case.name,$accepted,$acceptanceMatches,$strictMatches,$profileMatches,$errorText)
}
$immutable = (Get-FileHash -LiteralPath $AnalyzerPath -Algorithm SHA256).Hash -ceq $analyzerHash
foreach ($name in $sourceFiles) {
    if ((Get-FileHash -LiteralPath (Join-Path $PacketSource $name) -Algorithm SHA256).Hash -cne $sourceHashes[$name]) { $immutable = $false }
}
$report = [ordered]@{ purpose='source-only-uia-analyzer-replay'; analyzer=$AnalyzerPath; analyzerSha256=$analyzerHash;
    sourcePacket=$PacketSource; sourceHashes=$sourceHashes; originalInputsUnchanged=$immutable; scheduled=15; attempted=$results.Count;
    expectedBehavior=$ExpectedBehavior; acceptanceMatches=@($results | Where-Object acceptanceMatches).Count;
    strictContractMatches=@($results | Where-Object strictContractMatches).Count;
    expectedProfileMatches=@($results | Where-Object expectedProfileMatches).Count; nativeProcessesLaunched=0;
    originalFailureCause='unproven'; results=$results.ToArray() }
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'replay-results.receipt.txt'), ((ConvertTo-Json $report -Depth 16) + [char]10), $encoding)
if (-not $immutable -or $report.expectedProfileMatches -ne 15) { throw 'Replay expectations failed; every external case and result is retained.' }
Write-Host "Completed exactly fifteen source-only cases: acceptance=$($report.acceptanceMatches)/15; strengthened contract/reasons=$($report.strictContractMatches)/15; expected profile=$($report.expectedProfileMatches)/15."
