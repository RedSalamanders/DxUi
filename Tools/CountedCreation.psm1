<#
.SYNOPSIS
Finds native resource creation under src that bypasses the counted helpers of src/Support/CountedCreation.h.

.DESCRIPTION
The work counters (Specs/Core/Core_PerformanceAndResources.md, "Work counters") count each text layout, text format,
rendering-parameter set, geometry, brush, bitmap, effect, layer, command list, device context, D3D11 device, implicit DrawText layout and GDI
backdrop capture the library creates. They count only what goes through the helpers, so any other occurrence of a
counted method name is a finding: a call through `->`, `.` or `::`, a call split across lines, a member-function
pointer, or a function pointer such as `&::D3D11CreateDevice`. Comments, string literals and character literals are
ignored; longer identifiers that merely contain a name (CountedCreateTextLayout) are not matches.
#>
Set-StrictMode -Version Latest

# Every counted name. Keep this list equal to the helpers of src/Support/CountedCreation.h.
$script:CountedNames = @(
    'BitBlt', 'CreateBitmap', 'CreateBitmapBrush', 'CreateBitmapFromDxgiSurface', 'CreateBitmapFromWicBitmap', 'CreateCommandList',
    'CreateCustomRenderingParams', 'CreateDeviceContext', 'CreateEffect', 'CreateEllipseGeometry', 'CreateGdiCompatibleTextLayout',
    'CreateGeometryGroup', 'CreateGradientStopCollection', 'CreateImageBrush', 'CreateLayer', 'CreateLinearGradientBrush',
    'CreateMonitorRenderingParams', 'CreatePathGeometry',
    'CreateRadialGradientBrush', 'CreateRectangleGeometry', 'CreateRoundedRectangleGeometry', 'CreateSharedBitmap',
    'CreateSolidColorBrush', 'CreateStrokeStyle', 'CreateTextFormat', 'CreateTextLayout', 'CreateTransformedGeometry',
    'D3D11CreateDevice', 'DrawText', 'DrawTextW'
)

# The only files that may name a counted method directly, and which names: the helpers themselves, and the D3D11 device
# creator type whose every call is counted inside Detail::CreateD3D11DeviceWithWarpFallback.
$script:AllowedNames = @{
    'src/Support/CountedCreation.h' = $script:CountedNames
    'src/Support/NativeRendering.h' = @('D3D11CreateDevice')
}

function Get-CountedCreationNames { $script:CountedNames }

# Comments and literals, leftmost first: a raw string (any encoding prefix), a line or block comment, a string literal, or a
# character literal that does not follow a digit or identifier character (so 1'000 is a digit separator while L'x' is a
# literal). An unterminated literal ends at its line.
$script:LiteralPattern = [regex]::new(
    'R"(?<delimiter>[^()\\\s"]{0,16})\((?s:.*?)\)\k<delimiter>"' +
    '|//[^\n]*' +
    '|/\*(?s:.*?)(?:\*/|\z)' +
    '|"(?:\\.|[^"\\\n])*(?:"|$)' +
    "|(?<=^|[^0-9A-Za-z_]|(?<![0-9A-Za-z_])(?:L|u|U|u8))'(?:\\.|[^'\\\n])*(?:'|$)",
    [Text.RegularExpressions.RegexOptions]::Multiline)

# The source with every comment, string literal and character literal replaced by spaces; line breaks are kept, so line
# numbers still match.
function Remove-CppCommentsAndLiterals([Parameter(Mandatory)][AllowEmptyString()][string] $Text) {
    $script:LiteralPattern.Replace($Text, [Text.RegularExpressions.MatchEvaluator] { param($match) [regex]::Replace($match.Value, '[^\r\n]', ' ') })
}

# Findings in one source text: each occurrence of a counted name that its path may not use. RelativePath uses '/'.
function Find-UncountedCreationInText([Parameter(Mandatory)][string] $RelativePath, [Parameter(Mandatory)][AllowEmptyString()][string] $Text) {
    $allowed = if ($script:AllowedNames.ContainsKey($RelativePath)) { @($script:AllowedNames[$RelativePath]) } else { @() }
    $code = Remove-CppCommentsAndLiterals $Text
    $pattern = '(?<![A-Za-z0-9_])(' + (($script:CountedNames | Sort-Object Length -Descending) -join '|') + ')(?![A-Za-z0-9_])'
    $lines = $code -split "`n"
    $findings = [Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt $lines.Count; $index++) {
        foreach ($match in [regex]::Matches($lines[$index], $pattern)) {
            $name = $match.Groups[1].Value
            if ($allowed -ccontains $name) { continue }
            $findings.Add([pscustomobject]@{ Path = $RelativePath; Line = $index + 1; Name = $name })
        }
    }
    $findings.ToArray()
}

# Findings in every C/C++ source and header under <Root>/src.
function Find-UncountedCreation([Parameter(Mandatory)][string] $Root) {
    $source = Join-Path $Root 'src'
    if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw "No src directory under $Root." }
    $rootPath = (Resolve-Path -LiteralPath $Root).ProviderPath.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $findings = [Collections.Generic.List[object]]::new()
    foreach ($file in Get-ChildItem -LiteralPath $source -Recurse -File -Include '*.h', '*.hpp', '*.cpp', '*.inl' | Sort-Object FullName) {
        $relative = $file.FullName.Substring($rootPath.Length).Replace('\', '/')
        foreach ($finding in Find-UncountedCreationInText $relative ([IO.File]::ReadAllText($file.FullName))) { $findings.Add($finding) }
    }
    $findings.ToArray()
}

Export-ModuleMember -Function Get-CountedCreationNames, Remove-CppCommentsAndLiterals, Find-UncountedCreationInText, Find-UncountedCreation
