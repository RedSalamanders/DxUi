#include "../../Samples/EmbeddedControls/EmbeddedScene.h"
#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#include "../../src/Support/PostedPayload.h"
#include <DxUi/ControlCatalog.h>
#include <DxUi/Diagnostics.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>

// Counts library C++ heap calls within the deliberately isolated composition loop, not driver allocations.
static thread_local bool countAllocations = false;
static thread_local size_t allocations    = 0;
void* operator new(size_t bytes)
{
    if (countAllocations)
        ++allocations;
    if (auto* p = malloc(bytes ? bytes : 1))
        return p;
    throw std::bad_alloc();
}
void* operator new[](size_t bytes)
{
    return ::operator new(bytes);
}
void operator delete(void* p) noexcept
{
    free(p);
}
void operator delete[](void* p) noexcept
{
    free(p);
}
void operator delete(void* p, size_t) noexcept
{
    free(p);
}
void operator delete[](void* p, size_t) noexcept
{
    free(p);
}
void* operator new(size_t bytes, std::align_val_t alignment)
{
    if (countAllocations)
        ++allocations;
    if (auto* p = _aligned_malloc(bytes ? bytes : 1, static_cast<size_t>(alignment)))
        return p;
    throw std::bad_alloc();
}
void* operator new[](size_t b, std::align_val_t a)
{
    return ::operator new(b, a);
}
void operator delete(void* p, std::align_val_t) noexcept
{
    _aligned_free(p);
}
void operator delete[](void* p, std::align_val_t) noexcept
{
    _aligned_free(p);
}
void operator delete(void* p, size_t, std::align_val_t) noexcept
{
    _aligned_free(p);
}
void operator delete[](void* p, size_t, std::align_val_t) noexcept
{
    _aligned_free(p);
}
static size_t checks = 0;
static void Check(bool value, const char* text)
{
    ++checks;
    if (! value)
    {
        std::cerr << "FAIL: " << text << '\n';
        std::exit(1);
    }
}
static void Hr(HRESULT hr, const char* text)
{
    if (FAILED(hr))
        std::cerr << "HRESULT: " << std::hex << hr << std::dec << '\n';
    Check(SUCCEEDED(hr), text);
}
#include "ComplexUiBenchmark.h"
#include "EmbeddedAccessibilityTests.h"
#include "EmbeddedTextInputTests.h"

// Keep unrelated functional-test locals out of the benchmark entry stack, even under LTCG.
__declspec(noinline) static int RunFunctionalTests()
{
    static size_t diagnosticCalls = 0;
    DxUi::Diagnostics::sink       = [](std::wstring_view, std::wstring_view message) noexcept
    {
        if (message.find(L"public-probe detail") != std::wstring_view::npos)
            ++diagnosticCalls;
    };
    Check(DxUi::IsContextMenuDiagnosticsEnabled(), "native diagnostics use public sink");
    DxUi::TraceContextMenuDiagnostics(L"public-probe", L"detail");
    Check(diagnosticCalls == 1, "native trace reaches caller sink");
    DxUi::Diagnostics::sink = nullptr;
    Check(! DxUi::IsContextMenuDiagnosticsEnabled(), "clearing sink disables native diagnostics");
    GraphicsFixture gpu;
    Hr(gpu.Create(), "supplied WARP device");
    TestEmbeddedTextInput(gpu);
    TestEmbeddedAccessibility(gpu);
    EmbeddedScene scene;
    size_t requests = 0;
    Hr(scene.Initialize(gpu.device.get(), {&requests, [](void* p) noexcept { ++*static_cast<size_t*>(p); }}), "public scene");
    Check(scene.view.Controls().GetHwnd() == nullptr, "embedded has no HWND");
    Hr(scene.view.Prepare(480, 240), "prepare controls");
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "composite controls");
    std::vector<uint8_t> before;
    Hr(gpu.Read(before), "read initial pixels");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 45, 84}), "toggle down");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Up, 45, 84}), "toggle up");
    Check(! scene.enabled, "toggle changes model once");
    Check(scene.view.Controls().GetCapturedControl() == nullptr, "toggle release clears logical capture");
    Check(requests == 1, "dirty notifications coalesce");
    Hr(scene.view.Prepare(480, 240), "prepare changed toggle");
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "compose changed toggle");
    std::vector<uint8_t> after;
    Hr(gpu.Read(after), "read changed pixels");
    size_t changed = 0;
    for (size_t i = 0; i < before.size(); i += 4)
        if (memcmp(before.data() + i, after.data() + i, 4) != 0)
            ++changed;
    Check(changed > 100, "toggle changes visible pixels");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 45, 84}), "toggle press before cancel");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Cancel}), "toggle cancellation");
    Check(! scene.enabled && scene.view.Controls().GetCapturedControl() == nullptr, "canceled toggle does not activate");
    Hr(scene.view.Prepare(480, 240), "prepare after canceled toggle");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 45, 84}), "toggle press before outside release");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Up, 470, 230}), "captured toggle release outside");
    Check(! scene.enabled, "outside release does not toggle");
    Hr(scene.view.Prepare(480, 240), "prepare after outside release");
    size_t preview = 0, commit = 0, cancel = 0;
    scene.slider->SetOnChange([&](DxUi::SliderChange c)
    {
        scene.intensity = c.value;
        if (c.phase == DxUi::SliderChangePhase::Preview)
            ++preview;
        else if (c.phase == DxUi::SliderChangePhase::Commit)
            ++commit;
        else
            ++cancel;
    });
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 120, 184}), "slider down");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Move, 340, 184}), "slider preview");
    Check(preview >= 2 && commit == 0 && scene.intensity > 70, "live slider previews");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Cancel}), "capture cancellation");
    Check(cancel == 1 && scene.intensity == 65, "cancel restores initial value");
    Hr(scene.view.Prepare(480, 240), "prepare between gestures");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 300, 184}), "second slider down");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Up, 300, 184}), "slider release");
    Check(commit == 1 && scene.intensity > 60, "exactly one drag commit");
    Check(scene.view.DispatchKey(VK_END, true), "keyboard end");
    Check(scene.intensity == 100 && commit == 2, "keyboard commits");
    Hr(scene.view.Prepare(480, 240), "prepare after input");
    const auto prepared = scene.view.GetStatistics();
    Check(scene.view.Prepare(480, 240) == S_FALSE, "clean preparation is skipped");
    gpu.Bind();
    const auto start = std::chrono::steady_clock::now();
    countAllocations = true;
    for (int i = 0; i < 1000; ++i)
        Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "warm compose");
    countAllocations = false;
    const auto us    = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
    Check(allocations == 0, "1000 warm composites have zero C++ allocations");
    Check(scene.view.GetStatistics().surfaceAllocations == prepared.surfaceAllocations && scene.view.GetStatistics().preparations == prepared.preparations,
          "warm composition creates no surface or raster work");
    // Poison the state a preceding widget can leave behind; composite must establish its own pipeline.
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "reference pipeline");
    std::vector<uint8_t> reference;
    Hr(gpu.Read(reference), "reference pixels");
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode      = D3D11_FILL_SOLID;
    raster.CullMode      = D3D11_CULL_FRONT;
    raster.ScissorEnable = TRUE;
    wil::com_ptr_nothrow<ID3D11RasterizerState> hostileRaster;
    Hr(gpu.device->CreateRasterizerState(&raster, hostileRaster.put()), "hostile raster state");
    gpu.Bind();
    gpu.context->RSSetState(hostileRaster.get());
    const D3D11_RECT empty{};
    gpu.context->RSSetScissorRects(1, &empty);
    gpu.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    gpu.context->VSSetShader(nullptr, nullptr, 0);
    gpu.context->PSSetShader(nullptr, nullptr, 0);
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "hostile state composite");
    std::vector<uint8_t> hostile;
    Hr(gpu.Read(hostile), "hostile output");
    Check(hostile == reference, "output independent of previous raster and shader state");
    auto negative     = gpu.Viewport();
    negative.TopLeftX = -20;
    negative.TopLeftY = -10;
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), negative), "negative page-swipe viewport");
    Hr(gpu.Read(hostile), "negative output");
    Check(memcmp(hostile.data() + size_t(174 * 480 + 280) * 4, reference.data() + size_t(184 * 480 + 300) * 4, 4) == 0,
          "negative origin translates pixels without rerasterization");
    auto invalid     = gpu.Viewport();
    invalid.TopLeftX = std::numeric_limits<float>::quiet_NaN();
    Check(scene.view.Composite(gpu.context.get(), invalid) == E_INVALIDARG, "reject NaN viewport");
    scene.view.SetVisible(false);
    Check(! scene.view.NeedsAnimation() && ! scene.view.NeedsPreparation(), "hidden has no scheduled work");
    Check(scene.view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE && ! scene.view.DispatchPointer({DxUi::PointerAction::Down, 45, 84}),
          "hidden ignores render/input");
    scene.view.SetVisible(true);
    Hr(scene.view.Prepare(480, 240), "resume");
    Check(scene.view.Prepare(0, 0) == S_FALSE, "zero target suspends");
    Check(! scene.view.NeedsPreparation() && ! scene.view.NeedsAnimation(), "zero target schedules no work");
    Check(! scene.view.DispatchPointer({DxUi::PointerAction::Down, 45, 84}), "zero target ignores input");
    Hr(scene.view.Prepare(960, 480, 192), "DPI and resize");
    Check(scene.view.GetStatistics().surfaceBytes == 960ull * 480 * 4, "surface budget accounted");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 90, 168}) && scene.view.DispatchPointer({DxUi::PointerAction::Up, 90, 168}),
          "physical pixel input transforms at 200 percent");
    Check(scene.enabled, "DPI toggle works");
    Check(scene.view.Prepare(16384, 16384) == E_OUTOFMEMORY, "over-budget rejected");
    Check(! scene.view.DispatchPointer({DxUi::PointerAction::Down, 90, 168}), "failed preparation disables input");
    Hr(scene.view.Prepare(480, 240), "recover from invalid target");
    const auto originalBounds  = scene.slider->GetBounds();
    const double capturedStart = scene.slider->GetValue();
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 100, 184}), "capture before external geometry mutation");
    scene.slider->SetBounds(D2D1::RectF(24, 200, 440, 232));
    Check(! scene.view.DispatchPointer({DxUi::PointerAction::Move, 220, 210}), "unprepared geometry cannot retarget captured input");
    Check(scene.view.Controls().GetCapturedControl() == nullptr && scene.slider->GetValue() == capturedStart, "geometry invalidation cancels draft");
    scene.slider->SetBounds(originalBounds);
    Hr(scene.view.Prepare(480, 240), "prepare restored geometry");
    for (bool hide : {false, true})
    {
        const double initial        = scene.slider->GetValue();
        const size_t canceledBefore = cancel;
        Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 80, 184}), "press before availability change");
        if (hide)
            scene.slider->SetVisible(false);
        else
            scene.slider->SetEnabled(false);
        Hr(scene.view.Prepare(480, 240), "prepare unavailable control cancels before pruning capture");
        Check(scene.slider->GetValue() == initial && cancel == canceledBefore + 1 && scene.view.Controls().GetCapturedControl() == nullptr,
              "hidden or disabled slider restores its draft exactly once");
        scene.slider->SetVisible(true);
        scene.slider->SetEnabled(true);
        Hr(scene.view.Prepare(480, 240), "restore control availability");
    }
    std::shared_ptr<DxUi::GraphicsDevice> shared;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), shared), "shared supplied device pool");
    DxUi::EmbeddedHost a, b;
    Hr(a.Attach(shared), "first shared view");
    Hr(b.Attach(shared), "second shared view");
    for (const auto& item : DxUi::GetControlCatalog())
    {
        std::unique_ptr<DxUi::Control> control;
        Hr(DxUi::CreateControl(item.kind, control), "catalog creates control");
        a.Controls().SetRoot(std::move(control));
        a.MarkDirty();
        Hr(a.Prepare(480, 240), "catalog prepares every concrete control");
        gpu.Bind();
        Check(a.Composite(gpu.context.get(), gpu.Viewport()) == S_OK, "catalog draws every concrete control without suppression");
    }
    Check(DxUi::GetControlCatalog().size() == 26, "catalog contains all 26 controls");
    b.Controls().SetRoot(std::make_unique<DxUi::Toggle>(L"Second view"));
    Hr(b.Prepare(320, 160, 144), "independent second layout");
    Check(a.GetStatistics().surfaceBytes != b.GetStatistics().surfaceBytes, "shared pool has independent surfaces");
    auto progress     = std::make_unique<DxUi::ProgressBar>();
    auto* progressPtr = progress.get();
    progressPtr->SetIndeterminate(true);
    b.Controls().SetRoot(std::move(progress));
    Check(b.NeedsAnimation(), "attaching an indeterminate control schedules initial discovery");
    const auto tick = GetTickCount64();
    Check(b.AdvanceAnimation(tick), "indeterminate progress requests another host tick");
    Check(b.Prepare(0, 0) == S_FALSE, "zero-sized animation suspends");
    Check(! b.NeedsAnimation() && ! b.NeedsPreparation() && ! b.AdvanceAnimation(tick + 8), "zero-sized view has no ticking or preparation");
    b.MarkDirty();
    Check(! b.NeedsPreparation(), "zero-sized invalidation remains deferred");
    Hr(b.Prepare(320, 160, 144), "resume sized animation");
    Check(b.NeedsAnimation(), "sizing restores pending animation");
    b.SetVisible(false);
    Check(! b.NeedsAnimation(), "hidden animation suspends");
    b.SetVisible(true);
    Check(b.NeedsAnimation(), "shown animation resumes");
    progressPtr->SetIndeterminate(false);
    Check(! b.AdvanceAnimation(tick + 16), "stopped animation goes idle");
    Check(b.Controls().SetTooltipDelayed(L"Help", D2D1::Point2F(8, 8)), "schedule embedded tooltip");
    b.AdvanceAnimation(tick + 2000);
    Check(b.Controls().HasTooltip(), "host ticks advance tooltip deadlines");
    Hr(scene.view.Prepare(480, 240), "prepare before resize cancellation");
    const double resizeInitial = scene.slider->GetValue();
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 120, 184}), "drag before resize");
    Hr(scene.view.Prepare(640, 320), "resize cancels capture");
    Check(scene.slider->GetValue() == resizeInitial, "resize restores canceled draft");
    GraphicsFixture replacement;
    Hr(replacement.Create(), "new device generation");
    std::shared_ptr<DxUi::GraphicsDevice> next;
    Hr(DxUi::GraphicsDevice::Create(replacement.device.get(), next), "replacement pool");
    const double previous = scene.slider->GetValue();
    Hr(scene.view.ReplaceDevice(next), "device replacement");
    Check(scene.slider->GetValue() == previous, "model survives replacement");
    Hr(scene.view.Prepare(480, 240), "prepare replacement");
    Check(scene.view.Composite(gpu.context.get(), gpu.Viewport()) == E_INVALIDARG, "reject foreign device context");
    replacement.Bind();
    Hr(scene.view.Composite(replacement.context.get(), replacement.Viewport()), "draw replacement");
    Hr(replacement.Save(L".build/test-artifacts/embedded-controls.png"), "PNG evidence");
    struct Payload
    {
        size_t* destroyed;
        ~Payload()
        {
            ++*destroyed;
        }
    };
    wil::unique_hwnd payloadWindow(CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr));
    Check(bool(payloadWindow), "payload fixture window");
    InitPostedPayloadWindow(payloadWindow.get());
    size_t destroyed = 0;
    for (size_t i = 0; i < 128; ++i)
        Check(PostMessagePayload(payloadWindow.get(), WM_APP + 77, 0, std::make_unique<Payload>(&destroyed)), "bounded payload accepted");
    Check(! PostMessagePayload(payloadWindow.get(), WM_APP + 77, 0, std::make_unique<Payload>(&destroyed)) && destroyed == 1,
          "saturation rejects and releases ownership");
    Check(DrainPostedPayloadsForWindow(payloadWindow.get()) == 128 && destroyed == 129, "drain releases each payload once");
    MSG stale{};
    while (PeekMessageW(&stale, payloadWindow.get(), WM_APP + 77, WM_APP + 77, PM_REMOVE))
        Check(! TakeMessagePayload<Payload>(stale.lParam), "drained tokens cannot resurrect payload");
    InitPostedPayloadWindow(payloadWindow.get());
    Check(PostMessagePayload(payloadWindow.get(), WM_APP + 77, 0, std::make_unique<Payload>(&destroyed)), "reinitialize window generation");
    Check(PeekMessageW(&stale, payloadWindow.get(), WM_APP + 77, WM_APP + 77, PM_REMOVE) != FALSE, "take new token");
    Check(! TakeMessagePayload<int>(stale.lParam) && destroyed == 130, "wrong payload type rejected and released");
    DrainPostedPayloadsForWindow(payloadWindow.get());
    // A callback may destroy the current tree, including the slider dispatching the event.
    scene.slider->SetOnChange([&](DxUi::SliderChange) { scene.view.Controls().SetRoot(std::make_unique<DxUi::Panel>()); });
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 100, 184}), "root replacement during slider callback");
    {
        ComplexUiScene complex;
        Hr(complex.Initialize(gpu.device.get()), "independent complex sample");
        Hr(complex.view.Prepare(1280, 720), "prepare independent controls");
        Check(complex.view.DispatchPointer({DxUi::PointerAction::Down, 240, 52}), "complex sample slider input");
        Check(complex.progress[0]->GetValue() == complex.sliders[0]->GetValue() && complex.sliders[0]->GetValue() > 50,
              "independent sample binds slider preview to progress");
        Check(complex.view.DispatchPointer({DxUi::PointerAction::Cancel}), "complex sample slider cancellation");
        Check(complex.progress[0]->GetValue() == 0, "complex sample cancellation restores progress");
    }
    std::cout << "PASS " << checks << " checks; 1000 warm composites " << us << " us; C++ allocations " << allocations << "; changed toggle pixels " << changed
              << '\n';
    return 0;
}

#include "BenchmarkMain.h"
