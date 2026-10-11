Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$external = 'C:/Users/eric/.codex/tmp/dpf-s0-s2-merge'
$head = '4bd3480170140090ab2d8ff883d05507281b52ee'
$tested = '80d875a1de91d2b2c8602faa5614cf75eab6025b'
$runId = 38172114803
$helper = Join-Path $external 'ci-review-tools/Collect-PrCiEvidence.ps1'
if ((Get-FileHash -LiteralPath $helper).Hash -cne 'D85108857996838D2FAC9FCDC883A34D8059C14EA53FDE7F11A9FC67D98749FF') { throw 'Byte reader source changed.' }
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($helper, [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw 'Collector parse errors.' }
foreach ($name in @('Invoke-GhBytes', 'Get-CheckoutCommit')) {
    $fn = @($ast.FindAll({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $name }, $true))
    if ($fn.Count -ne 1) { throw 'Missing reviewed byte/checkout reader.' }
    . ([scriptblock]::Create($fn[0].Extent.Text))
}
$destination = Join-Path $external ('early-ci-ordinary-399-' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($destination)
$utf8 = [Text.UTF8Encoding]::new($false)
$runBytes = Invoke-GhBytes @('api', "repos/RedSalamanders/DxUi/actions/runs/$runId")
[IO.File]::WriteAllBytes((Join-Path $destination 'run.receipt.txt'), $runBytes)
$run = $utf8.GetString($runBytes) | ConvertFrom-Json -AsHashtable
if ($run['head_sha'] -cne $head -or $run['run_attempt'] -ne 1 -or $run['event'] -cne 'pull_request') { throw 'Ordinary run binding differs.' }
$reviewed = [Collections.Generic.List[object]]::new()
foreach ($entry in @(
    @{ id=114566698466; name='validation'; required='All validation passed.' },
    @{ id=114566698240; name='windows-tooling'; required='All validation passed.' },
    @{ id=114566698331; name='format'; required='clang-format version 22.1.3' },
    @{ id=114566698393; name='native-scope'; required='Native jobs: run;' }
)) {
    $jobBytes = Invoke-GhBytes @('api', "repos/RedSalamanders/DxUi/actions/jobs/$($entry.id)")
    [IO.File]::WriteAllBytes((Join-Path $destination ($entry.name + '.receipt.txt')), $jobBytes)
    $job = $utf8.GetString($jobBytes) | ConvertFrom-Json -AsHashtable
    if ($job['id'] -ne $entry.id -or $job['run_id'] -ne $runId -or $job['run_attempt'] -ne 1 -or $job['name'] -cne $entry.name) { throw 'Ordinary job source differs.' }
    if ($job['status'] -cne 'completed') { continue }
    if ($job['conclusion'] -cne 'success') { throw "Ordinary CI blocker: $($entry.name)" }
    $logBytes = Invoke-GhBytes @('api', "repos/RedSalamanders/DxUi/actions/jobs/$($entry.id)/logs", '--allow-escape-sequences')
    $logPath = Join-Path $destination ($entry.name + '.log')
    [IO.File]::WriteAllBytes($logPath, $logBytes)
    if ((Get-CheckoutCommit $logPath) -cne $tested) { throw 'Actual ordinary checkout differs from tested merge.' }
    $text = $utf8.GetString($logBytes)
    if ($text -notmatch [regex]::Escape($entry.required)) { throw "Missing final ordinary success evidence: $($entry.name)" }
    $reviewed.Add([ordered]@{ job=$entry.id; name=$entry.name; conclusion=$job['conclusion']; logSha256=(Get-FileHash -LiteralPath $logPath).Hash; actualCheckout=$tested; completedUtc=$job['completed_at'] })
}
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $destination 'capture-driver.ps1')
$receipt = [ordered]@{ run=$runId; attempt=1; head=$head; testedMerge=$tested; reviewed=$reviewed.ToArray(); scope='Only completed ordinary jobs. Missing running jobs stay pending; no native/consumer/paired or aggregate qualification.'; reviewedUtc=[DateTime]::UtcNow.ToString('o') }
[IO.File]::WriteAllText((Join-Path $destination 'root-review.receipt.txt'), ($receipt | ConvertTo-Json -Depth 8), $utf8)
$seal = foreach ($file in Get-ChildItem -LiteralPath $destination -File | Sort-Object Name) { (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant() + '  ' + $file.Name }
[IO.File]::WriteAllText((Join-Path $destination 'SHA256SUMS'), (($seal -join "`n") + "`n"), $utf8)
Write-Host "ROOT_ORDINARY_JOBS_REVIEWED: $($reviewed.Count) completed exact-source jobs; $destination"
