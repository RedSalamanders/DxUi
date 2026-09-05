[CmdletBinding()]
param([Parameter(Mandatory)][string] $DxUiRoot, [Parameter(Mandatory)][string] $LockFile)
$ErrorActionPreference = 'Stop'
$pin = Get-Content -LiteralPath $LockFile -Raw | ConvertFrom-Json
if ($pin.commit -notmatch '^[0-9a-f]{40}$' -or $pin.repository -ne 'https://github.com/RedSalamanders/DxUI') {
    throw 'DxUi lock must name the canonical repository and an exact 40-character commit.'
}
$head = & git -C $DxUiRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $head.Trim() -ne $pin.commit) { throw 'DxUi checkout does not match the consumer lock.' }
$dirty = & git -C $DxUiRoot status --porcelain --untracked-files=no
if ($LASTEXITCODE -ne 0 -or $dirty) { throw 'Pinned DxUi checkout is not clean.' }
$api = Get-Content -LiteralPath (Join-Path $DxUiRoot 'capabilities.json') -Raw | ConvertFrom-Json
if ($pin.apiRevision -ne $api.apiRevision) { throw 'DxUi public API revision mismatch.' }
if (@($pin.targets).Count -ne 1 -or $pin.targets[0] -ne 'Foundation') { throw 'Only Foundation is available in the bootstrap.' }
Write-Host "DxUi pin verified: $head"
