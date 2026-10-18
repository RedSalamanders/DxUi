[CmdletBinding()]param([string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputDirectory){throw 'Fresh synthetic test output required.'}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$cases=@(for($index=0;$index -lt 4;++$index){
 $mode=if($index -in @(0,3)){'Default'}else{'CallingThread'};$name='synthetic-'+$index+'.json';$path=Join-Path $OutputDirectory $name
 $windows=@(for($window=0;$window -lt 8;++$window){
  $start=10000*$window;$kind=if($window -lt 4){$window}else{7-$window}
  @{clear=$kind -in @(0,1);composite=$kind -in @(0,2);startedQpc=$start;endedQpc=$start+4000;
   cpuBefore=@{main=@{kernel100ns=0;user100ns=0;cycles=0;timesAvailable=$true;cyclesAvailable=$true};process=@{kernel100ns=0;user100ns=0;cycles=0;timesAvailable=$true;cyclesAvailable=$true}};
   cpuAfter=@{main=@{kernel100ns=100;user100ns=200;cycles=1000;timesAvailable=$true;cyclesAvailable=$true};process=@{kernel100ns=200;user100ns=400;cycles=2000;timesAvailable=$true;cyclesAvailable=$true}};
   frames=@(for($frame=0;$frame -lt 40;++$frame){@{qpc=@(for($edge=0;$edge -lt 9;++$edge){$start+100*$frame+$edge})}})}
 })
 @{clock=@{ticksPerSecond=1000};requestedDeviceFlags=$(if($mode -ceq 'Default'){32}else{40});deviceCreationFlags=$(if($mode -ceq 'Default'){32}else{40});windows=$windows}|
  ConvertTo-Json -Depth 24|Set-Content ($path+'.raw.json')
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'DebugSmoke/Default.json.png') -Destination ($path+'.png')
 @{index=$index+1;mode=$mode;report=$name;validationPassed=$true;rawSha256=(Get-FileHash ($path+'.raw.json')).Hash;captureSha256=(Get-FileHash ($path+'.png')).Hash}
})
@{scheduled=4;attempted=4;elapsedSeconds=1.0;cases=$cases}|ConvertTo-Json -Depth 12|Set-Content (Join-Path $OutputDirectory 'completion.receipt.txt')
& (Join-Path $PSScriptRoot 'Analyze-WarpPolicies.ps1') -OutputDirectory $OutputDirectory
$analysis=Get-Content (Join-Path $OutputDirectory 'analysis.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable -Depth 32
foreach($case in $analysis.cases){foreach($window in $case.windows){
 if($window.frameP95Ms -ne 8.0 -or $window.frameMedianMs -ne 8.0 -or $window.p95Frame -ne 37 -or $window.mapP95Ms -ne 1.0 -or
  $window.mapShareAtFrameP95Percent -ne 12.5 -or $window.cpu.main.cpuMs -ne .03 -or $window.cpu.process.cpuMs -ne .06 -or $window.cpu.main.cycles -ne 1000){throw 'Independent synthetic frame/counter arithmetic differs.'}
}}
$varied=Join-Path $OutputDirectory 'Varied'
New-Item -ItemType Directory -Path $varied|Out-Null
$variedCases=@(foreach($case in $cases){
 $path=Join-Path $varied $case.report
 $native=Get-Content (Join-Path $OutputDirectory ($case.report+'.raw.json')) -Raw|ConvertFrom-Json -AsHashtable -Depth 32
 foreach($window in $native.windows){for($frame=0;$frame -lt 40;++$frame){
  $first=[long]$window.startedQpc+100*$frame;$duration=40-$frame
  # The p95 frame has a short Map. A separately sorted Map percentile must not
  # be presented as the Map interval of that same frame.
  $map=if($frame -eq 2){1}else{$duration-1}
  $window.frames[$frame].qpc=@($first,$first,$first,$first,$first,$first,($first+$map),($first+$map),($first+$duration))
 }}
 $native|ConvertTo-Json -Depth 24|Set-Content ($path+'.raw.json')
 Copy-Item -LiteralPath (Join-Path $OutputDirectory ($case.report+'.png')) -Destination ($path+'.png')
 @{index=$case.index;mode=$case.mode;report=$case.report;validationPassed=$true;
  rawSha256=(Get-FileHash ($path+'.raw.json')).Hash;captureSha256=(Get-FileHash ($path+'.png')).Hash}
})
@{scheduled=4;attempted=4;elapsedSeconds=1.0;cases=$variedCases}|ConvertTo-Json -Depth 12|Set-Content (Join-Path $varied 'completion.receipt.txt')
& (Join-Path $PSScriptRoot 'Analyze-WarpPolicies.ps1') -OutputDirectory $varied
$variedAnalysis=Get-Content (Join-Path $varied 'analysis.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable -Depth 32
foreach($case in $variedAnalysis.cases){foreach($window in $case.windows){
 if($window.frameMedianMs -ne 20.0 -or $window.frameP95Ms -ne 38.0 -or $window.maximumMs -ne 40.0 -or
  $window.p95Frame -ne 2 -or $window.mapP95Ms -ne 36.0 -or [math]::Abs($window.mapShareAtFrameP95Percent-100.0/38.0) -gt 1e-12){
  throw 'Independent unsorted nearest-rank or same-frame Map attribution differs.'
 }
}}
$target=Join-Path $OutputDirectory 'synthetic-0.json.raw.json';[IO.File]::AppendAllText($target,' ')
$rejected=$false
try{& (Join-Path $PSScriptRoot 'Analyze-WarpPolicies.ps1') -OutputDirectory $OutputDirectory}catch [System.Exception]{if($_.Exception.Message -cne 'Original raw report/capture changed.'){throw};$rejected=$true}
if(-not$rejected){throw 'Synthetic raw tamper accepted.'}
@{classification='Synthetic arithmetic/negative integrity fixtures, not native observations';windows=64;frames=2560;expectedFrameMs=8;expectedMapMs=1;expectedMainCpuMs=.03;
 variedMedianMs=20;variedP95Ms=38;variedP95Frame=2;variedMapP95Ms=36;sameFrameMapMs=1;
 tamperRejected=$rejected;analyzerSha256=(Get-FileHash (Join-Path $PSScriptRoot 'Analyze-WarpPolicies.ps1')).Hash;testSha256=(Get-FileHash $PSCommandPath).Hash}|
 ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'tests.receipt.txt')
Write-Host 'Synthetic frame/window/counter arithmetic and exact raw-byte tamper rejection passed.'
