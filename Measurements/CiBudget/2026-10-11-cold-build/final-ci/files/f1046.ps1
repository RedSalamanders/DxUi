<# Independent raw binding/budget review. Does not qualify policy, project away judge disagreement, rerun, or merge. #>
[CmdletBinding()] param([Parameter(Mandatory)][string] $CollectionRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$CollectionRoot=[IO.Path]::GetFullPath($CollectionRoot)
$external=[IO.Path]::GetFullPath('C:/Users/eric/.codex/tmp/dpf-s0-s2-merge')
if(-not $CollectionRoot.StartsWith($external+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Outside task evidence.'}
$helper=Join-Path $PSScriptRoot 'Review-PrCiEvidence.ps1'
if((Get-FileHash -LiteralPath $helper).Hash -cne 'D366AC86418220A88E7E49A5B59EC5EB69285AEA7AF7C55E6D5361E2BD55797A'){throw 'Frozen reviewed helper changed.'}
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($helper,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Frozen helper parse failed.'}
$functionBindings=[Collections.Generic.List[object]]::new()
foreach($name in @('Require','Digest','Receipt-Instant','Assert-Freeze','Assert-Seal','Artifact-Root','Find-File','Assert-CompletedInJob','Assert-Sha','Read-ExactInteger','Assert-PairedRawBudgetAndFixture','Bind-PairedRoleReceipt','Bind-UploadedBuildAttestation')){
    $definition=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $name},$false))
    if($definition.Count -ne 1){throw ('Missing/duplicate frozen function: '+$name)}
    . ([scriptblock]::Create($definition[0].Extent.Text))
    $functionBindings.Add([ordered]@{name=$name;sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($definition[0].Extent.Text)))})
}
$utf8=[Text.UTF8Encoding]::new($false)
$collection=Get-Content -LiteralPath (Join-Path $CollectionRoot 'collection.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable
$RepositoryRoot=[string]$collection['repositoryRoot']
$initialSeal=(Get-FileHash -LiteralPath (Join-Path $CollectionRoot 'SHA256SUMS')).Hash
Require ($collection['expectedHead'] -ceq '4bd3480170140090ab2d8ff883d05507281b52ee' -and $collection['testedMerge'] -ceq '80d875a1de91d2b2c8602faa5614cf75eab6025b' -and $collection['expectedBase'] -ceq 'ec680e866a218b57000a1780018c00101a437f91' -and $collection['runId'] -eq 38172114803 -and $collection['attempt'] -eq 1) 'Wrong source/run.'
Assert-Freeze;Assert-Seal
foreach($module in @('PerformanceComparison','PerformancePolicy','PairedRun','BenchmarkGate')){Import-Module (Join-Path $RepositoryRoot ('Tools/'+$module+'.psm1')) -Force}
$jobs=@(Get-Content -LiteralPath (Join-Path $CollectionRoot 'jobs.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable)
$pairJob=@($jobs|Where-Object{$_['name'] -clike 'paired-benchmark*'})
Require ($pairJob.Count -eq 1 -and $pairJob[0]['status'] -ceq 'completed') 'Unique completed paired job required.'
$pairJob=$pairJob[0]
$artifact=Artifact-Root 'paired-benchmark-x64-Release'
$summaryPath=Find-File $artifact 'summary.json';$reports=[IO.Path]::GetDirectoryName($summaryPath)
$summary=Read-GateJson $summaryPath
Require ($summary['baseline']['commit'] -ceq $collection['expectedBase'] -and $summary['candidate']['commit'] -ceq $collection['testedMerge'] -and $summary['harnessCommit'] -ceq $collection['testedMerge'] -and $summary['harnessDirty'] -eq $false) 'Wrong paired role/harness source.'
Require ((Read-ExactInteger $summary 'studyVersion') -eq 3 -and (Read-ExactInteger $summary 'blocks') -eq 12 -and $summary['studyPurpose'] -ceq 'regression-qualification' -and $summary['platform'] -ceq 'x64' -and $summary['configuration'] -ceq 'Release') 'Wrong study protocol/profile.'
$expectedHarness=@(Get-PairedHarness)+@('build.ps1','Tools/ScopedTesting.psm1','Tools/Validation.psm1','Tools/Changelog.psm1')
Require ($summary['harness'].Count -eq 17 -and $expectedHarness.Count -eq 17) 'Wrong harness inventory.'
foreach($path in $expectedHarness){Require ($summary['harness'].Contains($path) -and (Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $path)).Hash -ceq $summary['harness'][$path]) 'Actual current harness bytes differ.'}
$overlays=$expectedHarness+@('Tests/Embedded/BenchmarkMain.h','Tests/Embedded/ComplexUiBenchmark.h','Tests/Support/HeapDiagnostic.h')
$sideStates=@{};$raw=[Collections.Generic.List[object]]::new();$exact=[Collections.Generic.List[object]]::new();$judgments=[Collections.Generic.List[object]]::new()
$roundCount=0;$metricCount=0
Require ($summary['scenarios'].Count -eq 3 -and (@($summary['scenarios']|ForEach-Object{$_['scenario']}|Sort-Object)-join ',') -ceq 'Default,MultilineGrid,MultilineGridDistinct') 'Wrong scenario inventory.'
Require ($summary['steps'].Count -eq 48) 'Wrong scheduled report count.'
foreach($scene in $summary['scenarios']){
    $names=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    Require ($scene['runs'].Count -eq 48 -and $scene['judgeComparison']['sameReceiptsSha256'].Count -eq 48 -and $scene['set']['metrics'].Count -eq 26) 'Incomplete scenario reports/metrics.'
    $metricCount+=$scene['set']['metrics'].Count
    foreach($record in $scene['judgeComparison']['sameReceiptsSha256']){
        $run=[string]$record['run'];Require ($names.Add($run)) 'Duplicate report.'
        $path=Join-Path $reports ($scene['scenario']+'-'+$run+'.json')
        $sha=(Get-FileHash -LiteralPath $path).Hash;Require ($sha -ceq $record['sha256']) 'Judged raw bytes changed.'
        $value=Read-PerformanceReceipt $path
        $identity=Test-PerformanceReceiptIdentity -Receipt $value
        Require $identity.verifiable 'Raw v2 build/report identity is not verifiable.'
        Assert-CompletedInJob $value $pairJob
        Bind-PairedRoleReceipt $summary $scene $run $value $sideStates $overlays
        Assert-PairedRawBudgetAndFixture $value ([string]$scene['scenario'])
        Require ((Read-ExactInteger $value 'hiddenPreparations') -eq 0 -and (Read-ExactInteger $value 'hiddenComposites') -eq 0) 'Hidden work failed.'
        $dirtyPeak=0
        foreach($phase in $value['scenarios']){foreach($round in $phase['rounds']){
            $roundCount++
            Require ((Read-ExactInteger $round 'composeAllocations') -eq 0) 'Composition budget failed.'
            $allocations=Read-ExactInteger $round 'cppAllocations'
            Require ($allocations -ge 0 -and $(if($phase['name'] -ceq 'clean'){$allocations -eq 0}else{$allocations -le 2560})) 'Whole-frame typed allocation budget failed.'
            if($phase['name'] -ceq 'dirty'){$dirtyPeak=[Math]::Max($dirtyPeak,$allocations)}
        }}
        $attestation=Bind-UploadedBuildAttestation $artifact $value $pairJob
        $raw.Add([ordered]@{scenario=$scene['scenario'];run=$run;file=$path;sha256=$sha;sourceCommit=$value['sourceCommit'];sourceFingerprint=$value['sourceFingerprint'];sourceDirty=$value['sourceDirty'];executableSha256=$value['executableSha256'];maximumDirtyCppPerRound=$dirtyPeak;uploadedAttestation=$attestation})
    }
    $scheduled=@($summary['steps']|ForEach-Object{$_['Name']}|Sort-Object)-join ','
    Require ((@($names|Sort-Object)-join ',') -ceq $scheduled -and (@($scene['runs'].Keys|Sort-Object)-join ',') -ceq $scheduled) 'Raw reports do not equal scheduled and retained roles.'
    foreach($metric in $scene['set']['metrics']){if($metric['exact']){Require ($metric['verdict'] -cne 'regressed') 'Exact resource regression.';$exact.Add($metric)}}
    $comparison=$scene['judgeComparison']
    $judgments.Add([ordered]@{scenario=$scene['scenario'];recordedAgreement=$comparison['verdictAgreement'];legacyStatus=$comparison['legacyJudge']['status'];migratedStatus=$comparison['migratedJudge']['status'];legacyRegressions=$comparison['legacyJudge']['regressedMetrics'];migratedRegressions=$comparison['migratedJudge']['regressedMetrics'];scope='Recorded judgments preserved verbatim. Numeric judge/control replay and explicit policy review are separate; this raw binding receipt cannot qualify them.'})
}
Require ($raw.Count -eq 144 -and $roundCount -eq 1440 -and $metricCount -eq 78 -and $exact.Count -eq 24 -and $sideStates.Count -eq 2) 'Incomplete paired raw/accounting inventory.'
Assert-Freeze;Assert-Seal
Require ((Get-FileHash -LiteralPath (Join-Path $CollectionRoot 'SHA256SUMS')).Hash -ceq $initialSeal) 'Original seal identity changed.'
$output=Join-Path $CollectionRoot ('root-paired-raw-bindings-'+[guid]::NewGuid().ToString('N')+'.receipt.txt')
$result=[ordered]@{status='ROOT_PAIRED_RAW_BINDINGS_AND_BUDGETS_VERIFIED_NOT_QUALIFIED';runId=38172114803;head=$collection['expectedHead'];base=$collection['expectedBase'];testedMerge=$collection['testedMerge'];originalSealSha256=$initialSeal;driverSha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash;frozenHelperSha256=(Get-FileHash -LiteralPath $helper).Hash;exactHelperFunctions=$functionBindings.ToArray();rawReports=$raw.Count;rounds=$roundCount;metricDecisions=$metricCount;exactBudgets=$exact.Count;harnessFiles=$expectedHarness.Count;roleIdentities=$sideStates;measuredLibraryChanged=($sideStates['baseline']['sourceFingerprint'] -cne $sideStates['candidate']['sourceFingerprint']);reports=$raw.ToArray();recordedJudgeDisagreements=$judgments.ToArray();limits='No numeric dual-judge/control replay, policy qualification, successful gate or integration acceptance is claimed by this raw binding check.';recordedUtc=[DateTimeOffset]::UtcNow.ToString('o')}
[IO.File]::WriteAllText($output,(($result|ConvertTo-Json -Depth 30)+"`n"),$utf8)
Write-Output ('PAIRED_RAW_BINDINGS_VERIFIED_NOT_QUALIFIED: 144 reports/1440 rounds/78 slots/24 exact budgets; '+$output)
