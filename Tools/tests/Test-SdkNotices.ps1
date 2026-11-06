Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $root 'Tools/SdkNotices.psm1') -Force
$scratch=Join-Path $root ('.build/sdk-notice-fixtures/'+[guid]::NewGuid().ToString('N'))
$stage=Join-Path $scratch 'stage'
$installed=Join-Path $scratch 'installed'
[void][IO.Directory]::CreateDirectory((Join-Path $installed 'share/wil'))
[IO.File]::WriteAllText((Join-Path $installed 'share/wil/copyright'),"Synthetic restored WIL notice — inventory fixture only`n")
$commit='0123456789abcdef0123456789abcdef01234567'
$arguments=@{Root=$root;InstalledRoot=$installed;Stage=$stage;SourceCommit=$commit}
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
function Refuses([scriptblock]$Action,[string]$Pattern){
    $refused=$false
    try{& $Action|Out-Null}catch [System.Management.Automation.RuntimeException]{if($_.Exception.Message -notlike $Pattern){throw};$refused=$true}
    Require $refused ('Missing refusal: '+$Pattern)
}
Copy-DxUiSdkNotices @arguments
$inventory=@(Assert-DxUiSdkNotices @arguments)
# This independent required set must change deliberately when redistributable
# inputs change; it is not derived from the staging implementation's map.
$required=@('LICENSE.txt','THIRD-PARTY-NOTICES.md','WIL-LICENSE.txt','katex-LICENSE.txt','tomlplusplus-LICENSE.txt',
            'rapidyaml-LICENSE.txt','rapidyaml-bundled-NOTICES.txt','libfyaml-LICENSE.txt','libfyaml-xxHash-LICENSE.txt','Unicode-LICENSE.txt','WHATWG-LICENSE.txt')
Require (@(Compare-Object $required @(Get-ChildItem (Join-Path $stage 'licenses') -Name) -CaseSensitive).Count -eq 0) 'Exact independently reviewed license set'
Require ($inventory.Count -eq 11) 'All license bytes are recorded'
foreach($pair in @(
    @('Unicode-LICENSE.txt','ThirdParty/Unicode/17.0.0/LICENSE.txt'),
    @('WHATWG-LICENSE.txt','ThirdParty/Markdown/whatwg-2026-10-18/LICENSE.txt'),
    @('libfyaml-LICENSE.txt','ThirdParty/libfyaml/0.9.4/LICENSE'),
    @('libfyaml-xxHash-LICENSE.txt','ThirdParty/libfyaml/0.9.4/LICENSE-xxHash.txt'),
    @('WIL-LICENSE.txt','installed/share/wil/copyright')
)){
    $source=if($pair[0] -eq 'WIL-LICENSE.txt'){Join-Path $scratch $pair[1]}else{Join-Path $root $pair[1]}
    Require ((Get-FileHash (Join-Path $stage ('licenses/'+$pair[0]))).Hash -ceq (Get-FileHash $source).Hash) ('Full original license bytes: '+$pair[0])
}
$notice=[IO.File]::ReadAllText((Join-Path $stage 'licenses/THIRD-PARTY-NOTICES.md'))
Require ($notice.Contains('Unicode 17.0.0') -and $notice.Contains('WHATWG') -and $notice.Contains('Unicode-LICENSE.txt') -and $notice.Contains('WHATWG-LICENSE.txt')) 'Embedded data attributions and local licenses'
Require ($notice.Contains('libfyaml 0.9.4') -and $notice.Contains('libfyaml-LICENSE.txt') -and $notice.Contains('libfyaml-xxHash-LICENSE.txt')) 'Private YAML backend attributions and offline licenses'
Require ($notice.Contains("https://github.com/RedSalamanders/DxUi/blob/$commit/vcpkg.json")) 'Repository-only provenance is pinned'
foreach($match in [regex]::Matches($notice,'\]\(([^)]+)\)')){
    $link=$match.Groups[1].Value
    if($link -notmatch '^(?:https?://|#)'){Require (Test-Path -LiteralPath (Join-Path $stage ('licenses/'+$link)) -PathType Leaf) ('Offline notice link: '+$link)}
}
$zip=Join-Path $scratch 'notices.zip'
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip)
$extracted=Join-Path $scratch 'extracted'
[IO.Compression.ZipFile]::ExtractToDirectory($zip,$extracted)
$extractedArguments=$arguments.Clone();$extractedArguments.Stage=$extracted
$after=@(Assert-DxUiSdkNotices @extractedArguments)
Require (@(Compare-Object ($inventory|ForEach-Object {$_.path+':'+$_.sha256}) ($after|ForEach-Object {$_.path+':'+$_.sha256})).Count -eq 0) 'Extracted license hash closure'
$unicode=Join-Path $extracted 'licenses/Unicode-LICENSE.txt'
Remove-Item -LiteralPath $unicode
Refuses {Assert-DxUiSdkNotices @extractedArguments} '*license set differs*'
Copy-Item -LiteralPath (Join-Path $stage 'licenses/Unicode-LICENSE.txt') -Destination $unicode
[IO.File]::AppendAllText($unicode,'changed')
Refuses {Assert-DxUiSdkNotices @extractedArguments} '*license bytes differ: Unicode-LICENSE.txt'
Copy-Item -LiteralPath (Join-Path $stage 'licenses/Unicode-LICENSE.txt') -Destination $unicode
[IO.File]::AppendAllText((Join-Path $extracted 'licenses/THIRD-PARTY-NOTICES.md'),'broken projection')
Refuses {Assert-DxUiSdkNotices @extractedArguments} '*source notices differ*'
Refuses {Copy-DxUiSdkNotices -Root $root -InstalledRoot $installed -Stage (Join-Path $scratch 'bad-commit') -SourceCommit 'main'} '*exact source commit*'
# Bind the public package wrapper to both admission boundaries and its receipt.
$wrapper=[IO.File]::ReadAllText((Join-Path $root 'Tools/New-SdkPackage.ps1'))
Require ($wrapper.Contains('Copy-DxUiSdkNotices -Root $root') -and [regex]::Matches($wrapper,'Assert-DxUiSdkNotices -Root \$root').Count -eq 2 -and
         $wrapper.Contains('licenses=$licenseInventory') -and $wrapper.Contains("`$manifest['licenseClosurePassed']=`$true")) 'Real SDK staging/extraction and receipt use the license guard'
$project=Get-Content -LiteralPath (Join-Path $root '.ci/project.json') -Raw|ConvertFrom-Json
foreach($inputPath in @('LICENSE.txt','THIRD-PARTY-NOTICES.md','docs/licenses/**')){
    Require ($inputPath -cin $project.include) ('Explicit packaging input overrides documentation exclusions: '+$inputPath)
}
$policy=Join-Path $root '.ci-engine/Tools/Modules/Ci.Policy.psm1'
if(Test-Path -LiteralPath $policy){
    Import-Module $policy -Force
    $configuration=Read-CiProject $root
    foreach($inputPath in @('LICENSE.txt','THIRD-PARTY-NOTICES.md','docs/licenses/Unicode.txt','docs/licenses/WHATWG.txt','docs/licenses/future-component.txt','Tools/SdkNotices.psm1')){
        Require (Get-CiPathDisposition -Path $inputPath -Project $configuration).native ('Pinned engine includes packaging dependency: '+$inputPath)
    }
    Require (-not (Get-CiPathDisposition -Path 'docs/README.md' -Project $configuration).native) 'Ordinary documentation remains outside native packaging inputs'
    Write-Host 'Pinned CI engine classifies every SDK notice dependency as a packaging/native input.'
}
Write-Host 'SDK notices: exact set, original Unicode/WHATWG/WIL bytes, offline links, pinned provenance, extraction, missing/mutated/license-projection and bad-identity refusals passed.'
