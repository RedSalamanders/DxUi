<# .SYNOPSIS Proves the trace capture tool (Tools/TraceCapture.psm1, Tools/Capture-DxUiTrace.ps1) with a fake runner: the
logman and tracerpt command lines, the provider identity shared with src/Support/Trace.h, and its refusals. No ETW session is
started. #>
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $root 'Tools/TraceCapture.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force

function New-FakeRunner([int] $ExitCode = 0, [string] $Output = 'The command completed successfully.') {
    $calls = [Collections.Generic.List[object]]::new()
    $runner = { param($tool, $arguments) $calls.Add([pscustomobject]@{ Tool = $tool; Arguments = @($arguments) }); [pscustomobject]@{ ExitCode = $ExitCode; Output = $Output } }.GetNewClosure()
    [pscustomobject]@{ Calls = $calls; Runner = $runner }
}

Invoke-TestCase 'the provider identity matches the native provider definition' {
    $provider = Get-DxUiTraceProvider
    $header = Get-Content -LiteralPath (Join-Path $root 'src/Support/Trace.h') -Raw
    $source = Get-Content -LiteralPath (Join-Path $root 'src/Foundation/Trace.cpp') -Raw
    Assert-Equal 'DxUi' $provider.Name 'provider name'
    Assert-True ($header.Contains('kProviderName[] = "DxUi"')) 'the header names the provider DxUi'
    $g = [guid]$provider.Guid
    $bytes = $g.ToByteArray()
    $headerGuid = '{{0x{0:x8}u, 0x{1:x4}u, 0x{2:x4}u, {{0x{3:x2}u, 0x{4:x2}u, 0x{5:x2}u, 0x{6:x2}u, 0x{7:x2}u, 0x{8:x2}u, 0x{9:x2}u, 0x{10:x2}u}}}}' -f `
        [BitConverter]::ToUInt32($bytes, 0), [BitConverter]::ToUInt16($bytes, 4), [BitConverter]::ToUInt16($bytes, 6), $bytes[8], $bytes[9], $bytes[10], $bytes[11], $bytes[12], $bytes[13], $bytes[14], $bytes[15]
    Assert-True ($header.Contains("kProviderId$headerGuid")) "the header's GUID is $($provider.Guid)"
    Assert-True ($source.Contains('"DxUi", (0x89b12dac, 0xc17b, 0x5934, 0xba, 0xa0, 0x35, 0xb9, 0xf9, 0x49, 0x90, 0xe6)')) 'the provider definition uses the same name and GUID'
    # The EventSource name hash, so tools that enable a provider by name derive the same GUID.
    $name = [Text.Encoding]::BigEndianUnicode.GetBytes('DXUI')
    $namespace = [byte[]](0x48, 0x2C, 0x2D, 0xB2, 0xC3, 0x90, 0x47, 0xC8, 0x87, 0xF8, 0x1A, 0x15, 0xBF, 0xC1, 0x30, 0xFB)
    $hash = [Security.Cryptography.SHA1]::Create().ComputeHash([byte[]]($namespace + $name))
    $guidBytes = [byte[]]$hash[0..15]
    $guidBytes[7] = ($guidBytes[7] -band 0x0F) -bor 0x50
    Assert-Equal ([guid]::new($guidBytes)) $g 'the GUID is the name hash of DxUi'
}

Invoke-FixtureCase 'start runs logman with the provider, its keywords and level, the QPC clock and the trace file' {
    param($fixture)
    $fake = New-FakeRunner
    $etl = Join-Path $fixture 'traces/run.etl'
    $session = Start-DxUiTrace -EtlPath $etl -SessionName 'DxUiProbe' -Runner $fake.Runner
    Assert-Equal 1 $fake.Calls.Count 'one command'
    Assert-Equal 'logman.exe' $fake.Calls[0].Tool 'logman starts the session'
    Assert-Equal ((@('start', 'DxUiProbe', '-ets', '-p', '{89b12dac-c17b-5934-baa0-35b9f94990e6}', '0x7', '0x4', '-o', [IO.Path]::GetFullPath($etl), '-ct', 'perf')) -join ' ') ($fake.Calls[0].Arguments -join ' ') 'logman arguments'
    Assert-True (Test-Path -LiteralPath (Join-Path $fixture 'traces') -PathType Container) 'the trace directory exists'
    Assert-Equal ([IO.Path]::GetFullPath($etl)) $session.EtlPath 'the session reports its file'
}

Invoke-FixtureCase 'stop stops the session and decodes the trace to XML beside it, or not when asked' {
    param($fixture)
    $fake = New-FakeRunner
    $etl = Join-Path $fixture 'run.etl'
    $result = Stop-DxUiTrace -EtlPath $etl -SessionName 'DxUiProbe' -Runner $fake.Runner
    Assert-Equal 2 $fake.Calls.Count 'stop and decode'
    Assert-Equal 'logman.exe stop DxUiProbe -ets' ("$($fake.Calls[0].Tool) $($fake.Calls[0].Arguments -join ' ')") 'logman stops the session'
    $xml = [IO.Path]::ChangeExtension([IO.Path]::GetFullPath($etl), '.xml')
    Assert-Equal "tracerpt.exe $([IO.Path]::GetFullPath($etl)) -o $xml -of XML -y" ("$($fake.Calls[1].Tool) $($fake.Calls[1].Arguments -join ' ')") 'tracerpt decodes to XML'
    Assert-Equal $xml $result.XmlPath 'the decoded path is reported'
    $quiet = New-FakeRunner
    $result = Stop-DxUiTrace -EtlPath $etl -SessionName 'DxUiProbe' -NoDecode -Runner $quiet.Runner
    Assert-Equal 1 $quiet.Calls.Count 'no decode when asked'
    Assert-True ($null -eq $result.XmlPath) 'no decoded path'
}

Invoke-FixtureCase 'a refused session says why, and other failures report the tool output' {
    param($fixture)
    $denied = New-FakeRunner -ExitCode 5 -Output 'Access is denied.'
    $message = ''
    try { Start-DxUiTrace -EtlPath (Join-Path $fixture 'a.etl') -Runner $denied.Runner | Out-Null } catch { $message = $_.Exception.Message }
    Assert-True ($message -match 'lacks the right to trace') "a refusal names the missing right: $message"
    $failed = New-FakeRunner -ExitCode 1 -Output 'Data Collector Set already exists.'
    $message = ''
    try { Stop-DxUiTrace -EtlPath (Join-Path $fixture 'a.etl') -Runner $failed.Runner | Out-Null } catch { $message = $_.Exception.Message }
    Assert-True ($message -match 'already exists' -and $message -match 'exit 1') "a failure carries the tool's output and exit code: $message"
}

Invoke-TestCase 'invalid session names, non-etl files and drive-root files are refused before any command runs' {
    $fake = New-FakeRunner
    Assert-Throws { Start-DxUiTrace -EtlPath (Join-Path $root '.build/x.etl') -SessionName 'bad name;' -Runner $fake.Runner } 'a session name with spaces or separators'
    Assert-Throws { Start-DxUiTrace -EtlPath (Join-Path $root '.build/x.txt') -Runner $fake.Runner } 'a file that is not .etl'
    $driveRoot = [IO.Path]::GetPathRoot($root)
    Assert-Throws { Start-DxUiTrace -EtlPath (Join-Path $driveRoot 'x.etl') -Runner $fake.Runner } 'a trace at a drive root'
    Assert-Equal 0 $fake.Calls.Count 'nothing ran'
}

Complete-TestRun 'Test-TraceCapture'
