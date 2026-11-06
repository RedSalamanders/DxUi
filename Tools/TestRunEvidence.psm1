# Standalone runner support, mirrored in RedPrism, RedXe and RedTilde. No sibling repository is loaded.
# Results are born under evidence: finishing a run never copies, deletes or changes payload attributes.
Set-StrictMode -Version Latest

function Initialize-TestRunEvidenceNative {
    if ('TestRunEvidenceV2.Native' -as [type]) { return }
    Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using Microsoft.Win32.SafeHandles;
namespace TestRunEvidenceV2 {
    public sealed class StreamRecord { public string Name; public long Length; public string Sha256; }
    public sealed class MetadataRecord { public int Attributes; public long CreatedTicks, WrittenTicks; }
    public static class Native {
        [StructLayout(LayoutKind.Sequential)] struct Info {
            public uint Attributes; public System.Runtime.InteropServices.ComTypes.FILETIME Created, Accessed, Written;
            public uint Volume, SizeHigh, SizeLow, Links, IndexHigh, IndexLow;
        }
        [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct StreamData {
            public long Size;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst=296)] public string Name;
        }
        [StructLayout(LayoutKind.Sequential)] struct FileIdInfo { public ulong Volume, Low, High; }
        [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
        static extern SafeFileHandle CreateFileW(string path, uint access, uint share, IntPtr security, uint mode, uint flags, IntPtr template);
        [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetFileInformationByHandle(SafeFileHandle file, out Info info);
        [DllImport("kernel32.dll", SetLastError=true)]
        static extern bool GetFileInformationByHandleEx(SafeFileHandle file, int kind, out FileIdInfo info, uint size);
        [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
        static extern IntPtr FindFirstStreamW(string path, int level, out StreamData data, uint flags);
        [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
        static extern bool FindNextStreamW(IntPtr find, out StreamData data);
        [DllImport("kernel32.dll")] static extern bool FindClose(IntPtr find);
        static string LongPath(string path) { return path.StartsWith(@"\\?\") ? path : @"\\?\" + path; }
        // Deny write/delete sharing. Fresh 128-bit identity checks also detect ReFS open-directory renames.
        public static SafeFileHandle Pin(string path, bool directory) {
            var handle = CreateFileW(LongPath(path), directory ? 0x80u : 0x80000000u,
                1u, IntPtr.Zero, 3, 0x02200000, IntPtr.Zero);
            if (handle.IsInvalid) { int e=Marshal.GetLastWin32Error(); handle.Dispose(); throw new Win32Exception(e, path); }
            Info info;
            if (!GetFileInformationByHandle(handle, out info)) { int e=Marshal.GetLastWin32Error(); handle.Dispose(); throw new Win32Exception(e, path); }
            if ((info.Attributes & 0x400) != 0 || ((info.Attributes & 0x10) != 0) != directory) {
                handle.Dispose(); throw new IOException("Refusing reparse point or unexpected file kind: " + path);
            }
            return handle;
        }
        public static string Identity(SafeFileHandle handle) {
            FileIdInfo info;
            if (!GetFileInformationByHandleEx(handle, 18, out info, 24)) throw new Win32Exception(Marshal.GetLastWin32Error());
            return info.Volume.ToString("X16") + ":" + info.High.ToString("X16") + info.Low.ToString("X16");
        }
        public static MetadataRecord Metadata(SafeFileHandle handle) {
            Info info;
            if (!GetFileInformationByHandle(handle, out info)) throw new Win32Exception(Marshal.GetLastWin32Error());
            long created = ((long)(uint)info.Created.dwHighDateTime << 32) | (uint)info.Created.dwLowDateTime;
            long written = ((long)(uint)info.Written.dwHighDateTime << 32) | (uint)info.Written.dwLowDateTime;
            return new MetadataRecord { Attributes=(int)info.Attributes,
                CreatedTicks=DateTime.FromFileTimeUtc(created).Ticks, WrittenTicks=DateTime.FromFileTimeUtc(written).Ticks };
        }
        public static FileStream Lock(string path) {
            var handle = CreateFileW(LongPath(path), 0xC0000000, 0, IntPtr.Zero, 4, 0x02200000, IntPtr.Zero);
            if (handle.IsInvalid) { int e=Marshal.GetLastWin32Error(); handle.Dispose(); throw new Win32Exception(e, path); }
            Info info;
            if (!GetFileInformationByHandle(handle, out info)) { int e=Marshal.GetLastWin32Error(); handle.Dispose(); throw new Win32Exception(e, path); }
            if ((info.Attributes & 0x410) != 0) { handle.Dispose(); throw new IOException("Refusing linked or directory lease: " + path); }
            return new FileStream(handle, FileAccess.ReadWrite);
        }
        public static StreamRecord[] Streams(string path) {
            var result = new List<StreamRecord>();
            StreamData data;
            IntPtr find = FindFirstStreamW(LongPath(path), 0, out data, 0);
            if (find == new IntPtr(-1)) {
                int e=Marshal.GetLastWin32Error();
                if (e != 38) throw new Win32Exception(e, path);
            } else try {
                do {
                    string streamPath = data.Name == "::$DATA" ? path : path + data.Name;
                    using (var handle=Pin(streamPath, false))
                    using (var file=new FileStream(handle, FileAccess.Read))
                    using (var sha=SHA256.Create()) {
                        result.Add(new StreamRecord { Name=data.Name, Length=file.Length,
                            Sha256=BitConverter.ToString(sha.ComputeHash(file)).Replace("-", "").ToLowerInvariant() });
                    }
                } while (FindNextStreamW(find, out data));
                int e=Marshal.GetLastWin32Error(); if (e != 38) throw new Win32Exception(e, path);
            } finally { FindClose(find); }
            return result.ToArray();
        }
    }
}
'@
}

function Write-TestRunEvidenceRecord([string] $Path, $Value) {
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes(($Value | ConvertTo-Json -Depth 14))
    if (Test-Path -LiteralPath $Path) { throw "Evidence record already exists: $Path" }
    $temporary = $Path + '.pending-' + [guid]::NewGuid().ToString('N')
    $stream = [IO.File]::Open($temporary, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::Read)
    try { $stream.Write($bytes, 0, $bytes.Length); $stream.Flush($true) } finally { $stream.Dispose() }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $expected = [BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-', '') } finally { $sha.Dispose() }
    if ((Get-FileHash -LiteralPath $temporary).Hash -cne $expected) { throw "Evidence record failed staging readback: $temporary" }
    [IO.File]::Move($temporary, $Path)
    if ((Get-FileHash -LiteralPath $Path).Hash -cne $expected) {
        throw "Evidence record failed readback: $Path"
    }
}

function Start-TestRunEvidence {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Repository, [Parameter(Mandatory)][hashtable] $Context, [switch] $CaptureNativeArtifacts)
    Initialize-TestRunEvidenceNative
    $root = [IO.Path]::GetFullPath($Repository).TrimEnd('\')
    if (-not [IO.Path]::IsPathRooted($Repository) -or $root -eq [IO.Path]::GetPathRoot($root).TrimEnd('\') -or $root.StartsWith('\\')) {
        throw 'Evidence requires an existing absolute local repository directory, never a drive root or UNC path.'
    }
    $failureDirectory = ''
    if ($CaptureNativeArtifacts -and ($Context.ContainsKey('platform') -or $Context.ContainsKey('configuration'))) {
        if (-not $Context.ContainsKey('platform') -or -not $Context.ContainsKey('configuration') -or
            $Context.platform -cnotin @('x64','ARM64') -or $Context.configuration -cnotin @('Debug','Release','ASan Debug')) {
            throw 'Native failure evidence requires an exact supported platform and configuration.'
        }
        $failureDirectory = Join-Path $root ('.build/'+$Context.platform+'/'+$Context.configuration+'/FailureEvidence')
    }
    $pins = [Collections.Generic.List[IDisposable]]::new()
    $anchors = [Collections.Generic.List[object]]::new()
    $lock = $null
    $artifactLock = $null
    try {
        $ancestors = [Collections.Generic.Stack[string]]::new()
        $part = $root
        while ($part) { $ancestors.Push($part); $part = [IO.Path]::GetDirectoryName($part) }
        foreach ($part in $ancestors) {
            $pin = [TestRunEvidenceV2.Native]::Pin($part, $true)
            $pins.Add($pin)
            $anchors.Add(@{path=$part;identity=[TestRunEvidenceV2.Native]::Identity($pin)})
        }
        $parent = $root
        foreach ($name in @('.build', 'evidence', 'test-runs')) {
            $parent = Join-Path $parent $name
            [void][IO.Directory]::CreateDirectory($parent)
            $pin = [TestRunEvidenceV2.Native]::Pin($parent, $true)
            $pins.Add($pin)
            $anchors.Add(@{path=$parent;identity=[TestRunEvidenceV2.Native]::Identity($pin)})
        }
        $id = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ') + "-$PID-" + [guid]::NewGuid().ToString('N')
        $directory = Join-Path $parent $id
        if (Test-Path -LiteralPath $directory) { throw "Evidence collision: $directory" }
        [void][IO.Directory]::CreateDirectory($directory)
        $pin = [TestRunEvidenceV2.Native]::Pin($directory, $true)
        $pins.Add($pin)
        $anchors.Add(@{path=$directory;identity=[TestRunEvidenceV2.Native]::Identity($pin)})
        $lock = [IO.File]::Open((Join-Path $directory 'run.lock'), 'CreateNew', 'ReadWrite', 'Read')
        $path = Join-Path $directory 'payload'
        [void][IO.Directory]::CreateDirectory($path)
        $pin = [TestRunEvidenceV2.Native]::Pin($path, $true)
        $pins.Add($pin)
        $anchors.Add(@{path=$path;identity=[TestRunEvidenceV2.Native]::Identity($pin)})
        $run = [pscustomobject]@{ Repository=$root; Id=$id; Directory=$directory; Path=$path; Pins=$pins; Anchors=$anchors; Lock=$lock;
            Owner=$PID; OwnerStarted=(Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks; Closed=$false;
            CaptureNativeArtifacts=[bool]$CaptureNativeArtifacts; ArtifactLock=$null; FailureDirectory=$failureDirectory }
        Write-TestRunEvidenceRecord (Join-Path $directory 'started.json') ([ordered]@{
            schema=1; id=$id; repository=$root; owner=$run.Owner; ownerStartedTicks=$run.OwnerStarted;
            startedUtc=[DateTime]::UtcNow.ToString('o'); context=$Context
        })
        if ($CaptureNativeArtifacts) {
            $artifactLock = [TestRunEvidenceV2.Native]::Lock((Join-Path $root '.build/test-artifacts.lock'))
            $run.ArtifactLock = $artifactLock
            Save-TestRunNativeArtifacts -Run $run -Label 'previous-native-artifacts'
        }
        Write-Host "Run evidence: $directory"
        return $run
    } catch {
        if ($artifactLock) { $artifactLock.Dispose() }
        if ($lock) { $lock.Dispose() }
        for ($i=$pins.Count-1; $i -ge 0; $i--) { $pins[$i].Dispose() }
        throw
    }
}

function Save-TestRunNativeArtifacts($Run, [string] $Label) {
    $sources = @(@{path=(Join-Path $Run.Repository '.build/test-artifacts');label=$Label;profile=$false})
    if ($Run.FailureDirectory) {
        $allowed = '^'+[regex]::Escape($Run.Repository)+'\\\.build\\(?:x64|ARM64)\\(?:Debug|Release|ASan Debug)\\FailureEvidence$'
        if ($Run.FailureDirectory -cnotmatch $allowed) { throw 'Native failure evidence escaped its exact profile.' }
        $sources += @{path=$Run.FailureDirectory;label=($Label+'-failures');profile=$true}
    }
    foreach ($entry in $sources) {
    $source = $entry.path
    $labelName = $entry.label
    if (-not (Test-Path -LiteralPath $source)) { continue }
    $parentPins = [Collections.Generic.List[IDisposable]]::new()
    try {
    if ($entry.profile) {
        $profile = [IO.Path]::GetDirectoryName($source)
        foreach ($ancestor in @([IO.Path]::GetDirectoryName($profile),$profile)) {
            $parentPins.Add([TestRunEvidenceV2.Native]::Pin($ancestor,$true))
        }
    }
    # This historical native path is shared across profiles. Refuse independently launched native tests;
    # the runner lease serializes new invocations, and neither path ever terminates another process.
    $prefix = (Join-Path $Run.Repository '.build').TrimEnd('\') + '\'
    $profilePrefix = [IO.Path]::GetDirectoryName($source).TrimEnd('\') + '\'
    $active = @(Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
        ($_.Name -match '^DxUi\.(ControlTests|EmbeddedTests|FoundationTests|InteractiveLease)\.exe$' -and
        $_.ExecutablePath -and $_.ExecutablePath.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) -or
        ($entry.profile -and (($_.ExecutablePath -and $_.ExecutablePath.StartsWith($profilePrefix, [StringComparison]::OrdinalIgnoreCase)) -or
        ($_.Name -in @('MSBuild.exe','cl.exe','link.exe') -and $_.CommandLine -and $_.CommandLine.Contains($profilePrefix, [StringComparison]::OrdinalIgnoreCase))))
    })
    if ($active.Count) { throw "Native artifact output is in use (PID $($active.ProcessId -join ', ')); it was preserved." }
    $destination = Join-Path $Run.Path $labelName
    if (Test-Path -LiteralPath $destination) { throw "Native artifact evidence already exists: $destination" }
    $beforePins = [Collections.Generic.List[IDisposable]]::new()
    try {
        $beforePins.Add([TestRunEvidenceV2.Native]::Pin($source, $true))
        $identity = [TestRunEvidenceV2.Native]::Identity($beforePins[0])
        $before = Get-TestRunEvidenceInventory $source $beforePins | ConvertTo-Json -Depth 12 -Compress
    } finally { for ($i=$beforePins.Count-1; $i -ge 0; $i--) { $beforePins[$i].Dispose() } }
    Write-TestRunEvidenceRecord (Join-Path $Run.Directory ($labelName+'.intent.json')) @{source=$source;destination=$destination;identity=$identity;inventory=($before | ConvertFrom-Json)}
    # Fixed native-artifact/profile child of the pinned repository; same-volume rename, no recursive deletion.
    [IO.Directory]::Move($source, $destination)
    $afterPins = [Collections.Generic.List[IDisposable]]::new()
    try {
        $afterPins.Add([TestRunEvidenceV2.Native]::Pin($destination, $true))
        $after = Get-TestRunEvidenceInventory $destination $afterPins | ConvertTo-Json -Depth 12 -Compress
        Write-TestRunEvidenceRecord (Join-Path $Run.Directory ($labelName+'.after.json')) @{identity=[TestRunEvidenceV2.Native]::Identity($afterPins[0]);inventory=($after | ConvertFrom-Json)}
        if ([TestRunEvidenceV2.Native]::Identity($afterPins[0]) -cne $identity -or $after -cne $before) {
            throw "Native artifact rename failed preservation verification; evidence retained at $destination"
        }
    } finally { for ($i=$afterPins.Count-1; $i -ge 0; $i--) { $afterPins[$i].Dispose() } }
    if ($entry.profile -and $Label -ceq 'native-artifacts') {
        # Current failure dumps need their actual linked images and PDBs. Prior
        # dumps remain historical and are never paired with replacement binaries.
        $images = Join-Path $Run.Path ($labelName+'-images.zip')
        $bindings = @()
        $imagePins = [Collections.Generic.List[IDisposable]]::new()
        $archive = [IO.Compression.ZipFile]::Open($images,[IO.Compression.ZipArchiveMode]::Create)
        try {
        foreach ($name in @('DxUi.ControlTests','DxUi.FoundationTests','DxUi.EmbeddedTests')) {
            foreach ($extension in @('.exe','.pdb')) {
                $image = Join-Path ([IO.Path]::GetDirectoryName($source)) ($name+$extension)
                if (-not (Test-Path -LiteralPath $image -PathType Leaf)) { continue }
                $imagePins.Add([TestRunEvidenceV2.Native]::Pin($image,$false))
                $sha = (Get-FileHash -LiteralPath $image -Algorithm SHA256).Hash
                # Stream directly: staging three full linked PDB copies can exhaust
                # the volume before a failed run's evidence can be completed.
                $zipEntry = [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$image,($name+$extension),[IO.Compression.CompressionLevel]::Optimal)
                $bindings += @{file=($name+$extension);sha256=$sha;bytes=([IO.FileInfo]::new($image)).Length}
            }
        }
        $archive.Dispose();$archive=$null
        $archive = [IO.Compression.ZipFile]::OpenRead($images)
        foreach ($binding in $bindings) {
            $zipEntry = $archive.GetEntry($binding.file)
            if (-not $zipEntry -or $zipEntry.Length -ne $binding.bytes) { throw 'Failure image archive member length differs.' }
            $stream = $zipEntry.Open()
            try { $actual = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($stream)) } finally { $stream.Dispose() }
            if ($actual -cne $binding.sha256) { throw 'Failure image archive changed bytes.' }
        }
        } finally {
            if ($archive) { $archive.Dispose() }
            for ($i=$imagePins.Count-1; $i -ge 0; $i--) { $imagePins[$i].Dispose() }
        }
        Write-TestRunEvidenceRecord (Join-Path $Run.Path ($labelName+'-images.identity.json')) @{archive=($labelName+'-images.zip');files=$bindings}
    }
    } finally { for ($i=$parentPins.Count-1; $i -ge 0; $i--) { $parentPins[$i].Dispose() } }
    }
}

function Get-TestRunEvidenceInventory([string] $Path, [Collections.Generic.List[IDisposable]] $Pins) {
        $rows = [Collections.Generic.List[object]]::new()
        $queue = [Collections.Generic.Queue[string]]::new()
        $queue.Enqueue($Path)
        while ($queue.Count) {
            $directory = $queue.Dequeue()
            $pins.Add([TestRunEvidenceV2.Native]::Pin($directory, $true))
            foreach ($item in Get-ChildItem -LiteralPath $directory -Force) {
                $isDirectory = [bool]($item.Attributes -band [IO.FileAttributes]::Directory)
                $handle = [TestRunEvidenceV2.Native]::Pin($item.FullName, $isDirectory)
                $pins.Add($handle)
                # Directory enumeration can cache NTFS attributes/times. The pinned handle is authoritative.
                $metadata = [TestRunEvidenceV2.Native]::Metadata($handle)
                $row = [ordered]@{ path=$item.FullName.Substring($Path.Length+1); directory=$isDirectory;
                    identity=[TestRunEvidenceV2.Native]::Identity($handle); attributes=$metadata.Attributes;
                    createdTicks=$metadata.CreatedTicks; writtenTicks=$metadata.WrittenTicks;
                    security=(Get-Acl -LiteralPath $item.FullName).Sddl }
                if ($isDirectory) { $queue.Enqueue($item.FullName) }
                else { $row.streams = @([TestRunEvidenceV2.Native]::Streams($item.FullName) | Sort-Object Name -CaseSensitive) }
                $rows.Add([pscustomobject]$row)
            }
        }
    return ,@($rows | Sort-Object path -CaseSensitive)
}

function Complete-TestRunEvidence {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Run, [Parameter(Mandatory)][bool] $Passed)
    if ($Run.Closed) { throw 'Run evidence has already been closed.' }
    if ($Run.Owner -ne $PID -or $Run.OwnerStarted -ne (Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks -or
        $Run.Directory -cne (Join-Path $Run.Repository ".build\evidence\test-runs\$($Run.Id)") -or
        $Run.Path -cne (Join-Path $Run.Directory 'payload')) { throw 'Evidence ownership or path mismatch.' }
    $pins = [Collections.Generic.List[IDisposable]]::new()
    try {
        foreach ($anchor in $Run.Anchors) {
            $pin = [TestRunEvidenceV2.Native]::Pin($anchor.path, $true)
            $pins.Add($pin)
            if ([TestRunEvidenceV2.Native]::Identity($pin) -cne $anchor.identity) { throw "Evidence directory identity changed: $($anchor.path)" }
        }
        if ($Run.CaptureNativeArtifacts) { Save-TestRunNativeArtifacts -Run $Run -Label 'native-artifacts' }
        $rows = Get-TestRunEvidenceInventory -Path $Run.Path -Pins $pins
        Write-TestRunEvidenceRecord (Join-Path $Run.Directory 'completed.json') ([ordered]@{
            schema=1; id=$Run.Id; outcome=$(if ($Passed) { 'passed' } else { 'failed-or-interrupted' });
            completedUtc=[DateTime]::UtcNow.ToString('o'); payload=@($rows)
        })
    } finally {
        for ($i=$pins.Count-1; $i -ge 0; $i--) { $pins[$i].Dispose() }
        $Run.Lock.Dispose()
        if ($Run.ArtifactLock) { $Run.ArtifactLock.Dispose() }
        for ($i=$Run.Pins.Count-1; $i -ge 0; $i--) { $Run.Pins[$i].Dispose() }
        $Run.Closed=$true
    }
}

# Preserve an in-flight runner failure while still reporting evidence finalization errors.
function Complete-TestRunEvidenceAfterFailure {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Run, [Parameter(Mandatory)][bool] $Passed, [System.Management.Automation.ErrorRecord] $PrimaryFailure)
    try { Complete-TestRunEvidence -Run $Run -Passed $Passed }
    catch {
        if ($null -eq $PrimaryFailure) { throw }
        Write-Warning -WarningAction Continue "Test run evidence finalization also failed: $($_.Exception.Message)"
    }
}
function Publish-TestRunEvidenceReceipt {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Run, [Parameter(Mandatory)][string] $Name, [Parameter(Mandatory)] $Receipt)
    if ($Name -notmatch '^[A-Za-z0-9_. -]+\.json$' -or $Run.Closed) { throw 'Invalid receipt publication.' }
    $archive = Join-Path $Run.Path $Name
    Write-TestRunEvidenceRecord $archive $Receipt
    $reports = Join-Path $Run.Repository '.build\reports'
    [void][IO.Directory]::CreateDirectory($reports)
    $pin = [TestRunEvidenceV2.Native]::Pin($reports, $true)
    $publication = $null
    try {
        # Serialize only the small compatibility view. Runs and their payloads never share writable output.
        $publication = [TestRunEvidenceV2.Native]::Lock((Join-Path $reports '.evidence-publication.lock'))
        $latest = Join-Path $reports $Name
        if (Test-Path -LiteralPath $latest) {
            $file = [TestRunEvidenceV2.Native]::Pin($latest, $false)
            try { $before = [TestRunEvidenceV2.Native]::Streams($latest) | ConvertTo-Json -Depth 4 -Compress } finally { $file.Dispose() }
            $previous = Join-Path $Run.Path 'previous-receipts'
            [void][IO.Directory]::CreateDirectory($previous)
            $destination = Join-Path $previous ($Name + '.' + [guid]::NewGuid().ToString('N'))
            [IO.File]::Move($latest, $destination)
            if ($before -cne ([TestRunEvidenceV2.Native]::Streams($destination) | ConvertTo-Json -Depth 4 -Compress)) {
                throw "Previous receipt failed preservation verification: $destination"
            }
        }
        # This JSON is a disposable compatibility view; the run's original receipt and logs remain under evidence.
        [IO.File]::Copy($archive, $latest, $false)
        if ((Get-FileHash -LiteralPath $archive).Hash -cne (Get-FileHash -LiteralPath $latest).Hash) { throw 'Receipt publication failed readback.' }
    } finally { if ($publication) { $publication.Dispose() }; $pin.Dispose() }
}

Export-ModuleMember -Function Start-TestRunEvidence, Complete-TestRunEvidence, Complete-TestRunEvidenceAfterFailure, Publish-TestRunEvidenceReceipt
