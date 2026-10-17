[CmdletBinding()] param([string] $ReferenceRoot,[string] $GeneratedRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Generate-Data.ps1') -InputDirectory $ReferenceRoot -OutputDirectory $GeneratedRoot
function Properties([int] $Cp){
    if($Cp -ge 0xac00 -and $Cp -le 0xd7a3){if(($Cp-0xac00)%28 -eq 0){return 12}else{return 13}}
    return (Find-Value $gcb $Cp) -bor (Find-Value $pictographic $Cp) -bor (Find-Value $indic $Cp)
}
function Next-Cluster([int[]] $CodePoints,[int] $Start){
    $previous=0;$seen=$false;$epPrefix=$false;$previousZwjFromEp=$false;$oddRegional=$false;$indicState=0
    for($index=$Start;$index -lt $CodePoints.Length;$index++){
        $property=Properties $CodePoints[$index];$current=$property -band 15;$ep=($property -band 16)-ne 0;$incb=($property -shr 5)-band 3
        if($seen){
            $break=$true
            if($previous -eq 1 -and $current -eq 2){$break=$false}
            elseif(($previous -ge 1 -and $previous -le 3) -or ($current -ge 1 -and $current -le 3)){$break=$true}
            elseif($previous -eq 9 -and $current -in @(9,10,12,13)){$break=$false}
            elseif($previous -in @(10,12) -and $current -in @(10,11)){$break=$false}
            elseif($previous -in @(11,13) -and $current -eq 11){$break=$false}
            elseif($current -in @(4,5,8) -or $previous -eq 7){$break=$false}
            elseif($indicState -eq 2 -and $incb -eq 1){$break=$false}
            elseif($previous -eq 5 -and $previousZwjFromEp -and $ep){$break=$false}
            elseif($previous -eq 6 -and $current -eq 6 -and $oddRegional){$break=$false}
            if($break){return $index}
        }
        $previousZwjFromEp=$current -eq 5 -and $epPrefix
        $epPrefix=$ep -or ($current -eq 4 -and $epPrefix)
        $oddRegional=if($current -eq 6){-not $oddRegional}else{$false}
        if($incb -eq 1){$indicState=1}elseif($incb -eq 3 -and $indicState){$indicState=2}elseif($incb -ne 2 -and $incb -ne 3){$indicState=0}
        $previous=$current;$seen=$true
    }
    return $CodePoints.Length
}
$count=0
foreach($line in Get-Content -LiteralPath (Join-Path $ReferenceRoot 'GraphemeBreakTest.txt')){
    $data=$line.Split('#')[0].Trim();if(-not $data){continue}
    $cps=[Collections.Generic.List[int]]::new();$expected=[Collections.Generic.List[int]]::new()
    foreach($token in $data.Split(' ',[StringSplitOptions]::RemoveEmptyEntries)){
        if($token -ceq '÷'){$expected.Add($cps.Count)}elseif($token -cne '×'){$cps.Add((Parse-Hex $token))}
    }
    $actual=[Collections.Generic.List[int]]::new();$actual.Add(0);$position=0
    while($position -lt $cps.Count){$next=Next-Cluster $cps.ToArray() $position;if($next -le $position){throw 'No segmentation progress'};$actual.Add($next);$position=$next}
    if(($actual.ToArray()-join ',') -cne ($expected.ToArray()-join ',')){throw "Unicode row $count differs: $data; got $($actual -join ',')"}
    $count++
}
Write-Host "Prototype rule state machine matches all $count original Unicode 17 vectors. This is not native integration evidence."
