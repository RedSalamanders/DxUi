[CmdletBinding()] param([string]$Root)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$path=Join-Path $Root 'src/Controls/DxUi.RichMarkdownProjection.cpp'
$text=[IO.File]::ReadAllText($path).Replace("`r`n","`n")
$first=$text.IndexOf('    Projection result{')
$end=$text.IndexOf('    void Hide(',$first)
if($first -lt 0 -or $end -le $first){throw 'Unexpected projection builder storage.'}
$text=$text.Remove($first,$end-$first).Insert($first,"    explicit Builder(Projection& projection) noexcept : result(projection) {}`n    Projection& result;`n")
$text=$text.Replace('Projection ProjectText(std::wstring text, std::uint64_t revision, ViewMode mode)',
 'void FillProjection(Projection& result, std::wstring&& text, std::uint64_t revision, ViewMode mode)')
$text=[regex]::Replace($text,'    Builder builder;\n    auto& result\s*= builder.result;','    Builder builder(result);')
$text=$text.Replace('    return std::as_const(result); // Copy avoids allocating Debug proxies inside noexcept string moves.','')
$helpers=@'
// Use throwing size constructors for Debug STL proxy failures. Block preparation
// copies this empty value into owned staging, then fills it directly: no complete
// source/display/mapping payload is moved or copied into another Projection.
Projection MakeEmptyProjection()
{
    return {std::wstring(0u, L'\0'), std::wstring(0u, L'\0'), std::vector<std::size_t>(0u),
            std::vector<SourceUnit>(0u), std::vector<StyleRun>(0u), 0u, ViewMode::Source};
}
std::shared_ptr<const Projection> ProjectBlock(std::wstring&& text, ViewMode mode)
{
    const auto empty = MakeEmptyProjection();
    auto staged = std::make_shared<Projection>(empty);
    FillProjection(*staged, std::move(text), 0u, mode);
    return staged;
}
'@
$anchor="} // namespace`nProjection Project(SourceSnapshot source, std::uint64_t revision, ViewMode mode)"
if(-not$text.Contains($anchor)){throw 'Missing projection namespace boundary.'}
$text=$text.Replace($anchor,$helpers+"`n"+$anchor)
$text=$text.Replace('    return ProjectText(source.Text(), revision, mode);',
 "    auto result = MakeEmptyProjection();`n    FillProjection(result, source.Text(), revision, mode);`n    return result;")
$text=[regex]::Replace($text,'        const auto local = ProjectText\(changedText.substr\(begin, end - begin\), 0u, mode\);\n        auto content\s*= std::make_shared<const Projection>\(local\);',
 '        auto content = ProjectBlock(changedText.substr(begin, end - begin), mode);')
$text=$text.Replace("        const auto local = ProjectText(std::wstring(0u, L'\0'), 0u, mode);`n        next->blocks.push_back({0u, 0u, std::make_shared<const Projection>(local)});",
 "        next->blocks.push_back({0u, 0u, ProjectBlock(std::wstring(0u, L'\0'), mode)});")
if($text.Contains('ProjectText(')){throw 'A draft copying path remains.'}
[IO.File]::WriteAllText($path,$text.Replace("`n","`r`n"))
Write-Host 'Canonical full projection and block staging fill their final owned payload directly. Reuse remains disabled.'
