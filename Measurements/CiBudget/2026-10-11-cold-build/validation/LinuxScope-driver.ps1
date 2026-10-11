Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$source='/mnt/c/Users/eric/.codex/worktrees/performance-forms/DxUi'
$fixture='/home/a/codex-dpf-s0-s2-portability-20261011'
$external='/mnt/c/Users/eric/.codex/tmp/dpf-s0-s2-merge'
[Environment]::SetEnvironmentVariable('PATH','/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin')
$inputs=[ordered]@{}
foreach($path in @('.github/workflows/ci.yml','Tools/tests/Test-NativeScope.ps1','Tests/test-scopes.json',
    'Tools/BenchmarkGate.psm1','Tools/NativeScope.psm1','Tools/tests/TestSupport.psm1')) {
    Copy-Item -LiteralPath (Join-Path $source $path) -Destination (Join-Path $fixture $path)
    $inputs[$path]=(Get-FileHash -LiteralPath (Join-Path $source $path)).Hash
    if((Get-FileHash -LiteralPath (Join-Path $fixture $path)).Hash -cne $inputs[$path]){throw 'Linux focused overlay mismatch.'}
}
Set-Location -LiteralPath $fixture
$fixtureHead=(& git rev-parse HEAD).Trim()
$log=Join-Path $external 'native-cold-budget-linux-focused.log'
& (Get-Process -Id $PID).Path -NoProfile -File './Tools/tests/Test-NativeScope.ps1' *> $log
$code=$LASTEXITCODE
foreach($path in $inputs.Keys){if((Get-FileHash -LiteralPath (Join-Path $fixture $path)).Hash -cne $inputs[$path]){throw 'Focused source changed.'}}
$result=[ordered]@{host='Native Ubuntu WSL';runtime=$PSVersionTable.PSVersion.ToString();fixtureBase=$fixtureHead;inputs=$inputs;scope='Focused NativeScope only; listed exact budget overlay and helper bytes, not whole-candidate or native runtime qualification.';exitCode=$code;log=$log;logSha256=(Get-FileHash -LiteralPath $log).Hash;completedUtc=[DateTime]::UtcNow.ToString('o')}
[IO.File]::WriteAllText((Join-Path $external 'native-cold-budget-linux-focused.receipt.txt'),($result|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
Get-Content -LiteralPath $log -Tail 8
exit $code
