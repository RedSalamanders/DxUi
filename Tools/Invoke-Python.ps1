[CmdletBinding()]
param([Parameter(Mandatory)][string] $Script, [string[]] $Arguments = @())
$ErrorActionPreference = 'Stop'
if ($env:DXUI_PYTHON) {
    & $env:DXUI_PYTHON $Script @Arguments
} elseif (Get-Command py.exe -ErrorAction SilentlyContinue) {
    & py.exe -3 $Script @Arguments
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
    & python $Script @Arguments
} else {
    throw 'Python 3.11+ is required. Install Python or set DXUI_PYTHON to its executable.'
}
if ($LASTEXITCODE -ne 0) { throw "Python validation failed with exit code $LASTEXITCODE." }
