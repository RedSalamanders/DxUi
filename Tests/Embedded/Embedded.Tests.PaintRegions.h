#pragma once
// Paint regions and culling (plan item W4.10) on the embedded host. Included once from Embedded.Tests.Embedded.cpp after the
// performance fixtures, the allocation instrument and the incremental-parity scene; uses Check and Hr from there.
//
// - TestFx5HoverRegionGate: FX5's hover frames repaint only around the two toggles that lost and gained hover: one region
//   rectangle per toggle, and in each the root, the toggle's card, the toggle and the slider just below it (which the toggle's
//   3 DIP reach meets) paint, nothing else: the gate restated for a region design (changed controls, their ancestors and the
//   neighbours their reach meets, under each rectangle). Prints the C++ allocations of a hover frame with regions on and off
//   (G3).
// - TestFx5RegionFramesKeepCaches: region frames keep the entries of the controls outside the region (text layouts, formats,
//   brushes): a whole frame after many hover frames creates nothing.
// - TestFx12ScrollPaintsVisibleChildren: FX12 form-10k and form-layout-10k scrolled: child paints <= the controls in the
//   viewport plus one row.
// - TestEnclosedClipIsNotPushed: a region rectangle inside a ScrollPanel's viewport: the ScrollPanel pushes no clip.
// - TestUndeclaredControlsRepaintTheWholeView: an undeclared consumer control, and a library control below an undeclared
//   consumer panel, repaint the whole view; a declared one repaints its region; text that escapes its control does too.
// - TestDeclaredReachCoversEveryPixel: every library control kind, painted alone in each of its states and after each
//   transition that shrinks its reach, draws no pixel outside its bounds plus declared reach, snapped outward.
// - TestRegionParityScripts: the regions scripts of the oracle (Support.Tests.PaintRegions.h) at 96, 144 and 192 DPI, with
//   a check that the incremental scene really painted partial frames.
#include "Controls/DxUi.Internal.h"
#include "Support.Tests.PaintReach.h"
#include "Support.Tests.PaintRegions.h"
#include "Support/RetainedCaches.Tests.Hooks.h"

#include <set>

#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

namespace PaintRegionTests
{
// Diagnostics: the call stack that made a paint region whole (installed while a gate expects regions).
inline void PrintFullRegionStack() noexcept
{
    static bool initialized = false;
    static int printed      = 0;
    if (++printed > 60)
        return;
    if (! initialized)
        initialized = SymInitialize(GetCurrentProcess(), nullptr, TRUE) != FALSE;
    std::array<void*, 12u> frames{};
    const USHORT count = CaptureStackBackTrace(1u, static_cast<DWORD>(frames.size()), frames.data(), nullptr);
    std::cerr << "  region made whole by:";
    for (USHORT index = 0u; index < count; ++index)
    {
        alignas(SYMBOL_INFO) std::array<char, sizeof(SYMBOL_INFO) + 256u> buffer{};
        auto* symbol         = reinterpret_cast<SYMBOL_INFO*>(buffer.data());
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen   = 255u;
        DWORD64 displacement = 0u;
        if (SymFromAddr(GetCurrentProcess(), reinterpret_cast<DWORD64>(frames[index]), &displacement, symbol))
            std::cerr << " <- " << symbol->Name;
    }
    std::cerr << '\n' << std::flush;
}
// The controls each frame's Panels dispatched Paint to.
struct PaintLog final
{
    std::vector<const DxUi::Control*> painted;
    static void Observe(const DxUi::Control& control, void* context) noexcept
    {
        auto* log = static_cast<PaintLog*>(context);
        if (log->painted.size() < log->painted.capacity())
            log->painted.push_back(&control);
    }
};
class PaintLogScope final
{
public:
    explicit PaintLogScope(PaintLog& log) noexcept
    {
        DxUi::Detail::SetPaintObserverForTest(&PaintLog::Observe, &log);
    }
    ~PaintLogScope()
    {
        DxUi::Detail::SetPaintObserverForTest(nullptr, nullptr);
    }
    PaintLogScope(const PaintLogScope&)            = delete;
    PaintLogScope& operator=(const PaintLogScope&) = delete;
};
class RegionsSwitch final
{
public:
    explicit RegionsSwitch(bool enabled) noexcept
    {
        DxUi::Detail::SetPaintRegionsEnabledForTest(enabled);
    }
    ~RegionsSwitch()
    {
        DxUi::Detail::SetPaintRegionsEnabledForTest(true);
    }
    RegionsSwitch(const RegionsSwitch&)            = delete;
    RegionsSwitch& operator=(const RegionsSwitch&) = delete;
};

[[nodiscard]] inline bool IsDescendantOf(const DxUi::Control& control, const DxUi::Control& ancestor)
{
    for (size_t index = 0u; index < ancestor.GetLogicalChildCount(); ++index)
        if (const DxUi::Control* child = ancestor.GetLogicalChild(index); child && (child == &control || IsDescendantOf(control, *child)))
            return true;
    return false;
}

// One FX5 run through its warm, clean and dirty phases, then its hover phase with every frame checked. Returns the C++
// allocations of the hover frames.
[[nodiscard]] inline uint64_t RunFx5Hover(bool regions, uint64_t& hoverFrames)
{
    using namespace PerformanceFixtures;
    const RegionsSwitch regionsSwitch(regions);
    const Definition& definition = Fx5::kDefinition;
    const Variant& variant       = Fx5::kVariants[0];
    GraphicsFixture gpu;
    gpu.width  = variant.width;
    gpu.height = variant.height;
    Hr(gpu.Create(true), "W4.10 FX5 device");
    EmbeddedInstance instance;
    instance.Instantiate(definition);
    Hr(instance.Create(gpu.device.get(), variant), "W4.10 FX5 build");
    uint64_t hoverAllocations = 0u;
    hoverFrames               = 0u;
    PaintLog log;
    log.painted.reserve(4096u);
    const DxUi::Control* root = instance.View().Controls().GetRoot();
    for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
        for (size_t step = 0u; step < definition.phases[phase].steps; ++step)
        {
            const bool hover = definition.phases[phase].name == "hover";
            DxUi::Detail::SetFullRegionObserverForTest(hover && regions ? &PrintFullRegionStack : nullptr);
            const auto clearObserver = wil::scope_exit([] { DxUi::Detail::SetFullRegionObserverForTest(nullptr); });
            log.painted.clear();
            const PaintLogScope observing(log);
            const DxUi::Detail::WorkCounterScope scope;
            allocations      = 0u;
            countAllocations = hover;
            instance.Step(phase, step);
            Hr(instance.Frame(gpu), "W4.10 FX5 frame");
            countAllocations                      = false;
            const DxUi::Detail::WorkCounters work = scope.Delta();
            if (! hover)
                continue;
            ++hoverFrames;
            hoverAllocations += allocations;
            if (! regions)
                continue;
            // The toggles that lost and gained hover: the leaves this frame painted.
            std::vector<const DxUi::Control*> leaves;
            for (const DxUi::Control* control : log.painted)
                if (control->GetLogicalChildCount() == 0u)
                    leaves.push_back(control);
            std::set<const DxUi::Control*> distinctLeaves(leaves.begin(), leaves.end());
            const uint64_t rectangles = work.frame.paintRegionRects;
            if (step == 0u)
                for (const DxUi::Control* control : log.painted)
                {
                    const D2D1_RECT_F b = control->GetBounds();
                    std::cout << "    painted " << typeid(*control).name()
                              << std::format(" [{:.1f}, {:.1f}, {:.1f}, {:.1f}]\n", b.left, b.top, b.right, b.bottom);
                }
            std::cout << "  FX5 hover step " << step << ": " << rectangles << " region rectangles, " << work.frame.paintExecutions << " Paint executions ("
                      << leaves.size() << " leaves), " << work.frame.paintCulled << " culled, " << work.frame.pixelsPainted << " pixels, " << allocations
                      << " C++ allocations\n";
            if (work.frame.paintRegionFull != 0u)
                std::cerr << "  FX5 hover step " << step << " repainted the whole view: invalidations " << work.frame.invalidations << ", tooltip marks "
                          << work.frame.tooltipTargetMarks << ", tooltip validations " << work.frame.tooltipTargetValidations << ", preparations "
                          << work.frame.preparations << ", ink escapes " << work.frame.inkEscapes << ", fresh " << DxUi::IsFreshPreparationForTest()
                          << ", regions " << DxUi::Detail::PaintRegionsEnabled() << ", tooltip " << instance.View().Controls().HasTooltip() << '\n'
                          << std::flush;
            Check(work.frame.preparations == 1u && work.frame.paintRegionFull == 0u, "W4.10: an FX5 hover frame repaints its region, not the whole view");
            Check(rectangles >= 1u && rectangles <= 2u, "W4.10: an FX5 hover frame has one rectangle per toggle that changed hover");
            // The leaves: each changed toggle once, and the slider just below it in its card, whose extent the toggle's reach
            // (its focus ring, 2 DIP below the toggle, over the slider's top edge) meets.
            size_t toggles = 0u, sliders = 0u;
            for (const DxUi::Control* leaf : leaves)
            {
                toggles += dynamic_cast<const DxUi::Toggle*>(leaf) != nullptr ? 1u : 0u;
                sliders += dynamic_cast<const DxUi::Slider*>(leaf) != nullptr ? 1u : 0u;
            }
            Check(toggles == rectangles && sliders == rectangles && leaves.size() == toggles + sliders && distinctLeaves.size() == leaves.size(),
                  "W4.10: the leaves an FX5 hover frame paints are each changed toggle and the slider its reach meets, once each");
            // In each rectangle: the root (dispatched by the host), the toggle's card, the toggle and the slider below it.
            Check(work.frame.paintExecutions == 4u * rectangles,
                  "W4.10 (G2 as restated): Paint executions equal the changed controls, their ancestors and the neighbours their reach "
                  "meets, under each rectangle");
            for (const DxUi::Control* control : log.painted)
                Check(control->GetLogicalChildCount() == 0u ||
                          (dynamic_cast<const DxUi::CardPanel*>(control) != nullptr && root && IsDescendantOf(*control, *root)),
                      "W4.10: the containers painted are the changed toggles' cards");
        }
    return hoverAllocations;
}

__declspec(noinline) inline void TestFx5HoverRegionGate()
{
    uint64_t framesOn = 0u, framesOff = 0u;
    const uint64_t on  = RunFx5Hover(true, framesOn);
    const uint64_t off = RunFx5Hover(false, framesOff);
    Check(framesOn == PerformanceFixtures::Fx5::kHoverSteps && framesOff == framesOn, "FX5 records its hover frames");
    std::cout << "  FX5 hover C++ allocations per frame (G3): " << static_cast<double>(on) / static_cast<double>(framesOn) << " with regions, "
              << static_cast<double>(off) / static_cast<double>(framesOff) << " with regions off\n";
    std::cout << "PASS W4.10 FX5 hover: per changed toggle the root, its card, the toggle and the slider its reach meets "
                 "(4 Paint executions per rectangle; G2's literal <= 2 is not met)\n";
}

__declspec(noinline) inline void TestFx5RegionFramesKeepCaches()
{
    using namespace PerformanceFixtures;
    const Definition& definition = Fx5::kDefinition;
    const Variant& variant       = Fx5::kVariants[0];
    GraphicsFixture gpu;
    gpu.width  = variant.width;
    gpu.height = variant.height;
    Hr(gpu.Create(true), "W4.10 FX5 cache device");
    EmbeddedInstance instance;
    instance.Instantiate(definition);
    Hr(instance.Create(gpu.device.get(), variant), "W4.10 FX5 cache build");
    // Settle: every phase once, then one whole frame, so every string, format and brush of the scene is retained.
    for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
        for (size_t step = 0u; step < definition.phases[phase].steps; ++step)
        {
            instance.Step(phase, step);
            Hr(instance.Frame(gpu), "W4.10 FX5 cache settle");
        }
    instance.View().Controls().Invalidate();
    Hr(instance.Frame(gpu), "W4.10 FX5 whole frame");
    size_t hoverPhase = 0u;
    for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
        if (definition.phases[phase].name == "hover")
            hoverPhase = phase;
    uint64_t regionFrames = 0u;
    for (size_t step = 0u; step < 3u * Fx5::kHoverSteps; ++step)
    {
        const DxUi::Detail::WorkCounterScope scope;
        instance.Step(hoverPhase, step);
        Hr(instance.Frame(gpu), "W4.10 FX5 region frame");
        regionFrames += scope.Delta().frame.paintRegionRects != 0u ? 1u : 0u;
    }
    Check(regionFrames == 3u * Fx5::kHoverSteps, "W4.10: every hover frame paints a region");
    instance.View().Controls().Invalidate();
    const DxUi::Detail::WorkCounterScope scope;
    Hr(instance.Frame(gpu), "W4.10 FX5 whole frame after regions");
    const DxUi::Detail::WorkCounters work = scope.Delta();
    std::cout << "  FX5 whole frame after " << regionFrames << " region frames: " << work.resources.textLayout.created << " text layouts, "
              << DxUi::Detail::TotalCreated(work) << " resources created, " << work.text.textLayoutCacheMisses << " table misses\n";
    Check(work.frame.paintRegionFull == 1u, "the frame after ControlHost::Invalidate is whole");
    Check(work.resources.textLayout.created == 0u && work.text.textLayoutCacheMisses == 0u && DxUi::Detail::TotalCreated(work) == 0u,
          "W4.10: region frames release no text layout, format or brush of the controls outside them");
    std::cout << "PASS W4.10 region frames keep the caches of the controls outside them\n";
}

// The controls below `scroll` whose bounds (plus the library reach) meet its viewport.
inline void CountVisible(const DxUi::Control& control, const D2D1_RECT_F& viewContent, size_t& visible)
{
    for (size_t index = 0u; index < control.GetLogicalChildCount(); ++index)
    {
        const DxUi::Control* child = control.GetLogicalChild(index);
        if (! child || ! child->IsVisible())
            continue;
        const D2D1_RECT_F b = child->GetBounds();
        const float reach   = DxUi::kLibraryPaintReachDip;
        if (b.left - reach < viewContent.right && viewContent.left < b.right + reach && b.top - reach < viewContent.bottom &&
            viewContent.top < b.bottom + reach)
            ++visible;
        CountVisible(*child, viewContent, visible);
    }
}

__declspec(noinline) inline void TestFx12ScrollPaintsVisibleChildren()
{
    using namespace PerformanceFixtures;
    const Definition& definition = Fx12::kDefinition;
    for (const std::string_view name : {std::string_view("form-10k"), std::string_view("form-layout-10k")})
    {
        const Variant* variant = FindVariant(definition, name);
        Check(variant != nullptr, "FX12 has the form variant");
        GraphicsFixture gpu;
        gpu.width  = variant->width;
        gpu.height = variant->height;
        Hr(gpu.Create(true), "W4.10 FX12 device");
        EmbeddedInstance instance;
        instance.Instantiate(definition);
        Hr(instance.Create(gpu.device.get(), *variant), "W4.10 FX12 build");
        const DxUi::Control* root = instance.View().Controls().GetRoot();
        const auto* scroll        = root ? dynamic_cast<const DxUi::ScrollPanel*>(root->GetLogicalChild(0u)) : nullptr;
        Check(scroll != nullptr, "FX12's form is a ScrollPanel");
        PaintLog log;
        log.painted.reserve(32768u);
        size_t scrollFrames = 0u, worst = 0u, worstVisible = 0u;
        for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
            for (size_t step = 0u; step < definition.phases[phase].steps; ++step)
            {
                log.painted.clear();
                const PaintLogScope observing(log);
                const DxUi::Detail::WorkCounterScope scope;
                instance.Step(phase, step);
                Hr(instance.Frame(gpu), "W4.10 FX12 frame");
                const DxUi::Detail::WorkCounters work = scope.Delta();
                if (definition.phases[phase].name != "scroll" || work.frame.preparations == 0u)
                    continue;
                ++scrollFrames;
                // Children below the ScrollPanel this frame painted, against the controls its viewport shows plus one row.
                size_t childPaints = 0u;
                for (const DxUi::Control* control : log.painted)
                    childPaints += IsDescendantOf(*control, *scroll) ? 1u : 0u;
                D2D1_RECT_F viewport = scroll->GetViewportRect();
                viewport.top += scroll->GetScrollOffset();
                viewport.bottom += scroll->GetScrollOffset();
                size_t visible = 0u;
                CountVisible(*scroll, viewport, visible);
                const size_t oneRow = name == "form-10k" ? Fx12::kFormRowControls : Fx12::kFormRowControls + 1u; // a laid-out row is a FlexPanel too
                Check(childPaints <= visible + oneRow, "W4.10: FX12 scrolled: child paints <= visible children + one row");
                if (childPaints > worst)
                {
                    worst        = childPaints;
                    worstVisible = visible;
                }
            }
        Check(scrollFrames > 0u, "FX12 scrolls");
        std::cout << "  FX12 " << name << " scrolled: at most " << worst << " child paints for " << worstVisible
                  << " children in the viewport (of 10,000 controls) over " << scrollFrames << " frames\n";
    }
    std::cout << "PASS W4.10 FX12 scrolled: child paints <= visible children + one row\n";
}

// A probe view of its own on the given device.
struct SmallView final
{
    DxUi::EmbeddedHost view;
    UINT width  = 480u;
    UINT height = 320u;
    explicit SmallView(GraphicsFixture& gpu)
    {
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "W4.10 view graphics");
        Hr(view.Attach(std::move(graphics), StoppedTestClock()), "W4.10 view");
        view.Controls().SetTheme(IncrementalParity::ParityTheme(false));
    }
    ~SmallView()
    {
        view.Detach();
    }
    SmallView(const SmallView&)            = delete;
    SmallView& operator=(const SmallView&) = delete;
    DxUi::Detail::WorkCounters Frame(float dpi = 96.0f)
    {
        const DxUi::Detail::WorkCounterScope scope;
        for (int turn = 0; turn < 8; ++turn)
        {
            const HRESULT prepared = view.Prepare(width, height, dpi);
            Check(SUCCEEDED(prepared) || prepared == E_ABORT, "W4.10 probe view preparation");
            if (! view.NeedsPreparation())
                break;
        }
        return scope.Delta();
    }
};

__declspec(noinline) inline void TestEnclosedClipIsNotPushed(GraphicsFixture& gpu)
{
    SmallView probe(gpu);
    auto root  = std::make_unique<DxUi::Panel>();
    auto* rows = root->AddChild<DxUi::ScrollPanel>();
    rows->SetBounds(D2D1::RectF(20, 20, 300, 300));
    std::vector<DxUi::Label*> labels;
    for (int index = 0; index < 40; ++index)
    {
        auto* label = rows->AddChild<DxUi::Label>(std::format(L"Row {}", index));
        label->SetBounds(D2D1::RectF(30, 24.0f + 28.0f * static_cast<float>(index), 200, 48.0f + 28.0f * static_cast<float>(index)));
        labels.push_back(label);
    }
    rows->SetContentHeight(28.0f * 40.0f + 8.0f);
    probe.view.Controls().SetRoot(std::move(root));
    const DxUi::Detail::WorkCounters first = probe.Frame();
    Check(first.frame.paintRegionFull == 1u && first.frame.clipPushesSkipped == 0u, "the first frame is whole and clips to the viewport");
    Check(first.frame.paintCulled > 0u, "W4.10: a whole frame culls the rows outside the viewport");
    labels[3]->SetText(L"Row three, changed");
    const DxUi::Detail::WorkCounters changed = probe.Frame();
    std::cout << "  enclosed clip: " << changed.frame.paintRegionRects << " rectangle, " << changed.frame.clipPushesSkipped << " inner clips skipped, "
              << changed.frame.paintExecutions << " paints, " << changed.frame.paintCulled << " culled\n";
    Check(changed.frame.paintRegionRects == 1u && changed.frame.paintRegionFull == 0u, "the changed row repaints a region");
    Check(changed.frame.clipPushesSkipped == 1u, "W4.10: the ScrollPanel pushes no clip when the region lies inside its viewport (0 inner pushes)");
    // The root, the ScrollPanel and the changed row; at most its neighbours touching the snapped rectangle.
    Check(changed.frame.paintExecutions <= 5u, "W4.10: the region paints the changed row and its ancestors");
    std::cout << "PASS W4.10 enclosed clips emit no inner PushAxisAlignedClip\n";
}

__declspec(noinline) inline void TestUndeclaredControlsRepaintTheWholeView(GraphicsFixture& gpu)
{
    using PaintRegionScript::HaloButton;
    SmallView probe(gpu);
    auto root        = std::make_unique<DxUi::Panel>();
    auto* undeclared = root->AddChild<HaloButton>(L"Undeclared");
    undeclared->SetBounds(D2D1::RectF(20, 20, 140, 50));
    auto* declared = root->AddChild<PaintRegionScript::DeclaredHaloButton>(L"Declared");
    declared->SetBounds(D2D1::RectF(20, 80, 140, 110));
    auto* shifted = root->AddChild<PaintRegionScript::ShiftingPanel>();
    shifted->SetBounds(D2D1::RectF(200, 20, 400, 120));
    auto* inside = shifted->AddChild<DxUi::Button>(L"Inside");
    inside->SetBounds(D2D1::RectF(210, 30, 330, 60));
    auto* plain = root->AddChild<DxUi::Button>(L"Plain");
    plain->SetBounds(D2D1::RectF(20, 140, 140, 170));
    auto* escaping = root->AddChild<DxUi::Label>(L"Fits");
    escaping->SetMultiline(true);
    escaping->SetBounds(D2D1::RectF(200, 140, 260, 164));
    probe.view.Controls().SetRoot(std::move(root));
    static_cast<void>(probe.Frame());

    const auto expect = [&](std::string_view what, bool whole)
    {
        const DxUi::Detail::WorkCounters work = probe.Frame();
        std::cout << "  " << what << ": " << (work.frame.paintRegionFull != 0u ? "whole view" : "region") << ", " << work.frame.paintRegionRects
                  << " rectangles\n";
        // A preparation in which text newly escapes paints its rectangles, then the whole view.
        Check(work.frame.preparations == 1u && (work.frame.paintRegionFull != 0u) == whole && (whole || work.frame.paintRegionRects != 0u),
              std::string(what).c_str());
    };
    plain->SetText(L"Plain 2");
    expect("a library control repaints its region", false);
    undeclared->SetHalo(true);
    expect("W4.10: an undeclared consumer control repaints the whole view", true);
    declared->SetHalo(true);
    expect("W4.10: a declared consumer control repaints its region", false);
    declared->SetHalo(false);
    expect("W4.10: a declared consumer control's shrinking reach repaints its region", false);
    inside->SetText(L"Inside 2");
    expect("W4.10: a library control below an undeclared consumer panel repaints the whole view", true);
    {
        const DxUi::Detail::WorkCounterScope scope;
        escaping->SetText(L"A caption far wider than its narrow box, escaping it");
        expect("W4.10: text that newly escapes its control repaints the whole view in the same preparation", true);
        Check(scope.Delta().frame.inkEscapes == 1u, "W4.10: the escape is counted once");
    }
    escaping->SetText(L"Fits");
    expect("W4.10: a control whose ink escaped repaints the whole view when it changes", true);
    plain->SetText(L"Plain 3");
    expect("W4.10: once it fits again, regions resume", false);
    std::cout << "PASS W4.10 undeclared, declared and escaping controls\n";
}

// The verifier of declared reach: every library control class alone on a transparent surface, in each state and at the sizes
// that maximise its reach, at 96, 120, 144, 168 and 192 DPI. Every inked pixel lies inside the extent a panel culls it by
// (PaintExtentAccess::Extent: its bounds plus its reach, a panel's with its children), snapped outward. A control whose
// extent is unbounded in a state (an open ComboBox popup, text that escaped) has nothing to bound and is counted apart.
// Not covered here: TooltipLayer (the host's own layer, never in a tree; the host repaints the whole view for every
// tooltip change), PageHost during a transition (unbounded by declaration) and the ComboBox popup and Menu popups (W2.4).
__declspec(noinline) inline void TestDeclaredReachCoversEveryPixel(GraphicsFixture& gpu)
{
    // Tree and Grid borrow a model the probe keeps alive.
    using Add = DxUi::Control* (*)(DxUi::Panel&, std::vector<std::shared_ptr<void>>&);
    struct Case final
    {
        const char* name;
        Add add;
        D2D1_SIZE_F size;
    };
    const std::vector<Case> cases{
        {"Label", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::Label>(L"Label text gjpqy"); }, {120, 24}},
        {"Label (wrapped, overflowing)",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* label = r.AddChild<DxUi::Label>(L"A caption far wider than its narrow box, escaping it");
        label->SetMultiline(true);
        return label;
    },
         {60, 20}},
        {"Separator", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::Separator>(); }, {160, 8}},
        {"Button", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::Button>(L"Apply"); }, {100, 32}},
        {"Button (short)", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::Button>(L"Apply"); }, {40, 12}},
        {"Toggle", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::Toggle>(L"Sync"); }, {140, 28}},
        {"Toggle (short)", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::Toggle>(L"Sync"); }, {40, 12}},
        {"Checkbox", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::Checkbox>(L"Check"); }, {140, 28}},
        {"Checkbox (indeterminate, short)",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* c = r.AddChild<DxUi::Checkbox>(L"Check");
        c->SetIndeterminate(true);
        return c;
    },
         {40, 12}},
        {"RadioButton", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::RadioButton>(L"Radio"); }, {140, 28}},
        {"RadioButtons",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* group = r.AddChild<DxUi::RadioButtons>();
        group->SetHeader(L"Header");
        group->AddItem(L"One");
        group->AddItem(L"Two");
        group->SetSelectedIndex(1);
        return group;
    },
         {160, 90}},
        {"ProgressBar",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* bar = r.AddChild<DxUi::ProgressBar>();
        bar->SetValue(60.0);
        return bar;
    },
         {160, 16}},
        {"ProgressBar (indeterminate, short)",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* bar = r.AddChild<DxUi::ProgressBar>();
        bar->SetIndeterminate(true);
        return bar;
    },
         {60, 4}},
        {"PageIndicator (5)",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* dots = r.AddChild<DxUi::PageIndicator>();
        dots->SetPageCount(5u);
        return dots;
    },
         {120, 20}},
        {"PageIndicator (16 in 120)",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* dots = r.AddChild<DxUi::PageIndicator>();
        dots->SetPageCount(16u);
        dots->SetSelectedIndex(15u);
        return dots;
    },
         {120, 20}},
        {"PageIndicator (16 in 60, 8 tall)",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* dots = r.AddChild<DxUi::PageIndicator>();
        dots->SetPageCount(16u);
        return dots;
    },
         {60, 8}},
        {"ThroughputGraph", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::ThroughputGraph>(); }, {160, 60}},
        {"Slider",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* slider = r.AddChild<DxUi::Slider>();
        slider->SetValue(0.0);
        return slider;
    },
         {160, 32}},
        {"Slider (12 tall, ticks, value 100)",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* slider = r.AddChild<DxUi::Slider>();
        slider->SetTickMarks({0.0, 50.0, 100.0});
        slider->SetValue(100.0);
        return slider;
    },
         {200, 12}},
        {"Slider (vertical, 10 wide, ticks)",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* slider = r.AddChild<DxUi::Slider>();
        slider->SetOrientation(DxUi::SliderOrientation::Vertical);
        slider->SetTickMarks({0.0, 50.0, 100.0});
        slider->SetValue(50.0);
        return slider;
    },
         {10, 160}},
        {"Toolbar",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* bar = r.AddChild<DxUi::Toolbar>();
        bar->AddChild<DxUi::Button>(L"Tool")->SetBounds(D2D1::RectF(84, 64, 140, 88));
        return bar;
    },
         {200, 32}},
        {"MenuBar",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* bar = r.AddChild<DxUi::MenuBar>();
        bar->SetItems({{L"File", L'F'}, {L"Edit", L'E'}});
        return bar;
    },
         {200, 28}},
        {"TabControl (8 tabs in 160)",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* tabs = r.AddChild<DxUi::TabControl>();
        for (int index = 0; index < 8; ++index)
            tabs->AddTab<DxUi::Label>(std::format(L"Tab title {}", index), L"Page");
        return tabs;
    },
         {160, 100}},
        {"ColorSwatch", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::ColorSwatch>(0xFF3366CCu); }, {32, 32}},
        {"RichTextEditor", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::RichTextEditor>(L"Rich text in a box"); }, {160, 60}},
        {"TextField", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::TextField>(L"Field text"); }, {160, 32}},
        {"TextField (short)", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::TextField>(L"Field text"); }, {60, 12}},
        {"ComboBox",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* combo = r.AddChild<DxUi::ComboBox>();
        combo->SetItems({{L"a", L"Alpha"}, {L"b", L"Beta"}});
        combo->SetSelectedIndex(0u);
        return combo;
    },
         {160, 32}},
        {"TagPicker",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* tags = r.AddChild<DxUi::TagPicker>();
        tags->SetOptions(L"All", {L"Red", L"Green", L"Blue"});
        return tags;
    },
         {200, 32}},
        {"StatusStrip", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::StatusStrip>(L"Ready"); }, {200, 24}},
        {"Splitter", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::Splitter>(); }, {8, 100}},
        {"NumericStepper", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::NumericStepper>(); }, {140, 32}},
        {"ColorPicker", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::ColorPicker>(); }, {316, 236}},
        {"ColorPicker (200x160)", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::ColorPicker>(); }, {200, 160}},
        {"Tree",
         [](DxUi::Panel& r, std::vector<std::shared_ptr<void>>& models) -> DxUi::Control*
    {
        auto model = std::make_shared<IncrementalParity::ParityTreeModel>();
        auto* tree = r.AddChild<DxUi::Tree>();
        tree->SetModel(model.get());
        models.push_back(std::move(model));
        return tree;
    },
         {160, 100}},
        {"Grid",
         [](DxUi::Panel& r, std::vector<std::shared_ptr<void>>& models) -> DxUi::Control*
    {
        auto model = std::make_shared<IncrementalParity::ParityGridModel>();
        auto* grid = r.AddChild<DxUi::Grid>();
        grid->SetModel(model.get());
        models.push_back(std::move(model));
        return grid;
    },
         {200, 100}},
        {"CardPanel", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::CardPanel>(); }, {160, 80}},
        {"FormField", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::FormField>(L"name", L"Name"); }, {200, 32}},
        {"Panel (a Button child)",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* panel = r.AddChild<DxUi::Panel>();
        panel->AddChild<DxUi::Button>(L"Child")->SetBounds(D2D1::RectF(90, 70, 190, 102));
        return panel;
    },
         {160, 60}},
        {"StackPanel", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::StackPanel>(); }, {160, 60}},
        {"FlexPanel", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::FlexPanel>(); }, {160, 60}},
        {"FormPanel", [](DxUi::Panel& r, auto&) -> DxUi::Control* { return r.AddChild<DxUi::FormPanel>(); }, {160, 60}},
        {"ScrollPanel (rows)",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* scroll = r.AddChild<DxUi::ScrollPanel>();
        for (int index = 0; index < 8; ++index)
            scroll->AddChild<DxUi::Button>(std::format(L"Row {}", index))
                ->SetBounds(D2D1::RectF(84, 64.0f + 30.0f * static_cast<float>(index), 220, 90.0f + 30.0f * static_cast<float>(index)));
        scroll->SetContentHeight(260.0f);
        return scroll;
    },
         {160, 80}},
        {"PopupLayer (a Button child)",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* layer = r.AddChild<DxUi::PopupLayer>();
        layer->AddChild<DxUi::Button>(L"Popup")->SetBounds(D2D1::RectF(90, 70, 190, 102));
        return layer;
    },
         {160, 60}},
        {"PageHost",
         [](DxUi::Panel& r, auto&) -> DxUi::Control*
    {
        auto* host = r.AddChild<DxUi::PageHost>();
        return host;
    },
         {160, 60}},
    };
    enum class State : uint8_t
    {
        Normal,
        Hovered,
        Pressed,
        FocusVisible,
        FocusLost,
        Disabled,
    };
    constexpr std::array<std::pair<State, const char*>, 6u> states{{{State::Normal, "normal"},
                                                                    {State::Hovered, "hovered"},
                                                                    {State::Pressed, "pressed"},
                                                                    {State::FocusVisible, "focused, focus visible"},
                                                                    {State::FocusLost, "after the focus ring left"},
                                                                    {State::Disabled, "disabled"}}};
    size_t checked = 0u, unbounded = 0u;
    for (const Case& kind : cases)
    {
        float worst = -1000.0f;
        std::string worstWhere;
        for (const float dpi : {96.0f, 120.0f, 144.0f, 168.0f, 192.0f})
            for (const auto& [state, stateName] : states)
            {
                std::vector<std::shared_ptr<void>> models;
                SmallView probe(gpu);
                probe.width            = static_cast<UINT>(std::lround(480.0f * dpi / 96.0f));
                probe.height           = static_cast<UINT>(std::lround(360.0f * dpi / 96.0f));
                auto root              = std::make_unique<DxUi::Panel>();
                DxUi::Control* control = kind.add(*root, models);
                // A fractional origin, so the fractional scales put the edges between pixels.
                const D2D1_RECT_F bounds = D2D1::RectF(140.3f, 100.6f, 140.3f + kind.size.width, 100.6f + kind.size.height);
                control->SetBounds(bounds);
                probe.view.Controls().SetRoot(std::move(root));
                static_cast<void>(probe.Frame(dpi));
                const auto point   = D2D1::Point2F((bounds.left + bounds.right) * 0.5f * dpi / 96.0f, (bounds.top + bounds.bottom) * 0.5f * dpi / 96.0f);
                const auto pointer = [&](DxUi::PointerAction action)
                {
                    DxUi::PointerEvent event{action, std::round(point.x), std::round(point.y)};
                    static_cast<void>(probe.view.DispatchPointer(event));
                };
                switch (state)
                {
                    case State::Normal: break;
                    case State::Hovered: pointer(DxUi::PointerAction::Move); break;
                    case State::Pressed:
                        pointer(DxUi::PointerAction::Move);
                        pointer(DxUi::PointerAction::Down);
                        break;
                    case State::FocusVisible:
                    case State::FocusLost:
                        probe.view.Controls().SetFocusControl(control, false);
                        static_cast<void>(probe.view.DispatchKey(VK_SHIFT, true));
                        static_cast<void>(probe.view.DispatchKey(VK_SHIFT, false));
                        if (state == State::FocusLost)
                        {
                            static_cast<void>(probe.Frame(dpi));
                            probe.view.Controls().SetFocusControl(nullptr, false);
                        }
                        break;
                    case State::Disabled: control->SetEnabled(false); break;
                }
                // A tooltip (a Tree's overflowing row) is the host's own layer, not the control's ink.
                static_cast<void>(probe.view.Controls().ClearTooltip());
                static_cast<void>(probe.Frame(dpi));
                DxUi::WindowHostBitmapCapture surface;
                Hr(probe.view.DebugReadSurfaceForTest(surface), "W4.10 verifier surface");
                if (state == State::Pressed)
                    pointer(DxUi::PointerAction::Up);
                D2D1_RECT_F extent{};
                if (! DxUi::PaintExtentAccess::Extent(probe.view.Controls(), *control, extent))
                {
                    ++unbounded; // nothing to bound: every invalidation of it repaints the whole view
                    continue;
                }
                DxUi::PaintOverhang drawn{};
                if (PaintReach::MeasureInkReach(surface, bounds, dpi, drawn))
                {
                    const float reach = (std::max)((std::max)(drawn.left, drawn.top), (std::max)(drawn.right, drawn.bottom));
                    if (reach > worst)
                    {
                        worst      = reach;
                        worstWhere = std::format("{} at {:.0f} DPI (left {:.2f} top {:.2f} right {:.2f} bottom {:.2f})",
                                                 stateName,
                                                 dpi,
                                                 drawn.left,
                                                 drawn.top,
                                                 drawn.right,
                                                 drawn.bottom);
                    }
                }
                if (const std::string where = PaintReach::InkOutsideExtent(surface, extent, dpi); ! where.empty())
                {
                    std::cerr << "  " << kind.name << ' ' << stateName << " at " << dpi << " DPI: " << where
                              << std::format(" (ink reaches left {:.2f} top {:.2f} right {:.2f} bottom {:.2f} DIP; extent [{:.2f}, {:.2f}, {:.2f}, {:.2f}])",
                                             drawn.left,
                                             drawn.top,
                                             drawn.right,
                                             drawn.bottom,
                                             extent.left,
                                             extent.top,
                                             extent.right,
                                             extent.bottom)
                              << '\n';
                    Check(false, "W4.10: a library control draws only within its painted extent (bounds plus declared reach)");
                }
                ++checked;
            }
        std::cout << "  reach " << kind.name << ": at most " << std::format("{:.2f}", worst) << " DIP outside its bounds, " << worstWhere << '\n';
    }
    std::cout << "PASS W4.10 declared reach covers every pixel: " << checked << " cases of " << cases.size()
              << " control kinds at 96, 120, 144, 168 and 192 DPI (" << unbounded << " unbounded cases skipped)\n";
}

// The clip-miss predicate at an extent's edge: an extent that ends exactly where the clip begins misses it, one that reaches
// any part of the clip's first pixel meets it (a cull with the extent short by one device pixel fails here).
__declspec(noinline) inline void TestClipMissAtTheExtentEdge(GraphicsFixture& gpu)
{
    for (const float dpi : {96.0f, 120.0f, 168.0f})
    {
        SmallView probe(gpu);
        probe.view.Controls().SetRoot(std::make_unique<DxUi::Panel>());
        static_cast<void>(probe.Frame(dpi));
        DxUi::ControlHost& host = probe.view.Controls();
        const float pixel       = 96.0f / dpi;
        // The clip is the pixel columns 100 to 120 and rows 100 to 120, as a region rectangle is.
        const D2D1_RECT_F clip = D2D1::RectF(100.0f * pixel, 100.0f * pixel, 120.0f * pixel, 120.0f * pixel);
        const DxUi::PaintExtentAccess::ClipScope restore(host);
        DxUi::PaintExtentAccess::SetClip(host, clip);
        const D2D1_MATRIX_3X2_F identity = D2D1::Matrix3x2F::Identity();
        const auto misses                = [&](float left, float top, float right, float bottom)
        { return DxUi::PaintExtentAccess::MissesClip(host, identity, D2D1::RectF(left, top, right, bottom)); };
        const float tenth = pixel * 0.1f;
        Check(misses(clip.left - 5.0f, clip.top, clip.left, clip.bottom), "W4.10: an extent that ends where the clip begins misses it");
        Check(! misses(clip.left - 5.0f, clip.top, clip.left + tenth, clip.bottom), "W4.10: an extent reaching a tenth into the clip's first pixel meets it");
        Check(! misses(clip.right - tenth, clip.top, clip.right + 5.0f, clip.bottom), "W4.10: an extent beginning in the clip's last pixel meets it");
        Check(misses(clip.right, clip.top, clip.right + 5.0f, clip.bottom), "W4.10: an extent beginning where the clip ends misses it");
        Check(! misses(clip.left, clip.bottom - tenth, clip.right, clip.bottom + 5.0f) && misses(clip.left, clip.bottom, clip.right, clip.bottom + 5.0f),
              "W4.10: the same holds at the clip's bottom edge");
    }
    std::cout << "PASS W4.10 the clip-miss predicate at the extent edge, 96, 120 and 168 DPI\n";
}

// Deterministic oracle scripts (Support.Tests.PaintRegions.h): the EdgeInk's last pixel column under a one-column region at
// 96, 120 and 168 DPI, a declared reach taken away, a moved control's old place, an overfull PageIndicator and a 12 DIP
// Slider. Each frame after the first is a region frame, compared with a fresh whole repaint.
__declspec(noinline) inline void TestRegionEdgeScripts(GraphicsFixture& gpu)
{
    using EmbeddedParity::EmbeddedParityScene;
    const auto make                   = [&] { return std::make_unique<EmbeddedParityScene>(gpu.device.get()); };
    constexpr std::string_view replay = "DxUi.EmbeddedTests.exe --test-paint-regions";
    const DxUi::Detail::WorkCounterScope scope;
    RunParityScript(PaintRegionScript::EdgeScript<96u, EmbeddedParityScene>(), make, "embedded 96 DPI", replay);
    RunParityScript(PaintRegionScript::EdgeScript<120u, EmbeddedParityScene>(), make, "embedded 120 DPI", replay);
    RunParityScript(PaintRegionScript::EdgeScript<168u, EmbeddedParityScene>(), make, "embedded 168 DPI", replay);
    RunParityScript(PaintRegionScript::OverfullPageIndicatorScript<EmbeddedParityScene>(), make, "embedded", replay);
    RunParityScript(PaintRegionScript::ThinSliderScript<EmbeddedParityScene>(), make, "embedded", replay);
    Check(scope.Delta().frame.paintRegionRects > 0u, "W4.10: the edge scripts compare region frames");
    std::cout << "PASS W4.10 region edges: a one-pixel extent edge, a reach taken away, a moved control, an overfull PageIndicator and a "
                 "12 DIP Slider, 0 differences\n";
}

// An undeclared consumer panel that paints its children into a command list and draws it 200 DIP lower: its child above the
// view must still be drawn. Also a library Panel inside it (declared, but painted through the undeclared parent's list).
class OffsetImagePanel final : public DxUi::Panel
{
public:
    OffsetImagePanel()                                   = default;
    OffsetImagePanel(const OffsetImagePanel&)            = delete;
    OffsetImagePanel& operator=(const OffsetImagePanel&) = delete;
    OffsetImagePanel(OffsetImagePanel&&)                 = delete;
    OffsetImagePanel& operator=(OffsetImagePanel&&)      = delete;
    ~OffsetImagePanel() override                         = default;
    void Paint(DxUi::ControlHost& host) const override
    {
        auto* dc = host.GetDeviceContext();
        if (! dc)
            return;
        wil::com_ptr<ID2D1CommandList> list;
        if (FAILED(dc->CreateCommandList(list.put())))
            return;
        wil::com_ptr<ID2D1Image> previous;
        dc->GetTarget(previous.put());
        dc->SetTarget(list.get());
        DxUi::Panel::Paint(host);
        dc->SetTarget(previous.get());
        if (FAILED(list->Close()))
            return;
        dc->DrawImage(list.get(), D2D1::Point2F(0.0f, 200.0f));
    }
};

__declspec(noinline) inline void TestUndeclaredComposingPanelsCullNothing(GraphicsFixture& gpu)
{
    for (const bool nested : {false, true})
    {
        const auto render = [&](bool regions, uint64_t& culled) -> std::vector<uint8_t>
        {
            const RegionsSwitch regionsSwitch(regions);
            SmallView probe(gpu);
            auto root           = std::make_unique<DxUi::Panel>();
            auto* panel         = root->AddChild<OffsetImagePanel>();
            DxUi::Panel* parent = panel;
            panel->SetBounds(D2D1::RectF(0, 0, 400, 300));
            if (nested)
            {
                parent = panel->AddChild<DxUi::Panel>();
                parent->SetBounds(D2D1::RectF(0, -200, 400, 300));
            }
            parent->AddChild<DxUi::Button>(L"Above the view")->SetBounds(D2D1::RectF(20, -150, 160, -118));
            parent->AddChild<DxUi::Button>(L"In the view")->SetBounds(D2D1::RectF(200, 20, 340, 52));
            probe.view.Controls().SetRoot(std::move(root));
            culled = probe.Frame().frame.paintCulled;
            DxUi::WindowHostBitmapCapture surface;
            Hr(probe.view.DebugReadSurfaceForTest(surface), "W4.10 composing panel surface");
            return std::move(surface.bgraPixels);
        };
        uint64_t culledOn = 0u, culledOff = 0u;
        const std::vector<uint8_t> on  = render(true, culledOn);
        const std::vector<uint8_t> off = render(false, culledOff);
        size_t differing               = 0u;
        for (size_t index = 0u; index + 4u <= on.size() && index + 4u <= off.size(); index += 4u)
            differing += std::memcmp(&on[index], &off[index], 4u) != 0 ? 1u : 0u;
        std::cout << "  undeclared composing panel" << (nested ? " around a library Panel" : "") << ": " << culledOn << " culled, " << differing
                  << " pixels differ from regions off\n";
        Check(on.size() == off.size() && differing == 0u && culledOn == 0u,
              "W4.10: nothing below an undeclared control is culled; it paints exactly as with regions off");
    }
    std::cout << "PASS W4.10 an undeclared panel that paints its children elsewhere loses none of them\n";
}

// A ColorPicker in a box smaller than its parts (200 x 160 DIP, the review's probe): its field, strip and swatches are drawn
// past the box, which its reach covers; its R, G, B and hex rows are laid out in less room than they need and their text
// overflows their boxes, which ink escape detects, so every change repaints the whole view (a known cost, recorded in the
// receipt). The oracle requires the pixels to stay right at 96 and 168 DPI.
template <UINT Dpi> void BuildSmallColorPicker(EmbeddedParity::EmbeddedParityScene& scene)
{
    scene.Host().SetTheme(IncrementalParity::ParityTheme(false));
    auto root    = std::make_unique<DxUi::Panel>();
    auto* picker = root->AddChild<DxUi::ColorPicker>();
    picker->SetColor(0xFF3366CCu);
    picker->SetBounds(D2D1::RectF(100.3f, 80.6f, 300.3f, 240.6f));
    scene.slots[0] = root.get();
    scene.slots[1] = picker;
    scene.SetRoot(std::move(root));
}
template <UINT Dpi> void StepSmallColorPicker(EmbeddedParity::EmbeddedParityScene& scene, IncrementalParity::Rng& rng, size_t step)
{
    if (step == 1u)
    {
        IncrementalParity::NoteStep("dpi");
        scene.SetDpi(Dpi);
        return;
    }
    IncrementalParity::NoteStep("ColorPicker::SetColor");
    static_cast<DxUi::ColorPicker*>(scene.slots[1])->SetColor(0xFF000000u | rng.Below(0xFFFFFFu));
}
inline constexpr std::array<uint64_t, 2u> kSmallColorPickerSeeds{0x04101001u, 0x04101002u};

__declspec(noinline) inline void TestSmallColorPickerStaysRight(GraphicsFixture& gpu)
{
    using EmbeddedParity::EmbeddedParityScene;
    const auto make = [&] { return std::make_unique<EmbeddedParityScene>(gpu.device.get()); };
    const DxUi::Detail::WorkCounterScope scope;
    RunParityScript(
        IncrementalParity::ParityScript<EmbeddedParityScene>{
            "small-color-picker", kSmallColorPickerSeeds, 8u, &BuildSmallColorPicker<96u>, &StepSmallColorPicker<96u>},
        make,
        "embedded 96 DPI",
        "DxUi.EmbeddedTests.exe --test-paint-regions");
    RunParityScript(
        IncrementalParity::ParityScript<EmbeddedParityScene>{
            "small-color-picker", kSmallColorPickerSeeds, 8u, &BuildSmallColorPicker<168u>, &StepSmallColorPicker<168u>},
        make,
        "embedded 168 DPI",
        "DxUi.EmbeddedTests.exe --test-paint-regions");
    const DxUi::Detail::WorkCounters work = scope.Delta();
    std::cout << "  ColorPicker 200 x 160: " << work.frame.paintRegionRects << " region rectangles, " << work.frame.paintRegionFull << " whole repaints, "
              << work.frame.inkEscapes << " ink escapes over the two scripts (incremental and fresh scenes)\n";
    std::cout << "PASS W4.10 a ColorPicker smaller than its parts stays right (its changes repaint the whole view)\n";
}
__declspec(noinline) inline void TestRegionParityScripts(GraphicsFixture& gpu)
{
    using EmbeddedParity::EmbeddedParityScene;
    const auto make = [&] { return std::make_unique<EmbeddedParityScene>(gpu.device.get()); };
    const DxUi::Detail::WorkCounterScope scope;
    RunParityScript(PaintRegionScript::RegionScript<EmbeddedParityScene>(), make, "embedded", "DxUi.EmbeddedTests.exe --test-paint-regions");
    RunParityScript(PaintRegionScript::RegionDpiScript<EmbeddedParityScene>(), make, "embedded", "DxUi.EmbeddedTests.exe --test-paint-regions");
    const DxUi::Detail::WorkCounters work = scope.Delta();
    std::cout << "  regions oracle: " << work.frame.paintRegionRects << " region rectangles and " << work.frame.paintRegionFull << " whole repaints, "
              << work.frame.paintCulled << " children culled, " << work.frame.inkEscapes << " ink escapes\n";
    Check(work.frame.paintRegionRects > 0u && work.frame.paintCulled > 0u && work.frame.inkEscapes > 0u,
          "W4.10: the regions oracle compares partial frames, culled children and escaping text");
    std::cout << "PASS W4.10 regions oracle: 0 differences after every partial frame\n";
}

// The cost of culling (receipt): DxUi.EmbeddedTests.exe --paint-region-cull-bench <frames> [Hardware|Warp]. One view
// (1920x1080, 96 DPI) whose root holds a Label that changes every frame and, in mode "cull", 10,000 Labels below the view's
// region that a frame skips; mode "empty" holds the changing Label alone. Per frame the UI thread's cycles in Prepare
// (QueryThreadCycleTime); the medians' difference over 10,000 is the cycles per culled child. One JSON line per mode.
inline int RunCullBench(int argc, wchar_t** argv)
{
    if (argc < 3)
    {
        std::fputs("--paint-region-cull-bench requires a frame count.\n", stderr);
        return 2;
    }
    const size_t frames      = static_cast<size_t>(std::wcstoull(argv[2], nullptr, 10));
    const bool warp          = argc < 4 || std::wstring_view(argv[3]) != L"Hardware";
    constexpr size_t kCulled = 10'000u;
    GraphicsFixture gpu;
    gpu.width  = 1920u;
    gpu.height = 1080u;
    Hr(gpu.Create(warp), "cull bench device");
    std::array<uint64_t, 2u> medians{};
    for (size_t mode = 0u; mode < 2u; ++mode)
    {
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "cull bench graphics");
        DxUi::EmbeddedHost view;
        Hr(view.Attach(std::move(graphics), StoppedTestClock()), "cull bench view");
        auto root     = std::make_unique<DxUi::Panel>();
        auto* changed = root->AddChild<DxUi::Label>(L"Value 0");
        changed->SetBounds(D2D1::RectF(8, 8, 200, 32));
        if (mode == 0u)
            for (size_t index = 0u; index < kCulled; ++index)
                // 500 distinct strings, within the host's text layout table (beyond its bound a string's ink is unknown and its
                // control is never culled).
                root->AddChild<DxUi::Label>(std::format(L"Row {}", index % 500u))
                    ->SetBounds(D2D1::RectF(8, 60.0f + 2.0f * static_cast<float>(index % 500u), 200, 80.0f + 2.0f * static_cast<float>(index % 500u)));
        view.Controls().SetRoot(std::move(root));
        Hr(view.Prepare(gpu.width, gpu.height), "cull bench first frame");
        std::vector<uint64_t> cycles;
        cycles.reserve(frames);
        uint64_t culled = 0u, rects = 0u;
        for (size_t frame = 0u; frame < frames + 8u; ++frame)
        {
            changed->SetText(frame % 2u == 0u ? L"Value 1" : L"Value 2");
            const DxUi::Detail::WorkCounterScope scope;
            ULONG64 before = 0u, after = 0u;
            QueryThreadCycleTime(GetCurrentThread(), &before);
            Hr(view.Prepare(gpu.width, gpu.height), "cull bench frame");
            QueryThreadCycleTime(GetCurrentThread(), &after);
            gpu.Bind();
            Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "cull bench composition");
            gpu.context->Flush();
            if (frame < 8u)
                continue;
            const DxUi::Detail::WorkCounters work = scope.Delta();
            culled += work.frame.paintCulled;
            rects += work.frame.paintRegionRects;
            cycles.push_back(after - before);
        }
        std::ranges::sort(cycles);
        medians[mode] = cycles[cycles.size() / 2u];
        std::cout << "{\"mode\":\"" << (mode == 0u ? "cull" : "empty") << "\",\"adapter\":\"" << (warp ? "WARP" : "Hardware") << "\",\"frames\":" << frames
                  << ",\"prepareCyclesMedian\":" << medians[mode] << ",\"prepareCyclesP10\":" << cycles[cycles.size() / 10u]
                  << ",\"prepareCyclesP90\":" << cycles[cycles.size() * 9u / 10u] << ",\"culledPerFrame\":" << culled / frames
                  << ",\"regionRectsPerFrame\":" << static_cast<double>(rects) / static_cast<double>(frames) << "}\n";
        view.Detach();
    }
    std::cout << "{\"cyclesPerCulledChild\":" << (static_cast<double>(medians[0]) - static_cast<double>(medians[1])) / static_cast<double>(kCulled) << "}\n";
    return 0;
}

inline void Run(GraphicsFixture& gpu)
{
    TestDeclaredReachCoversEveryPixel(gpu);
    TestFx5HoverRegionGate();
    TestFx5RegionFramesKeepCaches();
    TestFx12ScrollPaintsVisibleChildren();
    TestEnclosedClipIsNotPushed(gpu);
    TestUndeclaredControlsRepaintTheWholeView(gpu);
    TestRegionEdgeScripts(gpu);
    TestClipMissAtTheExtentEdge(gpu);
    TestUndeclaredComposingPanelsCullNothing(gpu);
    TestSmallColorPickerStaysRight(gpu);
    TestRegionParityScripts(gpu);
}
} // namespace PaintRegionTests
