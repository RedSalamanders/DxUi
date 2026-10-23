#pragma once
#include "../../src/Controls/DxUi.RichSemanticGeometry.h"
#include "Scope.Tests.RichSemanticDocument.h"
#include <cmath>
#include <limits>
#include <wil/resource.h>
#include <wil/result.h>
#include <wincodec.h>

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
inline void Complete(PreparedSemanticGeometry& geometry, std::size_t* layoutCalls = nullptr)
{
    std::size_t steps = 0u;
    while (geometry.Pending())
    {
        const auto result = geometry.Advance({1u, 3u});
        if (layoutCalls)
            *layoutCalls += result.layoutsPrepared;
        Check(result.layoutsPrepared <= 1u && result.blockSteps <= 3u, "semantic geometry respects independent SDK/event budgets");
        Check(++steps < 100000u, "semantic geometry finishes its bounded staging graph");
    }
    Check(geometry.Ready(), "semantic geometry publishes one complete ready state");
}
inline void ReorderedFootnoteCaretSitesAndSourceLines()
{
    auto factory                    = Factory();
    const std::wstring_view cases[] = {
        L"[^z]: Z body\n[^a]: A first\n\n    A second\n\nmain[^a] then[^z].\n",
        L"[^n]:\n\nmain[^n]\n",
        L"main[^n]\r\n\r\n[^n]: first\r\n\r\n    second\r\n",
        L"[^n]:\n    | h | other |\n    | --- | --- |\n    | x |\n\nmain[^n]\n",
    };
    for (const auto source : cases)
    {
        const auto document = RichSemanticDocumentTests::Build(source, MarkdownProfile::RichMarkdown);
        PreparedSemanticGeometry geometry;
        LayoutOptions options;
        options.widthDip = 360.0f;
        geometry.Begin(factory.get(), document, options);
        std::size_t turns{};
        while (geometry.Pending())
        {
            const auto step = geometry.Advance({1u, 1u});
            Check(step.blockSteps <= 1u && step.layoutsPrepared <= 1u && ++turns < 20000u,
                  "footnote sorting/indexing yields between declared owner events and SDK calls");
        }
        Check(geometry.Ready(), "reordered footnotes publish complete native geometry");
        std::size_t raw{};
        for (const auto& line : geometry.SourceLines())
        {
            Check(line.first == raw && line.second >= line.first && line.second <= source.size() && IsEditBoundary(source, line.first) &&
                      IsEditBoundary(source, line.second),
                  "source visual line units stay an ordered raw-source partition after footer reordering");
            raw = line.second;
        }
        Check(raw == source.size() && geometry.SourceLines().back() == std::pair{source.size(), source.size()},
              "reordered source lines include the exact source extent and one trailing EOF unit");
        if (source == cases[0u])
        {
            const std::pair<std::size_t, std::size_t> expected[] = {{0u, 13u}, {13u, 27u}, {27u, 28u}, {28u, 41u}, {41u, 42u}, {42u, 61u}, {61u, 61u}};
            Check(std::ranges::equal(geometry.SourceLines(), expected), "adjacent reordered definition lines retain their own exact original newline units");
        }
        const auto main      = source.find(L"main");
        const auto mainCaret = geometry.SourceCaret(main);
        for (const auto& note : document->Syntax()->Footnotes())
            if (note.ordinal && document->Syntax()->Nodes()[note.definition].block.source.last == source.size())
                Check(geometry.SourceCaret(source.size()).y > mainCaret.y,
                      "a trailing source EOF site follows its owning footer rather than the main-text segment");
        for (std::size_t i = 0u; i < document->Elements().size(); ++i)
            if (document->Elements()[i].role == SemanticRole::Footnote)
            {
                const auto bounds = geometry.ElementBounds(i);
                Check(bounds && bounds->top > mainCaret.y, "used definitions appear below main text regardless of their source position");
            }
        for (const auto needle : {L"Z body", L"A first", L"A second", L"second", L"x"})
        {
            const auto at = source.find(needle);
            if (at == std::wstring_view::npos)
                continue;
            const auto caret = geometry.SourceCaret(at);
            const auto hit   = geometry.HitTestSource(caret.x, caret.y + caret.height * .5f);
            Check(hit.position == at, "footer body hit testing returns the exact earlier raw source ACP");
        }
        for (const auto& empty : document->Syntax()->EmptyLines())
        {
            auto owner = empty.owner;
            while (owner != MarkdownNoNode && document->Syntax()->Nodes()[owner].block.kind != MarkdownBlockKind::FootnoteDefinition)
                owner = document->Syntax()->Nodes()[owner].parent;
            if (owner == MarkdownNoNode)
                continue;
            const auto caret = geometry.SourceCaret(empty.insertion);
            Check(caret.y > mainCaret.y, "definition-only and internal blank caret sites follow their footer instead of source-order main text");
        }
        const auto ranges = geometry.SourceRange({0u, source.size()});
        Check(! ranges.empty(), "whole-source selection paints reordered real text without owning generated backlinks");
    }
    const auto atomic = RichSemanticDocumentTests::Build(L"![a very long image alternative that wraps over many SDK lines](local)\n");
    PreparedSemanticGeometry wrapped;
    LayoutOptions narrow;
    narrow.widthDip = 36.0f;
    wrapped.Begin(factory.get(), atomic, narrow);
    Complete(wrapped);
    const auto end                                          = atomic->Map().Source().size();
    const std::pair<std::size_t, std::size_t> atomicLines[] = {{0u, end}, {end, end}};
    Check(std::ranges::equal(wrapped.SourceLines(), atomicLines), "a wrapped atomic alternative and its EOL own one raw line without an EOL-only duplicate");
    std::optional<float> footerTextEdge;
    for (const auto source : {L"[^n]: note\n\nmain[^n]\n",
                              L"- [^n]: note\n\nmain[^n]\n",
                              L"> [^n]: note\n\nmain[^n]\n",
                              L"> - [^n]: note\n\nmain[^n]\n",
                              L"- outer\n  [^n]: note\n\nmain[^n]\n"})
    {
        const auto footer = RichSemanticDocumentTests::Build(source, MarkdownProfile::RichMarkdown);
        PreparedSemanticGeometry relocated;
        LayoutOptions options;
        options.widthDip = 360.0f;
        relocated.Begin(factory.get(), footer, options);
        Complete(relocated);
        const auto caret = relocated.SourceCaret(std::wstring_view(source).find(L"note"));
        if (! footerTextEdge)
            footerTextEdge = caret.x;
        Check(std::abs(caret.x - *footerTextEdge) < .01f, "a root footer excludes its former outside list/quote gutter while preserving its own body geometry");
    }
}
inline void LiteralFootnoteBlockCaretSites()
{
    auto factory = Factory();
    for (const bool trailing : {false, true})
    {
        std::wstring source = L"[^n]: first\n\n    second";
        if (trailing)
            source += L'\n';
        const auto document = RichSemanticDocumentTests::Build(source, MarkdownProfile::RichMarkdown);
        PreparedSemanticGeometry geometry;
        LayoutOptions options;
        options.widthDip = 480.0f;
        geometry.Begin(factory.get(), document, options);
        Complete(geometry);
        Check(geometry.BoxCount() == (trailing ? 2u : 1u), "literal note child blank trivia cannot create duplicate visual boxes");
        const auto at    = source.find(L"second");
        const auto caret = geometry.SourceCaret(at);
        const auto hit   = geometry.HitTestSource(caret.x, caret.y + caret.height * .5f);
        Check(hit.position == at, "literal note body caret retains its raw indented source ACP");
    }
    const auto manyBlankSource = L"[^n]: first\n" + std::wstring(1024u, L'\n') + L"    second";
    const auto manyBlank       = RichSemanticDocumentTests::Build(manyBlankSource, MarkdownProfile::RichMarkdown);
    PreparedSemanticGeometry geometry;
    LayoutOptions options;
    options.widthDip = 480.0f;
    geometry.Begin(factory.get(), manyBlank, options);
    std::size_t turns{};
    while (geometry.Pending())
    {
        const auto result = geometry.Advance({1u, 1u});
        Check(result.blockSteps <= 1u && result.layoutsPrepared <= 1u, "literal blank-body preparation yields under an independent one-event/one-SDK budget");
        Check(++turns < 20000u, "large literal blank body completes without spinning an idle owner");
    }
    Check(geometry.Ready() && geometry.BoxCount() == 1u && geometry.SourceLines().size() == 1026u,
          "many literal note blank lines remain real source lines without creating duplicate layout boxes");
}
inline void LinkRasterOracle()
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    wil::com_ptr<ID2D1Factory> drawing;
    THROW_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, drawing.put()));
    auto factory      = Factory();
    const auto pixels = [&](std::wstring_view source, bool reference, bool underline)
    {
        constexpr UINT width = 128u, height = 48u, stride = width * 4u;
        wil::com_ptr<IWICBitmap> bitmap;
        THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
        wil::com_ptr<ID2D1RenderTarget> target;
        const auto properties = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
        target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        wil::com_ptr<ID2D1SolidColorBrush> brush;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(.2f, .4f, .7f), brush.put()));
        target->BeginDraw();
        target->Clear(D2D1::ColorF(D2D1::ColorF::White));
        if (reference)
        {
            // Independent SDK oracle: literal label, explicitly declared font,
            // dimensions and underline, without the semantic builder/layout adapter.
            wil::com_ptr<IDWriteTextFormat> format;
            THROW_IF_FAILED(factory->CreateTextFormat(
                L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"en-us", format.put()));
            wil::com_ptr<IDWriteTextLayout> layout;
            THROW_IF_FAILED(factory->CreateTextLayout(L"label", 5u, format.get(), static_cast<float>(width), 1000000.0f, layout.put()));
            THROW_IF_FAILED(layout->SetUnderline(underline ? TRUE : FALSE, {0u, 5u}));
            target->DrawTextLayout(D2D1::Point2F(0.0f, 0.0f), layout.get(), brush.get(), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
        }
        else
        {
            PreparedSemanticGeometry geometry;
            LayoutOptions options;
            options.widthDip = static_cast<float>(width);
            geometry.Begin(factory.get(), RichSemanticDocumentTests::Build(source), options);
            Complete(geometry);
            geometry.Draw(target.get(), brush.get(), brush.get(), D2D1::Point2F(0.0f, 0.0f), 0.0f, static_cast<float>(height));
        }
        THROW_IF_FAILED(target->EndDraw());
        std::vector<BYTE> result(stride * height);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(result.size()), result.data()));
        return std::vector<BYTE>(result);
    };
    const auto expected = pixels({}, true, true), plain = pixels({}, true, false);
    Check(expected != plain, "independent native SDK underline has visible pixels");
    for (const auto source : {L"[label](local)", L"<p><a href='#target'>label</a></p>", L"<u>label</u>"})
        Check(pixels(source, false, false) == expected, "Markdown link, HTML anchor and underline exactly match declared SDK pixels and inherited text ink");
    Check(pixels(L"label", false, false) == plain, "ordinary text remains unchanged by native link presentation");
}
inline void HangingMarkersLinksAndHtml()
{
    auto factory = Factory();
    for (const bool rtl : {false, true})
    {
        const std::wstring word = rtl ? L"\u05d0\u05d1\u05d2" : L"word";
        std::wstring source     = L"9. ";
        const auto first        = source.size();
        for (unsigned i = 0u; i < 18u; ++i)
            source += word + L" ";
        const auto firstEnd = source.size();
        source += L"\n10. ";
        const auto second = source.size();
        source += word + L"\n\n    ";
        const auto continuation = source.size();
        source += word + L"\n\n    - ";
        const auto nested = source.size();
        source += word + L"\n";
        const auto document = RichSemanticDocumentTests::Build(source);
        PreparedSemanticGeometry geometry;
        LayoutOptions options;
        options.widthDip         = 200.0f;
        options.readingDirection = rtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
        geometry.Begin(factory.get(), document, options);
        Complete(geometry);
        const auto lines = geometry.SourceRange({first, firstEnd});
        Check(lines.size() > 1u, "list body wraps through native geometry");
        const auto edge = [&](const auto& rectangle) { return rtl ? rectangle.right : rectangle.left; };
        for (const auto& line : lines)
            Check(std::abs(edge(line) - edge(lines.front())) < .15f, "wrapped list text retains its measured leading gutter");
        const auto secondRange       = geometry.SourceRange({second, second + word.size()});
        const auto continuationRange = geometry.SourceRange({continuation, continuation + word.size()});
        const auto nestedRange       = geometry.SourceRange({nested, nested + word.size()});
        Check(! secondRange.empty() && ! continuationRange.empty() && ! nestedRange.empty(),
              "list sibling, continuation and nested text have owned native glyphs");
        Check(std::abs(edge(secondRange.front()) - edge(lines.front())) < .15f && std::abs(edge(continuationRange.front()) - edge(lines.front())) < .15f,
              "9./10. siblings and loose continuation paragraphs share the list's maximum measured marker column");
        Check(rtl ? edge(nestedRange.front()) < edge(lines.front()) : edge(nestedRange.front()) > edge(lines.front()),
              "nested list text adds the ancestor marker column in either direction");
        Check(geometry.SourceRange({0u, 3u}).empty(), "generated marker ink has no owned source-selection glyphs");
        for (auto raw : {first, second, continuation, nested})
        {
            const auto caret = geometry.SourceCaret(raw);
            const auto hit   = geometry.HitTestSource(caret.x, caret.y + caret.height * .5f);
            Check(hit.position == raw, "list text leading caret and real point hit agree on its source ACP");
        }
        const auto generation = geometry.Generation();
        options.widthDip      = 80.0f;
        geometry.Begin(factory.get(), document, options);
        bool measured = false;
        for (unsigned turn = 0u; turn < 10000u && ! measured; ++turn)
            measured = geometry.Advance({1u, 1u}).layoutsPrepared != 0u;
        Check(measured && geometry.Ready() && geometry.Generation() == generation,
              "a measured marker remains staged while the old coherent geometry is published");
        geometry.Cancel();
        Check(geometry.Generation() == generation, "marker preparation cancellation preserves publication");
        options.widthDip = 1.0f;
        geometry.Begin(factory.get(), document, options);
        Complete(geometry);
        const auto narrow = geometry.SourceCaret(first);
        Check(std::isfinite(narrow.x) && std::isfinite(narrow.y) && narrow.height > 0.0f, "narrow gutters keep finite native insertion geometry");
    }
    {
        const std::wstring tableSource = L"- body\n\n  | left | right |\n  | :--- | ---: |\n  | a | b |\n";
        const auto table               = RichSemanticDocumentTests::Build(tableSource);
        PreparedSemanticGeometry nestedTable;
        LayoutOptions placement;
        placement.widthDip = 300.0f;
        nestedTable.Begin(factory.get(), table, placement);
        Complete(nestedTable);
        const auto textEdge = nestedTable.SourceCaret(tableSource.find(L"body")).x;
        const auto left = tableSource.find(L"left"), right = tableSource.find(L"right");
        const auto leftInk = nestedTable.SourceRange({left, left + 4u}), rightInk = nestedTable.SourceRange({right, right + 5u});
        Check(! leftInk.empty() && ! rightInk.empty() && std::abs(leftInk.front().left - textEdge - 6.0f) < .15f &&
                  std::abs(rightInk.back().right - 294.0f) < .15f,
              "nested table text inherits the measured list gutter and declared physical alignment");
        const auto element =
            std::find_if(table->Elements().begin(), table->Elements().end(), [](const auto& value) { return value.role == SemanticRole::Table; });
        Check(element != table->Elements().end(), "nested table source is a real semantic table");
        const auto bounds = nestedTable.ElementBounds(static_cast<std::size_t>(element - table->Elements().begin()));
        Check(bounds && std::abs(bounds->left - textEdge) < .15f && std::abs(bounds->right - 300.0f) < .15f,
              "table frame/clip placement uses the same measured content width as its glyphs");
    }
    const std::wstring source = L"- [x] task <b>bold</b> [go](local)\n\n<p><a href='https://example.invalid'>link</a><br/>&fjlig;</p>\n\n<p><b></b></p>\n";
    const auto document       = RichSemanticDocumentTests::Build(source);
    PreparedSemanticGeometry geometry;
    LayoutOptions options;
    options.widthDip = 300.0f;
    geometry.Begin(factory.get(), document, options);
    Complete(geometry);
    const auto task = source.find(L"[x]"), bold = source.find(L"bold"), link = source.find(L"link</a>"), entity = source.find(L"&fjlig;"),
               empty = source.find(L"<p><b></b></p>") + 6u;
    Check(! geometry.SourceRange({task, task + 3u}).empty() && geometry.SourceRange({source.find(L"<b>"), source.find(L"<b>") + 3u}).empty(),
          "task object keeps real marker-column geometry while native HTML tags stay hidden");
    Check(! geometry.SourceRange({bold, bold + 4u}).empty() && ! geometry.SourceRange({link, link + 4u}).empty(),
          "styled and linked Unicode source has native range geometry");
    const auto entityRect = geometry.SourceRange({entity, entity + 7u});
    Check(! entityRect.empty(), "HTML reference geometry owns its original raw token");
    const auto caret = geometry.SourceCaret(empty);
    const auto hit   = geometry.HitTestSource(caret.x, caret.y + caret.height * .5f);
    Check(hit.position == empty && geometry.IsEmptySourceSite(empty, hit.site), "an empty native HTML wrapper has a real interior caret and point hit");
    const auto units = geometry.SourceLines();
    for (std::size_t i = 1u; i < units.size(); ++i)
        Check(units[i - 1u].second == units[i].first, "marker splitting and native HTML retain gap-free raw source line units");
    std::wstring many;
    for (unsigned item = 0u; item < 2049u; ++item)
        many += L"- text\n";
    const auto overflow = RichSemanticDocumentTests::Build(many);
    geometry.Begin(factory.get(), overflow, options);
    std::size_t sdkCalls = 0u;
    RichSemanticTests::Reject<std::length_error>(
        [&]
    {
        while (geometry.Pending())
            sdkCalls += geometry.Advance({1u, 7u}).layoutsPrepared;
    },
        "marker and body layouts both participate in native admission");
    Check(sdkCalls == 0u && geometry.Ready() && &geometry.Document() == document.get(),
          "marker admission rejects before SDK work and retains the old publication");
}
inline void TaskCheckboxAlignment()
{
    auto factory              = Factory();
    const std::wstring label  = L"same Unicode \u6771\u4eac task body wraps across the available text column";
    const std::wstring source = L"- [ ] " + label + L"\r\n- [x] " + label + L"\r\n";
    const auto second         = source.find(L"[x]");
    const auto document       = RichSemanticDocumentTests::Build(source);
    for (const auto family : {L"Segoe UI", L"Consolas"})
        for (const float size : {12.0f, 18.0f, 28.0f})
            for (const bool rtl : {false, true})
            {
                LayoutOptions options;
                options.widthDip         = 240.0f;
                options.fontSizeDip      = size;
                options.fontFamily       = family;
                options.readingDirection = rtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
                PreparedSemanticGeometry geometry;
                geometry.Begin(factory.get(), document, options);
                Complete(geometry);
                const auto empty = geometry.SourceRange({2u, 5u}), checked = geometry.SourceRange({second, second + 3u});
                Check(empty.size() == 1u && checked.size() == 1u, "each task state owns one atomic source hit rectangle");
                Check(std::abs(empty.front().left - checked.front().left) < .01f && std::abs(empty.front().right - checked.front().right) < .01f,
                      "checked and unchecked siblings keep exactly the same checkbox column and advance");
                const auto before = geometry.SourceCaret(6u);
                auto changed      = source;
                changed[3u]       = L'x';
                PreparedSemanticGeometry toggled;
                toggled.Begin(factory.get(), RichSemanticDocumentTests::Build(changed), options);
                Complete(toggled);
                const auto after = toggled.SourceCaret(6u);
                const auto box   = toggled.SourceRange({2u, 5u});
                Check(box.size() == 1u && std::abs(box.front().left - empty.front().left) < .01f && std::abs(box.front().right - empty.front().right) < .01f &&
                          std::abs(box.front().top - empty.front().top) < .01f && std::abs(box.front().bottom - empty.front().bottom) < .01f,
                      "toggling the only unchecked item never moves or resizes its hit box");
                Check(std::abs(before.x - after.x) < .01f && std::abs(before.y - after.y) < .01f && std::abs(before.height - after.height) < .01f &&
                          std::abs(geometry.Height() - toggled.Height()) < .01f,
                      "task toggles retain body caret, wrapping and total geometry in both reading directions and custom body fonts");
                const auto singleSource = source.substr(0u, source.find(L'\r') + 2u);
                auto checkedSingle      = singleSource;
                checkedSingle[3u]       = L'x';
                PreparedSemanticGeometry uncheckedOnly, checkedOnly;
                uncheckedOnly.Begin(factory.get(), RichSemanticDocumentTests::Build(singleSource), options);
                checkedOnly.Begin(factory.get(), RichSemanticDocumentTests::Build(checkedSingle), options);
                Complete(uncheckedOnly);
                Complete(checkedOnly);
                const auto singleBefore = uncheckedOnly.SourceCaret(6u), singleAfter = checkedOnly.SourceCaret(6u);
                Check(std::abs(singleBefore.x - singleAfter.x) < .01f && std::abs(singleBefore.y - singleAfter.y) < .01f &&
                          std::abs(uncheckedOnly.Height() - checkedOnly.Height()) < .01f,
                      "a list containing just one task keeps its gutter and wrapping when toggled, without a wider sibling to reserve space");
            }
}
inline void SemanticSpacingPolicy()
{
    auto factory = Factory();
    for (const auto& [size, multiplier] : {std::pair{16.0f, 1.65f}, {15.0f, 1.65f}, {18.0f, 1.65f}, {18.0f, 0.0f}})
    {
        LayoutOptions options;
        options.widthDip                   = 420.0f;
        options.fontSizeDip                = size;
        options.lineHeightMultiplier       = multiplier;
        options.paragraphSpacingAfterLines = 1.0f;
        options.listItemSpacingDip         = 10.0f;
        // Independent native body measurement; the baseline value does not
        // affect line height and deliberately differs from editor positioning.
        wil::com_ptr<IDWriteTextFormat> format;
        THROW_IF_FAILED(factory->CreateTextFormat(options.fontFamily.c_str(),
                                                  nullptr,
                                                  DWRITE_FONT_WEIGHT_NORMAL,
                                                  DWRITE_FONT_STYLE_NORMAL,
                                                  DWRITE_FONT_STRETCH_NORMAL,
                                                  size,
                                                  options.locale.c_str(),
                                                  format.put()));
        wil::com_ptr<IDWriteTextLayout> measured;
        THROW_IF_FAILED(factory->CreateTextLayout(L"x", 1u, format.get(), options.widthDip, 100000.0f, measured.put()));
        if (multiplier > 0.0f)
            THROW_IF_FAILED(measured->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, size * multiplier, size * multiplier * .75f));
        DWRITE_LINE_METRICS metric{};
        UINT32 actual{};
        THROW_IF_FAILED(measured->GetLineMetrics(&metric, 1u, &actual));
        Check(actual == 1u && metric.height > 0.0f, "spacing oracle obtains a real native body line");
        const auto line = metric.height;
        for (const unsigned blanks : {1u, 2u})
        {
            const std::wstring source = L"a" + std::wstring(blanks + 1u, L'\n') + L"b";
            const auto document       = RichSemanticDocumentTests::Build(source);
            PreparedSemanticGeometry geometry;
            geometry.Begin(factory.get(), document, options);
            std::size_t calls{};
            Complete(geometry, &calls);
            Check(calls == 2u + blanks + (multiplier == 0.0f ? 1u : 0u),
                  "native body-line measurement is one budgeted SDK layout and explicit line height adds none");
            const auto first = geometry.SourceCaret(0u), last = geometry.SourceCaret(source.size() - 1u);
            Check(std::abs(first.height - line) < .02f && std::abs(last.y - first.y - line * static_cast<float>(blanks + 1u)) < .04f,
                  "real separator caret lines fill the one-line paragraph margin without stacking another blank gap");
            float previous = first.y;
            for (const auto& empty : document->Syntax()->EmptyLines())
            {
                const auto caret = geometry.SourceCaret(empty.insertion);
                const auto hit   = geometry.HitTestSource(caret.x, caret.y + caret.height * .5f);
                Check(caret.y > previous && hit.position == empty.insertion && geometry.IsEmptySourceSite(hit.position, hit.site),
                      "every separator retains a distinct exact ACP caret and native hit site under collapsed spacing");
                previous = caret.y;
            }
            Check(last.y > previous && document->Map().Source() == source, "paragraph spacing retains source bytes and ordered source sites");
            const auto generation = geometry.Generation();
            Check(geometry.Begin(factory.get(), document, options) == LayoutPreparationStatus::Complete && geometry.Generation() == generation,
                  "identical semantic spacing reuses the complete geometry generation");
            auto changed                       = options;
            changed.paragraphSpacingAfterLines = 2.0f;
            geometry.Begin(factory.get(), document, changed);
            geometry.Cancel();
            Check(geometry.Generation() == generation && geometry.SourceCaret(source.size() - 1u).y == last.y,
                  "cancelled spacing change retains the prior exact geometry publication");
            geometry.Begin(factory.get(), document, changed);
            Complete(geometry);
            Check(geometry.Generation() != generation, "spacing values participate in geometry reuse identity");
        }
        for (const std::wstring_view source : {L"- one\n- two", L"- outer\n  - inner\n  - second\n- final"})
        {
            const auto document = RichSemanticDocumentTests::Build(source);
            PreparedSemanticGeometry geometry;
            geometry.Begin(factory.get(), document, options);
            Complete(geometry);
            float previousBottom{};
            bool first = true;
            for (const auto& paragraph : document->Paragraphs())
            {
                const auto bounds = geometry.ElementBounds(paragraph.element);
                Check(bounds.has_value(), "each list body has complete real layout bounds");
                if (! first)
                    Check(std::abs(bounds.value().top - previousBottom - 10.0f) < .04f,
                          "sibling, nested-entry and outer-sibling list transitions use exactly the requested ten DIP gap");
                previousBottom = bounds.value().bottom;
                first          = false;
            }
        }
        for (const std::wstring_view source : {L"- one\n\n- two", L"- one\n\n  continuation\n\n- two"})
        {
            const auto document = RichSemanticDocumentTests::Build(source);
            PreparedSemanticGeometry geometry;
            geometry.Begin(factory.get(), document, options);
            Complete(geometry);
            float previousBottom{};
            bool first = true;
            for (const auto& paragraph : document->Paragraphs())
            {
                const auto bounds = geometry.ElementBounds(paragraph.element);
                Check(bounds.has_value(), "loose list body has native layout bounds");
                if (! first)
                    Check(std::abs(bounds.value().top - previousBottom - line) < .04f,
                          "a loose-list separator or continuation supplies its real body line instead of stacking a list gap");
                previousBottom = bounds.value().bottom;
                first          = false;
            }
        }
        const auto document = RichSemanticDocumentTests::Build(L"a\n# b");
        PreparedSemanticGeometry geometry;
        geometry.Begin(factory.get(), document, options);
        Complete(geometry);
        const auto first = geometry.SourceCaret(0u), heading = geometry.SourceCaret(4u);
        Check(std::abs(heading.y - first.y - first.height - line) < .04f,
              "a paragraph without a physical blank separator still receives one nominal body line of bottom margin");
        auto legacy = options;
        legacy.paragraphSpacingAfterLines.reset();
        legacy.listItemSpacingDip.reset();
        const auto tight = RichSemanticDocumentTests::Build(L"- one\n- two");
        geometry.Begin(factory.get(), tight, legacy);
        Complete(geometry);
        const auto one = geometry.ElementBounds(tight->Paragraphs()[0u].element), two = geometry.ElementBounds(tight->Paragraphs()[1u].element);
        Check(one && two && std::abs(two.value().top - one.value().bottom - 8.0f) < .04f,
              "absent spacing options preserve the exact established eight DIP semantic box gap");
        geometry.Begin(factory.get(), tight, options);
        Complete(geometry);
        const auto listGeneration    = geometry.Generation();
        auto widerList               = options;
        widerList.listItemSpacingDip = 20.0f;
        geometry.Begin(factory.get(), tight, widerList);
        Complete(geometry);
        const auto widerOne = geometry.ElementBounds(tight->Paragraphs()[0u].element), widerTwo = geometry.ElementBounds(tight->Paragraphs()[1u].element);
        Check(widerOne && widerTwo && std::abs(widerTwo.value().top - widerOne.value().bottom - 20.0f) < .04f && geometry.Generation() != listGeneration,
              "list minimum alone participates in same-document geometry reuse and actual placement");
        const auto separated = RichSemanticDocumentTests::Build(L"a\n\nb");
        geometry.Begin(factory.get(), separated, legacy);
        Complete(geometry);
        const auto oldA = geometry.SourceCaret(0u), oldB = geometry.SourceCaret(3u);
        Check(std::abs(oldB.y - oldA.y - 2.0f * line - 16.0f) < .04f, "null/null also preserves legacy eight DIP after each real separator caret box");
        auto listOnly               = legacy;
        listOnly.listItemSpacingDip = 10.0f;
        geometry.Begin(factory.get(), tight, listOnly);
        Complete(geometry);
        const auto listOne = geometry.ElementBounds(tight->Paragraphs()[0u].element), listTwo = geometry.ElementBounds(tight->Paragraphs()[1u].element);
        Check(listOne && listTwo && std::abs(listTwo.value().top - listOne.value().bottom - 10.0f) < .04f,
              "list-only engagement applies its ten DIP minimum to tight items");
        geometry.Begin(factory.get(), separated, listOnly);
        Complete(geometry);
        Check(std::abs(geometry.SourceCaret(3u).y - geometry.SourceCaret(0u).y - 2.0f * line) < .04f,
              "list-only engagement uses an eight DIP paragraph minimum credited by the real blank line");
        geometry.Begin(factory.get(), document, listOnly);
        Complete(geometry);
        const auto listA = geometry.SourceCaret(0u), listHeading = geometry.SourceCaret(4u);
        Check(std::abs(listHeading.y - listA.y - listA.height - 8.0f) < .04f,
              "list-only engagement keeps the unset paragraph minimum at eight DIP without a source separator");
        auto paragraphOnly                       = legacy;
        paragraphOnly.paragraphSpacingAfterLines = 1.0f;
        geometry.Begin(factory.get(), tight, paragraphOnly);
        Complete(geometry);
        const auto paragraphOne = geometry.ElementBounds(tight->Paragraphs()[0u].element),
                   paragraphTwo = geometry.ElementBounds(tight->Paragraphs()[1u].element);
        Check(paragraphOne && paragraphTwo && std::abs(paragraphTwo.value().top - paragraphOne.value().bottom - line) < .04f,
              "paragraph-only engagement applies its paragraph minimum when no list-specific override is supplied");
        geometry.Begin(factory.get(), tight, legacy);
        Complete(geometry);
        const auto generation = geometry.Generation();
        for (const float invalid : {-1.0f, 257.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            auto rejected               = options;
            rejected.listItemSpacingDip = invalid;
            RichSemanticTests::Reject<std::invalid_argument>([&] { geometry.Begin(factory.get(), tight, rejected); }, "invalid list spacing rejects admission");
            rejected                            = options;
            rejected.paragraphSpacingAfterLines = invalid == 257.0f ? 4.01f : invalid;
            RichSemanticTests::Reject<std::invalid_argument>([&] { geometry.Begin(factory.get(), tight, rejected); },
                                                             "invalid paragraph spacing rejects admission");
            Check(geometry.Ready() && ! geometry.Pending() && geometry.Generation() == generation && &geometry.Document() == tight.get(),
                  "invalid spacing cannot change the previous complete publication");
        }
        auto zero                       = options;
        zero.paragraphSpacingAfterLines = 0.0f;
        zero.listItemSpacingDip         = 0.0f;
        geometry.Begin(factory.get(), tight, zero);
        Complete(geometry);
        const auto zeroOne = geometry.ElementBounds(tight->Paragraphs()[0u].element), zeroTwo = geometry.ElementBounds(tight->Paragraphs()[1u].element);
        Check(zeroOne && zeroTwo && std::abs(zeroTwo.value().top - zeroOne.value().bottom) < .04f, "explicit zero spacing is valid and distinct from null");
    }
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
    SemanticSpacingPolicy();
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
inline void SourceCaretSitesPolicy(bool spacing)
{
    auto factory = Factory();
    LayoutOptions options;
    options.widthDip = 360.0f;
    if (spacing)
    {
        options.paragraphSpacingAfterLines = 1.0f;
        options.listItemSpacingDip         = 10.0f;
    }
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
        for (std::size_t raw = 0u; raw <= std::wstring_view(sourceText).size(); ++raw)
            if (IsEditBoundary(sourceText, raw))
                Check(geometry.StepSource(raw, MarkdownNoNode, false).position <= raw && geometry.StepSource(raw, MarkdownNoNode, true).position >= raw,
                      "every legal source ACP preserves logical arrow direction across marker and body boxes");
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
inline void SourceCaretSites()
{
    SourceCaretSitesPolicy(false);
    SourceCaretSitesPolicy(true);
}
} // namespace RichSemanticGeometryTests
