Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

function Get-NoticeFiles {
    param([string]$SourceCommit='')
    $files=@(
        @{Source='LICENSE.txt';Name='LICENSE.txt'},
        @{Source='share/wil/copyright';Name='WIL-LICENSE.txt';Installed=$true;Link='docs/licenses/WIL.txt'},
        @{Source='docs/licenses/katex.txt';Name='katex-LICENSE.txt'},
        @{Source='docs/licenses/tomlplusplus.txt';Name='tomlplusplus-LICENSE.txt'},
        @{Source='docs/licenses/rapidyaml.txt';Name='rapidyaml-LICENSE.txt'},
        @{Source='docs/licenses/rapidyaml-bundled.txt';Name='rapidyaml-bundled-NOTICES.txt'},
        @{Source='ThirdParty/libfyaml/0.9.4/LICENSE';Name='libfyaml-LICENSE.txt'},
        @{Source='ThirdParty/libfyaml/0.9.4/LICENSE-xxHash.txt';Name='libfyaml-xxHash-LICENSE.txt'},
        @{Source='docs/licenses/Unicode.txt';Name='Unicode-LICENSE.txt'},
        @{Source='docs/licenses/WHATWG.txt';Name='WHATWG-LICENSE.txt'}
    )
    # These exact historical pins have different compiled dependencies. Never infer a missing license is optional.
    if($SourceCommit -ceq '4cdf1ce52c7785446698e346b8aec058ed6a9283'){return @($files|Where-Object Name -cin @('LICENSE.txt','WIL-LICENSE.txt'))}
    if($SourceCommit -ceq 'f2e328a0a26a18ca66bf4b2aea77bf010c9515e1'){
        return @($files|Where-Object Name -cin @('LICENSE.txt','WIL-LICENSE.txt','katex-LICENSE.txt','tomlplusplus-LICENSE.txt','rapidyaml-LICENSE.txt','rapidyaml-bundled-NOTICES.txt'))
    }
    return $files
}

function Get-PackagedNotice([string]$Root,[string]$SourceCommit) {
    if($SourceCommit -cnotmatch '^[0-9a-f]{40}$'){throw 'SDK notices require an exact source commit.'}
    $links=@{}
    foreach($file in Get-NoticeFiles -SourceCommit $SourceCommit){$links[$(if($file.ContainsKey('Link')){$file.Link}else{$file.Source})]=$file.Name}
    $source=[IO.File]::ReadAllText((Join-Path $Root 'THIRD-PARTY-NOTICES.md'))
    # Keep licenses usable offline. Repository-only provenance links name the
    # exact public source tree instead of pointing outside the extracted SDK.
    [regex]::Replace($source,'(\[[^\]]+\]\()([^\s)]+)(\))',[Text.RegularExpressions.MatchEvaluator]{
        param($match)
        $target=$match.Groups[2].Value
        if($target -match '^(?:[a-zA-Z][a-zA-Z0-9+.-]*:|#)'){return $match.Value}
        $replacement=if($links.ContainsKey($target)){$links[$target]}else{"https://github.com/RedSalamanders/DxUi/blob/$SourceCommit/$target"}
        return $match.Groups[1].Value+$replacement+$match.Groups[3].Value
    })
}

function Copy-DxUiSdkNotices {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][string]$InstalledRoot,
          [Parameter(Mandatory)][string]$Stage,[Parameter(Mandatory)][string]$SourceCommit)
    $directory=Join-Path $Stage 'licenses'
    [void][IO.Directory]::CreateDirectory($directory)
    foreach($file in Get-NoticeFiles -SourceCommit $SourceCommit){
        $owner=if($file.ContainsKey('Installed') -and $file.Installed){$InstalledRoot}else{$Root}
        Copy-Item -LiteralPath (Join-Path $owner $file.Source) -Destination (Join-Path $directory $file.Name)
    }
    [IO.File]::WriteAllText((Join-Path $directory 'THIRD-PARTY-NOTICES.md'),(Get-PackagedNotice $Root $SourceCommit),[Text.UTF8Encoding]::new($false))
}

function Assert-DxUiSdkNotices {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][string]$InstalledRoot,
          [Parameter(Mandatory)][string]$Stage,[Parameter(Mandatory)][string]$SourceCommit)
    $directory=Join-Path $Stage 'licenses'
    $expected=@(Get-NoticeFiles -SourceCommit $SourceCommit)
    $names=@($expected.Name)+@('THIRD-PARTY-NOTICES.md')
    $actual=@(Get-ChildItem -LiteralPath $directory -Force)
    if(@($actual|Where-Object PSIsContainer).Count -or @(Compare-Object $names @($actual.Name) -CaseSensitive).Count){throw 'SDK license set differs from its distributable contract.'}
    foreach($file in $expected){
        $owner=if($file.ContainsKey('Installed') -and $file.Installed){$InstalledRoot}else{$Root}
        if((Get-FileHash -LiteralPath (Join-Path $directory $file.Name)).Hash -cne (Get-FileHash -LiteralPath (Join-Path $owner $file.Source)).Hash){throw ('SDK license bytes differ: '+$file.Name)}
    }
    if([IO.File]::ReadAllText((Join-Path $directory 'THIRD-PARTY-NOTICES.md')) -cne (Get-PackagedNotice $Root $SourceCommit)){throw 'SDK source notices differ from their pinned projection.'}
    foreach($file in $actual){[pscustomobject]@{path='licenses/'+$file.Name;sha256=(Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()}}
}

Export-ModuleMember -Function Copy-DxUiSdkNotices,Assert-DxUiSdkNotices
