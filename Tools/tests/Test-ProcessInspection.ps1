<# .SYNOPSIS Exercises exact output-process ownership, stale snapshots and inaccessible live-process refusal. #>
[CmdletBinding()]param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $PSScriptRoot TestSupport.psm1) -Force
Import-Module (Join-Path $root Tools/ProcessInspection.psm1) -Force
$module=Get-Module ProcessInspection
$original=& $module { (Get-Command Open-DxUiObservedProcess).ScriptBlock }
function With-Opener([scriptblock]$Open,[scriptblock]$CaseAction) {
    try { & $module {param($replacement) Set-Item -LiteralPath Function:script:Open-DxUiObservedProcess -Value $replacement} $Open; & $CaseAction }
    finally { & $module {param($replacement) Set-Item -LiteralPath Function:script:Open-DxUiObservedProcess -Value $replacement} $original }
}
function Observation { [pscustomobject]@{ExecutablePath=$null;ProcessId=12345} }
Invoke-TestCase 'known CIM image needs no direct process lookup' {
    With-Opener {throw 'Unexpected lookup'} {
        Assert-Equal 'C:\synthetic\profile\DxUi.ControlTests.exe' (Resolve-DxUiObservedProcessPath ([pscustomobject]@{ExecutablePath='C:\synthetic\profile\DxUi.ControlTests.exe';ProcessId=12345})) 'original image path'
    }
}
Invoke-TestCase 'PID absent after snapshot is safely ignored' {
    With-Opener {throw [ArgumentException]::new('synthetic missing PID')} {
        Assert-True ($null -eq (Resolve-DxUiObservedProcessPath (Observation))) 'confirmed gone observation'
    }
}
foreach($mode in @('Exited','Live','Race','Denied','LateDenied')){
    Invoke-TestCase ('direct inspection preserves process instance and fails closed ('+$mode+')') {
        $state=[pscustomobject]@{mode=$mode;reads=0;disposed=0}
        $process=[pscustomobject]@{}
        $process|Add-Member ScriptProperty HasExited { $state.reads++; if($state.mode -ceq 'LateDenied' -and $state.reads -ge 2){throw [ComponentModel.Win32Exception]::new(5)}; return ($state.mode -ceq 'Exited' -or ($state.mode -ceq 'Race' -and $state.reads -ge 2)) }.GetNewClosure()
        $process|Add-Member ScriptProperty MainModule { if($state.mode -in @('Race','Denied')){throw [ComponentModel.Win32Exception]::new(5)}; return [pscustomobject]@{FileName='C:\synthetic\other\DxUi.ControlTests.exe'} }.GetNewClosure()
        $process|Add-Member ScriptMethod Dispose { $state.disposed++ }.GetNewClosure()
        $open={param($ProcessId) Assert-Equal 12345 $ProcessId 'original observed PID';return $process}.GetNewClosure()
        With-Opener $open {
            $denied=$false;$image=$null
            try {$image=Resolve-DxUiObservedProcessPath (Observation) -DisplayName 'synthetic child'}
            catch [InvalidOperationException] {$denied=$_.Exception.Message.Contains('PID 12345')}
            if($mode -in @('Denied','LateDenied')){Assert-True $denied 'live uninspectable or uncertain process refused'}
            elseif($mode -ceq 'Live'){Assert-Equal 'C:\synthetic\other\DxUi.ControlTests.exe' $image 'current direct image path'}
            else{Assert-True ($null -eq $image -and -not $denied) 'confirmed exit is ignored'}
        }
        Assert-Equal 1 $state.disposed 'only the borrowed observation handle is disposed'
    }
}
Invoke-FixtureCase 'actual owned process survives missing CIM path and can then exit' {
    param($fixture)
    $process=$null
    try {
        $start=[Diagnostics.ProcessStartInfo]::new((Get-Process -Id $PID).Path)
        $start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardInput=$true
        foreach($argument in @('-NoProfile','-Command','[Console]::WriteLine("ready"); [Console]::ReadLine() | Out-Null')){[void]$start.ArgumentList.Add($argument)}
        $process=[Diagnostics.Process]::Start($start)
        $ready=$process.StandardOutput.ReadLineAsync()
        Assert-True ($ready.Wait(10000) -and $ready.GetAwaiter().GetResult() -ceq 'ready') 'owned child readiness'
        $observed=[pscustomobject]@{ExecutablePath=$null;ProcessId=$process.Id}
        Assert-Equal $process.MainModule.FileName (Resolve-DxUiObservedProcessPath $observed) 'actual missing-path fallback'
        Assert-True (-not $process.HasExited) 'inspection did not terminate the independent child'
        $process.StandardInput.WriteLine('exit')
        Assert-True ($process.WaitForExit(10000)) 'owned child exits normally'
        Assert-True ($null -eq (Resolve-DxUiObservedProcessPath $observed)) 'actual stale observation ignored'
    } finally {
        if($process){if(-not $process.HasExited){$process.Kill($true);[void]$process.WaitForExit(5000)};$process.Dispose()}
    }
}
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $root build.ps1),[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Actual build script does not parse'}
$phase=@($ast.FindAll({param($node)$node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -ceq 'Invoke-DxUiBuildPhase' -and $node.Extent.Text.Contains('-Phase OutputUseChecks')},$true))
Assert-Equal 1 $phase.Count 'one actual output-use phase'
$body=@($phase[0].CommandElements|Where-Object {$_ -is [Management.Automation.Language.ScriptBlockExpressionAst]})[0].ScriptBlock.GetScriptBlock()
foreach($mode in @('Current','OtherConfiguration','OtherWorkspace','Exited')){
    Invoke-FixtureCase ('actual build phase scopes the output process ('+$mode+')') {
        param($fixture)
        $output=Join-Path $fixture profile
        $Platform='x64';$Configuration='Release'
        $image=switch($mode){'Current'{Join-Path $output 'x64/Release/DxUi.ControlTests.exe'}'OtherConfiguration'{Join-Path $output 'x64/Debug/DxUi.ControlTests.exe'}'OtherWorkspace'{Join-Path $fixture 'other/x64/Release/DxUi.ControlTests.exe'}default{$null}}
        function Get-CimInstance($Class,[string]$Filter){if($Filter.Contains("'DxUi.ControlTests.exe'")){[pscustomobject]@{ExecutablePath=$image;ProcessId=12345}}}
        $open={throw [ArgumentException]::new('synthetic disappeared child')}
        With-Opener $open {
            $blocked=$false
            try {& $body}catch {$blocked=$_.Exception.Message.StartsWith('Build output is running: PID 12345,')}
            Assert-Equal ($mode -ceq 'Current') $blocked 'only the exact live output blocks rebuilding'
        }
    }
}
Complete-TestRun 'Process inspection'
