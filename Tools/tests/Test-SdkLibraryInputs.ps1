<# .SYNOPSIS Tests library input provenance with real Git/files and synthetic metadata; no native build or test verdict. #>
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$sdkModule=Import-Module (Join-Path $root 'Tools/SdkLibraryInputs.psm1') -Force -PassThru
$run=Join-Path $root ('.build/sdk-library-tests/'+[guid]::NewGuid().ToString('N'))
$fixture=Join-Path $run 'repository'
[void][IO.Directory]::CreateDirectory($fixture)
function Put([string]$Name,[string]$Value){$path=Join-Path $fixture $Name;[void][IO.Directory]::CreateDirectory((Split-Path $path));[IO.File]::WriteAllText($path,$Value,[Text.UTF8Encoding]::new($false))}
function Git([string[]]$Arguments){& (Get-Command git -CommandType Application|Select-Object -First 1).Source -C $fixture @Arguments *> $null;if($LASTEXITCODE){throw "Fixture Git failed: $Arguments"}}
function Require([bool]$Value,[string]$Message){if(-not $Value){throw $Message}}
function Refuses([scriptblock]$Body){$refused=$false;try{& $Body|Out-Null}catch{$refused=$true};Require $refused 'Expected SDK input refusal.'}
$passed=0
function Case([string]$Name,[scriptblock]$Body){& $Body;$script:passed++;Write-Host "PASS $Name"}
Put '.gitignore' ".build/`n"
Put 'src/source.cpp' 'synthetic source; never compiled'
Git @('init','-q','-b','main')
foreach($pair in @(@('user.name','SDK fixture'),@('user.email','fixture@example.invalid'),@('core.autocrlf','false'),@('commit.gpgsign','false'))){Git (@('config')+$pair)}
Git @('add','-A');Git @('commit','-qm','fixture baseline')
$state=[ordered]@{buildIdentityVersion=2;resolvedBuildIdentity=('a'*64);buildInputIdentity=('b'*64);toolchainIdentity=('c'*64)}
foreach($name in @('DxUi.lib','DxUi.pdb')){Put ".build/x64/Release/$name" ('synthetic '+$name+' bytes')}
$receipt=Join-Path $fixture '.build/reports/sdk-library/x64/receipt.json'
function FreshRecord {
    $before=Get-DxUiSdkLibraryInputState $fixture x64 -BuildState $state
    Complete-DxUiSdkLibraryBuild $fixture x64 -Before $before -BuildState $state|Out-Null
}
Case 'library-only inputs bind complete v2 state and never create native test or Full proof' {
    FreshRecord
    $actual=Assert-DxUiSdkLibraryBuild $fixture x64 -BuildState $state -RequireClean
    Require ($actual.role -ceq 'ReleaseLibraryBuildInputs' -and $actual.files.Count -eq 2) 'Wrong SDK input role or files.'
    Require (-not $actual.Contains('passed') -and -not $actual.Contains('executed')) 'Library inputs granted a test result.'
    Require (-not(Test-Path (Join-Path $fixture '.build/reports/scoped-tests/x64-Release/build.json'))) 'Library-only inputs fabricated Full build evidence.'
}
Case 'changed source/dependency/profile/toolchain identities refuse without timestamp shortcuts' {
    foreach($name in @('resolvedBuildIdentity','buildInputIdentity','toolchainIdentity')){
        $changed=[ordered]@{buildIdentityVersion=2;resolvedBuildIdentity=$state.resolvedBuildIdentity;buildInputIdentity=$state.buildInputIdentity;toolchainIdentity=$state.toolchainIdentity}
        $changed[$name]='d'*64
        Refuses {Assert-DxUiSdkLibraryBuild $fixture x64 -BuildState $changed}
    }
    Refuses {Get-DxUiSdkLibraryInputState $fixture x64 -BuildState @{buildIdentityVersion=1}}
}
Case 'same-size timestamp-restored archive corruption and absent symbols refuse' {
    $file=Join-Path $fixture '.build/x64/Release/DxUi.lib';$bytes=[IO.File]::ReadAllBytes($file);$stamp=[IO.File]::GetLastWriteTimeUtc($file)
    $changed=[byte[]]$bytes.Clone();$changed[0]=$changed[0] -bxor 1
    [IO.File]::WriteAllBytes($file,$changed);[IO.File]::SetLastWriteTimeUtc($file,$stamp)
    try{Refuses {Assert-DxUiSdkLibraryBuild $fixture x64 -BuildState $state}}finally{[IO.File]::WriteAllBytes($file,$bytes)}
    $pdb=Join-Path $fixture '.build/x64/Release/DxUi.pdb';[IO.File]::Move($pdb,$pdb+'.held')
    try{Refuses {Assert-DxUiSdkLibraryBuild $fixture x64 -BuildState $state}}finally{[IO.File]::Move($pdb+'.held',$pdb)}
}
Case 'changed and unexpected record fields cannot supply provenance' {
    $bytes=[IO.File]::ReadAllBytes($receipt)
    try{
        $record=Get-Content $receipt -Raw|ConvertFrom-Json -AsHashtable;$record['passed']=$true
        [IO.File]::WriteAllText($receipt,($record|ConvertTo-Json -Depth 10))
        Refuses {Assert-DxUiSdkLibraryBuild $fixture x64 -BuildState $state}
        [IO.File]::WriteAllBytes($receipt,$bytes)
        $record=Get-Content $receipt -Raw|ConvertFrom-Json -AsHashtable;$record.digest='e'*64
        [IO.File]::WriteAllText($receipt,($record|ConvertTo-Json -Depth 10))
        Refuses {Assert-DxUiSdkLibraryBuild $fixture x64 -BuildState $state}
    }finally{[IO.File]::WriteAllBytes($receipt,$bytes)}
}
Case 'dirty and changed commits cannot be relabeled as clean SDK qualification' {
    Put 'src/source.cpp' 'changed source'
    Refuses {Assert-DxUiSdkLibraryBuild $fixture x64 -BuildState $state -RequireClean}
    FreshRecord
    Refuses {Assert-DxUiSdkLibraryBuild $fixture x64 -BuildState $state -RequireClean}
    Git @('add','-A');Git @('commit','-qm','fixture source change')
    Refuses {Assert-DxUiSdkLibraryBuild $fixture x64 -BuildState $state}
    FreshRecord
}
Case 'failed or mutated builds revoke an earlier library record' {
    $before=Get-DxUiSdkLibraryInputState $fixture x64 -BuildState $state
    $changed=[ordered]@{buildIdentityVersion=2;resolvedBuildIdentity=$state.resolvedBuildIdentity;buildInputIdentity=('f'*64);toolchainIdentity=$state.toolchainIdentity}
    Refuses {Complete-DxUiSdkLibraryBuild $fixture x64 -Before $before -BuildState $changed}
    Require (-not(Test-Path $receipt)) 'A failed SDK build retained old provenance.'
    FreshRecord
    Remove-DxUiSdkLibraryReceipt $fixture x64
    Refuses {Assert-DxUiSdkLibraryBuild $fixture x64 -BuildState $state}
}
Case 'authenticated-run transfer restores only exact library inputs and revokes full/performance proof' {
    FreshRecord
    $performanceModule=& $sdkModule {$script:performance}
    $script:originalStateGetter=& $performanceModule {(Get-Command Get-PerformanceBuildState).ScriptBlock}
    & $performanceModule {
        param($State)
        $script:SdkFixtureState=$State
        Set-Item Function:script:Get-PerformanceBuildState {param($Root,$Platform,$Configuration) return $script:SdkFixtureState}
    } $state
    $script:transfer=Export-DxUiSdkLibraryInputs $fixture x64 -RunId 123 -RunAttempt 1
    Put '.build/x64/Release/DxUi.lib' 'old output must be replaced'
    Put '.build/reports/scoped-tests/x64-Release/build.json' 'sentinel Full receipt'
    Put '.build/reports/performance-build/x64-Release/build.json' 'sentinel performance receipt'
    Put '.build/reports/performance-build/x64-Release/incremental.json' 'sentinel intermediate receipt'
    $result=Import-DxUiSdkLibraryInputs $fixture x64 -Directory $transfer -RunId 123 -RunAttempt 1
    Require $result.hit 'Matching SDK transfer did not reuse the original library.'
    Assert-DxUiSdkLibraryBuild $fixture x64 -BuildState $state -RequireClean|Out-Null
    foreach($path in @('.build/reports/scoped-tests/x64-Release/build.json','.build/reports/performance-build/x64-Release/build.json','.build/reports/performance-build/x64-Release/incremental.json')){
        Require (-not(Test-Path (Join-Path $fixture $path))) "Library input transfer retained unrelated proof: $path"
    }
}
Case 'wrong run, attempt, incomplete payload, corrupt bytes and changed native inputs never reuse a transfer' {
    $arguments=@{Root=$fixture;Platform='x64';Directory=$transfer;RunId='123';RunAttempt=1}
    $changed=$arguments.Clone();$changed.RunId='124';Require (-not(Import-DxUiSdkLibraryInputs @changed).hit) 'Wrong run reused SDK inputs.'
    $changed=$arguments.Clone();$changed.RunAttempt=2;Require (-not(Import-DxUiSdkLibraryInputs @changed).hit) 'Wrong run attempt reused SDK inputs.'
    $file=Join-Path $transfer 'DxUi.lib';$bytes=[IO.File]::ReadAllBytes($file);$stamp=[IO.File]::GetLastWriteTimeUtc($file)
    $corrupt=[byte[]]$bytes.Clone();$corrupt[0]=$corrupt[0] -bxor 1
    [IO.File]::WriteAllBytes($file,$corrupt);[IO.File]::SetLastWriteTimeUtc($file,$stamp)
    try{Require (-not(Import-DxUiSdkLibraryInputs @arguments).hit) 'Corrupt same-size/timestamp SDK payload was reused.'}finally{[IO.File]::WriteAllBytes($file,$bytes)}
    $marker=Join-Path $transfer 'transfer.json';[IO.File]::Move($marker,$marker+'.held')
    try{Require (-not(Import-DxUiSdkLibraryInputs @arguments).hit) 'Incomplete transfer was reused.'}finally{[IO.File]::Move($marker+'.held',$marker)}
    $performanceModule=& $sdkModule {$script:performance}
    try{
        & $performanceModule {$script:SdkFixtureState.toolchainIdentity='d'*64}
        Require (-not(Import-DxUiSdkLibraryInputs @arguments).hit) 'Changed evaluated toolchain reused SDK inputs.'
    }finally{& $performanceModule {$script:SdkFixtureState.toolchainIdentity='c'*64}}
    $extra=Join-Path $transfer 'unexpected.dll';[IO.File]::WriteAllText($extra,'unexpected payload')
    try{Refuses {Import-DxUiSdkLibraryInputs @arguments}}finally{[IO.File]::Delete($extra)}
}
Case 'production route omits source build only for exact inputs and rebuilds missing or incompatible handoffs' {
    $script:buildCalls=0
    $builder={$script:buildCalls++;FreshRecord}
    $result=Invoke-DxUiSdkProductionLibrary $fixture x64 -Directory $transfer -RunId 123 -RunAttempt 1 -BuildFromSource $builder
    Require ($result.hit -and $script:buildCalls -eq 0) 'An exact library transfer invoked the source build.'
    $result=Invoke-DxUiSdkProductionLibrary $fixture x64 -RunId 123 -RunAttempt 1 -BuildFromSource $builder
    Require (-not $result.hit -and $script:buildCalls -eq 1) 'Missing artifact failed to invoke the governed source fallback.'
    $result=Invoke-DxUiSdkProductionLibrary $fixture x64 -Directory $transfer -RunId 123 -RunAttempt 2 -BuildFromSource $builder
    Require (-not $result.hit -and $script:buildCalls -eq 2) 'Another attempt bypassed the fresh-source fallback.'
    Refuses {Invoke-DxUiSdkProductionLibrary $fixture x64 -RunId 123 -RunAttempt 1 -BuildFromSource {throw 'seeded source build failure'}}
    Require (-not(Test-Path -LiteralPath $receipt)) 'Failed source fallback retained an older library record.'
}
Case 'SDK compatibility promises reject incompatible evaluated CRT, annotations, options and native profiles' {
    $profile=@{RuntimeLibrary='MultiThreadedDLL';LanguageStandard='stdcpplatest';WholeProgramOptimization='true';EnableASAN='false';
        DebugInformationFormat='ProgramDatabase';ExceptionHandling='Sync';PreprocessorDefinitions='NDEBUG;UNICODE;_UNICODE;';
        AdditionalOptions='/utf-8 /Zc:preprocessor';ForcedIncludeFiles='';UndefineAllPreprocessorDefinitions='false';UndefinePreprocessorDefinitions=''}
    & $sdkModule {param($Profile) Assert-DxUiSdkEvaluatedProfile @($Profile)} $profile
    $withoutOptional=$profile.Clone();$withoutOptional.Remove('UndefinePreprocessorDefinitions')
    & $sdkModule {param($Profile) Assert-DxUiSdkEvaluatedProfile @($Profile)} $withoutOptional
    foreach($row in @(@('RuntimeLibrary','MultiThreaded'),@('LanguageStandard','stdcpp20'),@('WholeProgramOptimization','false'),
        @('EnableASAN','true'),@('DebugInformationFormat','None'),@('PreprocessorDefinitions','NDEBUG;_DISABLE_VECTOR_ANNOTATION'),
        @('PreprocessorDefinitions','NDEBUG;_DISABLE_STRING_ANNOTATION=1'),@('PreprocessorDefinitions','_DEBUG'),
        @('PreprocessorDefinitions','NDEBUG;_DISABLE_STL_ANNOTATION'),
        @('AdditionalOptions','/MDd'),@('AdditionalOptions','/D_DISABLE_VECTOR_ANNOTATION'),@('ForcedIncludeFiles','override.h'),
        @('UndefineAllPreprocessorDefinitions','true'),@('UndefinePreprocessorDefinitions','NDEBUG'))){
        $changed=$profile.Clone();$changed[$row[0]]=$row[1]
        Refuses {& $sdkModule {param($Profile) Assert-DxUiSdkEvaluatedProfile @($Profile)} $changed}
    }
}
Case 'external source build descriptors bind exact producer tooling, library and original diagnostics without test verdicts' {
    FreshRecord
    $library=Assert-DxUiSdkLibraryBuild $fixture x64 -BuildState $state -RequireClean
    $tooling=@{repository='RedSalamanders/DxUi';commit=('a'*40);contentIdentity=('b'*64)}
    Put '.build/logs/source-build.log' 'synthetic diagnostic bytes; not a native build'
    $log=Join-Path $fixture '.build/logs/source-build.log'
    Complete-DxUiSdkSourceBuildRecord $fixture x64 -Tooling $tooling -LibraryRecord $library -Arguments @('fixture.sln','/t:DxUi:Rebuild') -LogPath $log|Out-Null
    $record=Assert-DxUiSdkSourceBuildRecord $fixture x64 -Tooling $tooling -LibraryRecord $library
    Require ($record.role -ceq 'PinnedSourceSdkBuildInputs' -and -not $record.Contains('passed')) 'Source input data supplied a test verdict.'
    $changed=$tooling.Clone();$changed.commit='c'*40
    Refuses {Assert-DxUiSdkSourceBuildRecord $fixture x64 -Tooling $changed -LibraryRecord $library}
    $changed=$tooling.Clone();$changed.contentIdentity='c'*64
    Refuses {Assert-DxUiSdkSourceBuildRecord $fixture x64 -Tooling $changed -LibraryRecord $library}
    $changed=$tooling.Clone();$changed.passed=$true
    Refuses {Assert-DxUiSdkSourceBuildRecord $fixture x64 -Tooling $changed -LibraryRecord $library}
    [IO.File]::AppendAllText($log,'changed')
    Refuses {Assert-DxUiSdkSourceBuildRecord $fixture x64 -Tooling $tooling -LibraryRecord $library}
    Remove-DxUiSdkLibraryReceipt $fixture x64
    Require (-not(Test-Path (Join-Path (Split-Path $receipt) 'source-producer.json'))) 'Failed/replaced library left a producer descriptor.'
}
Case 'foreign output roots cannot create SDK receipt folders' {
    $foreign=Join-Path $run 'not-a-repository';[void][IO.Directory]::CreateDirectory($foreign)
    Refuses {Remove-DxUiSdkLibraryReceipt $foreign x64}
    Require (-not(Test-Path (Join-Path $foreign '.build'))) 'SDK records initialized a foreign output root.'
}
Write-Host "SDK library inputs: $passed controls passed; synthetic metadata/files only; retained at $run"
if($script:originalStateGetter){
    $performanceModule=& $sdkModule {$script:performance}
    & $performanceModule {param($Original) Set-Item Function:script:Get-PerformanceBuildState $Original;Remove-Variable SdkFixtureState -Scope Script} $script:originalStateGetter
}
$global:LASTEXITCODE=0
