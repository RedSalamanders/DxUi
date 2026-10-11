
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$toolRoot='C:/Users/eric/.codex/tmp/dpf-s0-s2-merge/ci-review-tools'
$repairRoot=Join-Path $toolRoot 'native-review-repair-247257c535f54346845f705e17f8f3f1'
$packet='C:/Users/eric/.codex/tmp/dpf-s0-s2-merge/early-ci-native-398/debug-317a7b0d7f51497d81c6542849fec670'
$repo='C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$helper=Join-Path $toolRoot 'Review-PrCiEvidence.ps1'
$utf8=[Text.UTF8Encoding]::new($false)
$tokens=$null; $errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($helper,[ref]$tokens,[ref]$errors)
if(@($errors).Count){throw 'External reviewer parser errors.'}
foreach($name in @('Require','Digest','Receipt-Instant','Find-File','Receipt','Assert-CompletedInJob','Assert-Sha','Read-ExactExitCode','Read-ExactInteger','Assert-HostedLease','Assert-NativeSuiteLog','Assert-ConsumerMetadata','Assert-NativeProducerStep')){
    $definition=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $name},$true))
    Require ($definition.Count -eq 1) "Missing pure helper $name."
    . ([scriptblock]::Create($definition[0].Extent.Text))
}
function Test-PacketSeal {
    $count=0
    foreach($line in [IO.File]::ReadAllLines((Join-Path $packet 'SHA256SUMS'))){
        Require ($line -cmatch '^([a-f0-9]{64})  (.+)$') 'Malformed Debug packet seal.'
        $sha=$Matches[1]; $path=[IO.Path]::GetFullPath((Join-Path $packet $Matches[2]))
        Require ($path.StartsWith([IO.Path]::GetFullPath($packet)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Debug seal path escapes packet.'
        Require ([string]::Equals((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash,$sha,[StringComparison]::OrdinalIgnoreCase)) "Debug bytes changed: $path"
        $count++
    }
    return $count
}
$sealedBefore=Test-PacketSeal
$pr=Get-Content -Raw -LiteralPath (Join-Path $packet 'pr.receipt.txt') | ConvertFrom-Json -AsHashtable
$merge=Get-Content -Raw -LiteralPath (Join-Path $packet 'merge.receipt.txt') | ConvertFrom-Json -AsHashtable
$job=Get-Content -Raw -LiteralPath (Join-Path $packet 'job.receipt.txt') | ConvertFrom-Json -AsHashtable
Require ($pr['head']['sha'] -ceq '1cdbfde4c93bafa6a0c794fd80939387467fefa4' -and $pr['base']['sha'] -ceq 'ec680e866a218b57000a1780018c00101a437f91') 'Debug packet names another head/base.'
Require ($merge['sha'] -ceq $pr['merge_commit_sha'] -and $merge['parents'].Count -eq 2 -and $merge['parents'][0]['sha'] -ceq $pr['base']['sha'] -and $merge['parents'][1]['sha'] -ceq $pr['head']['sha']) 'Debug tested merge parents differ.'
Require ($job['id'] -eq 114550858183 -and $job['run_id'] -eq 38166757540 -and $job['run_attempt'] -eq 1 -and $job['name'] -ceq 'native (x64, windows-2025-vs2026, Debug, 40)') 'Debug job attempt/profile differs.'
Assert-NativeProducerStep $job 0
$jobText=[IO.File]::ReadAllText((Join-Path $packet 'job-114550858183.log'))
$scopes=Get-Content -Raw -LiteralPath (Join-Path $repo 'Tests/test-scopes.json') | ConvertFrom-Json -AsHashtable
$profile=@($scopes['prCoverage'] | Where-Object { $_['platform'] -ceq 'x64' -and $_['configuration'] -ceq 'Debug' })
Require ($profile.Count -eq 1) 'Missing actual Debug PR scope.'
$expected=@($profile[0]['scopes'] | Where-Object { $_ -cne 'Tooling' })+@('Menu','NativeTextInput')
$artifact=Join-Path $packet 'artifact'
$native=[Collections.Generic.List[object]]::new()
foreach($suite in $expected){
    $record=Receipt $artifact $suite x64 Debug
    $value=$record['value']
    Assert-CompletedInJob $value $job
    Require ((Read-ExactExitCode $value) -eq 0 -and @($value['skips']).Count -eq 0) "Real Debug suite is incomplete: $suite"
    Assert-Sha $value['sha256'] 'real native executable'
    Require ([string]::Equals([string]$value['nativeArchitecture'],'x64',[StringComparison]::OrdinalIgnoreCase)) 'Real Debug suite is not native x64.'
    if($suite -cin @('Menu','NativeTextInput')){Assert-HostedLease $value $false}
    $log=Assert-NativeSuiteLog $artifact $value $suite x64 Debug
    $performanceLeaf=([string]$value['performanceReport'] -split '[/\\]')[-1]
    $performancePath=Find-File $artifact $performanceLeaf
    $performance=Get-Content -Raw -LiteralPath $performancePath | ConvertFrom-Json -AsHashtable
    Require ($performance['sourceCommit'] -ceq $merge['sha'] -and $performance['sourceDirty'] -is [bool] -and $performance['sourceDirty'] -eq $false) 'Real Debug suite benchmark names a different or dirty tested merge.'
    $native.Add([ordered]@{suite=$suite;receipt=$record['file'];receiptSha256=$record['sha256'];log=$log;logSha256=(Get-FileHash -LiteralPath $log).Hash;performance=$performancePath;lease=$(if($value.Contains('lease')){$value['lease']}else{$null})})
}
Require ($native.Count -eq 21) 'Real Debug native inventory is not complete.'
$consumer=Receipt $artifact ExternalConsumer x64 Debug $false
$api=Read-ExactInteger (Get-Content -Raw -LiteralPath (Join-Path $repo 'capabilities.json') | ConvertFrom-Json -AsHashtable) 'apiRevision'
$fingerprint=Assert-ConsumerMetadata $consumer['value'] $merge['sha'] x64 $false $api
Assert-CompletedInJob $consumer['value'] $job
$consumerStep=@($job['steps'] | Where-Object { $_['name'] -ceq 'Qualify the relocated public consumer' })
Require ($consumerStep.Count -eq 1 -and $consumerStep[0]['conclusion'] -ceq 'success') 'Real Debug consumer stage did not complete.'
Require ([regex]::Matches($jobText,'PASS relocated exact-pin consumer, rendering and 10 rejected pin/build mismatches').Count -eq 1 -and [regex]::Matches($jobText,'Native module fixture: 0 failed probes across EXE \+ two DLLs').Count -eq 1) 'Real Debug consumer final completion is missing.'
$sealedAfter=Test-PacketSeal
Require ($sealedAfter -eq $sealedBefore) 'Debug packet seal changed.'
$result=[ordered]@{status='CURRENT_DEBUG_EXTERNAL_HELPER_COMPATIBILITY_PASS';runId=$job['run_id'];attempt=$job['run_attempt'];job=$job['id'];head=$pr['head']['sha'];base=$pr['base']['sha'];testedMerge=$merge['sha'];testedTree=$merge['tree']['sha'];sealedFiles=$sealedBefore;native=$native.ToArray();consumer=$consumer['file'];consumerFingerprint=$fingerprint;apiRevision=$api;helperSha256=(Get-FileHash -LiteralPath $helper).Hash;scope='Read-only compatibility of newly added evidence checks against one completed Debug artifact; not six-profile CI qualification or ci-gate success.';completedUtc=[DateTime]::UtcNow.ToString('o')}
$output=Join-Path $repairRoot 'current-debug-compatibility.receipt.txt'
Require (-not (Test-Path -LiteralPath $output)) 'Refusing to overwrite compatibility evidence.'
[IO.File]::WriteAllText($output,($result | ConvertTo-Json -Depth 30),$utf8)
Write-Host "DEBUG_COMPATIBILITY_PASS: 21 native suite logs, 2 current hosted leases, 1 exact consumer, $sealedBefore sealed files; $output"
