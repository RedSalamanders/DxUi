[CmdletBinding()] param([string] $Root,[string] $Packet,[string] $ExpectedSource)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Packet){throw 'Preserve the previous packet; use a fresh directory.'}
$Packet=[IO.Path]::GetFullPath($Packet)
$work=Join-Path $Root '.build/reports/rich-source-changes-20261018'
New-Item -ItemType Directory -Path $Packet | Out-Null
$copies=[Collections.Generic.List[object]]::new()
function Copy-Exact([string] $From,[string] $Relative){
    $to=[IO.Path]::GetFullPath((Join-Path $Packet $Relative))
    if(-not $to.StartsWith(($Packet+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)){throw 'Archive path escapes packet.'}
    New-Item -ItemType Directory -Path (Split-Path $to) -Force | Out-Null
    $hash=(Get-FileHash -LiteralPath $From).Hash
    Copy-Item -LiteralPath $From -Destination $to
    if((Get-FileHash -LiteralPath $to).Hash -cne $hash){throw 'Archive copy changed bytes.'}
    $copies.Add(@{source=$From;path=$Relative.Replace('\','/');sha256=$hash;bytes=(Get-Item -LiteralPath $From).Length})
}
function Read-Receipt([string] $Path){Get-Content -LiteralPath $Path -Raw|ConvertFrom-Json -AsHashtable -Depth 32}
$scopes=@('Foundation','Embedded','Grid','Theme','Control','MenuExitLifetime','NewControls','EditorControls','TextField','MultilineText','ReadOnly','ComboBox','Tree','Tooltip','Rendering','Animation','Accessibility','WindowHost','InteractiveLease')
$native=[Collections.Generic.List[object]]::new()
$performances=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($configuration in @('Release','Debug','ASan Debug')){foreach($scope in $scopes){
    $stem="$scope-x64-$configuration"
    $file=Join-Path $Root ".build/reports/$stem.json"
    $receipt=Read-Receipt $file
    if($receipt.suite -cne $scope -or $receipt.configuration -cne $configuration -or $receipt.nativeArchitecture -cne 'X64' -or
        $receipt.platform -cne 'x64' -or $receipt.exitCode -ne 0 -or $receipt.skips.Count -or $receipt.Contains('tests')){throw 'Not a full matching native success.'}
    if((Get-FileHash -LiteralPath $receipt.executable).Hash -cne $receipt.sha256){throw 'Native artifact differs from its successful run.'}
    $performance=Read-Receipt $receipt.performanceReport
    if($performance.sourceFingerprint -cne $ExpectedSource -or $performance.configuration -cne $configuration -or
        $performance.executableSha256 -cne $receipt.performanceExecutableSha256){throw 'Native/performance binding differs.'}
    if((ConvertTo-Json $receipt.performanceScenarios -Depth 16 -Compress) -cne (ConvertTo-Json $performance.scenarios -Depth 16 -Compress)){throw 'Original frame values differ.'}
    [void]$performances.Add($receipt.performanceReport)
    Copy-Exact $file "Native/$stem.receipt.txt"
    Copy-Exact (Join-Path $Root ".build/logs/test-$stem.log") "Native/$stem.log"
    $native.Add(@{suite=$scope;configuration=$configuration;sha256=$receipt.sha256;performance=(Split-Path $receipt.performanceReport -Leaf)})
}}
# Also retain canonical bindings from original, preliminary and failed intermediate runs.
foreach($file in Get-ChildItem -LiteralPath $work -Recurse -File -Filter '*.receipt.txt'){
    $value=Read-Receipt $file.FullName
    if($value.Contains('performanceReport')){[void]$performances.Add([string]$value.performanceReport)}
}
foreach($file in Get-ChildItem -LiteralPath $work -Recurse -File){
    $relative=[IO.Path]::GetRelativePath($work,$file.FullName).Replace('\','/')
    if($relative -in @('Archive.ps1','Verify.ps1','Invoke-Qualification.ps1','Invoke-Before.ps1')){Copy-Exact $file.FullName $relative;continue}
    if($relative.EndsWith('.json')){$relative=$relative.Substring(0,$relative.Length-5)+'.receipt.txt'}
    Copy-Exact $file.FullName ('Raw/'+$relative)
}
foreach($file in Get-ChildItem -LiteralPath $work -Recurse -File -Filter '*.log'){
    foreach($line in Get-Content -LiteralPath $file.FullName){
        if($line -match '^Log: (.+)$' -and (Test-Path -LiteralPath $Matches[1])){
            Copy-Exact $Matches[1] ('Build/Logs/'+(Split-Path $Matches[1] -Leaf))
        }
    }
}
foreach($path in $performances){
    Copy-Exact $path ('Performance/'+(Split-Path $path -Leaf))
    Copy-Exact ($path+'.comparison.json') ('Performance/'+(Split-Path ($path+'.comparison.json') -Leaf))
}
foreach($platform in @('x64','ARM64')){foreach($configuration in @('Release','Debug','ASan Debug')){
    $profile=$platform+'-'+$configuration.Replace(' ','')
    foreach($kind in @('build','incremental')){Copy-Exact (Join-Path $Root ".build/reports/performance-build/$profile/$kind.json") "Build/$profile-$kind.receipt.txt"}
}}
Copy-Exact (Join-Path $Root '.build/reports/AddressSanitizer-x64.json') 'Native/AddressSanitizer-x64.receipt.txt'
Copy-Exact (Join-Path $Root '.build/logs/test-AddressSanitizer-x64.log') 'Native/AddressSanitizer-x64.log'
foreach($path in @('include/DxUi/RichDocument.h','src/Controls/DxUi.RichDocument.cpp','src/Controls/DxUi.Accessibility.cpp',
 'Tests/Controls/Scope.Tests.RichDocument.cpp','Tests/Controls/Scope.Tests.TextElementMovement.cpp',
 'Tests/Controls/Scope.Tests.RichSourceChanges.h','Tests/Controls/Scope.Tests.RichSourceChanges.cpp',
 'Tests/Embedded/Scope.Tests.RichDocumentStorageResources.h','Tests/Embedded/Scope.Tests.RichTextResources.h','Tests/Embedded/Embedded.Tests.Embedded.cpp','Tests/native-test-files.json')){
 $relative='Source/'+$path
 if($relative.EndsWith('.json')){$relative=$relative.Substring(0,$relative.Length-5)+'.receipt.txt'}
 Copy-Exact (Join-Path $Root $path) $relative
}
@{classification='bounded-source-changes-and-localized-transient-functional-evidence';sourceFingerprint=$ExpectedSource;
    nativeSuiteCount=$native.Count;nativeSuites=$native.ToArray();canonicalReportCount=$performances.Count;copies=$copies.ToArray();
    limitations='Before/after edit/read/single-step timings are descriptive; performance acceptance, physical IME/AT and native ARM64 execution remain open.'} |
    ConvertTo-Json -Depth 16|Set-Content (Join-Path $Packet 'inventory.receipt.txt') -Encoding utf8
Write-Host "Archived $($native.Count) native successes, original and intermediate outcomes and $($performances.Count) original canonical reports."
