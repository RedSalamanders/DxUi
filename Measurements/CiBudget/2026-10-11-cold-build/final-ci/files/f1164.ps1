[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo='C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$external='C:/Users/eric/.codex/tmp/dpf-s0-s2-merge'
$head='4bd3480170140090ab2d8ff883d05507281b52ee'
$base='ec680e866a218b57000a1780018c00101a437f91'
$tree='e695cf74db0e2fc1a5c8c8d183cfb0bd136d639d'
$decision=Join-Path $external 'ci-evidence/pr-73-run-38172114803-attempt-1-858f3ba12193400b84801b3a1fa75ac2/root-implementation-integration-decision-399.receipt.txt'
$proof=Join-Path $external 'ordinary-remote-merge-73.receipt.txt'
function Need([bool]$test,[string]$message){if(-not $test){throw $message}}
function Api([string]$endpoint){$raw=@(& gh api $endpoint);Need ($LASTEXITCODE -eq 0) "GitHub read failed: $endpoint";($raw -join [char]10)|ConvertFrom-Json -AsHashtable}
function New-Receipt([string]$path,[object]$object){Need (-not(Test-Path -LiteralPath $path)) 'Receipt already exists.';$bytes=[Text.UTF8Encoding]::new($false).GetBytes(($object|ConvertTo-Json -Depth 40)+[char]10);$stream=[IO.File]::Open($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);try{$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}}
Need (-not(Test-Path -LiteralPath $proof)) 'Merge proof already exists; read the actual outcome before any retry.'
Need ((Get-FileHash -LiteralPath $decision).Hash -ceq 'E0717A367AADC269D4203812A51588CBBBC07BE87667B81A038ACFDAD09E99F6') 'Reviewed decision changed.'
$sourceHead=(& git -C $repo rev-parse HEAD).Trim();Need ($LASTEXITCODE -eq 0 -and $sourceHead -ceq $head) 'Frozen head changed.'
$sourceTree=(& git -C $repo rev-parse 'HEAD^{tree}').Trim();Need ($LASTEXITCODE -eq 0 -and $sourceTree -ceq $tree) 'Frozen source tree changed.'
$status=@(& git -C $repo status --porcelain --untracked-files=normal);Need ($LASTEXITCODE -eq 0 -and $status.Count -eq 0) 'Frozen source is dirty.'
$pr=Api 'repos/RedSalamanders/DxUi/pulls/73'
$main=Api 'repos/RedSalamanders/DxUi/branches/main'
$rules=Api 'repos/RedSalamanders/DxUi/rules/branches/main'
$run=Api 'repos/RedSalamanders/DxUi/actions/runs/38172114803'
Need ($pr.state -ceq 'open' -and $pr.draft -eq $false -and $pr.head.sha -ceq $head -and $pr.base.sha -ceq $base -and $pr.base.ref -ceq 'main' -and $pr.mergeable -eq $true) 'PR state, actual base/head or mergeability changed.'
Need ($main.commit.sha -ceq $base) 'Remote main advanced beyond the reviewed base.'
Need ($run.status -ceq 'completed' -and $run.conclusion -ceq 'failure' -and $run.run_attempt -eq 1 -and $run.head_sha -ceq $head) 'The original failed run changed.'
Need (@($rules|Where-Object type -CEQ 'required_status_checks').Count -eq 0) 'Required status-check rules need separate satisfaction; this script never bypasses them.'
Need (@($rules|Where-Object type -CEQ 'pull_request').Count -eq 1) 'Expected ordinary PR integration rule is absent.'
New-Receipt (Join-Path $external 'ordinary-remote-merge-73.before.receipt.txt') ([ordered]@{rootDecision=$decision;rootDecisionSha256=(Get-FileHash -LiteralPath $decision).Hash;pullRequest=$pr;main=$main;rules=$rules;run=$run;method='ordinary PUT /pulls/73/merge with exact sha and merge_method=merge';adminOverride=$false;checkedUtc=[DateTimeOffset]::UtcNow})
# This ordinary endpoint enforces server-side branch rules. No admin/bypass/auto-merge flags are used.
$raw=@(& gh api --method PUT 'repos/RedSalamanders/DxUi/pulls/73/merge' -f "sha=$head" -f 'merge_method=merge')
$mergeExit=$LASTEXITCODE
New-Receipt (Join-Path $external 'ordinary-remote-merge-73.response.receipt.txt') ([ordered]@{exitCode=$mergeExit;rawResponse=($raw-join[char]10);completedUtc=[DateTimeOffset]::UtcNow})
Need ($mergeExit -eq 0) 'Ordinary merge was rejected; preserve response and re-read state before proceeding.'
$response=($raw-join[char]10)|ConvertFrom-Json -AsHashtable
Need ($response.merged -eq $true -and $response.sha -cmatch '^[a-f0-9]{40}$') 'Ordinary merge response is not confirmed.'
$merged=Api 'repos/RedSalamanders/DxUi/pulls/73'
$actual=Api "repos/RedSalamanders/DxUi/git/commits/$($response.sha)"
$mainAfter=Api 'repos/RedSalamanders/DxUi/branches/main'
Need ($merged.merged -eq $true -and $merged.state -ceq 'closed' -and $merged.merge_commit_sha -ceq $response.sha -and $mainAfter.commit.sha -ceq $response.sha) 'PR merge and remote-main identities do not agree.'
Need ($actual.tree.sha -ceq $tree -and @($actual.parents).Count -eq 2 -and $actual.parents[0].sha -ceq $base -and $actual.parents[1].sha -ceq $head) 'Actual ordinary merge differs from the reviewed ordered parents/tree.'
$r=[ordered]@{status='ORDINARY_REMOTE_MAIN_MERGE_VERIFIED';pullRequest=73;url=$merged.html_url;ordinaryMergedHead=$response.sha;base=$base;head=$head;testedMerge='80d875a1de91d2b2c8602faa5614cf75eab6025b';actualTree=$actual.tree.sha;orderedParents=@($actual.parents|ForEach-Object sha);mergedAt=$merged.merged_at;remoteMain=$mainAfter.commit.sha;response=$response;actualGitObject=$actual;rootDecision=$decision;rootDecisionSha256=(Get-FileHash -LiteralPath $decision).Hash;ciConclusion='failure';ciGateSucceeded=$false;qualificationPassed=$false;adminOverride=$false;forcePush=$false;policyChanged=$false;completedUtc=[DateTimeOffset]::UtcNow}
New-Receipt $proof $r
[ordered]@{proof=$proof;proofSha256=(Get-FileHash -LiteralPath $proof).Hash;status=$r.status;mergedHead=$r.ordinaryMergedHead;tree=$tree;ciGateSucceeded=$false}|ConvertTo-Json
