#include "Controls.Tests.WorkCounters.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Support/NativeRendering.h"
#include "Support/WorkCounters.h"

#include <d2d1_3.h>
#include <d2d1effects.h>
#include <dwrite.h>
#include <set>
#include <string>

#pragma comment(lib, "dxguid.lib")

using namespace DxUi;

namespace
{
// A WARP device with its Direct2D device and DirectWrite factory, created through the counted D3D11 seam.
struct CountedGraphics final
{
    wil::com_ptr<ID3D11Device> device;
    wil::com_ptr<ID3D11DeviceContext> context;
    wil::com_ptr<ID2D1Factory1> factory;
    wil::com_ptr<ID2D1Device> d2dDevice;
    wil::com_ptr<IDWriteFactory> write;
};

CountedGraphics CreateCountedGraphics()
{
    CountedGraphics graphics;
    const std::array levels{D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL{};
    RequireSucceeded(
        Detail::CreateD3D11DeviceWithWarpFallback(
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, true, graphics.device.put(), &level, graphics.context.put(), nullptr, Detail::SystemD3D11DeviceCreator()),
        "create the WARP device through the counted seam");
    const D2D1_FACTORY_OPTIONS options{};
    RequireSucceeded(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options, graphics.factory.put_void()),
                     "create the Direct2D factory");
    wil::com_ptr<IDXGIDevice> dxgi;
    RequireSucceeded(graphics.device->QueryInterface(IID_PPV_ARGS(dxgi.put())), "query the DXGI device");
    RequireSucceeded(graphics.factory->CreateDevice(dxgi.get(), graphics.d2dDevice.put()), "create the Direct2D device");
    RequireSucceeded(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), graphics.write.put_unknown()), "create the DirectWrite factory");
    return graphics;
}

// Three labels and one hidden button under a panel: four children, three visible.
Panel& BuildThreeVisibleLabels(WindowHost& host)
{
    auto root = std::make_unique<Panel>();
    for (int index = 0; index < 3; ++index)
        root->AddChild<Label>(L"Label " + std::to_wstring(index))
            ->SetBounds(D2D1::RectF(4.0f, 4.0f + 28.0f * static_cast<float>(index), 150.0f, 28.0f + 28.0f * static_cast<float>(index)));
    auto* hidden = root->AddChild<Button>(L"Hidden");
    hidden->SetBounds(D2D1::RectF(160.0f, 4.0f, 260.0f, 32.0f));
    hidden->SetVisible(false);
    Panel& panel = *root;
    host.SetRoot(std::move(root));
    return panel;
}
} // namespace

void TestWorkCounterScopeReportsExactDeltasAndChurn()
{
    const Detail::WorkCounterScope scope;
    auto& work = Detail::Work();
    work.frame.paintExecutions += 3u;
    ++work.wakes.animationTimer;
    work.resources.textLayout.created += 4u;
    ++work.resources.textLayout.destroyed;
    work.resources.brush.destroyed += 2u;
    ++work.accessibility.snapshotBuilds;

    const Detail::WorkCounters delta = scope.Delta();
    Require(delta.frame.paintExecutions == 3u && delta.wakes.animationTimer == 1u && delta.accessibility.snapshotBuilds == 1u,
            "a scope reports exactly what was added on this thread since it began");
    Require(delta.resources.textLayout.created == 4u && delta.resources.textLayout.destroyed == 1u && delta.resources.brush.destroyed == 2u,
            "resource tallies report creations and destructions separately");
    Require(Detail::TotalCreated(delta) == 4u, "total creations sum every kind");
    Require(Detail::Churn(delta) == 1u, "churn is min(created, destroyed) per kind: one text layout, no brush created");

    std::set<std::string> names;
    size_t visited = 0u;
    uint64_t sum   = 0u;
    Detail::ForEachWorkCounter(delta,
                               [&](const char* group, const char* name, uint64_t value)
    {
        names.insert(std::string(group) + '.' + name);
        ++visited;
        sum += value;
    });
    Require(names.size() == visited, "every counter has a distinct group-qualified name");
    Require(sum == 3u + 1u + 4u + 1u + 2u + 1u, "the visitor reports every counter value exactly once");
    for (const char* expected : {"frame.paintExecutions",
                                 "frame.treeVisitsPaint",
                                 "frame.treeVisitsTick",
                                 "frame.treeVisitsHitTest",
                                 "frame.pixelsPainted",
                                 "wakes.animationTimer",
                                 "resources.textLayout.created",
                                 "resources.implicitTextLayout.destroyed",
                                 "resources.d3d11Device.created",
                                 "popups.backdropCaptures",
                                 "accessibility.snapshotNodes"})
        Require(names.contains(expected), "the visitor names each documented counter");

    const Detail::WorkCounterScope empty;
    uint64_t total = 0u;
    Detail::ForEachWorkCounter(empty.Delta(), [&](const char*, const char*, uint64_t value) { total += value; });
    Require(total == 0u, "a scope with no work between its readings reports zero for every counter");
}

void TestCountedCreationHelpersCountEveryResourceKind()
{
    const Detail::WorkCounterScope scope;
    CountedGraphics graphics = CreateCountedGraphics();
    Require(scope.Delta().resources.d3d11Device.created == 1u, "the device-creation seam counts one D3D11 device");

    wil::com_ptr<ID2D1DeviceContext> dc;
    RequireSucceeded(CountedCreateDeviceContext(graphics.d2dDevice, D2D1_DEVICE_CONTEXT_OPTIONS_NONE, dc.put()), "counted device context");
    const D2D1_BITMAP_PROPERTIES1 targetProperties =
        D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    wil::com_ptr<ID2D1Bitmap1> target;
    RequireSucceeded(CountedCreateBitmap(dc, D2D1::SizeU(64u, 32u), nullptr, 0u, &targetProperties, target.put()), "counted bitmap");
    wil::com_ptr<ID2D1SolidColorBrush> solid;
    RequireSucceeded(CountedCreateSolidColorBrush(dc, D2D1::ColorF(D2D1::ColorF::Black), solid.put()), "counted solid brush");
    const D2D1_GRADIENT_STOP stops[]{{0.0f, D2D1::ColorF(D2D1::ColorF::Black)}, {1.0f, D2D1::ColorF(D2D1::ColorF::White)}};
    wil::com_ptr<ID2D1GradientStopCollection> stopCollection;
    RequireSucceeded(CountedCreateGradientStopCollection(dc, stops, 2u, stopCollection.put()), "counted gradient stops");
    wil::com_ptr<ID2D1LinearGradientBrush> gradient;
    RequireSucceeded(
        CountedCreateLinearGradientBrush(dc, D2D1::LinearGradientBrushProperties({0.0f, 0.0f}, {0.0f, 32.0f}), stopCollection.get(), gradient.put()),
        "counted gradient brush");
    wil::com_ptr<ID2D1Effect> effect;
    RequireSucceeded(CountedCreateEffect(dc, CLSID_D2D1GaussianBlur, effect.put()), "counted effect");
    wil::com_ptr<ID2D1CommandList> commandList;
    RequireSucceeded(CountedCreateCommandList(dc, commandList.put()), "counted command list");
    wil::com_ptr<ID2D1Layer> layer;
    RequireSucceeded(CountedCreateLayer(dc, layer.put()), "counted layer");
    wil::com_ptr<ID2D1PathGeometry> path;
    RequireSucceeded(CountedCreatePathGeometry(graphics.factory, path.put()), "counted path geometry");
    wil::com_ptr<ID2D1RoundedRectangleGeometry> rounded;
    RequireSucceeded(CountedCreateRoundedRectangleGeometry(graphics.factory, D2D1::RoundedRect(D2D1::RectF(0.0f, 0.0f, 8.0f, 8.0f), 2.0f, 2.0f), rounded.put()),
                     "counted rounded geometry");
    wil::com_ptr<ID2D1StrokeStyle> stroke;
    RequireSucceeded(CountedCreateStrokeStyle(graphics.factory, D2D1::StrokeStyleProperties(), nullptr, 0u, stroke.put()), "counted stroke style");

    wil::com_ptr<IDWriteTextFormat> format;
    RequireSucceeded(
        CountedCreateTextFormat(
            graphics.write, L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"", format.put()),
        "counted text format");
    wil::com_ptr<IDWriteTextFormat> typography;
    RequireSucceeded(CountedCreateTypographyTextFormat(graphics.write.get(), Typography::GetSpec(FontRole::Body), typography.put()),
                     "counted typography format");
    wil::com_ptr<IDWriteTextFormat> refused;
    Require(FAILED(CountedCreateTextFormat(graphics.write,
                                           L"Segoe UI",
                                           nullptr,
                                           DWRITE_FONT_WEIGHT_NORMAL,
                                           DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL,
                                           -1.0f,
                                           L"",
                                           refused.put())) &&
                ! refused,
            "a refused creation returns its own failure");
    wil::com_ptr<IDWriteTextLayout> layout;
    RequireSucceeded(CountedCreateTextLayout(graphics.write, L"Counted", 7u, format.get(), 100.0f, 20.0f, layout.put()), "counted text layout");
    wil::com_ptr<IDWriteRenderingParams> monitorParams;
    RequireSucceeded(CountedCreateMonitorRenderingParams(graphics.write, MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), monitorParams.put()),
                     "counted monitor rendering parameters");
    wil::com_ptr<IDWriteRenderingParams> customParams;
    RequireSucceeded(
        CountedCreateCustomRenderingParams(graphics.write, 1.8f, 0.5f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_DEFAULT, customParams.put()),
        "counted custom rendering parameters");

    dc->SetTarget(target.get());
    dc->BeginDraw();
    CountedDrawText(dc, L"Counted", 7u, format.get(), D2D1::RectF(0.0f, 0.0f, 64.0f, 32.0f), solid.get());
    RequireSucceeded(dc->EndDraw(), "draw the implicit layout");
    dc->SetTarget(nullptr);

    wil::unique_hdc source(CreateCompatibleDC(nullptr));
    wil::unique_hdc destination(CreateCompatibleDC(nullptr));
    Require(source && destination, "two memory device contexts");
    Require(CountedBitBlt(destination.get(), 0, 0, 1, 1, source.get(), 0, 0, SRCCOPY) != FALSE, "counted GDI capture");

    const Detail::WorkCounters delta = scope.Delta();
    const auto& resources            = delta.resources;
    Require(resources.d3d11Device.created == 1u && resources.deviceContext.created == 1u && resources.bitmap.created == 1u,
            "device, device context and bitmap are counted once each");
    Require(resources.brush.created == 2u && resources.gradientStops.created == 1u, "both brushes and the gradient stop collection are counted");
    Require(resources.effect.created == 1u && resources.commandList.created == 1u && resources.layer.created == 1u,
            "effect, command list and layer are counted once each");
    Require(resources.geometry.created == 2u && resources.strokeStyle.created == 1u, "geometries and stroke styles are counted");
    Require(resources.textFormat.created == 2u, "both text formats are counted and the refused one is not");
    Require(resources.textLayout.created == 1u, "the text layout is counted");
    Require(resources.renderingParams.created == 2u, "monitor and custom rendering parameters are counted");
    Require(resources.implicitTextLayout.created == 1u && resources.implicitTextLayout.destroyed == 1u,
            "a DrawText call counts one implicit layout created and released inside it");
    Require(Detail::TotalCreated(delta) == 18u, "nothing else is counted as created");
    Require(resources.textLayout.destroyed == 0u && resources.brush.destroyed == 0u, "without a tracker no destruction of an object is observed");
    Require(delta.popups.backdropCaptures == 1u && delta.popups.backdropCapturePixels == 1u, "the GDI capture and its pixels are counted");
}

void TestResourceLifetimeTrackerCountsDestructionAtSweep()
{
    wil::com_ptr<ID2D1Factory1> factory;
    const D2D1_FACTORY_OPTIONS options{};
    RequireSucceeded(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options, factory.put_void()),
                     "create the Direct2D factory");
    Require(Detail::g_resourceLifetimeTracker == nullptr, "no tracker is installed outside a test that installs one");

    std::array<Detail::TrackedResource, 2u> storage{};
    wil::com_ptr<ID2D1PathGeometry> kept;
    const Detail::WorkCounterScope scope;
    {
        Detail::ResourceLifetimeTracker tracker(storage);
        Require(Detail::g_resourceLifetimeTracker == &tracker, "constructing a tracker installs it on this thread");
        wil::com_ptr<ID2D1PathGeometry> dropped;
        wil::com_ptr<ID2D1PathGeometry> beyondStorage;
        RequireSucceeded(CountedCreatePathGeometry(factory, kept.put()), "kept geometry");
        RequireSucceeded(CountedCreatePathGeometry(factory, dropped.put()), "dropped geometry");
        RequireSucceeded(CountedCreatePathGeometry(factory, beyondStorage.put()), "geometry beyond the tracker's storage");
        Require(tracker.Held() == 2u && tracker.Overflow() == 1u, "the tracker holds what its storage fits and counts the rest as overflow");

        dropped.reset();
        beyondStorage.reset();
        Require(scope.Delta().resources.geometry.destroyed == 0u, "a released object is counted at the sweep, not when its owner releases it");
        Require(tracker.Sweep() == 1u, "the sweep finds the one tracked object its owner released");
        Detail::WorkCounters delta = scope.Delta();
        Require(delta.resources.geometry.created == 3u && delta.resources.geometry.destroyed == 1u, "the sweep counts that destruction");
        Require(Detail::Churn(delta) == 1u, "churn counts the object created and destroyed in the same interval");
        Require(tracker.Held() == 1u && tracker.Sweep() == 0u, "a live object stays tracked and is not counted again");

        kept->AddRef();
        Require(kept->Release() == 2u, "the tracker holds exactly one extra reference to a live object");
        {
            std::array<Detail::TrackedResource, 1u> innerStorage{};
            Detail::ResourceLifetimeTracker inner(innerStorage);
            Require(Detail::g_resourceLifetimeTracker == &inner, "a nested tracker takes over");
        }
        Require(Detail::g_resourceLifetimeTracker == &tracker, "destroying a nested tracker restores the previous one");
    }
    Require(Detail::g_resourceLifetimeTracker == nullptr, "destroying the tracker uninstalls it");
    kept->AddRef();
    Require(kept->Release() == 1u, "destroying the tracker drops its references without destroying live objects");
    Require(scope.Delta().resources.geometry.destroyed == 1u, "dropping the tracker's references counts nothing");
}

void TestWorkCountersCountNativeFramesWalksAndWakes()
{
    AttachedHostWindow window;
    WindowHost& host = window.Host();
    static_cast<void>(BuildThreeVisibleLabels(host));
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    {
        const Detail::WorkCounterScope paint;
        RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        Require(paint.Delta().wakes.paintMessage >= 1u, "each WM_PAINT is a counted wake");
    }
    window.PumpMessages();

    const uint64_t partialBefore = host.DebugGetPartialPresentAttemptCount();
    const Detail::WorkCounterScope frame;
    host.DebugRenderDirtyRectForTest(RECT{0, 0, 40, 30});
    Detail::WorkCounters delta = frame.Delta();
    Require(host.DebugGetPartialPresentAttemptCount() == partialBefore + 1u, "the measured frame is a partial frame");
    Require(delta.frame.renders == 1u && delta.frame.presents == 1u, "one render and one present");
    Require(delta.frame.pixelsPainted == 40u * 30u, "a partial frame paints the pixels of its dirty rectangle");
    Require(delta.frame.paintExecutions == 4u, "the root and its three visible children paint; the hidden child does not");
    Require(delta.frame.treeVisitsPaint == 5u, "the paint walk examines the root and all four children");

    // W3.1: a wake with nothing requested ticks nothing; the compatibility walk of an anonymous request still examines the
    // root and every child, visible or not.
    const Detail::WorkCounterScope idleTick;
    static_cast<void>(host.DebugAnimationTickForTest(1'000u));
    Require(idleTick.Delta().frame.treeVisitsTick == 0u, "a wake with nothing requested walks nothing");
    const Detail::WorkCounterScope tick;
    host.RequestAnimation();
    static_cast<void>(host.DebugAnimationTickForTest(1'000u));
    Require(tick.Delta().frame.treeVisitsTick == 5u && tick.Delta().frame.tickLegacyTreeWalks == 1u, "the tick walk examines the root and all four children");

    const Detail::WorkCounterScope hit;
    Require(host.DebugHitTestControl(D2D1::Point2F(20.0f, 14.0f)) != nullptr, "the first label is hit");
    delta = hit.Delta();
    Require(delta.frame.treeVisitsHitTest >= 2u && delta.frame.treeVisitsHitTest <= 5u, "a hit test visits the root and the children it offers the point to");
}

void TestWorkCountersCountAccessibilitySnapshotBuilds()
{
    AttachedHostWindow window;
    WindowHost& host = window.Host();
    Panel& panel     = BuildThreeVisibleLabels(host);
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    root.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(root != nullptr, "the host creates its root provider");
    const auto labelProvider = GetProviderAtDipPoint(window.Hwnd(), host, *root.get(), 20.0f, 14.0f, "the first label provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> labelSimple;
    RequireSucceeded(labelProvider.query_to(labelSimple.put()), "retain the first label provider");
    window.PumpMessages();
    Require(DebugSetAccessibilityMutationInterestForTest(window.Hwnd(), true), "this attachment is interested in mutations, as with a client");
    const auto clearInterest = wil::scope_exit([]() noexcept { static_cast<void>(DebugSetAccessibilityMutationInterestForTest(nullptr, std::nullopt)); });

    // A content change of an interested attachment makes the next owner query rebuild the snapshot.
    const uint64_t buildsBefore = DebugGetAccessibilitySnapshotBuildCountForTest();
    const Detail::WorkCounterScope scope;
    static_cast<Label*>(panel.GetLogicalChild(0u))->SetText(L"Changed");
    Require(ReadProviderStringProperty(*labelSimple.get(), UIA_NamePropertyId, "the changed label name") == L"Changed", "the owner query publishes the change");
    const Detail::WorkCounters delta = scope.Delta();
    const uint64_t builds            = DebugGetAccessibilitySnapshotBuildCountForTest() - buildsBefore;
    std::cerr << "    [WORK] snapshot builds=" << builds << " counted=" << delta.accessibility.snapshotBuilds << " nodes=" << delta.accessibility.snapshotNodes
              << " bytes=" << delta.accessibility.snapshotBytes << '\n';
    Require(builds >= 1u && delta.accessibility.snapshotBuilds == builds, "the work counter counts the same snapshot builds as the existing diagnostic");
    Require(delta.accessibility.snapshotNodes % builds == 0u && delta.accessibility.snapshotNodes / builds >= 3u,
            "each build records at least the three visible labels");
    Require(delta.accessibility.snapshotBytes > delta.accessibility.snapshotNodes, "the snapshot's record storage is counted in bytes");
}
