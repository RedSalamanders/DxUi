#pragma once
#include "../../src/Controls/DxUi.RichSemanticGeometry.h"
#include "Scope.Tests.RichSemanticDocument.h"
#include <cmath>
#include <wil/result.h>

namespace RichSemanticGeometryTests
{
using namespace DxUi::RichText;
using DxUi::RichText::Detail::PreparedSemanticGeometry;
using DxUi::RichText::Detail::SourceCaretHit;
using RichSemanticTests::Check;
inline wil::com_ptr<IDWriteFactory> Factory()
{
    wil::com_ptr<IDWriteFactory> result;
    THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(result.put())));
    return result;
}
inline void Complete(PreparedSemanticGeometry& geometry)
{
    std::size_t steps = 0u;
    while (geometry.Pending())
    {
        const auto result = geometry.Advance({1u, 3u});
        Check(result.layoutsPrepared <= 1u && result.blockSteps <= 3u, "semantic geometry respects independent SDK/event budgets");
        Check(++steps < 100000u, "semantic geometry finishes its bounded staging graph");
    }
    Check(geometry.Ready(), "semantic geometry publishes one complete ready state");
}
inline void ParagraphsTablesAndSourceRanges()
{
    auto factory              = Factory();
    const std::wstring source = L"# Heading\n\n| left | right |\n| :--- | ---: |\n| a | b |\n\n> quote **strong**\n\n![alt](local)\n";
    const auto document       = RichSemanticDocumentTests::Build(source);
    PreparedSemanticGeometry geometry;
    LayoutOptions options;
    options.widthDip = 300.0f;
    geometry.Begin(factory.get(), document, options);
    Check(geometry.Pending() && ! geometry.Ready(), "new semantic geometry never exposes partially placed cells");
    Complete(geometry);
    Check(geometry.Revision() == document->Syntax()->Revision() && geometry.Height() > 0.0f && &geometry.Document() == document.get(),
          "geometry publication retains its exact semantic source revision");
    const auto generation = geometry.Generation();
    Check(geometry.Begin(factory.get(), document, options) == LayoutPreparationStatus::Complete && ! geometry.Pending() && geometry.Generation() == generation,
          "completed semantic geometry request coalesces without a new generation");
    Check(geometry.SourceRange({0u, 2u}).empty(), "hidden heading syntax never receives a viewport rectangle");
    const auto left = source.find(L"left"), right = source.find(L"right"), quote = source.find(L"quote");
    const auto leftRectangles  = geometry.SourceRange({left, left + 4u});
    const auto rightRectangles = geometry.SourceRange({right, right + 5u});
    Check(! leftRectangles.empty() && ! rightRectangles.empty() && std::abs(leftRectangles.front().left - 6.0f) < .1f &&
              std::abs(rightRectangles.back().right - 294.0f) < .1f,
          "LTR table glyphs obey physical left/right alignment and cell padding");
    Check(std::abs(leftRectangles.front().top - rightRectangles.front().top) < .1f, "same table row cells share one prepared origin");
    const auto quoteCaret = geometry.Caret(document->Map().ToDisplay(quote));
    Check(quoteCaret.x >= 16.0f && quoteCaret.height > 0.0f, "quote text has retained indentation and real SDK caret metrics");
    const auto imageSource = source.find(L"local");
    Check(! geometry.SourceRange({imageSource, imageSource + 5u}).empty(), "atomic image metadata maps to its displayed alternative");
    const auto visible = geometry.VisibleRange(0.0f, geometry.Height());
    Check(visible.first <= visible.second && visible.second <= document->Map().Display().size(), "semantic visible ranges remain bounded display intervals");
    options.readingDirection = DWRITE_READING_DIRECTION_RIGHT_TO_LEFT;
    geometry.Begin(factory.get(), document, options);
    Complete(geometry);
    const auto rtlLeft  = geometry.SourceRange({left, left + 4u});
    const auto rtlRight = geometry.SourceRange({right, right + 5u});
    Check(! rtlLeft.empty() && ! rtlRight.empty() && std::abs(rtlLeft.front().left - 156.0f) < .1f && std::abs(rtlRight.back().right - 144.0f) < .1f,
          "RTL table columns reverse while declared physical alignment stays explicit");
    geometry.Invalidate();
    Check(! geometry.Ready(), "semantic invalidation rejects stale geometry immediately");
    RichSemanticTests::Reject<std::logic_error>([&] { geometry.Caret(0u); }, "invalidated semantic geometry is unavailable");
    Check(geometry.Begin(factory.get(), document, options) == LayoutPreparationStatus::Complete && geometry.Ready(),
          "matching invalidated geometry can be acknowledged again");
    geometry.Clear();
    Check(! geometry.Ready() && ! geometry.Pending() && geometry.Height() == 0.0f, "semantic retirement releases both staged and published SDK graphs");
}
inline void AtomicNavigationAndCancellation()
{
    auto factory        = Factory();
    const auto document = RichSemanticDocumentTests::Build(L"&fjlig; A\U0001f600\u0301\n");
    PreparedSemanticGeometry geometry;
    LayoutOptions options;
    options.widthDip = 180.0f;
    geometry.Begin(factory.get(), document, options);
    Complete(geometry);
    Check(geometry.NextCluster(0u) == 2u && geometry.PreviousCluster(2u) == 0u, "decoded two-scalar entity remains one visual edit stop");
    Check(geometry.DeletionRange(1u, false) == std::pair<std::size_t, std::size_t>{0u, 2u} &&
              geometry.DeletionRange(1u, true) == std::pair<std::size_t, std::size_t>{0u, 2u},
          "interior decoded token deletion expands in either direction");
    const auto last = geometry.Document().Map().Display().find(L"\U0001f600");
    Check(geometry.NextCluster(last) == last + 3u, "semantic navigation keeps supplementary-plus-combining graphemes together");
    const auto generation = geometry.Generation();
    const auto previous   = geometry.ElementBounds(1u);
    auto different        = options;
    different.widthDip    = 80.0f;
    geometry.Begin(factory.get(), document, different);
    RichSemanticTests::Reject<std::invalid_argument>([&] { geometry.Advance({0u, 1u}); }, "zero SDK budget rejected before changing staging");
    Check(geometry.Pending() && geometry.Generation() == generation, "rejected advance preserves its prior request and publication");
    geometry.Cancel();
    Check(geometry.Ready() && ! geometry.Pending() && geometry.Generation() == generation && geometry.ElementBounds(1u).has_value() == previous.has_value(),
          "semantic cancellation preserves the previous complete geometry");
    const auto empty = RichSemanticDocumentTests::Build(L"");
    geometry.Begin(factory.get(), empty, options);
    Complete(geometry);
    Check(geometry.Caret(0u).height > 0.0f && geometry.HitTest(0.0f, 0.0f).position == 0u,
          "empty semantic document has an SDK caret without invented source text");
    RichSemanticTests::Reject<std::out_of_range>([&] { geometry.Range(0u, 1u); }, "semantic range bound validation");
}
inline void SourceCaretSites()
{
    auto factory = Factory();
    LayoutOptions options;
    options.widthDip = 360.0f;
    std::wstring source(0u, L' ');
    std::vector<std::size_t> insertions(0u);
    const std::array<std::pair<std::wstring_view, std::wstring_view>, 6u> lines{
        {{L"", L"\r\n"}, {L" \t", L"\r"}, {L">", L"\r\n"}, {L"> >", L"\n"}, {L"- ", L"\r\n"}, {L"", L"\n"}}};
    for (const auto& [body, eol] : lines)
    {
        insertions.push_back(source.size() + body.size());
        source.append(body);
        source.append(eol);
    }
    source.append(L"text\r\n");
    insertions.push_back(source.size());
    const auto document = RichSemanticDocumentTests::Build(source);
    Check(document->Syntax()->EmptyLines().size() == insertions.size(), "block recognizer preserves physical empty/container/trailing source sites");
    PreparedSemanticGeometry geometry;
    geometry.Begin(factory.get(), document, options);
    Complete(geometry);
    float previousY = -1.0f;
    for (std::size_t i = 0u; i < insertions.size(); ++i)
    {
        const auto& line = document->Syntax()->EmptyLines()[i];
        Check(line.insertion == insertions[i] && IsEditBoundary(source, line.insertion) && line.owner < document->Syntax()->Nodes().size(),
              "source caret metadata retains exact admitted raw ACPs and current container owners");
        const auto caret = geometry.SourceCaret(line.insertion);
        Check(caret.height > 0.0f && caret.y > previousY, "every empty/container/trailing physical source line has distinct native vertical geometry");
        const auto hit = geometry.HitTestSource(caret.x, caret.y + caret.height * .5f);
        Check(hit.position == line.insertion && hit.site != MarkdownNoNode, "empty source caret and pointer hit round-trip without invented source units");
        const auto boundary = geometry.SourceLineBoundary(line.insertion, hit.site, true);
        Check(boundary.position == line.insertion && boundary.site == hit.site, "Home/End preserve the empty visual line affinity");
        Check(geometry.SourceRange({line.source.first, line.source.last}).empty(), "empty caret scaffolding contributes no source-selection glyphs");
        if (i == 2u || i == 3u)
            Check(caret.x >= 16.0f * static_cast<float>(i - 1u), "empty nested quotes retain parsed container indentation");
        previousY = caret.y;
    }
    const auto partitions = geometry.SourceLines();
    Check(! partitions.empty() && partitions.front().first == 0u && partitions.back() == std::pair{source.size(), source.size()},
          "source line units include the final raw insertion line");
    for (std::size_t i = 1u; i < partitions.size(); ++i)
        Check(partitions[i - 1u].second == partitions[i].first, "empty visual source lines retain ordered gap-free ACP partitions");
    RichSemanticTests::Reject<std::out_of_range>([&] { geometry.SourceCaret(1u); }, "source caret rejects the interior of a CRLF pair");

    const std::wstring table = L"| a | b | c |\n| - | - | - |\n| value |\n";
    const auto cells         = RichSemanticDocumentTests::Build(table);
    for (const bool rtl : {false, true})
    {
        options.readingDirection = rtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
        geometry.Begin(factory.get(), cells, options);
        Complete(geometry);
        const auto anchor = table.rfind(L'\n');
        std::array<SourceCaretHit, 2u> hits{};
        for (std::size_t column = 1u; column < 3u; ++column)
        {
            const auto physical  = rtl ? 2u - column : column;
            const auto valueRect = geometry.SourceRange({table.find(L"value"), table.find(L"value") + 5u});
            hits[column - 1u]    = geometry.HitTestSource((static_cast<float>(physical) + .5f) * 120.0f, valueRect.front().top + 5.0f);
            Check(hits[column - 1u].position == anchor, "padded cells expose the real end-of-row ACP in both flow directions");
        }
        Check(hits[0].site != hits[1].site, "padded cells with equal raw ACPs have distinct native site identities");
        const auto first = geometry.SourceCaret(anchor, hits[0].site), second = geometry.SourceCaret(anchor, hits[1].site);
        Check(std::abs(first.x - second.x) > 100.0f && std::abs(first.y - second.y) < .1f,
              "padded cell affinity disambiguates caret geometry without text mutation");
        const auto next = geometry.StepSource(anchor, hits[0].site, true);
        Check(next.position == anchor && next.site == hits[1].site, "logical Right can move between padded cells without changing the raw ACP");
        const auto previous = geometry.StepSource(anchor, hits[1].site, false);
        Check(previous.position == anchor && previous.site == hits[0].site, "logical Left reverses an equal-ACP cell move");
        auto narrower     = options;
        narrower.widthDip = 240.0f;
        geometry.Begin(factory.get(), cells, narrower);
        Complete(geometry);
        const auto reflowed = geometry.SourceCaret(anchor, hits[1].site);
        Check(reflowed.height > 0.0f && geometry.HitTestSource(reflowed.x, reflowed.y + reflowed.height * .5f).site == hits[1].site,
              "same-source reflow retains equal-ACP cell identity across layout widths");
        geometry.Begin(factory.get(), cells, options);
        geometry.Cancel();
        Check(geometry.SourceCaret(anchor, hits[1].site).x == reflowed.x, "cancelled reflow retains the previous complete source caret site");
        geometry.Invalidate();
        RichSemanticTests::Reject<std::logic_error>([&] { geometry.SourceCaret(anchor, hits[1].site); },
                                                    "invalidated source caret sites reject stale geometry");
        geometry.Begin(factory.get(), cells, narrower);
        Complete(geometry);
    }
    for (const auto suffix : {L"\r\n", L"\r", L""})
    {
        const std::wstring ending     = std::wstring(L"| a | b | c |\r\n| - | - | - |\r\n| value |") + suffix;
        const auto documentWithEnding = RichSemanticDocumentTests::Build(ending);
        geometry.Begin(factory.get(), documentWithEnding, options);
        Complete(geometry);
        const auto value      = ending.find(L"value");
        const auto rectangles = geometry.SourceRange({value, value + 5u});
        const auto hit        = geometry.HitTestSource(180.0f, rectangles.front().top + 5.0f);
        Check(hit.position == ending.size() - std::wstring_view(suffix).size() && IsEditBoundary(ending, hit.position),
              "padded cell insertion is before whole CRLF/CR or exact un-terminated row EOF");
    }
    for (const auto text : {L"", L"[ref]: local", L"```\n\n\n```\n", L"```\n\n", L"#\n", L"-\n", L">\n"})
    {
        const auto special = RichSemanticDocumentTests::Build(text);
        geometry.Begin(factory.get(), special, options);
        Complete(geometry);
        Check(geometry.SourceCaret(std::wstring_view(text).size()).height > 0.0f && geometry.BoxCount() > 0u,
              "empty/hidden/code/heading/marker documents retain a source EOF caret");
        if (std::wstring_view(text) == L"```\n\n")
            Check(special->Syntax()->EmptyLines().empty() && geometry.BoxCount() == 1u, "unclosed code-body blank lines do not acquire duplicate scaffolding");
    }
    const auto previousDocument   = &geometry.Document();
    const auto previousGeneration = geometry.Generation();
    const auto overflow           = RichSemanticDocumentTests::Build(std::wstring(SemanticDocument::MaximumParagraphs + 1u, L'\n'));
    geometry.Begin(factory.get(), overflow, options);
    std::size_t sdkCalls = 0u;
    bool rejected        = false;
    try
    {
        while (geometry.Pending())
            sdkCalls += geometry.Advance({1u, 3u}).layoutsPrepared;
    }
    catch (const std::length_error&)
    {
        rejected = true;
    }
    Check(rejected && sdkCalls == 0u && geometry.Ready() && ! geometry.Pending() && &geometry.Document() == previousDocument &&
              geometry.Generation() == previousGeneration,
          "caret-site admission rejects before SDK calls and preserves the prior complete native publication");
    for (const auto sourceText : {L"- [ ] one", L"- words", L"# heading", L"**bold**"})
    {
        const auto prefixed = RichSemanticDocumentTests::Build(sourceText);
        geometry.Begin(factory.get(), prefixed, options);
        Complete(geometry);
        Check(geometry.StepSource(0u, MarkdownNoNode, false).position == 0u, "source-zero Left never follows a generated marker anchor forward into content");
    }
    const auto checkLines = [&](std::wstring_view text, std::initializer_list<std::pair<std::size_t, std::size_t>> expected)
    {
        const auto lineDocument = RichSemanticDocumentTests::Build(text);
        geometry.Begin(factory.get(), lineDocument, options);
        Complete(geometry);
        Check(std::ranges::equal(geometry.SourceLines(), expected),
              "source line units attach original EOLs to their line and expose exactly one empty EOF unit");
    };
    checkLines(L"a\n", {{0u, 2u}, {2u, 2u}});
    checkLines(L"a\r\n", {{0u, 3u}, {3u, 3u}});
    checkLines(L"a\n\nb", {{0u, 2u}, {2u, 3u}, {3u, 4u}});
    checkLines(L"a\r\n\r\nb\r\n", {{0u, 3u}, {3u, 5u}, {5u, 8u}, {8u, 8u}});
    checkLines(L"**a**\r\n\r\nb", {{0u, 7u}, {7u, 9u}, {9u, 10u}});
    checkLines(L"\r\n\r\n", {{0u, 2u}, {2u, 4u}, {4u, 4u}});
    checkLines(L"", {{0u, 0u}});
    for (const auto sourceText : {L"&amp;", L"&fjlig;", L"![alt](local)"})
    {
        const auto atomicDocument = RichSemanticDocumentTests::Build(sourceText);
        geometry.Begin(factory.get(), atomicDocument, options);
        Complete(geometry);
        Check(geometry.StepSource(1u, MarkdownNoNode, false).position == 0u &&
                  geometry.StepSource(1u, MarkdownNoNode, true).position == std::wstring_view(sourceText).size(),
              "raw interior entity/image ACPs move to the atomic source boundary in either direction");
    }
}
} // namespace RichSemanticGeometryTests
