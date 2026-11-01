# Run all named tooling scripts even after a failure. A script's own exit status
# belongs to that invocation, never to a prior native command or earlier script.
function Invoke-DxUiToolingTestRoster {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string[]] $Scripts)
    Set-StrictMode -Version Latest
    $ErrorActionPreference = 'Stop'
    $failed = [Collections.Generic.List[string]]::new()
    foreach ($path in $Scripts) {
        $name = Split-Path -Leaf $path
        Write-Host "== $name"
        $global:LASTEXITCODE = 0
        try {
            & $path
            if ($global:LASTEXITCODE -ne 0) {
                $failed.Add($name)
                Write-Host "$name returned exit code $global:LASTEXITCODE"
            }
        } catch {
            $failed.Add($name)
            Write-Host $_.Exception.Message
        }
    }
    if ($failed.Count) { throw "Tooling tests failed: $($failed -join ', ')" }
    $global:LASTEXITCODE = 0
}
Export-ModuleMember -Function Invoke-DxUiToolingTestRoster
