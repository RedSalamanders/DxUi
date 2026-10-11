[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$source = 'bf11ba44fbd661fd5e1e6015f95d5719a4bfba83'
Import-Module (Join-Path $root 'Tools/PerformancePolicy.psm1') -Force
foreach ($configuration in @('Debug','Release','ASan Debug')) {
    $lower = switch ($configuration) { 'ASan Debug' { 'asan' } default { $configuration.ToLowerInvariant() } }
    $focused = Join-Path $PSScriptRoot "qualified-$lower-budget-final"
    $focusAttempt = if ($configuration -ceq 'Release') { 6 } else { 5 }
    $focus = Join-Path $PSScriptRoot "focus-integration-$($configuration.Replace(' ',''))-attempt$focusAttempt"
    $before = Get-Content -LiteralPath (Join-Path $focused 'before.state.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
    $after = Get-Content -LiteralPath (Join-Path $focused 'after.state.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
    $post = Get-Content -LiteralPath (Join-Path $focus 'post-focus.state.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
    Assert-PerformanceMeasurementState -Before $before -After $after
    Assert-PerformanceMeasurementState -Before $before -After $post
    if ($before.sourceCommit -cne $source -or $before.sourceDirty -or $before.configuration -cne $configuration -or
        $before.identityStatus -cne 'verifiable' -or $before.identityScope -cne 'full') { throw 'Wrong frozen native identity.' }
    $embedded = Join-Path $root ".build/x64/$configuration/DxUi.EmbeddedTests.exe"
    $control = Join-Path $root ".build/x64/$configuration/DxUi.ControlTests.exe"
    if ((Get-FileHash -LiteralPath $embedded).Hash -cne $before.executableSha256) { throw 'Embedded executable drift.' }
    $form = Get-Content -LiteralPath (Join-Path $focused 'form.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
    if ($form.exitCode -ne 0 -or @($form.skips).Count -ne 0 -or @($form.tests).Count -ne 14 -or
        (Get-FileHash -LiteralPath $control).Hash -cne $form.sha256) { throw 'Incomplete form validation or executable drift.' }
    $formLog = Get-Content -LiteralPath (Join-Path $focused 'form.log') -Raw
    foreach ($case in $form.tests) {
        if ($formLog -notmatch ('\[DONE\] '+[regex]::Escape($case)+' \(')) { throw "Selected form case did not complete: $case" }
    }
    if ($formLog -match '(?m)^(SKIPPED:|TIMEOUT:)' -or $formLog -notmatch '(?m)^All DxUi tests passed\.\r?$') { throw 'Selected form log lacks zero-skip completion.' }
    $embeddedLog = Get-Content -LiteralPath (Join-Path $focused 'embedded-events.log') -Raw
    if ($embeddedLog -notmatch '\[RELATIONSHIP DELIVERY\] intermediate=0' -or $embeddedLog -notmatch 'gateOpen=1') { throw 'Missing executed relationship/coalescing witnesses.' }
    $focusedProof = Get-Content -LiteralPath (Join-Path $focused 'root-review.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
    if ($focusedProof.embeddedExitCode -ne 0 -or $focusedProof.formExitCode -ne 0) { throw 'Embedded CLI or form runner failed.' }
    $lease = Get-Content -LiteralPath (Join-Path $focus 'interactive-result.txt') -Raw
    foreach ($line in @('state: completed','exit: 0','confirmation.autoStart: 1','confirmation.seconds: 30')) {
        if ($lease -notmatch ('(?m)^'+[regex]::Escape($line)+'\r?$')) { throw "Missing lease proof: $line" }
    }
    foreach ($part in @('foreground','focus','cursor')) {
        if ($lease -notmatch ('(?m)^restoration\.'+[regex]::Escape($part)+': (unchanged|restored)\r?$')) { throw "Missing successful desktop restoration: $part" }
    }
    if ($lease -notmatch '(?m)^confirmation: (started|autostarted)\r?$') { throw 'Missing admitted native confirmation.' }
    $confirmation = $Matches[1]
    foreach ($suite in @('Menu','NativeTextInput','MenuResources','MenuResourceScaling')) {
        $receipt = Get-Content -LiteralPath (Join-Path $focus "$suite.receipt.txt") -Raw | ConvertFrom-Json -AsHashtable -Depth 32
        if ($receipt.suite -cne $suite -or $receipt.configuration -cne $configuration -or $receipt.platform -cne 'x64' -or
            $receipt.exitCode -ne 0 -or @($receipt.skips).Count -ne 0 -or $receipt.Contains('tests') -or $receipt.sha256 -cne $form.sha256) { throw "Incomplete native suite: $configuration $suite" }
        if ($lease -notmatch ('(?m)^child: '+[regex]::Escape($suite)+' launched=1 exit=0 seconds=[0-9.]+ timedout=0 interrupted=0\r?$')) { throw "Missing native child completion: $suite" }
        if ((Get-Content -LiteralPath (Join-Path $focus "$suite.log") -Raw) -notmatch 'All DxUi tests passed\.') { throw "Missing native suite success log: $suite" }
    }
    $nativeLog = Get-Content -LiteralPath (Join-Path $focus 'NativeTextInput.log') -Raw
    foreach ($case in @('TestNativeImeUndoGroupingResetsDiscardedForegroundAttempt','TestNativeImeCommitOwnsOneUndoUnitAndLateResultPreservesApplicationText')) {
        if ($nativeLog -notmatch ('\[DONE\] '+[regex]::Escape($case))) { throw "IME regression did not complete: $case" }
    }
    foreach ($folder in @($focused,$focus)) {
        $report = Get-Content -LiteralPath (Join-Path $folder 'canonical-performance.json') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
        foreach ($key in $before.Keys) { if (($report[$key] | ConvertTo-Json -Depth 32 -Compress) -cne ($before[$key] | ConvertTo-Json -Depth 32 -Compress)) { throw "Canonical report identity differs: $key" } }
    }
    Write-Output "ROOT PASS ${configuration}: actual matching before/after/post-focus state, current EXE hashes, 14 forms, embedded delivery witnesses, four zero-skip full native suites and complete lease/IME witnesses; confirmation=$confirmation."
}
$probe = Get-Content -LiteralPath (Join-Path $root '.build/reports/AddressSanitizer-x64.json') -Raw | ConvertFrom-Json
if (-not $probe.detected -or $probe.exitCode -eq 0 -or (Get-FileHash -LiteralPath $probe.executable).Hash -cne $probe.sha256 -or
    (Get-Content -LiteralPath $probe.log -Raw) -notmatch 'AddressSanitizer: heap-use-after-free') { throw 'Deliberate sanitizer detection is unproved.' }
Write-Output 'ROOT PASS deliberate sanitizer detection matched the current ASan Foundation executable.'
