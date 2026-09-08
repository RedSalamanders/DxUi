// Compile the public accessibility header first: consumers must not supply its COM prerequisites.
#include <DxUi/EmbeddedAccessibility.h>

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

// Hidden and zero-extent views hold no surface; the next visible sized preparation reallocates exactly one and
// reproduces the previous pixels. Device replacement while hidden keeps working without a surface.
__declspec(noinline) static void TestSurfaceLifetime(GraphicsFixture& gpu)
{
    EmbeddedScene scene;
    size_t requests = 0;
    Hr(scene.Initialize(gpu.device.get(), {&requests, [](void* p) noexcept { ++*static_cast<size_t*>(p); }}), "surface lifetime scene");
    Hr(scene.view.Prepare(480, 240), "prepare surface lifetime scene");
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "composite before hide");
    std::vector<uint8_t> shown;
    Hr(gpu.Read(shown), "read pixels before hide");
    const auto visible = scene.view.GetStatistics();
    Check(visible.surfaceBytes == 480ull * 240 * 4 && visible.surfaceAllocations == 1, "visible view holds exactly one surface");
    const size_t requestsBeforeHide = requests;
    scene.view.SetVisible(false);
    const auto hidden = scene.view.GetStatistics();
    Check(hidden.surfaceBytes == 0 && hidden.surfaceAllocations == visible.surfaceAllocations, "hiding releases the surface without allocating");
    Check(! scene.view.NeedsPreparation() && ! scene.view.NeedsAnimation() && requests == requestsBeforeHide,
          "hidden view requests no preparation or animation");
    Check(scene.view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE, "hidden view composites nothing");
    Check(scene.view.Prepare(480, 240) == S_FALSE && scene.view.GetStatistics().surfaceBytes == 0, "hidden preparation allocates no surface");
    scene.view.MarkDirty();
    Check(! scene.view.NeedsPreparation() && requests == requestsBeforeHide, "hidden invalidation stays deferred");
    scene.view.SetVisible(true);
    Check(scene.view.NeedsPreparation() && requests == requestsBeforeHide + 1, "showing requests exactly one preparation");
    Hr(scene.view.Prepare(480, 240), "prepare after show");
    const auto restored = scene.view.GetStatistics();
    Check(restored.surfaceBytes == visible.surfaceBytes && restored.surfaceAllocations == visible.surfaceAllocations + 1,
          "showing reallocates exactly one surface");
    Check(restored.replacementPeakBytes == visible.replacementPeakBytes, "a released surface does not raise the replacement peak");
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "composite after show");
    std::vector<uint8_t> reshown;
    Hr(gpu.Read(reshown), "read pixels after show");
    Check(reshown == shown, "reallocated surface reproduces the pre-hide pixels");
    Check(scene.view.Prepare(0, 0) == S_FALSE, "zero extent suspends");
    Check(scene.view.GetStatistics().surfaceBytes == 0 && scene.view.GetStatistics().surfaceAllocations == restored.surfaceAllocations,
          "zero extent releases the surface");
    Check(! scene.view.NeedsPreparation() && ! scene.view.NeedsAnimation() && scene.view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE,
          "zero extent requests no work and composites nothing");
    Hr(scene.view.Prepare(480, 240), "prepare after zero extent");
    Check(scene.view.GetStatistics().surfaceBytes == visible.surfaceBytes && scene.view.GetStatistics().surfaceAllocations == restored.surfaceAllocations + 1,
          "sized preparation after zero extent reallocates exactly one surface");
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "composite after zero extent");
    Hr(gpu.Read(reshown), "read pixels after zero extent");
    Check(reshown == shown, "surface restored after zero extent reproduces the pixels");
    std::shared_ptr<DxUi::GraphicsDevice> pool;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "replacement pool for hidden view");
    scene.view.SetVisible(false);
    Hr(scene.view.ReplaceDevice(pool), "replace device while hidden");
    Check(scene.view.GetStatistics().surfaceBytes == 0, "replaced hidden view holds no surface");
    scene.view.SetVisible(true);
    Hr(scene.view.Prepare(480, 240), "prepare after hidden replacement");
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "composite after hidden replacement");
    Hr(gpu.Read(reshown), "read pixels after hidden replacement");
    Check(reshown == shown, "hidden device replacement reproduces the pixels");
}

// A hover or focus Invalidate must not swallow the following Down. The host message loop
// prepares after input, so a paint-dirty view is the normal state at the start of a tap.
__declspec(noinline) static void TestPointerGesturesOnPaintDirtyView(GraphicsFixture& gpu)
{
    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get()), "paint-dirty pointer scene");
    Hr(scene.view.Prepare(480, 240), "prepare paint-dirty pointer scene");
    Check(scene.view.Controls().GetInputModality() == DxUi::InputModality::Pointer, "embedded starts in pointer modality");
    Check(scene.view.DispatchKey(VK_TAB, true), "tab focuses a control");
    Check(scene.view.Controls().GetInputModality() == DxUi::InputModality::Keyboard, "key input marks keyboard focus visible");
    static_cast<void>(scene.view.DispatchPointer({DxUi::PointerAction::Move, 45, 84}));
    Check(scene.view.NeedsPreparation(), "hover marks the cached surface dirty");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 45, 84}), "paint-dirty down still begins a gesture");
    Check(scene.view.Controls().GetInputModality() == DxUi::InputModality::Pointer, "pointer down clears keyboard-only focus visuals");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Up, 45, 84}), "paint-dirty up still commits");
    Check(! scene.enabled, "first contact on a paint-dirty view activates the toggle");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 45, 84}), "second dirty down still begins a gesture");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Up, 45, 84}), "second dirty up still commits");
    Check(scene.enabled, "a second contact on the still-dirty view activates again");
}

// Host ticks dirty a view only through control invalidation: an idle root or an unchanged caret phase leaves a
// clean prepared view clean; a blink-phase flip prepares exactly once.
__declspec(noinline) static void TestTickDirtying(GraphicsFixture& gpu)
{
    std::shared_ptr<DxUi::GraphicsDevice> pool;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "tick pool");
    DxUi::EmbeddedHost view;
    Hr(view.Attach(pool), "tick view");
    auto root = std::make_unique<DxUi::Panel>();
    root->AddChild<DxUi::Label>(L"Static content")->SetBounds(D2D1::RectF(8, 8, 300, 40));
    view.Controls().SetRoot(std::move(root));
    Hr(view.Prepare(320, 160), "prepare static root");
    const auto idle = view.GetStatistics();
    Check(view.NeedsAnimation() && ! view.NeedsPreparation(), "a new root schedules one discovery tick on a clean prepared view");
    Check(! view.AdvanceAnimation(GetTickCount64()), "static root stops ticking after discovery");
    Check(! view.NeedsAnimation() && ! view.NeedsPreparation(), "the discovery tick does not dirty a static view");
    Check(view.Prepare(320, 160) == S_FALSE && view.GetStatistics().preparations == idle.preparations, "the discovery tick causes no preparation");
    view.Controls().RequestAnimation();
    Hr(view.Prepare(320, 160), "prepare after an explicit animation request");
    const auto requested = view.GetStatistics();
    Check(view.NeedsAnimation() && ! view.NeedsPreparation(), "an explicit animation request leaves a clean prepared view");
    Check(! view.AdvanceAnimation(GetTickCount64() + 1) && ! view.NeedsPreparation(), "a requested idle tick does not dirty the view");
    Check(view.Prepare(320, 160) == S_FALSE && view.GetStatistics().preparations == requested.preparations, "a requested idle tick causes no preparation");

    auto field     = std::make_unique<DxUi::TextField>();
    auto* fieldPtr = field.get();
    fieldPtr->SetText(L"Caret");
    fieldPtr->SetBounds(D2D1::RectF(8, 8, 300, 48));
    view.Controls().SetRoot(std::move(field));
    Hr(view.Prepare(320, 160), "prepare text field root");
    view.Controls().SetFocusControl(fieldPtr);
    const uint64_t focusTick = GetTickCount64();
    Check(fieldPtr->HasFocus() && view.NeedsAnimation(), "focused text field requests caret ticks");
    Hr(view.Prepare(320, 160), "prepare focused text field");
    const auto focused = view.GetStatistics();
    Check(! view.NeedsPreparation(), "focused text field is clean after preparation");
    Check(view.AdvanceAnimation(focusTick + 10), "caret keeps ticking");
    Check(! view.NeedsPreparation(), "a tick inside the blink phase does not dirty");
    Check(view.AdvanceAnimation(focusTick + 20), "caret keeps ticking within the phase");
    Check(! view.NeedsPreparation() && view.Prepare(320, 160) == S_FALSE && view.GetStatistics().preparations == focused.preparations,
          "ticks within the blink period cause no preparation");
    constexpr uint64_t kCaretBlinkPeriodMs = 530u; // kCaretBlinkPeriodMs in DxUi.TextInput.cpp
    Check(view.AdvanceAnimation(focusTick + 20 + kCaretBlinkPeriodMs), "caret keeps ticking across the blink");
    Check(view.NeedsPreparation(), "a blink-phase flip dirties the view");
    Check(view.Prepare(320, 160) == S_OK && view.GetStatistics().preparations == focused.preparations + 1, "a blink-phase flip prepares exactly once");
}

// Per-view brush and configured text-format caches stay within their bounds: an over-bound cache is cleared at
// the next preparation start, never while a paint borrows its entries, and refills with the painted working set.
__declspec(noinline) static void TestCacheBounds(GraphicsFixture& gpu)
{
    std::shared_ptr<DxUi::GraphicsDevice> pool;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "cache pool");
    DxUi::EmbeddedHost view;
    Hr(view.Attach(pool), "cache view");
    auto root = std::make_unique<DxUi::Panel>();
    root->AddChild<DxUi::Label>(L"Bounded caches")->SetBounds(D2D1::RectF(8, 8, 300, 40));
    root->AddChild<DxUi::Toggle>(L"Toggle")->SetBounds(D2D1::RectF(8, 48, 300, 88));
    view.Controls().SetRoot(std::move(root));
    Hr(view.Prepare(320, 160), "prepare cache view");
    const auto painted = view.GetStatistics();
    Check(painted.cachedBrushes > 0 && painted.cachedBrushes <= DxUi::ControlHost::kSolidBrushCacheLimit, "a painted view reports its cached brushes");
    Check(painted.cachedTextFormats <= DxUi::ControlHost::kConfiguredTextFormatCacheLimit, "a painted view reports its cached text formats");
    auto& controls = view.Controls();
    for (size_t i = 0; i <= DxUi::ControlHost::kSolidBrushCacheLimit; ++i)
        Check(controls.GetSolidBrush(D2D1::ColorF(float(i % 256) / 255.0f, float(i / 256) / 255.0f, 0.5f)) != nullptr, "distinct solid brush");
    Check(view.GetStatistics().cachedBrushes > DxUi::ControlHost::kSolidBrushCacheLimit, "distinct colors can exceed the brush bound between paints");
    for (int role = 0; role <= int(DxUi::FontRole::Small) && view.GetStatistics().cachedTextFormats <= DxUi::ControlHost::kConfiguredTextFormatCacheLimit;
         ++role)
        for (auto alignment : {DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_TEXT_ALIGNMENT_JUSTIFIED})
            for (auto paragraph : {DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_PARAGRAPH_ALIGNMENT_FAR, DWRITE_PARAGRAPH_ALIGNMENT_CENTER})
                for (bool wrap : {false, true})
                    Check(controls.GetTextFormat(DxUi::FontRole(role), alignment, paragraph, wrap) != nullptr, "distinct configured text format");
    Check(view.GetStatistics().cachedTextFormats > DxUi::ControlHost::kConfiguredTextFormatCacheLimit,
          "distinct configurations can exceed the text-format bound between paints");
    view.MarkDirty();
    Hr(view.Prepare(320, 160), "prepare trims the caches");
    const auto trimmed = view.GetStatistics();
    Check(trimmed.cachedBrushes > 0 && trimmed.cachedBrushes <= DxUi::ControlHost::kSolidBrushCacheLimit, "preparation trims the brush cache to its bound");
    Check(trimmed.cachedTextFormats <= DxUi::ControlHost::kConfiguredTextFormatCacheLimit, "preparation trims the text-format cache to its bound");
    Check(trimmed.cachedBrushes == painted.cachedBrushes, "the trimmed brush cache holds exactly the painted working set");
}

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
    TestSurfaceLifetime(gpu);
    TestPointerGesturesOnPaintDirtyView(gpu);
    TestTickDirtying(gpu);
    TestCacheBounds(gpu);
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
    const auto shownStats = scene.view.GetStatistics();
    scene.view.SetVisible(false);
    Check(! scene.view.NeedsAnimation() && ! scene.view.NeedsPreparation(), "hidden has no scheduled work");
    Check(scene.view.GetStatistics().surfaceBytes == 0, "hidden view holds no surface");
    Check(scene.view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE && ! scene.view.DispatchPointer({DxUi::PointerAction::Down, 45, 84}),
          "hidden ignores render/input");
    scene.view.SetVisible(true);
    Hr(scene.view.Prepare(480, 240), "resume");
    Check(scene.view.GetStatistics().surfaceBytes == shownStats.surfaceBytes &&
              scene.view.GetStatistics().surfaceAllocations == shownStats.surfaceAllocations + 1,
          "resume reallocates exactly one surface");
    Check(scene.view.Prepare(0, 0) == S_FALSE, "zero target suspends");
    Check(scene.view.GetStatistics().surfaceBytes == 0, "zero target holds no surface");
    Check(! scene.view.NeedsPreparation() && ! scene.view.NeedsAnimation(), "zero target schedules no work");
    Check(! scene.view.DispatchPointer({DxUi::PointerAction::Down, 45, 84}), "zero target ignores input");
    Hr(scene.view.Prepare(960, 480, 192), "DPI and resize");
    // The surface released by the zero target is replaced by exactly one allocation at the new physical extent.
    Check(scene.view.GetStatistics().surfaceBytes == 960ull * 480 * 4 && scene.view.GetStatistics().surfaceAllocations == shownStats.surfaceAllocations + 2,
          "surface budget accounted after the zero-target release");
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
    Check(DxUi::GetControlCatalog().size() == 27, "catalog contains all 27 controls");
    b.Controls().SetRoot(std::make_unique<DxUi::Toggle>(L"Second view"));
    Hr(b.Prepare(320, 160, 144), "independent second layout");
    Check(a.GetStatistics().surfaceBytes != b.GetStatistics().surfaceBytes, "shared pool has independent surfaces");
    auto progress     = std::make_unique<DxUi::ProgressBar>();
    auto* progressPtr = progress.get();
    progressPtr->SetIndeterminate(true);
    b.Controls().SetRoot(std::move(progress));
    Check(b.NeedsAnimation(), "attaching an indeterminate control schedules initial discovery");
    Hr(b.Prepare(320, 160, 144), "prepare indeterminate progress");
    Check(! b.NeedsPreparation(), "prepared indeterminate progress is clean until it ticks");
    const auto tick = GetTickCount64();
    Check(b.AdvanceAnimation(tick), "indeterminate progress requests another host tick");
    Check(b.NeedsPreparation(), "indeterminate progress dirties every tick");
    Check(b.Prepare(0, 0) == S_FALSE, "zero-sized animation suspends");
    Check(b.GetStatistics().surfaceBytes == 0, "zero-sized view releases its surface");
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
