[CmdletBinding()] param([string] $Root,[string] $Packet)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(Test-Path $Packet){throw 'Keep the previous packet; use a fresh directory.'}
$Packet=[IO.Path]::GetFullPath($Packet);$work=Join-Path $Root '.build/reports/rich-indexed-20261018'
New-Item -ItemType Directory -Path $Packet | Out-Null
$copied=[Collections.Generic.List[object]]::new()
function Copy-Exact([string] $From,[string] $Relative){
    $to=[IO.Path]::GetFullPath((Join-Path $Packet $Relative))
    if(-not $to.StartsWith(($Packet+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)){throw 'Archive path escapes packet.'}
    New-Item -ItemType Directory -Path (Split-Path $to) -Force | Out-Null
    $hash=(Get-FileHash -LiteralPath $From -Algorithm SHA256).Hash
    Copy-Item -LiteralPath $From -Destination $to
    if((Get-FileHash -LiteralPath $to -Algorithm SHA256).Hash -cne $hash){throw 'Archive byte copy differs.'}
    $copied.Add(@{source=$From;path=$Relative.Replace('\','/');sha256=$hash;bytes=(Get-Item -LiteralPath $From).Length})
}
$expectedSource='4EDDEF355659895319D81501089066D93E8F6885C2701A0C19C149C17C2EC014'
$scopes=@('Foundation','Embedded','Grid','Theme','Control','MenuExitLifetime','NewControls','EditorControls','TextField','MultilineText','ReadOnly','ComboBox','Tree','Tooltip','Rendering','Animation','Accessibility','WindowHost','InteractiveLease')
$performances=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$suites=[Collections.Generic.List[object]]::new()
foreach($configuration in @('Release','Debug','ASan Debug')){
    foreach($scope in $scopes){
        $file=Join-Path $Root ".build/reports/$scope-x64-$configuration.json"
        $receipt=Get-Content -LiteralPath $file -Raw | ConvertFrom-Json -AsHashtable -Depth 32
        if($receipt.suite -cne $scope -or $receipt.configuration -cne $configuration -or $receipt.platform -cne 'x64' -or
            $receipt.nativeArchitecture -cne 'X64' -or $receipt.exitCode -ne 0 -or $receipt.skips.Count -ne 0 -or $receipt.Contains('tests')){throw 'Not a full matching native suite success.'}
        if((Get-FileHash -LiteralPath $receipt.executable).Hash -cne $receipt.sha256){throw 'Suite binary differs from the exact successful run.'}
        $performance=Get-Content -LiteralPath $receipt.performanceReport -Raw | ConvertFrom-Json -AsHashtable -Depth 32
        if($performance.sourceFingerprint -cne $expectedSource -or $performance.configuration -cne $configuration -or
            $performance.executableSha256 -cne $receipt.performanceExecutableSha256){throw 'Suite/performance identity binding differs.'}
        if((ConvertTo-Json $receipt.performanceScenarios -Depth 16 -Compress) -cne (ConvertTo-Json $performance.scenarios -Depth 16 -Compress)){throw 'Suite embedded frame data differs from its original report.'}
        [void]$performances.Add($receipt.performanceReport)
        $stem="$scope-x64-$configuration"
        Copy-Exact $file "Native/$stem.receipt.txt"
        Copy-Exact (Join-Path $Root ".build/logs/test-$stem.log") "Native/$stem.log"
        $suites.Add(@{suite=$scope;configuration=$configuration;sha256=$receipt.sha256;performance=Split-Path $receipt.performanceReport -Leaf})
    }
}
foreach($file in $performances){
    Copy-Exact $file ('Performance/'+(Split-Path $file -Leaf))
    Copy-Exact ($file+'.comparison.json') ('Performance/'+(Split-Path ($file+'.comparison.json') -Leaf))
}
foreach($platform in @('x64','ARM64')){foreach($configuration in @('Debug','Release','ASan Debug')){
    $profile=$platform+'-'+$configuration.Replace(' ','')
    Copy-Exact (Join-Path $Root ".build/reports/performance-build/$profile/build.json") "Build/$profile.receipt.txt"
}}
foreach($file in Get-ChildItem -LiteralPath $work -File){
    if($file.Name -in @('Archive.ps1','Invoke-Qualification.ps1','Invoke-Diagnostics.ps1')){Copy-Exact $file.FullName $file.Name}
    else{Copy-Exact $file.FullName ('Raw/'+$file.Name)}
}
Copy-Exact (Join-Path $Root '.build/reports/AddressSanitizer-x64.json') 'Native/AddressSanitizer-x64.receipt.txt'
Copy-Exact (Join-Path $Root '.build/logs/test-AddressSanitizer-x64.log') 'Native/AddressSanitizer-x64.log'
$before=Get-Content -LiteralPath (Join-Path $work 'before-source-artifacts.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable
$testSource='Tests/Controls/Scope.Tests.RichDocument.cpp'
if((Get-FileHash -LiteralPath (Join-Path $Root $testSource)).Hash -cne $before[$testSource]){throw 'Lookup oracle changed between measurements.'}
foreach($file in Get-ChildItem -LiteralPath (Join-Path $work 'Diagnostics') -File){
    $name=$file.Name
    if($name.EndsWith('.raw.json') -or $name.EndsWith('.child.json')){$name=$name.Substring(0,$name.Length-5)+'.receipt.txt'}
    Copy-Exact $file.FullName ('Diagnostics/'+$name)
}
@{classification='exact-native-and-cross-build-functional-evidence';nativeSuites=$suites.ToArray();nativeSuiteCount=$suites.Count;
    linkedCanonicalReports=$performances.Count;sourceFingerprint=$expectedSource;lookupTestSha256=$before[$testSource];copies=$copied.ToArray();
    sourceQualification='x64 Debug/Release/ASan Debug executed; ARM64 Debug/Release/ASan Debug cross-built only; foreground/real IME/AT and performance acceptance unqualified.'} |
    ConvertTo-Json -Depth 16 | Set-Content (Join-Path $Packet 'inventory.receipt.txt') -Encoding utf8
Write-Host "Archived $($suites.Count) exact current native suites and $($performances.Count) original canonical benchmark reports."
