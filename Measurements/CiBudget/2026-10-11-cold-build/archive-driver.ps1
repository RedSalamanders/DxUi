Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$external='C:/Users/eric/.codex/tmp/dpf-s0-s2-merge'
$source=Join-Path $external 'arm64-timing-398-b6b32d27cc974c33acd467fdcc74b04a'
$repo='C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$destination=Join-Path $repo 'Measurements/CiBudget/2026-10-11-cold-build'
$utf8=[Text.UTF8Encoding]::new($false)
$copies=[Collections.Generic.List[object]]::new()
if((Get-FileHash -LiteralPath (Join-Path $source 'SHA256SUMS')).Hash -cne '1E504F8133E526FF3D58513CECD5B3BC8F1C7ACF8776D739B31305750E67E02D'){throw 'Agent packet seal differs.'}
foreach($line in [IO.File]::ReadAllLines((Join-Path $source 'SHA256SUMS'))){
    if($line -cnotmatch '^([a-f0-9]{64})  (.+)$'){throw 'Bad packet seal.'}
    $sha=$Matches[1];$path=[IO.Path]::GetFullPath((Join-Path $source $Matches[2]))
    if(-not $path.StartsWith($source + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or -not [string]::Equals((Get-FileHash -LiteralPath $path).Hash,$sha,[StringComparison]::OrdinalIgnoreCase)){throw 'Original source packet changed.'}
}
$binding=Get-Content -LiteralPath (Join-Path $source 'source-bindings.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable
foreach($input in $binding['verifiedInputs']){
    $path=Join-Path $source $input['retainedPath']
    $blob=(& git -C $repo hash-object --no-filters -- $path).Trim()
    if($LASTEXITCODE -ne 0 -or $blob -cne $input['gitBlob'] -or -not [string]::Equals((Get-FileHash -LiteralPath $path).Hash,$input['sha256'],[StringComparison]::OrdinalIgnoreCase)){throw 'Actual failed-source Git blob differs.'}
}
function Copy-Exact([string]$Original,[string]$Relative){
    $target=[IO.Path]::GetFullPath((Join-Path $destination $Relative))
    if(-not $target.StartsWith($destination + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $target)){throw 'Archive target unsafe or already exists.'}
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))
    Copy-Item -LiteralPath $Original -Destination $target
    $sha=(Get-FileHash -LiteralPath $Original).Hash
    if((Get-FileHash -LiteralPath $target).Hash -cne $sha){throw 'Archive copy differs.'}
    $copies.Add([ordered]@{original=$Original;archive=$Relative;bytes=(Get-Item -LiteralPath $Original).Length;sha256=$sha})
}
foreach($file in Get-ChildItem -LiteralPath $source -File -Recurse){Copy-Exact $file.FullName ('source-evidence/' + [IO.Path]::GetRelativePath($source,$file.FullName).Replace('\','/'))}
$artifacts=Get-Content -LiteralPath (Join-Path $source 'artifact-bindings.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable
foreach($profile in $artifacts){
    foreach($entry in $profile['fileInventory']){
        $original=Join-Path $profile['originalCaptureRoot'] $entry['path']
        if(-not [string]::Equals((Get-FileHash -LiteralPath $original).Hash,$entry['sha256'],[StringComparison]::OrdinalIgnoreCase) -or (Get-Item -LiteralPath $original).Length -ne $entry['bytes']){throw 'Agent artifact inventory changed.'}
        $relative=$entry['path']
        if($relative.EndsWith('.json',[StringComparison]::OrdinalIgnoreCase)){$relative += '.receipt.txt'}
        Copy-Exact $original ('native-artifacts/' + $profile['profile'] + '/' + $relative)
    }
}
# Original names and hashes remain in this copy map and original seals; JSON metadata/raw samples are not canonical benchmarks.
$copyReceipt=[ordered]@{sourceHead='1cdbfde4c93bafa6a0c794fd80939387467fefa4';testedMerge=$binding['testedMerge'];run=38166757540;attempt=1;copies=$copies.ToArray();originalTimingSealSha256=(Get-FileHash -LiteralPath (Join-Path $source 'SHA256SUMS')).Hash;scope='Failed Full timeouts and independently completed consumer evidence; no qualification of the repaired source.';completedUtc=[DateTime]::UtcNow.ToString('o')}
[IO.File]::WriteAllText((Join-Path $destination 'copy.receipt.txt'),($copyReceipt|ConvertTo-Json -Depth 12),$utf8)
Copy-Exact $PSCommandPath 'archive-driver.ps1'
$tokens=$null;$errors=$null
$collector=Join-Path $external 'ci-review-tools/Collect-PrCiEvidence.ps1'
$ast=[Management.Automation.Language.Parser]::ParseFile($collector,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Byte reader parse error.'}
$fn=@($ast.FindAll({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Invoke-GhBytes'},$true))
if($fn.Count -ne 1){throw 'Missing byte reader.'}
. ([scriptblock]::Create($fn[0].Extent.Text))
foreach($entry in @(@{name='run-after-cancel.receipt.txt';args=@('api','repos/RedSalamanders/DxUi/actions/runs/38166757540')},@{name='paired-after-cancel.receipt.txt';args=@('api','repos/RedSalamanders/DxUi/actions/jobs/114550798583')},@{name='paired-after-cancel.log';args=@('api','repos/RedSalamanders/DxUi/actions/jobs/114550798583/logs','--allow-escape-sequences')})){
    [IO.File]::WriteAllBytes((Join-Path $destination $entry.name),(Invoke-GhBytes $entry.args))
}
$after=Get-Content -LiteralPath (Join-Path $destination 'run-after-cancel.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable
if($after['status'] -cne 'completed' -or $after['conclusion'] -cne 'cancelled' -or $after['head_sha'] -cne $copyReceipt.sourceHead -or $after['run_attempt'] -ne 1){throw 'Cancelled run binding differs.'}
$sealPath=Join-Path $destination 'SHA256SUMS'
$seal=foreach($file in Get-ChildItem -LiteralPath $destination -File -Recurse | Where-Object{$_.FullName -cne $sealPath} | Sort-Object FullName){
    (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant() + '  ' + [IO.Path]::GetRelativePath($destination,$file.FullName).Replace('\','/')
}
[IO.File]::WriteAllText($sealPath,(($seal -join "`n")+"`n"),$utf8)
foreach($line in [IO.File]::ReadAllLines($sealPath)){
    if($line -cnotmatch '^([a-f0-9]{64})  (.+)$'){throw 'Archive seal malformed.'}
    if(-not [string]::Equals((Get-FileHash -LiteralPath (Join-Path $destination $Matches[2])).Hash,$Matches[1],[StringComparison]::OrdinalIgnoreCase)){throw 'Archive seal verification failed.'}
}
Write-Host "COLD_BUDGET_FAILED_EVIDENCE_RETAINED: $($copies.Count) exact original copies; $($seal.Count) sealed files; both Git blobs root reverified; $destination"
