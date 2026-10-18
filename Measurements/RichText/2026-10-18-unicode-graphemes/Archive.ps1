[CmdletBinding()] param([string] $Root, [string] $Packet, [string] $ExpectedSource)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (Test-Path -LiteralPath $Packet) { throw 'Preserve the existing packet; choose a new directory.' }
$Packet = [IO.Path]::GetFullPath($Packet)
$work = Join-Path $Root '.build/reports/rich-unicode-20261018'
New-Item -ItemType Directory -Path $Packet | Out-Null
$copies = [Collections.Generic.List[object]]::new()
function Copy-Exact([string] $From, [string] $Relative) {
    $to = [IO.Path]::GetFullPath((Join-Path $Packet $Relative))
    if (-not $to.StartsWith(($Packet + [IO.Path]::DirectorySeparatorChar), [StringComparison]::OrdinalIgnoreCase)) { throw 'Archive path escapes packet.' }
    New-Item -ItemType Directory -Path (Split-Path $to) -Force | Out-Null
    $hash = (Get-FileHash -LiteralPath $From).Hash
    Copy-Item -LiteralPath $From -Destination $to
    if ((Get-FileHash -LiteralPath $to).Hash -cne $hash) { throw 'Archive copy changed bytes.' }
    $copies.Add(@{ source = $From; path = $Relative.Replace('\', '/'); sha256 = $hash; bytes = (Get-Item -LiteralPath $From).Length })
}
function Read-Receipt([string] $Path) { Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -AsHashtable -Depth 32 }
$scopes = @('Foundation','Embedded','Grid','Theme','Control','MenuExitLifetime','NewControls','EditorControls','TextField','MultilineText','ReadOnly','ComboBox','Tree','Tooltip','Rendering','Animation','Accessibility','WindowHost','InteractiveLease')
$native = [Collections.Generic.List[object]]::new()
$performanceFiles = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($configuration in @('Release', 'Debug', 'ASan Debug')) {
    foreach ($scope in $scopes) {
        $stem = "$scope-x64-$configuration"
        $path = Join-Path $Root ".build/reports/$stem.json"
        $receipt = Read-Receipt $path
        if ($receipt.suite -cne $scope -or $receipt.configuration -cne $configuration -or $receipt.platform -cne 'x64' -or
            $receipt.nativeArchitecture -cne 'X64' -or $receipt.exitCode -ne 0 -or $receipt.skips.Count -or $receipt.Contains('tests')) { throw 'Not a complete current native success.' }
        if ((Get-FileHash -LiteralPath $receipt.executable).Hash -cne $receipt.sha256) { throw 'Native artifact changed after the successful run.' }
        $performance = Read-Receipt $receipt.performanceReport
        if ($performance.sourceFingerprint -cne $ExpectedSource -or $performance.configuration -cne $configuration -or
            $performance.executableSha256 -cne $receipt.performanceExecutableSha256) { throw 'Native/performance source or artifact binding differs.' }
        if ((ConvertTo-Json $receipt.performanceScenarios -Depth 16 -Compress) -cne (ConvertTo-Json $performance.scenarios -Depth 16 -Compress)) { throw 'Native frame values differ from their original report.' }
        [void]$performanceFiles.Add($receipt.performanceReport)
        Copy-Exact $path "Native/$stem.receipt.txt"
        Copy-Exact (Join-Path $Root ".build/logs/test-$stem.log") "Native/$stem.log"
        $native.Add(@{suite=$scope; configuration=$configuration; sha256=$receipt.sha256; performance=(Split-Path $receipt.performanceReport -Leaf)})
    }
}
foreach ($file in Get-ChildItem -LiteralPath $work -Recurse -File) {
    $relative = [IO.Path]::GetRelativePath($work, $file.FullName).Replace('\','/')
    if ($relative -in @('Archive.ps1','Verify.ps1','Invoke-Qualification.ps1')) { Copy-Exact $file.FullName $relative; continue }
    if ($relative.EndsWith('.json')) { $relative = $relative.Substring(0, $relative.Length - 5) + '.receipt.txt' }
    Copy-Exact $file.FullName ('Raw/' + $relative)
}
# The original filtered failure survives separately from the later full suite.
Copy-Exact (Join-Path $Root '.build/reports/EditorControls-x64-Release.filtered.json') 'Raw/before-corpus-native.receipt.txt'
foreach ($name in @('before-tests.log','release-first-tests.log','x64-Release.log','x64-Debug.log','x64-ASan-Debug.log','ARM64-Debug.log','ARM64-Release.log',
    'recovery/ARM64-Release.log','recovery/ARM64-ASan-Debug.log')) {
    foreach ($line in Get-Content -LiteralPath (Join-Path $work $name)) {
        if ($line -match '^Performance receipt: (.+)$') { [void]$performanceFiles.Add($Matches[1]) }
        if ($line -match '^Log: (.+)$') { Copy-Exact $Matches[1] ('Build/Logs/' + (Split-Path $Matches[1] -Leaf)) }
    }
}
Copy-Exact (Join-Path $work 'refused-ARM64-Release-msbuild.log') 'Build/Logs/refused-ARM64-Release-msbuild.log'
foreach ($path in $performanceFiles) {
    Copy-Exact $path ('Performance/' + (Split-Path $path -Leaf))
    Copy-Exact ($path + '.comparison.json') ('Performance/' + (Split-Path ($path + '.comparison.json') -Leaf))
}
foreach ($platform in @('x64','ARM64')) { foreach ($configuration in @('Release','Debug','ASan Debug')) {
    $profile = $platform + '-' + $configuration.Replace(' ','')
    foreach ($kind in @('build','incremental')) { Copy-Exact (Join-Path $Root ".build/reports/performance-build/$profile/$kind.json") "Build/$profile-$kind.receipt.txt" }
}}
Copy-Exact (Join-Path $Root '.build/reports/AddressSanitizer-x64.json') 'Native/AddressSanitizer-x64.receipt.txt'
Copy-Exact (Join-Path $Root '.build/logs/test-AddressSanitizer-x64.log') 'Native/AddressSanitizer-x64.log'
Copy-Exact (Join-Path $Root 'Tools/Generate-UnicodeGraphemeData.ps1') 'Generator/Generate-UnicodeGraphemeData.ps1'
Copy-Exact (Join-Path $Root 'src/Controls/DxUi.TextElementsData.h') 'Generated/DxUi.TextElementsData.h'
Copy-Exact (Join-Path $Root 'Tests/Controls/Scope.Tests.UnicodeGraphemeCorpus.h') 'Generated/Scope.Tests.UnicodeGraphemeCorpus.h'
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $Root 'ThirdParty/Unicode/17.0.0') -File) { Copy-Exact $file.FullName ('Inputs/' + $file.Name) }
foreach ($relative in @('src/Controls/DxUi.TextElements.cpp','src/Controls/DxUi.TextElements.h','Tests/Controls/Scope.Tests.TextElements.cpp',
    'Tests/Controls/Scope.Tests.RichDocument.cpp','Tests/Controls/Scope.Tests.RichTextLayout.cpp','Tests/Controls/Scope.Tests.RichTextAccessibility.cpp',
    'Tests/Controls/DxUi.Tests.TextField.cpp','Tests/Embedded/Scope.Tests.RichTextResources.h','src/Controls/DxUi.RichDocument.cpp',
    'src/Controls/DxUi.RichTextLayout.cpp','src/Controls/DxUi.SingleLineTextEditing.cpp','src/Controls/DxUi.TextInput.cpp',
    'Tools/tests/Test-UnicodeGraphemeData.ps1')) { Copy-Exact (Join-Path $Root $relative) ('Source/' + $relative) }
@{classification='unicode-17-native-functional-and-bounded-resource-evidence';sourceFingerprint=$ExpectedSource;nativeSuiteCount=$native.Count;
    nativeSuites=$native.ToArray();canonicalReportCount=$performanceFiles.Count;copies=$copies.ToArray();
    limits='64 Ki UTF16 units, 128 history; no ANSI API; ARM64 cross-build only; performance acceptance and physical IME/AT remain open.'} |
    ConvertTo-Json -Depth 16 | Set-Content -LiteralPath (Join-Path $Packet 'inventory.receipt.txt') -Encoding utf8
Write-Host "Archived $($native.Count) current native suites and $($performanceFiles.Count) original canonical reports."
