#pragma once
#include "Controls/DxUi.MarkdownInlines.h"
#include <DxUi/RichSemantic.h>
#include <DxUi/RichTextCommands.h>
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace RichCommandTests
{
using namespace DxUi::RichText;
inline std::shared_ptr<const MarkdownSyntax> Parse(const Document& document, MarkdownProfile profile = MarkdownProfile::Gfm029)
{
    MarkdownParser parser;
    parser.Begin(document.Snapshot(), document.Revision(), profile);
    while (parser.HasPendingPreparation())
        parser.Advance(113u);
    return parser.Published();
}
} // namespace RichCommandTests

// Shared independent coordinate/transaction oracles run in both native and
// supplied-device test processes. No prepared layout or provider internals.
namespace RichSemanticTests
{
using namespace DxUi::RichText;
inline void Check(bool condition, const char* message)
{
    if (! condition)
    {
        std::cerr << "FAIL rich semantic: " << message << '\n';
        std::exit(1);
    }
}
template <typename F> void Run(F&& operation)
{
    try
    {
        operation();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL rich semantic unexpected exception: " << error.what() << '\n';
        std::exit(1);
    }
}
template <typename Exception, typename F> void Reject(F&& operation, const char* message)
{
    bool rejected = false;
    try
    {
        operation();
    }
    catch (const Exception&)
    {
        rejected = true;
    }
    Check(rejected, message);
}
inline void CheckRanges(const std::vector<MarkdownRange>& actual, std::initializer_list<MarkdownRange> expected, const char* message)
{
    Check(actual.size() == expected.size() && std::equal(actual.begin(), actual.end(), expected.begin()), message);
}
inline void CoordinateOracles()
{
    // Source deliberately differs from display order, with unselected middle
    // spelling, a decoded two-scalar entity and a repeated link-like label.
    const std::wstring source  = L"**ab** ignored &fjlig; Z";
    const std::wstring display = L"Z [ab] fj ab";
    const std::array spans{DisplaySpan{{23u, 24u}, {0u, 1u}, DisplaySpanKind::Source},
                           DisplaySpan{{22u, 22u}, {1u, 3u}, DisplaySpanKind::Generated, false},
                           DisplaySpan{{2u, 4u}, {3u, 5u}, DisplaySpanKind::Source},
                           DisplaySpan{{4u, 4u}, {5u, 7u}, DisplaySpanKind::Generated, false},
                           DisplaySpan{{15u, 22u}, {7u, 9u}, DisplaySpanKind::Transformed},
                           DisplaySpan{{22u, 22u}, {9u, 10u}, DisplaySpanKind::Generated, false},
                           DisplaySpan{{2u, 4u}, {10u, 12u}, DisplaySpanKind::Source, false}};
    // Use explicit offsets so this oracle also catches accidental fixture drift.
    Check(source.substr(15u, 7u) == L"&fjlig;" && source[22u] == L' ', "semantic oracle source positions");
    const auto map = SourceDisplayMap::Build(source, display, spans);
    Check(map->Source() == source && map->Display() == display && map->Spans().size() == spans.size(), "immutable semantic storage");
    Check(map->ToDisplay(2u) == 3u && map->ToDisplay(23u) == 0u, "source caret uses primary reordered occurrences");
    Check(map->ToSource(11u) == 3u, "repeated hit maps back to its original source");
    Check(map->ToSource(3u) == 2u && map->ToSource(3u, MappingAffinity::Trailing) == 22u, "shared boundary affinity selects adjoining span");
    Check(map->ToSource(8u) == 15u && map->ToSource(8u, MappingAffinity::Trailing) == 22u, "entity interior retains atomic leading trailing anchors");
    Check(map->ToDisplay(18u) == 7u && map->ToDisplay(18u, MappingAffinity::Trailing) == 9u, "source entity interior affinity");
    Check(map->SnapDisplayBoundary(8u, MappingAffinity::Leading) == 7u && map->SnapDisplayBoundary(8u, MappingAffinity::Trailing) == 9u,
          "two-scalar transformed output has no partial edit stop");
    CheckRanges(map->DisplayRanges({0u, 2u}), {}, "hidden-only source has no glyphs");
    CheckRanges(map->DisplayRanges({2u, 4u}), {{3u, 5u}, {10u, 12u}}, "source selection includes separated repeated occurrences");
    CheckRanges(map->DisplayRanges({17u, 18u}), {{7u, 9u}}, "partial raw entity selects complete decoded value");
    CheckRanges(map->SourceRanges({0u, 5u}), {{2u, 4u}, {23u, 24u}}, "reordered selection excludes unrelated middle source");
    CheckRanges(map->SourceRanges({3u, 12u}), {{2u, 4u}, {15u, 22u}}, "repeated source is deduplicated and generated text has no edit ownership");
    CheckRanges(map->SourceRanges({1u, 3u}), {}, "generated-only visual selection edits nothing");
    CheckRanges(map->SourceRanges({8u, 9u}), {{15u, 22u}}, "partial decoded selection edits the complete entity spelling");
    CheckRanges(map->DisplayRanges({3u, 3u}), {}, "empty source range is not glyph selection");
    CheckRanges(map->SourceRanges({8u, 8u}), {}, "atomic caret is not implicit deletion");
    const auto hidden = SourceDisplayMap::Build(L"###", L"", {});
    Check(hidden->ToSource(0u) == 3u && hidden->ToSource(0u, MappingAffinity::Trailing) == 0u && hidden->ToDisplay(1u) == 0u,
          "all-hidden source exposes explicit outside affinities");
    const auto empty = SourceDisplayMap::Build(L"", L"", {});
    Check(empty->ToDisplay(0u) == 0u && empty->ToSource(0u) == 0u, "empty map boundaries");
    const std::array object{DisplaySpan{{0u, 8u}, {0u, 3u}, DisplaySpanKind::Object}};
    const auto image = SourceDisplayMap::Build(L"![a](xx)", L"[a]", object);
    CheckRanges(image->SourceRanges({1u, 2u}), {{0u, 8u}}, "image alternative edits whole source object");
    CheckRanges(image->DisplayRanges({4u, 5u}), {{0u, 3u}}, "image source selection paints the whole object");
}
inline void UnicodeAndAdmission()
{
    const std::wstring text = L"A\U0001f600\r\nB";
    const std::array spans{DisplaySpan{{0u, 6u}, {0u, 6u}, DisplaySpanKind::Source}};
    const auto map = SourceDisplayMap::Build(text, text, spans);
    for (const auto point : {0u, 1u, 3u, 5u, 6u})
        Check(map->ToDisplay(point) == point && map->ToSource(point) == point, "identity retains exact scalar/CRLF ACPs");
    for (const auto point : {2u, 4u})
    {
        Reject<std::out_of_range>([&] { map->ToSource(point); }, "display join must be rejected");
        Reject<std::out_of_range>([&] { map->ToDisplay(point); }, "source join must be rejected");
        Check(map->SnapDisplayBoundary(point, MappingAffinity::Leading) == point - 1u &&
                  map->SnapDisplayBoundary(point, MappingAffinity::Trailing) == point + 1u,
              "snap respects scalar and CRLF joins");
    }
    Reject<std::out_of_range>([&] { map->SourceRanges({3u, 1u}); }, "backward interval requires normalized selection");
    Reject<std::out_of_range>([&] { map->DisplayRanges({0u, 7u}); }, "source range bound");
    Reject<std::out_of_range>([&] { map->SnapDisplayBoundary(7u, MappingAffinity::Leading); }, "snap range bound");
    Reject<std::invalid_argument>([&] { map->ToSource(0u, static_cast<MappingAffinity>(99)); }, "unknown affinity rejected");
    auto bad           = spans;
    bad[0].source.last = 2u;
    Reject<std::out_of_range>([&] { SourceDisplayMap::Build(text, text, bad); }, "span cannot split a surrogate pair");
    bad                  = spans;
    bad[0].display.first = 1u;
    Reject<std::invalid_argument>([&] { SourceDisplayMap::Build(text, text, bad); }, "display coverage cannot have holes");
    bad         = spans;
    bad[0].kind = static_cast<DisplaySpanKind>(99);
    Reject<std::invalid_argument>([&] { SourceDisplayMap::Build(text, text, bad); }, "unknown kind rejected");
    bad            = spans;
    bad[0].primary = false;
    Reject<std::invalid_argument>([&] { SourceDisplayMap::Build(text, text, bad); }, "repeat without primary rejected");
    Reject<std::invalid_argument>([&] { SourceDisplayMap::Build(L"x", L"y", std::array{DisplaySpan{{0u, 1u}, {0u, 1u}}}); },
                                  "source-backed spelling must match");
    Reject<std::invalid_argument>([&] { SourceDisplayMap::Build(L"x", L"xx", std::array{DisplaySpan{{0u, 1u}, {0u, 1u}}, DisplaySpan{{0u, 1u}, {1u, 2u}}}); },
                                  "overlapping primary ownership rejected");
    Reject<std::invalid_argument>([&] { SourceDisplayMap::Build(L"", L"x", std::array{DisplaySpan{{0u, 0u}, {0u, 1u}, DisplaySpanKind::Generated}}); },
                                  "generated text cannot claim primary source");
    Reject<std::invalid_argument>([&] { SourceDisplayMap::Build(L"", std::wstring_view(L"\0", 1u), {}); },
                                  "counted display still requires a covering partition");
    const auto countedNul = std::wstring_view(L"\0", 1u);
    const auto rawNul = SourceDisplayMap::Build(countedNul, countedNul, std::array{DisplaySpan{{0u, 1u}, {0u, 1u}, DisplaySpanKind::Source}});
    Check(rawNul->Source() == countedNul && rawNul->Display() == countedNul && rawNul->ToDisplay(1u) == 1u && rawNul->ToSource(1u) == 1u,
          "a covering identity span retains counted NUL in both coordinate domains");
    const auto visibleNul = SourceDisplayMap::Build(countedNul, L"\ufffd", std::array{DisplaySpan{{0u, 1u}, {0u, 1u}, DisplaySpanKind::Transformed}});
    Check(visibleNul->Source() == countedNul && visibleNul->Display() == L"\ufffd" && visibleNul->ToDisplay(1u) == 1u && visibleNul->ToSource(1u) == 1u,
          "a transformed span preserves counted source NUL while displaying the replacement character");
    const wchar_t unpaired = static_cast<wchar_t>(0xd800u);
    Reject<std::invalid_argument>([&] { SourceDisplayMap::Build(std::wstring_view(&unpaired, 1u), L"", {}); },
                                  "source maps reject unpaired UTF-16 surrogates");
    Reject<std::length_error>([&] { SourceDisplayMap::Build(std::wstring(Document::MaximumUnits + 1u, L'x'), L"", {}); }, "source capacity bound");
    Reject<std::length_error>([&] { SourceDisplayMap::Build(L"", std::wstring(SourceDisplayMap::MaximumDisplayUnits + 1u, L'x'), {}); },
                              "display amplification bound");
    const std::wstring maximum(Document::MaximumUnits, L'x');
    const auto admitted = SourceDisplayMap::Build(maximum, maximum, std::array{DisplaySpan{{0u, maximum.size()}, {0u, maximum.size()}}});
    Check(admitted->ToDisplay(maximum.size()) == maximum.size(), "exact maximum source admitted");
}
inline void TransactionOracles()
{
    const std::wstring nulSource(L"A\0B A\0B", 7u);
    Document nulSearch(nulSource);
    Check(FindInSource(nulSearch.Snapshot(), std::wstring_view(nulSource).substr(0u, 3u)) == std::vector<MarkdownRange>{{0u, 3u}, {4u, 7u}},
          "source search preserves the complete counted NUL query and source length");
    Document search(L"Alpha alpha alphabet\r\nALPHA");
    const auto matches = FindInSource(search.Snapshot(), L"alpha", {.wholeWord = true});
    Check(matches == std::vector<MarkdownRange>{{0u, 5u}, {6u, 11u}, {22u, 27u}}, "ordinal whole-word search returns source ranges");
    Check(FindInSource(search.Snapshot(), L"alpha", {.matchCase = true}) == std::vector<MarkdownRange>{{6u, 11u}, {12u, 17u}},
          "case-sensitive search preserves source offsets");
    std::vector<SourceEdit> replacements(0u);
    for (const auto match : matches)
        replacements.push_back({match, L"x"});
    const auto originalSearch = search.Snapshot();
    Check(ApplySourceEdits(search, originalSearch, search.Revision(), replacements, {1u, 1u}) && search.Text() == L"x x alphabet\r\nx" && search.Undo() &&
              search.Snapshot().IsSameRevision(originalSearch),
          "search replacements apply as one undoable snapshot transaction");
    Document document(L"- [ ] one\r\n- [x] two\n");
    document.SetSelection({9u, 6u});
    const auto initial  = document.Text();
    const auto snapshot = document.Snapshot();
    const auto revision = document.Revision();
    const std::array edits{SourceEdit{{3u, 4u}, L"x"}, SourceEdit{{14u, 15u}, L" "}};
    Check(ApplySourceEdits(document, snapshot, revision, edits, {9u, 6u}), "two edits publish one source transaction");
    Check(document.Text() == L"- [x] one\r\n- [ ] two\n" && snapshot.Text() == initial && document.Revision() == revision + 1u,
          "transaction preserves mixed EOLs and immutable snapshots");
    Check(document.Undo() && document.Text() == initial && document.GetSelection() == Selection{9u, 6u} && ! document.CanUndo() && document.CanRedo(),
          "one undo restores both source edits and backward selection");
    Check(document.Redo() && document.Text() == L"- [x] one\r\n- [ ] two\n" && ! document.CanRedo(), "one redo reapplies both source edits");
    Reject<std::invalid_argument>([&] { ApplySourceEdits(document, snapshot, revision, edits, {}); }, "stale revision rejected");
    Document foreign(document.Text());
    Reject<std::invalid_argument>([&] { ApplySourceEdits(document, foreign.Snapshot(), document.Revision(), {}, {}); },
                                  "foreign snapshot rejected even with equal spelling");
    const auto unchanged         = document.Snapshot();
    const auto unchangedRevision = document.Revision();
    const std::array noOp{SourceEdit{{0u, 1u}, L"-"}, SourceEdit{{6u, 9u}, L"one"}};
    Check(! ApplySourceEdits(document, unchanged, unchangedRevision, noOp, {9u, 0u}) && document.Revision() == unchangedRevision &&
              document.GetSelection() == Selection{9u, 0u},
          "same spelling changes selection without history or revision");
    Check(! ApplySourceEdits(document, unchanged, unchangedRevision, {}, {0u, 0u}), "empty edit plan only updates valid selection");
    Document adjacent(L"abc");
    Check(ApplySourceEdits(adjacent, adjacent.Snapshot(), adjacent.Revision(), std::array{SourceEdit{{0u, 1u}, L"A"}, SourceEdit{{1u, 2u}, L"B"}}, {2u, 2u}) &&
              adjacent.Text() == L"ABc",
          "touching edits have deterministic order");
}
inline void TransactionRejections()
{
    Document document(L"A\U0001f600\r\nB");
    document.SetSelection({6u, 0u});
    document.Replace(5u, 6u, L"C", {6u, 0u});
    document.Undo(); // Both histories must survive every failed staging attempt.
    const auto source    = document.Snapshot();
    const auto before    = document.Text();
    const auto revision  = document.Revision();
    const auto selection = document.GetSelection();
    const auto undo = document.CanUndo(), redo = document.CanRedo();
    const auto reject = [&]<typename Exception>(std::span<const SourceEdit> edits, Selection after, const char* reason)
    {
        Reject<Exception>([&] { ApplySourceEdits(document, source, revision, edits, after); }, reason);
        Check(document.Text() == before && document.Revision() == revision && document.GetSelection() == selection && document.CanUndo() == undo &&
                  document.CanRedo() == redo && document.Snapshot().IsSameRevision(source),
              "rejected edit plan preserves complete published state");
    };
    reject.operator()<std::out_of_range>(std::array{SourceEdit{{1u, 2u}, L""}}, {}, "edit cannot split a UTF-16 scalar");
    reject.operator()<std::out_of_range>(std::array{SourceEdit{{3u, 4u}, L""}}, {}, "edit cannot split a CRLF pair");
    reject.operator()<std::invalid_argument>(std::array{SourceEdit{{0u, 3u}, L""}, SourceEdit{{1u, 3u}, L""}}, {}, "overlapping edits are rejected");
    reject.operator()<std::invalid_argument>(std::array{SourceEdit{{5u, 6u}, L""}, SourceEdit{{0u, 1u}, L""}}, {}, "unordered edits are rejected");
    reject.operator()<std::invalid_argument>(std::array{SourceEdit{{1u, 1u}, L"a"}, SourceEdit{{1u, 1u}, L"b"}}, {}, "duplicate edit positions are rejected");
    const wchar_t unpaired = static_cast<wchar_t>(0xd800u);
    reject.operator()<std::invalid_argument>(std::array{SourceEdit{{0u, 1u}, std::wstring_view(&unpaired, 1u)}}, {},
                                             "unpaired replacement surrogate is rejected");
    reject.operator()<std::out_of_range>(std::array{SourceEdit{{0u, 1u}, L"\r"}}, {2u, 2u}, "future selection cannot split a UTF-16 scalar");
    const std::wstring tooLarge(Document::MaximumUnits, L'x');
    reject.operator()<std::length_error>(std::array{SourceEdit{{0u, 0u}, tooLarge}}, {}, "transaction source capacity is enforced");
    reject.operator()<std::out_of_range>({}, {7u, 0u}, "selection beyond source is rejected");
    Document counted(L"abc");
    const auto countedBefore = counted.Snapshot();
    const std::wstring expected{L'a', L'\0', L'c'};
    Check(ApplySourceEdits(counted, countedBefore, counted.Revision(), std::array{SourceEdit{{1u, 2u}, std::wstring_view(L"\0", 1u)}}, {2u, 2u}) &&
              counted.Text() == expected && counted.GetSelection() == Selection{2u, 2u} && counted.Undo() && counted.Text() == L"abc" &&
              counted.Redo() && counted.Text() == expected,
          "counted NUL replacement is source-exact and remains one undoable transaction");
    Document join(L"aX\nb");
    const auto joinSnapshot = join.Snapshot();
    const auto joinRevision = join.Revision();
    Reject<std::out_of_range>([&] { ApplySourceEdits(join, joinSnapshot, joinRevision, std::array{SourceEdit{{0u, 2u}, L"\r"}}, {1u, 1u}); },
                              "future CRLF join selection rejected before publication");
    Check(join.Text() == L"aX\nb" && join.Revision() == joinRevision, "join failure is atomic");
}
} // namespace RichSemanticTests

namespace RichCommandTests
{
using RichSemanticTests::Check;
using RichSemanticTests::Reject;
inline void Exact(std::wstring_view source,
                  Selection before,
                  MarkdownCommandRequest request,
                  std::wstring_view expected,
                  std::optional<Selection> after = std::nullopt,
                  MarkdownProfile profile        = MarkdownProfile::Gfm029)
{
    Document document(source);
    document.SetSelection(before);
    const auto snapshot = document.Snapshot(), syntaxSnapshot = snapshot;
    const auto revision = document.Revision();
    const auto syntax   = Parse(document, profile);
    const auto plan     = BuildMarkdownCommand(snapshot, revision, syntax, before, request);
    if (! plan.IsApplicable())
        std::wcerr << L"unavailable Exact command=" << static_cast<unsigned>(request.command) << L" style=" << static_cast<unsigned>(request.style)
                   << L" source=[" << source << L"] selection={" << before.anchor << L',' << before.caret << L"}\n";
    Check(plan.IsApplicable(), "structural command admitted");
    Check(document.Snapshot().IsSameRevision(snapshot) && document.GetSelection() == before && ! document.CanUndo(), "planning is read-only");
    const auto changed = ApplyMarkdownCommand(document, plan);
    if (document.Text() != expected)
    {
        std::wcerr << L"command=" << static_cast<int>(request.command) << L" expected=[" << expected << L"] actual=[" << document.Text() << L"]\n";
    }
    Check(document.Text() == expected && changed == (source != expected), "independent literal structural source oracle");
    Check(! after || document.GetSelection() == *after, "exact forward/backward structural selection");
    Check(syntax->Source() == source && syntaxSnapshot.Text() == source, "old source and analysis remain immutable");
    const auto result = Parse(document, profile);
    Check(result && result->Source() == expected, "independent result parse accepts exact command output");
    if (changed)
    {
        const auto committed          = document.Snapshot();
        const auto committedSelection = document.GetSelection();
        Check(document.Undo() && document.Snapshot().IsSameRevision(snapshot) && document.GetSelection() == before && ! document.CanUndo(),
              "one undo restores all spelling and selection");
        Check(document.Redo() && document.Snapshot().IsSameRevision(committed) && document.GetSelection() == committedSelection && ! document.CanRedo(),
              "one redo restores complete command");
    }
    else
        Check(! document.CanUndo(), "selection-only or identical command does not add history");
}
inline void Unavailable(std::wstring_view source, Selection selection, MarkdownCommandRequest request, MarkdownProfile profile = MarkdownProfile::Gfm029)
{
    Document document(source);
    document.SetSelection(selection);
    const auto snapshot = document.Snapshot();
    const auto syntax   = Parse(document, profile);
    const auto plan     = BuildMarkdownCommand(snapshot, document.Revision(), syntax, selection, request);
    Check(! plan.IsApplicable(), "unsafe structural command is unavailable");
    Check(document.Snapshot().IsSameRevision(snapshot) && document.GetSelection() == selection && ! document.CanUndo(),
          "unavailable structural command leaves exact source and selection unchanged");
}
inline void ListsAndHeadings()
{
    MarkdownCommandRequest request;
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
    {
        request.command = MarkdownCommand::SetQuote;
        Exact(L"", {}, request, L"> ", std::nullopt, profile);
        Exact(L"alpha\nbeta\n", {0u, 10u}, request, L"> alpha\n> beta\n", std::nullopt, profile);
        Exact(L"> alpha\n", {2u, 2u}, request, L"alpha\n", std::nullopt, profile);
        request.quoteAction = MarkdownQuoteAction::Outdent;
        Exact(L"> > alpha\n", {4u, 4u}, request, L"> alpha\n", std::nullopt, profile);
        Exact(L"> - > item\n", {6u, 6u}, request, L"> - item\n", std::nullopt, profile);
        request.quoteAction = MarkdownQuoteAction::Toggle;
        Exact(L"- item\n", {2u, 2u}, request, L"- > item\n", std::nullopt, profile);
        Exact(L"> a\n# b\n", {0u, 7u}, request, L"> a\n> # b\n", std::nullopt, profile);
        Exact(L"a\n\nb\n", {0u, 4u}, request, L"> a\n> \n> b\n", std::nullopt, profile);
        Exact(L"~~~\nx\n~~~\n", {4u, 4u}, request, L"> ~~~\n> x\n> ~~~\n", std::nullopt, profile);
        Exact(L"> ~~~\n> x\n> ~~~\n", {8u, 8u}, request, L"~~~\nx\n~~~\n", std::nullopt, profile);
        Exact(L"<div>\nx\n</div>\n", {5u, 5u}, request, L"> <div>\n> x\n> </div>\n", std::nullopt, profile);
        Exact(L"---\n", {1u, 1u}, request, L"> ---\n", std::nullopt, profile);
        if (profile != MarkdownProfile::CommonMark0312)
            Exact(L"a | b\n- | -\nx | y\n", {2u, 2u}, request, L"> a | b\n> - | -\n> x | y\n", std::nullopt, profile);
        request.command = MarkdownCommand::SetCodeBlock;
        request.text    = L"cpp";
        Exact(L"hello\n", {0u, 5u}, request, L"~~~cpp\nhello\n~~~\n", std::nullopt, profile);
        Exact(L"> hello\n", {2u, 7u}, request, L"> ~~~cpp\n> hello\n> ~~~\n", std::nullopt, profile);
        Exact(L"- hello\n", {2u, 7u}, request, L"- ~~~cpp\n  hello\n  ~~~\n", std::nullopt, profile);
        Exact(L"a ~~~ b\n", {0u, 7u}, request, L"~~~~cpp\na ~~~ b\n~~~~\n", std::nullopt, profile);
        Exact(L"~~~\nhello\n~~~\n", {5u, 5u}, request, L"hello\n", std::nullopt, profile);
        Exact(L"> ~~~\n> hello\n> ~~~\n", {9u, 9u}, request, L"> hello\n", std::nullopt, profile);
        Exact(L"- ~~~\n  hello\n  ~~~\n", {9u, 9u}, request, L"- hello\n", std::nullopt, profile);
        Exact(L"- ~~~\n  ~~~\n", {4u, 4u}, request, L"- \n", std::nullopt, profile);
        Exact(L"~~~\nhello", {4u, 4u}, request, L"hello", std::nullopt, profile);
        Exact(L"> ~~~\n> hello", {8u, 8u}, request, L"> hello", std::nullopt, profile);
        Exact(L"- ~~~\n  hello", {9u, 9u}, request, L"- hello", std::nullopt, profile);
        Exact(L"- ~~~", {4u, 4u}, request, L"- ", std::nullopt, profile);
        request.command = MarkdownCommand::SetCodeLanguage;
        request.text    = L"rust";
        Document languageState(L"~~~cpp\nhello\n~~~\n");
        const auto languageSyntax = Parse(languageState, profile);
        Check(FindCodeLanguageTarget(*languageSyntax, {8u, 8u}, L"rust").has_value() && ! FindCodeLanguageTarget(*languageSyntax, {8u, 8u}, L"bad\nname"),
              "language-state predicate shares the planner source and input constraints");
        Exact(L"~~~cpp\nhello\n~~~\n", {8u, 8u}, request, L"~~~rust\nhello\n~~~\n", std::nullopt, profile);
        Exact(L"> ~~~cpp\n> hello\n> ~~~\n", {12u, 12u}, request, L"> ~~~rust\n> hello\n> ~~~\n", std::nullopt, profile);
        request.command = MarkdownCommand::ExitCodeBlock;
        Document codeState(L"~~~\nx\n~~~\n");
        const auto codeSyntax = Parse(codeState, profile);
        Check(FindExitCodeBlock(*codeSyntax, {4u, 4u}).has_value() && ! FindExitCodeBlock(*codeSyntax, {0u, 3u}),
              "nonallocating code-exit admission agrees with collapsed-selection contract");
        Exact(L"~~~\nx\n~~~\n", {4u, 4u}, request, L"~~~\nx\n~~~\n\n", Selection{10u, 10u}, profile);
        Exact(L"~~~\nx", {4u, 4u}, request, L"~~~\nx\n~~~\n\n", Selection{10u, 10u}, profile);
        Exact(L"> ~~~\n> x\n> ~~~\n", {8u, 8u}, request, L"> ~~~\n> x\n> ~~~\n> \n", std::nullopt, profile);
        Exact(L"> ~~~\n> x", {8u, 8u}, request, L"> ~~~\n> x\n> ~~~\n> \n", std::nullopt, profile);
        Exact(L"- ~~~\n  x\n  ~~~\n", {9u, 9u}, request, L"- ~~~\n  x\n  ~~~\n  \n", std::nullopt, profile);
        Exact(L"- ~~~\n  x", {9u, 9u}, request, L"- ~~~\n  x\n  ~~~\n  \n", std::nullopt, profile);
        request.command = MarkdownCommand::InsertHardBreak;
        Exact(L"hello\n", {2u, 2u}, request, L"he  \nllo\n", Selection{5u, 5u}, profile);
        Exact(L"hello\n", {1u, 4u}, request, L"h  \no\n", Selection{4u, 4u}, profile);
        Exact(L"> hello\r\n", {4u, 4u}, request, L"> he  \r\n> llo\r\n", Selection{10u, 10u}, profile);
        Exact(L"- item\n", {4u, 4u}, request, L"- it  \n  em\n", Selection{9u, 9u}, profile);
        Exact(L"- outer\n  - item\n", {14u, 14u}, request, L"- outer\n  - it  \n    em\n", Selection{21u, 21u}, profile);
        Exact(L"- > item\n", {6u, 6u}, request, L"- > it  \n  > em\n", Selection{13u, 13u}, profile);
        request.command = MarkdownCommand::InsertMath;
        if (profile == MarkdownProfile::RichMarkdown)
        {
            Document mathState(L"alpha");
            const auto mathSyntax = Parse(mathState, profile);
            Check(CanInsertMath(*mathSyntax, {0u, 5u}, {}, false) && ! CanInsertMath(*mathSyntax, {0u, 0u}, {}, false),
                  "math insertion query shares planner admission for selected and empty payloads");
            Document opaqueMath(L"[link](https://example.invalid)");
            const auto opaqueMathSyntax = Parse(opaqueMath, profile);
            Check(! CanInsertMath(*opaqueMathSyntax, {2u, 2u}, L"x", false), "math insertion refuses an opaque link interior");
            request.text = L"x";
            Unavailable(L"[link](https://example.invalid)", {2u, 2u}, request, profile);
            request.text = {};
            Exact(L"alpha", {0u, 5u}, request, L"\\(alpha\\)", Selection{2u, 7u}, profile);
            request.mathDisplay = true;
            request.text        = L"x";
            Exact(L"", {}, request, L"\\[x\\]", Selection{2u, 3u}, profile);
            request.text = L"x\ny";
            Exact(L"", {}, request, L"\\[x\ny\\]", Selection{2u, 5u}, profile);
            Document displayMath(L"\\[x\ny\\]");
            const auto displaySyntax = Parse(displayMath, profile);
            Check(std::ranges::any_of(displaySyntax->Inlines(), [](const auto& node) { return node.kind == MarkdownInlineKind::Math; }),
                  "multiline display insertion remains one math source envelope");
            for (const auto& sample : {std::pair{std::wstring_view(L"$a `$x` tail"), MarkdownInlineKind::Code},
                                       std::pair{std::wstring_view(L"$a <span title=\"$x\">tail"), MarkdownInlineKind::Html},
                                       std::pair{std::wstring_view(L"$a <https://example.invalid/$x> tail"), MarkdownInlineKind::Link}})
            {
                Document protectedSource{std::wstring(sample.first)};
                const auto protectedSyntax = Parse(protectedSource, profile);
                Check(std::ranges::any_of(protectedSyntax->Inlines(), [&](const auto& node) { return node.kind == sample.second; }) &&
                          std::ranges::none_of(protectedSyntax->Inlines(), [](const auto& node) { return node.kind == MarkdownInlineKind::Math; }),
                      "unmatched math opener cannot consume a closer owned by later code, HTML or angle autolink");
            }
            Document mathComparison(L"$x < y$");
            const auto comparisonSyntax = Parse(mathComparison, profile);
            Check(std::ranges::any_of(comparisonSyntax->Inlines(), [](const auto& node) { return node.kind == MarkdownInlineKind::Math; }),
                  "a literal comparison operator does not masquerade as an HTML ownership boundary");
            Document escapedTick(L"$a \\`b` c$");
            const auto escapedTickSyntax = Parse(escapedTick, profile);
            Check(std::ranges::any_of(escapedTickSyntax->Inlines(), [](const auto& node) { return node.kind == MarkdownInlineKind::Math; }),
                  "an escaped backtick does not begin an opaque Markdown code span inside math");
            Document escapedCloser(L"\\(x\\\\)y\\)");
            const auto escapedCloserSyntax = Parse(escapedCloser, profile);
            Check(std::ranges::any_of(escapedCloserSyntax->Inlines(), [](const auto& node) { return node.kind == MarkdownInlineKind::Math; }),
                  "an escaped closer does not hide a later valid math closer");
            for (const auto source : {std::wstring_view(L"\\[x\\]"), std::wstring_view(L"$$x$$")})
            {
                Document displayCloser{std::wstring(source)};
                const auto displayCloserSyntax = Parse(displayCloser, profile);
                Check(std::ranges::any_of(displayCloserSyntax->Inlines(), [](const auto& node) { return node.kind == MarkdownInlineKind::Math; }),
                      "indexed display delimiters retain valid closing recognition");
            }
            std::wstring oversizedMath(L"\\(");
            oversizedMath.append(MathMaximumSourceUnits + 1u, L'x');
            Document oversizedMathSource(oversizedMath);
            const auto oversizedMathSyntax = Parse(oversizedMathSource, profile);
            Check(std::ranges::any_of(oversizedMathSyntax->Inlines(), [](const auto& node) { return node.kind == MarkdownInlineKind::Math; }),
                  "unclosed notation beyond source capacity retains its opaque fallback");
#if DXUI_ENABLE_DIAGNOSTICS
            for (const auto pattern : {std::wstring_view(L"<aa:a"), std::wstring_view(L"<!--")})
            {
                std::size_t previousWork{};
                for (const size_t count : {size_t{400u}, size_t{800u}})
                {
                    std::wstring adversarial(L"$");
                    for (size_t i = 0u; i < count; ++i)
                        adversarial += pattern;
                    adversarial += L'>';
                    Document candidate(adversarial);
                    DxUi::RichText::Detail::testMathOwnershipProbeWork = 0u;
                    static_cast<void>(Parse(candidate, profile));
                    const auto work = DxUi::RichText::Detail::testMathOwnershipProbeWork;
                    Check(work > 0u && work <= adversarial.size() * 64u && (! previousWork || work <= previousWork * 3u),
                          "math opaque-ownership lookahead scales linearly on repeated invalid angle prefixes");
                    previousWork = work;
                }
            }
            std::size_t previousManyWork{};
            for (const size_t count : {size_t{400u}, size_t{800u}})
            {
                std::wstring many(0u, L' ');
                for (size_t i = 0u; i < count; ++i)
                    many += L"$x<y$ > ";
                Document candidate(many);
                DxUi::RichText::Detail::testMathOwnershipProbeWork = 0u;
                const auto syntax                                  = Parse(candidate, profile);
                const auto work                                    = DxUi::RichText::Detail::testMathOwnershipProbeWork;
                Check(work > 0u && work <= many.size() * 64u && (! previousManyWork || work <= previousManyWork * 3u) &&
                          std::ranges::count_if(syntax->Inlines(), [](const auto& node) { return node.kind == MarkdownInlineKind::Math; }) ==
                              static_cast<std::ptrdiff_t>(count),
                      "many separate math openers share one leaf terminator index with linear-scale work");
                previousManyWork = work;
            }
            for (const auto pattern : {std::wstring_view(L"\\("), std::wstring_view(L"\\(x\\\\)"), std::wstring_view(L"$a$1 ")})
            {
                std::size_t previousUnclosedWork{};
                for (const size_t count : {size_t{300u}, size_t{600u}})
                {
                    std::wstring unclosed(0u, L' ');
                    for (size_t i = 0u; i < count; ++i)
                        unclosed += pattern;
                    Document candidate(unclosed);
                    DxUi::RichText::Detail::testMathOwnershipProbeWork = 0u;
                    const auto syntax                                  = Parse(candidate, profile);
                    const auto work                                    = DxUi::RichText::Detail::testMathOwnershipProbeWork;
                    Check(std::ranges::none_of(syntax->Inlines(), [](const auto& node) { return node.kind == MarkdownInlineKind::Math; }) &&
                              work <= unclosed.size() * 64u && (! previousUnclosedWork || work <= previousUnclosedWork * 3u),
                          "unclosed and ineligible-closer math runs retain literal ownership with linear-scale scan work");
                    previousUnclosedWork = work;
                }
            }
            MarkdownParser cancelled;
            std::wstring oldSource(L"$x<y$ > ");
            for (size_t i = 0u; i < 400u; ++i)
                oldSource += L"$x<y$ ";
            cancelled.Begin(oldSource, 7u, profile);
            DxUi::RichText::Detail::testMathOwnershipProbeWork = 0u;
            for (size_t step = 0u; step < 20000u && cancelled.HasPendingPreparation() && ! DxUi::RichText::Detail::testMathOwnershipProbeWork; ++step)
                cancelled.Advance(1u);
            Check(DxUi::RichText::Detail::testMathOwnershipProbeWork > 0u, "cancellation fixture reaches leaf index preparation");
            cancelled.Cancel();
            cancelled.Begin(L"$x < y$", 8u, profile);
            while (cancelled.HasPendingPreparation())
                cancelled.Advance(113u);
            Check(cancelled.Published() && cancelled.Published()->Source() == L"$x < y$" &&
                      std::ranges::any_of(cancelled.Published()->Inlines(), [](const auto& node) { return node.kind == MarkdownInlineKind::Math; }),
                  "cancelling and replacing an indexed leaf cannot retain its old terminators");
#endif
            request.mathDisplay = false;
        }
        else
            Unavailable(L"alpha", {0u, 5u}, request, profile);
        request.command = MarkdownCommand::InsertThematicBreak;
        Document ruleState(L"hello\n");
        const auto ruleSyntax = Parse(ruleState, profile);
        Check(CanInsertThematicBreak(*ruleSyntax, {2u, 2u}) && ! CanInsertThematicBreak(*ruleSyntax, {0u, 2u}),
              "thematic-break state shares collapsed-selection admission with the planner");
        Exact(L"", {}, request, L"---", Selection{3u, 3u}, profile);
        Exact(L"hello\n", {2u, 2u}, request, L"he\n\n---\n\nllo\n", Selection{9u, 9u}, profile);
        Unavailable(L"- item\n", {3u, 3u}, request, profile);
        request.command     = MarkdownCommand::InsertImage;
        request.text        = L"sun";
        request.destination = L"https://example.invalid/a.png";
        request.title       = L"A";
        Exact(L"", {}, request, L"![sun](<https://example.invalid/a.png> \"A\")", std::nullopt, profile);
        Exact(L"**word**", {2u, 6u}, request, L"**![sun](<https://example.invalid/a.png> \"A\")**", std::nullopt, profile);
        request.command = MarkdownCommand::ClearFormatting;
        Exact(L"**word**", {2u, 6u}, request, L"word", Selection{0u, 4u}, profile);
        Exact(L"**a *b* c**", {2u, 9u}, request, L"a b c", Selection{0u, 5u}, profile);
        Exact(L"[word](https://example.invalid)", {1u, 5u}, request, L"word", Selection{0u, 4u}, profile);
        Exact(L"<https://example.invalid>", {1u, 24u}, request, L"https\\:\\/\\/example\\.invalid", std::nullopt, profile);
        Document literalUrl(L"https\\:\\/\\/example\\.invalid");
        const auto literalUrlSyntax = Parse(literalUrl, profile);
        Check(std::ranges::none_of(literalUrlSyntax->Inlines(), [](const auto& node) { return node.kind == MarkdownInlineKind::Link; }),
              "cleared autolink spelling does not become another link after reparsing");
        Exact(L"**word**", {3u, 5u}, request, L"**w**or**d**", Selection{5u, 7u}, profile);
        Exact(L"`word`", {1u, 5u}, request, L"word", Selection{0u, 4u}, profile);
        request.command = MarkdownCommand::InsertEscapedText;
        request.text    = L"[x]*";
        Document escapedState(L"[link](https://example.invalid)");
        const auto escapedSyntax = Parse(escapedState, profile);
        Check(! CanInsertEscapedText(*escapedSyntax, {2u, 2u}, request.text), "literal insertion refuses the interior of an opaque link");
        Document emptyEscapedState(L"");
        const auto emptyEscapedSyntax = Parse(emptyEscapedState, profile);
        Check(CanInsertEscapedText(*emptyEscapedSyntax, {}, request.text) && ! CanInsertEscapedText(*emptyEscapedSyntax, {}, {}),
              "literal insertion admission distinguishes usable and empty payloads");
        Unavailable(L"[link](https://example.invalid)", {2u, 2u}, request, profile);
        Exact(L"", {}, request, L"\\[x\\]\\*", Selection{7u, 7u}, profile);
        request.text        = {};
        request.destination = {};
        request.title       = {};
        request.text        = {};
        request.command     = MarkdownCommand::InsertParagraph;
        Exact(L"> 9) alpha\r\n", {10u, 10u}, request, L"> 9) alpha\r\n> 10) \r\n", Selection{18u, 18u}, profile);
        Exact(L"9. abc\n   tail\n", {4u, 4u}, request, L"9. a\n10. bc\n    tail\n", std::nullopt, profile);
        Exact(L"9.\titem\n\tmore\n", {7u, 7u}, request, L"9.\titem\n10.\t\n\tmore\n", std::nullopt, profile);
        Exact(L"- \n", {2u, 2u}, request, L"\n", Selection{0u, 0u}, profile);
        Exact(L"- parent\n\n  - \n", {14u, 14u}, request, L"- parent\n\n- \n", std::nullopt, profile);
        Exact(L"```\n- raw\n```", {9u, 9u}, request, L"```\n- raw\n\n```", std::nullopt, profile);
        request.command = MarkdownCommand::IndentList;
        Exact(L"- a\n- b\n- c\n", {12u, 4u}, request, L"- a\n  - b\n  - c\n", std::nullopt, profile);
        request.command = MarkdownCommand::OutdentList;
        Exact(L"- a\n  + b\n", {8u, 8u}, request, L"- a\n+ b\n", std::nullopt, profile);
        Exact(L"> - first\r\n>   continuation\n", {5u, 5u}, request, L"> first\r\n> continuation\n", std::nullopt, profile);
        request.command   = MarkdownCommand::SetList;
        request.listKind  = MarkdownListKind::Ordered;
        request.listStart = 7u;
        Exact(L"alpha\r\nbeta\n", {0u, 12u}, request, L"7. alpha\r\n8. beta\n", std::nullopt, profile);
        Exact(L"- a\n  next\n- b\n", {2u, 2u}, request, L"7. a\n   next\n- b\n", std::nullopt, profile);
        Exact(L"-\titem\n\tmore\n", {3u, 3u}, request, L"7. item\n   more\n", std::nullopt, profile);
        request.listStart.reset();
        Exact(L"09) a\n", {4u, 4u}, request, L"09) a\n", std::nullopt, profile);
        request.command = MarkdownCommand::JoinBackward;
        Exact(L"- **a**\n- b\n", {10u, 10u}, request, L"- **a** b\n", Selection{7u, 7u}, profile);
        request.command = MarkdownCommand::JoinForward;
        Exact(L"a\r\n\r\nb", {1u, 1u}, request, L"a b", Selection{1u, 1u}, profile);
        request.command      = MarkdownCommand::SetHeading;
        request.headingLevel = 2u;
        Exact(L"", {}, request, L"## ", Selection{3u, 3u}, profile);
        Exact(L">\n", {1u, 1u}, request, L">## \n", std::nullopt, profile);
        for (unsigned level = 1u; level <= 6u; ++level)
        {
            request.headingLevel = level;
            Exact(L"> title\r\n", {3u, 3u}, request, L"> " + std::wstring(level, L'#') + L" title\r\n", std::nullopt, profile);
        }
        request.headingLevel = 3u;
        Exact(L"Title\r\n===\r\n", {3u, 3u}, request, L"### Title\r\n", std::nullopt, profile);
        request.headingLevel = 2u;
        Exact(L"Title\n===\n", {3u, 3u}, request, L"Title\n---\n", std::nullopt, profile);
        request.headingLevel = 0u;
        Exact(L"  ## title ###  \n", {7u, 7u}, request, L"  title   \n", std::nullopt, profile);
    }

    request          = {};
    request.command  = MarkdownCommand::SetList;
    request.listKind = MarkdownListKind::None;
    Exact(L"- # Title\n", {4u, 9u}, request, L"# Title\n", Selection{2u, 7u});
    request.command = MarkdownCommand::OutdentList;
    Exact(L"- > quote\n", {4u, 9u}, request, L"> quote\n", Selection{2u, 7u});
    Exact(L"- - inner\n", {4u, 9u}, request, L"- inner\n", Selection{2u, 7u});
    request.command = MarkdownCommand::ToggleTask;
    Unavailable(L"- # Title\n", {4u, 9u}, request);

    request         = {};
    request.command = MarkdownCommand::OutdentList;
    Exact(L"- a\n    - b\n", {10u, 10u}, request, L"- a\n- b\n", Selection{6u, 6u});
    request.command = MarkdownCommand::IndentList;
    Exact(L"- a\n- b\n\n  tail\n- c\n", {6u, 6u}, request, L"- a\n  - b\n\n    tail\n- c\n", Selection{8u, 8u});
    Exact(L"- parent\n  - x\n  - a\n\n    tail\n", {19u, 19u}, request, L"- parent\n  - x\n    - a\n\n      tail\n", Selection{21u, 21u});
    request.command = MarkdownCommand::OutdentList;
    Exact(L"- parent\n  - x\n  - a\n\n    tail\n", {19u, 19u}, request, L"- parent\n  - x\n- a\n\n  tail\n", Selection{17u, 17u});
    request.command  = MarkdownCommand::SetList;
    request.listKind = MarkdownListKind::None;
    Exact(L"- parent\n  - x\n  - a\n\n    tail\n", {19u, 19u}, request, L"- parent\n  - x\n  a\n\n  tail\n", Selection{17u, 17u});

    request           = {};
    request.command   = MarkdownCommand::SetList;
    request.listKind  = MarkdownListKind::Ordered;
    request.listStart = 999999999u;
    Unavailable(L"alpha\nbeta\n", {0u, 11u}, request);
}
inline void InlineTasksLinks()
{
    MarkdownCommandRequest request;
    {
        constexpr std::wstring_view source = L"ref[^n]\n\n[^n]: body\n";
        const auto start                   = source.find(L"body");
        request.command                    = MarkdownCommand::ToggleStyle;
        request.style                      = MarkdownTextStyle::Strong;
        Exact(source, {start, start + 4u}, request, L"ref[^n]\n\n[^n]: **body**\n", std::nullopt, MarkdownProfile::RichMarkdown);
        Unavailable(L"[^n]: body\n", {6u, 10u}, request, MarkdownProfile::RichMarkdown);
        Unavailable(source, {0u, start + 4u}, request, MarkdownProfile::RichMarkdown);
        constexpr std::wstring_view task = L"ref[^n]\n\n[^n]: - [ ] task\n";
        const auto taskText              = task.find(L"task");
        request.command                  = MarkdownCommand::ToggleTask;
        Exact(task, {taskText, taskText}, request, L"ref[^n]\n\n[^n]: - [x] task\n", std::nullopt, MarkdownProfile::RichMarkdown);
        request.command = MarkdownCommand::InsertParagraph;
        Exact(source, {start + 2u, start + 2u}, request, L"ref[^n]\n\n[^n]: bo\n    dy\n", std::nullopt, MarkdownProfile::RichMarkdown);
        request.command      = MarkdownCommand::SetHeading;
        request.headingLevel = 2u;
        Exact(source, {start, start}, request, L"ref[^n]\n\n[^n]: ## body\n", std::nullopt, MarkdownProfile::RichMarkdown);
        request.command  = MarkdownCommand::SetList;
        request.listKind = MarkdownListKind::Unordered;
        Exact(source, {start, start}, request, L"ref[^n]\n\n[^n]: - body\n", std::nullopt, MarkdownProfile::RichMarkdown);
        request.command       = MarkdownCommand::InsertTable;
        request.tableColumns  = 2u;
        request.tableBodyRows = 1u;
        Exact(source, {start, start + 4u}, request, L"ref[^n]\n\n[^n]: | | |\n    | --- | --- |\n    | | |\n", std::nullopt, MarkdownProfile::RichMarkdown);
        constexpr std::wstring_view table = L"ref[^n]\n\n[^n]: | a | b |\n    | --- | --- |\n    | c | d |\n";
        const auto cell                   = table.find(L"d |");
        request.command                   = MarkdownCommand::InsertTableRow;
        Exact(table,
              {cell, cell},
              request,
              L"ref[^n]\n\n[^n]: | a | b |\n    | --- | --- |\n    | c | d |\n    | | |\n",
              std::nullopt,
              MarkdownProfile::RichMarkdown);
        request.command                    = MarkdownCommand::InsertParagraph;
        constexpr std::wstring_view nested = L"ref[^n]\r\n\r\n[^n]: - item\r\n";
        const auto nestedText              = nested.find(L"item");
        Exact(nested, {nestedText + 2u, nestedText + 2u}, request, L"ref[^n]\r\n\r\n[^n]: - it\r\n    - em\r\n", std::nullopt, MarkdownProfile::RichMarkdown);
    }
    request.command = MarkdownCommand::ToggleStyle;
    for (const auto style : {MarkdownTextStyle::Strong, MarkdownTextStyle::Emphasis, MarkdownTextStyle::Strikethrough})
    {
        request.style             = style;
        const std::wstring marker = style == MarkdownTextStyle::Strong ? L"**" : style == MarkdownTextStyle::Emphasis ? L"*" : L"~~";
        Exact(L"word", {4u, 0u}, request, marker + L"word" + marker, Selection{marker.size() + 4u, marker.size()});
        const auto source = marker + L"abc" + marker;
        Exact(source, {marker.size() + 1u, marker.size() + 2u}, request, marker + L"a" + marker + L"b" + marker + L"c" + marker);
        Exact(source, {marker.size() + 1u, marker.size() + 1u}, request, L"abc");
    }
    request.style = MarkdownTextStyle::Code;
    Exact(L"&copy; **bold**", {0u, 15u}, request, L"`\u00a9 bold`", Selection{1u, 7u});
    Exact(L"**left &amp; right**", {7u, 12u}, request, L"**left `&` right**", Selection{8u, 9u});
    Exact(L"`*foo* &copy;`", {2u, 2u}, request, L"\\*foo\\* \\&copy\\;");
    Exact(L"\\`", {0u, 2u}, request, L"`` ` ``", Selection{3u, 4u});
    request.command     = MarkdownCommand::SetLink;
    request.destination = L"https://example.invalid/?x=1&y=2";
    request.title       = L"A \"quote\"";
    Exact(L"**word**", {8u, 0u}, request, L"[**word**](<https://example.invalid/?x=1&amp;y=2> \"A &quot;quote&quot;\")", Selection{9u, 1u});
    request.destination = L"next";
    request.title       = L"title";
    Exact(L"<A  TITLE='old' HREF = \"old\">word</A>", {30u, 30u}, request, L"<A  TITLE='title' HREF = \"next\">word</A>");
    request.command = MarkdownCommand::RemoveLink;
    Exact(L"[label][ref]\n\n[ref]: target\n", {2u, 2u}, request, L"label\n\n[ref]: target\n", Selection{0u, 5u});
    Exact(L"<a href='old'>word</a>", {15u, 15u}, request, L"word");
    request.command = MarkdownCommand::ToggleTask;
    Exact(L"> + [X] task\r\n", {10u, 10u}, request, L"> + [ ] task\r\n");
    Exact(L"- [ ] task\n", {8u, 8u}, request, L"- [x] task\n");
    Exact(L"word", {2u, 2u}, request, L"- [ ] word");
    Exact(L"- [x] a\n- [ ] b\n", {0u, 16u}, request, L"- [ ] a\n- [x] b\n");
    request.command = MarkdownCommand::InsertParagraph;
    Exact(L"> 9) [x] alpha\r\n", {14u, 14u}, request, L"> 9) [x] alpha\r\n> 10) [ ] \r\n");
    Exact(L"> ***", {5u, 5u}, request, L"> ***\r\n", Selection{7u, 7u});
    request.command  = MarkdownCommand::SetList;
    request.listKind = MarkdownListKind::Unordered;
    Exact(L"- [x] **word**\n", {8u, 8u}, request, L"- **word**\n");

    request         = {};
    request.command = MarkdownCommand::ToggleStyle;
    request.style   = MarkdownTextStyle::Strong;
    Unavailable(L"**ab**c", {3u, 7u}, request);
    request.command     = MarkdownCommand::SetLink;
    request.destination = L"https://example.invalid/";
    Unavailable(L"**ab**c", {3u, 7u}, request);

    request         = {};
    request.command = MarkdownCommand::InsertParagraph;
    Unavailable(L"- > quote", {9u, 9u}, request);

    request         = {};
    request.command = MarkdownCommand::ToggleStyle;
    request.style   = MarkdownTextStyle::Code;
    Unavailable(L"see $E=mc^2$ here", {0u, 17u}, request, MarkdownProfile::RichMarkdown);
    Unavailable(L"note[^1] end", {0u, 12u}, request, MarkdownProfile::RichMarkdown);
    Unavailable(L"[label](https://example.invalid/)", {0u, 33u}, request);

    request         = {};
    request.command = MarkdownCommand::ToggleStyle;
    request.style   = MarkdownTextStyle::Strong;
    Exact(L"**Hello, world! (see e.g. foo)**", {3u, 3u}, request, L"Hello, world! (see e.g. foo)");
    Exact(L"Hello, world! (see e.g. foo)", {0u, 28u}, request, L"**Hello, world! (see e.g. foo)**");

    request             = {};
    request.command     = MarkdownCommand::SetLink;
    request.destination = L"next";
    Exact(L"<a href='old' title='tip'>word</a>", {28u, 28u}, request, L"<a href='next'>word</a>", Selection{17u, 17u});
}
inline void Tables()
{
    MarkdownCommandRequest request;
    request.command = MarkdownCommand::InsertTable;
    Exact(L"", {}, request, L"| | |\r\n| --- | --- |\r\n| | |", Selection{2u, 2u});
    request.command = MarkdownCommand::InsertTableRow;
    Exact(L"a | b\r\n:- | -:\r\nx | y", {0u, 0u}, request, L"a | b\r\n:- | -:\r\n| | |\r\nx | y");
    request.command = MarkdownCommand::DeleteTableRow;
    Exact(L"a | b\n- | -\nx | y\nz | q\n", {12u, 12u}, request, L"a | b\n- | -\nz | q\n");
    Exact(L"| a | b |\n| :- | -: |\n| x | y |\n", {2u, 2u}, request, L"| x | y |\n| :- | -: |\n");
    request.command = MarkdownCommand::InsertTableColumn;
    Exact(L"| a | b |\n| :- | -: |\n| x |\n", {6u, 6u}, request, L"| a |  | b |\n| :- | --- | -: |\n| x |  |\n");
    request.command = MarkdownCommand::DeleteTableColumn;
    Exact(L"| a | b |\n| :- | -: |\n| x | y |\n", {2u, 2u}, request, L"| b |\n| -: |\n| y |\n");
    request.command = MarkdownCommand::SetTableCell;
    request.text    = L"**new**\\|value";
    Exact(L"a | b\n- | -\nx | y\n", {16u, 16u}, request, L"a | b\n- | -\nx | **new**\\|value\n");
    request.text                 = L"()";
    request.tableCellCaretOffset = 1u;
    Exact(L"a | b\n- | -\nx | y\n", {16u, 16u}, request, L"a | b\n- | -\nx | ()\n", Selection{17u, 17u});
    Document caretBoundarySource(L"a | b\n- | -\nx | y\n");
    const auto caretBoundarySyntax = Parse(caretBoundarySource);
    request.text                   = L"\U0001f600";
    Reject<std::out_of_range>([&] {
        static_cast<void>(BuildMarkdownCommand(caretBoundarySource.Snapshot(), caretBoundarySource.Revision(), caretBoundarySyntax, {16u, 16u}, request));
    }, "table replacement caret cannot split a supplementary scalar");
    request.tableCellCaretOffset.reset();
    Document padded(L"| a | b | c |\r\n| :- | -: | --- |\r\n| x |\r\n");
    const auto syntax = Parse(padded);
    std::vector<std::size_t> cells(0u);
    for (std::size_t i = 0u; i < syntax->Nodes().size(); ++i)
        if (syntax->Nodes()[i].block.kind == MarkdownBlockKind::TableCell)
            cells.push_back(i);
    Check(cells.size() == 6u, "padded oracle contains six declared cells");
    request.targetCell = cells.back();
    request.text       = L"new";
    Exact(padded.Text(), {padded.Text().size(), padded.Text().size()}, request, L"| a | b | c |\r\n| :- | -: | --- |\r\n| x |  | new |\r\n");
    request.text                 = L"()";
    request.tableCellCaretOffset = 1u;
    const std::wstring pairedCell(L"| a | b | c |\r\n| :- | -: | --- |\r\n| x |  | () |\r\n");
    const auto pairedCaret = pairedCell.find(L"()") + 1u;
    Exact(padded.Text(), {padded.Text().size(), padded.Text().size()}, request, pairedCell, Selection{pairedCaret, pairedCaret});
    request.tableCellCaretOffset.reset();
    request.targetCell = MarkdownNoNode;
    request.command    = MarkdownCommand::NextTableCell;
    Exact(L"a | b\n- | -\nx | y", {16u, 16u}, request, L"a | b\n- | -\nx | y\n| | |", Selection{20u, 20u});
    request.command = MarkdownCommand::PreviousTableCell;
    Exact(L"a | b\n- | -\nx | y", {16u, 16u}, request, L"a | b\n- | -\nx | y", Selection{12u, 12u});

    request         = {};
    request.command = MarkdownCommand::DeleteTableColumn;
    Exact(L"| a | b |\n| - | -: |\n| x |\n", {2u, 2u}, request, L"| b |\n| -: |\n| |\n", Selection{2u, 2u});

    request         = {};
    request.command = MarkdownCommand::InsertTableColumn;
    Exact(L"a | b\n- | -\nx | y\n", {0u, 0u}, request, L"| | a | b\n| --- | - | -\n| | x | y\n", Selection{2u, 2u});

    request         = {};
    request.command = MarkdownCommand::DeleteTableRow;
    Exact(L"| a |\n| - |\n| x |", {2u, 2u}, request, L"| x |\n| - |\n");
    Exact(L"| a | b |\n| - | - |\n|😀|x|\n", {2u, 2u}, request, L"|😀|x|\n| - | - |\n", Selection{1u, 1u});
}
inline void GuardsAndDeletion()
{
    MarkdownCommandRequest request;
    request.command = MarkdownCommand::ToggleStyle;
    Document document(L"**alpha**\r\n");
    document.SetSelection({2u, 7u});
    const auto syntax   = Parse(document);
    const auto source   = document.Snapshot();
    const auto revision = document.Revision();
    const auto plan     = BuildMarkdownCommand(source, revision, syntax, document.GetSelection(), request);
    document.SetSelection({7u, 2u});
    Reject<std::invalid_argument>([&] { ApplyMarkdownCommand(document, plan); }, "direction/selection staleness rejects the whole plan");
    document.SetSelection({2u, 7u});
    document.Replace(2u, 3u, L"A", {2u, 7u});
    document.Undo();
    Reject<std::invalid_argument>([&] { ApplyMarkdownCommand(document, plan); }, "undo returning to same spelling does not revive old revision");
    Reject<std::invalid_argument>([&] { BuildMarkdownCommand(document.Snapshot(), document.Revision(), syntax, document.GetSelection(), request); },
                                  "stale analysis never reparsed by command");
    for (const auto command : {MarkdownCommand::ToggleTask, MarkdownCommand::InsertTable})
    {
        Document plain(L"word");
        const auto analysis = Parse(plain, MarkdownProfile::CommonMark0312);
        request.command     = command;
        Check(! BuildMarkdownCommand(plain.Snapshot(), plain.Revision(), analysis, {}, request).IsApplicable(),
              "GFM commands respect explicit CommonMark profile");
    }
    request.command      = MarkdownCommand::SetHeading;
    request.headingLevel = 2u;
    Document mixed(L"word\n\n```\ncode\n```\n");
    mixed.SetSelection({0u, mixed.Text().size()});
    Check(! BuildMarkdownCommand(mixed.Snapshot(), mixed.Revision(), Parse(mixed), mixed.GetSelection(), request).IsApplicable(),
          "mixed opaque selection rejects atomically");
    request.headingLevel = 7u;
    Reject<std::invalid_argument>([&] { BuildMarkdownCommand(source, revision, syntax, {2u, 7u}, request); }, "invalid heading rejected");
    Document unfinished(L"```\n");
    request.headingLevel = 2u;
    Check(! BuildMarkdownCommand(unfinished.Snapshot(), unfinished.Revision(), Parse(unfinished), {4u, 4u}, request).IsApplicable(),
          "empty EOF inside an unclosed fence is opaque");
    Document literal(L"`abc`");
    request.command = MarkdownCommand::ToggleStyle;
    Check(! BuildMarkdownCommand(literal.Snapshot(), literal.Revision(), Parse(literal), {2u, 3u}, request).IsApplicable(),
          "non-code style cannot mutate literal code interiors");
    Document tabs(L"1. a\n2.\tb\n");
    request.command = MarkdownCommand::IndentList;
    Check(! BuildMarkdownCommand(tabs.Snapshot(), tabs.Revision(), Parse(tabs), {8u, 8u}, request).IsApplicable(),
          "indentation rejects marker tabs whose new stop would change descendants");
    const std::wstring oversized(Document::MaximumUnits + 1u, L'x');
    request.command     = MarkdownCommand::SetLink;
    request.destination = oversized;
    Reject<std::length_error>([&] { BuildMarkdownCommand(source, revision, syntax, {2u, 7u}, request); },
                              "unbounded borrowed metadata rejected before staging");
    Document ambiguousTask(L"- [x] - literal\n");
    MarkdownCommandRequest list;
    list.command = MarkdownCommand::SetList;
    Check(! BuildMarkdownCommand(ambiguousTask.Snapshot(), ambiguousTask.Revision(), Parse(ambiguousTask), {8u, 8u}, list).IsApplicable(),
          "removing task syntax rejects a label that would become another block marker");
    std::wstring overflow(L"a | b\n- | -\n");
    for (unsigned cell = 0u; cell < 65u; ++cell)
    {
        if (cell)
            overflow += L" | ";
        overflow += L'x';
    }
    overflow += L'\n';
    Document largePromotion(overflow);
    MarkdownCommandRequest table;
    table.command = MarkdownCommand::DeleteTableRow;
    Reject<std::length_error>([&] { BuildMarkdownCommand(largePromotion.Snapshot(), largePromotion.Revision(), Parse(largePromotion), {}, table); },
                              "promoting ignored overflow fields cannot bypass declared table command limits");
    const std::vector<MarkdownRange> fragments(SourceDisplayMap::MaximumSpans + 1u);
    Reject<std::length_error>([&] { BuildMarkdownDeletion(source, revision, syntax, {2u, 7u}, fragments); },
                              "deletion fragment amplification rejected before staging");
    request.command     = MarkdownCommand::SetLink;
    request.destination = L"javascript:alert(1)";
    Reject<std::invalid_argument>([&] { BuildMarkdownCommand(source, revision, syntax, {2u, 7u}, request); },
                                  "active link destination rejected before mutation");
    Document maximum(std::wstring(Document::MaximumUnits, L'x'));
    maximum.SetSelection({0u, 1u});
    request.command = MarkdownCommand::ToggleStyle;
    Reject<std::length_error>([&] { BuildMarkdownCommand(maximum.Snapshot(), maximum.Revision(), Parse(maximum), maximum.GetSelection(), request); },
                              "command capacity checked before apply");
    for (const std::wstring_view spelling : {L"**[abc](target)**", L"<b><a href='target'>abc</a></b>"})
    {
        Document deleting(spelling);
        const auto at = spelling.find(L"abc");
        deleting.SetSelection({at + 3u, at});
        const auto old      = deleting.Snapshot();
        const auto analysis = Parse(deleting);
        const std::array ranges{MarkdownRange{at, at + 3u}};
        const auto deletion = BuildMarkdownDeletion(old, deleting.Revision(), analysis, deleting.GetSelection(), ranges);
        Check(ApplyMarkdownCommand(deleting, deletion) && deleting.Text().empty(), "whole visual body recursively cleans owned formatting/link wrappers");
        Check(deleting.Undo() && deleting.Snapshot().IsSameRevision(old), "cleanup restores exact original spelling in one undo");
    }
}
} // namespace RichCommandTests
