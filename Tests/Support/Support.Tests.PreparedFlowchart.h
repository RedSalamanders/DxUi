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
        check(result.layoutsPrepared <= 1u && result.blockSteps <= 3u && (result.blockSteps > 0u || result.status == LayoutPreparationStatus::Failed),
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
    invalid = prepared.Begin(nullptr, drawing.get(), graph, options, {3u, 3u}) == LayoutPreparationStatus::Failed;
    check(invalid && prepared.Pending() && prepared.Geometry() == original, "invalid native resource admission preserves pending work");
    const auto resumed = prepared.Advance({1u, 1u});
    check(resumed.status == LayoutPreparationStatus::Pending && resumed.result == S_OK && prepared.Result() == S_OK && prepared.Pending() &&
              prepared.Geometry() == original,
          "a rejected admission does not poison resumption of the prior request");
    prepared.Cancel();
    prepared.Begin(calls.Get(), drawing.get(), graph, options, {2u, 2u});
    calls->formatFailure = E_OUTOFMEMORY;
    bool failed{};
    Complete(prepared, check, calls.Get());
    failed               = prepared.Result() == E_OUTOFMEMORY;
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
        Complete(prepared, check, calls.Get());
        failed              = prepared.Result() == E_OUTOFMEMORY;
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
                                : family == 2u
                                    ? std::wstring(L"classDiagram\ndirection ") + direction + L"\nnamespace Outer {\nnamespace Inner {\nA-->B\n}\n}\n"
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
        const auto properties =
            D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
        wil::com_ptr<ID2D1SolidColorBrush> brush, fill;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), brush.put()));
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
        target->BeginDraw();
        target->Clear(D2D1::ColorF(D2D1::ColorF::White));
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
template <typename Check> inline void ConfiguredLabelWidth(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto graph     = FlowchartLayoutTests::Graph(L"graph TD;A[\"Unicode αβ שלום words and several more words for wrapping\"]");
    const auto write     = WriteFactory();
    const auto drawing   = DrawingFactory();
    for (const auto direction : {DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT})
    {
        LayoutOptions options;
        options.readingDirection     = direction;
        options.diagramLabelWidthDip = 100.f;
        PreparedFlowchart prepared;
        prepared.Begin(write.get(), drawing.get(), graph, options, {1u, 1u});
        Complete(prepared, check);
        const auto first = prepared.Geometry();
        wil::com_ptr<IDWriteTextFormat> format;
        THROW_IF_FAILED(write->CreateTextFormat(options.fontFamily.c_str(),
                                                nullptr,
                                                DWRITE_FONT_WEIGHT_NORMAL,
                                                DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL,
                                                options.fontSizeDip,
                                                options.locale.c_str(),
                                                format.put()));
        THROW_IF_FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP));
        THROW_IF_FAILED(format->SetReadingDirection(direction));
        THROW_IF_FAILED(format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING));
        wil::com_ptr<IDWriteTextLayout> layout;
        THROW_IF_FAILED(write->CreateTextLayout(
            graph->nodes[0u].label.c_str(), static_cast<UINT32>(graph->nodes[0u].label.size()), format.get(), 100.f, 65536.f, layout.put()));
        DWRITE_TEXT_METRICS metrics{};
        DWRITE_OVERHANG_METRICS ink{};
        THROW_IF_FAILED(layout->GetMetrics(&metrics));
        THROW_IF_FAILED(layout->GetOverhangMetrics(&ink));
        const auto expectedWidth =
            std::max(metrics.left + metrics.widthIncludingTrailingWhitespace, metrics.layoutWidth + ink.right) - std::min(metrics.left, -ink.left);
        const auto expectedHeight = std::max(metrics.top + metrics.height, metrics.layoutHeight + ink.bottom) - std::min(metrics.top, -ink.top);
        check(FlowchartLayoutTests::Near(first->nodes[0u].label.width, expectedWidth) &&
                  FlowchartLayoutTests::Near(first->nodes[0u].label.height, expectedHeight) && expectedHeight > 35.f,
              "configured Unicode/bidi label wrapping matches independent DirectWrite width and ink metrics");
        const auto generation = prepared.Generation();
        check(prepared.Begin(write.get(), drawing.get(), graph, options, {1u, 1u}) == LayoutPreparationStatus::Complete && prepared.Generation() == generation,
              "unchanged explicit label width reuses its prepared geometry");
        options.diagramLabelWidthDip = 300.f;
        check(prepared.Begin(write.get(), drawing.get(), graph, options, {1u, 1u}) == LayoutPreparationStatus::Pending,
              "changing explicit label width retires prepared diagram geometry");
        Complete(prepared, check);
        check(prepared.Generation() > generation && prepared.Geometry()->nodes[0u].label.height < first->nodes[0u].label.height,
              "wider label width publishes independently rewrapped geometry");
        for (const auto invalid : {-1.f, .5f, 4097.f, std::numeric_limits<float>::quiet_NaN()})
        {
            options.diagramLabelWidthDip = invalid;
            bool rejected{};
            rejected = prepared.Begin(write.get(), drawing.get(), graph, options, {1u, 1u}) == LayoutPreparationStatus::Failed;
            check(rejected && prepared.Ready(), "invalid label width leaves prior prepared geometry intact");
        }
    }
}
template <typename Check, typename Observe = std::nullptr_t> inline void PaletteRaster(Check&& check, Observe observe = nullptr)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto graph     = FlowchartLayoutTests::Graph(
        L"flowchart TD\nA[Alpha]-->B[Beta]\nstyle A fill:#000,stroke:#ff0,color:#fff\nstyle B fill:#fff,color:#fff\nlinkStyle 0 stroke:#f00\n");
    check(graph->outcome == FlowchartOutcome::Prepared && graph->nodes.size() == 2u && graph->edges.size() == 1u && graph->nodes[0u].fillColor == 0u &&
              graph->nodes[0u].strokeColor == 0xffff00u && graph->nodes[0u].textColor == 0xffffffu && graph->edges[0u].strokeColor == 0xff0000u,
          "authored RGB colors have independently expected values");
    if (graph->outcome != FlowchartOutcome::Prepared || graph->nodes.size() != 2u || graph->edges.size() != 1u)
        return;
    const auto write   = WriteFactory();
    const auto drawing = DrawingFactory();
    PreparedFlowchart prepared;
    prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
    Complete(prepared, check);
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    constexpr UINT width = 400u, height = 300u, stride = width * 4u;
    wil::com_ptr<IWICBitmap> bitmap, otherBitmap;
    THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
    THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, otherBitmap.put()));
    wil::com_ptr<ID2D1RenderTarget> target, other;
    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), D2D1::RenderTargetProperties(), target.put()));
    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(otherBitmap.get(), D2D1::RenderTargetProperties(), other.put()));
    wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), text.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
    DiagramPalette noGraph({}, target.get());
    check(noGraph.Result() == E_INVALIDARG && ! noGraph.Advance() && ! noGraph.Ready(),
          "null graph refuses palette advancement without dereferencing its owner");
    DiagramPalette noTarget(graph, nullptr);
    check(noTarget.Result() == E_INVALIDARG && ! noTarget.Advance() && ! noTarget.Ready(),
          "missing target refuses a colored palette without dereferencing its resources");
    auto emptyGraph = std::make_shared<FlowchartModel>(L"", 0u);
    DiagramPalette emptyNoTarget(emptyGraph, nullptr);
    check(emptyNoTarget.Result() == E_INVALIDARG && ! emptyNoTarget.Advance() && ! emptyNoTarget.Ready(), "an invalid empty palette never publishes ready");
    DiagramPalette palette(graph, target.get());
    std::size_t steps{};
    while (! palette.Advance())
        check(++steps < 16u, "palette stages at most one target brush per bounded step");
    check(palette.Ready() && palette.Matches(graph.get(), target.get()) && ! palette.Matches(graph.get(), other.get()),
          "palette is bound to the exact graph and raster target");
    check(steps == 6u && palette.texts[0].get() == palette.fills[1].get() && palette.fills[1].get() == palette.texts[1].get(),
          "repeated diagram colors share one SDK brush while every authored slot consumes a staging step");
#if DXUI_ENABLE_DIAGNOSTICS
    DiagramPalette pending(graph, target.get());
    const auto oldFailure              = testNativeColorPaletteBrushFailure;
    const auto restoreFailure          = wil::scope_exit([&]() noexcept { testNativeColorPaletteBrushFailure = oldFailure; });
    testNativeColorPaletteBrushFailure = E_OUTOFMEMORY;
    bool rejected{};
    rejected                           = ! pending.Advance() && pending.Result() == E_OUTOFMEMORY;
    testNativeColorPaletteBrushFailure = S_OK;
    check(rejected && ! pending.Ready() && pending.cursor == 0u && palette.Ready(),
          "diagram color creation failure retains its pending slot and prior publication");
    while (! pending.Advance())
    {
    }
    check(pending.Ready() && pending.texts[0].get() == pending.texts[1].get(), "diagram palette retries and still shares duplicate resources");
#endif
    const DiagramColorContext context{D2D1::ColorF(D2D1::ColorF::Black), D2D1::ColorF(D2D1::ColorF::Blue), D2D1::ColorF(D2D1::ColorF::White)};
    const auto raster = [&](const DiagramColorContext* colors, bool firstBlack, bool darkTheme)
    {
        if (darkTheme)
        {
            text->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            fill->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
        }
        target->BeginDraw();
        target->Clear(D2D1::ColorF(D2D1::ColorF::White));
        if constexpr (! std::is_same_v<Observe, std::nullptr_t>)
            observe(true);
        const auto drawn = prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}, &palette, colors);
        if constexpr (! std::is_same_v<Observe, std::nullptr_t>)
            observe(false);
        check(drawn, "palette draws on its owning target");
        THROW_IF_FAILED(target->EndDraw());
        std::vector<BYTE> pixels(stride * height);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
        const auto pixel = [&](std::size_t index)
        {
            const auto bounds = prepared.Geometry()->nodes[index].bounds;
            const auto x = static_cast<std::size_t>(bounds.x + 5.0), y = static_cast<std::size_t>(bounds.y + bounds.height / 2.0);
            const auto offset = y * stride + x * 4u;
            return std::array<BYTE, 3u>{pixels[offset], pixels[offset + 1u], pixels[offset + 2u]};
        };
        const auto first = pixel(0u), second = pixel(1u);
        check((firstBlack ? first[0u] < 40u && first[1u] < 40u && first[2u] < 40u : first[0u] > 220u && first[1u] > 220u && first[2u] > 220u) &&
                  (darkTheme ? second[0u] < 40u && second[1u] < 40u && second[2u] < 40u : second[0u] > 220u && second[1u] > 220u && second[2u] > 220u),
              "safe authored fill paints; low-contrast pair and forced theme fall back to the borrowed surface");
        if (colors && colors->fill.a == 1.f && ! darkTheme)
        {
            const auto bounds = prepared.Geometry()->nodes[0u].bounds;
            const auto x = static_cast<std::size_t>(bounds.x + bounds.width / 2.0), y = static_cast<std::size_t>(bounds.y);
            const auto border = y * stride + x * 4u;
            check(pixels[border] < 180u && pixels[border + 1u] > 80u && pixels[border + 2u] > 80u,
                  "contrast-safe authored yellow outline reaches actual pixels");
            const auto& edge = prepared.Geometry()->edges[0u].path;
            std::size_t redInk{};
            for (std::size_t part = 1u; part < edge.size(); ++part)
                if (FlowchartLayoutTests::Near(edge[part - 1u].x, edge[part].x))
                {
                    const auto edgeX  = static_cast<std::size_t>(edge[part].x);
                    const auto firstY = static_cast<std::size_t>(std::ceil(std::min(edge[part - 1u].y, edge[part].y) + 2.0));
                    const auto lastY  = static_cast<std::size_t>(std::floor(std::max(edge[part - 1u].y, edge[part].y) - 2.0));
                    for (auto edgeY = firstY; edgeY < lastY && edgeY < height && edgeX < width; ++edgeY)
                    {
                        const auto route = edgeY * stride + edgeX * 4u;
                        redInk += pixels[route] < 180u && pixels[route + 1u] < 180u && pixels[route + 2u] > 200u;
                    }
                }
            check(redInk > 0u, "contrast-safe authored red link reaches actual pixels");
        }
        if (darkTheme)
        {
            text->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
            fill->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        }
    };
    raster(&context, true, false);
    raster(nullptr, false, false);
    auto translucent   = context;
    translucent.fill.a = .5f;
    raster(&translucent, false, false);
    const DiagramColorContext dark{D2D1::ColorF(D2D1::ColorF::White), D2D1::ColorF(D2D1::ColorF::White), D2D1::ColorF(D2D1::ColorF::Black)};
    raster(&dark, true, true);
    for (const float dpi : {120.f, 144.f, 168.f})
    {
        target->SetDpi(dpi, dpi);
        const auto bounds = prepared.Geometry()->nodes[0u].bounds;
        const float scale = dpi / 96.f;
        const auto x      = static_cast<std::size_t>((bounds.x + 5.0) * scale);
        const auto y      = static_cast<std::size_t>((bounds.y + bounds.height / 2.0) * scale);
        check(x < width && y < height, "scaled palette sample stays inside the target");
        if (x >= width || y >= height)
            continue;
        const auto sample = [&](const DiagramColorContext* selected)
        {
            target->BeginDraw();
            target->Clear(D2D1::ColorF(D2D1::ColorF::White));
            check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}, &palette, selected), "scaled diagram palette draws");
            THROW_IF_FAILED(target->EndDraw());
            std::vector<BYTE> pixels(stride * height);
            THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
            const auto offset = y * stride + x * 4u;
            return std::array<BYTE, 3u>{pixels[offset], pixels[offset + 1u], pixels[offset + 2u]};
        };
        const auto authored = sample(&context), highContrast = sample(nullptr), unknown = sample(&translucent);
        check(authored[0] < 40u && authored[1] < 40u && authored[2] < 40u && highContrast[0] > 220u && highContrast[1] > 220u && highContrast[2] > 220u &&
                  unknown[0] > 220u && unknown[1] > 220u && unknown[2] > 220u,
              "diagram palette preserves authored and theme-fallback pixels at each DPI");
    }
    target->SetDpi(96.f, 96.f);
    const auto badOutline = FlowchartLayoutTests::Graph(L"flowchart TD\nA[Alpha]\nstyle A fill:#000,color:#fff\n");
    PreparedFlowchart unsafePrepared;
    unsafePrepared.Begin(write.get(), drawing.get(), badOutline, {}, {1u, 1u});
    Complete(unsafePrepared, check);
    DiagramPalette unsafePalette(badOutline, target.get());
    while (! unsafePalette.Advance())
    {
    }
    target->BeginDraw();
    target->Clear(D2D1::ColorF(D2D1::ColorF::White));
    check(unsafePrepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}, &unsafePalette, &context),
          "the unsafe themed outline case paints with fallback");
    THROW_IF_FAILED(target->EndDraw());
    std::vector<BYTE> unsafePixels(stride * height);
    THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(unsafePixels.size()), unsafePixels.data()));
    const auto unsafeBounds = unsafePrepared.Geometry()->nodes[0u].bounds;
    const auto unsafeOffset =
        static_cast<std::size_t>(unsafeBounds.y + unsafeBounds.height / 2.0) * stride + static_cast<std::size_t>(unsafeBounds.x + 5.0) * 4u;
    check(unsafePixels[unsafeOffset] > 220u && unsafePixels[unsafeOffset + 1u] > 220u && unsafePixels[unsafeOffset + 2u] > 220u,
          "a dark authored fill falls back when the unchanged blue theme outline would be illegible");
    check(! prepared.Draw(other.get(), text.get(), stroke.get(), fill.get(), {}, &palette, &context), "a palette cannot paint onto a different target");
    check(text->GetOpacity() == 1.f && stroke->GetOpacity() == 1.f && fill->GetOpacity() == 1.f && stroke->GetColor().b == 1.f,
          "authored colors leave borrowed brush attributes unchanged");
}
template <typename Check> inline void ThemeStyleRaster(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto graph =
        FlowchartLayoutTests::Graph(L"flowchart TD\nclassDef default font-weight:bold,font-style:italic\nclassDef heavy stroke-width:2px,stroke-dasharray:5 5\n"
                                    L"A[Alpha]:::heavy --> B[Beta]\nlinkStyle 0 stroke-width:2px,stroke-dasharray:5 5\n");
    check(graph->outcome == FlowchartOutcome::Prepared && graph->nodes.size() == 2u && graph->edges.size() == 1u && graph->nodes[0u].strokeWidth == 2u &&
              graph->nodes[0u].borderDashed && graph->nodes[1u].strokeWidth == 1u && graph->nodes[0u].styles.count == 1u &&
              graph->nodes[0u].styles.spans[0u].flags == (LabelBold | LabelItalic) && graph->nodes[1u].styles.count == 1u &&
              graph->edges[0u].strokeWidth == 2u && graph->edges[0u].dashed,
          "default and named classes merge bounded font and outline properties; link style targets one edge");
    if (graph->outcome != FlowchartOutcome::Prepared || graph->nodes.size() != 2u || graph->edges.size() != 1u)
        return;
    const auto write   = WriteFactory();
    const auto drawing = DrawingFactory();
    PreparedFlowchart prepared;
    prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
    Complete(prepared, check);
    wil::com_ptr<IDWriteTextFormat> expectedFormat;
    THROW_IF_FAILED(write->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 18.f, L"en-us", expectedFormat.put()));
    wil::com_ptr<IDWriteTextLayout> expectedLabel;
    THROW_IF_FAILED(write->CreateTextLayout(L"Alpha", 5u, expectedFormat.get(), 225.f, 65536.f, expectedLabel.put()));
    THROW_IF_FAILED(expectedLabel->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, {0u, 5u}));
    THROW_IF_FAILED(expectedLabel->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, {0u, 5u}));
    DWRITE_TEXT_METRICS metrics{};
    DWRITE_OVERHANG_METRICS overhang{};
    THROW_IF_FAILED(expectedLabel->GetMetrics(&metrics));
    THROW_IF_FAILED(expectedLabel->GetOverhangMetrics(&overhang));
    const auto expectedWidth =
        std::max(metrics.left + metrics.widthIncludingTrailingWhitespace, metrics.layoutWidth + overhang.right) - std::min(metrics.left, -overhang.left);
    check(FlowchartLayoutTests::Near(prepared.Geometry()->nodes[0u].label.width, expectedWidth),
          "default bold italic class uses independently measured DirectWrite metrics");
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    constexpr UINT width = 600u, height = 400u, stride = width * 4u;
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
    wil::com_ptr<ID2D1RenderTarget> target;
    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), D2D1::RenderTargetProperties(), target.put()));
    wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), text.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
    target->BeginDraw();
    target->Clear(D2D1::ColorF(D2D1::ColorF::White));
    check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}), "theme styles paint using the caller's brushes");
    THROW_IF_FAILED(target->EndDraw());
    std::vector<BYTE> pixels(stride * height);
    THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
    const auto bounds = prepared.Geometry()->nodes[0u].bounds;
    std::size_t ink{}, gaps{};
    const auto y = static_cast<UINT>(bounds.y);
    for (UINT x = static_cast<UINT>(std::ceil(bounds.x + 4.0)); x < static_cast<UINT>(std::floor(bounds.x + bounds.width - 4.0)); ++x)
    {
        const auto offset = static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) * 4u;
        if (pixels[offset] > 150u && pixels[offset + 1u] < 180u && pixels[offset + 2u] < 180u)
            ++ink;
        else
            ++gaps;
    }
    const auto strokeColor = stroke->GetColor(), textColor = text->GetColor(), fillColor = fill->GetColor();
    check(ink > 0u && gaps > 0u && stroke->GetOpacity() == 1.f && text->GetOpacity() == 1.f && fill->GetOpacity() == 1.f && strokeColor.b == 1.f &&
              strokeColor.r == 0.f && strokeColor.g == 0.f && textColor.r == 0.f && textColor.g == 0.f && textColor.b == 0.f && fillColor.r == 1.f &&
              fillColor.g == 1.f && fillColor.b == 1.f,
          "node outline alternates dash and gap while preserving borrowed colors and opacity");
    const auto& path = prepared.Geometry()->edges[0u].path;
    std::size_t segment{};
    double longest{};
    for (std::size_t i = 1u; i < path.size(); ++i)
        if (FlowchartLayoutTests::Near(path[i - 1u].x, path[i].x) && std::abs(path[i].y - path[i - 1u].y) > longest)
        {
            segment = i;
            longest = std::abs(path[i].y - path[i - 1u].y);
        }
    std::size_t edgeInk{}, edgeGaps{};
    if (segment && longest > 12.0)
    {
        const auto x     = static_cast<UINT>(path[segment].x);
        const auto first = static_cast<UINT>(std::ceil(std::min(path[segment - 1u].y, path[segment].y) + 3.0));
        const auto last  = static_cast<UINT>(std::floor(std::max(path[segment - 1u].y, path[segment].y) - 3.0));
        for (auto row = first; row < last && row < height && x < width; ++row)
        {
            const auto offset = static_cast<std::size_t>(row) * stride + static_cast<std::size_t>(x) * 4u;
            if (pixels[offset] > 150u && pixels[offset + 1u] < 180u && pixels[offset + 2u] < 180u)
                ++edgeInk;
            else
                ++edgeGaps;
        }
    }
    check(edgeInk > 0u && edgeGaps > 0u, "link style visibly alternates dash and gap along its routed edge");
    const auto invalid = FlowchartLayoutTests::Graph(L"flowchart TD\nA-->B\nlinkStyle 1 stroke-width:2px\n");
    check(invalid->outcome == FlowchartOutcome::Unsupported && invalid->nodes.empty(), "out-of-range link styling retains source");
    const auto classes =
        FlowchartLayoutTests::Graph(L"flowchart TD\nclassDef one,two stroke-width:3px\nclassDef normal font-weight:normal\n"
                                    L"A:::one,two --> B\nclass A,B normal\nstyle A stroke-width:2px\nlinkStyle default stroke:#000,stroke-dasharray:5 5\n"
                                    L"linkStyle 0 stroke-dasharray:0\n");
    check(classes->outcome == FlowchartOutcome::Prepared && classes->nodes.size() == 2u && classes->edges.size() == 1u &&
              classes->nodes[0u].strokeWidth == 2u && classes->nodes[1u].strokeWidth == 1u && classes->edges[0u].strokeColor == 0u &&
              classes->edges[0u].dashSpecified && ! classes->edges[0u].dashed,
          "multiple named classes, node lists, explicit overrides and default link properties compose in source order");
}
template <typename Check> inline void StyledLabelRaster(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto graph     = FlowchartLayoutTests::Graph(L"flowchart TD\nA[\"<b>Bold</b> <i>It</i> <u>U</u> <s>S</s>\"]-->|<b>go</b>|B\n");
    check(graph->outcome == FlowchartOutcome::Prepared && graph->nodes.size() == 2u && graph->edges.size() == 1u && graph->nodes[0u].label == L"Bold It U S" &&
              graph->nodes[0u].styles.count == 4u && graph->edges[0u].label == L"go" && graph->edges[0u].styles.count == 1u,
          "styled node and edge labels expose their independent text and style ranges");
    if (graph->outcome != FlowchartOutcome::Prepared || graph->nodes.size() != 2u || graph->edges.size() != 1u)
        return;
    const auto write   = WriteFactory();
    const auto drawing = DrawingFactory();
    PreparedFlowchart prepared;
    prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
    Complete(prepared, check);
    const auto geometry = prepared.Geometry();
    const auto width = static_cast<UINT>(std::ceil(geometry->extent.width)), height = static_cast<UINT>(std::ceil(geometry->extent.height)),
               stride = width * 4u;
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    const auto pixels = [&](bool reference)
    {
        wil::com_ptr<IWICBitmap> bitmap;
        THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
        wil::com_ptr<ID2D1RenderTarget> target;
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), D2D1::RenderTargetProperties(), target.put()));
        target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), text.put()));
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()));
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
        target->BeginDraw();
        target->Clear(D2D1::ColorF(D2D1::ColorF::White));
        check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}), "styled labels paint on the owner target");
        if (reference)
        {
            LayoutOptions options;
            wil::com_ptr<IDWriteTextFormat> format;
            THROW_IF_FAILED(write->CreateTextFormat(options.fontFamily.c_str(),
                                                    nullptr,
                                                    DWRITE_FONT_WEIGHT_NORMAL,
                                                    DWRITE_FONT_STYLE_NORMAL,
                                                    DWRITE_FONT_STRETCH_NORMAL,
                                                    options.fontSizeDip,
                                                    options.locale.c_str(),
                                                    format.put()));
            const auto draw = [&](std::wstring_view value, FlowchartRect box, bool node)
            {
                wil::com_ptr<IDWriteTextLayout> layout;
                THROW_IF_FAILED(write->CreateTextLayout(value.data(), static_cast<UINT32>(value.size()), format.get(), 225.f, 65536.f, layout.put()));
                THROW_IF_FAILED(layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, {0u, node ? 4u : 2u}));
                if (node)
                {
                    THROW_IF_FAILED(layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, {5u, 2u}));
                    THROW_IF_FAILED(layout->SetUnderline(TRUE, {8u, 1u}));
                    THROW_IF_FAILED(layout->SetStrikethrough(TRUE, {10u, 1u}));
                }
                DWRITE_TEXT_METRICS metrics{};
                DWRITE_OVERHANG_METRICS ink{};
                THROW_IF_FAILED(layout->GetMetrics(&metrics));
                THROW_IF_FAILED(layout->GetOverhangMetrics(&ink));
                const auto left = std::min(metrics.left, -ink.left), top = std::min(metrics.top, -ink.top);
                target->FillRectangle(D2D1::RectF(static_cast<float>(box.x) - 2.f,
                                                  static_cast<float>(box.y) - 2.f,
                                                  static_cast<float>(box.x + box.width) + 2.f,
                                                  static_cast<float>(box.y + box.height) + 2.f),
                                      fill.get());
                target->DrawTextLayout(D2D1::Point2F(static_cast<float>(box.x) - left, static_cast<float>(box.y) - top),
                                       layout.get(),
                                       text.get(),
                                       D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
            };
            draw(L"Bold It U S", geometry->nodes[0u].label, true);
            draw(L"go", geometry->edges[0u].label, false);
        }
        THROW_IF_FAILED(target->EndDraw());
        std::vector<BYTE> result(static_cast<std::size_t>(stride) * height);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(result.size()), result.data()));
        return result;
    };
    const auto actual = pixels(false), reference = pixels(true);
    std::size_t compared{}, different{};
    for (const auto box : {geometry->nodes[0u].label, geometry->edges[0u].label})
        for (auto y = static_cast<UINT>(std::ceil(box.y)); y < static_cast<UINT>(std::floor(box.y + box.height)); ++y)
            for (auto x = static_cast<UINT>(std::ceil(box.x)); x < static_cast<UINT>(std::floor(box.x + box.width)); ++x)
            {
                const auto offset = (static_cast<std::size_t>(y) * width + x) * 4u;
                ++compared;
                if (! std::equal(actual.data() + offset, actual.data() + offset + 4u, reference.data() + offset))
                    ++different;
            }
    check(compared > 0u && different == 0u, "styled node and edge pixels match independently styled DirectWrite layouts");
    auto invalid                             = std::make_shared<FlowchartModel>(*graph);
    invalid->nodes[0u].styles.spans[0u].last = invalid->nodes[0u].label.size() + 1u;
    const std::vector<FlowchartSize> nodes(invalid->nodes.size(), {16.0, 12.0}), edges(invalid->edges.size(), {16.0, 12.0});
    bool layoutRejected{}, paintRejected{};
    {
        NativeFlowchartLayout layout;
        layoutRejected = layout.Begin(invalid, nodes, edges) == SourceResult::InvalidArgument;
    }
    paintRejected = prepared.Begin(write.get(), drawing.get(), invalid, {}, {1u, 1u}) == LayoutPreparationStatus::Failed;
    check(layoutRejected && paintRejected && prepared.Ready(), "forged out-of-range label styles fail before replacing publication");
    const auto inlineEdge = FlowchartLayoutTests::Graph(L"flowchart TD\nA -- <b>go</b> --> B\n");
    check(inlineEdge->outcome == FlowchartOutcome::Prepared && inlineEdge->edges.size() == 1u && inlineEdge->edges[0u].label == L"go" &&
              inlineEdge->edges[0u].styles.count == 1u && inlineEdge->edges[0u].styles.spans[0u].flags == LabelBold,
          "inline edge text retains the same bounded markup as pipe labels");
    for (const auto source :
         {L"flowchart TD\nA[\"<b>unclosed\"]\n", L"flowchart TD\nA[\"<span class=x>bad</span>\"]\n", L"flowchart TD\nA[\"<b><i>bad</b></i>\"]\n"})
    {
        const auto rejected = FlowchartLayoutTests::Graph(source);
        check(rejected->outcome == FlowchartOutcome::Unsupported && rejected->source == source && rejected->nodes.empty(),
              "unsupported or unbalanced label markup preserves exact source");
    }
}
template <typename Check> inline void LegacyPolygonRaster(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    struct Case
    {
        std::wstring_view source;
        FlowchartShape shape;
        bool topLeft, bottomLeft, topRight;
    };
    constexpr std::array cases{Case{L"flowchart TD\nA>flag]\n", FlowchartShape::Asymmetric, true, true, false},
                               Case{L"flowchart TD\nA[/forward/]\n", FlowchartShape::Parallelogram, false, true, true},
                               Case{L"flowchart TD\nA[\\reverse\\]\n", FlowchartShape::ReverseParallelogram, true, false, false},
                               Case{L"flowchart TD\nA[/up\\]\n", FlowchartShape::Trapezoid, false, true, false},
                               Case{L"flowchart TD\nA[\\down/]\n", FlowchartShape::ReverseTrapezoid, true, false, true},
                               Case{L"flowchart TD\nA(-oval-)\n", FlowchartShape::Ellipse, false, false, false}};
    const auto write   = WriteFactory();
    const auto drawing = DrawingFactory();
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    constexpr UINT width = 256u, height = 256u, stride = width * 4u;
    for (const auto& item : cases)
    {
        const auto graph = FlowchartLayoutTests::Graph(item.source);
        check(graph->outcome == FlowchartOutcome::Prepared && graph->nodes.size() == 1u && graph->nodes[0u].shape == item.shape,
              "legacy shape parser retains the independently specified shape");
        if (graph->outcome != FlowchartOutcome::Prepared || graph->nodes.size() != 1u)
            return;
        PreparedFlowchart prepared;
        prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
        Complete(prepared, check);
        wil::com_ptr<IWICBitmap> bitmap;
        THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
        wil::com_ptr<ID2D1RenderTarget> target;
        const auto properties =
            D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
        wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), text.put()));
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()));
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), fill.put()));
        target->BeginDraw();
        target->Clear(D2D1::ColorF(D2D1::ColorF::White));
        check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}), "legacy shape paints on the borrowed target");
        THROW_IF_FAILED(target->EndDraw());
        std::vector<BYTE> pixels(stride * height);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
        const auto rect = prepared.Geometry()->nodes[0u].bounds;
        const auto red  = [&](double x, double y)
        {
            const auto px = static_cast<std::size_t>(rect.x + x * rect.width), py = static_cast<std::size_t>(rect.y + y * rect.height);
            if (px >= width || py >= height)
                return false;
            const auto offset = py * stride + px * 4u;
            return pixels[offset] < 100u && pixels[offset + 1u] < 100u && pixels[offset + 2u] > 150u;
        };
        check(red(.1, .1) == item.topLeft && red(.1, .9) == item.bottomLeft && red(.9, .1) == item.topRight,
              "legacy shape has its independently specified painted silhouette");
    }
}
template <typename Check> inline void ExpandedShapeRaster(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto graph     = FlowchartLayoutTests::Graph(L"flowchart LR\nA(((Double)))\nB[(Database)]\nA --> B\n");
    check(graph->outcome == FlowchartOutcome::Prepared && graph->nodes.size() == 2u && graph->nodes[0u].shape == FlowchartShape::DoubleCircle &&
              graph->nodes[1u].shape == FlowchartShape::Cylinder && graph->nodes[0u].label == L"Double" && graph->nodes[1u].label == L"Database",
          "double circle and cylinder preserve independent authored shapes and labels");
    if (graph->outcome != FlowchartOutcome::Prepared || graph->nodes.size() != 2u)
        return;
    const auto write   = WriteFactory();
    const auto drawing = DrawingFactory();
    PreparedFlowchart prepared;
    prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
    Complete(prepared, check);
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    constexpr UINT width = 600u, height = 300u, stride = width * 4u;
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
    wil::com_ptr<ID2D1RenderTarget> target;
    const auto properties =
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
    wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), text.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
    target->BeginDraw();
    target->Clear(D2D1::ColorF(D2D1::ColorF::White));
    check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}), "expanded shapes paint using the owner target");
    THROW_IF_FAILED(target->EndDraw());
    std::vector<BYTE> pixels(stride * height);
    THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
    const auto blueNear = [&](double px, double py)
    {
        const auto x = static_cast<int>(px), y = static_cast<int>(py);
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (x + dx >= 0 && y + dy >= 0 && x + dx < static_cast<int>(width) && y + dy < static_cast<int>(height))
                {
                    const auto offset = static_cast<std::size_t>(y + dy) * stride + static_cast<std::size_t>(x + dx) * 4u;
                    if (pixels[offset] > 150u && pixels[offset + 1u] < 220u && pixels[offset + 2u] < 220u)
                        return true;
                }
        return false;
    };
    const auto circle   = prepared.Geometry()->nodes[0u].bounds;
    const auto cylinder = prepared.Geometry()->nodes[1u].bounds;
    check(blueNear(circle.x + circle.width / 2.0, circle.y) && blueNear(circle.x + circle.width / 2.0, circle.y + 4.0),
          "double circle paints both independently separated outlines");
    check(blueNear(cylinder.x + cylinder.width / 2.0, cylinder.y) && blueNear(cylinder.x + cylinder.width / 2.0, cylinder.y + 16.0) &&
              blueNear(cylinder.x + cylinder.width / 2.0, cylinder.y + cylinder.height),
          "cylinder paints its top rim and curved bottom");
    for (const auto source : {L"flowchart TD\nA(((broken))\n", L"flowchart TD\nB[(broken)\n"})
    {
        const auto malformed = FlowchartLayoutTests::Graph(source);
        check(malformed->outcome != FlowchartOutcome::Prepared && malformed->source == source && malformed->nodes.empty(),
              "incomplete expanded shapes retain exact source without partial publication");
    }
}
template <typename Check> inline void NodeStrokeRaster(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto graph     = FlowchartLayoutTests::Graph(L"flowchart TD\nA[Alpha]:::strong\nclassDef strong stroke-width:3px\n");
    check(graph->outcome == FlowchartOutcome::Prepared && graph->nodes.size() == 1u && graph->nodes[0u].strokeWidth == 3u,
          "forward class declaration applies bounded node stroke width to its intended node");
    if (graph->outcome != FlowchartOutcome::Prepared || graph->nodes.size() != 1u)
        return;
    const auto write   = WriteFactory();
    const auto drawing = DrawingFactory();
    PreparedFlowchart prepared;
    prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
    Complete(prepared, check);
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    constexpr UINT width = 256u, height = 256u, stride = width * 4u;
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
    wil::com_ptr<ID2D1RenderTarget> target;
    const auto properties =
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
    wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), text.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
    target->BeginDraw();
    target->Clear(D2D1::ColorF(D2D1::ColorF::White));
    check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}), "styled node paints with the caller's theme brushes");
    THROW_IF_FAILED(target->EndDraw());
    std::vector<BYTE> pixels(stride * height);
    THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
    const auto box = prepared.Geometry()->nodes[0u].bounds;
    const auto x   = static_cast<UINT>(box.x + box.width / 2.0);
    const auto y   = static_cast<UINT>(box.y);
    std::size_t ink{};
    for (int dy = -2; dy <= 2; ++dy)
    {
        const auto row    = static_cast<UINT>(static_cast<int>(y) + dy);
        const auto offset = static_cast<std::size_t>(row) * stride + static_cast<std::size_t>(x) * 4u;
        ink += pixels[offset] > 150u && pixels[offset + 1u] < 180u && pixels[offset + 2u] < 180u;
    }
    check(ink >= 3u && stroke->GetOpacity() == 1.0f, "three-pixel styled border paints visibly and preserves caller brush state");
    auto invalid                   = std::make_shared<FlowchartModel>(*graph);
    invalid->nodes[0u].strokeWidth = 4u;
    bool layoutRejected{}, paintRejected{};
    const std::vector<FlowchartSize> nodeSizes(1u, {16.0, 12.0});
    {
        NativeFlowchartLayout layout;
        layoutRejected = layout.Begin(invalid, nodeSizes, {}) == SourceResult::InvalidArgument;
    }
    paintRejected = prepared.Begin(write.get(), drawing.get(), invalid, {}, {1u, 1u}) == LayoutPreparationStatus::Failed;
    check(layoutRejected && paintRejected && prepared.Ready(), "invalid direct stroke widths fail both services before publication");
    for (const auto source : {L"flowchart TD\nA\nstyle A fill:red\n",
                              L"flowchart TD\nA\nstyle A stroke-width:4px\n",
                              L"flowchart TD\nstyle missing stroke-width:2px\n",
                              L"flowchart TD\nA:::missing\n",
                              L"flowchart TD\nA\nstyle A fill:rgba(0,0,0,0)\n"})
    {
        const auto unsupported = FlowchartLayoutTests::Graph(source);
        check(unsupported->outcome == FlowchartOutcome::Unsupported && unsupported->source == source && unsupported->nodes.empty(),
              "unknown style properties, widths and targets preserve source without a partial model");
    }
    const auto named = FlowchartLayoutTests::Graph(L"flowchart TD\nA\nclass A strong\nclassDef strong stroke-width:2px\n");
    check(named->outcome == FlowchartOutcome::Prepared && named->nodes.size() == 1u && named->nodes[0u].strokeWidth == 2u,
          "named class statement resolves its later definition");
    const auto direct = FlowchartLayoutTests::Graph(L"flowchart TD\nA\nstyle A stroke-width:2px\n");
    check(direct->outcome == FlowchartOutcome::Prepared && direct->nodes.size() == 1u && direct->nodes[0u].strokeWidth == 2u,
          "direct style applies the same bounded width");
    const auto overrideStyle = FlowchartLayoutTests::Graph(L"flowchart TD\nA:::strong\nstyle A stroke-width:2px\nclassDef strong stroke-width:3px\n");
    check(overrideStyle->outcome == FlowchartOutcome::Prepared && overrideStyle->nodes[0u].strokeWidth == 2u,
          "explicit node style overrides the named class regardless of definition order");
}
template <typename Check> inline void MindmapRaster(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto graph     = FlowchartLayoutTests::Graph(L"mindmap\n  root((Central))\n    First\n      Leaf\n    Second\n");
    const auto write     = WriteFactory();
    const auto drawing   = DrawingFactory();
    PreparedFlowchart prepared;
    prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
    Complete(prepared, check);
    const auto geometry = prepared.Geometry();
    check(prepared.Ready() && geometry && geometry->nodes.size() == 4u && geometry->edges.size() == 3u,
          "mindmap root and branching hierarchy prepare native geometry");
    if (! prepared.Ready() || ! geometry || geometry->nodes.size() != 4u || geometry->edges.size() != 3u)
        return;
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    constexpr UINT width = 800u, height = 400u, stride = width * 4u;
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
    wil::com_ptr<ID2D1RenderTarget> target;
    const auto properties =
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
    wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), text.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
    target->BeginDraw();
    target->Clear(D2D1::ColorF(D2D1::ColorF::White));
    check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}), "mindmap paints on its owner target");
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
                    if (pixels[offset] > 150u && pixels[offset + 1u] < 220u && pixels[offset + 2u] < 220u)
                        return true;
                }
        return false;
    };
    for (const auto& edge : geometry->edges)
        check(blueNear((edge.path[1u].x + edge.path[2u].x) / 2.0, edge.path[1u].y), "each mindmap hierarchy connector has visible ink between node columns");
    const std::vector<FlowchartSize> nodes(graph->nodes.size(), {16.0, 12.0}), edges(graph->edges.size());
    for (unsigned variant = 0u; variant < 5u; ++variant)
    {
        auto invalid = std::make_shared<FlowchartModel>(*graph);
        switch (variant)
        {
            case 0u: invalid->direction = FlowchartDirection::LeftRight; break;
            case 1u: invalid->nodes[2u].mindParent = 2u; break;
            case 2u: invalid->nodes[2u].mindDepth = 8u; break;
            case 3u: invalid->edges[0u].arrow = true; break;
            case 4u: invalid->nodes[0u].shape = FlowchartShape::Bar; break;
        }
        bool layoutRejected{}, paintRejected{};
        {
            NativeFlowchartLayout layout;
            layoutRejected = layout.Begin(invalid, nodes, edges) == SourceResult::InvalidArgument;
        }
        paintRejected = prepared.Begin(write.get(), drawing.get(), invalid, {}, {1u, 1u}) == LayoutPreparationStatus::Failed;
        check(! ValidMindmapModel(*invalid) && layoutRejected && paintRejected && prepared.Ready() && prepared.Geometry() == geometry,
              "direct invalid mindmap models fail both services without replacing prior publication");
    }
}
template <typename Check> inline void GanttRaster(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto graph     = FlowchartLayoutTests::Graph(L"gantt\ntitle Release\nsection Build\nCompile : 2024-02-27, 3d\nShip : 2024-03-01, 2024-03-03\n");
    const auto write     = WriteFactory();
    const auto drawing   = DrawingFactory();
    PreparedFlowchart prepared;
    prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
    Complete(prepared, check);
    const auto geometry = prepared.Geometry();
    check(prepared.Ready() && geometry && geometry->nodes.size() == 2u, "Gantt tasks prepare measured native timeline bars");
    if (! prepared.Ready() || ! geometry || geometry->nodes.size() != 2u)
        return;
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    constexpr UINT width = 800u, height = 400u, stride = width * 4u;
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
    wil::com_ptr<ID2D1RenderTarget> target;
    const auto properties =
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
    wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), text.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
    target->BeginDraw();
    target->Clear(D2D1::ColorF(D2D1::ColorF::White));
    check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}), "Gantt timeline paints on its owner target");
    THROW_IF_FAILED(target->EndDraw());
    std::vector<BYTE> pixels(stride * height);
    THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
    for (const auto& task : geometry->nodes)
    {
        const auto x      = static_cast<std::size_t>(task.bounds.x + task.bounds.width / 2.0),
                   y      = static_cast<std::size_t>(task.bounds.y + task.bounds.height / 2.0);
        const auto offset = y * stride + x * 4u;
        check(x < width && y < height && pixels[offset] > 150u && pixels[offset + 1u] < 100u && pixels[offset + 2u] < 100u,
              "each independently measured task bar contains actual raster ink");
    }
    const std::vector<FlowchartSize> nodes(graph->nodes.size(), {16.0, 12.0}), labels(1u, {16.0, 12.0});
    for (unsigned variant = 0u; variant < 5u; ++variant)
    {
        auto invalid = std::make_shared<FlowchartModel>(*graph);
        switch (variant)
        {
            case 0u: invalid->direction = FlowchartDirection::LeftRight; break;
            case 1u: invalid->nodes[0u].ganttEnd = invalid->nodes[0u].ganttStart; break;
            case 2u: ++invalid->nodes[0u].ganttStart; break;
            case 3u: invalid->groups.emplace_back(); break;
            case 4u: invalid->nodes[0u].ganttEndSource.last = invalid->source.size() + 1u; break;
        }
        bool layoutRejected{}, paintRejected{};
        {
            NativeFlowchartLayout layout;
            layoutRejected = layout.Begin(invalid, nodes, {}, {}, {}, labels) == SourceResult::InvalidArgument;
        }
        paintRejected = prepared.Begin(write.get(), drawing.get(), invalid, {}, {1u, 1u}) == LayoutPreparationStatus::Failed;
        check(! ValidGanttModel(*invalid) && layoutRejected && paintRejected && prepared.Ready() && prepared.Geometry() == geometry,
              "direct invalid Gantt models fail both services without replacing the prior publication");
    }
}
template <typename Check> inline void ErRaster(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto graph     = FlowchartLayoutTests::Graph(L"erDiagram\nCUSTOMER ||--o{ ORDER : places\nCUSTOMER {\nstring name\nint id PK\n}\n");
    const auto write     = WriteFactory();
    const auto drawing   = DrawingFactory();
    PreparedFlowchart prepared;
    prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
    Complete(prepared, check);
    const auto geometry = prepared.Geometry();
    check(prepared.Ready() && geometry && geometry->edges.size() == 1u, "ER diagram prepares native entity and relationship geometry");
    if (! prepared.Ready() || ! geometry || geometry->edges.empty())
        return;
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    constexpr UINT width = 600u, height = 400u, stride = width * 4u;
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
    wil::com_ptr<ID2D1RenderTarget> target;
    const auto properties =
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
    wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), text.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
    target->BeginDraw();
    target->Clear(D2D1::ColorF(D2D1::ColorF::White));
    check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}), "ER entity and relationship paint");
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
                    if (pixels[offset] > 150u && pixels[offset + 1u] < 100u && pixels[offset + 2u] < 100u)
                        return true;
                }
        return false;
    };
    const auto& edge = geometry->edges[0u];
    check(edge.erFrom == ErMultiplicity::One && edge.erTo == ErMultiplicity::ZeroOrMany && blueNear(edge.path.front().x - 3.0, edge.path.front().y + 4.0) &&
              blueNear(edge.path.back().x - 3.0, edge.path.back().y - 4.0),
          "one and optional-many endpoint marks produce visible raster ink beside the relationship");
    const std::vector<FlowchartSize> nodeSizes(graph->nodes.size(), {16.0, 12.0}), edgeSizes(graph->edges.size(), {24.0, 12.0}),
        endpointSizes(graph->edges.size(), {16.0, 12.0});
    for (unsigned variant = 0u; variant < 5u; ++variant)
    {
        auto invalid = std::make_shared<FlowchartModel>(*graph);
        switch (variant)
        {
            case 0u: invalid->direction = FlowchartDirection::LeftRight; break;
            case 1u: invalid->nodes[0u].shape = FlowchartShape::Circle; break;
            case 2u: invalid->edges[0u].erFrom = ErMultiplicity::None; break;
            case 3u: invalid->edges[0u].toCardinality = L"1"; break;
            case 4u: invalid->edges[0u].arrow = true; break;
        }
        bool layoutRejected{}, paintRejected{};
        {
            NativeFlowchartLayout layout;
            layoutRejected = layout.Begin(invalid, nodeSizes, edgeSizes, endpointSizes, endpointSizes) == SourceResult::InvalidArgument;
        }
        paintRejected = prepared.Begin(write.get(), drawing.get(), invalid, {}, {1u, 1u}) == LayoutPreparationStatus::Failed;
        check(! ValidErModel(*invalid) && layoutRejected && paintRejected && prepared.Ready() && prepared.Geometry() == geometry,
              "direct invalid ER models fail both services without replacing the prior publication");
    }
}
template <typename Check> inline void PieRaster(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto graph     = FlowchartLayoutTests::Graph(L"pie showData title Inventory\n\"One\": 25\n\"Three\": 75\n");
    const auto write     = WriteFactory();
    const auto drawing   = DrawingFactory();
    PreparedFlowchart prepared;
    prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
    Complete(prepared, check);
    const auto geometry = prepared.Geometry();
    check(prepared.Ready() && geometry && geometry->nodes.size() == 2u, "bounded pie prepares two native sectors");
    if (! prepared.Ready() || ! geometry || geometry->nodes.size() != 2u)
        return;
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    constexpr UINT width = 600u, height = 400u, stride = width * 4u;
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
    wil::com_ptr<ID2D1RenderTarget> target;
    const auto properties =
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
    wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), text.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
    stroke->SetOpacity(.8f);
    target->BeginDraw();
    target->Clear(D2D1::ColorF(D2D1::ColorF::White));
    check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}), "native pie paints on the caller target");
    THROW_IF_FAILED(target->EndDraw());
    check(std::abs(stroke->GetOpacity() - .8f) < .001f, "pie painting restores caller brush opacity");
    std::vector<BYTE> pixels(stride * height);
    THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
    const auto red = [&](double x, double y)
    {
        const auto px = static_cast<std::size_t>(x), py = static_cast<std::size_t>(y);
        return pixels[py * stride + px * 4u + 2u];
    };
    const auto circle   = geometry->pieCircle;
    const double radius = circle.width / 2.0, centerX = circle.x + radius, centerY = circle.y + radius;
    const auto first       = red(centerX + radius * .35, centerY - radius * .35);
    const auto second      = red(centerX - radius * .35, centerY + radius * .35);
    const auto firstLegend = geometry->nodes[0u].bounds, secondLegend = geometry->nodes[1u].bounds;
    check(first > second + 15u && std::abs(static_cast<int>(first) - static_cast<int>(red(firstLegend.x + 6.0, firstLegend.y + 6.0))) <= 2 &&
              std::abs(static_cast<int>(second) - static_cast<int>(red(secondLegend.x + 6.0, secondLegend.y + 6.0))) <= 2,
          "quarter and three-quarter sector pixels independently match their ordered legend swatches");
    const std::vector<FlowchartSize> nodes(graph->nodes.size(), {16.0, 12.0}), labels(1u, {16.0, 12.0});
    for (unsigned variant = 0u; variant < 5u; ++variant)
    {
        auto invalid = std::make_shared<FlowchartModel>(*graph);
        switch (variant)
        {
            case 0u: invalid->direction = FlowchartDirection::LeftRight; break;
            case 1u: invalid->nodes[0u].pieValue = -1.0; break;
            case 2u: invalid->nodes[0u].pieValue = 26.0; break;
            case 3u: invalid->groups.emplace_back(); break;
            case 4u: invalid->nodes[0u].valueSource.last = invalid->source.size() + 1u; break;
        }
        bool layoutRejected{}, paintRejected{};
        {
            NativeFlowchartLayout layout;
            layoutRejected = layout.Begin(invalid, nodes, {}, {}, {}, labels) == SourceResult::InvalidArgument;
        }
        paintRejected = prepared.Begin(write.get(), drawing.get(), invalid, {}, {1u, 1u}) == LayoutPreparationStatus::Failed;
        check(! ValidPieModel(*invalid) && layoutRejected && paintRejected && prepared.Ready() && prepared.Geometry() == geometry,
              "direct invalid pie models fail in both services without replacing the prior publication");
    }
    const auto single = FlowchartLayoutTests::Graph(L"pie\n\"Whole\": 1\n\"Zero\": 0\n");
    prepared.Begin(write.get(), drawing.get(), single, {}, {2u, 2u});
    Complete(prepared, check);
    const auto whole = prepared.Geometry();
    check(prepared.Ready() && whole && whole->nodes.size() == 2u && FlowchartLayoutTests::Near(whole->nodes[1u].pieStart, whole->nodes[1u].pieEnd),
          "single positive slice and zero slice prepare without a spurious wedge");
    if (prepared.Ready() && whole)
    {
        target->BeginDraw();
        target->Clear(D2D1::ColorF(D2D1::ColorF::White));
        check(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}), "whole-circle pie paints");
        THROW_IF_FAILED(target->EndDraw());
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
        const auto c = whole->pieCircle;
        const auto r = c.width / 2.0, x = c.x + r, y = c.y + r;
        check(red(x + r * .4, y) < 240u && red(x - r * .4, y) < 240u && red(x, y + r * .4) < 240u && red(x, y - r * .4) < 240u,
              "a full-circle sector fills all four independently sampled quadrants");
    }
}
template <typename Check> inline void SequenceRaster(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto graph =
        FlowchartLayoutTests::Graph(L"sequenceDiagram\nparticipant A as Alpha\nparticipant B as Beta\nA->>B: hello\nB-->>A: reply\nA-)A: later\n");
    const auto write   = WriteFactory();
    const auto drawing = DrawingFactory();
    PreparedFlowchart prepared;
    prepared.Begin(write.get(), drawing.get(), graph, {}, {1u, 1u});
    Complete(prepared, check);
    const auto geometry = prepared.Geometry();
    check(prepared.Ready() && geometry && geometry->nodes.size() == 2u && geometry->edges.size() == 3u,
          "sequence participants and messages prepare as a native timeline");
    if (! prepared.Ready() || ! geometry || geometry->nodes.size() != 2u || geometry->edges.size() != 3u)
        return;
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    constexpr UINT width = 600u, height = 400u, stride = width * 4u;
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
    wil::com_ptr<ID2D1RenderTarget> target;
    const auto properties =
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
    wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), text.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()));
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), fill.put()));
    target->BeginDraw();
    target->Clear(D2D1::ColorF(D2D1::ColorF::White));
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
                    if (pixels[offset] > 150u && pixels[offset + 1u] < 100u && pixels[offset + 2u] < 100u)
                        return true;
                }
        return false;
    };
    const auto& first = geometry->edges[0u];
    check(blueNear((first.path[0u].x + first.path[3u].x) / 2.0, first.path[0u].y), "sequence message has actual horizontal raster ink");
    const auto& loop = geometry->edges[2u];
    check(loop.openArrow && blueNear((loop.arrow[0u].x + loop.arrow[1u].x) / 2.0, (loop.arrow[0u].y + loop.arrow[1u].y) / 2.0),
          "open asynchronous self-message arrowhead has visible raster ink");
    const auto& participant = geometry->nodes[0u].bounds;
    const auto lifelineX    = participant.x + participant.width / 2.0;
    std::size_t lifelineInk{};
    for (double y = participant.y + participant.height + 2.0; y < participant.y + participant.height + 18.0; y += 2.0)
        lifelineInk += blueNear(lifelineX, y);
    check(lifelineInk > 0u, "participant has a visible lifeline below its header");
    const auto published = prepared.Geometry();
    const std::vector<FlowchartSize> nodeSizes(graph->nodes.size(), {16.0, 12.0}), edgeSizes(graph->edges.size(), {24.0, 12.0});
    for (unsigned variant = 0u; variant < 7u; ++variant)
    {
        auto invalid = std::make_shared<FlowchartModel>(*graph);
        switch (variant)
        {
            case 0u: invalid->direction = FlowchartDirection::LeftRight; break;
            case 1u: invalid->nodes[0u].shape = FlowchartShape::Rounded; break;
            case 2u: invalid->groups.emplace_back(); break;
            case 3u: invalid->edges[0u].relation = FlowchartRelation::Inheritance; break;
            case 4u: invalid->edges[0u].style = FlowchartLineStyle::Thick; break;
            case 5u: invalid->edges[0u].fromCardinality = L"1"; break;
            case 6u: invalid->edges[0u].minimumRanks = 2u; break;
        }
        check(! ValidSequenceModel(*invalid), "direct sequence model validator rejects unsupported topology or message kinds");
        bool layoutRejected{}, paintRejected{};
        {
            NativeFlowchartLayout layout;
            const std::vector<FlowchartSize> groups(invalid->groups.size());
            layoutRejected = layout.Begin(invalid, nodeSizes, edgeSizes, {}, {}, groups) == SourceResult::InvalidArgument;
        }
        paintRejected = prepared.Begin(write.get(), drawing.get(), invalid, {}, {1u, 1u}) == LayoutPreparationStatus::Failed;
        check(layoutRejected && paintRejected && prepared.Ready() && prepared.Geometry() == published,
              "layout and paint reject forged sequence models while preserving the prior publication");
    }
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
                if (classNote && ! left)
                    continue;
                const auto source = classNote ? std::wstring(L"classDiagram\ndirection ") + direction + L"\nclass S\nnote for S \"hello\"\n"
                                              : std::wstring(L"stateDiagram-v2\ndirection ") + direction + L"\nS\n" +
                                                    (left ? L"note left of S\nhello\nworld\nend note\n" : L"note right of S: hello\n");
                const auto graph  = FlowchartLayoutTests::Graph(source);
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
                const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
                                                                     D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
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
        const auto properties =
            D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
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
