Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$external = 'C:/Users/eric/.codex/tmp/dpf-s0-s2-merge'
$log = Join-Path $external 'native-cold-budget-validate-final.log'
$receiptPath = Join-Path $external 'native-cold-budget-validate-final.receipt.txt'
if ((Test-Path -LiteralPath $log) -or (Test-Path -LiteralPath $receiptPath)) { throw 'Final validation output exists.' }
function Snapshot {
    $head = (& git -C $repo rev-parse HEAD).Trim()
    if ($LASTEXITCODE) { throw 'HEAD read failed.' }
    $status = @(& git -C $repo status --porcelain --untracked-files=normal)
    if ($LASTEXITCODE) { throw 'Status read failed.' }
    $files = @(& git -C $repo ls-files --cached --others --exclude-standard)
    if ($LASTEXITCODE) { throw 'Input inventory failed.' }
    $inventory = [ordered]@{}
    foreach ($relative in $files | Sort-Object -Unique) {
        $path = Join-Path $repo $relative
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $inventory[$relative] = [ordered]@{ bytes = (Get-Item -LiteralPath $path).Length; sha256 = (Get-FileHash -LiteralPath $path).Hash }
        } else {
            $inventory[$relative] = [ordered]@{ absent = $true }
        }
    }
    return [ordered]@{ head = $head; status = $status; inventory = $inventory }
}
$started = [DateTime]::UtcNow.ToString('o')
$before = Snapshot
Set-Location -LiteralPath $repo
& (Get-Command pwsh -CommandType Application).Source -NoProfile -File './validate.ps1' *> $log
$code = $LASTEXITCODE
$after = Snapshot
$beforeText = $before | ConvertTo-Json -Depth 8 -Compress
$afterText = $after | ConvertTo-Json -Depth 8 -Compress
if ($beforeText -cne $afterText) { throw 'Versioned or nonignored validation inputs changed during the full run.' }
$receipt = [ordered]@{ startedUtc = $started; completedUtc = [DateTime]::UtcNow.ToString('o'); runtime = $PSVersionTable.PSVersion.ToString(); command = 'pwsh -NoProfile -File ./validate.ps1'; exitCode = $code; inputsUnchanged = $true; before = $before; afterIdentityEqualsBefore = $true; log = $log; logSha256 = (Get-FileHash -LiteralPath $log).Hash; scope = 'All five validators and complete Windows tooling tests at the recorded dirty candidate snapshot. No native or final clean-head CI qualification.' }
[IO.File]::WriteAllText($receiptPath, ($receipt | ConvertTo-Json -Depth 10), [Text.UTF8Encoding]::new($false))
Get-Content -LiteralPath $log -Tail 40
exit $code
