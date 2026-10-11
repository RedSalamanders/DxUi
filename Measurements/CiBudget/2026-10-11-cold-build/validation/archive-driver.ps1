Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$external = 'C:/Users/eric/.codex/tmp/dpf-s0-s2-merge'
$repo = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$archive = Join-Path $repo 'Measurements/CiBudget/2026-10-11-cold-build'
$sealPath = Join-Path $archive 'SHA256SUMS'
$utf8 = [Text.UTF8Encoding]::new($false)
foreach ($line in [IO.File]::ReadAllLines($sealPath)) {
    if ($line -cnotmatch '^([a-f0-9]{64})  (.+)$') { throw 'Malformed pre-validation archive seal.' }
    if (-not [string]::Equals((Get-FileHash -LiteralPath (Join-Path $archive $Matches[2])).Hash, $Matches[1], [StringComparison]::OrdinalIgnoreCase)) { throw 'Original archive changed.' }
}
if ((Get-FileHash -LiteralPath $sealPath).Hash -cne '71FE44804F11FB60C070844DFC58526426F3F8C8B87B1E88B053209C753D6445') { throw 'Independently reviewed pre-validation archive seal differs.' }
$full = Get-Content -LiteralPath (Join-Path $external 'native-cold-budget-validate-final.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable
if ($full['exitCode'] -ne 0 -or $full['inputsUnchanged'] -ne $true -or $full['afterIdentityEqualsBefore'] -ne $true -or (Get-FileHash -LiteralPath $full['log']).Hash -cne $full['logSha256']) { throw 'Full validation is not successful and immutable.' }
$fullLog = [IO.File]::ReadAllText($full['log'])
if ($fullLog -notmatch 'All validation passed\.' -or $fullLog -notmatch 'NativeScope: 14 of 14 passed' -or $fullLog -notmatch 'All tooling tests passed\.' -or $fullLog -match '(?m)^FAILED:') { throw 'Final validation log is not complete.' }
$copies = [Collections.Generic.List[object]]::new()
foreach ($entry in @(
    @{source='native-cold-budget-validate-final.log';name='WindowsFull.log'},
    @{source='native-cold-budget-validate-final.receipt.txt';name='WindowsFull.receipt.txt'},
    @{source='Run-ColdBudget-Validation.ps1';name='WindowsFull-driver.ps1'},
    @{source='native-cold-budget-validate.log';name='WindowsInitialFailed.log'},
    @{source='native-cold-budget-bootstrap.receipt.txt';name='BootstrapFailed.receipt.txt'},
    @{source='Run-ColdBudget-Validation.bootstrap.ps1';name='BootstrapFailed-driver.ps1'},
    @{source='native-cold-budget-linux-focused.log';name='LinuxScope.log'},
    @{source='native-cold-budget-linux-focused.receipt.txt';name='LinuxScope.receipt.txt'},
    @{source='Run-Linux-ColdBudgetFocused.ps1';name='LinuxScope-driver.ps1'},
    @{source='native-cold-budget-linux-focused.log.initial';name='LinuxInitial.log'},
    @{source='native-cold-budget-linux-focused.receipt.txt.initial';name='LinuxInitial.receipt.txt'},
    @{source='native-cold-budget-format.log';name='FormatCheck.log'},
    @{source='native-cold-budget-explain.log';name='AffectedExplain.log'},
    @{source='cold-budget-archive-before-shortening.SHA256SUMS';name='BeforeShorteningSeal.receipt.txt'},
    @{source='cold-budget-copy-before-shortening.receipt.txt';name='BeforeShorteningCopy.receipt.txt'}
)) {
    $source = Join-Path $external $entry.source
    $target = Join-Path $archive ('validation/' + $entry.name)
    if (Test-Path -LiteralPath $target) { throw 'Validation copy already exists.' }
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))
    Copy-Item -LiteralPath $source -Destination $target
    if ((Get-FileHash -LiteralPath $source).Hash -cne (Get-FileHash -LiteralPath $target).Hash) { throw 'Validation archive copy differs.' }
    $copies.Add([ordered]@{ original=$source; archive=('validation/' + $entry.name); bytes=(Get-Item -LiteralPath $source).Length; sha256=(Get-FileHash -LiteralPath $source).Hash })
}
$review = [ordered]@{ reviewedUtc=[DateTime]::UtcNow.ToString('o'); workflowDigest='aee4e10d1136b1c5bda45be06591cb02644ec13decf94370a0f97d0c6b9ee1df'; preValidationArchiveSealSha256='71FE44804F11FB60C070844DFC58526426F3F8C8B87B1E88B053209C753D6445'; originalArchivedCopies=129; renamedNativeArtifacts=99; agentReview='merge_code_review independently verified final cap sums, unchanged failure handling/native watchdog, source API Git blobs, raw ARM/x64 consumer and suite evidence, cancelled paired verdict, all136 pre-validation sealed files and all129 original copy bytes. No blocking finding. Root independently read source/diffs/raw timing and verified seals, mappings and actual old x64 producer evidence.'; windowsValidation='Five validators and complete tooling PASS at recorded unchanged dirty candidate snapshot; not clean-head native qualification.'; linuxValidation='14/14 focused NativeScope cases with exact six-file overlay on native Ubuntu PowerShell7.6.6; final digest binds only this focused scope.'; initialFailure='Dependency validator rejected14 overlong archive paths; actual payload bytes retained through indexed renaming. Initial driver bootstrap failure occurred before any test and is retained explicitly.'; visual='No native/control/gallery rendering change in this workflow/test/archive repair; actual old-source five-theme and embedded captures reviewed under their own source identities.'; freshCI='Required after clean committed push. No old source receipt is relabeled as qualification of the repaired candidate.'; copies=$copies.ToArray() }
[IO.File]::WriteAllText((Join-Path $archive 'validation/review.receipt.txt'), ($review | ConvertTo-Json -Depth 8), $utf8)
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $archive 'validation/archive-driver.ps1')
$readmePath = Join-Path $archive 'README.md'
$readme = [IO.File]::ReadAllText($readmePath)
$readme += "`nRepair validation: all five validators and complete Windows tooling pass, including 14/14 NativeScope cases.`nThe validation receipt binds unchanged versioned/nonignored input bytes before and after that run. Native Ubuntu`nPowerShell7.6.6 independently passes the final 14-case focused overlay. Formatting passes. The initial 14 path`nfindings, pre-test launcher failure and earlier focused overlay retain their original diagnostics/source bindings.`nThese tooling results do not substitute for fresh exact-head native, consumer, sanitizer or paired CI.`n"
[IO.File]::WriteAllText($readmePath, $readme, $utf8)
$seal = foreach ($file in Get-ChildItem -LiteralPath $archive -File -Recurse | Where-Object { $_.FullName -cne $sealPath } | Sort-Object FullName) {
    $relative = [IO.Path]::GetRelativePath($repo, $file.FullName).Replace('\', '/')
    if ($relative.Length -gt 150) { throw "Final archive path too long: $relative" }
    (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant() + '  ' + [IO.Path]::GetRelativePath($archive, $file.FullName).Replace('\', '/')
}
[IO.File]::WriteAllText($sealPath, (($seal -join "`n") + "`n"), $utf8)
foreach ($line in [IO.File]::ReadAllLines($sealPath)) {
    if ($line -cnotmatch '^([a-f0-9]{64})  (.+)$') { throw 'Final archive seal malformed.' }
    if (-not [string]::Equals((Get-FileHash -LiteralPath (Join-Path $archive $Matches[2])).Hash, $Matches[1], [StringComparison]::OrdinalIgnoreCase)) { throw 'Final validation archive bytes differ.' }
}
Write-Host "COLD_BUDGET_VALIDATION_RETAINED: $($copies.Count) exact external copies; $($seal.Count) sealed files; max path <=150; fresh complete CI still required."
