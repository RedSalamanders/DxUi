#Requires -Version 5.1
<#
.SYNOPSIS
Tests the portable consumer process boundary without building DxUi or opening windows.
.DESCRIPTION
Borrows an existing caller-owned fixture parent, creates one unique retained child, and
uses only captured no-window children. Never runs the old bare-command baseline.
Reports and child scripts remain in that child on success and failure. The caller owns cleanup.
.PARAMETER FixtureParent
Existing absolute non-reparse directory owned by the invoking test operation.
.PARAMETER MSBuildPath
Optional trusted MSBuild executable for actual evaluation-only identity coverage on Windows.
No C++ compilation, dependency restore or installation occurs.
.OUTPUTS
Console case results and bounded JSON observations beneath the unique fixture child.
Only public/summary.json and public/manifest.json are publication projections;
the remaining files are private raw evidence. HEAD plus component hashes does
not attest that an uncommitted workspace is a qualified commit.
Public projection v2 adds only closed setup phase/type/code diagnostics. Raw
exceptions remain in the private control-error.json. Unix SafeHandle denotes a
runtime wait object; short-lived child creation metadata can be unavailable.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $FixtureParent,
    [string] $MSBuildPath = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$isWindowsHost = [Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT
function Test-ControlAbsolutePath([string] $Path) {
    # IsPathFullyQualified is unavailable on .NET Framework/PowerShell 5.1.
    # IsPathRooted alone also accepts drive-relative and root-relative Windows paths.
    if (-not [IO.Path]::IsPathRooted($Path)) { return $false }
    if ($isWindowsHost -and $Path -notmatch '^(?:[A-Za-z]:[\\/]|\\\\[^\\/]+[\\/][^\\/]+(?:[\\/]|$))') { return $false }
    return $true
}
foreach ($name in @('GIT_DIR','GIT_WORK_TREE','GIT_INDEX_FILE','GIT_OBJECT_DIRECTORY','GIT_COMMON_DIR')) {
    if ([Environment]::GetEnvironmentVariable($name)) { throw "Fixture Git refuses inherited $name routing." }
}
if (-not (Test-ControlAbsolutePath $FixtureParent)) { throw 'FixtureParent must be an existing fully-qualified directory.' }
$parent = [IO.Path]::GetFullPath($FixtureParent).TrimEnd([IO.Path]::DirectorySeparatorChar)
if ($parent -eq [IO.Path]::GetPathRoot($parent).TrimEnd([IO.Path]::DirectorySeparatorChar) -or
    -not [IO.Directory]::Exists($parent)) { throw 'FixtureParent must be an existing non-root directory.' }
$ancestor = [IO.DirectoryInfo]::new($parent)
while ($null -ne $ancestor) {
    if (($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'FixtureParent cannot traverse a reparse point.' }
    $ancestor = $ancestor.Parent
}
$root = Join-Path $parent ('consumer-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
if (($root.Length + 65) -gt 240) { throw 'FixtureParent is too deep for the bounded child paths (240 characters).' }
if (Test-Path -LiteralPath $root) { throw 'Unique consumer fixture already exists.' }
[void][IO.Directory]::CreateDirectory($root)
if (([IO.File]::GetAttributes($root) -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Consumer fixture cannot be a reparse point.' }
$emptyTemplate = Join-Path $root 'empty-template'
[void][IO.Directory]::CreateDirectory($emptyTemplate)
function Get-ControlHostHashBinding($Command, $Export, $TopModule, [string] $Manifest, [StringComparison] $Comparison) {
    # Importing a native 5.1 manifest reparents the surfaced function metadata,
    # while ExportedCommands retains its nested script-module metadata. Bind the
    # actual callable, not equality of those legitimate module wrappers.
    $topReal=$TopModule -is [Management.Automation.PSModuleInfo]
    $topPath=$topReal -and $TopModule.Name -ceq 'Microsoft.PowerShell.Utility' -and
        [string]::Equals([IO.Path]::GetFullPath($TopModule.Path),$Manifest,$Comparison)
    $commandReal=$Command -is [Management.Automation.FunctionInfo] -or $Command -is [Management.Automation.CmdletInfo]
    $ownerTop=$commandReal -and $Command.Module -is [Management.Automation.PSModuleInfo] -and $topReal -and
        [object]::ReferenceEquals($Command.Module,$TopModule)
    $ownerPath=$ownerTop -and [string]::Equals([IO.Path]::GetFullPath($Command.Module.Path),$Manifest,$Comparison)
    $functionExport=$false;$scriptSame=$false;$definitionSame=$false;$nestedOwner=$false
    $cmdletExport=$false;$implementationSame=$false
    if ($Command -is [Management.Automation.FunctionInfo]) {
        $functionExport=$Export -is [Management.Automation.FunctionInfo]
        $scriptSame=$functionExport -and $null -ne $Command.ScriptBlock -and $null -ne $Export.ScriptBlock -and
            [object]::ReferenceEquals($Command.ScriptBlock,$Export.ScriptBlock)
        $definition=Join-Path ([IO.Path]::GetDirectoryName($Manifest)) 'Microsoft.PowerShell.Utility.psm1'
        $definitionSame=$scriptSame -and $Command.ScriptBlock.File -and $Export.ScriptBlock.File -and
            [string]::Equals([IO.Path]::GetFullPath($Command.ScriptBlock.File),$definition,$Comparison) -and
            [string]::Equals([IO.Path]::GetFullPath($Export.ScriptBlock.File),$definition,$Comparison)
        $nestedOwner=$functionExport -and $Export.Module -is [Management.Automation.PSModuleInfo] -and
            [string]::Equals([IO.Path]::GetFullPath($Export.Module.Path),$definition,$Comparison) -and
            $null -ne $Command.ScriptBlock -and [object]::ReferenceEquals($Command.ScriptBlock.Module,$Export.Module)
    } elseif ($Command -is [Management.Automation.CmdletInfo]) {
        $cmdletExport=$Export -is [Management.Automation.CmdletInfo]
        $implementationSame=$cmdletExport -and $null -ne $Command.ImplementingType -and $null -ne $Export.ImplementingType -and
            [object]::ReferenceEquals($Command.ImplementingType,$Export.ImplementingType)
    }
    return [pscustomobject][ordered]@{top_module_real=$topReal;top_manifest_path_equal=$topPath;command_real=$commandReal;
        command_owner_is_top=$ownerTop;command_owner_path_equal=$ownerPath;function_export_real=$functionExport;
        function_scriptblock_reference_equal=$scriptSame;function_definition_file_equal=[bool]$definitionSame;function_nested_owner_equal=[bool]$nestedOwner;
        cmdlet_export_real=$cmdletExport;cmdlet_implementing_type_reference_equal=$implementationSame;
        owner_verified=($topPath -and $ownerPath -and (($functionExport -and $scriptSame -and $definitionSame -and $nestedOwner) -or ($cmdletExport -and $implementationSame)))}
}
# This is test-host bootstrap, not a producer hashing replacement. A cross-host
# parent can leave a PowerShell 7 Utility manifest loaded in Windows PowerShell
# 5.1. Bind the running host's own standard module once, before producer imports
# and before any process interception. Global means this isolated child only.
$utilityManifest=[IO.Path]::GetFullPath((Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1'))
if (-not [IO.File]::Exists($utilityManifest)) {throw 'Consumer control host bootstrap: native Utility manifest is unavailable.'}
$utilityModules=@(Import-Module $utilityManifest -Force -Global -PassThru -ErrorAction Stop)
if ($utilityModules.Count -ne 1 -or $utilityModules[0] -isnot [Management.Automation.PSModuleInfo]) {
    [IO.File]::WriteAllText((Join-Path $root 'host-bootstrap-binding.json'),('{"schema":"dxui.consumer-control-host-binding.v1","phase":"module-import","module_count":'+$utilityModules.Count+'}'),[Text.UTF8Encoding]::new($false))
    throw 'Consumer control host bootstrap: native Utility module is ambiguous.'
}
$hostUtility=$utilityModules[0]
$pathComparison=if($isWindowsHost){[StringComparison]::OrdinalIgnoreCase}else{[StringComparison]::Ordinal}
$hostHashCommand=$null
$hostLookupError=''
try {
    $hostHashCommand=Get-Command Get-FileHash -CommandType ([Management.Automation.CommandTypes]::Function -bor [Management.Automation.CommandTypes]::Cmdlet) -ErrorAction Stop
} catch [Management.Automation.CommandNotFoundException] {
    $hostLookupError=$_.FullyQualifiedErrorId
}
$hostHashExport=$hostUtility.ExportedCommands['Get-FileHash']
$hostBinding=Get-ControlHostHashBinding $hostHashCommand $hostHashExport $hostUtility $utilityManifest $pathComparison
$bindingPath=Join-Path $root 'host-bootstrap-binding.json'
[IO.File]::WriteAllText($bindingPath,([pscustomobject]@{schema='dxui.consumer-control-host-binding.v1';phase='callable';manifest_path=$utilityManifest;
    command_owner_path=$(if($hostHashCommand -and $hostHashCommand.Module){$hostHashCommand.Module.Path}else{''});
    export_owner_path=$(if($hostHashExport -and $hostHashExport.Module){$hostHashExport.Module.Path}else{''});
    command_definition_file=$(if($hostHashCommand -is [Management.Automation.FunctionInfo]){$hostHashCommand.ScriptBlock.File}else{''});
    export_definition_file=$(if($hostHashExport -is [Management.Automation.FunctionInfo]){$hostHashExport.ScriptBlock.File}else{''});
    lookup_error_id=$hostLookupError;clauses=$hostBinding}|ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
if (-not $hostBinding.owner_verified) {throw 'Consumer control host bootstrap: Get-FileHash has no verified native callable authority.'}
$hostBootstrap=[pscustomobject][ordered]@{schema='dxui.consumer-control-host-bootstrap.v1';host_version=$PSVersionTable.PSVersion.ToString();
    manifest_path=$utilityManifest;module_path=$hostUtility.Path;module_version=$hostUtility.Version.ToString();
    manifest_sha256=(& $hostHashCommand -LiteralPath $utilityManifest -Algorithm SHA256).Hash;
    hash_command_kind=$hostHashCommand.CommandType.ToString();hash_command_module_path=$hostHashCommand.Module.Path;owner_verified=$hostBinding.owner_verified;
    callable_binding_sha256=(& $hostHashCommand -LiteralPath $bindingPath -Algorithm SHA256).Hash}
[IO.File]::WriteAllText((Join-Path $root 'host-bootstrap.json'),($hostBootstrap|ConvertTo-Json -Depth 4),[Text.UTF8Encoding]::new($false))
$support = Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force -PassThru
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$modulePath = Join-Path $root 'ConsumerBuild.psm1'
Copy-Item -LiteralPath (Join-Path $repository 'Tools/ConsumerBuild.psm1') -Destination $modulePath
$module = Import-Module $modulePath -PassThru -Scope Local -ErrorAction Stop
$observations = [Collections.Generic.List[object]]::new()
[void]$observations.Add([pscustomobject]@{Kind='HostBootstrap';Values=$hostBootstrap})
$oldEncoding = [Console]::OutputEncoding
$oldExitVariable = Get-Variable LASTEXITCODE -Scope Global -ErrorAction SilentlyContinue
$oldExitCode = if ($null -ne $oldExitVariable) { $oldExitVariable.Value } else { $null }
$hostExecutable = [Diagnostics.Process]::GetCurrentProcess().MainModule.FileName
if ([IO.Path]::GetFileName($hostExecutable).ToLowerInvariant() -notin @('pwsh','pwsh.exe','powershell','powershell.exe')) {
    throw 'ConsumerBuild controls require a PowerShell CLI host; GUI hosts are not child fixtures.'
}
$git = Get-Command git -CommandType Application -ErrorAction Stop | Select-Object -First 1
$primaryFailure = $false
$primaryErrorHash = ''
$primaryErrorRecord = $null
$script:SetupPhase = 'initializing'
$script:SetupErrorType = 'NONE'
$script:SetupErrorCode = 0
$script:CaptureOrdinal = 0
$script:ObservedSourceHead = ''

function Get-ControlTextHash([AllowEmptyString()][string] $Text) {
    # Test-only portable hashing: TestSupport's unrelated fast hash helper needs
    # PowerShell 7/.NET, while this entrypoint also qualifies Windows PowerShell 5.1.
    $hash = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($hash.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text)))).Replace('-','').ToLowerInvariant() }
    finally { $hash.Dispose() }
}

function New-ControlPublicProjection([object[]] $Cases, [bool] $PrimaryFailure, [string] $PrimaryErrorHash, [string] $ObservationHash) {
    $failed = @($Cases | Where-Object { $_.Error }).Count
    $expected = 13 + [int]$isWindowsHost * 2 + [int][bool]$MSBuildPath + 1
    $rows = @(
        for ($index=0; $index -lt $Cases.Count; $index++) {
            $errorText = [string]$Cases[$index].Error
            [pscustomobject][ordered]@{ordinal=$index+1;outcome=$(if($errorText){'FAILED'}else{'PASSED'});
                error_category=$(if($errorText){'CASE_FAILURE'}else{'NONE'});
                error_sha256=$(if($errorText){Get-ControlTextHash $errorText}else{''})}
        }
    )
    $consoleRows=@($observations|Where-Object {$_.Kind -ceq 'OwnedConsoleFacts'})
    $console=$null
    if($consoleRows.Count -eq 1) {
        $facts=$consoleRows[0].Values
        $console=[pscustomobject][ordered]@{window_present=[bool]$facts.window_present;window_visible=[bool]$facts.window_visible;stdout_type=[int]$facts.stdout_type;stderr_type=[int]$facts.stderr_type}
    }
    return [pscustomobject][ordered]@{
        schema='dxui.consumer-process-controls-public.v2';run_token=([IO.Path]::GetFileName($root).Substring(9));
        source_commit=$script:ObservedSourceHead;test_sha256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash;
        module_sha256=(Get-FileHash -LiteralPath $modulePath -Algorithm SHA256).Hash;
        host_version=$PSVersionTable.PSVersion.ToString();platform_family=$(if($isWindowsHost){'Windows'}else{'Unix'});
        host_utility=[pscustomobject][ordered]@{version=$hostBootstrap.module_version;manifest_sha256=$hostBootstrap.manifest_sha256;hash_command_kind=$hostBootstrap.hash_command_kind;owner_verified=$hostBootstrap.owner_verified};
        status=$(if($PrimaryFailure -or $failed -or $Cases.Count -ne $expected){'FAILED'}else{'PASSED'});
        counts=[pscustomobject][ordered]@{passed=$Cases.Count-$failed;failed=$failed;total=$Cases.Count;expected=$expected};
        error_category=$(if($failed){'CASE_FAILURE'}elseif($PrimaryFailure){'CONTROL_FAILURE'}elseif($Cases.Count -ne $expected){'INCOMPLETE'}else{'NONE'});
        error_sha256=$PrimaryErrorHash;setup=[pscustomobject][ordered]@{phase=$script:SetupPhase;type=$script:SetupErrorType;code=$script:SetupErrorCode};
        observations_sha256=$ObservationHash;native_starts=(& $module {$script:TestStarts.Count});owned_console=$console;cases=$rows
    }
}

function Assert-ControlPublicProjection($Projection) {
    # A deliberately closed publication shape, not a sanitizer for arbitrary data.
    $fields=@('schema','run_token','source_commit','test_sha256','module_sha256','host_version','platform_family','host_utility','status','counts','error_category','error_sha256','setup','observations_sha256','native_starts','owned_console','cases')
    if ($null -eq $Projection -or $Projection -isnot [pscustomobject] -or
        (@($Projection.PSObject.Properties.Name | Sort-Object) -join '|') -cne (@($fields | Sort-Object) -join '|')) { throw 'Public projection has an unapproved field set.' }
    if ($Projection.schema -cne 'dxui.consumer-process-controls-public.v2' -or $Projection.run_token -cnotmatch '^[0-9a-f]{8}$') { throw 'Public projection schema or token is invalid.' }
    if ($Projection.source_commit -and $Projection.source_commit -cnotmatch '^[0-9a-f]{40}$') { throw 'Public projection source is invalid.' }
    foreach($name in @('test_sha256','module_sha256','observations_sha256')) {
        if ($Projection.$name -notmatch '^[0-9a-fA-F]{64}$') { throw 'Public projection hash is invalid.' }
    }
    if ($Projection.error_sha256 -and $Projection.error_sha256 -notmatch '^[0-9a-fA-F]{64}$') { throw 'Public projection error hash is invalid.' }
    if ($Projection.host_version -notmatch '^\d+\.\d+(?:\.\d+){0,2}$' -or $Projection.platform_family -cnotin @('Windows','Unix') -or
        $Projection.status -cnotin @('PASSED','FAILED') -or $Projection.error_category -cnotin @('NONE','CASE_FAILURE','CONTROL_FAILURE','INCOMPLETE')) { throw 'Public projection scalar is invalid.' }
    $utility=$Projection.host_utility
    if ((@($utility.PSObject.Properties.Name|Sort-Object) -join '|') -cne 'hash_command_kind|manifest_sha256|owner_verified|version' -or
        $utility.version -notmatch '^\d+\.\d+(?:\.\d+){0,2}$' -or $utility.manifest_sha256 -notmatch '^[0-9a-fA-F]{64}$' -or
        $utility.hash_command_kind -cnotin @('Function','Cmdlet') -or $utility.owner_verified -isnot [bool] -or -not $utility.owner_verified) {throw 'Public projection host-module fields are invalid.'}
    if (($Projection.native_starts -isnot [int] -and $Projection.native_starts -isnot [long]) -or $Projection.native_starts -lt 0) {throw 'Public projection launch count is invalid.'}
    $setup=$Projection.setup
    if ((@($setup.PSObject.Properties.Name|Sort-Object) -join '|') -cne 'code|phase|type' -or
        $setup.phase -cnotin @('initializing','source-head.pre-start','source-head.post-start','source-head.ownership-recorded','source-head.metadata','source-head.returned','cases','completed') -or
        $setup.type -cnotin @('NONE','InvalidOperationException','Win32Exception','IOException','PlatformNotSupportedException','CommandNotFoundException','RuntimeException','OTHER') -or
        ($setup.code -isnot [int] -and $setup.code -isnot [long]) -or $setup.code -lt [int]::MinValue -or $setup.code -gt [int]::MaxValue) {throw 'Public projection setup fields are invalid.'}
    if ($null -ne $Projection.owned_console) {
        $console=$Projection.owned_console
        if ((@($console.PSObject.Properties.Name|Sort-Object) -join '|') -cne 'stderr_type|stdout_type|window_present|window_visible' -or
            $console.window_present -isnot [bool] -or $console.window_visible -isnot [bool] -or
            ($console.stdout_type -isnot [int] -and $console.stdout_type -isnot [long]) -or
            ($console.stderr_type -isnot [int] -and $console.stderr_type -isnot [long])) {throw 'Public projection owned-console fields are invalid.'}
    }
    if ((@($Projection.counts.PSObject.Properties.Name | Sort-Object) -join '|') -cne 'expected|failed|passed|total') { throw 'Public projection count fields are invalid.' }
    foreach($name in @('expected','failed','passed','total')) {
        if (($Projection.counts.$name -isnot [int] -and $Projection.counts.$name -isnot [long]) -or $Projection.counts.$name -lt 0) { throw 'Public projection count is invalid.' }
    }
    $rows=@($Projection.cases)
    if ($rows.Count -ne $Projection.counts.total -or $Projection.counts.passed+$Projection.counts.failed -ne $rows.Count) { throw 'Public projection counts disagree.' }
    for($index=0; $index -lt $rows.Count; $index++) {
        $row=$rows[$index]
        if ((@($row.PSObject.Properties.Name | Sort-Object) -join '|') -cne 'error_category|error_sha256|ordinal|outcome' -or $row.ordinal -ne $index+1 -or
            $row.outcome -cnotin @('PASSED','FAILED') -or $row.error_category -cnotin @('NONE','CASE_FAILURE')) { throw 'Public projection case fields are invalid.' }
        if (($row.outcome -ceq 'FAILED' -and ($row.error_category -cne 'CASE_FAILURE' -or $row.error_sha256 -notmatch '^[0-9a-fA-F]{64}$')) -or
            ($row.outcome -ceq 'PASSED' -and ($row.error_category -cne 'NONE' -or $row.error_sha256))) { throw 'Public projection case error is invalid.' }
    }
    if (@($rows | Where-Object {$_.outcome -ceq 'FAILED'}).Count -ne $Projection.counts.failed -or
        ($Projection.status -ceq 'PASSED' -and ($Projection.counts.failed -ne 0 -or $Projection.counts.total -ne $Projection.counts.expected -or $Projection.error_category -cne 'NONE'))) { throw 'Public projection verdict is inconsistent.' }
}

function Assert-ControlPublicMembers([string] $Directory) {
    # Publication has exactly two ordinary members; never include adjacent raw evidence.
    $members=@(Get-ChildItem -LiteralPath $Directory -Force)
    if ((@($members.Name|Sort-Object) -join '|') -cne 'manifest.json|summary.json' -or
        @($members|Where-Object {$_.PSIsContainer -or ($_.Attributes -band [IO.FileAttributes]::ReparsePoint)}).Count) {throw 'Public projection membership is invalid.'}
}

function Invoke-Capture([string] $Executable, [string[]] $Values, [switch] $Stream, [string] $Directory = $root) {
    $result = & $module {
        param($executable, $values, $stream, $directory)
        Invoke-DxUiConsumerNativeQuery -FilePath $executable -Arguments $values -WorkingDirectory $directory -Stream:$stream
    } $Executable $Values $Stream.IsPresent $Directory
    if (-not $Stream) {
        $script:CaptureOrdinal++
        $name = 'query-' + $script:CaptureOrdinal
        [IO.File]::WriteAllText((Join-Path $root ($name+'.stdout.txt')), $result.StandardOutput, [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText((Join-Path $root ($name+'.stderr.txt')), $result.StandardError, [Text.UTF8Encoding]::new($false))
        [void]$observations.Add([pscustomobject]@{Kind='Capture';Name=$name;File=$Executable;Arguments=@($Values);Directory=$Directory;Exit=$result.ExitCode})
    }
    return $result
}

function Assert-IdentityOracle($Actual, [string] $Head, $Properties, [string] $BuildTool, [bool] $Disabled) {
    # Independent frozen API4 field/order/hash oracle, not another production builder.
    $hostFolder = 'Host' + $Properties.PreferredToolArchitecture
    $sdk = $Properties.WindowsTargetPlatformVersion.TrimEnd('\','/')
    $expected = [ordered]@{
        commit=$Head;apiRevision=4;platform='x64';toolset=$Properties.PlatformToolset;
        vcToolsVersion=$Properties.VCToolsVersion;windowsSdkVersion=$sdk;runtimeFamily='MD';
        preferredToolArchitecture=$Properties.PreferredToolArchitecture;disableStlAnnotations=$Disabled;
        releaseOptimizationPolicy='Gy-REF-GL-LTCG-full-v1';
        msbuildSha256=(Get-FileHash $BuildTool -Algorithm SHA256).Hash;
        compilerSha256=(Get-FileHash (Join-Path $Properties.VCToolsInstallDir "bin/$hostFolder/x64/cl.exe") -Algorithm SHA256).Hash;
        linkerSha256=(Get-FileHash (Join-Path $Properties.VCToolsInstallDir "bin/$hostFolder/x64/link.exe") -Algorithm SHA256).Hash;
        windowsHeaderSha256=(Get-FileHash (Join-Path $Properties.WindowsSdkDir "Include/$sdk/um/Windows.h") -Algorithm SHA256).Hash;
        kernel32Sha256=(Get-FileHash (Join-Path $Properties.WindowsSdkDir "Lib/$sdk/um/x64/kernel32.lib") -Algorithm SHA256).Hash;
        ucrtSha256=(Get-FileHash (Join-Path $Properties.WindowsSdkDir "Lib/$sdk/ucrt/x64/ucrt.lib") -Algorithm SHA256).Hash
    }
    $json = $expected | ConvertTo-Json -Compress
    $hash = [Security.Cryptography.SHA256]::Create()
    try { $digest = ([BitConverter]::ToString($hash.ComputeHash([Text.Encoding]::UTF8.GetBytes($json)))).Replace('-','').ToLowerInvariant() }
    finally { $hash.Dispose() }
    Assert-Equal $json ($Actual.Identity | ConvertTo-Json -Compress) 'exact API4 fields, values and serialization order'
    Assert-Equal $digest $Actual.Fingerprint 'unchanged API4 fingerprint algorithm'
}

try {
    [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
    & $module {
        param($FixtureRoot,$WindowsHost)
        $script:NeverStarted = Join-Path $FixtureRoot 'never-started.exe'
        $script:FixtureWindows = $WindowsHost
        $script:StartStage = 'pre-start'
        $script:TestStarts = [Collections.Generic.List[object]]::new()
        $script:TestLines = [Collections.Generic.List[object]]::new()
        $script:OriginalStart = (Get-Command Start-DxUiConsumerQueryProcess -CommandType Function).ScriptBlock
        $script:OriginalLine = (Get-Command Write-DxUiConsumerProcessLine -CommandType Function).ScriptBlock
        $script:LiveAck = ''
        $script:FailLine = ''
        # PowerShell's property binder can turn getter exceptions into null,
        # including CLR getters. Invoke these exact owned-object getters as
        # methods so the underlying exception survives for strict classification.
        # https://github.com/PowerShell/PowerShell/blob/v7.6.10/src/System.Management.Automation/engine/runtime/Binding/Binders.cs#L5027-L5040
        # https://learn.microsoft.com/dotnet/api/system.reflection.methodbase.invoke
        $script:StartTimeGetter=[Diagnostics.Process].GetProperty('StartTime').GetGetMethod()
        $script:SafeHandleGetter=[Diagnostics.Process].GetProperty('SafeHandle').GetGetMethod()
        $script:HasExitedGetter=[Diagnostics.Process].GetProperty('HasExited').GetGetMethod()
        $script:ReadProcessStartTime={param($Process) $script:StartTimeGetter.Invoke($Process,[object[]]@())}
        $script:ReadProcessSafeHandle={
            param($Process)
            $handle=$script:SafeHandleGetter.Invoke($Process,[object[]]@())
            if($handle -isnot [Runtime.InteropServices.SafeHandle]){throw [IO.InvalidDataException]::new('Owned child handle getter returned no SafeHandle.')}
            $handle
        }
        $script:ReadProcessHasExited={param($Process) $script:HasExitedGetter.Invoke($Process,[object[]]@())}
        $script:ReadStartMetadata = {
            param($Process,$Record)
            # Unix SafeHandle is supported, but represents a runtime wait object,
            # not a native Windows process-instance lease. Never reopen a PID.
            $Record.Handle=& $script:ReadProcessSafeHandle $Process
            $Record.HandleKind=if($script:FixtureWindows){'windows-process'}else{'unix-wait'}
            $creation=$null
            $unavailable=$false
            try {
                $creation=& $script:ReadProcessStartTime $Process
            } catch {
                if(Test-DxUiConsumerFatalException $_.Exception){throw}
                $cause=$_.Exception.GetBaseException()
                if($script:FixtureWindows -or ($cause -isnot [InvalidOperationException] -and $cause -isnot [ComponentModel.Win32Exception])){throw}
                $exited=& $script:ReadProcessHasExited $Process
                if($exited -isnot [bool] -or -not $exited){throw}
                $unavailable=$true
                $Record.StartTimeState='exited-before-observation'
            }
            if(-not $unavailable) {
                if($creation -isnot [DateTime] -or $creation.Ticks -le 0){throw [IO.InvalidDataException]::new('Owned child creation getter returned no positive DateTime.')}
                $Record.StartedTicks=$creation.ToUniversalTime().Ticks
                $Record.StartTimeState='observed'
            }
            if($null -eq $Record.Handle -or $Record.Handle.IsInvalid -or $Record.Handle.IsClosed -or
                ($script:FixtureWindows -and ($null -eq $Record.StartedTicks -or $Record.StartedTicks -le 0))) {
                throw [IO.InvalidDataException]::new('Started test child lacks its required owned handle/creation evidence.')
            }
        }
        Set-Item Function:script:Start-DxUiConsumerQueryProcess {
            param([Diagnostics.ProcessStartInfo] $StartInfo)
            # This assertion runs BEFORE the original factory can start a child.
            if ($StartInfo.UseShellExecute -or -not $StartInfo.CreateNoWindow -or
                -not $StartInfo.RedirectStandardOutput -or -not $StartInfo.RedirectStandardError) { throw 'Unsafe test child refused before start.' }
            $script:StartStage='pre-start'
            $process = & $script:OriginalStart -StartInfo $StartInfo
            $script:StartStage='post-start'
            try {
                # Retain the exact object immediately, before fallible procfs or
                # handle metadata. A recorded start is distinct from an attempt.
                $record=[pscustomobject]@{
                Pid = $process.Id; FileName = $StartInfo.FileName; Arguments = $StartInfo.Arguments
                WorkingDirectory = $StartInfo.WorkingDirectory; NoWindow = $StartInfo.CreateNoWindow
                RedirectOutput = $StartInfo.RedirectStandardOutput; RedirectError = $StartInfo.RedirectStandardError
                StartedTicks=$null; StartTimeState='pending'; Process=$process; Handle=$null; HandleKind='pending'; ObservationDisposed=$false
                }
                [void]$script:TestStarts.Add($record)
                $script:StartStage='ownership-recorded'
                $script:StartStage='metadata'
                & $script:ReadStartMetadata $process $record
                $script:StartStage='returned'
            } catch {
                $primary=$_
                try {
                    $exited=& $script:ReadProcessHasExited $process
                    if($exited -isnot [bool]){throw [IO.InvalidDataException]::new('Owned child exit getter returned no boolean.')}
                    if(-not $exited){$process.Kill();$process.WaitForExit()}
                } catch {
                    if(Test-DxUiConsumerFatalException $_.Exception){throw}
                    Write-Warning 'Test child observation cleanup also failed; preserving its primary error.' -WarningAction Continue
                } finally {
                    try {
                        $process.Dispose()
                        if($null -ne (Get-Variable record -Scope Local -ErrorAction SilentlyContinue)){$record.ObservationDisposed=$true}
                    } catch {
                        if(Test-DxUiConsumerFatalException $_.Exception){throw}
                        Write-Warning 'Test child observation disposal also failed; preserving its primary error.' -WarningAction Continue
                    }
                }
                throw $primary
            }
            return $process
        }
        Set-Item Function:script:Write-DxUiConsumerProcessLine {
            param([string] $Line, [bool] $IsError)
            [void]$script:TestLines.Add([pscustomobject]@{ Line = $Line; IsError = $IsError })
            if ($script:LiveAck -and $Line -ceq 'live-ready') {
                $exited=& $script:ReadProcessHasExited $script:TestStarts[-1].Process
                if ($exited -isnot [bool] -or $exited) { throw 'Live delivery happened after child exit or without verified exit metadata.' }
                [IO.File]::WriteAllText($script:LiveAck, 'observed before child exit')
            }
            if ($script:FailLine -and $Line -ceq $script:FailLine) { throw [IO.IOException]::new('Synthetic primary callback failure.') }
            & $script:OriginalLine -Line $Line -IsError $IsError
        }
    } $root $isWindowsHost

    $script:SetupPhase='source-head.pre-start'
    $sourceHead = Invoke-Capture $git.Source @('-C',$repository,'rev-parse','HEAD')
    if ($sourceHead.ExitCode -ne 0 -or $sourceHead.StandardOutput.Trim() -cnotmatch '^[0-9a-f]{40}$') { throw 'Cannot bind control source HEAD.' }
    $script:ObservedSourceHead = $sourceHead.StandardOutput.Trim()
    $script:SetupPhase='cases'

    Invoke-TestCase 'standalone module preserves the sole four-parameter public API' {
        Assert-Equal 'Get-DxUiConsumerBuildIdentity' (($module.ExportedFunctions.Keys | Sort-Object) -join ',') 'only the supported export'
        $public = $module.ExportedFunctions['Get-DxUiConsumerBuildIdentity']
        foreach ($name in @('DxUiRoot', 'MSBuildPath', 'Platform', 'DisableStlAnnotations')) {
            Assert-True $public.Parameters.ContainsKey($name) "public parameter $name"
        }
        Assert-Equal 4 @($public.Parameters.Keys | Where-Object { $_ -notin @('Verbose','Debug','ErrorAction','WarningAction','InformationAction','ProgressAction','ErrorVariable','WarningVariable','InformationVariable','OutVariable','OutBuffer','PipelineVariable') }).Count 'no added public parameter'
    }

    Invoke-TestCase 'the real factory refuses uncaptured flags before launch' {
        & $module {
            $start = [Diagnostics.ProcessStartInfo]::new()
            $start.FileName = $script:NeverStarted
            foreach ($flag in @('UseShellExecute', 'CreateNoWindow', 'RedirectStandardOutput', 'RedirectStandardError')) {
                $start.UseShellExecute = $false; $start.CreateNoWindow = $true
                $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
                $start.$flag = $flag -eq 'UseShellExecute'
                $message = ''
                try { & $script:OriginalStart -StartInfo $start } catch { $message = $_.Exception.Message }
                if (-not $message.Contains('captured no-window process flags')) { throw "Missing intrinsic $flag refusal." }
            }
            $savedStart=$script:OriginalStart; $savedMetadata=$script:ReadStartMetadata
            $savedStarts=$script:TestStarts; $savedWindows=$script:FixtureWindows; $savedStage=$script:StartStage
            $savedTime=$script:ReadProcessStartTime; $savedHandle=$script:ReadProcessSafeHandle; $savedExited=$script:ReadProcessHasExited
            try {
                $start.UseShellExecute=$false; $start.CreateNoWindow=$true
                $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
                # Deterministic metadata/ownership checks, not real starts and
                # never included in the recorded native-start observations.
                $script:FixtureWindows=$false
                $script:MetadataHandle=[pscustomobject]@{IsInvalid=$false;IsClosed=$false}
                $script:MetadataTime=[DateTime]::UtcNow; $script:MetadataFailure=$null; $script:MetadataExited=$true
                $script:ReadProcessStartTime={param($Process) if($null -ne $script:MetadataFailure){throw $script:MetadataFailure}; $script:MetadataTime}
                $script:ReadProcessSafeHandle={param($Process) $script:MetadataHandle}
                $script:ReadProcessHasExited={param($Process) $script:MetadataExited}
                $checkMetadata={
                    param([bool]$Accept,[bool]$Unavailable,[Type]$FailureType)
                    $row=[pscustomobject]@{Handle=$null;HandleKind='pending';StartedTicks=$null;StartTimeState='pending'}
                    $failure=$null
                    try {& $savedMetadata ([pscustomobject]@{}) $row} catch {$failure=$_.Exception.GetBaseException()}
                    if($Accept) {
                        if($null -ne $failure){throw $failure}
                        if($Unavailable) {
                            if($row.StartTimeState -cne 'exited-before-observation' -or $null -ne $row.StartedTicks -or $row.HandleKind -cne 'unix-wait'){throw 'Unix unavailable creation facts were fabricated.'}
                        } elseif($row.StartTimeState -cne 'observed' -or $row.StartedTicks -le 0){throw 'Positive creation metadata was lost.'}
                    } elseif($null -eq $failure -or -not $FailureType.IsInstanceOfType($failure)){throw 'Metadata refusal lost its exact underlying cause.'}
                }
                & $checkMetadata $true $false $null
                $script:FixtureWindows=$true
                & $checkMetadata $true $false $null
                $script:FixtureWindows=$false
                foreach($badTime in @($null,'not a DateTime',[DateTime]::MinValue)) {
                    $script:MetadataTime=$badTime
                    & $checkMetadata $false $false ([IO.InvalidDataException])
                }
                $script:MetadataTime=[DateTime]::UtcNow
                foreach($known in @([InvalidOperationException]::new('Synthetic exited creation.'),[ComponentModel.Win32Exception]::new(2))) {
                    $script:MetadataFailure=[Reflection.TargetInvocationException]::new($known)
                    $script:FixtureWindows=$false; $script:MetadataExited=$true
                    & $checkMetadata $true $true $null
                    $script:MetadataExited=$false
                    & $checkMetadata $false $false ($known.GetType())
                    $script:MetadataExited=$null
                    & $checkMetadata $false $false ($known.GetType())
                    $script:MetadataExited=$true; $script:FixtureWindows=$true
                    & $checkMetadata $false $false ($known.GetType())
                }
                $script:FixtureWindows=$false
                foreach($unexpected in @([IO.IOException]::new('Synthetic unrelated creation.'),[OutOfMemoryException]::new('Synthetic fatal only; no allocation failure.'))) {
                    foreach($failure in @($unexpected,[Reflection.TargetInvocationException]::new($unexpected))) {
                        $script:MetadataFailure=$failure
                        & $checkMetadata $false $false ($unexpected.GetType())
                    }
                }
                $script:MetadataFailure=$null; $script:MetadataHandle=$null
                & $checkMetadata $false $false ([IO.InvalidDataException])
                foreach($badHandle in @([pscustomobject]@{IsInvalid=$true;IsClosed=$false},[pscustomobject]@{IsInvalid=$false;IsClosed=$true})) {
                    $script:MetadataHandle=$badHandle
                    & $checkMetadata $false $false ([IO.InvalidDataException])
                }
                foreach($alreadyExited in @($false,$true)) {
                    $script:TestStarts=[Collections.Generic.List[object]]::new()
                    $script:FakeStarted=[pscustomobject]@{Id=4242;HasExited=$alreadyExited;Killed=$false;Waited=$false;Disposed=$false}
                    $script:FakeStarted|Add-Member ScriptMethod Kill {$this.Killed=$true;$this.HasExited=$true}
                    $script:FakeStarted|Add-Member ScriptMethod WaitForExit {$this.Waited=$true}
                    $script:FakeStarted|Add-Member ScriptMethod Dispose {$this.Disposed=$true}
                    $script:OriginalStart={param($StartInfo) $script:FakeStarted}
                    $script:ReadStartMetadata={param($Process,$Record) throw [IO.IOException]::new('Synthetic primary post-start metadata failure.')}
                    $script:ReadProcessHasExited={param($Process) $Process.HasExited}
                    $failure=$null
                    try {Start-DxUiConsumerQueryProcess -StartInfo $start|Out-Null} catch {$failure=$_.Exception}
                    if($null -eq $failure -or -not $failure.Message.Contains('Synthetic primary post-start metadata failure.')) {throw 'Post-start primary metadata failure was lost.'}
                    if($script:TestStarts.Count -ne 1 -or -not [object]::ReferenceEquals($script:TestStarts[0].Process,$script:FakeStarted) -or
                        -not $script:TestStarts[0].ObservationDisposed -or -not $script:FakeStarted.Disposed) {throw 'The exact successfully started object was not retained and disposed.'}
                    if($script:FakeStarted.Killed -ne (-not $alreadyExited) -or $script:FakeStarted.Waited -ne (-not $alreadyExited)) {throw 'Observation cleanup did not distinguish the live owned child from an exited one.'}
                }
            } finally {
                $script:OriginalStart=$savedStart; $script:ReadStartMetadata=$savedMetadata
                $script:ReadProcessStartTime=$savedTime; $script:ReadProcessSafeHandle=$savedHandle; $script:ReadProcessHasExited=$savedExited
                $script:TestStarts=$savedStarts; $script:FixtureWindows=$savedWindows; $script:StartStage=$savedStage
                Remove-Variable FakeStarted -Scope Script -ErrorAction SilentlyContinue
                Remove-Variable MetadataHandle,MetadataTime,MetadataFailure,MetadataExited -Scope Script -ErrorAction SilentlyContinue
            }
        }
        [void]$observations.Add([pscustomobject]@{Kind='StartMetadataCounterfactuals';Mode='Pure reader seams; not native starts';ExitedTimeUnavailable=$true;NullRejected=$true;LiveAndWindowsRefused=$true;FatalPropagated=$true;ExactOwnerCleanup=$true})
    }

    Invoke-TestCase 'native Git preserves literal empty, quoted, Unicode and metacharacter arguments' {
        # Git is a native argv oracle. PowerShell 5.1's own -File CLI cannot
        # represent every empty script argument, independently of native quoting.
        $checkout = Join-Path $root ('arguments &!% ' + [char]0x96ea)
        [void][IO.Directory]::CreateDirectory($checkout)
        Assert-Equal 0 (Invoke-Capture $git.Source @('init','--quiet',('--template='+$emptyTemplate),$checkout)).ExitCode 'argument oracle initialized'
        foreach($value in @('', ' space value ', 'quote"value', 'trailing\', '&!%', ([string][char]0x96ea))) {
            $set = Invoke-Capture $git.Source @('-C',$checkout,'config','fixture.literal',$value)
            Assert-Equal 0 $set.ExitCode 'native setter receives its value, including empty'
            $get = Invoke-Capture $git.Source @('-C',$checkout,'config','--get','fixture.literal')
            Assert-Equal 0 $get.ExitCode 'native getter succeeds'
            Assert-Equal $value $get.StandardOutput.TrimEnd([char[]]"`r`n") 'literal native argument roundtrip'
        }
    }

    Invoke-TestCase 'real child drains separate full pipes, Unicode and actual exit 17' {
        $child = Join-Path $root 'query.ps1'
        [IO.File]::WriteAllText($child, @'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
[Console]::Out.WriteLine(('snow ' + [char]0x96ea))
if([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) {
    # Test-local read-only observations: no allocation, attachment, activation,
    # input, close, process launch or desktop-wide enumeration API is exposed.
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class ConsumerOwnedConsoleFacts {
    [DllImport("kernel32.dll")] public static extern IntPtr GetConsoleWindow();
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("kernel32.dll")] public static extern IntPtr GetStdHandle(int value);
    [DllImport("kernel32.dll")] public static extern uint GetFileType(IntPtr handle);
}
"@
    $window=[ConsumerOwnedConsoleFacts]::GetConsoleWindow()
    $facts=[ordered]@{window_present=($window -ne [IntPtr]::Zero);window_visible=($window -ne [IntPtr]::Zero -and [ConsumerOwnedConsoleFacts]::IsWindowVisible($window));
        stdout_type=[ConsumerOwnedConsoleFacts]::GetFileType([ConsumerOwnedConsoleFacts]::GetStdHandle(-11));stderr_type=[ConsumerOwnedConsoleFacts]::GetFileType([ConsumerOwnedConsoleFacts]::GetStdHandle(-12))}
    [Console]::Out.WriteLine('owned-console:'+($facts|ConvertTo-Json -Compress))
}
[Console]::Error.WriteLine(('e' * 131072))
[Console]::Out.WriteLine(('o' * 131072))
exit 17
'@, [Text.Encoding]::ASCII)
        $result = Invoke-Capture $hostExecutable @('-NoProfile', '-NonInteractive', '-File', $child)
        Assert-Equal 17 $result.ExitCode 'actual child exit'
        $lines = $result.StandardOutput -split '\r?\n'
        Assert-Equal ('snow ' + [char]0x96ea) $lines[0] 'Unicode pipe decoding'
        Assert-True $result.StandardError.Contains(('e' * 131072)) 'complete separate stderr'
        Assert-True $result.StandardOutput.Contains(('o' * 131072)) 'complete stdout after stderr pipe fills'
        if($isWindowsHost) {
            $factLine=@($lines|Where-Object {$_.StartsWith('owned-console:')})
            Assert-Equal 1 $factLine.Count 'one owned-child console observation'
            $facts=$factLine[0].Substring(14)|ConvertFrom-Json
            Assert-True (-not $facts.window_visible) 'the owned child has no visible console window'
            Assert-Equal 3 ([int]$facts.stdout_type) 'owned stdout is a pipe'
            Assert-Equal 3 ([int]$facts.stderr_type) 'owned stderr is a pipe'
            [void]$observations.Add([pscustomobject]@{Kind='OwnedConsoleFacts';Values=$facts;Scope='Owned child only; no global broker or console-server absence claim'})
        }
    }

    Invoke-TestCase 'real Git succeeds and fails without touching parent environment or current directory' {
        $checkout = Join-Path $root ('source &!% ' + [char]0x96ea)
        [void][IO.Directory]::CreateDirectory($checkout)
        $before = (Get-Location).ProviderPath
        $preferred = [Environment]::GetEnvironmentVariable('PreferredToolArchitecture')
        $init = Invoke-Capture $git.Source @('init', '--quiet', ('--template='+$emptyTemplate), $checkout)
        Assert-Equal 0 $init.ExitCode 'actual Git init'
        [IO.File]::WriteAllText((Join-Path $checkout 'fixture.txt'), 'synthetic identity fixture')
        Assert-Equal 0 (Invoke-Capture $git.Source @('-C', $checkout, '-c', ('core.hooksPath='+$emptyTemplate), '-c', 'core.fsmonitor=false', 'add', 'fixture.txt')).ExitCode 'actual Git add'
        Assert-Equal 0 (Invoke-Capture $git.Source @('-C', $checkout, '-c', ('core.hooksPath='+$emptyTemplate), '-c', 'core.fsmonitor=false', '-c', 'commit.gpgsign=false', '-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid', 'commit', '--quiet', '-m', 'fixture')).ExitCode 'actual Git commit'
        $head = Invoke-Capture $git.Source @('-C', $checkout, 'rev-parse', 'HEAD')
        Assert-True ($head.StandardOutput.Trim() -cmatch '^[0-9a-f]{40}$') 'actual full Git identity'
        $bad = Invoke-Capture $git.Source @('-C', $checkout, 'rev-parse', '--verify', 'refs/heads/not-present')
        Assert-True ($bad.ExitCode -ne 0 -and $bad.StandardError.Length -gt 0) 'real Git failure retains stderr and exit'
        Assert-Equal $before (Get-Location).ProviderPath 'parent CWD unchanged'
        Assert-Equal $preferred ([Environment]::GetEnvironmentVariable('PreferredToolArchitecture')) 'parent compiler host unchanged'
    }

    Invoke-TestCase 'background streaming retains separate lines and only the scalar exit on the success pipeline' {
        $child = Join-Path $root 'stream.ps1'
        [IO.File]::WriteAllText($child, '[Console]::Out.WriteLine("stream-out"); [Console]::Error.WriteLine("stream-error"); exit 17', [Text.Encoding]::ASCII)
        $result = @(Invoke-Capture $hostExecutable @('-NoProfile', '-NonInteractive', '-File', $child) -Stream)
        Assert-Equal 1 $result.Count 'no stream text or module object leaks into the result'
        Assert-Equal 17 $result[0] 'actual streaming exit'
        $lines = & $module { $script:TestLines.ToArray() }
        Assert-True (@($lines | Where-Object { $_.Line -ceq 'stream-out' -and -not $_.IsError }).Count -eq 1) 'stdout routed once'
        Assert-True (@($lines | Where-Object { $_.Line -ceq 'stream-error' -and $_.IsError }).Count -eq 1) 'stderr routed once'
    }

    Invoke-TestCase 'stream delivery occurs while the owned child is waiting, before exit' {
        $child = Join-Path $root 'live.ps1'
        $ack = Join-Path $root 'live-ack.txt'
        [IO.File]::WriteAllText($child, @'
param([string]$Ack)
[Console]::Out.WriteLine('live-ready')
$deadline=[DateTime]::UtcNow.AddSeconds(10)
while(-not [IO.File]::Exists($Ack)) {
    if([DateTime]::UtcNow -gt $deadline){exit 23}
    [Threading.Thread]::Sleep(10)
}
[Console]::Error.WriteLine('live-after-ack')
exit 17
'@, [Text.Encoding]::ASCII)
        & $module {param($Path) $script:LiveAck=$Path} $ack
        try {
            $result = @(Invoke-Capture $hostExecutable @('-NoProfile','-NonInteractive','-File',$child,$ack) -Stream)
            Assert-Equal 1 $result.Count 'live stream has only one scalar result'
            Assert-Equal 17 $result[0] 'child observed the live acknowledgment'
            Assert-Equal 'observed before child exit' ([IO.File]::ReadAllText($ack)) 'delivery preceded native exit'
        } finally { & $module { $script:LiveAck='' } }
    }

    Invoke-TestCase 'callback failure stops only the owned child and preserves the primary error' {
        $child = Join-Path $root 'callback-failure.ps1'
        $naturalExit = Join-Path $root 'callback-natural-exit.txt'
        [IO.File]::WriteAllText($child, "param([string]`$NaturalExit) [Console]::Out.WriteLine('callback-ready'); [Threading.Thread]::Sleep(10000); [IO.File]::WriteAllText(`$NaturalExit,'natural exit')", [Text.Encoding]::ASCII)
        & $module { $script:FailLine='callback-ready' }
        $message=''
        try { Invoke-Capture $hostExecutable @('-NoProfile','-NonInteractive','-File',$child,$naturalExit) -Stream | Out-Null }
        catch { $message=$_.Exception.Message }
        finally { & $module { $script:FailLine='' } }
        Assert-True $message.Contains('Synthetic primary callback failure') 'original callback error retained'
        $owned = & $module { $script:TestStarts[-1] | Select-Object Pid,StartedTicks,StartTimeState,Handle,HandleKind }
        Assert-True ($owned.StartTimeState -ceq 'observed' -and $owned.StartedTicks -gt 0) 'the controlled long-lived child has actual positive creation evidence'
        Assert-Equal $(if($isWindowsHost){'windows-process'}else{'unix-wait'}) $owned.HandleKind 'the retained handle semantics match the actual platform'
        $remaining = Get-Process -Id $owned.Pid -ErrorAction SilentlyContinue
        Assert-True ($null -eq $remaining -or $remaining.StartTime.ToUniversalTime().Ticks -ne $owned.StartedTicks) 'the same owned process is no longer running'
        Assert-True $owned.Handle.IsClosed 'the owned process handle was disposed'
        Assert-True (-not [IO.File]::Exists($naturalExit)) 'cleanup did not merely wait for natural completion'
    }

    Invoke-TestCase 'direct and wrapped synthetic fatal errors are not downgraded' {
        & $module {
            foreach($fatal in @([OutOfMemoryException]::new('synthetic'),[AccessViolationException]::new('synthetic'),[StackOverflowException]::new('synthetic'))) {
                if(-not(Test-DxUiConsumerFatalException $fatal)){throw 'Direct fatal classification failed.'}
                if(-not(Test-DxUiConsumerFatalException ([Reflection.TargetInvocationException]::new($fatal)))){throw 'Wrapped fatal classification failed.'}
            }
            if(Test-DxUiConsumerFatalException ([IO.IOException]::new('ordinary'))){throw 'Ordinary I/O is not fatal.'}
        }
    }

    Invoke-TestCase 'redirected console markers cannot enable interactive installer inheritance' {
        $child=Join-Path $root 'predicate.ps1'
        [IO.File]::WriteAllText($child, @'
param([string]$ModulePath)
$env:WT_SESSION='synthetic-marker-is-not-a-console'
$m=Import-Module $ModulePath -PassThru
$answer=& $m {Test-DxUiConsumerInteractiveConsole}
[Console]::Out.WriteLine([bool]$answer)
'@, [Text.Encoding]::ASCII)
        $result=Invoke-Capture $hostExecutable @('-NoProfile','-NonInteractive','-File',$child,$modulePath)
        Assert-Equal 0 $result.ExitCode 'redirected predicate child exit'
        Assert-Equal 'False' $result.StandardOutput.Trim() 'inherited terminal marker does not grant a console'
    }

    Invoke-TestCase 'interactive fallback preserves direct and wrapped synthetic fatal exceptions' {
        $ast=[Management.Automation.Language.Parser]::ParseFile($modulePath,[ref]$null,[ref]$null)
        $predicate=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Test-DxUiConsumerInteractiveConsole'},$true)
        $handler=$predicate.Find({param($node) $node -is [Management.Automation.Language.CatchClauseAst]},$true)
        Assert-True ($null -ne $handler) 'actual fallback catch was found'
        $body=$handler.Body.Extent.Text
        & $module {
            param($HandlerBody)
            $Probe=[scriptblock]::Create('param($Failure) try {throw $Failure} catch '+$HandlerBody)
            foreach($fatal in @([OutOfMemoryException]::new('synthetic'),[Reflection.TargetInvocationException]::new([OutOfMemoryException]::new('synthetic')))) {
                $caught=$false
                try {& $Probe $fatal|Out-Null} catch {$caught=Test-DxUiConsumerFatalException $_.Exception}
                if(-not $caught){throw 'Actual fallback catch swallowed a synthetic fatal error.'}
            }
            if((& $Probe ([IO.IOException]::new('ordinary'))) -ne $false){throw 'Ordinary fallback must remain false.'}
        } $body
    }

    Invoke-TestCase 'secondary cleanup under WarningPreference Stop cannot hide a primary pipe failure' {
        & $module {
            $savedStart=(Get-Command Start-DxUiConsumerQueryProcess).ScriptBlock
            $savedWarning=$WarningPreference
                $script:FakeDisposed=$false
            try {
                Set-Item Function:script:Start-DxUiConsumerQueryProcess {
                    param($StartInfo)
                    $reader=[pscustomobject]@{}
                    $reader|Add-Member ScriptMethod ReadToEndAsync {throw [IO.IOException]::new('synthetic primary pipe failure')}
                    $fake=[pscustomobject]@{HasExited=$false;StandardOutput=$reader;StandardError=$reader}
                    $fake|Add-Member ScriptMethod Kill {
                        if($script:CleanupFailureKind -ceq 'direct-fatal'){throw [OutOfMemoryException]::new('synthetic cleanup fatal')}
                        if($script:CleanupFailureKind -ceq 'wrapped-fatal'){throw [Reflection.TargetInvocationException]::new([OutOfMemoryException]::new('synthetic cleanup fatal'))}
                        throw [IO.IOException]::new('synthetic secondary cleanup failure')
                    }
                    $fake|Add-Member ScriptMethod WaitForExit {}
                    $fake|Add-Member ScriptMethod Dispose {$script:FakeDisposed=$true}
                    return $fake
                }
                $WarningPreference='Stop'
                foreach($kind in @('ordinary','direct-fatal','wrapped-fatal')) {
                    $script:CleanupFailureKind=$kind
                    $script:FakeDisposed=$false
                    $failure=$null
                    try { Invoke-DxUiConsumerNativeQuery -FilePath $script:NeverStarted | Out-Null }
                    catch {$failure=$_.Exception}
                    if($null -eq $failure){throw 'The synthetic pipe failure was lost.'}
                    if($kind -ceq 'ordinary') {
                        if(-not $failure.Message.Contains('synthetic primary pipe failure')){throw 'Primary pipe error was replaced.'}
                    } elseif(-not(Test-DxUiConsumerFatalException $failure)){throw 'Fatal cleanup was downgraded to the prior failure.'}
                    if(-not $script:FakeDisposed){throw 'The failed fake owner was not disposed.'}
                }
            } finally {
                $WarningPreference=$savedWarning
                Set-Item Function:script:Start-DxUiConsumerQueryProcess $savedStart
            }
        }
    }

    if ($isWindowsHost) {
        Invoke-TestCase 'batch route rejects caller arguments or a different shell before launch' {
            $before=& $module {$script:TestStarts.Count}
            foreach($mode in @('arguments','shell','not-streaming')) {
                $message=''
                $failure=$null
                try {
                    & $module {
                        param($Mode)
                        $file=Join-Path $env:SystemRoot 'System32/cmd.exe'
                        if($Mode -eq 'shell'){$file='not-the-approved-shell'}
                        # An empty pipeline from an if expression becomes null;
                        # preserve a genuine empty array under strict PowerShell 5.1.
                        $arguments=@()
                        if($Mode -eq 'arguments'){$arguments=@('untrusted & input')}
                        Invoke-DxUiConsumerNativeQuery -FilePath $file -Arguments $arguments -BootstrapBatch -Stream:($Mode -ne 'not-streaming')
                    } $mode | Out-Null
                } catch {$failure=$_;$message=$_.Exception.Message}
                $refusal=[pscustomobject]@{Kind='BatchRefusal';Mode=$mode;Message=$message;
                    ExceptionType=$(if($null -ne $failure){$failure.Exception.GetType().FullName}else{''});
                    BaseExceptionType=$(if($null -ne $failure){$failure.Exception.GetBaseException().GetType().FullName}else{''});
                    ErrorId=$(if($null -ne $failure){$failure.FullyQualifiedErrorId}else{''})}
                [void]$observations.Add($refusal)
                [IO.File]::WriteAllText((Join-Path $root ('batch-refusal-'+$mode+'.json')),($refusal|ConvertTo-Json -Depth 4),[Text.UTF8Encoding]::new($false))
                Assert-True $message.Contains('fixed captured cmd route') "specific $mode refusal"
            }
            Assert-Equal $before (& $module {$script:TestStarts.Count}) 'no refused batch reached the starter'
        }
        Invoke-TestCase 'fixed batch route cannot interpret metacharacters or Unicode from its working directory' {
            $directory = Join-Path $root ('batch &!% ' + [char]0x96ea)
            [void][IO.Directory]::CreateDirectory($directory)
            [IO.File]::WriteAllText((Join-Path $directory 'bootstrap-vcpkg.bat'), '@echo off' + [Environment]::NewLine + 'echo batch-ok' + [Environment]::NewLine + 'if not "%~1"=="-disableMetrics" exit /b 19' + [Environment]::NewLine + 'exit /b 17', [Text.Encoding]::ASCII)
            $exitCode = & $module { param($directory) Invoke-DxUiConsumerNativeQuery -FilePath (Join-Path $env:SystemRoot 'System32/cmd.exe') -WorkingDirectory $directory -BootstrapBatch -Stream } $directory
            Assert-Equal 17 $exitCode 'fixed batch actual exit and flags'
            Assert-True (-not [IO.File]::Exists((Join-Path $root 'injected'))) 'no shell injection marker'
        }
    }

    Invoke-TestCase 'identity oracle and specific query failures preserve API4 semantics without native launch' {
        $fixture=Join-Path $root 'identity-oracle'
        $tools=Join-Path $fixture 'tools'
        $sdk=Join-Path $fixture 'sdk'
        $buildTool=Join-Path $fixture 'MSBuild.exe'
        foreach($relative in @('capabilities.json','MSBuild.exe','tools/bin/Hostx64/x64/cl.exe','tools/bin/Hostx64/x64/link.exe','sdk/Include/fixture/um/Windows.h','sdk/Lib/fixture/um/x64/kernel32.lib','sdk/Lib/fixture/ucrt/x64/ucrt.lib')) {
            $path=Join-Path $fixture $relative
            [void][IO.Directory]::CreateDirectory((Split-Path $path))
            [IO.File]::WriteAllText($path, 'synthetic '+$relative)
        }
        [IO.File]::WriteAllText((Join-Path $fixture 'capabilities.json'),' {"apiRevision":4}')
        $properties=[pscustomobject]@{PlatformToolset='v145';VCToolsInstallDir=$tools;VCToolsVersion='fixture';WindowsSdkDir=$sdk;
            WindowsTargetPlatformVersion='fixture';PreferredToolArchitecture='x64'}
        $before=& $module {$script:TestStarts.Count}
        & $module {
            param($Properties,$BuildTool)
            $script:Oracle=[pscustomobject]@{Mode='valid';Properties=$Properties;BuildTool=$BuildTool}
            $script:SavedQuery=(Get-Command Invoke-DxUiConsumerNativeQuery).ScriptBlock
            Set-Item Function:script:Invoke-DxUiConsumerNativeQuery {
                param($FilePath,$Arguments)
                if($Arguments[-1] -ceq 'HEAD') {
                    return [pscustomobject]@{ExitCode=$(if($script:Oracle.Mode -ceq 'git-exit'){17}else{0});StandardOutput=('a'*40);StandardError='synthetic Git stderr'}
                }
                if($FilePath -cne $script:Oracle.BuildTool -or $Arguments[-1] -cnotlike '-getProperty:*'){throw 'Unexpected identity probe arguments.'}
                $props=$script:Oracle.Properties|Select-Object *
                if($script:Oracle.Mode -ceq 'unsupported'){$props.PreferredToolArchitecture='foreign-host'}
                if($script:Oracle.Mode -ceq 'missing'){$props.WindowsSdkDir=''}
                $json=@{Properties=$props}|ConvertTo-Json
                return [pscustomobject]@{ExitCode=$(if($script:Oracle.Mode -ceq 'metadata-exit'){29}else{0});StandardOutput=$json;StandardError='synthetic metadata stderr'}
            }
        } $properties $buildTool
        try {
            $actual=Get-DxUiConsumerBuildIdentity -DxUiRoot $fixture -MSBuildPath $buildTool -Platform x64
            Assert-IdentityOracle $actual ('a'*40) $properties $buildTool $false
            foreach($case in @(
                @{Mode='git-exit';Message='Cannot identify the pinned DxUi source'},
                @{Mode='metadata-exit';Message='Could not evaluate the DxUi toolchain identity'},
                @{Mode='missing';Message='Missing DxUi build identity property: WindowsSdkDir'},
                @{Mode='unsupported';Message='Unsupported compiler host architecture: foreign-host'}
            )) {
                & $module {param($Mode) $script:Oracle.Mode=$Mode} $case.Mode
                $message=''
                try {Get-DxUiConsumerBuildIdentity -DxUiRoot $fixture -MSBuildPath $buildTool -Platform x64|Out-Null}
                catch {$message=$_.Exception.Message}
                Assert-True $message.Contains($case.Message) ('specific '+$case.Mode+' refusal')
            }
        } finally { & $module {Set-Item Function:script:Invoke-DxUiConsumerNativeQuery $script:SavedQuery} }
        Assert-Equal $before (& $module {$script:TestStarts.Count}) 'mocked identity controls launched no process'
    }

    if ($MSBuildPath) {
        Invoke-TestCase 'selected MSBuild preserves the complete API4 identity oracle without compiling or restoring' {
            Assert-True $isWindowsHost 'actual MSBuild metadata is Windows-only'
            Assert-True ([IO.File]::Exists($MSBuildPath)) 'trusted selected MSBuild exists'
            & $module {
                $script:IdentityQueries=[Collections.Generic.List[object]]::new()
                $script:OriginalQuery=(Get-Command Invoke-DxUiConsumerNativeQuery).ScriptBlock
                Set-Item Function:script:Invoke-DxUiConsumerNativeQuery {
                    param([string]$FilePath,[string[]]$Arguments=@(),[string]$WorkingDirectory=(Get-Location).ProviderPath,[switch]$Stream,[switch]$BootstrapBatch)
                    $result=& $script:OriginalQuery @PSBoundParameters
                    [void]$script:IdentityQueries.Add([pscustomobject]@{File=$FilePath;Arguments=@($Arguments);Directory=$WorkingDirectory;Result=$result})
                    return $result
                }
            }
            try {
                foreach($disabled in @($false,$true)) {
                    $identity = Get-DxUiConsumerBuildIdentity -DxUiRoot $repository -MSBuildPath $MSBuildPath -Platform x64 -DisableStlAnnotations:$disabled
                    $queries=@(& $module { $script:IdentityQueries.ToArray() })
                    $headQuery=$queries[-2];$metadataQuery=$queries[-1]
                    Assert-Equal $git.Source $headQuery.File 'first actual Git application'
                    Assert-Equal ('-C|'+$repository+'|rev-parse|HEAD') ($headQuery.Arguments -join '|') 'unchanged literal Git identity query'
                    Assert-Equal $MSBuildPath $metadataQuery.File 'actual selected MSBuild is executed and hashed'
                    Assert-Equal (Get-Location).ProviderPath $metadataQuery.Directory 'caller filesystem CWD preserved'
                    Assert-Equal 0 $headQuery.Result.ExitCode 'actual Git identity exit'
                    Assert-Equal 0 $metadataQuery.Result.ExitCode 'actual MSBuild metadata exit'
                    $properties=($metadataQuery.Result.StandardOutput|ConvertFrom-Json).Properties
                    Assert-IdentityOracle $identity $headQuery.Result.StandardOutput.Trim() $properties $MSBuildPath $disabled
                    [void]$observations.Add([pscustomobject]@{Kind='ActualIdentity';Fingerprint=$identity.Fingerprint;Identity=$identity.Identity;RawMetadata=$metadataQuery.Result.StandardOutput})
                }
            } finally {
                [void]$observations.Add([pscustomobject]@{Kind='IdentityQueries';Values=@(& $module {$script:IdentityQueries.ToArray()})})
                & $module { Set-Item Function:script:Invoke-DxUiConsumerNativeQuery $script:OriginalQuery }
            }
        }
    } else {
        Write-Host 'Actual selected-MSBuild identity: NOT_EVALUATED (no trusted MSBuildPath supplied).'
        [void]$observations.Add([pscustomobject]@{Kind='ActualIdentity';Status='NOT_EVALUATED';Reason='No trusted MSBuildPath supplied'})
    }

    Invoke-TestCase 'public projection rejects unknown fields and path-shaped values without copying raw evidence' {
        $lookalike=[pscustomobject]@{Name='Get-FileHash';Module=$hostUtility;CommandType=$hostHashCommand.CommandType}
        $lookalikeBinding=Get-ControlHostHashBinding $lookalike $hostHashExport $hostUtility $utilityManifest $pathComparison
        Assert-True (-not $lookalikeBinding.command_real -and -not $lookalikeBinding.owner_verified) 'a matching metadata lookalike is not an actual callable'
        $nullBinding=Get-ControlHostHashBinding $null $hostHashExport $hostUtility $utilityManifest $pathComparison
        Assert-True (-not $nullBinding.owner_verified) 'a missing callable is rejected'
        $wrongExport=if($hostHashCommand -is [Management.Automation.FunctionInfo]){Get-Command Get-ControlTextHash -CommandType Function}else{Get-Command Get-Process -CommandType Cmdlet}
        $wrongBinding=Get-ControlHostHashBinding $hostHashCommand $wrongExport $hostUtility $utilityManifest $pathComparison
        Assert-True ($wrongBinding.command_owner_is_top -and -not $wrongBinding.owner_verified) 'the correct top-level owner cannot authorize a different exported callable'
        if($hostHashCommand -is [Management.Automation.FunctionInfo]) {
            Assert-True (-not $wrongBinding.function_scriptblock_reference_equal) 'wrong function body is rejected by actual ScriptBlock identity'
        } else {
            Assert-True (-not $wrongBinding.cmdlet_implementing_type_reference_equal) 'wrong cmdlet is rejected by actual implementing-type identity'
        }
        [void]$observations.Add([pscustomobject]@{Kind='HostBindingCounterfactuals';Lookalike=$lookalikeBinding;Missing=$nullBinding;WrongCallable=$wrongBinding})
        Assert-True (Test-ControlAbsolutePath $parent) 'the actual borrowed parent is fully qualified'
        foreach($relative in @('relative-child','Z:child','\child')) {
            Assert-True (-not (Test-ControlAbsolutePath $relative)) 'nonwriting relative-parent refusal'
        }
        $projection=New-ControlPublicProjection @([pscustomobject]@{Error='synthetic private failure';Name='not published'}) $true ('a'*64) ('b'*64)
        Assert-ControlPublicProjection $projection
        foreach($mode in @('field','scalar','case','host-owner','setup-field','setup-phase','setup-type','setup-code')) {
            $bad=$projection|ConvertTo-Json -Depth 8|ConvertFrom-Json
            switch($mode) {
                field {$bad|Add-Member NoteProperty fixture_path 'C:\synthetic-private'}
                scalar {$bad.host_version='C:\synthetic-private'}
                case {$bad.cases[0]|Add-Member NoteProperty raw_error 'C:\synthetic-private'}
                host-owner {$bad.host_utility|Add-Member NoteProperty raw_path 'C:\synthetic-private'}
                setup-field {$bad.setup|Add-Member NoteProperty raw_message 'C:\synthetic-private'}
                setup-phase {$bad.setup.phase='C:\synthetic-private'}
                setup-type {$bad.setup.type='System.Private.UnreviewedType'}
                setup-code {$bad.setup.code='C:\synthetic-private'}
            }
            $rejected=$false
            try {Assert-ControlPublicProjection $bad} catch {$rejected=$true}
            Assert-True $rejected ('closed public projection rejects '+$mode)
        }
        Assert-True (-not (($projection|ConvertTo-Json -Depth 8).Contains('synthetic private failure'))) 'raw error is represented only by a hash'
        $membership=Join-Path $root 'publication-negative'
        [void][IO.Directory]::CreateDirectory($membership)
        foreach($name in @('summary.json','manifest.json','raw-private.txt')) {[IO.File]::WriteAllText((Join-Path $membership $name),'synthetic')}
        $rejected=$false
        try {Assert-ControlPublicMembers $membership} catch {$rejected=$_.Exception.Message -ceq 'Public projection membership is invalid.'}
        Assert-True $rejected 'an extra ordinary raw member is refused without publishing it'
    }

    Complete-TestRun 'ConsumerBuild'
    $script:SetupPhase='completed'
} catch {
    $primaryFailure = $true
    $primaryErrorHash = Get-ControlTextHash $_.Exception.ToString()
    if($script:SetupPhase.StartsWith('source-head.')) {
        $script:SetupPhase='source-head.'+(& $module {$script:StartStage})
    }
    $cause=$_.Exception.GetBaseException()
    $type=$cause.GetType().Name
    $script:SetupErrorType=if($type -cin @('InvalidOperationException','Win32Exception','IOException','PlatformNotSupportedException','CommandNotFoundException','RuntimeException')){$type}else{'OTHER'}
    $script:SetupErrorCode=[int]$cause.HResult
    $primaryErrorRecord=[pscustomobject]@{phase=$script:SetupPhase;exception_type=$cause.GetType().FullName;hresult=[int]$cause.HResult;
        fully_qualified_error_id=$_.FullyQualifiedErrorId;exception=$_.Exception.ToString();script_stack=$_.ScriptStackTrace}
    throw
} finally {
    try {
        if($null -ne $primaryErrorRecord) {
            [IO.File]::WriteAllText((Join-Path $root 'control-error.json'),($primaryErrorRecord|ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
        }
        $starts = & $module {
            @($script:TestStarts | Select-Object Pid, StartedTicks, StartTimeState, HandleKind, ObservationDisposed, FileName, Arguments, WorkingDirectory, NoWindow, RedirectOutput, RedirectError)
        }
        [void]$observations.Add([pscustomobject]@{ Kind = 'Starts'; Values = @($starts) })
        [void]$observations.Add([pscustomobject]@{Kind='Cases';Values=@(& $support { $script:Results.ToArray() })})
        [void]$observations.Add([pscustomobject]@{Kind='Context';HostVersion=$PSVersionTable.PSVersion.ToString();Host=$hostExecutable;
            ModuleSha256=(Get-FileHash $modulePath -Algorithm SHA256).Hash;MSBuildRequested=[bool]$MSBuildPath;NoProductBuild=$true})
        $observationPath = Join-Path $root 'observations.json'
        [IO.File]::WriteAllText($observationPath, ($observations | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
        $cases = @(& $support { $script:Results.ToArray() })
        $failed = @($cases | Where-Object { $_.Error }).Count
        $summary = [ordered]@{schema='dxui.consumer-process-controls.v1';status=$(if($primaryFailure -or $failed -or $cases.Count -ne (14+[int]$isWindowsHost*2+[int][bool]$MSBuildPath)){'FAILED'}else{'PASSED'});
            passed=($cases.Count-$failed);failed=$failed;total=$cases.Count;expected_cases=(14+[int]$isWindowsHost*2+[int][bool]$MSBuildPath);
            host_version=$PSVersionTable.PSVersion.ToString();source_commit=$script:ObservedSourceHead;
            test_sha256=(Get-FileHash $PSCommandPath -Algorithm SHA256).Hash;module_sha256=(Get-FileHash $modulePath -Algorithm SHA256).Hash;
            host_bootstrap_sha256=(Get-FileHash -LiteralPath (Join-Path $root 'host-bootstrap.json') -Algorithm SHA256).Hash;
            actual_msbuild_requested=[bool]$MSBuildPath;actual_identity_observations=@($observations|Where-Object {$_.Kind -ceq 'ActualIdentity' -and $_.PSObject.Properties['Identity']}).Count;
            native_starts=@($starts).Count;observations_sha256=(Get-FileHash $observationPath -Algorithm SHA256).Hash;
            setup=[ordered]@{phase=$script:SetupPhase;type=$script:SetupErrorType;code=$script:SetupErrorCode};
            fixture=$root;product_compilation=$false;foreground_input=$false;completed_utc=[DateTime]::UtcNow.ToString('o')}
        [IO.File]::WriteAllText((Join-Path $root 'summary.json'), ($summary|ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
        $public=Join-Path $root 'public'
        if (Test-Path -LiteralPath $public) {throw 'Public projection directory must be new.'}
        [void][IO.Directory]::CreateDirectory($public)
        if (([IO.File]::GetAttributes($public) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {throw 'Public projection directory cannot be a reparse point.'}
        $projection=New-ControlPublicProjection $cases $primaryFailure $primaryErrorHash $summary.observations_sha256
        Assert-ControlPublicProjection $projection
        $publicSummary=Join-Path $public 'summary.json'
        [IO.File]::WriteAllText($publicSummary,($projection|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
        Assert-ControlPublicProjection (Get-Content -LiteralPath $publicSummary -Raw|ConvertFrom-Json)
        $member=Get-Item -LiteralPath $publicSummary
        $publicManifest=[ordered]@{schema='dxui.consumer-process-controls-public-manifest.v1';self_excluded=$true;members=@([ordered]@{name='summary.json';length=$member.Length;sha256=(Get-FileHash -LiteralPath $publicSummary -Algorithm SHA256).Hash})}
        [IO.File]::WriteAllText((Join-Path $public 'manifest.json'),($publicManifest|ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
        Assert-ControlPublicMembers $public
        $bound=Get-Content -LiteralPath (Join-Path $public 'manifest.json') -Raw|ConvertFrom-Json
        if ($bound.members.Count -ne 1 -or $bound.members[0].name -cne 'summary.json' -or $bound.members[0].length -ne $member.Length -or
            $bound.members[0].sha256 -cne (Get-FileHash -LiteralPath $publicSummary -Algorithm SHA256).Hash) {throw 'Public projection member hash is invalid.'}
        Write-Host "Retained ConsumerBuild fixture: $root"
    } catch [IO.IOException] {
        if (-not $primaryFailure) { throw }
        Write-Warning 'ConsumerBuild secondary evidence retention failed; preserving the primary failure.' -WarningAction Continue
    } catch [UnauthorizedAccessException] {
        if (-not $primaryFailure) { throw }
        Write-Warning 'ConsumerBuild secondary evidence retention was denied; preserving the primary failure.' -WarningAction Continue
    } finally {
        & $module {
            Set-Item Function:script:Start-DxUiConsumerQueryProcess $script:OriginalStart
            Set-Item Function:script:Write-DxUiConsumerProcessLine $script:OriginalLine
        }
        [Console]::OutputEncoding = $oldEncoding
        if ($null -eq $oldExitVariable) { Remove-Variable LASTEXITCODE -Scope Global -ErrorAction SilentlyContinue }
        else { $global:LASTEXITCODE = $oldExitCode }
    }
}
