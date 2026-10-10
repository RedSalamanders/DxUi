<#
.SYNOPSIS Summarizes the live-heap cost of an open described menu from a MenuResourceScaling or MenuRowLiveBytes log.
.DESCRIPTION Reads the JSON sample lines of a suite log. For every variant it takes open-minus-before live heap bytes
(busy bytes of every process heap, from the suite's own heap diagnostic) per cycle and reports the median, minimum and
maximum of the last -Last cycles, like the retained popup-text-sharing packet. MenuRowLiveBytes lines carry the summed
live bytes directly and the exact DebugGetContextMenuResources counts.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $LogPath,
    [int] $Last = 16
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-Median([double[]] $values) {
    $sorted = @($values | Sort-Object)
    if (-not $sorted.Count) { return [double]::NaN }
    if ($sorted.Count % 2) { return $sorted[[int](($sorted.Count - 1) / 2)] }
    return ($sorted[$sorted.Count / 2 - 1] + $sorted[$sorted.Count / 2]) / 2
}

$samples = @{}
foreach ($line in [IO.File]::ReadLines($LogPath)) {
    if ($line -notmatch '^\{"fixture":"(dxui-menu-resource-scaling-v[45]|dxui-menu-row-live-bytes-v1)"') { continue }
    if ($line -notmatch '"phase"') { continue }
    $s = $line | ConvertFrom-Json
    if ($s.fixture -like 'dxui-menu-resource-scaling-v*') {
        $captions = if ($s.PSObject.Properties.Name -contains 'captions') { $s.captions } else { 'repeated' }
        $key = 'entries={0,-3} descriptions={1,-3} captions={2}' -f $s.entries, $s.descriptions, $captions
        $live = ($s.heaps | Measure-Object -Property busyBytes -Sum).Sum
        $layouts = $null
    } else {
        $key = 'entries={0,-3} descriptions={1,-3} captions={2}' -f $s.rows, $(if ($s.described) { $s.rows } else { 0 }), $s.captions
        $live = [double]$s.liveBytes
        $layouts = [double]$s.rowLayouts
    }
    $cycleKey = "$key|$($s.cycle)"
    if (-not $samples.ContainsKey($cycleKey)) { $samples[$cycleKey] = @{ key = $key; cycle = [int]$s.cycle } }
    $samples[$cycleKey][$s.phase] = $live
    if ($null -ne $layouts -and $s.phase -eq 'rendered') { $samples[$cycleKey]['layouts'] = $layouts }
    if ($null -ne $layouts -and $s.phase -eq 'before') { $samples[$cycleKey]['layoutsBefore'] = $layouts }
}
$rows = foreach ($group in ($samples.Values | Group-Object { $_.key } | Sort-Object Name)) {
    $cycles = @($group.Group | Where-Object { $_.ContainsKey('before') -and $_.ContainsKey('rendered') } | Sort-Object { $_.cycle })
    if ($cycles.Count -lt 1) { continue }
    $tail = @($cycles | Select-Object -Last $Last)
    $open = @($tail | ForEach-Object { [double]($_['rendered'] - $_['before']) })
    $closedResidual = @($tail | Where-Object { $_.ContainsKey('closed') } | ForEach-Object { [double]($_['closed'] - $_['before']) })
    $rowLayoutDelta = @($tail | Where-Object { $_.ContainsKey('layouts') } | ForEach-Object { [double]($_['layouts'] - $_['layoutsBefore']) })
    [pscustomobject]@{
        Variant        = $group.Name
        Cycles         = $cycles.Count
        OpenMedian     = Get-Median $open
        OpenMin        = ($open | Measure-Object -Minimum).Minimum
        OpenMax        = ($open | Measure-Object -Maximum).Maximum
        ClosedResidual = if ($closedResidual.Count) { Get-Median $closedResidual } else { [double]::NaN }
        RowLayouts     = if ($rowLayoutDelta.Count) { Get-Median $rowLayoutDelta } else { [double]::NaN }
    }
}
$rows | Format-Table -AutoSize | Out-String -Width 200
