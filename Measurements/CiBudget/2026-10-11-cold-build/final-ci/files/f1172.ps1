<# Run only on an isolated documentation branch AFTER the independently reviewed ordinary remote merge.
Retains exact raw bytes under the existing immutable evidence attributes; performs no Git or remote mutation. #>
[CmdletBinding()] param(
    [Parameter(Mandatory)][string] $RepositoryRoot,
    [Parameter(Mandatory)][string] $CollectionRoot,
    [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{40}$')][string] $MergedHead,
    [Parameter(Mandatory)][string] $MergeReceipt,
    [Parameter(Mandatory)][string[]] $AdditionalRootFiles
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$utf8 = [Text.UTF8Encoding]::new($false)
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$CollectionRoot = [IO.Path]::GetFullPath($CollectionRoot)
$external = [IO.Path]::GetFullPath('C:/Users/eric/.codex/tmp/dpf-s0-s2-merge')
function Require([bool] $condition,[string] $message) { if (-not $condition) { throw $message } }
function Write-NewText([string] $path,[string] $text) {
    $stream = [IO.File]::Open($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try { $bytes = $utf8.GetBytes($text); $stream.Write($bytes,0,$bytes.Length); $stream.Flush($true) }
    finally { $stream.Dispose() }
}
Require ($CollectionRoot.StartsWith($external + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Collection is outside task evidence.'
$inputSealPath = Join-Path $CollectionRoot 'SHA256SUMS'
$inputSealSha256 = (Get-FileHash -LiteralPath $inputSealPath).Hash
$inputSealLines = [IO.File]::ReadAllLines($inputSealPath)
function Assert-InputSeal {
    Require ((Get-FileHash -LiteralPath $inputSealPath).Hash -ceq $inputSealSha256) 'Original CI seal identity changed.'
    Require ($inputSealLines.Count -gt 0) 'Original CI seal is empty.'
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($line in $inputSealLines) {
        Require ($line -cmatch '^([a-f0-9]{64})  (.+)$') 'Malformed original CI seal.'
        $sha = $Matches[1]; $path = [IO.Path]::GetFullPath((Join-Path $CollectionRoot $Matches[2]))
        Require ($path.StartsWith($CollectionRoot + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Original seal escapes collection.'
        Require ($seen.Add($path)) 'Original CI seal contains a duplicate or case alias.'
        Require ((Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -ceq $sha) 'Original CI bytes changed.'
    }
}
$head = (& git -C $RepositoryRoot rev-parse HEAD).Trim()
Require ($LASTEXITCODE -eq 0 -and $head -ceq $MergedHead) 'Archive must start at verified merged source.'
$status = @(& git -C $RepositoryRoot status --porcelain=v1 --untracked-files=normal)
Require ($LASTEXITCODE -eq 0 -and $status.Count -eq 0) 'Archive source must begin clean.'
$branch = (& git -C $RepositoryRoot branch --show-current).Trim()
Require ($LASTEXITCODE -eq 0 -and $branch.StartsWith('codex/',[StringComparison]::Ordinal)) 'Use an isolated documentation feature branch.'
$tree = (& git -C $RepositoryRoot rev-parse 'HEAD^{tree}').Trim()
Require ($LASTEXITCODE -eq 0 -and $tree -ceq 'e695cf74db0e2fc1a5c8c8d183cfb0bd136d639d') 'Merged source differs from tested implementation tree.'
$parents = ((& git -C $RepositoryRoot show -s --format=%P HEAD).Trim() -split ' ')
Require ($LASTEXITCODE -eq 0 -and $parents.Count -eq 2 -and $parents[0] -ceq 'ec680e866a218b57000a1780018c00101a437f91' -and $parents[1] -ceq '4bd3480170140090ab2d8ff883d05507281b52ee') 'Ordinary merge parents differ.'
Assert-InputSeal
$collection = Get-Content -LiteralPath (Join-Path $CollectionRoot 'collection.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable
$expectedCollection = [ordered]@{version=1;repository='RedSalamanders/DxUi';runId=38172114803;attempt=1;pullRequest=73;
    expectedHead='4bd3480170140090ab2d8ff883d05507281b52ee';expectedBase='ec680e866a218b57000a1780018c00101a437f91';
    testedMerge='80d875a1de91d2b2c8602faa5614cf75eab6025b';testedTree=$tree;runConclusion='failure';
    workflowDigest='aee4e10d1136b1c5bda45be06591cb02644ec13decf94370a0f97d0c6b9ee1df'}
foreach ($key in $expectedCollection.Keys) {
    $value = $collection[$key]; $expectedValue = $expectedCollection[$key]
    if ($expectedValue -is [int] -or $expectedValue -is [long]) {
        Require (($value -is [int] -or $value -is [long]) -and $value -eq $expectedValue) ('Collection integer identity differs: '+$key)
    } else {
        Require ($value -is [string] -and $value -ceq $expectedValue) ('Collection scalar string identity differs: '+$key)
    }
}
$destination = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'Measurements/CiBudget/2026-10-11-cold-build/final-ci'))
Require ($destination.StartsWith($RepositoryRoot + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -and -not (Test-Path -LiteralPath $destination)) 'Archive destination is unsafe or already exists.'
$attributes = [IO.File]::ReadAllText((Join-Path $RepositoryRoot '.gitattributes'))
Require ($attributes -cmatch '(?m)^Measurements/CiBudget/2026-10-11-cold-build/\*\* -text -whitespace\r?$') 'Reviewed byte-preserving attributes are missing.'
$initialCollectionPaths = @(Get-ChildItem -LiteralPath $CollectionRoot -File -Recurse -Force | Sort-Object FullName | ForEach-Object FullName)
$inputs = [Collections.Generic.List[object]]::new()
$originals = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
function Add-Input([string] $original,[string] $kind,[string] $relative) {
    $original = [IO.Path]::GetFullPath($original)
    Require ($original.StartsWith($external + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $original -PathType Leaf)) 'Input is outside task evidence or absent.'
    Require ($originals.Add($original)) 'Duplicate source input or case alias.'
    $inputs.Add([ordered]@{original=$original;kind=$kind;relative=$relative;bytes=(Get-Item -LiteralPath $original).Length;sha256=(Get-FileHash -LiteralPath $original).Hash})
}
foreach ($file in $initialCollectionPaths) { Add-Input $file 'complete-ci-collection' ([IO.Path]::GetRelativePath($CollectionRoot,$file).Replace('\','/')) }
Add-Input $MergeReceipt 'ordinary-remote-merge-proof' ([IO.Path]::GetFileName($MergeReceipt))
foreach ($file in $AdditionalRootFiles) { Add-Input $file 'independent-root-review' ([IO.Path]::GetFileName($file)) }
Add-Input $PSCommandPath 'archive-driver' ([IO.Path]::GetFileName($PSCommandPath))
function Assert-InputSnapshot {
    Assert-InputSeal
    $current = @(Get-ChildItem -LiteralPath $CollectionRoot -File -Recurse -Force | Sort-Object FullName | ForEach-Object FullName)
    Require ($current.Count -eq $initialCollectionPaths.Count -and ($current -join "`n") -ceq ($initialCollectionPaths -join "`n")) 'Complete CI input inventory changed.'
    foreach ($inputRecord in $inputs) {
        Require ((Get-FileHash -LiteralPath $inputRecord['original']).Hash -ceq $inputRecord['sha256'] -and (Get-Item -LiteralPath $inputRecord['original']).Length -eq $inputRecord['bytes']) 'Pinned original or derived review input changed.'
    }
}
Assert-InputSnapshot
[void][IO.Directory]::CreateDirectory((Join-Path $destination 'files'))
$copies = [Collections.Generic.List[object]]::new()
function Copy-Exact([System.Collections.IDictionary] $inputRecord) {
    $original = [string]$inputRecord['original']; $kind = [string]$inputRecord['kind']; $sourceRelative = [string]$inputRecord['relative']
    $original = [IO.Path]::GetFullPath($original)
    Require ($original.StartsWith($external + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $original -PathType Leaf)) 'Input copy is outside task evidence or absent.'
    $extension = [IO.Path]::GetExtension($original)
    # Retain source/data bytes as evidence without reinterpreting displaced Markdown links or workflow payloads.
    if ($extension -cin @('.json', '.md', '.yml', '.yaml')) { $extension = '.receipt.txt' }
    if (-not $extension) { $extension = '.txt' }
    $relative = 'files/f' + ($copies.Count + 1).ToString('D4') + $extension
    $target = [IO.Path]::GetFullPath((Join-Path $destination $relative))
    Require ($target.StartsWith($destination + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -and -not (Test-Path -LiteralPath $target)) 'Unsafe or duplicate archive copy.'
    Require ([IO.Path]::GetRelativePath($RepositoryRoot,$target).Length -le 150) 'Archive path exceeds repository contract.'
    $sha = [string]$inputRecord['sha256']; $bytes = $inputRecord['bytes']
    Require ((Get-FileHash -LiteralPath $original).Hash -ceq $sha -and (Get-Item -LiteralPath $original).Length -eq $bytes) 'Pinned source changed before copy.'
    [IO.File]::Copy($original,$target,$false)
    Require ((Get-FileHash -LiteralPath $target).Hash -ceq $sha -and (Get-Item -LiteralPath $target).Length -eq $bytes -and (Get-FileHash -LiteralPath $original).Hash -ceq $sha) 'Exact original copy differs or source changed.'
    $copies.Add([ordered]@{kind=$kind;original=$original;originalRelative=$sourceRelative;archive=$relative;bytes=$bytes;sha256=$sha})
}
foreach ($inputRecord in $inputs) { Copy-Exact $inputRecord }
function Assert-ArchiveCopies {
    $current = @(Get-ChildItem -LiteralPath (Join-Path $destination 'files') -File -Recurse -Force | Sort-Object FullName | ForEach-Object FullName)
    $expected = @($copies | ForEach-Object { [IO.Path]::GetFullPath((Join-Path $destination $_['archive'])) } | Sort-Object)
    Require ($current.Count -eq $expected.Count -and ($current -join "`n") -ceq ($expected -join "`n")) 'Exact copied-file inventory differs from its map.'
    foreach ($copy in $copies) {
        $target = Join-Path $destination $copy['archive']
        Require ((Get-FileHash -LiteralPath $target).Hash -ceq $copy['sha256'] -and (Get-Item -LiteralPath $target).Length -eq $copy['bytes']) 'Late target mutation differs from recorded source bytes.'
    }
}
Assert-InputSnapshot
Assert-ArchiveCopies
$map = [ordered]@{head='4bd3480170140090ab2d8ff883d05507281b52ee';base='ec680e866a218b57000a1780018c00101a437f91';testedMerge='80d875a1de91d2b2c8602faa5614cf75eab6025b';testedTree=$tree;ordinaryMergedHead=$MergedHead;runId=38172114803;attempt=1;originalCollection=$CollectionRoot;originalCollectionSealSha256=$inputSealSha256;copies=$copies.ToArray();scope='Implementation-only integration evidence. Literal failed/incomplete policy and verified foreground outcomes remain failed/incomplete; no main-push, hardware/input or actual consumer adoption acceptance.';copiedUtc=[DateTimeOffset]::UtcNow.ToString('o')}
Write-NewText (Join-Path $destination 'copy.receipt.txt') (($map | ConvertTo-Json -Depth 20)+"`n")
$mapSha256 = (Get-FileHash -LiteralPath (Join-Path $destination 'copy.receipt.txt')).Hash
function Assert-CompleteArchiveInventory([bool] $withSeal) {
    $expected = @(@($copies | ForEach-Object { [IO.Path]::GetFullPath((Join-Path $destination $_['archive'])) }) + (Join-Path $destination 'copy.receipt.txt'))
    if ($withSeal) { $expected += Join-Path $destination 'SHA256SUMS' }
    $expected = @($expected | Sort-Object)
    $current = @(Get-ChildItem -LiteralPath $destination -File -Recurse -Force | Sort-Object FullName | ForEach-Object FullName)
    Require ($current.Count -eq $expected.Count -and ($current -join "`n") -ceq ($expected -join "`n")) 'Complete final archive file inventory differs from mapped copies and metadata.'
    $directories = @(Get-ChildItem -LiteralPath $destination -Directory -Recurse -Force)
    Require ($directories.Count -eq 1 -and $directories[0].FullName -ceq (Join-Path $destination 'files')) 'Unexpected final archive directory.'
}
Assert-CompleteArchiveInventory $false
$seal = foreach ($file in Get-ChildItem -LiteralPath $destination -File -Recurse -Force | Sort-Object FullName) {
    (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant() + '  ' + [IO.Path]::GetRelativePath($destination,$file.FullName).Replace('\','/')
}
$sealText = ($seal -join "`n")+"`n"
$sealSha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($utf8.GetBytes($sealText)))
Write-NewText (Join-Path $destination 'SHA256SUMS') $sealText
Require ((Get-FileHash -LiteralPath (Join-Path $destination 'SHA256SUMS')).Hash -ceq $sealSha256) 'Written final seal bytes differ from their pinned identity.'
foreach ($line in $seal) {
    Require ($line -cmatch '^([a-f0-9]{64})  (.+)$') 'Malformed final archive seal.'
    Require ((Get-FileHash -LiteralPath (Join-Path $destination $Matches[2])).Hash.ToLowerInvariant() -ceq $Matches[1]) 'Final archive bytes differ.'
}
Assert-InputSnapshot
Assert-ArchiveCopies
Require ((Get-FileHash -LiteralPath (Join-Path $destination 'copy.receipt.txt')).Hash -ceq $mapSha256) 'Copy map changed after recording.'
Assert-CompleteArchiveInventory $true
Require ((Get-FileHash -LiteralPath (Join-Path $destination 'SHA256SUMS')).Hash -ceq $sealSha256) 'Written final seal identity changed after verification.'
$postHead = (& git -C $RepositoryRoot rev-parse HEAD).Trim()
Require ($LASTEXITCODE -eq 0 -and $postHead -ceq $MergedHead) 'Source HEAD changed while archiving.'
Write-Output ('FINAL_INTEGRATION_CI_ARCHIVED: originals=' + $copies.Count + '; sealed=' + @($seal).Count + '; ' + $destination)
