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
function Assert-InputSeal {
    foreach ($line in [IO.File]::ReadAllLines((Join-Path $CollectionRoot 'SHA256SUMS'))) {
        Require ($line -cmatch '^([a-f0-9]{64})  (.+)$') 'Malformed original CI seal.'
        $sha = $Matches[1]; $path = [IO.Path]::GetFullPath((Join-Path $CollectionRoot $Matches[2]))
        Require ($path.StartsWith($CollectionRoot + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Original seal escapes collection.'
        Require ((Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -ceq $sha) 'Original CI bytes changed.'
    }
}
Require ($CollectionRoot.StartsWith($external + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Collection is outside task evidence.'
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
$destination = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'Measurements/CiBudget/2026-10-11-cold-build/final-ci'))
Require ($destination.StartsWith($RepositoryRoot + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -and -not (Test-Path -LiteralPath $destination)) 'Archive destination is unsafe or already exists.'
$attributes = [IO.File]::ReadAllText((Join-Path $RepositoryRoot '.gitattributes'))
Require ($attributes -cmatch '(?m)^Measurements/CiBudget/2026-10-11-cold-build/\*\* -text -whitespace\r?$') 'Reviewed byte-preserving attributes are missing.'
[void][IO.Directory]::CreateDirectory((Join-Path $destination 'files'))
$copies = [Collections.Generic.List[object]]::new()
function Copy-Exact([string] $original,[string] $kind,[string] $sourceRelative) {
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
    $sha = (Get-FileHash -LiteralPath $original).Hash; $bytes = (Get-Item -LiteralPath $original).Length
    Copy-Item -LiteralPath $original -Destination $target
    Require ((Get-FileHash -LiteralPath $target).Hash -ceq $sha -and (Get-Item -LiteralPath $target).Length -eq $bytes -and (Get-FileHash -LiteralPath $original).Hash -ceq $sha) 'Exact original copy differs or source changed.'
    $copies.Add([ordered]@{kind=$kind;original=$original;originalRelative=$sourceRelative;archive=$relative;bytes=$bytes;sha256=$sha})
}
foreach ($file in Get-ChildItem -LiteralPath $CollectionRoot -File -Recurse -Force | Sort-Object FullName) {
    Copy-Exact $file.FullName 'complete-ci-collection' ([IO.Path]::GetRelativePath($CollectionRoot,$file.FullName).Replace('\','/'))
}
Copy-Exact $MergeReceipt 'ordinary-remote-merge-proof' ([IO.Path]::GetFileName($MergeReceipt))
foreach ($file in $AdditionalRootFiles) { Copy-Exact $file 'independent-root-review' ([IO.Path]::GetFileName($file)) }
Copy-Exact $PSCommandPath 'archive-driver' ([IO.Path]::GetFileName($PSCommandPath))
Assert-InputSeal
$map = [ordered]@{head='4bd3480170140090ab2d8ff883d05507281b52ee';base='ec680e866a218b57000a1780018c00101a437f91';testedMerge='80d875a1de91d2b2c8602faa5614cf75eab6025b';testedTree=$tree;ordinaryMergedHead=$MergedHead;runId=38172114803;attempt=1;originalCollection=$CollectionRoot;originalCollectionSealSha256=(Get-FileHash -LiteralPath (Join-Path $CollectionRoot 'SHA256SUMS')).Hash;copies=$copies.ToArray();scope='Implementation-only integration evidence. Literal failed/incomplete policy and verified foreground outcomes remain failed/incomplete; no main-push, hardware/input or actual consumer adoption acceptance.';copiedUtc=[DateTimeOffset]::UtcNow.ToString('o')}
[IO.File]::WriteAllText((Join-Path $destination 'copy.receipt.txt'),(($map | ConvertTo-Json -Depth 20)+"`n"),$utf8)
$seal = foreach ($file in Get-ChildItem -LiteralPath $destination -File -Recurse -Force | Sort-Object FullName) {
    (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant() + '  ' + [IO.Path]::GetRelativePath($destination,$file.FullName).Replace('\','/')
}
[IO.File]::WriteAllText((Join-Path $destination 'SHA256SUMS'),(($seal -join "`n")+"`n"),$utf8)
foreach ($line in $seal) {
    Require ($line -cmatch '^([a-f0-9]{64})  (.+)$') 'Malformed final archive seal.'
    Require ((Get-FileHash -LiteralPath (Join-Path $destination $Matches[2])).Hash.ToLowerInvariant() -ceq $Matches[1]) 'Final archive bytes differ.'
}
$postHead = (& git -C $RepositoryRoot rev-parse HEAD).Trim()
Require ($LASTEXITCODE -eq 0 -and $postHead -ceq $MergedHead) 'Source HEAD changed while archiving.'
Write-Output ('FINAL_INTEGRATION_CI_ARCHIVED: originals=' + $copies.Count + '; sealed=' + @($seal).Count + '; ' + $destination)
