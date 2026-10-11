<# Read-only Git/GitHub collection. Writes only a fresh external directory; performs no builds, tests, reruns or remote changes. #>
[CmdletBinding()] param(
    [Parameter(Mandatory)][ValidateRange(1, 2147483647)][int] $PullRequest,
    [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{40}$')][string] $ExpectedHead,
    [ValidatePattern('^[a-f0-9]{40}$')][string] $ExpectedBase = 'ec680e866a218b57000a1780018c00101a437f91',
    [long] $RunId = 0,
    [string] $RepositoryRoot = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi',
    [string] $OutputParent = 'C:/Users/eric/.codex/tmp/dpf-s0-s2-merge/ci-evidence',
    [ValidatePattern('^[a-f0-9]{64}$')][string] $ExpectedWorkflowDigest = '7d4a1afd4c0a74ae8b93185f5ca4f78eda76d0eda4eff3e368bffd8c207730c8'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = 'RedSalamanders/DxUi'
$utf8 = [Text.UTF8Encoding]::new($false)
$sourceRecords = [Collections.Generic.List[object]]::new()
$apiPayloads = [Collections.Generic.List[object]]::new()
$approvedExternalRoot = [IO.Path]::GetFullPath('C:/Users/eric/.codex/tmp/dpf-s0-s2-merge')
$fullOutputParent = [IO.Path]::GetFullPath($OutputParent)
if ($fullOutputParent -cne $approvedExternalRoot -and -not $fullOutputParent.StartsWith($approvedExternalRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Collection outputs must stay inside the approved external dpf-s0-s2-merge directory.'
}

function Invoke-GhBytes([string[]] $Arguments) {
    $start = [Diagnostics.ProcessStartInfo]::new((Get-Command gh -CommandType Application).Source)
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    $stream = [IO.MemoryStream]::new()
    try {
        $copy = $process.StandardOutput.BaseStream.CopyToAsync($stream)
        $errors = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(180000)) {
            $process.Kill($true)
            $process.WaitForExit()
            throw 'The owned read-only gh request exceeded its bounded three-minute allowance.'
        }
        [void]$copy.GetAwaiter().GetResult()
        $errorText = $errors.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) { throw "GitHub read failed: $errorText" }
        return ,$stream.ToArray()
    } finally { $stream.Dispose(); $process.Dispose() }
}

function Read-GhJson([string] $Endpoint) {
    $bytes = Invoke-GhBytes @('api', $Endpoint)
    $apiPayloads.Add([ordered]@{ endpoint = $Endpoint; bytes = $bytes })
    return ($utf8.GetString($bytes) | ConvertFrom-Json -AsHashtable)
}

function Save-Json([string] $Path, [object] $Value) {
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path))
    [IO.File]::WriteAllText($Path, (($Value | ConvertTo-Json -Depth 60) + "`n"), $utf8)
}

function Get-Digest([byte[]] $Bytes) { return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant() }
function Receipt-Instant([object] $Value) {
    if ($Value -is [DateTimeOffset]) { return $Value }
    if ($Value -is [DateTime]) { return [DateTimeOffset]::new($Value.ToUniversalTime()) }
    return [DateTimeOffset]::Parse([string]$Value, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::RoundtripKind)
}

function Assert-LocalFreeze {
    $head = (& git -C $RepositoryRoot rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $head -cne $ExpectedHead) { throw "Local source is not frozen head $ExpectedHead." }
    $status = @(& git -C $RepositoryRoot status --porcelain --untracked-files=normal)
    if ($LASTEXITCODE -ne 0 -or $status.Count) { throw 'PR delegation/review requires a clean committed candidate.' }
    $base = (& git -C $RepositoryRoot rev-parse refs/remotes/origin/main).Trim()
    if ($LASTEXITCODE -ne 0 -or $base -cne $ExpectedBase) { throw 'Local origin/main does not match the expected actual PR base; refresh/account for drift.' }
}

function Assert-PrSnapshot([System.Collections.IDictionary] $Pr) {
    if ($Pr['state'] -cne 'open' -or $Pr['draft'] -ne $false -or $Pr['head']['sha'] -cne $ExpectedHead -or
        $Pr['head']['ref'] -cne 'codex/performance-forms' -or $Pr['base']['ref'] -cne 'main' -or $Pr['base']['sha'] -cne $ExpectedBase) {
        throw 'The open non-draft PR does not bind the requested branch/head/base.'
    }
    if ($Pr['merge_commit_sha'] -cnotmatch '^[a-f0-9]{40}$') { throw 'The current synthetic PR merge commit is unavailable; do not infer the tested tree.' }
}

function Save-SourceInput([string] $Revision, [string] $RepositoryPath, [string] $RelativePath) {
    $data = Read-GhJson "repos/$repo/contents/${RepositoryPath}?ref=$Revision"
    if ($data['encoding'] -cne 'base64') { throw "GitHub did not return exact source bytes for $RepositoryPath." }
    $bytes = [Convert]::FromBase64String($data['content'])
    $prefix = $utf8.GetBytes("blob $($bytes.Length)`0")
    $sha1 = [Security.Cryptography.SHA1]::Create()
    try { $blob = ([BitConverter]::ToString($sha1.ComputeHash($prefix + $bytes))).Replace('-', '').ToLowerInvariant() }
    finally { $sha1.Dispose() }
    if ($blob -cne $data['sha']) { throw "Git blob byte verification failed: $RepositoryPath" }
    $path = Join-Path $collectionRoot $RelativePath
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))
    [IO.File]::WriteAllBytes($path, $bytes)
    $sourceRecords.Add([ordered]@{ commit = $Revision; path = $RepositoryPath; savedPath = $RelativePath; gitBlob = $blob; sha256 = Get-Digest $bytes; bytes = $bytes.Length })
    return ,$bytes
}

function Get-CheckoutCommit([string] $LogPath) {
    $lines = [IO.File]::ReadAllLines($LogPath)
    for ($index = 0; $index -lt $lines.Count; $index++) {
        if ($lines[$index].Contains('log -1 --format=%H')) {
            for ($next = $index + 1; $next -lt [Math]::Min($index + 5, $lines.Count); $next++) {
                if ($lines[$next] -cmatch '(?<sha>[a-f0-9]{40})\s*$') { return $Matches['sha'] }
            }
            throw "Checkout SHA was not printed after its Git log command: $LogPath"
        }
    }
    throw "No checkout commit proof in $LogPath"
}

Assert-LocalFreeze
$prBefore = Read-GhJson "repos/$repo/pulls/$PullRequest"
Assert-PrSnapshot $prBefore
$liveMain = Read-GhJson "repos/$repo/git/ref/heads/main"
if ($liveMain['object']['sha'] -cne $ExpectedBase) { throw 'Remote main drifted from the requested base.' }
$workflow = Read-GhJson "repos/$repo/actions/workflows/ci.yml"
if ($workflow['state'] -cne 'active') { throw 'The exact reviewed PR workflow is not active.' }
if ($RunId -eq 0) {
    $runs = Read-GhJson "repos/$repo/actions/workflows/ci.yml/runs?event=pull_request&head_sha=$ExpectedHead&per_page=30"
    $eligible = @($runs['workflow_runs'] | Where-Object {
        $_['head_sha'] -ceq $ExpectedHead -and @($_['pull_requests'] | Where-Object { $_['number'] -eq $PullRequest }).Count -eq 1
    } | Sort-Object { $_['run_number'] } -Descending)
    if (-not $eligible.Count) { throw 'No normal pull_request workflow is attached to the exact frozen head.' }
    $RunId = [long]$eligible[0]['id']
}
$run = Read-GhJson "repos/$repo/actions/runs/$RunId"
if ($run['event'] -cne 'pull_request' -or $run['head_sha'] -cne $ExpectedHead -or $run['workflow_id'] -ne $workflow['id'] -or
    @($run['pull_requests'] | Where-Object { $_['number'] -eq $PullRequest }).Count -ne 1) { throw 'Run identity/event/PR association differs from the frozen publication.' }
if ($run['status'] -cne 'completed') { throw "Exact-head run $RunId is $($run['status']); pending evidence is not a pass. Collect after completion." }
$attempt = [int]$run['run_attempt']
$attemptRun = Read-GhJson "repos/$repo/actions/runs/$RunId/attempts/$attempt"
$testedMerge = [string]$prBefore['merge_commit_sha']
$merge = Read-GhJson "repos/$repo/git/commits/$testedMerge"
$head = Read-GhJson "repos/$repo/git/commits/$ExpectedHead"
$base = Read-GhJson "repos/$repo/git/commits/$ExpectedBase"
if (@($merge['parents']).Count -ne 2 -or $merge['parents'][0]['sha'] -cne $ExpectedBase -or $merge['parents'][1]['sha'] -cne $ExpectedHead) {
    throw 'The current tested merge does not have the frozen base/head as its ordered parents.'
}
if ($merge['tree']['sha'] -cne $head['tree']['sha']) { throw 'Merge tree differs from the frozen local candidate tree; independently integrate/requalify before delegation.' }
$collectionRoot = Join-Path $fullOutputParent ("pr-$PullRequest-run-$RunId-attempt-$attempt-$([guid]::NewGuid().ToString('N'))")
if (Test-Path -LiteralPath $collectionRoot) { throw 'Collection output already exists; refusing to overwrite evidence.' }
[void][IO.Directory]::CreateDirectory($collectionRoot)
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $collectionRoot 'collection-driver.ps1')
foreach ($entry in @(
    @{ Name = 'pr-before.receipt.txt'; Value = $prBefore }, @{ Name = 'run.receipt.txt'; Value = $run }, @{ Name = 'attempt.receipt.txt'; Value = $attemptRun },
    @{ Name = 'workflow.receipt.txt'; Value = $workflow }, @{ Name = 'main-before.receipt.txt'; Value = $liveMain }, @{ Name = 'merge-commit.receipt.txt'; Value = $merge },
    @{ Name = 'head-commit.receipt.txt'; Value = $head }, @{ Name = 'base-commit.receipt.txt'; Value = $base }
)) { Save-Json (Join-Path $collectionRoot $entry.Name) $entry.Value }
$jobs = [Collections.Generic.List[object]]::new()
for ($page = 1; ; $page++) {
    $data = Read-GhJson "repos/$repo/actions/runs/$RunId/attempts/$attempt/jobs?per_page=100&page=$page"
    foreach ($job in $data['jobs']) { $jobs.Add($job) }
    if (@($data['jobs']).Count -lt 100) { break }
}
Save-Json (Join-Path $collectionRoot 'jobs.receipt.txt') $jobs.ToArray()
$nativeJobs = @($jobs | Where-Object { $_['name'].StartsWith('native (', [StringComparison]::Ordinal) })
if ($nativeJobs.Count -ne 6) { throw 'The actual run does not contain exactly six expanded native profiles.' }
$nativeJobBindings = [Collections.Generic.List[object]]::new()
$nativeProfileKeys = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($job in $nativeJobs) {
    # GitHub may include every include-matrix value in the expanded display name. The exact source digest
    # still binds the selected jobTimeoutMinutes when the service omits that value from its display.
    $display = [regex]::Match($job['name'], '^native \((?<platform>x64|ARM64), (?<runner>windows-2025-vs2026|windows-11-vs2026-arm), (?<configuration>Debug|Release|ASan Debug)(?:, (?<budget>40|60))?\)$')
    if (-not $display.Success) { throw "Unexpected native matrix display: $($job['name'])" }
    $nativePlatform = $display.Groups['platform'].Value
    $nativeConfiguration = $display.Groups['configuration'].Value
    $nativeRunner = if ($nativePlatform -ceq 'ARM64') { 'windows-11-vs2026-arm' } else { 'windows-2025-vs2026' }
    $nativeBudget = if ($nativePlatform -ceq 'ARM64' -and $nativeConfiguration -ceq 'ASan Debug') { 60 } else { 40 }
    if ($display.Groups['runner'].Value -cne $nativeRunner -or ($display.Groups['budget'].Success -and [int]$display.Groups['budget'].Value -ne $nativeBudget)) { throw 'Native profile runner/budget display differs from its exact selected workflow row.' }
    if (-not $nativeProfileKeys.Add("$nativePlatform/$nativeConfiguration")) { throw 'The run repeats a native profile.' }
    $nativeJobBindings.Add([ordered]@{ job=$job['id']; name=$job['name']; platform=$nativePlatform; configuration=$nativeConfiguration; runner=$nativeRunner; declaredBudgetMinutes=$nativeBudget; displayedBudgetMinutes=$(if ($display.Groups['budget'].Success) { [int]$display.Groups['budget'].Value } else { $null }) })
}
if (-not $nativeProfileKeys.SetEquals([string[]]@('x64/Debug','x64/Release','x64/ASan Debug','ARM64/Debug','ARM64/Release','ARM64/ASan Debug'))) { throw 'Actual native profiles differ from the exact required six-row matrix.' }
[void][IO.Directory]::CreateDirectory((Join-Path $collectionRoot 'logs'))
$checkoutBindings = [Collections.Generic.List[object]]::new()
foreach ($job in $jobs) {
    $logPath = Join-Path $collectionRoot "logs/job-$($job['id']).log"
    $bytes = Invoke-GhBytes @('api', "repos/$repo/actions/jobs/$($job['id'])/logs", '--allow-escape-sequences')
    [IO.File]::WriteAllBytes($logPath, $bytes)
    if ($job['id'] -in @($nativeJobs | ForEach-Object { $_['id'] }) -or $job['name'] -clike 'paired-benchmark*') {
        $checkout = Get-CheckoutCommit $logPath
        if ($checkout -cne $testedMerge) { throw "Job $($job['name']) tested a different checkout commit: $checkout" }
        $checkoutBindings.Add([ordered]@{ job = $job['id']; name = $job['name']; checkout = $checkout; log = "logs/job-$($job['id']).log" })
    }
}
$workflowBytes = Save-SourceInput $testedMerge '.github/workflows/ci.yml' 'source/ci.yml'
$normalized = $utf8.GetString($workflowBytes) -replace "`r`n", "`n"
if ((Get-Digest ($utf8.GetBytes($normalized))) -cne $ExpectedWorkflowDigest) { throw 'Actual tested-merge workflow bytes differ from the approved manifest digest.' }
$manifestBytes = Save-SourceInput $testedMerge 'Tests/test-scopes.json' 'source/test-scopes.receipt.txt'
$manifest = $utf8.GetString($manifestBytes) | ConvertFrom-Json -AsHashtable
if ($manifest['prWorkflowDigest'] -cne $ExpectedWorkflowDigest -or $manifest['version'] -ne 1 -or @($manifest['prCoverage']).Count -ne 6) { throw 'Actual tested-merge scope manifest differs from the reviewed contract.' }
[void](Save-SourceInput $testedMerge 'Tools/PerformanceAcceptancePolicy.v1.json' 'source/candidate-policy.receipt.txt')
[void](Save-SourceInput $ExpectedBase 'Tools/PerformanceAcceptancePolicy.v1.json' 'source/base-policy.receipt.txt')
[void](Save-SourceInput $testedMerge 'Tools/PerformanceComparison.psm1' 'source/candidate-judge.psm1')
[void](Save-SourceInput $ExpectedBase 'Tools/PerformanceComparison.psm1' 'source/base-judge.psm1')
[void](Save-SourceInput $testedMerge 'Specs/Testing/Testing_Validation.md' 'source/Testing_Validation.md')
$artifacts = Read-GhJson "repos/$repo/actions/runs/$RunId/artifacts?per_page=100"
Save-Json (Join-Path $collectionRoot 'artifacts.receipt.txt') $artifacts
if ([int]$artifacts['total_count'] -gt 100) { throw 'Artifact list needs pagination; refusing an incomplete inventory.' }
$names = @($manifest['prCoverage'] | ForEach-Object { "native-$($_['platform'])-$($_['configuration'])" }) + @('paired-benchmark-x64-Release', 'docs-gallery-x64-Release')
$artifactBindings = [Collections.Generic.List[object]]::new()
foreach ($name in $names) {
    $selected = @($artifacts['artifacts'] | Where-Object { $_['name'] -ceq $name })
    if ($selected.Count -ne 1 -or $selected[0]['expired'] -ne $false) { throw "Exact live artifact missing, duplicate or expired: $name" }
    $artifact = $selected[0]
    if ((Receipt-Instant $artifact['created_at']) -lt (Receipt-Instant $attemptRun['run_started_at'])) { throw "Artifact $name belongs to an earlier attempt." }
    if ($artifact['workflow_run']['id'] -ne $RunId -or $artifact['workflow_run']['head_sha'] -cne $ExpectedHead) { throw "Artifact $name is bound to another run/head." }
    $relative = 'artifacts/' + $name.Replace(' ', '_')
    $directory = Join-Path $collectionRoot $relative
    [void][IO.Directory]::CreateDirectory($directory)
    & gh run download $RunId --repo $repo --name $name --dir $directory
    if ($LASTEXITCODE -ne 0) { throw "Artifact download failed: $name" }
    $artifactBindings.Add([ordered]@{ name = $name; id = $artifact['id']; directory = $relative; createdUtc = $artifact['created_at']; digest = $artifact['digest'] })
}
$prAfter = Read-GhJson "repos/$repo/pulls/$PullRequest"
Assert-PrSnapshot $prAfter
if ($prAfter['merge_commit_sha'] -cne $testedMerge) { throw 'The PR merge reference changed during collection.' }
$mainAfter = Read-GhJson "repos/$repo/git/ref/heads/main"
if ($mainAfter['object']['sha'] -cne $ExpectedBase) { throw 'Remote main changed during collection.' }
Assert-LocalFreeze
Save-Json (Join-Path $collectionRoot 'pr-after.receipt.txt') $prAfter
Save-Json (Join-Path $collectionRoot 'main-after.receipt.txt') $mainAfter
$prCheckBytes = Invoke-GhBytes @('pr','view',[string]$PullRequest,'--repo',$repo,'--json','number,url,state,isDraft,headRefName,headRefOid,baseRefName,baseRefOid,reviewDecision,mergeable,mergeStateStatus,statusCheckRollup')
[IO.File]::WriteAllBytes((Join-Path $collectionRoot 'pr-check-rollup.receipt.txt'), $prCheckBytes)
$rawApiRecords = [Collections.Generic.List[object]]::new()
[void][IO.Directory]::CreateDirectory((Join-Path $collectionRoot 'raw-api'))
foreach ($payload in $apiPayloads) {
    $relative = 'raw-api/{0:D4}.receipt.txt' -f ($rawApiRecords.Count + 1)
    [IO.File]::WriteAllBytes((Join-Path $collectionRoot $relative), $payload['bytes'])
    $rawApiRecords.Add([ordered]@{ endpoint = $payload['endpoint']; path = $relative; bytes = $payload['bytes'].Length; sha256 = Get-Digest $payload['bytes'] })
}
$collection = [ordered]@{
    version = 1; repository = $repo; pullRequest = $PullRequest; runId = $RunId; attempt = $attempt
    expectedHead = $ExpectedHead; expectedBase = $ExpectedBase; testedMerge = $testedMerge; testedTree = $merge['tree']['sha']
    workflowDigest = $ExpectedWorkflowDigest; repositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
    runConclusion = $run['conclusion']; collectedUtc = [DateTime]::UtcNow.ToString('o'); logSource='Exact gh api job-logs endpoint stdout bytes with escape sequences allowed'
    sourceInputs = $sourceRecords.ToArray(); rawApiPayloads = $rawApiRecords.ToArray(); checkouts = $checkoutBindings.ToArray(); nativeJobBindings=$nativeJobBindings.ToArray(); artifacts = $artifactBindings.ToArray()
    limitations = @('CI executable/compiler/SDK/dependency bytes and inventories are not uploaded; recorded identities cannot be independently recomputed from those absent inputs.', 'The current native upload selects reports/*.json, while build sidecars live under reports/performance-build/<profile>/; the reviewer inventories actual uploads and verifies any sidecars present without inventing an upload obligation.', 'Artifact API zip digests are retained; gh download extracts content, so the original zip digest is not reverified.', 'Collection is not ci-gate success or implementation acceptance.')
}
Save-Json (Join-Path $collectionRoot 'collection.receipt.txt') $collection
$seal = foreach ($file in (Get-ChildItem -LiteralPath $collectionRoot -File -Recurse | Sort-Object FullName)) {
    $relative = [IO.Path]::GetRelativePath($collectionRoot, $file.FullName).Replace('\', '/')
    "$(Get-Digest ([IO.File]::ReadAllBytes($file.FullName)))  $relative"
}
[IO.File]::WriteAllText((Join-Path $collectionRoot 'SHA256SUMS'), (($seal -join "`n") + "`n"), $utf8)
Write-Host "CI_COLLECTION_COMPLETE: $collectionRoot"
Write-Host "Run $RunId attempt $attempt conclusion $($run['conclusion']); this does not mean ci-gate passed."
