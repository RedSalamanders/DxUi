<# .SYNOPSIS Proves the counted-creation matcher (Tools/CountedCreation.psm1): src creates every counted native resource through
the helpers of src/Support/CountedCreation.h, and a raw creation is found whatever its receiver. #>
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $root 'Tools/CountedCreation.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force

function Find([string] $Text, [string] $Path = 'src/Controls/Probe.cpp') { , @(Find-UncountedCreationInText $Path $Text) }

Invoke-TestCase 'the repository source creates every counted resource through the helpers' {
    $findings = @(Find-UncountedCreation -Root $root)
    Assert-Equal 0 $findings.Count ("raw creation under src: " + (($findings | ForEach-Object { '{0}:{1} {2}' -f $_.Path, $_.Line, $_.Name }) -join '; '))
}

Invoke-TestCase 'a seeded raw CreateTextFormat call fails whatever its receiver' {
    foreach ($seed in @(
            'HRESULT hr = factory->CreateTextFormat(L"Segoe UI", nullptr, weight, style, stretch, 12.0f, L"", format.put());',
            'HRESULT hr = factory.CreateTextFormat(L"Segoe UI", nullptr, weight, style, stretch, 12.0f, L"", format.put());',
            'HRESULT hr = Typography::CreateTextFormat(factory, spec, format.put());',
            'HRESULT hr = CreateTextFormat(factory, spec, format.put());',
            "HRESULT hr = factory`r`n    ->CreateTextFormat (L`"Segoe UI`", nullptr, weight, style, stretch, 12.0f, L`"`", format.put());",
            'auto method = &IDWriteFactory::CreateTextFormat;')) {
        $findings = Find $seed
        Assert-Equal 1 $findings.Count "one finding for: $seed"
        Assert-Equal 'CreateTextFormat' $findings[0].Name "the finding names the method for: $seed"
    }
    Assert-Equal 2 (Find "int a;`nint b = x->CreateTextFormat(1);")[0].Line 'the finding reports its line'
}

Invoke-FixtureCase 'a source tree with one seeded raw CreateTextFormat call fails the repository scan' {
    param($fixture)
    $controls = Join-Path $fixture 'src/Controls'
    New-Item -ItemType Directory -Force -Path $controls | Out-Null
    [IO.File]::WriteAllText((Join-Path $controls 'Counted.cpp'), "HRESULT hr = CountedCreateTextFormat(factory, L`"Segoe UI`", nullptr, w, s, t, 12.0f, L`"`", format.put());`n")
    Assert-Equal 0 @(Find-UncountedCreation -Root $fixture).Count 'a tree that uses the helper passes'
    [IO.File]::WriteAllText((Join-Path $controls 'Seeded.cpp'), "// seeded`nHRESULT hr = factory->CreateTextFormat(L`"Segoe UI`", nullptr, w, s, t, 12.0f, L`"`", format.put());`n")
    $findings = @(Find-UncountedCreation -Root $fixture)
    Assert-Equal 1 $findings.Count 'the seeded raw call is the one finding'
    Assert-Equal 'src/Controls/Seeded.cpp:2 CreateTextFormat' ('{0}:{1} {2}' -f $findings[0].Path, $findings[0].Line, $findings[0].Name) 'the finding names its file, line and method'
}

Invoke-TestCase 'function pointers and every counted method name are found' {
    Assert-Equal 'D3D11CreateDevice' (Find 'return Create(flags, &::D3D11CreateDevice);')[0].Name 'a D3D11CreateDevice function pointer'
    Assert-Equal 'D3D11CreateDevice' (Find 'using Creator = decltype(&D3D11CreateDevice);')[0].Name 'a D3D11CreateDevice type'
    Assert-Equal 'DrawTextW' (Find 'dc->DrawTextW(text, length, format, rect, brush);')[0].Name 'an implicit DrawText layout'
    Assert-Equal 'BitBlt' (Find 'BitBlt(memory, 0, 0, w, h, screen, x, y, SRCCOPY);')[0].Name 'a GDI capture'
    foreach ($name in Get-CountedCreationNames) {
        Assert-Equal 1 (Find "hr = receiver->$name(argument);").Count "receiver call of $name"
    }
}

Invoke-TestCase 'helpers, longer identifiers, comments and literals are not findings' {
    foreach ($clean in @(
            'HRESULT hr = CountedCreateTextFormat(factory, L"Segoe UI", nullptr, weight, style, stretch, 12.0f, L"", format.put());',
            'HRESULT hr = CountedCreateTypographyTextFormat(factory, spec, format.put());',
            'CountedDrawText(dc, text, length, format, rect, brush);',
            'void DrawTextWithMnemonic(ControlHost& host); CreateTextFormatWithStyle(factory);',
            '// factory->CreateTextLayout(text, length, format, width, height, layout.put());',
            "/* dc->CreateEffect(id, effect.put());`n   dc->CreateLayer(layer.put()); */",
            'Debug::Warning(L"CreateTextFormat failed: 0x{:08X}", hr);',
            'const char* name = R"x(factory->CreateTextLayout(")x";',
            "const wchar_t quote = L'`"'; auto n = 1'000'000; // CreateBitmap",
            'const char* escaped = "\"->CreateBitmap(\"";')) {
        Assert-Equal 0 (Find $clean).Count "no finding for: $clean"
    }
}

Invoke-TestCase 'only the helper and creator-type headers may name counted methods, and only those names' {
    Assert-Equal 0 (Find 'return receiver->CreateTextFormat(args...);' 'src/Support/CountedCreation.h').Count 'the helpers themselves'
    Assert-Equal 0 (Find 'using D3D11DeviceCreator = decltype(&::D3D11CreateDevice);' 'src/Support/NativeRendering.h').Count 'the creator type'
    Assert-Equal 1 (Find 'factory->CreateTextLayout(text, length, format, w, h, out);' 'src/Support/NativeRendering.h').Count 'the creator-type header may not create other resources'
}

Complete-TestRun 'Counted creation matcher'
