#include "Scope.Tests.RichMarkdown.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Scope.Tests.MarkdownCorpus.h"
#include <DxUi/MarkdownSyntax.h>
#include <DxUi/RichMarkdown.h>
#include <span>
#include <type_traits>
namespace
{
using namespace DxUi::RichText;
static_assert(! std::is_constructible_v<MarkdownSyntax, std::wstring_view>);
static_assert(! std::is_copy_assignable_v<MarkdownSyntax>);
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
std::shared_ptr<const MarkdownSyntax> CompleteMarkdown(MarkdownParser& parser, std::size_t budget = 7u)
{
    std::size_t total = 0u;
    for (std::size_t turn = 0u; turn < Document::MaximumUnits * 2u + 4u; ++turn)
    {
        const auto advance = parser.Advance(budget);
        Require(advance.workUnits <= budget, "Markdown advance respects the supplied operation budget");
        total += advance.workUnits;
        if (advance.status == MarkdownPreparationStatus::Complete)
        {
            const auto syntax = parser.Published();
            Require(syntax && total <= syntax->Source().size() * 2u + 3u, "scans resume with linear bounded work and atomically publish");
            return syntax;
        }
        Require(advance.status == MarkdownPreparationStatus::Pending, "started Markdown parse remains pending until publication");
    }
    throw std::runtime_error("Markdown preparation failed to finish bounded source");
}
void CheckMarkdownPartitions(const MarkdownSyntax& syntax, std::wstring_view original)
{
    Require(syntax.Source() == original, "lossless source keeps every original UTF-16 unit");
    std::size_t end = 0u;
    for (const auto& block : syntax.Blocks())
    {
        Require(block.source.first == end && block.source.last > end && block.source.last <= original.size(), "root blocks partition exact source");
        Require(block.content.first >= block.source.first && block.content.first <= block.content.last && block.content.last <= block.source.last,
                "content is a bounded original source range");
        static_cast<void>(syntax.Slice(block.info));
        end = block.source.last;
    }
    Require(end == original.size(), "no gaps at block/EOF boundaries");
    end = 0u;
    for (std::size_t index = 0u; index < syntax.Lexemes().size(); ++index)
    {
        const auto& token = syntax.Lexemes()[index];
        Require(token.source.first == end && token.source.last > end && token.source.last <= original.size(), "lexemes independently partition exact source");
        Require(syntax.LexemeSourceAnchor(index, 0u) == token.source.first &&
                    syntax.LexemeSourceAnchor(index, syntax.LexemeValue(index).size(), true) == token.source.last,
                "lexical values have explicit source boundary anchors");
        end = token.source.last;
    }
    Require(end == original.size(), "no lost delimiters, trivia or unknown lexical source");
}
void TestLosslessMarkdownOriginalCorporaPreservation()
{
    std::size_t examples = 0u;
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029})
    {
        const auto cases = profile == MarkdownProfile::CommonMark0312 ? std::span<const std::wstring_view>(MarkdownCorpus::kCommonMark)
                                                                      : std::span<const std::wstring_view>(MarkdownCorpus::kGfm);
        for (const auto input : cases)
        {
            MarkdownParser parser;
            parser.Begin(input, examples, profile);
            const auto syntax = CompleteMarkdown(parser, 13u);
            CheckMarkdownPartitions(*syntax, input);
            Require(syntax->Profile() == profile && syntax->Revision() == examples, "corpora remain separate keyed grammar targets");
            ++examples;
        }
    }
    Require(examples == 1324u, "all 652 CommonMark and 672 GFM original examples retained without exclusions");
}
void TestLosslessMarkdownRootLeafBlocks()
{
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
    {
        MarkdownParser parser;
        const std::wstring source = L"  ## H\u00e9ading ### \r\n\t \rparagraph\rcontinuation\n---\n\n* * *\r\n";
        parser.Begin(source, 1u, profile);
        const auto syntax = CompleteMarkdown(parser, 1u);
        CheckMarkdownPartitions(*syntax, source);
        const auto& blocks = syntax->Blocks();
        Require(blocks.size() == 5u && blocks[0].kind == MarkdownBlockKind::AtxHeading && blocks[0].heading == 2u &&
                    syntax->Slice(blocks[0].content) == L"H\u00e9ading",
                "ATX indentation, closing hashes and source trivia are distinct");
        Require(blocks[1].kind == MarkdownBlockKind::BlankLine && blocks[2].kind == MarkdownBlockKind::SetextHeading && blocks[2].heading == 2u &&
                    syntax->Slice(blocks[2].content) == L"paragraph\rcontinuation",
                "setext underline takes precedence over a thematic break after a multiline paragraph");
        Require(blocks[4].kind == MarkdownBlockKind::ThematicBreak, "spaced same-character thematic marker recognized");
        for (const auto input : {L"#", L"# ", L"# ###", L"###### six", L"   # one\t", L"# content ## more"})
        {
            parser.Begin(input, 2u, profile);
            const auto heading = CompleteMarkdown(parser);
            Require(heading->Blocks()[0].kind == MarkdownBlockKind::AtxHeading, "empty, six-level and trailing-content headings recognized");
        }
        for (const auto input :
             {L"####### literal", L"#no-space", L"    # indented", L"\t# tab", L"* _ *", L"\\# escaped", L"&#35; not heading", L"&#10;&#10;still one block"})
        {
            parser.Begin(input, 2u, profile);
            Require(CompleteMarkdown(parser)->Blocks()[0].kind == MarkdownBlockKind::Paragraph, "invalid root marker remains exact paragraph source");
        }
        parser.Begin(L"# abc ## def", 2u, profile);
        const auto heading = CompleteMarkdown(parser);
        Require(heading->Slice(heading->Blocks()[0].content) == L"abc ## def", "interior hash run never truncates later content");
    }
}
void TestLosslessMarkdownFencedCodeBoundaries()
{
    MarkdownParser parser;
    const std::wstring source = L"   ```` c++\t\r\n# literal &amp;\r\n```\n~~~~\r\n  ````` \t\nnext";
    parser.Begin(source, 7u);
    const auto syntax = CompleteMarkdown(parser, 1u);
    CheckMarkdownPartitions(*syntax, source);
    const auto& code = syntax->Blocks()[0];
    Require(syntax->Blocks().size() == 2u && code.kind == MarkdownBlockKind::FencedCode && code.closed && code.fenceIndent == 3u && code.fenceLength == 4u &&
                code.fenceMarker == L'`' && syntax->Slice(code.info) == L"c++",
            "fence identity, info trivia and longer whitespace-only closing run retained");
    Require(syntax->Slice(code.content) == L"# literal &amp;\r\n```\n~~~~\r\n", "shorter or opposite fence cannot close code or start a heading");
    for (const auto input : {L"~~~lang`\nbody", L"```\n", L"```\n# still code\n``` trailing\n"})
    {
        parser.Begin(input, 7u);
        const auto unclosed = CompleteMarkdown(parser);
        CheckMarkdownPartitions(*unclosed, input);
        Require(unclosed->Blocks().size() == 1u && unclosed->Blocks()[0].kind == MarkdownBlockKind::FencedCode && ! unclosed->Blocks()[0].closed,
                "unfinished code retains source through EOF without inventing a closing delimiter");
    }
    parser.Begin(L"``` invalid`info", 7u);
    Require(CompleteMarkdown(parser)->Blocks()[0].kind == MarkdownBlockKind::Paragraph, "backtick in backtick-fence info prevents opening");
}
void TestLosslessMarkdownLexicalValuesAndAffinity()
{
    MarkdownParser parser;
    for (const auto& entity : MarkdownCorpus::kEntities)
    {
        parser.Begin(entity.source, 0u);
        const auto syntax = CompleteMarkdown(parser, 1u);
        Require(syntax->Lexemes().size() == 1u && syntax->Lexemes()[0].kind == MarkdownLexemeKind::CharacterReference &&
                    syntax->LexemeValue(0u) == entity.expected,
                "every authoritative semicolon name decodes to its independent Unicode string");
        for (std::size_t offset = 1u; offset < entity.expected.size(); ++offset)
            Require(syntax->LexemeSourceAnchor(0u, offset) == 0u && syntax->LexemeSourceAnchor(0u, offset, true) == entity.source.size(),
                    "one source span expands to several UTF-16 units with explicit reverse affinity");
    }
    for (const auto& item : {std::pair{L"&#0;", L"\ufffd"},
                             {L"&#xD800;", L"\ufffd"},
                             {L"&#1114112;", L"\ufffd"},
                             {L"&#X1f600;", L"\U0001f600"},
                             {L"&#128;", L"\u0080"},
                             {L"&#0000001;", L"\x0001"}})
    {
        parser.Begin(item.first, 0u);
        Require(CompleteMarkdown(parser)->LexemeValue(0u) == item.second, "numeric digit limits, scalar validation and NUL replacement");
    }
    for (const auto input : {L"&amp", L"&MadeUp;", L"&#;", L"&#x;", L"&#12345678;", L"&#x1234567;", L"\\a"})
    {
        parser.Begin(input, 0u);
        const auto syntax = CompleteMarkdown(parser);
        Require(syntax->Lexemes().size() == 1u && syntax->Lexemes()[0].kind == MarkdownLexemeKind::Text && syntax->LexemeValue(0u) == input,
                "malformed references and nonpunctuation escapes remain original text");
    }
    for (const wchar_t punctuation : std::wstring_view(L"!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"))
    {
        const std::wstring escaped{L'\\', punctuation};
        parser.Begin(escaped, 0u);
        const auto syntax = CompleteMarkdown(parser);
        Require(syntax->Lexemes().size() == 1u && syntax->Lexemes()[0].kind == MarkdownLexemeKind::EscapedPunctuation &&
                    syntax->LexemeValue(0u) == escaped.substr(1u),
                "all ASCII punctuation escapes keep original spelling");
    }
    const std::wstring original(L"a\0\U0001f600\r\nb\rc\n", 10u);
    parser.Begin(original, 9u);
    const auto syntax = CompleteMarkdown(parser, 1u);
    CheckMarkdownPartitions(*syntax, original);
    Require(syntax->Source()[1u] == L'\0' && syntax->LexemeValue(1u) == L"\ufffd", "NUL replacement never modifies the admitted parser source");
    Reject([&] { syntax->Slice({2u, original.size() + 1u}); }, "out of bounds syntax slice rejected");
    Reject([&] { syntax->LexemeValue(999u); }, "out of bounds lexical index rejected");
    Reject([&] { syntax->LexemeSourceAnchor(1u, 9u); }, "out of bounds transformed offset rejected");
}
void TestLosslessMarkdownSchedulingCancellationAndIdentity()
{
    MarkdownParser parser;
    Require(parser.Advance().status == MarkdownPreparationStatus::Idle, "default parser is idle");
    std::wstring borrowed = L"owned source";
    parser.Begin(borrowed, 0u);
    borrowed.assign(L"caller changed input");
    Require(CompleteMarkdown(parser)->Source() == L"owned source", "Begin owns source independently of the caller's mutable buffer");
    parser.Begin(L"old", 1u);
    const auto old = CompleteMarkdown(parser);
    const std::wstring latest(Document::MaximumUnits, L'&');
    parser.Begin(latest, 2u);
    Require(parser.Advance(0u).workUnits == 0u && parser.Published() == old, "zero budget cannot publish or consume work");
    const auto first = parser.Advance(3u);
    parser.Begin(latest, 2u);
    std::size_t total = first.workUnits;
    while (parser.HasPendingPreparation())
        total += parser.Advance(17u).workUnits;
    Require(total == latest.size() * 2u + 3u && parser.Published()->Source() == latest,
            "same pending request coalesces without restarting or quadratic hostile ampersand scans");
    const auto published = parser.Published();
    parser.Begin(latest, 2u);
    Require(! parser.HasPendingPreparation() && parser.Published() == published, "same completed request retains immutable identity");
    parser.Begin(L"pending", 3u);
    parser.Advance(1u);
    Reject([&] { parser.Begin(std::wstring(1u, 0xd800), 3u); }, "malformed UTF-16 request rejected");
    Reject([&] { parser.Begin(std::wstring(Document::MaximumUnits + 1u, L'x'), 3u); }, "oversized request rejected");
    Reject([&] { parser.Begin(L"pending", 3u, static_cast<MarkdownProfile>(99)); }, "unknown profile rejected");
    Require(parser.HasPendingPreparation() && CompleteMarkdown(parser)->Source() == L"pending", "invalid Begin preserves the previous pending request");
    parser.Begin(L"new same revision", 3u);
    Require(CompleteMarkdown(parser)->Source() == L"new same revision", "equal numeric revision cannot alias different source");
    parser.Begin(L"new same revision", 3u, MarkdownProfile::Gfm029);
    Require(CompleteMarkdown(parser)->Profile() == MarkdownProfile::Gfm029, "profile changes cannot reuse another grammar result");
    parser.Begin(L"cancel", 4u);
    parser.Cancel();
    Require(! parser.HasPendingPreparation() && parser.Published()->Revision() == 3u, "cancellation discards staging only");
    MarkdownParser moved(std::move(parser));
    Require(moved.Published()->Revision() == 3u && old->Source() == L"old", "parser moves and later revisions never mutate held syntax");
    moved.Clear();
    Require(! moved.Published() && moved.Advance().status == MarkdownPreparationStatus::Idle, "clear returns parser to idle");
}
} // namespace

void RunRichMarkdownTests()
{
    DXUI_RUN_TEST(TestRichMarkdownPrototypeContracts);
    DXUI_RUN_TEST(TestRichMarkdownSnapshotAndValidationContracts);
    DXUI_RUN_TEST(TestRichMarkdownV0StylesCommandsAndMalformedMappings);
    DXUI_RUN_TEST(TestLosslessMarkdownOriginalCorporaPreservation);
    DXUI_RUN_TEST(TestLosslessMarkdownRootLeafBlocks);
    DXUI_RUN_TEST(TestLosslessMarkdownFencedCodeBoundaries);
    DXUI_RUN_TEST(TestLosslessMarkdownLexicalValuesAndAffinity);
    DXUI_RUN_TEST(TestLosslessMarkdownSchedulingCancellationAndIdentity);
}
