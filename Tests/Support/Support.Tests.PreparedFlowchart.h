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
    const auto huge     = FlowchartLayoutTests::Graph(L"graph TD;A[\"" + std::wstring(1024u, L'W') + L"\"]");
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
    for (const auto source :
         {L"graph TD;A[label]", L"graph TD;A[\"Unicode \u03b2 \U0001f600\"]", L"graph TD;A[\"\u05d0\u05d1\u05d2\"]", L"graph TD;A[\"first\\nsecond\"]"})
        for (const auto direction : {DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT})
        {
            const auto graph = FlowchartLayoutTests::Graph(source);
            LayoutOptions options;
            options.readingDirection = direction;
            PreparedFlowchart prepared;
            prepared.Begin(write.get(), drawing.get(), graph, options, {1u, 1u});
            Complete(prepared, check);
            check(prepared.Ready(), "plain Unicode/bidi/multiline labels produce native geometry");
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
                        THROW_IF_FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
                        THROW_IF_FAILED(format->SetReadingDirection(direction));
                        wil::com_ptr<IDWriteTextLayout> layout;
                        const auto& label = graph->nodes[0u].label;
                        THROW_IF_FAILED(
                            write->CreateTextLayout(label.data(), static_cast<UINT32>(label.size()), format.get(), 65536.0f, 65536.0f, layout.put()));
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
            THROW_IF_FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
            for (std::size_t i = 0u; i < graph->edges.size(); ++i)
                if (! graph->edges[i].label.empty())
                {
                    const auto& source = graph->edges[i].label;
                    const auto box     = plan->edges[i].label;
                    wil::com_ptr<IDWriteTextLayout> layout;
                    THROW_IF_FAILED(write->CreateTextLayout(source.data(), static_cast<UINT32>(source.size()), format.get(), 65536.0f, 65536.0f, layout.put()));
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
        const auto& head  = relationPlan->edges.front().arrow;
        const auto x      = static_cast<UINT>(std::lround((head[0u].x + head[1u].x + head[2u].x) / 3.0));
        const auto y      = static_cast<UINT>(std::lround((head[0u].y + head[1u].y + head[2u].y) / 3.0));
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

} // namespace PreparedFlowchartTests
