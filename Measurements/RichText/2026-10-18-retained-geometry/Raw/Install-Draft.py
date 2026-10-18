from pathlib import Path
root = Path.cwd()
work = root / '.build/reports/rich-retained-geometry-20261018'
def replace_once(value, before, after):
    assert value.count(before) == 1, (before[:100], value.count(before))
    return value.replace(before, after, 1)
header = root / 'include/DxUi/RichTextLayout.h'
text = header.read_text()
text = replace_once(text, 'public:\n    void Prepare', 'public:\n    PreparedLayout();\n    ~PreparedLayout() noexcept;\n    PreparedLayout(const PreparedLayout&) = delete;\n    PreparedLayout& operator=(const PreparedLayout&) = delete;\n    void Prepare')
text = replace_once(text, '''    const Projection& Content() const noexcept
    {
        return projection_;
    }''', '    const Projection& Content() const noexcept { return projection_; }')
text = replace_once(text, 'return static_cast<bool>(layout_);', 'return static_cast<bool>(state_);')
text = replace_once(text, '''    float Height() const noexcept
    {
        return height_;
    }''', '    float Height() const noexcept;')
text = replace_once(text, '''    Projection projection_;
    wil::com_ptr<IDWriteTextLayout> layout_;
    std::vector<std::size_t> clusters_, lines_;
    float height_{};''', '''    struct State;
    // Keep the returned member object stable across preparation and retirement.
    Projection projection_;
    std::unique_ptr<State> state_;''')
text = replace_once(text, '} // namespace DxUi::RichText', (work/'Draft-DocumentLayout.h').read_text() + '\n} // namespace DxUi::RichText')
header.write_text(text)
file = root / 'src/Controls/DxUi.RichTextLayout.cpp'
text = file.read_text()
text = replace_once(text, '''void PreparedLayout::Clear() noexcept
{
    layout_.reset();
    projection_ = {};
    clusters_.clear();
    lines_.clear();
    height_ = 0;
    ++generation_;
}''', '''void PreparedLayout::Clear() noexcept
{
    // CLEAR_RETAINED_STATE
}''')
text = replace_once(text, '''    projection_ = std::move(projection);
    layout_     = std::move(layout);
    clusters_   = std::move(clusters);
    lines_      = std::move(lines);
    height_     = metrics.height;''', '''    auto next = std::make_unique<State>();
    projection_ = std::move(projection);
    next->layout = std::move(layout);
    next->clusters = std::move(clusters);
    next->lines = std::move(lines);
    next->height = metrics.height;
    state_.swap(next);''')
text = text.replace('! layout_', '! state_').replace('layout_->', 'state_->layout->').replace('layout_.get()', 'state_->layout.get()')
text = text.replace('projection_.', 'Content().').replace('clusters_', 'state_->clusters').replace('lines_', 'state_->lines')
text = replace_once(text, '    // CLEAR_RETAINED_STATE', '''    if (state_)
    {
        // Prepare constructs the empty retirement containers fallibly. Swapping
        // individual buffers here neither allocates nor moves a Debug proxy
        // through a noexcept default/move constructor. Destroy the old graph.
        projection_.source.swap(state_->retirement.source);
        projection_.text.swap(state_->retirement.text);
        projection_.sourceToDisplay.swap(state_->retirement.sourceToDisplay);
        projection_.units.swap(state_->retirement.units);
        projection_.runs.swap(state_->retirement.runs);
    }
    projection_.revision = 0u;
    projection_.mode = ViewMode::Source;
    state_.reset();
    ++generation_;''')
text = replace_once(text, 'if (result && Content().text[result - 1] ==', 'if (result > position && Content().text[result - 1] ==')
text = replace_once(text, '} // namespace\nvoid PreparedLayout::Prepare', '''} // namespace
struct PreparedLayout::State
{
    State() : retirement{std::wstring(0u, L'\\0'), std::wstring(0u, L'\\0'), std::vector<std::size_t>(0u), std::vector<SourceUnit>(0u), std::vector<StyleRun>(0u)}, clusters(0u), lines(0u) {}
    Projection retirement;
    wil::com_ptr<IDWriteTextLayout> layout;
    std::vector<std::size_t> clusters, lines;
    float height{};
};
PreparedLayout::PreparedLayout()
    : projection_{std::wstring(0u, L'\\0'), std::wstring(0u, L'\\0'), std::vector<std::size_t>(0u),
             std::vector<SourceUnit>(0u), std::vector<StyleRun>(0u)}
{
    // Size constructors propagate Debug STL proxy failures. Retirement never
    // default-constructs containers inside a noexcept callback.
}
PreparedLayout::~PreparedLayout() noexcept = default;

float PreparedLayout::Height() const noexcept { return state_ ? state_->height : 0.0f; }
void PreparedLayout::Prepare''')
file.write_text(text)
header = root / 'include/DxUi/RichMarkdown.h'
text = header.read_text()
text = replace_once(text, '    void Prepare(SourceSnapshot source,', '    // Fallible index copy sharing immutable payloads, for joint geometry staging.\n    PreparedProjection Clone() const;\n    void Prepare(SourceSnapshot source,')
header.write_text(text)
file = root / 'src/Controls/DxUi.RichMarkdownProjection.cpp'
text = file.read_text()
text = replace_once(text, 'void PreparedProjection::Prepare(', '''PreparedProjection PreparedProjection::Clone() const
{
    PreparedProjection copy;
    if (state_)
        copy.state_ = std::make_unique<State>(*state_);
    return copy;
}
void PreparedProjection::Prepare(''')
text = replace_once(text, '    std::wstring result;\n    result.reserve(DisplayLength());', "    std::wstring result(0u, L'\\0');\n    result.reserve(DisplayLength());")
file.write_text(text)
(root / 'src/Controls/DxUi.RichDocumentLayout.cpp').write_text((work/'Draft-DocumentLayout.cpp').read_text())
(root / 'Tests/Controls/Scope.Tests.RichDocumentLayout.cpp').write_text((work/'Draft-DocumentLayoutTests.cpp').read_text())
(root / 'Tests/Controls/Scope.Tests.RichDocumentLayout.h').write_text('#pragma once\nvoid RunRichDocumentLayoutTests();\n')
(root / 'Tests/Embedded/Scope.Tests.RichGeometryResources.h').write_text((work/'Draft-GeometryResources.h').read_text())
file = root / 'src/Controls/DxUi.RichTextEditor.cpp'
text = file.read_text().replace('RichText::PreparedLayout layout;', 'RichText::PreparedDocumentLayout layout;')
text = replace_once(text, '''    if (sourceChanged)
        RetireLayout();''', '''    if (sourceChanged)
    {
        // Reject stale queries immediately, but retain bounded immutable chunks
        // for the next owner-thread preparation. Host/font/visibility retirement
        // still releases the complete graph through RetireLayout/Clear.
        _state->layout.Invalidate();
        _state->revealCaret = true;
    }''')
text = replace_once(text, 'RichText::Project(state.document.Snapshot(), state.document.Revision(), state.mode), options', 'state.document.Snapshot(), state.document.Revision(), state.mode, options')
text = text.replace('.layout.Content()', '.layout').replace('->layout.Content()', '->layout')
text = text.replace('projection.text.size()', 'projection.DisplayLength()').replace('projection.source.size()', 'projection.SourceLength()')
text = text.replace('projection.text[displayEnd]', 'projection.DisplayUnit(displayEnd)')
text = text.replace('! projection.text.empty() && projection.text.back()', 'projection.DisplayLength() && projection.DisplayUnit(projection.DisplayLength() - 1u)')
text = replace_once(text, '''static_cast<size_t>(std::lower_bound(projection.sourceToDisplay.begin(), projection.sourceToDisplay.end(), displayEnd) -
                                          projection.sourceToDisplay.begin())''', 'projection.SourceBoundary(displayEnd)')
text = text.replace('input.text == projection.source', 'projection.SourceMatches(input.text)')
text = text.replace('projection.text', 'projection.DisplayText()')
text = text.replace('state.layout.Draw(context, textBrush, origin);', 'state.layout.Draw(context, textBrush, origin, state.scrollDip, state.scrollDip + viewport.bottom - viewport.top);')
text = text.replace('state.layout.Draw(context, selectedTextBrush, origin);', 'state.layout.Draw(context, selectedTextBrush, origin, rectangle.top, rectangle.bottom);')
file.write_text(text)
file = root / 'src/DxUi.vcxproj'
text = replace_once(file.read_text(), '<ClCompile Include="Controls\\DxUi.RichTextLayout.cpp" />', '<ClCompile Include="Controls\\DxUi.RichTextLayout.cpp" /><ClCompile Include="Controls\\DxUi.RichDocumentLayout.cpp" />')
file.write_text(text)
file = root / 'Tests/Controls/DxUi.ControlTests.vcxproj'
text = replace_once(file.read_text(), '<ClCompile Include="Scope.Tests.RichTextLayout.cpp" />', '<ClCompile Include="Scope.Tests.RichTextLayout.cpp" /><ClCompile Include="Scope.Tests.RichDocumentLayout.cpp" />')
file.write_text(text)
file = root / 'Tests/Controls/DxUi.Tests.EditorControls.cpp'
text = replace_once(file.read_text(), '#include "Scope.Tests.RichTextLayout.h"', '#include "Scope.Tests.RichTextLayout.h"\n#include "Scope.Tests.RichDocumentLayout.h"')
text = replace_once(text, '    RunRichTextLayoutTests();', '    RunRichTextLayoutTests();\n    RunRichDocumentLayoutTests();')
file.write_text(text)
import json
file = root / 'Tests/native-test-files.json'
entries = json.loads(file.read_text())
assert isinstance(entries,list)
entries += ['Tests/Controls/Scope.Tests.RichDocumentLayout.cpp','Tests/Controls/Scope.Tests.RichDocumentLayout.h','Tests/Embedded/Scope.Tests.RichGeometryResources.h']
file.write_text(json.dumps(sorted(entries),indent=2)+'\n')
print('Installed bounded document geometry draft; standalone prototype and consumer pins untouched.')

