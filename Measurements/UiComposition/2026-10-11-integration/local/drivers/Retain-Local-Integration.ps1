[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$target = Join-Path $PSScriptRoot 'retained-local'
if (Test-Path -LiteralPath $target) { throw 'Preserve the previous retained packet.' }
New-Item -ItemType Directory -Path $target | Out-Null
$bindings = [Collections.Generic.List[object]]::new()
$map = [ordered]@{
    'negative-control'='negative';
    'positive-debug'='first-positive';
    'positive-debug-event-diagnostics'='payload-probe';
    'positive-debug-final'='gated-positive';
    'isolated-form-idempotence'='form-repeat';
    'form-diagnostics-missing-events'='form-arrivals';
    'form-diagnostics-owner-queue'='form-owner';
    'form-diagnostics-delivery-stages'='form-worker';
    'form-diagnostics-late-delivery'='form-late';
    'form-diagnostics-message-wait'='form-wake';
    'qualified-debug-final'='before-retry-debug';
    'qualified-release-final'='before-retry-release';
    'qualified-asan-final'='before-retry-asan';
    'focus-integration-Debug-attempt1'='before-retry-focus-debug';
    'focus-integration-Release-attempt1'='before-retry-focus-release';
    'focus-integration-ASanDebug-attempt1'='focus-asan-failed';
    'input-diagnostic-undo-isolated'='input-undo-repeat';
    'focus-integration-ASanDebug-attempt2'='focus-asan-failed2';
    'input-diagnostic-undo-attempts'='input-activation-timeout';
    'input-diagnostic-activation-wait'='input-repeat-edit';
    'input-diagnostic-forced-loss-negative'='input-invalid-guard';
    'input-diagnostic-forced-loss-negative-2'='input-forced-negative';
    'input-diagnostic-forced-loss-positive'='input-forced-positive';
    'qualified-debug-ime-final'='final-debug';
    'qualified-release-ime-final'='final-release';
    'qualified-asan-ime-final'='final-asan';
    'focus-integration-Debug-attempt3'='focus-debug';
    'focus-integration-Release-attempt3'='focus-release';
    'focus-integration-ASanDebug-attempt3'='focus-asan'
}
foreach ($entry in $map.GetEnumerator()) {
    $source = Join-Path $PSScriptRoot $entry.Key
    if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw "Missing required packet: $source" }
    $files = @(Get-ChildItem -LiteralPath $source -File)
    if (-not $files.Count) { throw "Empty packet: $source" }
    $destination = Join-Path $target $entry.Value
    New-Item -ItemType Directory -Path $destination | Out-Null
    foreach ($file in $files) {
        $relative = $entry.Value + '/' + $file.Name
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        $copy = Join-Path $target $relative
        Copy-Item -LiteralPath $file.FullName -Destination $copy
        if ((Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash -cne $hash -or
            (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -cne $hash -or (Get-Item -LiteralPath $copy).Length -ne $file.Length) {
            throw "Changed retained bytes: $relative"
        }
        $bindings.Add([ordered]@{original=$file.FullName; copy=$relative; bytes=$file.Length; sha256=$hash})
    }
}
$scripts = @('Invoke-NegativeControl.ps1','Invoke-PositiveDebug.ps1','Invoke-PositiveDebugEventDiagnostics.ps1',
    'Invoke-PositiveDebugFinal.ps1','Invoke-FormDiagnostics.ps1','Invoke-Integration-Focused.ps1','Invoke-Integration-Focused-Native.ps1',
    'Invoke-Integration-Focus.ps1','Invoke-Integration-Focus-Native.ps1','Snapshot-Integration-Focus.ps1','Invoke-InputDiagnostic.ps1',
    'Invoke-InputFullDiagnostic.ps1','Invoke-InputCaseDiagnostic.ps1','Invoke-Final-Local.ps1','Assert-Archival-Inputs.ps1','Retain-Local-Integration.ps1')
New-Item -ItemType Directory -Path (Join-Path $target 'drivers') | Out-Null
foreach ($name in $scripts) {
    $source = Join-Path $PSScriptRoot $name
    $relative = 'drivers/' + $name
    $hash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    Copy-Item -LiteralPath $source -Destination (Join-Path $target $relative)
    if ((Get-FileHash -LiteralPath (Join-Path $target $relative) -Algorithm SHA256).Hash -cne $hash) { throw "Changed driver bytes: $name" }
    $bindings.Add([ordered]@{original=$source; copy=$relative; bytes=(Get-Item -LiteralPath $source).Length; sha256=$hash})
}
$negativeInput = Join-Path $PSScriptRoot 'original-integrated-accessibility.cpp'
if ((Get-FileHash -LiteralPath $negativeInput -Algorithm SHA256).Hash -cne '54E16DE73B9DF580E1AC4F15355EDA744605FFF2B3283D89EEBD25DC9ECE2D27') { throw 'Original negative production bytes changed.' }
Copy-Item -LiteralPath $negativeInput -Destination (Join-Path $target 'negative/original-accessibility.cpp')
$bindings.Add([ordered]@{original=$negativeInput; copy='negative/original-accessibility.cpp'; bytes=(Get-Item -LiteralPath $negativeInput).Length;
    sha256=(Get-FileHash -LiteralPath $negativeInput -Algorithm SHA256).Hash})
$imeNegative = Join-Path $PSScriptRoot 'forced-ime-retry-negative.cpp'
if ((Get-FileHash -LiteralPath $imeNegative -Algorithm SHA256).Hash -cne 'BF725CAC98BC5282AB2D1BF0F83DA318F1C1F7E40FAE85E8CC5D4CC5902BCB5B') { throw 'Forced IME negative source bytes changed.' }
Copy-Item -LiteralPath $imeNegative -Destination (Join-Path $target 'input-forced-negative/native-text-input.cpp')
$bindings.Add([ordered]@{original=$imeNegative; copy='input-forced-negative/native-text-input.cpp'; bytes=(Get-Item -LiteralPath $imeNegative).Length;
    sha256=(Get-FileHash -LiteralPath $imeNegative -Algorithm SHA256).Hash})
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
foreach ($pair in @(@('.build/reports/AddressSanitizer-x64.json','final-asan/detection.receipt.txt'),
                    @('.build/logs/test-AddressSanitizer-x64.log','final-asan/detection.log'))) {
    $source = Join-Path $root $pair[0]
    $hash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    Copy-Item -LiteralPath $source -Destination (Join-Path $target $pair[1])
    if ((Get-FileHash -LiteralPath (Join-Path $target $pair[1]) -Algorithm SHA256).Hash -cne $hash) { throw 'Changed ASan detection bytes.' }
    $bindings.Add([ordered]@{original=$source; copy=$pair[1]; bytes=(Get-Item -LiteralPath $source).Length; sha256=$hash})
}
[IO.File]::WriteAllText((Join-Path $target 'copy.receipt.txt'), ((ConvertTo-Json -InputObject $bindings.ToArray() -Depth 12)+[char]10), [Text.UTF8Encoding]::new($false))
Write-Output "Retained $($bindings.Count) byte-verified original/copy bindings at $target."
