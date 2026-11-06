# Exercise the actual filter launcher with an owned process that emits distinct output and fails.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Invoke-FixtureCase 'filter failure preserves child output and executable identity before assertions' {
    param($root)
    $source = Join-Path $PSScriptRoot 'Test-TestFilter.ps1'
    $tokens = $null; $errors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile($source, [ref]$tokens, [ref]$errors)
    Assert-Equal 0 $errors.Count 'the real filter script parses'
    $functions = @($ast.FindAll({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Invoke-Runner'
    }, $false))
    Assert-Equal 1 $functions.Count 'the real launcher is exercised'
    . ([scriptblock]::Create($functions[0].Extent.Text))
    $repo = $root
    $exe = Join-Path $PSHOME $(if ($IsWindows) { 'pwsh.exe' } else { 'pwsh' })
    $runnerSha256 = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    Set-FixtureFile $root 'child.ps1' @'
[Console]::Out.WriteLine('original native stdout')
[Console]::Error.WriteLine('original native stderr')
exit 37
'@
    $captured = @(& { Invoke-Runner @('-NoProfile', '-File', (Join-Path $root 'child.ps1')) } 6>&1)
    $results = @($captured | Where-Object { $_ -is [pscustomobject] -and $_.PSObject.Properties['Exit'] })
    Assert-Equal 1 $results.Count 'one actual process result is preserved'
    Assert-Equal 37 $results[0].Exit 'the original nonzero exit is preserved'
    $messages = @($captured | Where-Object { $_ -is [Management.Automation.InformationRecord] } | ForEach-Object { "$($_.MessageData)" })
    Assert-Contains $messages 'original native stdout' 'the original stdout remains visible before a caller assertion'
    Assert-Contains $messages 'original native stderr' 'the original stderr remains visible before a caller assertion'
    Assert-True (@($messages | Where-Object { $_.Contains('exit=37') -and $_.Contains("executableSha256=$runnerSha256") }).Count -eq 1) 'the failure output names its exit and actual executable hash'
}
Complete-TestRun 'Native test-filter output'
