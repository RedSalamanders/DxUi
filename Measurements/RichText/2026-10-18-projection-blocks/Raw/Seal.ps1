[CmdletBinding()] param([string]$Packet)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$Packet=[IO.Path]::GetFullPath($Packet)
$seal=Join-Path $Packet 'SHA256SUMS'
$lines=@(Get-ChildItem -LiteralPath $Packet -Recurse -File | Where-Object FullName -NE $seal | Sort-Object FullName | ForEach-Object{
 (Get-FileHash -LiteralPath $_.FullName).Hash+'  '+[IO.Path]::GetRelativePath($Packet,$_.FullName).Replace('\','/')
})
[IO.File]::WriteAllLines($seal,$lines,[Text.UTF8Encoding]::new($false))
Write-Host "Sealed $($lines.Count) exact packet files."
