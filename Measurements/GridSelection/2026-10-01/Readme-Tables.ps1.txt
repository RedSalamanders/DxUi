<#
.SYNOPSIS Prints the compact Markdown tables of the grid-selection README from a packet summary (scratch tooling).
#>
[CmdletBinding()] param([Parameter(Mandatory)][string] $Summary, [ValidateSet('paint', 'cost', 'membership', 'mutators', 'repeatpaint')][string] $Table = 'paint')
Set-StrictMode -Version Latest
$s = Get-Content -Raw -LiteralPath $Summary | ConvertFrom-Json
$inv = [Globalization.CultureInfo]::InvariantCulture
function Find($kind, $name, $metric) { return $s.metrics | Where-Object { $_.kind -eq $kind -and $_.name -eq $name -and $_.metric -eq $metric } | Select-Object -First 1 }
function Pct($m) { if ($null -eq $m.changePercent) { return 'n/a' } return ([double]$m.changePercent).ToString('+0.0;-0.0', $inv) + '%' }
function P($m) { $p = [double]$m.pValue; if ($p -lt 0.001) { return '<0.001' } return $p.ToString('0.000', $inv) }
function N($v, $f) { return ([double]$v).ToString($f, $inv) }
switch ($Table) {
    'paint' {
        '| Scenario | Rows | Selected | Prepare p50 A (ms) | Prepare p50 B (ms) | Change | p | Verdict | Mcycles A | Mcycles B | Frame p50 A (ms) | Frame p50 B (ms) | Frame verdict |'
        '|---|---:|---:|---:|---:|---:|---:|---|---:|---:|---:|---:|---|'
        $first = Get-Content -Raw -LiteralPath (Join-Path (Split-Path $Summary) 'A1.json') | ConvertFrom-Json
        foreach ($scenario in $first.paint) {
            $ms = Find 'paint' $scenario.name 'prepareP50Ms'; $cy = Find 'paint' $scenario.name 'prepareMcycP50'; $fr = Find 'paint' $scenario.name 'frameP50Ms'
            '| {0} | {1:N0} | {2:N0} | {3} | {4} | {5} | {6} | {7} | {8} | {9} | {10} | {11} | {12} |' -f $scenario.name, $scenario.rows, $scenario.selected, (N $ms.baselineMedian '0.000'), (N $ms.candidateMedian '0.000'), (Pct $ms), (P $ms), $ms.verdict, (N $cy.baselineMedian '0.000'), (N $cy.candidateMedian '0.000'), (N $fr.baselineMedian '0.000'), (N $fr.candidateMedian '0.000'), $fr.verdict
        }
    }
    'cost' {
        '| Rows | Selected in the full state | A cost (ms) | B cost (ms) | A cost (Mcycles) | B cost (Mcycles) | A share of its Prepare | p | Verdict |'
        '|---:|---:|---:|---:|---:|---:|---:|---:|---|'
        foreach ($rows in 20000, 200000, 1000000) {
            $c = Find 'selectionCost' ([string]$rows) 'costMcyc'; $ms = Find 'selectionCost' ([string]$rows) 'costMs'; $every = Find 'selectionCost' ([string]$rows) 'everyMcycP50'; $small = Find 'selectionCost' ([string]$rows) 'smallMcycP50'
            '| {0:N0} | {0:N0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} |' -f $rows, (N $ms.baselineMedian '0.0000'), (N $ms.candidateMedian '0.0000'), (N $c.baselineMedian '0.000'), (N $c.candidateMedian '0.000'), (([double]$c.baselineMedian / [double]$every.baselineMedian * 100).ToString('0.0', $inv) + '%'), (P $c), $c.verdict
        }
    }
    'membership' {
        '| Selected ids | A hit (ns) | B hit (ns) | A miss (ns) | B miss (ns) | Miss: change | p | Verdict |'
        '|---:|---:|---:|---:|---:|---:|---:|---|'
        foreach ($size in 0, 1, 2, 3, 4, 8, 16, 32, 64, 256, 1000, 20000, 200000, 1000000) {
            $hit = Find 'membership' ([string]$size) 'hitNs'; $miss = Find 'membership' ([string]$size) 'missNs'
            '| {0:N0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} |' -f $size, $(if ($hit) { N $hit.baselineMedian '0.0' } else { '-' }), $(if ($hit) { N $hit.candidateMedian '0.0' } else { '-' }), (N $miss.baselineMedian '0.0'), (N $miss.candidateMedian '0.0'), (Pct $miss), (P $miss), $miss.verdict
        }
    }
    'mutators' {
        '| Operation | Rows | Ids | A (ms) | B (ms) | Change | p | A heap (calls, bytes) | B heap (calls, bytes) |'
        '|---|---:|---|---:|---:|---:|---:|---:|---:|'
        foreach ($rows in 20000, 200000) {
            foreach ($operation in 'SetRange all, cold', 'SetRange all, warm', 'PreserveOrdered all kept', 'PreserveOrdered half kept', 'Toggle one row on and off') {
                foreach ($scattered in 'False', 'True') {
                    $name = '{0}|{1}|{2}' -f $operation, $rows, $scattered
                    $ms = Find 'mutator' $name 'ms'; $bytes = Find 'mutator' $name 'cppBytes'; $calls = Find 'mutator' $name 'cppAllocations'
                    '| {0} | {1:N0} | {2} | {3} | {4} | {5} | {6} | {7} | {8} |' -f $operation, $rows, $(if ($scattered -eq 'True') { 'scattered' } else { 'ascending' }), (N $ms.baselineMedian $(if ($operation -like 'Toggle*') { '0.00000' } else { '0.0000' })), (N $ms.candidateMedian $(if ($operation -like 'Toggle*') { '0.00000' } else { '0.0000' })), (Pct $ms), (P $ms), $(if ($bytes) { '{0:N0}, {1:N0}' -f $calls.baselineMedian, $bytes.baselineMedian } else { '-' }), $(if ($bytes) { '{0:N0}, {1:N0}' -f $calls.candidateMedian, $bytes.candidateMedian } else { '-' })
                }
            }
        }
    }
}
