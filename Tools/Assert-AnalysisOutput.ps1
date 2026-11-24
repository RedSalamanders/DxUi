#Requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][string]$IntermediateDirectory,
    [ValidateSet('x64','ARM64')][string]$Platform,
    [ValidateSet('Debug','Release','ASan Debug')][string]$Configuration,
    [ValidatePattern('^[a-zA-Z][a-zA-Z0-9]+(?:\.[a-zA-Z0-9]+)*$')][string]$ProjectName,
    [string]$TrackingDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $root '.ci-engine/Tools/Modules/Ci.Policy.psm1') -ErrorAction Stop
if ($Platform) {
    if (-not ($Configuration -and $ProjectName)) { throw 'A standard native Clean requires all profile and project coordinates.' }
    $outputRoot = Assert-CiContainedPath -Root $root -Path (Join-Path $root '.build')
    $expectedOutput = [IO.Path]::GetFullPath((Join-Path $outputRoot "$Platform/$Configuration"))
    $expectedIntermediate = [IO.Path]::GetFullPath((Join-Path $outputRoot "Intermediate/$Platform/$Configuration/$ProjectName"))
    $evaluatedOutput = (Assert-CiContainedPath -Root $outputRoot -Path $OutputDirectory).TrimEnd('\','/')
    $evaluatedIntermediate = (Assert-CiContainedPath -Root $outputRoot -Path $IntermediateDirectory).TrimEnd('\','/')
    if ($evaluatedOutput -ine $expectedOutput -or $evaluatedIntermediate -ine $expectedIntermediate) {
        throw "Unsafe native Clean output for $ProjectName in $Platform $Configuration."
    }
    if ($TrackingDirectory) {
        $null = Assert-CiContainedPath -Root $evaluatedIntermediate -Path $TrackingDirectory
    }
    return
}
$analysisRoot=Assert-CiContainedPath -Root $root -Path (Join-Path $root '.build/analysis')
$null=Assert-CiContainedPath -Root $analysisRoot -Path $OutputDirectory
$null=Assert-CiContainedPath -Root $analysisRoot -Path $IntermediateDirectory
