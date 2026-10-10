<#
.SYNOPSIS Adds scratch-only sabotage switches (DXUI_ROWLAYOUT_SABOTAGE=n) to the one-layout-per-row implementation.
.DESCRIPTION Every edit is uncommitted: `git checkout -- src` removes all of them. Used to falsify the new tests with one build.
  1 no spacer calibration          2 brush never rebound to a new device   3 brush color never set per row
  4 no tab stop override           5 no color-font option                 6 description drawn in the label's color
  7 small format line spacing differs (host)                               9 drawing effect never bound
 11 row layout counter counts two per row                               12 row height from the unrounded label height
#>
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..\..')).Path
$utf8 = New-Object System.Text.UTF8Encoding($false)
$crlf = "`r`n"
function Edit-File([string] $path, [scriptblock] $edit) {
    $text = [IO.File]::ReadAllText($path)
    $script:text = $text
    & $edit
    [IO.File]::WriteAllText($path, $script:text, $utf8)
}
function Rep([string] $old, [string] $new) {
    if (-not $script:text.Contains($old)) { throw "anchor missing: $old" }
    $script:text = $script:text.Replace($old, $new)
}
Edit-File (Join-Path $repo 'src\Controls\DxUi.Menu.cpp') {
    Rep ('// The size of one field of a described row, as its layout reports it.') ('[[nodiscard]] int RowLayoutSabotage() noexcept' + $crlf + '{' + $crlf + '    static const int value = []() noexcept' + $crlf + '    {' + $crlf + '        wchar_t buffer[8]{};' + $crlf + '        const DWORD n = GetEnvironmentVariableW(L"DXUI_ROWLAYOUT_SABOTAGE", buffer, 8);' + $crlf + '        return n > 0 && n < 8 ? _wtoi(buffer) : 0;' + $crlf + '    }();' + $crlf + '    return value;' + $crlf + '}' + $crlf + $crlf + '// The size of one field of a described row, as its layout reports it.')
    Rep '(font.overridesTabStop && FAILED(layout->SetIncrementalTabStop(font.tabStop))) ||' '(font.overridesTabStop && RowLayoutSabotage() != 4 && FAILED(layout->SetIncrementalTabStop(font.tabStop))) ||'
    Rep '        const float wantedHeight = std::ceil(primary.height) - primary.height + kDescriptionGapDip;' ('        if (RowLayoutSabotage() == 1)' + $crlf + '        {' + $crlf + '            row.primaryMetrics   = primary;' + $crlf + '            row.secondaryMetrics = secondary;' + $crlf + '            row.heightDip        = 2.0f * kDescriptionPaddingDip + std::ceil(primary.height) + kDescriptionGapDip + std::ceil(secondary.height);' + $crlf + '            return true;' + $crlf + '        }' + $crlf + '        const float wantedHeight = std::ceil(primary.height) - primary.height + kDescriptionGapDip;')
    Rep '            row.heightDip        = 2.0f * kDescriptionPaddingDip + std::ceil(primary.height) + kDescriptionGapDip + std::ceil(secondary.height);' '            row.heightDip        = 2.0f * kDescriptionPaddingDip + (RowLayoutSabotage() == 12 ? primary.height : std::ceil(primary.height)) + kDescriptionGapDip + std::ceil(secondary.height);'
    Rep '            ++layoutCount;' '            layoutCount += RowLayoutSabotage() == 11 ? 2u : 1u;'
    Rep '    if (! popup.descriptionBrush || popup.descriptionBrushDevice.get() != device.get())' '    if (! popup.descriptionBrush || (RowLayoutSabotage() != 2 && popup.descriptionBrushDevice.get() != device.get()))'
    Rep '    popup.descriptionBrush->SetColor(color);' ('    if (RowLayoutSabotage() != 3)' + $crlf + '        popup.descriptionBrush->SetColor(color);')
    Rep '    if (row.descriptionBrush != popup.descriptionBrush.get())' '    if (RowLayoutSabotage() != 9 && row.descriptionBrush != popup.descriptionBrush.get())'
    Rep 'BindMenuDescriptionBrush(dc, popup, row, secondaryColor))' 'BindMenuDescriptionBrush(dc, popup, row, RowLayoutSabotage() == 6 ? primaryColor : secondaryColor))'
    Rep 'row.layout.get(), primaryBrush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);' 'row.layout.get(), primaryBrush, RowLayoutSabotage() == 5 ? D2D1_DRAW_TEXT_OPTIONS_NONE : D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);'
}
Edit-File (Join-Path $repo 'src\Controls\DxUi.WindowHost.cpp') {
    $anchor = '        const HRESULT hr            = Typography::CreateTextFormat(_dwriteFactory.get(), spec, _smallTextFormat.addressof());'
    Rep $anchor ($anchor + $crlf + '        {' + $crlf + '            wchar_t sabotage[8]{};' + $crlf + '            if (GetEnvironmentVariableW(L"DXUI_ROWLAYOUT_SABOTAGE", sabotage, 8) > 0 && _wtoi(sabotage) == 7 && _smallTextFormat)' + $crlf + '                static_cast<void>(_smallTextFormat->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, 18.0f, 14.0f));' + $crlf + '        }')
}
Push-Location $repo
git diff --stat
Pop-Location

