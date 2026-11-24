#pragma once
// W7.7 round 2 on the embedded executable's allocation instrument (Embedded.Tests.BenchmarkInstrument.h):
//   - a colored formula's and a diagram's paint allocate nothing, on a first, a second and a third pair of page colors (the
//     decisions' storage is made at preparation; a paint without storage decides nothing);
//   - the math font cache's registry row reads and drops without allocating, on a thread that prepared no formula;
//   - the cache keeps no DirectWrite factory alive: after its last formula a thread holds no reference to it.
// Included by Embedded.Tests.Embedded.cpp after Embedded.Tests.MathDiagramsFx8.h; uses its Check, Hr and allocation counters.
#include "Controls/DxUi.NativeColorPalette.h"
#include "Controls/DxUi.PreparedFlowchart.h"
#include "Controls/DxUi.PreparedMath.h"
#include "Embedded.Tests.MathDiagramsFx8.h"
#include "Support.Tests.PreparedFlowchart.h"

#include <array>
#include <iostream>
#include <thread>

namespace MathPaintAllocations
{
// Every clause is evaluated before the run fails, so a run on code that misses several clauses reports each of them.
inline unsigned failedClauses = 0u;
inline void Expect(bool holds, const char* clause)
{
    if (! holds)
    {
        ++failedClauses;
        std::cerr << "  CLAUSE NOT MET: " << clause << '\n';
    }
}

// R5: nothing the cache does keeps a DirectWrite factory alive. A thread that prepared formulas and released them leaves the
// factory with the references it had before, so an application that destroys its hosts and releases its factory sees it go.
inline void TestMathFontCacheKeepsNoFactoryAlive()
{
    using namespace DxUi::RichText;
    using Math = DxUi::RichText::Detail::PreparedMath;
    wil::com_ptr<IDWriteFactory> factory;
    Hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.put())), "lifetime factory");
    const auto references = [&]
    {
        factory->AddRef();
        return factory->Release();
    };
    const auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\frac{a}{b} + \\sqrt{x^2}"));
    Math::DropFontCache();
    const ULONG before = references();
    {
        Math math(factory.get(), expression, 24.f);
        while (! math.Advance())
        {
        }
        Expect(references() > before, "a prepared formula holds its factory");
    }
    // DirectWrite keeps some references to a factory of its own once the factory has built fonts (its system font collection,
    // for one): a control factory that only built a collection shows the same count. What the cache must add is none, which
    // dropping it shows: it releases nothing, because it holds nothing.
    const ULONG released = references();
    Expect(Math::FontCacheEntries() == 0u, "after its last formula the thread's cache knows no data");
    Math::DropFontCache();
    Expect(references() == released, "dropping the cache releases no reference to the factory: it held none");
    wil::com_ptr<IDWriteFactory> control;
    Hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(control.put())), "control factory");
    {
        wil::com_ptr<IDWriteFontCollection> collection;
        Hr(control->GetSystemFontCollection(collection.put(), FALSE), "control collection");
    }
    {
        // What building the formula's own DirectWrite objects (a text format and a layout of the math family) leaves behind.
        wil::com_ptr<IDWriteTextFormat> format;
        Hr(control->CreateTextFormat(
               L"Cambria Math", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24.f, L"en-us", format.put()),
           "control format");
        wil::com_ptr<IDWriteTextLayout> layout;
        Hr(control->CreateTextLayout(L"x2+y", 4u, format.get(), 1000.f, 1000.f, layout.put()), "control layout");
        DWRITE_TEXT_METRICS metrics{};
        Hr(layout->GetMetrics(&metrics), "control metrics");
    }
    control->AddRef();
    const ULONG controlReferences = control->Release();
    std::cout << "  factory references: " << before << " before, " << released
              << " after its formula is released and its cache dropped; a factory that only built "
              << "a system font collection, a text format and a layout holds " << controlReferences << "\n";
}

// R4: the cache's registry row reads and drops without allocating, on a thread that never prepared a formula.
inline void TestMathFontCacheRowNeverAllocates()
{
    using Math     = DxUi::RichText::Detail::PreparedMath;
    size_t made    = 1u;
    size_t entries = 1u;
    uint64_t bytes = 1u;
    std::thread worker([&]
    {
        allocations      = 0u;
        countAllocations = true;
        entries          = Math::FontCacheEntries();
        bytes            = Math::FontCacheBytes();
        Math::DropFontCache();
        countAllocations = false;
        made             = allocations;
    });
    worker.join();
    std::cout << "  math font cache row on a fresh thread: " << made << " allocations, " << entries << " entries, " << bytes << " bytes\n";
    Expect(made == 0u && entries == 0u && bytes == 0u, "the math font cache's registry row reads and drops on a fresh thread without allocating");
}

// A 400x300 WIC target and the three brushes a diagram draws with.
struct Target
{
    wil::com_ptr<IWICBitmap> bitmap;
    wil::com_ptr<ID2D1RenderTarget> target;
    wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
    explicit Target(ID2D1Factory* drawing)
    {
        wil::com_ptr<IWICImagingFactory> imaging;
        Hr(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())), "WIC factory");
        Hr(imaging->CreateBitmap(400u, 300u, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()), "WIC bitmap");
        Hr(drawing->CreateWicBitmapRenderTarget(bitmap.get(), D2D1::RenderTargetProperties(), target.put()), "WIC target");
        Hr(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), text.put()), "brush");
        Hr(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()), "brush");
        Hr(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()), "brush");
    }
};

// R2: a colored formula's first paint on a pair of page colors, on a second pair and on a third (which displaces one), allocates
// nothing: the storage of the decisions is made when the formula is prepared.
inline void TestColoredMathPaintsAllocateNothing()
{
    using namespace DxUi::RichText;
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const auto apartment   = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto write       = PreparedFlowchartTests::WriteFactory();
    const auto drawing     = PreparedFlowchartTests::DrawingFactory();
    Target raster(drawing.get());
    const auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\textcolor{red}{x}+\\colorbox{yellow}{\\textcolor{#204080}{y}}+z"));
    DxUi::RichText::Detail::PreparedMath math(write.get(), expression, 24.f);
    while (! math.Advance())
    {
    }
    Expect(! math.Colors().empty(), "the formula carries authored colors");
    DxUi::RichText::Detail::NativeColorPalette palette(raster.target.get(), math.Colors());
    while (! palette.Advance())
    {
    }
    Expect(palette.Ready(), "the formula's palette is staged");
    const std::array<DxUi::RichText::Detail::MathColorContext, 3u> pages{{{D2D1::ColorF(D2D1::ColorF::Black), D2D1::ColorF(D2D1::ColorF::White)},
                                                                          {D2D1::ColorF(D2D1::ColorF::White), D2D1::ColorF(D2D1::ColorF::Black)},
                                                                          {D2D1::ColorF(0.5f, 0.5f, 0.5f), D2D1::ColorF(1.f, 1.f, 0.8f)}}};
    // Two slots: the third pair displaces the first, so the second round decides every pair again.
    for (size_t round = 0u; round < 2u; ++round)
        for (size_t index = 0u; index < pages.size(); ++index)
        {
            const DxUi::Detail::WorkCounterScope scope;
            raster.target->BeginDraw();
            raster.target->Clear(D2D1::ColorF(D2D1::ColorF::White));
            allocations      = 0u;
            countAllocations = true;
            math.Draw(raster.target.get(), raster.text.get(), D2D1::Point2F(10.f, 10.f), 1.f, &palette, &pages[index]);
            countAllocations  = false;
            const size_t made = allocations;
            Hr(raster.target->EndDraw(), "colored math paint");
            const uint64_t evaluations = scope.Delta().rich.luminanceEvaluations;
            std::cout << "  colored math paint, page " << index << " round " << round << ": " << made << " allocations, " << evaluations
                      << " luminance evaluations\n";
            Expect(made == 0u, "a colored formula's paint allocates nothing, whichever pair of page colors it is the first on");
            Expect(evaluations > 0u, "a paint on a pair of colors the formula holds no decision for decides it");
        }
    const DxUi::Detail::WorkCounterScope scope;
    raster.target->BeginDraw();
    math.Draw(raster.target.get(), raster.text.get(), D2D1::Point2F(10.f, 10.f), 1.f, &palette, &pages[2u]);
    Hr(raster.target->EndDraw(), "repeated colored math paint");
    Expect(scope.Delta().rich.luminanceEvaluations == 0u, "a repeated paint on the same colors decides nothing");
}

// R3: a diagram's paint allocates nothing, with authored colors or without, and a palette handed to a diagram that needs none is
// not a way to make it decide colors at a paint.
inline void TestDiagramPaintsAllocateNothing()
{
    using namespace DxUi::RichText::Detail;
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const auto apartment   = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto write       = PreparedFlowchartTests::WriteFactory();
    const auto drawing     = PreparedFlowchartTests::DrawingFactory();
    Target raster(drawing.get());
    const auto colored =
        FlowchartLayoutTests::Graph(L"flowchart TD\nA[Alpha]-->B[Beta]\nstyle A fill:#204080,color:#ffffff\nlinkStyle 0 stroke:#c04040,stroke-width:3px\n");
    const auto plain = FlowchartLayoutTests::Graph(L"flowchart TD\nC[Gamma]-->D[Delta]\n");
    Expect(DiagramPalette::Needed(*colored) && ! DiagramPalette::Needed(*plain), "one diagram has authored colors and the other none");
    const std::array<DiagramColorContext, 3u> pages{{{D2D1::ColorF(D2D1::ColorF::Black), D2D1::ColorF(D2D1::ColorF::Blue), D2D1::ColorF(D2D1::ColorF::White)},
                                                     {D2D1::ColorF(D2D1::ColorF::White), D2D1::ColorF(D2D1::ColorF::Yellow), D2D1::ColorF(D2D1::ColorF::Black)},
                                                     {D2D1::ColorF(0.3f, 0.3f, 0.3f), D2D1::ColorF(0.9f, 0.5f, 0.1f), D2D1::ColorF(1.f, 1.f, 0.8f)}}};
    for (const auto& graph : {colored, plain})
    {
        PreparedFlowchart prepared;
        prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
        PreparedFlowchartTests::Complete(prepared, Check);
        DiagramPalette palette(graph, raster.target.get()); // for the plain diagram too: a palette is never a reason to allocate
        while (! palette.Advance())
        {
        }
        for (size_t round = 0u; round < 2u; ++round)
            for (const auto& page : pages)
            {
                raster.target->BeginDraw();
                allocations       = 0u;
                countAllocations  = true;
                const bool drawn  = prepared.Draw(raster.target.get(), raster.text.get(), raster.stroke.get(), raster.fill.get(), {}, &palette, &page);
                countAllocations  = false;
                const size_t made = allocations;
                Hr(raster.target->EndDraw(), "diagram paint");
                Expect(drawn && made == 0u, "a diagram's first paint on a pair of page colors allocates nothing");
            }
    }
}

// R6: the ink of a connector through an acute bend, 3 px wide, stays inside the bounds of the ink its segments draw apart, in the
// solid and the dashed style; Direct2D's default (miter) join does not.
inline void TestConnectorJoinsStayInsideTheSegmentInk()
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const auto apartment   = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto drawing     = PreparedFlowchartTests::DrawingFactory();
    Target raster(drawing.get());
    // Two segments meeting at 30 degrees.
    const D2D1_POINT_2F route[3]{D2D1::Point2F(30.f, 60.f), D2D1::Point2F(370.f, 60.f), D2D1::Point2F(370.f - 300.f * 0.8660254f, 60.f + 300.f * 0.5f)};
    wil::com_ptr<ID2D1PathGeometry> path;
    Hr(drawing->CreatePathGeometry(path.put()), "route geometry");
    {
        wil::com_ptr<ID2D1GeometrySink> sink;
        Hr(path->Open(sink.put()), "route sink");
        sink->BeginFigure(route[0], D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddLine(route[1]);
        sink->AddLine(route[2]);
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        Hr(sink->Close(), "route sink close");
    }
    struct Box
    {
        int left = 1 << 20, top = 1 << 20, right = -1, bottom = -1;
        [[nodiscard]] bool Inside(const Box& other) const noexcept
        {
            return left >= other.left && top >= other.top && right <= other.right && bottom <= other.bottom;
        }
    };
    const auto ink = [&](auto&& draw)
    {
        raster.target->BeginDraw();
        raster.target->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 0.f));
        draw();
        Hr(raster.target->EndDraw(), "connector ink");
        std::vector<BYTE> pixels(400u * 300u * 4u);
        Hr(raster.bitmap->CopyPixels(nullptr, 400u * 4u, static_cast<UINT>(pixels.size()), pixels.data()), "connector pixels");
        Box box;
        for (int y = 0; y < 300; ++y)
            for (int x = 0; x < 400; ++x)
                if (pixels[(static_cast<size_t>(y) * 400u + static_cast<size_t>(x)) * 4u + 3u])
                {
                    box.left   = (std::min)(box.left, x);
                    box.right  = (std::max)(box.right, x);
                    box.top    = (std::min)(box.top, y);
                    box.bottom = (std::max)(box.bottom, y);
                }
        return box;
    };
    const Box segments = ink([&]
    {
        raster.target->DrawLine(route[0], route[1], raster.stroke.get(), 3.f, nullptr);
        raster.target->DrawLine(route[1], route[2], raster.stroke.get(), 3.f, nullptr);
    });
    const Box miter    = ink([&] { raster.target->DrawGeometry(path.get(), raster.stroke.get(), 3.f, nullptr); });
    std::cout << "  acute 3 px connector ink: segments (" << segments.left << ',' << segments.top << ")-(" << segments.right << ',' << segments.bottom
              << "), default miter join (" << miter.left << ',' << miter.top << ")-(" << miter.right << ',' << miter.bottom << ")\n";
    Expect(! miter.Inside(segments), "negative control: Direct2D's default miter join reaches outside the ink the segments draw");
    for (const bool dashed : {false, true})
    {
        wil::com_ptr<ID2D1StrokeStyle> style;
        Hr(DxUi::RichText::Detail::CreateEdgeConnectorStrokeStyle(drawing.get(), dashed, style.put()), "connector stroke style");
        const Box joined = ink([&] { raster.target->DrawGeometry(path.get(), raster.stroke.get(), 3.f, style.get()); });
        std::cout << "  " << (dashed ? "dashed" : "solid") << " bevel-joined connector ink (" << joined.left << ',' << joined.top << ")-(" << joined.right
                  << ',' << joined.bottom << ")\n";
        Expect(joined.right >= joined.left && joined.Inside(segments), "a bevel-joined connector's ink stays inside the bounds of its segments' ink");
    }
}

inline void Run()
{
    TestConnectorJoinsStayInsideTheSegmentInk();
    TestMathFontCacheKeepsNoFactoryAlive();
    TestMathFontCacheRowNeverAllocates();
    TestColoredMathPaintsAllocateNothing();
    TestDiagramPaintsAllocateNothing();
    Check(failedClauses == 0u, "the math font cache and the paints of colored objects meet the round-2 clauses of W7.7");
}
} // namespace MathPaintAllocations
