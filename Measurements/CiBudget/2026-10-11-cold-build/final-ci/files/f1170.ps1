[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$external='C:/Users/eric/.codex/tmp/dpf-s0-s2-merge'
$repo='C:/Users/eric/.codex/worktrees/integration-evidence/DxUi'
$collection=Join-Path $external 'ci-evidence/pr-73-run-38172114803-attempt-1-858f3ba12193400b84801b3a1fa75ac2'
$additional=[Collections.Generic.List[string]]::new()
foreach($file in Get-ChildItem -LiteralPath (Join-Path $external 'ci-review-tools') -File -Recurse -Force){$additional.Add($file.FullName)}
foreach($folder in @('paired-root-replay-run399-01','paired-independent-run399-01','early-ci-ordinary-399-739033b29d9444de8cd9988f45501ea8')){
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $external $folder) -File -Recurse -Force){$additional.Add($file.FullName)}
}
$functional=Get-Content -LiteralPath (Join-Path $external 'early-ci-native-cold-budget/root-functional-visual-399-318fd6cdb9ac4db68bcf4a942c7321f0.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable
foreach($profile in $functional.profiles){$additional.Add([string]$profile.review)}
foreach($relative in @(
    'root-descriptive-performance-review-399.receipt.txt',
    'early-ci-native-cold-budget/root-arm-before-suite-input-399.receipt.txt',
    'early-ci-native-cold-budget/root-native-agent-crosscheck-399.receipt.txt',
    'early-ci-native-cold-budget/root-functional-visual-399-318fd6cdb9ac4db68bcf4a942c7321f0.receipt.txt',
    'early-ci-native-cold-budget/root-viewed-asan-gallery-399.receipt.txt',
    'early-ci-native-cold-budget/Review-FunctionalVisual-399.ps1',
    'early-ci-native-cold-budget/Record-RootArmRefusals-399.ps1',
    'early-ci-native-cold-budget/Record-RootArmRefusals-399.before-git-filter.ps1',
    'early-ci-native-cold-budget/root-arm-refusal-git-filter-attempt.receipt.txt',
    'early-ci-native-398/root-x64-functional-visual.receipt.txt',
    'prepush399-explain.log',
    'pr-body.before-final-ci.md',
    'pr-body-final-review.md',
    'Merge-ReviewedImplementation.ps1',
    'ordinary-remote-merge-73.before.receipt.txt',
    'ordinary-remote-merge-73.response.receipt.txt',
    'Archive-FinalIntegrationCi.before-source-extension.ps1',
    'Archive-FinalIntegrationCi.before-immutable-inventory.ps1',
    'Archive-FinalIntegrationCi.before-complete-seal.ps1'
)){$additional.Add((Join-Path $external $relative))}
$additional.Add($PSCommandPath)
foreach($path in @($repo,$collection,$additional.ToArray())){
    $item=Get-Item -LiteralPath $path -Force
    if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'A selected source is a reparse point.'}
}
if(@(Get-ChildItem -LiteralPath $collection -Recurse -Force | Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}).Count -ne 0){throw 'Original collection contains a reparse point.'}
$attrs=@(& git -C $repo check-attr text whitespace filter -- 'Measurements/CiBudget/2026-10-11-cold-build/final-ci/files/probe.receipt.txt')
if($LASTEXITCODE -ne 0 -or -not($attrs[0].EndsWith(': text: unset')) -or -not($attrs[1].EndsWith(': whitespace: unset')) -or -not($attrs[2].EndsWith(': filter: unspecified'))){throw 'Effective Git evidence attributes differ.'}
if((Get-FileHash -LiteralPath (Join-Path $external 'Archive-FinalIntegrationCi.ps1')).Hash -cne '4B848ED15F4FCDDF51400476C9FBEDCF74F72FC3F6DF07719DE04B0DF816AF45'){throw 'Independently reviewed archive driver changed.'}
& (Join-Path $external 'Archive-FinalIntegrationCi.ps1') -RepositoryRoot $repo -CollectionRoot $collection -MergedHead '670207429d62ecc541769b5dd6c70b9607745e64' -MergeReceipt (Join-Path $external 'ordinary-remote-merge-73.receipt.txt') -AdditionalRootFiles $additional.ToArray()
