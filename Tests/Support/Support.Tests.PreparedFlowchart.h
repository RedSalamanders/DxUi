#pragma once
#include "Controls/DxUi.PreparedFlowchart.h"
#include "GraphicsFixture.h"
#include "Support.Tests.CountingWriteFactory.h"
#include "Support.Tests.FlowchartLayout.h"
#include <algorithm>
#include <iostream>
#include <wil/resource.h>
#include <wil/result.h>
#include <wincodec.h>

namespace PreparedFlowchartTests
{
using namespace DxUi::RichText;
using namespace DxUi::RichText::Detail;
inline wil::com_ptr<IDWriteFactory> WriteFactory()
{
    wil::com_ptr<IDWriteFactory> factory;
    THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.put())));
    return factory;
}
inline wil::com_ptr<ID2D1Factory> DrawingFactory()
{
    wil::com_ptr<ID2D1Factory> factory;
    THROW_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.put()));
    return factory;
}
template <typename Check> inline std::size_t Complete(PreparedFlowchart& prepared, Check&& check, NativeTextTests::CountingWriteFactory* calls = nullptr)
{
    std::size_t total{}, steps{};
    while (prepared.Pending())
    {
        const auto before = calls ? calls->layouts : 0u;
        const auto result = prepared.Advance({1u, 3u});
        check(result.layoutsPrepared <= 1u && result.blockSteps <= 3u && result.blockSteps > 0u,
              "native paint preparation respects SDK and CPU event allowances");
        if (calls)
            check(calls->layouts - before == result.layoutsPrepared, "reported label work equals actual intercepted CreateTextLayout calls");
        total += result.layoutsPrepared;
        check(++steps < 20000u, "native paint preparation makes finite bounded progress");
    }
    return total;
}
template <typename Check> inline void BudgetAndLifecycle(Check&& check)
{
    const auto write   = WriteFactory();
    const auto drawing = DrawingFactory();
    const auto calls   = Microsoft::WRL::Make<NativeTextTests::CountingWriteFactory>(write.get());
    if (! calls)
        throw std::bad_alloc();
    const auto graph = FlowchartLayoutTests::Graph(L"graph TD;A[\"\"]-->|yes|B{\"Unicode \u03b2 \U0001f600\"};B-->A;B-->B");
    PreparedFlowchart prepared;
    LayoutOptions options;
    check(prepared.Begin(calls.Get(), drawing.get(), graph, options, {1u, 1u}) == LayoutPreparationStatus::Pending && calls->layouts == 0u &&
              calls->formats == 0u,
          "native graph admission creates no SDK resource before owner preparation");
    for (const auto budget : {LayoutWorkBudget{0u, 3u}, LayoutWorkBudget{1u, 0u}})
    {
        const auto result = prepared.Advance(budget);
        check(result.blockSteps == 0u && result.layoutsPrepared == 0u && calls->formats == 0u,
              "zero SDK/CPU budget cannot create a format or hidden label layout");
    }
    check(Complete(prepared, check, calls.Get()) == 3u && calls->layouts == 3u && calls->formats == 1u && prepared.Ready() && prepared.Revision() == 7u,
          "empty nodes and nonempty edge labels each consume exactly one actual SDK layout");
    const auto original = prepared.Geometry();
    FlowchartLayoutTests::Invariants(*original, check);
    auto generation            = prepared.Generation();
    auto otherLayout           = options;
    otherLayout.widthDip       = 17.0f;
    otherLayout.codeFontFamily = L"not used";
    otherLayout.textAlignment  = DWRITE_TEXT_ALIGNMENT_TRAILING;
    check(prepared.Begin(calls.Get(), drawing.get(), graph, otherLayout, {1u, 1u}) == LayoutPreparationStatus::Complete &&
              prepared.Generation() == generation && calls->layouts == 3u,
          "completed intrinsic labels coalesce without depending on paragraph width or code style");
    options.fontSizeDip = 28.0f;
    prepared.Begin(calls.Get(), drawing.get(), graph, options, {2u, 2u});
    prepared.Advance({1u, 2u});
    prepared.Cancel();
    check(prepared.Ready() && prepared.Generation() == generation && prepared.Geometry() == original,
          "cancelled font replacement preserves complete prior resources");
    prepared.Begin(calls.Get(), drawing.get(), graph, options, {2u, 2u});
    bool invalid{};
    try
    {
        prepared.Begin(nullptr, drawing.get(), graph, options, {3u, 3u});
    }
    catch (const std::invalid_argument&)
    {
        invalid = true;
    }
    check(invalid && prepared.Pending() && prepared.Geometry() == original, "invalid native resource admission preserves pending work");
    calls->formatFailure = E_OUTOFMEMORY;
    bool failed{};
    try
    {
        Complete(prepared, check, calls.Get());
    }
    catch (const wil::ResultException& error)
    {
        failed = error.GetErrorCode() == E_OUTOFMEMORY;
    }
    calls->formatFailure = S_OK;
    check(failed && ! prepared.Pending() && prepared.Geometry() == original && prepared.Generation() == generation,
          "format HRESULT failure retires staging while preserving old publication");
    prepared.Begin(calls.Get(), drawing.get(), graph, options, {2u, 2u});
    Complete(prepared, check, calls.Get());
    check(prepared.Ready() && prepared.Generation() == ++generation && prepared.Geometry()->nodes[1u].bounds.height > original->nodes[1u].bounds.height,
          "explicit retry adopts the changed native font and retires prior resources atomically");
    for (std::size_t failure = 0u; failure < 3u; ++failure)
    {
        const auto old      = prepared.Geometry();
        calls->failLayoutAt = calls->layouts + failure;
        prepared.Begin(calls.Get(), drawing.get(), graph, options, {2u, 10u + failure});
        failed = false;
        try
        {
            Complete(prepared, check, calls.Get());
        }
        catch (const wil::ResultException& error)
        {
            failed = error.GetErrorCode() == E_OUTOFMEMORY;
        }
        calls->failLayoutAt = std::numeric_limits<std::size_t>::max();
        check(failed && ! prepared.Pending() && prepared.Geometry() == old && prepared.Generation() == generation,
              "each actual label SDK failure preserves the old prepared graph");
        prepared.Begin(calls.Get(), drawing.get(), graph, options, {2u, 10u + failure});
        Complete(prepared, check, calls.Get());
        ++generation;
    }
    const auto before = calls->layouts;
    prepared.Invalidate();
    check(! prepared.Ready() && ! prepared.Draw(nullptr, nullptr, nullptr, nullptr, {}), "invalidated resources cannot submit drawing");
    prepared.Begin(calls.Get(), drawing.get(), graph, options, {100u, 100u});
    Complete(prepared, check, calls.Get());
    check(calls->layouts == before + 3u && prepared.Ready(), "resource epoch invalidation requires fresh owner preparation");
    const auto beforeEpochPair = calls->layouts;
    prepared.Begin(calls.Get(), drawing.get(), graph, options, {1u, 2u});
    Complete(prepared, check, calls.Get());
    const auto epochPlan = prepared.Geometry();
    check(prepared.Begin(calls.Get(), drawing.get(), graph, options, {2u, 1u}) == LayoutPreparationStatus::Pending && prepared.Geometry() == epochPlan,
          "swapped typography/resource epochs cannot alias despite identical sum and xor");
    Complete(prepared, check, calls.Get());
    check(calls->layouts == beforeEpochPair + 6u, "each distinct owner epoch pair prepares a fresh bounded label set");
    const auto longWord = FlowchartLayoutTests::Graph(L"graph TD;A[\"" + std::wstring(100u, L'W') + L"\"]");
    options.fontSizeDip = 18.0f;
    prepared.Begin(calls.Get(), drawing.get(), longWord, options, {100u, 101u});
    Complete(prepared, check, calls.Get());
    check(prepared.Ready() && prepared.Geometry()->nodes[0u].label.width <= 230.0 && prepared.Geometry()->nodes[0u].label.height > 100.0,
          "long unbroken Unicode text wraps rather than exceeding the bounded node width");
    std::wstring hugeSource = L"graph TD;A[\"";
    for (unsigned row = 0u; row < 512u; ++row)
    {
        if (row)
            hugeSource += L"\\n";
        hugeSource += L'W';
    }
    hugeSource += L"\"]";
    const auto huge     = FlowchartLayoutTests::Graph(hugeSource);
    options.fontSizeDip = 512.0f;
    prepared.Begin(calls.Get(), drawing.get(), huge, options, {101u, 101u});
    Complete(prepared, check, calls.Get());
    check(! prepared.Ready() && prepared.Geometry() && prepared.Geometry()->issue != FlowchartLayoutIssue::None && prepared.Geometry()->nodes.empty(),
          "oversized real SDK ink yields explicit capacity fallback without clamping or partial paint");
    prepared.Clear();
    check(! prepared.Pending() && ! prepared.Ready() && ! prepared.Geometry() && original->nodes.size() == 2u &&
              prepared.Advance().status == LayoutPreparationStatus::Idle,
          "native retirement releases resources while detached CPU geometry remains immutable");
}
template <typename Check> inline void BorrowedTargets(Check&& check, void (*observeDraw)(bool) noexcept = nullptr)
{
    const auto real  = WriteFactory();
    const auto calls = Microsoft::WRL::Make<NativeTextTests::CountingWriteFactory>(real.get());
    if (! calls)
        throw std::bad_alloc();
    wil::com_ptr<ID2D1Factory1> drawing;
    THROW_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(drawing.put())));
    const auto graph = FlowchartLayoutTests::Graph(L"graph LR;A[\"Unicode \u03b2\"]-->|yes|B(round);B-->C((circle));C-->D{why};D-->A;D-->D");
    LayoutOptions options;
    options.fontSizeDip = 14.0f;
    PreparedFlowchart prepared;
    prepared.Begin(calls.Get(), drawing.get(), graph, options, {1u, 1u});
    Complete(prepared, check, calls.Get());
    const auto plan       = prepared.Geometry();
    const auto generation = prepared.Generation(), layouts = calls->layouts;
    check(prepared.Ready(), "all native shapes/cycle/self-loop paths are prepared before borrowed-target drawing");
    for (const auto dpi : {96.0f, 192.0f})
        for (unsigned replacement = 0u; replacement < 2u; ++replacement)
        {
            GraphicsFixture graphics;
            THROW_IF_FAILED(graphics.Create());
            const auto width  = static_cast<UINT>(std::ceil((plan->extent.width + 16.0) * dpi / 96.0)),
                       height = static_cast<UINT>(std::ceil((plan->extent.height + 16.0) * dpi / 96.0));
            check(width < 4096u && height < 4096u, "borrowed native graph fixture bounds its actual GPU target");
            THROW_IF_FAILED(graphics.Resize(width, height));
            wil::com_ptr<IDXGIDevice> dxgi;
            THROW_IF_FAILED(graphics.device.query_to(dxgi.put()));
            wil::com_ptr<ID2D1Device> device;
            THROW_IF_FAILED(drawing->CreateDevice(dxgi.get(), device.put()));
            wil::com_ptr<ID2D1DeviceContext> context;
            THROW_IF_FAILED(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, context.put()));
            wil::com_ptr<IDXGISurface> surface;
            THROW_IF_FAILED(graphics.target.query_to(surface.put()));
            wil::com_ptr<ID2D1Bitmap1> bitmap;
            const auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                                            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                                                            dpi,
                                                            dpi);
            THROW_IF_FAILED(context->CreateBitmapFromDxgiSurface(surface.get(), &properties, bitmap.put()));
            context->SetTarget(bitmap.get());
            context->SetDpi(dpi, dpi);
            wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
            THROW_IF_FAILED(context->CreateSolidColorBrush(D2D1::ColorF(.1f, .1f, .1f), text.put()));
            THROW_IF_FAILED(context->CreateSolidColorBrush(D2D1::ColorF(.2f, .4f, .7f), stroke.put()));
            THROW_IF_FAILED(context->CreateSolidColorBrush(D2D1::ColorF(.9f, .95f, 1.0f), fill.put()));
            context->BeginDraw();
            context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
            if (observeDraw)
                observeDraw(true);
            const auto submitted = prepared.Draw(context.get(), text.get(), stroke.get(), fill.get(), D2D1::Point2F(8.0f, 8.0f));
            if (observeDraw)
                observeDraw(false);
            check(submitted, "native diagram draws into each unrelated caller-owned WARP device");
            THROW_IF_FAILED(context->EndDraw());
            std::vector<std::uint8_t> pixels;
            THROW_IF_FAILED(graphics.Read(pixels));
            std::size_t visible{};
            for (std::size_t index = 3u; index < pixels.size(); index += 4u)
                visible += pixels[index] != 0u;
            check(visible > 1000u && prepared.Generation() == generation && prepared.Geometry() == plan && calls->layouts == layouts,
                  "device replacement and DPI reuse prepared labels/paths with visible actual pixels and zero hidden SDK layouts");
        }
}
template <typename Check> inline void RasterOracle(Check&& check)
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
    const auto drawing = DrawingFactory();
    const auto write   = WriteFactory();
    for (const auto source : {L"graph TD;A[label]",
                              L"graph TD;A[\"Unicode \u03b2 \U0001f600\"]",
                              L"graph TD;A[\"\u05d0\u05d1\u05d2\"]",
                              L"graph TD;A[\"first\\nsecond\"]",
                              L"graph TD;A[one<br>two #9829;]",
                              L"classDiagram\nclass Shape\n<<interface>> Shape\n",
                              L"classDiagram\nclass Shape {\n+size: List~int~\n+draw() Result~T~\n}\n",
                              L"graph TD;A[\"ordinary Unicode words wrap over several lines without truncation\"]"})
        for (const auto direction : {DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT})
        {
            const auto graph = FlowchartLayoutTests::Graph(source);
            LayoutOptions options;
            options.readingDirection = direction;
            PreparedFlowchart prepared;
            prepared.Begin(write.get(), drawing.get(), graph, options, {1u, 1u});
            Complete(prepared, check);
            check(prepared.Ready(), "plain Unicode/bidi/multiline labels produce native geometry");
            if (graph->nodes[0u].label.size() > 50u)
                check(prepared.Geometry()->nodes[0u].label.width <= 230.0 && prepared.Geometry()->nodes[0u].label.height >= 35.0,
                      "long Unicode label wraps within its independent 225-DIP typography bound");
            for (const auto dpi : {96.0f, 144.0f, 192.0f})
            {
                const auto pixels = [&](bool reference)
                {
                    constexpr UINT width = 600u, height = 300u, stride = width * 4u;
                    wil::com_ptr<IWICBitmap> bitmap;
                    THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
                    wil::com_ptr<ID2D1RenderTarget> target;
                    const auto properties = D2D1::RenderTargetProperties(
                        D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);
                    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
                    target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
                    wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
                    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(.1f, .1f, .1f), text.put()));
                    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(.2f, .4f, .7f), stroke.put()));
                    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(.9f, .95f, 1.0f), fill.put()));
                    const auto previous = D2D1::Matrix3x2F::Scale(1.25f, 1.25f) * D2D1::Matrix3x2F::Translation(3.0f, 2.0f);
                    target->SetTransform(previous);
                    target->BeginDraw();
                    target->Clear(D2D1::ColorF(D2D1::ColorF::White));
                    const auto origin = D2D1::Point2F(7.0f, 5.0f);
                    if (reference)
                    {
                        // Independent public SDK oracle: no prepared graph, geometry
                        // service, source map or paint helper is used for these pixels.
                        wil::com_ptr<IDWriteTextFormat> format;
                        THROW_IF_FAILED(write->CreateTextFormat(L"Segoe UI",
                                                                nullptr,
                                                                DWRITE_FONT_WEIGHT_NORMAL,
                                                                DWRITE_FONT_STYLE_NORMAL,
                                                                DWRITE_FONT_STRETCH_NORMAL,
                                                                18.0f,
                                                                L"en-us",
                                                                format.put()));
                        THROW_IF_FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP));
                        THROW_IF_FAILED(format->SetReadingDirection(direction));
                        wil::com_ptr<IDWriteTextLayout> layout;
                        const auto& label = graph->nodes[0u].label;
                        THROW_IF_FAILED(write->CreateTextLayout(label.data(), static_cast<UINT32>(label.size()), format.get(), 225.0f, 65536.0f, layout.put()));
                        DWRITE_TEXT_METRICS metrics{};
                        DWRITE_OVERHANG_METRICS ink{};
                        THROW_IF_FAILED(layout->GetMetrics(&metrics));
                        THROW_IF_FAILED(layout->GetOverhangMetrics(&ink));
                        const auto left = std::min(metrics.left, -ink.left), top = std::min(metrics.top, -ink.top);
                        const double labelWidth =
                            static_cast<double>(std::max(metrics.left + metrics.widthIncludingTrailingWhitespace, metrics.layoutWidth + ink.right)) - left;
                        const double labelHeight = static_cast<double>(std::max(metrics.top + metrics.height, metrics.layoutHeight + ink.bottom)) - top;
                        const auto boxWidth = std::max(40.0, labelWidth + 24.0), boxHeight = std::max(32.0, labelHeight + 16.0);
                        target->SetTransform(D2D1::Matrix3x2F::Translation(origin.x, origin.y) * previous);
                        const auto bounds = D2D1::RectF(8.0f, 8.0f, static_cast<float>(8.0 + boxWidth), static_cast<float>(8.0 + boxHeight));
                        target->FillRectangle(bounds, fill.get());
                        target->DrawRectangle(bounds, stroke.get());
                        if (std::wstring_view(source).starts_with(L"classDiagram\nclass Shape {"))
                        {
                            // Independent literal source positions and public SDK hit testing.
                            for (const UINT32 position : {6u, 23u})
                            {
                                FLOAT x{}, y{};
                                DWRITE_HIT_TEST_METRICS hit{};
                                THROW_IF_FAILED(layout->HitTestTextPosition(position, FALSE, &x, &y, &hit));
                                const auto lineY = static_cast<float>(8.0 + (boxHeight - labelHeight) / 2.0) - top + y;
                                target->DrawLine(D2D1::Point2F(bounds.left, lineY), D2D1::Point2F(bounds.right, lineY), stroke.get());
                            }
                        }
                        target->DrawTextLayout(D2D1::Point2F(static_cast<float>(8.0 + (boxWidth - labelWidth) / 2.0) - left,
                                                             static_cast<float>(8.0 + (boxHeight - labelHeight) / 2.0) - top),
                                               layout.get(),
                                               text.get(),
                                               D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
                        target->SetTransform(previous);
                    }
                    else
                    {
                        check(! prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), D2D1::Point2F(std::numeric_limits<float>::quiet_NaN(), 0.0f)),
                              "invalid drawing origin is refused before target mutation");
                        check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), origin),
                              "prepared native diagram submits to its caller's borrowed target");
                    }
                    D2D1_MATRIX_3X2_F after;
                    target->GetTransform(&after);
                    check(after._11 == previous._11 && after._12 == previous._12 && after._21 == previous._21 && after._22 == previous._22 &&
                              after._31 == previous._31 && after._32 == previous._32,
                          "native diagram restores every caller transform component");
                    THROW_IF_FAILED(target->EndDraw());
                    std::vector<BYTE> bytes(stride * height);
                    THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(bytes.size()), bytes.data()));
                    return std::vector<BYTE>(bytes);
                };
                check(pixels(false) == pixels(true), "native diagram Unicode/bidi/multiline pixels exactly match an independent SDK oracle at each DPI");
            }
            if (direction == DWRITE_READING_DIRECTION_LEFT_TO_RIGHT && graph->nodes[0u].label == L"label")
            {
                const auto otherFactory = DrawingFactory();
                wil::com_ptr<IWICBitmap> bitmap;
                THROW_IF_FAILED(imaging->CreateBitmap(32u, 32u, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
                wil::com_ptr<ID2D1RenderTarget> otherTarget;
                THROW_IF_FAILED(otherFactory->CreateWicBitmapRenderTarget(bitmap.get(), D2D1::RenderTargetProperties(), otherTarget.put()));
                wil::com_ptr<ID2D1SolidColorBrush> brush;
                THROW_IF_FAILED(otherTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), brush.put()));
                otherTarget->BeginDraw();
                check(! prepared.Draw(otherTarget.get(), brush.get(), brush.get(), brush.get(), {}),
                      "an incompatible drawing factory is refused before submitting foreign resources");
                THROW_IF_FAILED(otherTarget->EndDraw());
            }
        }
}
template <typename Check> inline void EdgeLabelRasterOracle(Check&& check)
{
    const auto init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(init);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(init))
            CoUninitialize();
    });
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    const auto drawing = DrawingFactory();
    const auto write   = WriteFactory();
    const auto graph   = FlowchartLayoutTests::Graph(L"graph TD;A[load]-->B{valid};B-->|yes|C(Display);B-->|no|D((Retry));D-->A;C-->C");
    LayoutOptions options;
    PreparedFlowchart prepared;
    prepared.Begin(write.get(), drawing.get(), graph, options, {1u, 1u});
    Complete(prepared, check);
    check(prepared.Ready(), "label fixture requires prepared native geometry");
    const auto plan  = prepared.Geometry();
    const auto width = static_cast<UINT>(std::ceil(plan->extent.width)), height = static_cast<UINT>(std::ceil(plan->extent.height)), stride = width * 4u;
    const auto pixels = [&](bool reference)
    {
        wil::com_ptr<IWICBitmap> bitmap;
        THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
        wil::com_ptr<ID2D1RenderTarget> target;
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), D2D1::RenderTargetProperties(), target.put()));
        target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(.1f, .1f, .1f), text.put()));
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(.2f, .4f, .7f), stroke.put()));
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(.9f, .95f, 1.0f), fill.put()));
        target->BeginDraw();
        target->Clear(D2D1::ColorF(D2D1::ColorF::White));
        if (! reference)
            check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}), "actual label fixture submits");
        else
        {
            wil::com_ptr<IDWriteTextFormat> format;
            THROW_IF_FAILED(write->CreateTextFormat(
                L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"en-us", format.put()));
            THROW_IF_FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP));
            for (std::size_t i = 0u; i < graph->edges.size(); ++i)
                if (! graph->edges[i].label.empty())
                {
                    const auto& source = graph->edges[i].label;
                    const auto box     = plan->edges[i].label;
                    wil::com_ptr<IDWriteTextLayout> layout;
                    THROW_IF_FAILED(write->CreateTextLayout(source.data(), static_cast<UINT32>(source.size()), format.get(), 225.0f, 65536.0f, layout.put()));
                    DWRITE_TEXT_METRICS metrics{};
                    DWRITE_OVERHANG_METRICS ink{};
                    THROW_IF_FAILED(layout->GetMetrics(&metrics));
                    THROW_IF_FAILED(layout->GetOverhangMetrics(&ink));
                    const auto left = std::min(metrics.left, -ink.left), top = std::min(metrics.top, -ink.top);
                    const auto bounds = D2D1::RectF(static_cast<float>(box.x) - 2.0f,
                                                    static_cast<float>(box.y) - 2.0f,
                                                    static_cast<float>(box.x + box.width) + 2.0f,
                                                    static_cast<float>(box.y + box.height) + 2.0f);
                    target->FillRectangle(bounds, fill.get());
                    target->DrawTextLayout(D2D1::Point2F(static_cast<float>(box.x) - left, static_cast<float>(box.y) - top),
                                           layout.get(),
                                           text.get(),
                                           D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
                }
        }
        THROW_IF_FAILED(target->EndDraw());
        std::vector<BYTE> bytes(stride * height);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(bytes.size()), bytes.data()));
        return std::vector<BYTE>(bytes);
    };
    const auto actual = pixels(false), expected = pixels(true);
    std::size_t compared{}, different{};
    for (const auto& edge : plan->edges)
        if (edge.hasLabel)
        {
            const auto box = edge.label;
            for (auto y = static_cast<UINT>(std::ceil(box.y)); y < static_cast<UINT>(std::floor(box.y + box.height)); ++y)
                for (auto x = static_cast<UINT>(std::ceil(box.x)); x < static_cast<UINT>(std::floor(box.x + box.width)); ++x)
                {
                    const auto offset = (static_cast<std::size_t>(y) * width + x) * 4u;
                    ++compared;
                    if (! std::equal(actual.data() + offset, actual.data() + offset + 4u, expected.data() + offset))
                        ++different;
                }
        }
    std::cout << "Edge label interior pixels compared=" << compared << " different=" << different << "\n";
    check(compared > 100u && different == 0u, "routes must not obscure edge labels or their opaque background");

    const auto arrowHeadCenter = [&](std::wstring_view source)
    {
        const auto relationGraph = FlowchartLayoutTests::Graph(source);
        PreparedFlowchart relationPaint;
        relationPaint.Begin(write.get(), drawing.get(), relationGraph, options, {2u, 2u});
        Complete(relationPaint, check);
        const auto relationPlan = relationPaint.Geometry();
        wil::com_ptr<IWICBitmap> bitmap;
        THROW_IF_FAILED(imaging->CreateBitmap(512u, 512u, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
        wil::com_ptr<ID2D1RenderTarget> target;
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), D2D1::RenderTargetProperties(), target.put()));
        wil::com_ptr<ID2D1SolidColorBrush> black, white;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), black.put()));
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), white.put()));
        target->BeginDraw();
        target->Clear(D2D1::ColorF(D2D1::ColorF::White));
        check(relationPaint.Draw(target.get(), black.get(), black.get(), white.get(), {}), "class relation fixture submits native painting");
        THROW_IF_FAILED(target->EndDraw());
        constexpr UINT stride = 512u * 4u;
        std::vector<BYTE> pixels(stride * 512u);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
        const auto& edge   = relationPlan->edges.front();
        const auto diamond = edge.relation == FlowchartRelation::Aggregation || edge.relation == FlowchartRelation::Composition;
        const auto x       = static_cast<UINT>(
            std::lround(diamond ? (edge.diamond[0u].x + edge.diamond[2u].x) / 2.0 : (edge.arrow[0u].x + edge.arrow[1u].x + edge.arrow[2u].x) / 3.0));
        const auto y = static_cast<UINT>(
            std::lround(diamond ? (edge.diamond[0u].y + edge.diamond[2u].y) / 2.0 : (edge.arrow[0u].y + edge.arrow[1u].y + edge.arrow[2u].y) / 3.0));
        const auto offset = (static_cast<std::size_t>(y) * 512u + x) * 4u;
        return std::array<BYTE, 4u>{pixels[offset], pixels[offset + 1u], pixels[offset + 2u], pixels[offset + 3u]};
    };
    for (const auto direction : {L"TB", L"BT", L"LR", L"RL"})
    {
        const auto header = L"classDiagram\ndirection " + std::wstring(direction) + L"\n";
        const auto hollow = arrowHeadCenter(header + L"Base <|-- Derived\n");
        const auto filled = arrowHeadCenter(header + L"Base --> Derived\n");
        check(hollow[0u] > 240u && hollow[1u] > 240u && hollow[2u] > 240u && filled[0u] < 32u && filled[1u] < 32u && filled[2u] < 32u,
              "class inheritance has a hollow triangle and associations a filled arrowhead in all four directions");
        const auto aggregate   = arrowHeadCenter(header + L"Whole o-- Part\n");
        const auto composition = arrowHeadCenter(header + L"Whole *-- Part\n");
        check(aggregate[0u] > 240u && aggregate[1u] > 240u && aggregate[2u] > 240u && composition[0u] < 32u && composition[1u] < 32u && composition[2u] < 32u,
              "class aggregation has a hollow diamond and composition a filled diamond in all four directions");
    }

    const auto stadiumGraph = FlowchartLayoutTests::Graph(L"flowchart LR;Terminal([capsule])");
    PreparedFlowchart stadiumPaint;
    stadiumPaint.Begin(write.get(), drawing.get(), stadiumGraph, options, {3u, 3u});
    Complete(stadiumPaint, check);
    const auto stadiumPlan   = stadiumPaint.Geometry();
    const auto stadiumBounds = stadiumPlan->nodes.front().bounds;
    wil::com_ptr<IWICBitmap> stadiumBitmap;
    THROW_IF_FAILED(imaging->CreateBitmap(512u, 512u, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, stadiumBitmap.put()));
    wil::com_ptr<ID2D1RenderTarget> stadiumTarget;
    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(stadiumBitmap.get(), D2D1::RenderTargetProperties(), stadiumTarget.put()));
    wil::com_ptr<ID2D1SolidColorBrush> stadiumBlack, stadiumWhite, stadiumFill;
    THROW_IF_FAILED(stadiumTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), stadiumBlack.put()));
    THROW_IF_FAILED(stadiumTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), stadiumWhite.put()));
    THROW_IF_FAILED(stadiumTarget->CreateSolidColorBrush(D2D1::ColorF(.9f, .95f, 1.0f), stadiumFill.put()));
    stadiumTarget->BeginDraw();
    stadiumTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
    check(stadiumPaint.Draw(stadiumTarget.get(), stadiumBlack.get(), stadiumBlack.get(), stadiumFill.get(), {}), "stadium geometry submits native painting");
    THROW_IF_FAILED(stadiumTarget->EndDraw());
    constexpr UINT stadiumStride = 512u * 4u;
    std::vector<BYTE> stadiumPixels(stadiumStride * 512u);
    THROW_IF_FAILED(stadiumBitmap->CopyPixels(nullptr, stadiumStride, static_cast<UINT>(stadiumPixels.size()), stadiumPixels.data()));
    const auto pixel = [&](double x, double y)
    {
        const auto offset = (static_cast<std::size_t>(std::lround(y)) * 512u + static_cast<UINT>(std::lround(x))) * 4u;
        return std::array<BYTE, 4u>{stadiumPixels[offset], stadiumPixels[offset + 1u], stadiumPixels[offset + 2u], stadiumPixels[offset + 3u]};
    };
    const auto radius          = stadiumBounds.height / 2.0;
    const auto corner          = pixel(stadiumBounds.x + 1.0, stadiumBounds.y + 1.0);
    const auto capOutside      = pixel(stadiumBounds.x + 1.0, stadiumBounds.y + stadiumBounds.height * .2);
    const auto topMid          = pixel(stadiumBounds.x + stadiumBounds.width / 2.0, stadiumBounds.y);
    const auto inside          = pixel(stadiumBounds.x + radius + 2.0, stadiumBounds.y + radius);
    const auto capsuleContains = [&](double x, double y)
    {
        const auto centerX  = stadiumBounds.x + stadiumBounds.width / 2.0;
        const auto centerY  = stadiumBounds.y + radius;
        const auto nearestX = std::clamp(x, centerX - (stadiumBounds.width / 2.0 - radius), centerX + (stadiumBounds.width / 2.0 - radius));
        const auto dx = x - nearestX, dy = y - centerY;
        return dx * dx + dy * dy <= radius * radius;
    };
    const bool capsuleRaster = ! capsuleContains(stadiumBounds.x + 1.0, stadiumBounds.y + 1.0) &&
                               capsuleContains(stadiumBounds.x + radius + 2.0, stadiumBounds.y + radius) && corner[0u] > 240u && corner[1u] > 240u &&
                               corner[2u] > 240u && capOutside[0u] > 240u && capOutside[1u] > 240u && capOutside[2u] > 240u && topMid[0u] < 200u &&
                               inside[2u] > 180u;
    if (! capsuleRaster)
    {
        std::cerr << "Stadium bounds=" << stadiumBounds.x << ',' << stadiumBounds.y << ',' << stadiumBounds.width << ',' << stadiumBounds.height;
        for (const auto& sample : {corner, capOutside, topMid, inside})
            std::cerr << " pixel=" << unsigned(sample[0u]) << ',' << unsigned(sample[1u]) << ',' << unsigned(sample[2u]) << ',' << unsigned(sample[3u]);
        std::cerr << '\n';
    }
    check(capsuleRaster, "stadium raster has rounded capsule corners, a curved top boundary, and filled center rather than rectangular corners");
}

template <typename Check> inline void CardinalityRaster(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto drawing   = DrawingFactory();
    const auto write     = WriteFactory();
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    for (const auto direction : {L"TD", L"BT", L"LR", L"RL"})
    {
        const auto source = std::wstring(L"classDiagram\ndirection ") + direction + L"\nA \"1\" --> \"0..*\" B\n";
        const auto graph  = FlowchartLayoutTests::Graph(source);
        LayoutOptions options;
        PreparedFlowchart prepared;
        prepared.Begin(write.get(), drawing.get(), graph, options, {1u, 1u});
        Complete(prepared, check);
        const auto geometry = prepared.Geometry();
        check(prepared.Ready() && geometry && geometry->issue == FlowchartLayoutIssue::None && geometry->edges.size() == 1u,
              "endpoint cardinalities prepare in each graph orientation");
        if (! geometry || geometry->issue != FlowchartLayoutIssue::None || geometry->edges.size() != 1u)
            continue;
        constexpr UINT width = 600u, height = 300u, stride = width * 4u;
        wil::com_ptr<IWICBitmap> bitmap;
        THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
        wil::com_ptr<ID2D1RenderTarget> target;
        const auto properties =
            D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
        target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), text.put()));
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()));
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
        target->BeginDraw();
        target->Clear(D2D1::ColorF(D2D1::ColorF::White));
        check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}), "cardinality fixture paints");
        THROW_IF_FAILED(target->EndDraw());
        std::vector<BYTE> pixels(stride * height);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
        for (const auto rect : {geometry->edges[0].fromCardinality, geometry->edges[0].toCardinality})
        {
            std::size_t ink{};
            for (UINT y = static_cast<UINT>(rect.y); y < std::min(height, static_cast<UINT>(std::ceil(rect.y + rect.height))); ++y)
                for (UINT x = static_cast<UINT>(rect.x); x < std::min(width, static_cast<UINT>(std::ceil(rect.x + rect.width))); ++x)
                {
                    const auto at = static_cast<std::size_t>(y) * stride + x * 4u;
                    ink += pixels[at] < 80u && pixels[at + 1u] < 80u && pixels[at + 2u] < 80u;
                }
            check(ink != 0u, "each independent cardinality region contains actual text ink");
        }
    }
    const auto crowded = FlowchartLayoutTests::Graph(L"classDiagram\nA \"1\" --> B\n");
    NativeFlowchartLayout layout;
    const std::array<FlowchartSize, 2u> nodes{{{20.0, 20.0}, {20.0, 20.0}}};
    const std::array<FlowchartSize, 1u> edges{{{0.0, 0.0}}}, from{{{100.0, 1000.0}}}, to{{{0.0, 0.0}}};
    layout.Begin(crowded, nodes, edges, from, to);
    const auto rejected = FlowchartLayoutTests::Complete(layout, check);
    check(rejected->issue == FlowchartLayoutIssue::DiagramExtent && rejected->nodes.empty() && rejected->edges.empty(),
          "colliding endpoint labels fail without publishing partial geometry");
}
template <typename Check> inline void GroupRaster(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto drawing   = DrawingFactory();
    const auto write     = WriteFactory();
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    for (const auto family : {0u, 1u, 2u})
        for (const auto direction : {L"TD", L"BT", L"LR", L"RL"})
        {
            const auto source = family == 1u ? std::wstring(L"stateDiagram-v2\ndirection ") + direction + L"\nstate Outer {\nstate Inner {\nA-->B\n}\n}\n"
                                : family == 2u ? std::wstring(L"classDiagram\ndirection ") + direction + L"\nnamespace Outer {\nnamespace Inner {\nA-->B\n}\n}\n"
                                               : std::wstring(L"graph ") + direction + L"\nsubgraph O[Outer]\nsubgraph I[Inner]\nA-->B\nend\nend\n";
            const auto graph  = FlowchartLayoutTests::Graph(source);
            PreparedFlowchart prepared;
            prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
            Complete(prepared, check);
            const auto geometry = prepared.Geometry();
            check(prepared.Ready() && geometry && geometry->groups.size() == 2u, "nested groups prepare in all four orientations");
            if (! geometry || geometry->groups.size() != 2u)
                continue;
            const auto outer = geometry->groups[0].bounds, inner = geometry->groups[1].bounds;
            check(outer.x < inner.x && outer.y < inner.y && outer.x + outer.width > inner.x + inner.width && outer.y + outer.height > inner.y + inner.height,
                  "outer group visibly contains the complete inner group");
            constexpr UINT width = 600u, height = 300u, stride = width * 4u;
            wil::com_ptr<IWICBitmap> bitmap;
            THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
            wil::com_ptr<ID2D1RenderTarget> target;
            const auto properties =
                D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
            THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
            target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
            THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), text.put()));
            THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()));
            THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
            target->BeginDraw();
            target->Clear(D2D1::ColorF(D2D1::ColorF::White));
            check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}), "group fixture paints");
            THROW_IF_FAILED(target->EndDraw());
            std::vector<BYTE> pixels(stride * height);
            THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
            for (const auto& group : geometry->groups)
            {
                std::size_t ink{};
                const auto rect = group.label;
                for (UINT y = static_cast<UINT>(rect.y); y < std::min(height, static_cast<UINT>(std::ceil(rect.y + rect.height))); ++y)
                    for (UINT x = static_cast<UINT>(rect.x); x < std::min(width, static_cast<UINT>(std::ceil(rect.x + rect.width))); ++x)
                    {
                        const auto at = static_cast<std::size_t>(y) * stride + x * 4u;
                        ink += pixels[at] < 80u && pixels[at + 1u] < 80u && pixels[at + 2u] < 80u;
                    }
                check(ink != 0u, "every group title has visible text ink within its own measured bounds");
            }
        }
    const auto grid = FlowchartLayoutTests::Graph(L"graph TD\nC1-->A2\nsubgraph One\nA1-->A2\nend\nsubgraph Two\nB1-->B2\nend\nsubgraph Three\nC1-->C2\nend\n");
    PreparedFlowchart gridPaint;
    gridPaint.Begin(write.get(), drawing.get(), grid, {}, {1u, 1u});
    Complete(gridPaint, check);
    check(gridPaint.Ready() && gridPaint.Geometry() && gridPaint.Geometry()->groups.size() == 3u,
          "interleaved reference group grid prepares with actual measured labels");
    if (gridPaint.Ready())
    {
        constexpr UINT width = 600u, height = 300u, stride = width * 4u;
        wil::com_ptr<IWICBitmap> bitmap;
        THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
        wil::com_ptr<ID2D1RenderTarget> target;
        const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
                                                               D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
        wil::com_ptr<ID2D1SolidColorBrush> brush, fill;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), brush.put()));
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
        target->BeginDraw(); target->Clear(D2D1::ColorF(D2D1::ColorF::White));
        check(gridPaint.Draw(target.get(), brush.get(), brush.get(), fill.get(), {}), "group grid paints");
        THROW_IF_FAILED(target->EndDraw());
        std::vector<BYTE> pixels(stride * height);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
        for (const auto& group : gridPaint.Geometry()->groups)
        {
            std::size_t ink{};
            for (UINT y = static_cast<UINT>(group.label.y); y < std::min(height, static_cast<UINT>(std::ceil(group.label.y + group.label.height))); ++y)
                for (UINT x = static_cast<UINT>(group.label.x); x < std::min(width, static_cast<UINT>(std::ceil(group.label.x + group.label.width))); ++x)
                {
                    const auto offset = static_cast<std::size_t>(y) * stride + x * 4u;
                    ink += pixels[offset] < 80u && pixels[offset + 1u] < 80u && pixels[offset + 2u] < 80u;
                }
            check(ink != 0u, "each independently placed group title has visible raster ink");
        }
    }
    const auto crowded = FlowchartLayoutTests::Graph(L"graph TD\nsubgraph A[Extremely wide first group title]\nX\nend\n"
                                                     L"subgraph B[Extremely wide second group title]\nY\nend\n");
    PreparedFlowchart rejected;
    rejected.Begin(write.get(), drawing.get(), crowded, {}, {1u, 1u});
    Complete(rejected, check);
    check(! rejected.Ready() && rejected.Geometry() && rejected.Geometry()->issue == FlowchartLayoutIssue::DiagramExtent && rejected.Geometry()->groups.empty(),
          "overlapping unrelated groups fail without publishing partial geometry");
}
template <typename Check> inline void SequenceRaster(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept { if (SUCCEEDED(initialized)) CoUninitialize(); });
    const auto graph = FlowchartLayoutTests::Graph(L"sequenceDiagram\nparticipant A as Alpha\nparticipant B as Beta\nA->>B: hello\nB-->>A: reply\nA-)A: later\n");
    const auto write = WriteFactory();
    const auto drawing = DrawingFactory();
    PreparedFlowchart prepared;
    prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
    Complete(prepared, check);
    const auto geometry = prepared.Geometry();
    check(prepared.Ready() && geometry && geometry->nodes.size() == 2u && geometry->edges.size() == 3u,
          "sequence participants and messages prepare as a native timeline");
    if (! prepared.Ready() || ! geometry || geometry->nodes.size() != 2u || geometry->edges.size() != 3u) return;
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    constexpr UINT width = 600u, height = 400u, stride = width * 4u;
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
    wil::com_ptr<ID2D1RenderTarget> target;
    const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
                                                           D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
    wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), text.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
    target->BeginDraw(); target->Clear(D2D1::ColorF(D2D1::ColorF::White));
    check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}), "native sequence timeline paints");
    THROW_IF_FAILED(target->EndDraw());
    std::vector<BYTE> pixels(stride * height);
    THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
    const auto blueNear = [&](double px, double py)
    {
        const auto x = static_cast<int>(px), y = static_cast<int>(py);
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
                if (x + dx >= 0 && y + dy >= 0 && x + dx < static_cast<int>(width) && y + dy < static_cast<int>(height))
                {
                    const auto offset = static_cast<std::size_t>(y + dy) * stride + static_cast<std::size_t>(x + dx) * 4u;
                    if (pixels[offset] > 150u && pixels[offset + 1u] < 100u && pixels[offset + 2u] < 100u) return true;
                }
        return false;
    };
    const auto& first = geometry->edges[0u];
    check(blueNear((first.path[0u].x + first.path[3u].x) / 2.0, first.path[0u].y), "sequence message has actual horizontal raster ink");
    const auto& loop = geometry->edges[2u];
    check(loop.openArrow && blueNear((loop.arrow[0u].x + loop.arrow[1u].x) / 2.0, (loop.arrow[0u].y + loop.arrow[1u].y) / 2.0),
          "open asynchronous self-message arrowhead has visible raster ink");
    const auto& participant = geometry->nodes[0u].bounds;
    const auto lifelineX = participant.x + participant.width / 2.0;
    std::size_t lifelineInk{};
    for (double y = participant.y + participant.height + 2.0; y < participant.y + participant.height + 18.0; y += 2.0)
        lifelineInk += blueNear(lifelineX, y);
    check(lifelineInk > 0u, "participant has a visible lifeline below its header");
}
template <typename Check> inline void StateNoteRaster(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto drawing   = DrawingFactory();
    const auto write     = WriteFactory();
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    for (const auto direction : {L"TD", L"BT", L"LR", L"RL"})
        for (const bool left : {false, true})
          for (const bool classNote : {false, true})
        {
            if (classNote && ! left) continue;
            const auto source = classNote ? std::wstring(L"classDiagram\ndirection ") + direction + L"\nclass S\nnote for S \"hello\"\n"
                                          : std::wstring(L"stateDiagram-v2\ndirection ") + direction + L"\nS\n" +
                                                (left ? L"note left of S\nhello\nworld\nend note\n" : L"note right of S: hello\n");
            const auto graph = FlowchartLayoutTests::Graph(source);
            PreparedFlowchart prepared;
            prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
            Complete(prepared, check);
            const auto geometry = prepared.Geometry();
            check(prepared.Ready() && geometry && geometry->nodes.size() == 2u && geometry->edges.size() == 1u, "state note prepares in every orientation");
            if (! geometry || geometry->nodes.size() != 2u || geometry->edges.size() != 1u)
                continue;
            const auto state = geometry->nodes[0].bounds, note = geometry->nodes[1].bounds;
            check(left ? note.x + note.width < state.x : note.x > state.x + state.width, "state note is on the authored physical side of its state");
            check(geometry->edges[0].relation == FlowchartRelation::Note && geometry->edges[0].style == FlowchartLineStyle::Dotted,
                  "state note has a dotted attachment");
            constexpr UINT width = 600u, height = 300u, stride = width * 4u;
            wil::com_ptr<IWICBitmap> bitmap;
            THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
            wil::com_ptr<ID2D1RenderTarget> target;
            const auto properties =
                D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
            THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
            target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
            THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), text.put()));
            THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()));
            THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
            target->BeginDraw();
            target->Clear(D2D1::ColorF(D2D1::ColorF::White));
            check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}), "state note fixture paints");
            THROW_IF_FAILED(target->EndDraw());
            std::vector<BYTE> pixels(stride * height);
            THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
            const auto label = geometry->nodes[1].label;
            std::size_t ink{};
            for (UINT y = static_cast<UINT>(label.y); y < std::min(height, static_cast<UINT>(std::ceil(label.y + label.height))); ++y)
                for (UINT x = static_cast<UINT>(label.x); x < std::min(width, static_cast<UINT>(std::ceil(label.x + label.width))); ++x)
                {
                    const auto at = static_cast<std::size_t>(y) * stride + x * 4u;
                    ink += pixels[at] < 80u && pixels[at + 1u] < 80u && pixels[at + 2u] < 80u;
                }
            check(ink != 0u, "state note contains visible text ink");
        }
    const auto standalone = FlowchartLayoutTests::Graph(L"classDiagram\nnote \"Visible standalone note\"\n");
    PreparedFlowchart note;
    note.Begin(write.get(), drawing.get(), standalone, {}, {1u, 1u});
    Complete(note, check);
    check(note.Ready() && note.Geometry() && note.Geometry()->nodes.size() == 1u && note.Geometry()->edges.empty(),
          "standalone class note prepares without an attachment");
    if (note.Ready())
    {
        constexpr UINT width = 400u, height = 200u, stride = width * 4u;
        wil::com_ptr<IWICBitmap> bitmap;
        THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
        wil::com_ptr<ID2D1RenderTarget> target;
        const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
                                                               D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
        wil::com_ptr<ID2D1SolidColorBrush> brush, fill;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), brush.put()));
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
        target->BeginDraw();
        target->Clear(D2D1::ColorF(D2D1::ColorF::White));
        check(note.Draw(target.get(), brush.get(), brush.get(), fill.get(), {}), "standalone class note paints");
        THROW_IF_FAILED(target->EndDraw());
        std::vector<BYTE> pixels(stride * height);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
        const auto label = note.Geometry()->nodes[0].label;
        std::size_t ink{};
        for (UINT y = static_cast<UINT>(label.y); y < std::min(height, static_cast<UINT>(std::ceil(label.y + label.height))); ++y)
            for (UINT x = static_cast<UINT>(label.x); x < std::min(width, static_cast<UINT>(std::ceil(label.x + label.width))); ++x)
            {
                const auto offset = static_cast<std::size_t>(y) * stride + x * 4u;
                ink += pixels[offset] < 80u && pixels[offset + 1u] < 80u && pixels[offset + 2u] < 80u;
            }
        check(ink != 0u, "standalone class note contains visible text ink");
    }
    const auto crowded = FlowchartLayoutTests::Graph(L"stateDiagram-v2\nS\nnote right of S: one\nnote right of S: two\n");
    PreparedFlowchart rejected;
    rejected.Begin(write.get(), drawing.get(), crowded, {}, {1u, 1u});
    Complete(rejected, check);
    check(! rejected.Ready() && rejected.Geometry() && rejected.Geometry()->issue == FlowchartLayoutIssue::DiagramExtent && rejected.Geometry()->nodes.empty(),
          "colliding notes fail without publishing partial geometry");
}
template <typename Check> inline void LineStyleRasterOracle(Check&& check)
{
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    const auto drawing = DrawingFactory();
    const auto write   = WriteFactory();
    std::array<unsigned, 4u> ink{}, pathHits{};
    constexpr std::array tokens{L"---", L"-.-", L"===", L"~~~"};
    for (std::size_t index = 0u; index < tokens.size(); ++index)
    {
        const auto graph = FlowchartLayoutTests::Graph(L"flowchart LR\nA " + std::wstring(tokens[index]) + L" B\n");
        PreparedFlowchart prepared;
        prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
        Complete(prepared, check);
        check(prepared.Ready() && prepared.Geometry()->edges.size() == 1u && prepared.Geometry()->edges[0u].style == graph->edges[0u].style,
              "prepared edge retains its source style");
        wil::com_ptr<IWICBitmap> bitmap;
        THROW_IF_FAILED(imaging->CreateBitmap(512u, 256u, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
        const auto properties =
            D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        wil::com_ptr<ID2D1RenderTarget> target;
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
        wil::com_ptr<ID2D1SolidColorBrush> stroke, fill;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f, 1.f), stroke.put()));
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.f), fill.put()));
        target->BeginDraw();
        target->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 0.f));
        check(prepared.Draw(target.get(), stroke.get(), stroke.get(), fill.get(), D2D1::Point2F(4.f, 4.f)), "styled diagram draws offscreen");
        THROW_IF_FAILED(target->EndDraw());
        std::vector<BYTE> pixels(512u * 256u * 4u);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, 512u * 4u, static_cast<UINT>(pixels.size()), pixels.data()));
        for (std::size_t offset = 3u; offset < pixels.size(); offset += 4u)
            if (pixels[offset])
                ++ink[index];
        const auto& path    = prepared.Geometry()->edges[0u].path;
        std::size_t longest = 1u;
        double length{};
        for (std::size_t segment = 1u; segment < path.size(); ++segment)
        {
            const auto distance = std::abs(path[segment].x - path[segment - 1u].x) + std::abs(path[segment].y - path[segment - 1u].y);
            if (distance > length)
            {
                length  = distance;
                longest = segment;
            }
        }
        check(length >= 10.0, "edge fixture has a measurable route segment");
        const auto a = path[longest - 1u], b = path[longest];
        for (auto step = static_cast<unsigned>(length * .2); step < static_cast<unsigned>(length * .8); ++step)
        {
            const double fraction = static_cast<double>(step) / length;
            const auto x          = static_cast<unsigned>(std::lround(4.0 + a.x + (b.x - a.x) * fraction));
            const auto y          = static_cast<unsigned>(std::lround(4.0 + a.y + (b.y - a.y) * fraction));
            check(x < 512u && y < 256u, "sample stays inside the offscreen target");
            if (x < 512u && y < 256u && pixels[(static_cast<std::size_t>(y) * 512u + x) * 4u + 3u])
                ++pathHits[index];
        }
    }
    check(ink[2u] > ink[0u] && pathHits[0u] > pathHits[1u] && pathHits[1u] > 0u && pathHits[3u] == 0u,
          "thick edges add ink, dotted edges contain visible gaps, and invisible edges add no path pixels");
    for (const auto& [direction, horizontal] : {std::pair{L"TB", false}, std::pair{L"BT", false}, std::pair{L"LR", true}, std::pair{L"RL", true}})
    {
        const auto graph = FlowchartLayoutTests::Graph(L"stateDiagram-v2\ndirection " + std::wstring(direction) + L"\nstate F <<fork>>\nA --> F\nF --> B\n");
        PreparedFlowchart prepared;
        prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
        Complete(prepared, check);
        check(prepared.Ready() && prepared.Geometry()->nodes[0u].shape == FlowchartShape::Bar, "fork fixture prepares its native bar");
        wil::com_ptr<IWICBitmap> bitmap;
        THROW_IF_FAILED(imaging->CreateBitmap(512u, 256u, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
        const auto properties =
            D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        wil::com_ptr<ID2D1RenderTarget> target;
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
        wil::com_ptr<ID2D1SolidColorBrush> stroke, fill;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f, 1.f), stroke.put()));
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.f), fill.put()));
        target->BeginDraw();
        target->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 0.f));
        check(prepared.Draw(target.get(), stroke.get(), stroke.get(), fill.get(), D2D1::Point2F(4.f, 4.f)), "fork diagram draws offscreen");
        THROW_IF_FAILED(target->EndDraw());
        std::vector<BYTE> pixels(512u * 256u * 4u);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, 512u * 4u, static_cast<UINT>(pixels.size()), pixels.data()));
        const auto bounds = prepared.Geometry()->nodes[0u].bounds;
        const auto x      = static_cast<unsigned>(std::lround(4.0 + bounds.x + bounds.width * (horizontal ? .5 : .25)));
        const auto y      = static_cast<unsigned>(std::lround(4.0 + bounds.y + bounds.height * (horizontal ? .25 : .5)));
        check(x < 512u && y < 256u && pixels[(static_cast<std::size_t>(y) * 512u + x) * 4u + 3u] != 0u,
              "fork bar paints opaque pixels away from its connector in each direction");
    }
    const auto hexagonGraph = FlowchartLayoutTests::Graph(L"flowchart TD;A{{label}}-->B");
    PreparedFlowchart hexagon;
    hexagon.Begin(write.get(), drawing.get(), hexagonGraph, {}, {1u, 1u});
    Complete(hexagon, check);
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(imaging->CreateBitmap(512u, 256u, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
    const auto properties =
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    wil::com_ptr<ID2D1RenderTarget> target;
    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
    wil::com_ptr<ID2D1SolidColorBrush> stroke, fill;
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f, 1.f), stroke.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.f), fill.put()));
    target->BeginDraw();
    target->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 0.f));
    check(hexagon.Draw(target.get(), stroke.get(), stroke.get(), fill.get(), D2D1::Point2F(4.f, 4.f)), "hexagon draws offscreen");
    THROW_IF_FAILED(target->EndDraw());
    std::vector<BYTE> pixels(512u * 256u * 4u);
    THROW_IF_FAILED(bitmap->CopyPixels(nullptr, 512u * 4u, static_cast<UINT>(pixels.size()), pixels.data()));
    const auto bounds = hexagon.Geometry()->nodes[0u].bounds;
    const auto y      = static_cast<unsigned>(4.0 + bounds.y);
    const auto middle = static_cast<unsigned>(4.0 + bounds.x + bounds.width * .5);
    const auto corner = static_cast<unsigned>(4.0 + bounds.x + 1.0);
    check(y < 256u && middle < 512u && corner < 512u && pixels[(static_cast<std::size_t>(y) * 512u + middle) * 4u + 3u] != 0u &&
              pixels[(static_cast<std::size_t>(y) * 512u + corner) * 4u + 3u] == 0u,
          "hexagon has a top edge but no rectangular top-left corner");
    const auto subroutineGraph = FlowchartLayoutTests::Graph(L"flowchart TD;A[[label]]-->B");
    PreparedFlowchart subroutine;
    subroutine.Begin(write.get(), drawing.get(), subroutineGraph, {}, {1u, 1u});
    Complete(subroutine, check);
    target->BeginDraw();
    target->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 0.f));
    check(subroutine.Draw(target.get(), stroke.get(), stroke.get(), fill.get(), D2D1::Point2F(4.f, 4.f)), "subroutine draws offscreen");
    THROW_IF_FAILED(target->EndDraw());
    THROW_IF_FAILED(bitmap->CopyPixels(nullptr, 512u * 4u, static_cast<UINT>(pixels.size()), pixels.data()));
    const auto subBounds = subroutine.Geometry()->nodes[0u].bounds;
    const auto subY      = static_cast<unsigned>(4.0 + subBounds.y + 4.0);
    const auto rule      = static_cast<unsigned>(4.0 + subBounds.x + 8.0);
    const auto gap       = static_cast<unsigned>(4.0 + subBounds.x + 4.0);
    check(subY < 256u && rule < 512u && gap < 512u && pixels[(static_cast<std::size_t>(subY) * 512u + rule) * 4u + 3u] != 0u &&
              pixels[(static_cast<std::size_t>(subY) * 512u + gap) * 4u + 3u] == 0u,
          "subroutine has a separate inset vertical rule with clear space before the outer border");
}
} // namespace PreparedFlowchartTests
