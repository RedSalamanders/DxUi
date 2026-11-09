# Release library input provenance only; never a native test, Full or publication result.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$script:performance=Import-Module (Join-Path $PSScriptRoot 'PerformancePolicy.psm1') -PassThru
$script:scoped=Import-Module (Join-Path $PSScriptRoot 'ScopedTesting.psm1') -PassThru

function Assert-DxUiSdkProducerTooling {
    param([Collections.IDictionary]$Tooling)
    if($null -eq $Tooling -or $Tooling.Count -ne 3 -or $Tooling.repository -cne 'RedSalamanders/DxUi' -or
        $Tooling.commit -cnotmatch '^[a-f0-9]{40}$' -or $Tooling.contentIdentity -cnotmatch '^[A-Fa-f0-9]{64}$'){
        throw 'SDK producer tooling requires an exact source and content identity, without verdict fields.'
    }
}

function Assert-DxUiSdkOwnedPath {
    param([string]$Root,[string]$Path)
    $base=[IO.Path]::GetFullPath($Root).TrimEnd('\','/')
    if($base -eq [IO.Path]::GetPathRoot($base).TrimEnd('\','/') -or -not(Test-Path -LiteralPath (Join-Path $base '.git'))){throw 'SDK records require an owned repository, never a drive root.'}
    $path=[IO.Path]::GetFullPath($Path)
    $prefix=Join-Path $base '.build'
    $comparison=if([OperatingSystem]::IsWindows()){[StringComparison]::OrdinalIgnoreCase}else{[StringComparison]::Ordinal}
    if(-not $path.StartsWith(($prefix+[IO.Path]::DirectorySeparatorChar),$comparison)){throw 'SDK input operations must remain under the owned repository build directory.'}
    for($cursor=$path;$cursor;$cursor=Split-Path -Parent $cursor){
        if((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Reparse SDK receipt paths are unsupported.'}
    }
    return $path
}

function Get-DxUiSdkReceiptPath {
    param([string]$Root,[ValidateSet('x64','ARM64')][string]$Platform)
    return Assert-DxUiSdkOwnedPath $Root (Join-Path ([IO.Path]::GetFullPath($Root)) ".build/reports/sdk-library/$Platform/receipt.json")
}

function Remove-DxUiSdkLibraryReceipt {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform)
    $path=Get-DxUiSdkReceiptPath $Root $Platform
    if(Test-Path -LiteralPath $path -PathType Leaf){[IO.File]::Delete($path)}
    $producer=Assert-DxUiSdkOwnedPath $Root (Join-Path (Split-Path $path) 'source-producer.json')
    if(Test-Path -LiteralPath $producer -PathType Leaf){[IO.File]::Delete($producer)}
}

function Complete-DxUiSdkSourceBuildRecord {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform,
        [Parameter(Mandatory)][Collections.IDictionary]$Tooling,[Parameter(Mandatory)][Collections.IDictionary]$LibraryRecord,
        [Parameter(Mandatory)][string[]]$Arguments,[Parameter(Mandatory)][string]$LogPath)
    $path=Assert-DxUiSdkOwnedPath $Root (Join-Path (Split-Path (Get-DxUiSdkReceiptPath $Root $Platform)) 'source-producer.json')
    Assert-DxUiSdkProducerTooling $Tooling
    $log=Assert-DxUiSdkOwnedPath $Root $LogPath
    if(-not(Test-Path -LiteralPath $log -PathType Leaf)){throw 'Pinned-source library inputs require their original build log.'}
    if($LibraryRecord.inputState.sourceDirty){throw 'Pinned SDK source must remain clean and committed.'}
    $record=[ordered]@{schemaVersion=1;role='PinnedSourceSdkBuildInputs';sourceCommit=$LibraryRecord.inputState.sourceCommit;platform=$Platform;
        tooling=$Tooling;libraryInputDigest=$LibraryRecord.digest;commandIdentity=(Get-ScopedDigest ($Arguments|ConvertTo-Json -Compress));
        diagnosticLog=([IO.Path]::GetRelativePath([IO.Path]::GetFullPath($Root),$log).Replace('\','/'));
        diagnosticLogSha256=(Get-FileHash -LiteralPath $log).Hash.ToLowerInvariant()}
    $temporary=$path+'.'+[guid]::NewGuid().ToString('N')+'.tmp'
    try{
        [IO.File]::WriteAllText($temporary,($record|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
        [IO.File]::Move($temporary,$path,$true)
    }finally{if(Test-Path -LiteralPath $temporary){[IO.File]::Delete($temporary)}}
    return $record
}

function Assert-DxUiSdkSourceBuildRecord {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform,
        [Parameter(Mandatory)][Collections.IDictionary]$Tooling,[Parameter(Mandatory)][Collections.IDictionary]$LibraryRecord)
    $path=Assert-DxUiSdkOwnedPath $Root (Join-Path (Split-Path (Get-DxUiSdkReceiptPath $Root $Platform)) 'source-producer.json')
    Assert-DxUiSdkProducerTooling $Tooling
    if(-not(Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -gt 16KB){throw 'No bounded pinned-source SDK build descriptor.'}
    $record=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json -AsHashtable
    if($record.Count -ne 9 -or $record.schemaVersion -ne 1 -or $record.role -cne 'PinnedSourceSdkBuildInputs' -or
        $record.platform -cne $Platform -or $record.sourceCommit -cne $LibraryRecord.inputState.sourceCommit -or
        $record.libraryInputDigest -cne $LibraryRecord.digest -or
        ($record.tooling|ConvertTo-Json -Depth 8 -Compress) -cne ($Tooling|ConvertTo-Json -Depth 8 -Compress) -or
        $record.commandIdentity -cnotmatch '^[a-f0-9]{64}$' -or $record.diagnosticLog -cnotmatch '^\.build/logs/[A-Za-z0-9_.-]+\.log$'){
        throw 'Pinned-source SDK tooling, command or library input identity changed.'
    }
    $log=Assert-DxUiSdkOwnedPath $Root (Join-Path $Root $record.diagnosticLog)
    if(-not(Test-Path -LiteralPath $log -PathType Leaf) -or (Get-FileHash -LiteralPath $log).Hash.ToLowerInvariant() -cne $record.diagnosticLogSha256){throw 'Pinned-source SDK build diagnostics changed or are missing.'}
    return $record
}

function Get-DxUiSdkLibraryInputState {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform,
        [Collections.IDictionary]$BuildState)
    if($null -eq $BuildState){$BuildState=& $script:performance {param($r,$p) Get-PerformanceBuildState -Root $r -Platform $p -Configuration Release} $Root $Platform}
    if($BuildState['buildIdentityVersion'] -ne 2){throw 'SDK inputs require the complete v2 native build identity.'}
    $state=[ordered]@{buildIdentityVersion=2}
    foreach($name in @('resolvedBuildIdentity','buildInputIdentity','toolchainIdentity')){
        if([string]$BuildState[$name] -cnotmatch '^[A-Fa-f0-9]{64}$'){throw "SDK native identity omits $name."}
        $state[$name]=([string]$BuildState[$name]).ToLowerInvariant()
    }
    $head=(& $script:scoped {param($r) Invoke-ScopedGit $r @('rev-parse','--verify','HEAD')} $Root).Trim()
    if($head -cnotmatch '^[a-f0-9]{40}$'){throw 'SDK inputs require an exact source commit.'}
    $dirty=(& $script:scoped {param($r) Invoke-ScopedGit $r @('status','--porcelain','--untracked-files=all')} $Root).Trim()
    $absolute=[IO.Path]::GetFullPath($Root).TrimEnd('\','/')
    if([OperatingSystem]::IsWindows()){$absolute=$absolute.ToUpperInvariant()}
    return [ordered]@{schemaVersion=1;role='ReleaseLibraryBuildInputs';rootIdentity=(Get-ScopedDigest $absolute);
        sourceCommit=$head;sourceDirty=[bool]$dirty;platform=$Platform;configuration='Release';nativeInputs=$state}
}

function Get-DxUiSdkLibraryFiles {
    param([string]$Root,[string]$Platform)
    Assert-ScopedBuildOutputs $Root $Platform Release @('DxUi')
    return @(foreach($name in @('DxUi.lib','DxUi.pdb')){
        $path=Join-Path $Root ".build/$Platform/Release/$name"
        $file=Get-Item -LiteralPath $path -Force
        if($file.Length -le 0){throw "Empty SDK library input: $name"}
        [ordered]@{path=$name;length=$file.Length;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
    })
}

function Get-DxUiSdkRecordDigest {
    param([Collections.IDictionary]$State,[object[]]$Files)
    return Get-ScopedDigest ('dxui-sdk-library-inputs-v1'+"`n"+($State|ConvertTo-Json -Depth 8 -Compress)+"`n"+($Files|ConvertTo-Json -Depth 5 -Compress))
}

function Complete-DxUiSdkLibraryBuild {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform,
        [Parameter(Mandatory)][Collections.IDictionary]$Before,[Collections.IDictionary]$BuildState)
    $path=Get-DxUiSdkReceiptPath $Root $Platform
    try{
        $after=Get-DxUiSdkLibraryInputState -Root $Root -Platform $Platform -BuildState $BuildState
        if(($Before|ConvertTo-Json -Depth 8 -Compress) -cne ($after|ConvertTo-Json -Depth 8 -Compress)){throw 'SDK native inputs changed during the build.'}
        $files=@(Get-DxUiSdkLibraryFiles $Root $Platform)
        $record=[ordered]@{schemaVersion=1;role='ReleaseLibraryBuildInputs';inputState=$after;files=$files;digest=(Get-DxUiSdkRecordDigest $after $files)}
        [void][IO.Directory]::CreateDirectory((Split-Path $path))
        $temporary=$path+'.'+[guid]::NewGuid().ToString('N')+'.tmp'
        try{
            [IO.File]::WriteAllText($temporary,($record|ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
            [IO.File]::Move($temporary,$path,$true)
        }finally{if(Test-Path -LiteralPath $temporary){[IO.File]::Delete($temporary)}}
        return $record
    }catch{Remove-DxUiSdkLibraryReceipt $Root $Platform;throw}
}

function Assert-DxUiSdkLibraryBuild {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform,
        [switch]$RequireClean,[Collections.IDictionary]$BuildState)
    $path=Get-DxUiSdkReceiptPath $Root $Platform
    if(-not(Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -gt 64KB){throw 'No bounded SDK library build record; build Release through the governed entrypoint.'}
    $record=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json -AsHashtable
    if($record.Count -ne 5 -or $record.schemaVersion -ne 1 -or $record.role -cne 'ReleaseLibraryBuildInputs'){throw 'Invalid SDK library input record.'}
    $actual=Get-DxUiSdkLibraryInputState -Root $Root -Platform $Platform -BuildState $BuildState
    if($RequireClean -and $actual.sourceDirty){throw 'SDK qualification requires clean committed source.'}
    if(($record.inputState|ConvertTo-Json -Depth 8 -Compress) -cne ($actual|ConvertTo-Json -Depth 8 -Compress)){throw 'SDK source, dependency, profile, root or toolchain input identity changed.'}
    $files=@(Get-DxUiSdkLibraryFiles $Root $Platform)
    if(($record.files|ConvertTo-Json -Depth 5 -Compress) -cne ($files|ConvertTo-Json -Depth 5 -Compress) -or
        $record.digest -cne (Get-DxUiSdkRecordDigest $actual $files)){throw 'SDK library/PDB bytes or input record changed.'}
    return $record
}

function Assert-DxUiSdkReleaseProfile {
    # The SDK integration promises /MD, LTCG, latest C++ and enabled STL annotations.
    # A complete digest detects changes but cannot, by itself, establish that compatibility promise.
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform)
    if($env:CL -or $env:_CL_){throw 'SDK qualification does not support CL/_CL_ option overrides.'}
    $metadata=& $script:performance {
        param($r,$p)
        Import-Module (Join-Path $r 'Tools/VisualStudio.psm1')
        $installation=Get-DxUiVisualStudioInstallation
        Get-PerformanceProjectMetadataCore -MSBuildPath (Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe') `
            -ProjectPath (Join-Path $r 'src/DxUi.vcxproj') -Configuration Release -Platform $p `
            -OutputRoot ((Join-Path $r '.build')+[IO.Path]::DirectorySeparatorChar) -Root $r -Role library
    } $Root $Platform
    Assert-DxUiSdkEvaluatedProfile -Items @($metadata.items.ClCompile)
}

function Assert-DxUiSdkEvaluatedProfile {
    param([object[]]$Items)
    if(-not $Items.Count){throw 'SDK qualification requires evaluated compiler items.'}
    foreach($item in $Items){
        if($item.RuntimeLibrary -cne 'MultiThreadedDLL' -or $item.LanguageStandard -cne 'stdcpplatest' -or
            $item.WholeProgramOptimization -cne 'true' -or $item.EnableASAN -cne 'false' -or
            $item.DebugInformationFormat -cne 'ProgramDatabase' -or $item.ExceptionHandling -cne 'Sync' -or
            $item['ForcedIncludeFiles'] -or $item.UndefineAllPreprocessorDefinitions -cne 'false' -or $item['UndefinePreprocessorDefinitions']){
            throw 'SDK requires the evaluated Release /MD, latest C++, LTCG and compiler-PDB profile without ASan.'
        }
        $definitions=@(([string]$item.PreprocessorDefinitions).Split(';',[StringSplitOptions]::RemoveEmptyEntries))
        if('NDEBUG' -cnotin $definitions -or ($definitions -match '^(?:_DEBUG|_DISABLE_(?:STL|VECTOR|STRING)_ANNOTATION)(?:=|$)')){
            throw 'SDK qualification requires Release definitions with STL annotations enabled.'
        }
        # Admit only the supported library options; opaque profile-changing switches must not override metadata.
        foreach($option in @(([string]$item.AdditionalOptions).Split([char[]]" `t`r`n",[StringSplitOptions]::RemoveEmptyEntries))){
            if($option -cnotin @('/utf-8','/Zc:preprocessor')){throw 'Unsupported additional compiler options for SDK qualification.'}
        }
    }
}

function Export-DxUiSdkLibraryInputs {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform,
        [Parameter(Mandatory)][ValidatePattern('^[1-9][0-9]{0,19}$')][string]$RunId,
        [Parameter(Mandatory)][ValidateRange(1,100000)][int]$RunAttempt)
    $destination=Assert-DxUiSdkOwnedPath $Root (Join-Path $Root ('.build/sdk-library-transfer/'+$Platform+'/'+[guid]::NewGuid().ToString('N')))
    $lease=Enter-ScopedProfileLease $Root $Platform Release
    try{
        $before=Assert-DxUiSdkLibraryBuild $Root $Platform -RequireClean
        [void][IO.Directory]::CreateDirectory($destination)
        foreach($file in $before.files){[IO.File]::Copy((Join-Path $Root ".build/$Platform/Release/$($file.path)"),(Join-Path $destination $file.path),$false)}
        [IO.File]::Copy((Get-DxUiSdkReceiptPath $Root $Platform),(Join-Path $destination 'build.json'),$false)
        $files=@(foreach($name in @('DxUi.lib','DxUi.pdb','build.json')){
            $path=Join-Path $destination $name
            [ordered]@{path=$name;length=(Get-Item -LiteralPath $path).Length;sha256=(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()}
        })
        foreach($file in $before.files){
            $copied=@($files|Where-Object path -CEQ $file.path)
            if($copied.Count -ne 1 -or $copied[0].sha256 -cne $file.sha256 -or $copied[0].length -ne $file.length){throw 'SDK transfer changed the library/PDB; retained incomplete staging is not admissible.'}
        }
        $after=Assert-DxUiSdkLibraryBuild $Root $Platform -RequireClean
        if($after.digest -cne $before.digest){throw 'SDK source/library provenance changed during transfer staging.'}
        $record=[ordered]@{schemaVersion=1;role='SdkReleaseLibraryTransfer';repository='RedSalamanders/DxUi';runId=$RunId;runAttempt=$RunAttempt;
            sourceCommit=$before.inputState.sourceCommit;platform=$Platform;files=$files}
        # A complete transfer marker is written last; the upload is authenticated by the owning workflow run.
        [IO.File]::WriteAllText((Join-Path $destination 'transfer.json'),($record|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
        return $destination
    }finally{Exit-ScopedProfileLease $lease}
}

function Import-DxUiSdkLibraryInputs {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform,
        [string]$Directory='',[Parameter(Mandatory)][ValidatePattern('^[1-9][0-9]{0,19}$')][string]$RunId,
        [Parameter(Mandatory)][ValidateRange(1,100000)][int]$RunAttempt)
    if(-not $Directory){return [pscustomobject]@{hit=$false;reason='No SDK library input artifact was supplied.'}}
    $directory=Assert-DxUiSdkOwnedPath $Root $Directory
    $marker=Assert-DxUiSdkOwnedPath $Root (Join-Path $directory 'transfer.json')
    if(-not(Test-Path -LiteralPath $marker -PathType Leaf)){return [pscustomobject]@{hit=$false;reason='No complete SDK transfer marker; build from source.'}}
    if((Get-Item -LiteralPath $marker).Length -gt 64KB){throw 'Oversized SDK transfer marker.'}
    $transfer=Get-Content -LiteralPath $marker -Raw|ConvertFrom-Json -AsHashtable
    if($transfer.Count -ne 8 -or $transfer.schemaVersion -ne 1 -or $transfer.role -cne 'SdkReleaseLibraryTransfer' -or
        $transfer.repository -cne 'RedSalamanders/DxUi' -or $transfer.runId -cne $RunId -or $transfer.runAttempt -ne $RunAttempt -or
        $transfer.platform -cne $Platform){return [pscustomobject]@{hit=$false;reason='SDK transfer provenance/profile does not match this qualification run.'}}
    $expected=@('DxUi.lib','DxUi.pdb','build.json')
    if(($transfer.files.path -join '|') -cne ($expected -join '|') -or
        (@(Get-ChildItem -LiteralPath $directory -Force|ForEach-Object Name|Sort-Object) -join '|') -cne (@($expected+'transfer.json'|Sort-Object) -join '|')){throw 'SDK transfer has unexpected or missing payload entries.'}
    foreach($file in $transfer.files){
        $path=Assert-DxUiSdkOwnedPath $Root (Join-Path $directory $file.path)
        if($file.Count -ne 3 -or $file.length -le 0 -or $file.length -gt 1GB -or $file.sha256 -cnotmatch '^[a-f0-9]{64}$' -or
            -not(Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -ne $file.length -or
            (Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -cne $file.sha256){return [pscustomobject]@{hit=$false;reason='SDK transfer payload integrity failed; retain it and build from source.'}}
    }
    if(($transfer.files|Measure-Object -Property length -Sum).Sum -gt 1GB -or
        (Get-Item -LiteralPath (Join-Path $directory 'build.json')).Length -gt 64KB){throw 'SDK transfer exceeds the bounded library input format.'}
    $sourceRecord=Get-Content -LiteralPath (Join-Path $directory 'build.json') -Raw|ConvertFrom-Json -AsHashtable
    $lease=Enter-ScopedProfileLease $Root $Platform Release
    try{
        $actual=Get-DxUiSdkLibraryInputState $Root $Platform
        if($sourceRecord.Count -ne 5 -or $sourceRecord.schemaVersion -ne 1 -or $sourceRecord.role -cne 'ReleaseLibraryBuildInputs' -or
            $actual.sourceDirty -or $transfer.sourceCommit -cne $actual.sourceCommit -or
            ($sourceRecord.inputState|ConvertTo-Json -Depth 8 -Compress) -cne ($actual|ConvertTo-Json -Depth 8 -Compress)){
            return [pscustomobject]@{hit=$false;reason='SDK source/dependency/root/evaluated-toolchain inputs differ; build from source.'}
        }
        if(($sourceRecord.files.path -join '|') -cne 'DxUi.lib|DxUi.pdb' -or
            $sourceRecord.digest -cne (Get-DxUiSdkRecordDigest $actual $sourceRecord.files)){throw 'Invalid original SDK library input record.'}
        foreach($file in $sourceRecord.files){
            $transferred=@($transfer.files|Where-Object path -CEQ $file.path)
            if($file.Count -ne 3 -or $transferred.Count -ne 1 -or $file.sha256 -cne $transferred[0].sha256 -or $file.length -ne $transferred[0].length){throw 'SDK transfer differs from the original library/PDB record.'}
        }
        # No library-only transfer can create or retain a full-solution/performance build receipt.
        Remove-ScopedBuildReceipts $Root $Platform Release
        Remove-ScopedIteration -Root $Root -Path (Join-Path $Root ".build/reports/scoped-tests/$Platform-Release/iteration.json")
        $performancePaths=& $script:performance {param($r,$p) Get-PerformanceBuildAttestationPath $r $p Release;Get-PerformanceIncrementalAttestationPath $r $p Release} $Root $Platform
        foreach($path in $performancePaths){$path=Assert-DxUiSdkOwnedPath $Root $path;if(Test-Path -LiteralPath $path -PathType Leaf){[IO.File]::Delete($path)}}
        Remove-DxUiSdkLibraryReceipt $Root $Platform
        $staging=Assert-DxUiSdkOwnedPath $Root (Join-Path $Root ('.build/sdk-library-import/'+[guid]::NewGuid().ToString('N')))
        [void][IO.Directory]::CreateDirectory($staging)
        foreach($name in @('DxUi.lib','DxUi.pdb')){[IO.File]::Copy((Join-Path $directory $name),(Join-Path $staging $name),$false)}
        foreach($file in @($sourceRecord.files)){
            if($file.path -cnotin @('DxUi.lib','DxUi.pdb') -or (Get-FileHash -LiteralPath (Join-Path $staging $file.path)).Hash.ToLowerInvariant() -cne $file.sha256){throw 'SDK import source changed; retained staging is not a build result.'}
        }
        $profile=Assert-DxUiSdkOwnedPath $Root (Join-Path $Root ".build/$Platform/Release")
        [void][IO.Directory]::CreateDirectory($profile)
        foreach($name in @('DxUi.lib','DxUi.pdb')){[IO.File]::Move((Join-Path $staging $name),(Join-Path $profile $name),$true)}
        $receipt=Get-DxUiSdkReceiptPath $Root $Platform
        [void][IO.Directory]::CreateDirectory((Split-Path $receipt))
        [IO.File]::Copy((Join-Path $directory 'build.json'),$receipt,$false)
        try{$verified=Assert-DxUiSdkLibraryBuild $Root $Platform -RequireClean}
        catch{Remove-DxUiSdkLibraryReceipt $Root $Platform;throw}
        return [pscustomobject]@{hit=$true;reason='Reused exact run-bound Release library/PDB inputs; extracted SDK smoke remains required.';digest=$verified.digest}
    }finally{Exit-ScopedProfileLease $lease}
}

function Invoke-DxUiSdkProductionLibrary {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform,
        [string]$Directory='',[Parameter(Mandatory)][string]$RunId,[Parameter(Mandatory)][int]$RunAttempt,
        [Parameter(Mandatory)][scriptblock]$BuildFromSource)
    $result=Import-DxUiSdkLibraryInputs -Root $Root -Platform $Platform -Directory $Directory -RunId $RunId -RunAttempt $RunAttempt
    Write-Host $result.reason
    if(-not $result.hit){
        # The governed build entrypoint revokes failed attempts and supplies a fresh library-only record.
        Remove-DxUiSdkLibraryReceipt -Root $Root -Platform $Platform
        try{
            & $BuildFromSource
            Assert-DxUiSdkLibraryBuild -Root $Root -Platform $Platform -RequireClean|Out-Null
        }catch{Remove-DxUiSdkLibraryReceipt -Root $Root -Platform $Platform;throw}
    }
    return $result
}

Export-ModuleMember -Function Get-DxUiSdkLibraryInputState,Remove-DxUiSdkLibraryReceipt,Complete-DxUiSdkLibraryBuild,Assert-DxUiSdkLibraryBuild,
    Export-DxUiSdkLibraryInputs,Import-DxUiSdkLibraryInputs,Assert-DxUiSdkReleaseProfile,Invoke-DxUiSdkProductionLibrary,
    Complete-DxUiSdkSourceBuildRecord,Assert-DxUiSdkSourceBuildRecord
