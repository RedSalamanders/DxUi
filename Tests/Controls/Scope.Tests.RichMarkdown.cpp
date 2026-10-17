#include "Scope.Tests.RichMarkdown.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include <DxUi/RichMarkdown.h>
namespace
{
using namespace DxUi::RichText;
template <typename F> void Reject(F&& operation, const char* description)
{
    bool rejected = false;
    try
    {
        operation();
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    Require(rejected, description);
}
void TestRichMarkdownPrototypeContracts()
{
    Document document(L"# Title\r\nA **bold** and *italic* with "
                      L"`code`.\r\nUnknown $x$ and [^note].");
    const auto original = document.Text();
    const auto revision = document.Revision();
    auto rich           = Project(document.Snapshot(), revision, ViewMode::Rich);
    Require(rich.text == L"Title\nA bold and italic with code.\nUnknown $x$ and [^note].", "defined rich projection");
    Require(document.Text() == original && document.Revision() == revision, "view creation does not edit source");
    Require(rich.runs.front().heading == 1, "heading style");
    const auto bold = rich.text.find(L"bold"), source = original.find(L"bold");
    Require(rich.ToSource(bold) == source, "leading source mapping");
    Require(rich.ToSource(bold + 4, true) == source + 4, "trailing closing-delimiter affinity");
    document.SetSelection({rich.ToSource(bold), rich.ToSource(bold + 4, true)});
    document.ReplaceSelection(L"strong");
    Require(document.Text().find(L"**strong**") != std::wstring::npos, "rich edit retains surrounding delimiter");
    document.Undo();
    Require(document.Text() == original, "rich edit undo preserves exact source");
    auto sourceView = Project(document.Snapshot(), document.Revision(), ViewMode::Source);
    Require(sourceView.text.find(L"**bold**") != std::wstring::npos, "source view keeps delimiters");
    for (std::size_t i = 0; i < rich.sourceToDisplay.size(); ++i)
    {
        Require(rich.sourceToDisplay[i] <= rich.text.size(), "all source offsets mapped");
        if (i)
            Require(rich.sourceToDisplay[i] >= rich.sourceToDisplay[i - 1], "mapping monotonic");
    }
    Document simple(L"hello world");
    simple.SetSelection({0, 5});
    ToggleMarker(simple, L"**");
    Require(simple.Text() == L"**hello** world", "bold transaction");
    Require(simple.GetSelection() == Selection{2, 7}, "format keeps content selected");
    ToggleMarker(simple, L"**");
    Require(simple.Text() == L"hello world", "bold toggle removes delimiters");
    simple.Undo();
    Require(simple.Text() == L"**hello** world", "format undo");
    simple.Redo();
    Require(simple.Text() == L"hello world", "format redo");
    Document nested(L"**a *b* c** and `*literal*` and *unclosed");
    auto projection = Project(nested.Snapshot(), 1, ViewMode::Rich);
    Require(projection.text == L"a b c and *literal* and *unclosed", "nested formats literal code and incomplete input");
    bool combined = false;
    for (const auto& run : projection.runs)
        if ((run.flags & (Bold | Italic)) == (Bold | Italic))
            combined = true;
    Require(combined, "combined style runs");
    Document unicode(L"**\U0001f600\u0301**\r\n");
    auto u = Project(unicode.Snapshot(), 1, ViewMode::Rich);
    Require(u.text == L"\U0001f600\u0301\n", "supplementary and combining projection");
    Require(u.ToSource(3, true) == 5, "UTF-16 source positions");
    Document backwards(L"hello");
    backwards.SetSelection({5, 0});
    ToggleMarker(backwards, L"**");
    Require(backwards.GetSelection() == Selection{7, 2}, "format preserves backwards anchor/caret");
    ToggleMarker(backwards, L"**");
    Require(backwards.Text() == L"hello" && backwards.GetSelection() == Selection{5, 0}, "unformat preserves backwards selection");
    backwards.Undo();
    Require(backwards.GetSelection() == Selection{7, 2}, "undo preserves backwards selection");
    Document headingOnly(L"# ");
    auto emptyHeading = Project(headingOnly.Snapshot(), 1, ViewMode::Rich);
    Require(emptyHeading.text.empty() && emptyHeading.ToSource(0) == 2, "empty heading maps caret after its marker");
    Document empty;
    auto p = Project(empty.Snapshot(), 0, ViewMode::Rich);
    Require(p.ToSource(0) == 0 && p.ToDisplay(0) == 0, "empty source maps");
}
void TestRichMarkdownSnapshotAndValidationContracts()
{
    Document document(L"**old**");
    const auto old      = document.Snapshot();
    const auto revision = document.Revision();
    document.Reset(L"new");
    auto projection = Project(old, revision, ViewMode::Rich);
    Require(projection.text == L"old" && projection.revision == revision && projection.source == L"**old**",
            "worker snapshot/revision is explicit and immutable");
    Reject([&] { Project(old, revision, static_cast<ViewMode>(99)); }, "invalid public mode rejected");
    Reject([&] { projection.ToSource(999); }, "display mapping out of range rejected");
    Reject([&] { projection.ToDisplay(999); }, "source mapping out of range rejected");
    const auto before = document.Text();
    Reject([&] { ToggleMarker(document, L"not-a-supported-marker"); }, "unsupported formatting command rejected");
    Require(document.Text() == before && ! document.CanUndo(), "invalid format command is atomic");
}
void TestRichMarkdownV0StylesCommandsAndMalformedMappings()
{
    for (unsigned heading = 1; heading <= 6; ++heading)
    {
        Document document(std::wstring(heading, L'#') + L" Heading");
        const auto projection = Project(document.Snapshot(), document.Revision(), ViewMode::Rich);
        Require(projection.text == L"Heading" && projection.runs.front().heading == heading, "six supported heading levels");
    }
    for (const auto* literal : {L"####### literal", L"#no-space", L"***", L"`unclosed", L"[^note] $x$"})
    {
        Document document(literal);
        Require(Project(document.Snapshot(), document.Revision(), ViewMode::Rich).text == literal, "unsupported/incomplete v0 source retained");
    }
    Document strikes(L"~~gone~~ `**literal**`");
    auto projection = Project(strikes.Snapshot(), strikes.Revision(), ViewMode::Rich);
    Require(projection.text == L"gone **literal**" && projection.runs.front().flags == Strike, "strike and literal code");
    Document newlines(L"a\r\nb\rc\n");
    const auto sourceView = Project(newlines.Snapshot(), newlines.Revision(), ViewMode::Source);
    Require(sourceView.text == L"a\nb\nc\n" && sourceView.source == L"a\r\nb\rc\n", "display newline normalization preserves exact source");
    Require(sourceView.ToSource(2) == 3 && sourceView.ToDisplay(2) == 1, "CRLF source/display affinity");
    for (const auto* marker : {L"*", L"**", L"`", L"~~"})
    {
        Document document(L"\U0001f600word");
        document.SetSelection({2, 6});
        Require(ToggleMarker(document, marker), "supported format marker applied after supplementary scalar");
        Require(ToggleMarker(document, marker) && document.Text() == L"\U0001f600word", "supported format marker removed without scalar split");
    }
    Document empty;
    ToggleMarker(empty, L"**");
    Require(empty.Text() == L"****" && empty.GetSelection() == Selection{2, 2}, "collapsed formatting inserts a typing position");
    auto malformed = projection;
    malformed.units.clear();
    Reject([&] { malformed.ToSource(1, true); }, "mapping query rejects missing display units");
    malformed                   = projection;
    malformed.units.front().end = projection.source.size() + 1;
    Reject([&] { malformed.ToSource(1, true); }, "mapping query rejects invalid source range");
    malformed                         = projection;
    malformed.sourceToDisplay.front() = projection.text.size() + 1;
    Reject([&] { malformed.ToDisplay(0); }, "mapping query rejects invalid display result");
}
} // namespace

void RunRichMarkdownTests()
{
    DXUI_RUN_TEST(TestRichMarkdownPrototypeContracts);
    DXUI_RUN_TEST(TestRichMarkdownSnapshotAndValidationContracts);
    DXUI_RUN_TEST(TestRichMarkdownV0StylesCommandsAndMalformedMappings);
}
