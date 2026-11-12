<# .SYNOPSIS Executes the actual public foreground routing at recording boundaries without input or native processes. #>
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $root 'test.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Actual test runner does not parse.'}
$assignments=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -ceq '$lease' -and $n.Right.Extent.Text.Contains('Invoke-CiInteractiveDesktopOperation')},$true))
if($assignments.Count -ne 1){throw 'Expected one shared-admission foreground assignment.'}
$body=[scriptblock]::Create($assignments[0].Extent.Text)
foreach($hosted in @($false,$true)){
    Invoke-TestCase "actual foreground routing preserves shared admission and the native hosted=$hosted result" {
        $probe=@{outer=0;native=0;hosted=$null;required=$false;released=$false;parameters=$null}
        function Invoke-CiInteractiveDesktopOperation {param($RequiresInteractiveDesktop,$Operation)
            $probe.outer++;$probe.required=$RequiresInteractiveDesktop
            try{& $Operation}finally{$probe.released=$true}
        }
        function Invoke-DxUiHostedForegroundLease {
            param($Executable,$Runs,$Label,$EstimateSeconds,$WorkDirectory,$ChildTimeoutSeconds)
            $probe.hosted=$true;$probe.native++;$probe.parameters=$PSBoundParameters
            return [pscustomobject]@{Result='hosted result';ExitCode=0}
        }
        function Invoke-DxUiInteractiveLease {
            param($Executable,$Runs,$Label,$EstimateSeconds,$WorkDirectory,$ChildTimeoutSeconds,[switch]$AutoStart)
            $probe.hosted=$false;$probe.native++;$probe.parameters=$PSBoundParameters
            return [pscustomobject]@{Result='local result';ExitCode=0}
        }
        $hostedForegroundLease=$hosted;$Platform='x64';$leaseExecutable='C:/synthetic-not-launched/lease.exe'
        $plan=@([pscustomobject]@{Name='literal 日';CommandLine='unchanged "argument"'})
        $label='fixture';$estimate=77;$logs='C:/synthetic-not-created/evidence';$bound=123;$InteractiveAutoStart=$true
        . $body
        Assert-True ($probe.outer -eq 1 -and $probe.native -eq 1 -and $probe.required -and $probe.released) 'one admission surrounds one existing native operation'
        Assert-Equal $hosted $probe.hosted 'the original hosted/local native route remains selected'
        Assert-Equal $leaseExecutable $probe.parameters.Executable 'native executable unchanged'
        Assert-Equal $plan[0].CommandLine $probe.parameters.Runs[0].CommandLine 'literal native plan unchanged'
        Assert-Equal $bound $probe.parameters.ChildTimeoutSeconds 'original watchdog unchanged'
        Assert-Equal $estimate $probe.parameters.EstimateSeconds 'original estimate unchanged'
        Assert-Equal $(if($hosted){'hosted result'}else{'local result'}) $lease.Result 'original native result unchanged'
        if(-not $hosted){Assert-True $probe.parameters.AutoStart 'explicit consent flag retained'}
    }
}
Complete-TestRun 'Shared desktop admission'
$global:LASTEXITCODE=0
