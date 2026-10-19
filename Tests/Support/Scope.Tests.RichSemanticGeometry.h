#pragma once
#include "../../src/Controls/DxUi.RichSemanticGeometry.h"
#include "Scope.Tests.RichSemanticDocument.h"
#include <cmath>
#include <wil/result.h>

namespace RichSemanticGeometryTests
{
using namespace DxUi::RichText;
using DxUi::RichText::Detail::PreparedSemanticGeometry;
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
} // namespace RichSemanticGeometryTests
