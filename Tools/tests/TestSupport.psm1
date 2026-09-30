# Assertions and fixture trees for the tooling tests; plain PowerShell, no test framework to install.
Set-StrictMode -Version Latest

$script:Results = [Collections.Generic.List[object]]::new()
$script:FixtureParent = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../.build/tooling-fixtures'))

function Invoke-TestCase([Parameter(Mandatory)][string] $Name, [Parameter(Mandatory)][scriptblock] $Body) {
    try {
        & $Body
        $script:Results.Add([pscustomobject]@{ Name = $Name; Error = $null })
    } catch {
        $script:Results.Add([pscustomobject]@{ Name = $Name; Error = "$($_.Exception.Message) (line $($_.InvocationInfo.ScriptLineNumber))" })
    }
}

function Invoke-FixtureCase([Parameter(Mandatory)][string] $Name, [Parameter(Mandatory)][scriptblock] $Body) {
    # Runs the body with a fresh fixture root under .build/tooling-fixtures, removed afterwards.
    Invoke-TestCase $Name {
        $root = New-FixtureRoot
        try { & $Body $root } finally { Remove-FixtureRoot $root }
    }.GetNewClosure()
}

function Complete-TestRun([Parameter(Mandatory)][string] $Suite) {
    $failed = @($script:Results | Where-Object { $_.Error })
    foreach ($result in $script:Results) {
        Write-Host ('{0} {1}' -f $(if ($result.Error) { 'FAIL' } else { 'ok  ' }), $result.Name)
        if ($result.Error) { Write-Host "     $($result.Error)" }
    }
    Write-Host "${Suite}: $($script:Results.Count - $failed.Count) of $($script:Results.Count) passed"
    if ($failed.Count) { throw "${Suite}: $($failed.Count) test(s) failed" }
    # Cases run commands that exit 1 on purpose; a passing suite must not leave that code for the caller or CI.
    $global:LASTEXITCODE = 0
}

function Assert-True([object] $Condition, [string] $Message) {
    if (-not $Condition) { throw "Assertion failed: $Message" }
}

function Assert-Equal([object] $Expected, [object] $Actual, [string] $Message) {
    if (-not [object]::Equals($Expected, $Actual)) { throw "${Message}: expected <$Expected>, got <$Actual>" }
}

function Assert-Contains([object[]] $Collection, [string] $Item, [string] $Message) {
    if (@($Collection) -cnotcontains $Item) { throw "${Message}: <$Item> not in <$(@($Collection) -join ' | ')>" }
}

function Assert-Throws([scriptblock] $Body, [string] $Message) {
    $threw = $false
    try { & $Body } catch { $threw = $true }
    if (-not $threw) { throw "Expected an error: $Message" }
}

function New-FixtureRoot {
    $root = Join-Path $script:FixtureParent ([guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $root -Force | Out-Null
    return $root
}

function Remove-FixtureRoot([Parameter(Mandatory)][string] $Root) {
    # Recursive removal only of a directory directly under .build/tooling-fixtures.
    $full = [IO.Path]::GetFullPath($Root)
    if (-not [string]::Equals([IO.Path]::GetDirectoryName($full), $script:FixtureParent, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove a path outside the tooling fixtures: $full"
    }
    if (Test-Path -LiteralPath $full) { Remove-Item -LiteralPath $full -Recurse -Force }
}

function Set-FixtureFile([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][string] $Name, [AllowEmptyString()][string] $Value) {
    $path = Join-Path $Root $Name
    New-Item -ItemType Directory -Path (Split-Path $path) -Force | Out-Null
    [IO.File]::WriteAllText($path, $Value, [Text.UTF8Encoding]::new($false))
}

function Set-FixtureBytes([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][string] $Name, [byte[]] $Value) {
    $path = Join-Path $Root $Name
    New-Item -ItemType Directory -Path (Split-Path $path) -Force | Out-Null
    [IO.File]::WriteAllBytes($path, $Value)
}

function Set-FixtureJson([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][string] $Name, [object] $Value) {
    Set-FixtureFile $Root $Name (ConvertTo-Json -InputObject $Value -Depth 12)
}

function Get-FixtureJson([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][string] $Name) {
    return [IO.File]::ReadAllText((Join-Path $Root $Name)) | ConvertFrom-Json -AsHashtable
}

function Copy-JsonValue([object] $Value) {
    return ConvertTo-Json -InputObject $Value -Depth 12 | ConvertFrom-Json -AsHashtable
}

function Get-Sha256Hex([byte[]] $Bytes) {
    return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant()
}

Export-ModuleMember -Function Invoke-TestCase, Invoke-FixtureCase, Complete-TestRun, Assert-True, Assert-Equal, Assert-Contains,
    Assert-Throws, New-FixtureRoot, Remove-FixtureRoot, Set-FixtureFile, Set-FixtureBytes, Set-FixtureJson, Get-FixtureJson,
    Copy-JsonValue, Get-Sha256Hex
