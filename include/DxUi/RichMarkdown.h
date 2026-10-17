#pragma once
#include <DxUi/RichDocument.h>
namespace DxUi::RichText
{
enum class ViewMode
{
    Source,
    Rich
};
enum Style : unsigned
{
    Regular = 0,
    Bold    = 1,
    Italic  = 2,
    Code    = 4,
    Strike  = 8
};
struct StyleRun
{
    std::size_t start{}, length{};
    unsigned flags{};
    unsigned heading{};
};
struct SourceUnit
{
    std::size_t start{}, end{};
};
struct Projection
{
    std::wstring source, text;
    std::vector<std::size_t> sourceToDisplay;
    std::vector<SourceUnit> units;
    std::vector<StyleRun> runs;
    std::uint64_t revision{};
    ViewMode mode{};
    std::size_t ToDisplay(std::size_t sourceOffset) const;
    // At a hidden delimiter the same display boundary has two source affinities.
    // A trailing glyph hit stays before its closing delimiter.
    std::size_t ToSource(std::size_t displayOffset, bool trailing = false) const;
};
// Version 0 prototype grammar, explicitly not CommonMark/GFM conformance.
Projection Project(SourceSnapshot source, std::uint64_t revision, ViewMode mode);
bool ToggleMarker(Document& document, std::wstring_view marker);
} // namespace DxUi::RichText
