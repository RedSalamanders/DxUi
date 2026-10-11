<# Records root's actual source/log review. It does not turn failed suites or ci-gate green. #>
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo='C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$receipts=@('root-native-review-114566761505-e138349cbe52499789150f129a93528d.receipt.txt','root-native-review-114566761575-bc14b80e380941fda01035c9610f0b57.receipt.txt','root-native-review-114566761574-f38f37d9dd6440c780ef60ae50110bb5.receipt.txt')
function Require([bool]$condition,[string]$message){if(-not $condition){throw $message}}
function Assert-Seal([string]$packet,[string]$expected){
    $packet=[IO.Path]::GetFullPath($packet);$seal=Join-Path $packet 'SHA256SUMS'
    Require ((Get-FileHash -LiteralPath $seal).Hash -ceq $expected) 'Original seal changed.'
    foreach($line in [IO.File]::ReadAllLines($seal)){
        Require ($line -cmatch '^([a-f0-9]{64})  (.+)$') 'Malformed original seal.'
        $sha=$Matches[1];$path=[IO.Path]::GetFullPath((Join-Path $packet $Matches[2]))
        Require ($path.StartsWith($packet+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -and (Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -ceq $sha) 'Original bytes changed or path escapes.'
    }
}
$source=@(foreach($path in @('Tests/InteractiveLease/InteractiveLease.Tests.WarningBanner.h','Tests/InteractiveLease/InteractiveLease.Tests.Runner.cpp','Tests/Support/Support.Tests.InteractiveLease.h','Tests/Support/Support.Tests.DesktopLease.h')){
    $absolute=Join-Path $repo $path
    $blob=(& git -C $repo rev-parse ('HEAD:'+$path)).Trim();Require ($LASTEXITCODE -eq 0) 'Source Git blob unavailable.'
    $raw=(& git -C $repo hash-object --no-filters -- $absolute).Trim();Require ($LASTEXITCODE -eq 0) 'Raw reviewed source Git object unavailable.'
    $actual=(& git -C $repo hash-object ('--path='+$path) -- $absolute).Trim();Require ($LASTEXITCODE -eq 0 -and $actual -ceq $blob) 'Reviewed source differs from the committed Git text representation.'
    [ordered]@{path=$path;gitBlob=$blob;workingTreeRawGitBlob=$raw;sha256=(Get-FileHash -LiteralPath $absolute).Hash;textRepresentation='Clean committed checkout; normal Git text filters bind CRLF working-tree bytes to LF Git blobs.'}
})
$profiles=@(foreach($leaf in $receipts){
    $receipt=Join-Path $PSScriptRoot $leaf;$r=Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json -AsHashtable
    Require ($r['head'] -ceq '4bd3480170140090ab2d8ff883d05507281b52ee' -and $r['testedMerge'] -ceq '80d875a1de91d2b2c8602faa5614cf75eab6025b' -and $r['status'] -ceq 'ROOT_PROFILE_REVIEW_READY_NOT_CI_GATE_SUCCESS' -and $r['blockers'].Count -eq 0 -and $r['jobConclusion'] -ceq 'failure' -and $r['foregroundRefusalCandidates'].Count -eq 2) 'Profile/source/refusal inventory differs.'
    Assert-Seal $r['originalCapture'] $r['originalSealSha256']
    $job=Get-Content -LiteralPath (Join-Path $r['originalCapture'] 'job.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable
    $failed=@($job['steps']|Where-Object{$_['conclusion'] -cin @('failure','cancelled','timed_out','action_required')})
    Require ($failed.Count -eq 1 -and $failed[0]['name'].StartsWith('Run ./test.ps1 -Full',[StringComparison]::Ordinal)) 'Failure is beyond unlaunched Full foreground suites.'
    $log=Join-Path $r['originalCapture'] ('job-'+$r['job']+'.log');$text=[IO.File]::ReadAllText($log)
    $foreign=[regex]::Matches($text,'(?m)^.*\[LEASE\] warning target refused: ready=1 point=(?<point>\d+,\d+) hit=(?<hit>[0-9A-Fa-f]+) class=Windows\.UI\.Core\.CoreWindow process=(?<pid>\d+) warning=(?<warning>[0-9A-Fa-f]+) .*$')
    Require ($foreign.Count -eq 1 -and $foreign[0].Groups['hit'].Value -cne $foreign[0].Groups['warning'].Value -and $foreign[0].Groups['point'].Value -ceq '859,82') 'Precise read-only foreign-target warning evidence differs.'
    Require ($text.Contains('the warning does not own the safe input point') -and $text.Contains('[LEASE] Restoration: foreground=unchanged focus=unchanged cursor=unchanged')) 'Refusal reason or restoration differs.'
    Require (-not [regex]::IsMatch($text,'\[LEASE\] (?:Menu|NativeTextInput): started')) 'A refused suite child started.'
    foreach($entry in $r['foregroundRefusalCandidates']){
        $lease=$entry['lease'];Require ($entry['exitCode'] -eq 24 -and $lease['state'] -ceq 'unwarned' -and $lease['exitCode'] -eq 23 -and $lease['seconds'] -eq $null -and $lease['foreground'] -ceq 'unchanged' -and $lease['focus'] -ceq 'unchanged' -and $lease['cursor'] -ceq 'unchanged' -and $lease['runMovedSomething'] -eq $false -and $lease['cursorSaved'] -ceq '512,384' -and $lease['cursorAtExit'] -ceq '512,384') 'No-input/endpoint evidence differs.'
        $native=@($r['native']|Where-Object{$_['suite'] -ceq $entry['suite']});Require ($native.Count -eq 1 -and [IO.File]::ReadAllText($native[0]['suiteLog']).Trim() -ceq ('[LEASE] '+$entry['suite']+' did not run under the foreground lease')) 'Refused suite log is not literally unlaunched.'
    }
    Assert-Seal $r['originalCapture'] $r['originalSealSha256']
    [ordered]@{profile=$r['native'][0]['profile'];originalCapture=$r['originalCapture'];originalSealSha256=$r['originalSealSha256'];review=$receipt;reviewSha256=(Get-FileHash -LiteralPath $receipt).Hash;job=$r['job'];jobLog=$log;jobLogSha256=(Get-FileHash -LiteralPath $log).Hash;literalForeignTarget=$foreign[0].Value;nativeForeground='FAILED_INCOMPLETE_UNLAUNCHED';refusals=$r['foregroundRefusalCandidates'];sourceFinding='Either ownership failure branches before SendLeftButtonDown/Up; failed ShowWarning returns before RunChild. No button insertion or suite launch/input occurred.';cursorLimit='Saved/pre-restoration/exit pointer matches and EndPatch/Hide do no pointer reset. This supports the first guard interpretation, but shared error text and endpoint status do not continuously instrument transient or external pointer movement.'}
})
$head=(& git -C $repo rev-parse HEAD).Trim();Require ($LASTEXITCODE -eq 0 -and $head -ceq '4bd3480170140090ab2d8ff883d05507281b52ee' -and @(& git -C $repo status --porcelain=v1).Count -eq 0) 'Current source drifted.'
$output=Join-Path $PSScriptRoot 'root-arm-before-suite-input-399.receipt.txt'
Require (-not(Test-Path -LiteralPath $output)) 'Do not overwrite a root review receipt.'
[ordered]@{status='ROOT_VERIFIED_DATED_UNLAUNCHED_ARM_FOREGROUND_SCOPE';runId=38172114803;attempt=1;head=$head;testedMerge='80d875a1de91d2b2c8602faa5614cf75eab6025b';reviewedSource=$source;profiles=$profiles;contract='Specs/Testing/Testing_Validation.md:743-750';scope='Only implementation integration may use the individually verified refusal scope. All six foreground suites and three native jobs remain failed/incomplete; no ci-gate success, timeout waiver, continuous input instrumentation or broader qualification.';reviewedUtc=[DateTimeOffset]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 20|Set-Content -LiteralPath $output -Encoding utf8NoBOM
Write-Output ('ROOT_ARM_BEFORE_SUITE_INPUT_REVIEW: three profiles/six unlaunched suites remain failed/incomplete; '+$output)
