#pragma once
#include <DxUi/RichSemantic.h>
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

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
    Reject<std::invalid_argument>([&] { SourceDisplayMap::Build(L"", std::wstring_view(L"\0", 1u), {}); }, "NUL admission remains unchanged");
    Reject<std::length_error>([&] { SourceDisplayMap::Build(std::wstring(Document::MaximumUnits + 1u, L'x'), L"", {}); }, "source capacity bound");
    Reject<std::length_error>([&] { SourceDisplayMap::Build(L"", std::wstring(SourceDisplayMap::MaximumDisplayUnits + 1u, L'x'), {}); },
                              "display amplification bound");
    const std::wstring maximum(Document::MaximumUnits, L'x');
    const auto admitted = SourceDisplayMap::Build(maximum, maximum, std::array{DisplaySpan{{0u, maximum.size()}, {0u, maximum.size()}}});
    Check(admitted->ToDisplay(maximum.size()) == maximum.size(), "exact maximum source admitted");
}
inline void TransactionOracles()
{
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
    const auto reject = [&]<typename Exception>(std::span<const SourceEdit> edits, Selection after)
    {
        Reject<Exception>([&] { ApplySourceEdits(document, source, revision, edits, after); }, "invalid source edit plan must fail");
        Check(document.Text() == before && document.Revision() == revision && document.GetSelection() == selection && document.CanUndo() == undo &&
                  document.CanRedo() == redo && document.Snapshot().IsSameRevision(source),
              "rejected edit plan preserves complete published state");
    };
    reject.operator()<std::out_of_range>(std::array{SourceEdit{{1u, 2u}, L""}}, {});
    reject.operator()<std::out_of_range>(std::array{SourceEdit{{3u, 4u}, L""}}, {});
    reject.operator()<std::invalid_argument>(std::array{SourceEdit{{0u, 3u}, L""}, SourceEdit{{1u, 3u}, L""}}, {});
    reject.operator()<std::invalid_argument>(std::array{SourceEdit{{5u, 6u}, L""}, SourceEdit{{0u, 1u}, L""}}, {});
    reject.operator()<std::invalid_argument>(std::array{SourceEdit{{1u, 1u}, L"a"}, SourceEdit{{1u, 1u}, L"b"}}, {});
    reject.operator()<std::invalid_argument>(std::array{SourceEdit{{0u, 1u}, std::wstring_view(L"\0", 1u)}}, {});
    reject.operator()<std::out_of_range>(std::array{SourceEdit{{0u, 1u}, L"\r"}}, {2u, 2u});
    const std::wstring tooLarge(Document::MaximumUnits, L'x');
    reject.operator()<std::length_error>(std::array{SourceEdit{{0u, 0u}, tooLarge}}, {});
    reject.operator()<std::out_of_range>({}, {7u, 0u});
    Document join(L"aX\nb");
    const auto joinSnapshot = join.Snapshot();
    const auto joinRevision = join.Revision();
    Reject<std::out_of_range>([&] { ApplySourceEdits(join, joinSnapshot, joinRevision, std::array{SourceEdit{{0u, 2u}, L"\r"}}, {1u, 1u}); },
                              "future CRLF join selection rejected before publication");
    Check(join.Text() == L"aX\nb" && join.Revision() == joinRevision, "join failure is atomic");
}
} // namespace RichSemanticTests
