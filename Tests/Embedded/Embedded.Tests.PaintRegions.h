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
    // The ScrollPanel, the changed row and the rows whose extent (3 DIP reach) meets its snapped rectangle: 5, measured.
    Check(changed.frame.paintExecutions == 5u, "W4.10: the region paints the changed row, its ancestors and the rows its rectangle meets");
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
// Tree and Grid borrow a model the probe keeps alive.
using ReachAdd = DxUi::Control* (*)(DxUi::Panel&, std::vector<std::shared_ptr<void>>&);
struct ReachCase final
{
    const char* name;
    ReachAdd add;
    D2D1_SIZE_F size;
};
// Every library control kind at its usual size and at the sizes that maximise its reach (the reach verifier's cases, also
// sampled mid-animation by TestReachMidAnimation).
[[nodiscard]] inline std::vector<ReachCase> ReachCases()
{
    using Case = ReachCase;
    return std::vector<Case>{
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
}

__declspec(noinline) inline void TestDeclaredReachCoversEveryPixel(GraphicsFixture& gpu)
{
    using Case                    = ReachCase;
    const std::vector<Case> cases = ReachCases();
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

// ---------------------------------------------------------------------------------------------------------------------
// The second review's findings (W4.10 round 3), each a test that failed before its fix, and the reach verifier's coverage of
// the states the first one left out (every Button variant and appearance, every TabControl selection with focus, motion
// mid-flight, right-to-left flow and high contrast). The frame verifier (Support.Tests.FrameVerifier.h) checks every region
// and culled frame these tests make as well.
namespace PaintRegionReview
{
using EmbeddedParity::EmbeddedParityScene;
using IncrementalParity::CenterOf;
using IncrementalParity::NoteStep;
using IncrementalParity::ParityPointer;
using IncrementalParity::Rng;
using PaintRegionScript::Patch;
using PaintRegionTests::RegionsSwitch;
using PaintRegionTests::SmallView;

inline constexpr std::array<uint64_t, 1u> kSeed{0x0410'0003u};
inline constexpr std::string_view kReplay = "DxUi.EmbeddedTests.exe --test-paint-regions";

// The number and box of differing BGRA pixels, or an empty string.
[[nodiscard]] inline std::string DiffPixels(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, UINT width)
{
    if (a.size() != b.size())
        return std::format("sizes differ {} {}", a.size(), b.size());
    size_t differing = 0u;
    UINT left = UINT_MAX, top = UINT_MAX, right = 0u, bottom = 0u;
    for (size_t offset = 0u; offset + 4u <= a.size(); offset += 4u)
    {
        if (std::memcmp(&a[offset], &b[offset], 4u) == 0)
            continue;
        const UINT x = static_cast<UINT>((offset / 4u) % width), y = static_cast<UINT>((offset / 4u) / width);
        ++differing;
        left   = (std::min)(left, x);
        top    = (std::min)(top, y);
        right  = (std::max)(right, x);
        bottom = (std::max)(bottom, y);
    }
    if (differing == 0u)
        return {};
    return std::format("{} pixels differ inside ({},{})-({},{})", differing, left, top, right, bottom);
}

// --- E1: a Selector button focused with focus visible; a Patch 3.5 DIP to its right flips (a region frame that culled the
// button, whose ring reaches 4.5 DIP).
inline void BuildSelectorNeighbour(EmbeddedParityScene& scene)
{
    scene.Host().SetTheme(IncrementalParity::ParityTheme(false));
    auto root    = std::make_unique<DxUi::Panel>();
    auto* button = root->AddChild<DxUi::Button>(L"Selector");
    button->SetVariant(DxUi::ButtonVariant::Selector);
    button->SetBounds(D2D1::RectF(100, 100, 220, 132));
    auto* patch = root->AddChild<Patch>();
    patch->SetBounds(D2D1::RectF(223.5f, 100, 260, 132));
    scene.slots[0] = root.get();
    scene.slots[1] = button;
    scene.slots[2] = patch;
    scene.SetRoot(std::move(root));
}
inline void StepFocusThenFlip(EmbeddedParityScene& scene, Rng&, size_t step)
{
    if (step == 1u)
    {
        NoteStep("focus slot 1, focus visible");
        scene.Host().SetFocusControl(scene.slots[1], false);
        scene.Key(VK_SHIFT);
        return;
    }
    NoteStep("the Patch beside it flips");
    static_cast<Patch*>(scene.slots[2])->Flip();
}

// --- H1 / E1 in whole frames: a Selector whose declared 3 DIP extent ended outside the view while its ring reached into it.
inline void BuildSelectorOffView(EmbeddedParityScene& scene)
{
    scene.Host().SetTheme(IncrementalParity::ParityTheme(false));
    auto root    = std::make_unique<DxUi::Panel>();
    auto* button = root->AddChild<DxUi::Button>(L"Selector");
    button->SetVariant(DxUi::ButtonVariant::Selector);
    button->SetBounds(D2D1::RectF(-120, 100, -3.5f, 132));
    auto* patch = root->AddChild<Patch>();
    patch->SetBounds(D2D1::RectF(0.0f, 90.0f, 40.0f, 140.0f));
    scene.slots[0] = root.get();
    scene.slots[1] = button;
    scene.slots[2] = patch;
    scene.SetRoot(std::move(root));
}

// --- E2: a focused TabControl whose one tab title is far wider than the control; a Patch where its ring landed flips.
inline void BuildTabRing(EmbeddedParityScene& scene)
{
    scene.Host().SetTheme(IncrementalParity::ParityTheme(false));
    auto root  = std::make_unique<DxUi::Panel>();
    auto* tabs = root->AddChild<DxUi::TabControl>();
    tabs->AddTab<DxUi::Label>(L"A very long piece of text that cannot fit the box at all WWWWWWWW", L"Page");
    tabs->SetBounds(D2D1::RectF(140.3f, 100.6f, 220.3f, 200.6f));
    auto* patch = root->AddChild<Patch>();
    patch->SetBounds(D2D1::RectF(0.0f, 90.0f, 40.0f, 140.0f));
    scene.slots[0] = root.get();
    scene.slots[1] = tabs;
    scene.slots[2] = patch;
    scene.SetRoot(std::move(root));
}

// --- B1: the pointer moves straight onto a TagPicker's input box (its inner ComboBox) and away: TagPicker::Paint draws its
// whole frame in the hover colours of that ComboBox.
inline void BuildTagPicker(EmbeddedParityScene& scene)
{
    scene.Host().SetTheme(IncrementalParity::ParityTheme(false));
    auto root  = std::make_unique<DxUi::Panel>();
    auto* tags = root->AddChild<DxUi::TagPicker>();
    tags->SetOptions(L"All", {L"Red", L"Green", L"Blue", L"Cyan"});
    tags->SetSelectedValues({L"Red", L"Green"});
    tags->SetBounds(D2D1::RectF(40, 40, 440, 110));
    scene.slots[0] = root.get();
    scene.slots[1] = tags;
    scene.slots[2] = tags->DebugGetEmbeddedCombo();
    scene.SetRoot(std::move(root));
}
inline void StepTagPicker(EmbeddedParityScene& scene, Rng&, size_t step)
{
    if (step % 2u == 1u)
    {
        NoteStep("pointer onto the TagPicker's input box (its inner ComboBox)");
        scene.Pointer(ParityPointer::Move, CenterOf(*scene.slots[2]));
    }
    else
    {
        NoteStep("pointer away from the TagPicker");
        scene.Pointer(ParityPointer::Move, D2D1::Point2F(600.0f, 440.0f));
    }
}
// The same TagPicker inside a ScrollPanel: the inner hover a ScrollPanel tracks.
inline void BuildScrolledTagPicker(EmbeddedParityScene& scene)
{
    scene.Host().SetTheme(IncrementalParity::ParityTheme(false));
    auto root    = std::make_unique<DxUi::Panel>();
    auto* scroll = root->AddChild<DxUi::ScrollPanel>();
    auto* tags   = scroll->AddChild<DxUi::TagPicker>();
    tags->SetOptions(L"All", {L"Red", L"Green", L"Blue", L"Cyan"});
    tags->SetSelectedValues({L"Red"});
    tags->SetBounds(D2D1::RectF(40, 40, 440, 110));
    scroll->SetContentHeight(400.0f);
    scroll->SetBounds(D2D1::RectF(20, 20, 470, 300));
    scene.slots[0] = root.get();
    scene.slots[1] = tags;
    scene.slots[2] = tags->DebugGetEmbeddedCombo();
    scene.SetRoot(std::move(root));
}

// --- B3: a PopupLayer inside a ScrollPanel: its overlay child, below the ScrollPanel's bounds, moves when the panel scrolls.
inline void BuildScrolledPopup(EmbeddedParityScene& scene)
{
    scene.Host().SetTheme(IncrementalParity::ParityTheme(false));
    auto root    = std::make_unique<DxUi::Panel>();
    auto* scroll = root->AddChild<DxUi::ScrollPanel>();
    for (int index = 0; index < 20; ++index)
        scroll->AddChild<DxUi::Button>(std::format(L"Row {}", index))
            ->SetBounds(D2D1::RectF(30, 26.0f + 30.0f * static_cast<float>(index), 200, 52.0f + 30.0f * static_cast<float>(index)));
    auto* layer = scroll->AddChild<DxUi::PopupLayer>();
    layer->SetBounds(D2D1::RectF(40, 180, 260, 300));
    layer->AddChild<DxUi::Button>(L"Popup below the panel")->SetBounds(D2D1::RectF(40, 180, 260, 300));
    scroll->SetContentHeight(30.0f * 20.0f + 8.0f);
    scroll->SetBounds(D2D1::RectF(20, 20, 300, 200));
    scene.slots[0] = root.get();
    scene.slots[1] = scroll;
    scene.slots[2] = layer;
    scene.SetRoot(std::move(root));
}
inline void StepScrolledPopup(EmbeddedParityScene& scene, Rng&, size_t step)
{
    const D2D1_POINT_2F point = D2D1::Point2F(120.0f, 60.0f);
    switch (step % 4u)
    {
        case 1u:
        case 2u:
            NoteStep("wheel over the ScrollPanel (its PopupLayer child moves with the content)");
            scene.Pointer(ParityPointer::Move, point);
            scene.Pointer(ParityPointer::Wheel, point, step % 4u == 1u ? -120.0f : 120.0f);
            break;
        case 3u:
            NoteStep("ScrollPanel::SetScrollOffset");
            static_cast<DxUi::ScrollPanel*>(scene.slots[1])->SetScrollOffset(45.5f);
            break;
        default:
            NoteStep("the PopupLayer hides and shows");
            scene.slots[2]->SetVisible(! scene.slots[2]->IsVisible());
            break;
    }
}

__declspec(noinline) inline void TestReviewScripts(GraphicsFixture& gpu)
{
    const auto make = [&] { return std::make_unique<EmbeddedParityScene>(gpu.device.get()); };
    using Script    = IncrementalParity::ParityScript<EmbeddedParityScene>;
    const DxUi::Detail::WorkCounterScope scope;
    RunParityScript(Script{"selector-neighbour", kSeed, 3u, &BuildSelectorNeighbour, &StepFocusThenFlip}, make, "embedded", kReplay);
    RunParityScript(Script{"selector-off-view", kSeed, 3u, &BuildSelectorOffView, &StepFocusThenFlip}, make, "embedded", kReplay);
    RunParityScript(Script{"tabcontrol-ring", kSeed, 3u, &BuildTabRing, &StepFocusThenFlip}, make, "embedded", kReplay);
    RunParityScript(Script{"tagpicker-hover", kSeed, 6u, &BuildTagPicker, &StepTagPicker}, make, "embedded", kReplay);
    RunParityScript(Script{"scrolled-tagpicker-hover", kSeed, 6u, &BuildScrolledTagPicker, &StepTagPicker}, make, "embedded", kReplay);
    RunParityScript(Script{"scrolled-popup-layer", kSeed, 8u, &BuildScrolledPopup, &StepScrolledPopup}, make, "embedded", kReplay);
    Check(scope.Delta().frame.paintRegionRects > 0u, "W4.10: the review scripts compare region frames");
    std::cout << "PASS W4.10 review round 3 scripts: Selector focus ring (E1, H1), TabControl ring outside the control (E2), TagPicker inner "
                 "hover (B1), a PopupLayer in a scrolled ScrollPanel (B3), 0 differences\n";
}

// --- A1: a fractional DPI. The region and the cull snap with the DPI the surface is painted at, not an integer rounding of it.
__declspec(noinline) inline void TestFractionalDpiRegions(GraphicsFixture& gpu)
{
    for (const float dpi : {120.75f, 150.5f, 144.0f})
    {
        SmallView incremental(gpu), fresh(gpu);
        Patch* patches[2]{};
        SmallView* views[2]{&incremental, &fresh};
        for (int index = 0; index < 2; ++index)
        {
            auto root = std::make_unique<DxUi::Panel>();
            root->AddChild<DxUi::Label>(L"Fractional scale")->SetBounds(D2D1::RectF(10, 10, 200, 34));
            patches[index] = root->AddChild<Patch>();
            patches[index]->SetBounds(D2D1::RectF(100.3f, 60.7f, 250.6f, 150.2f));
            views[index]->view.Controls().SetRoot(std::move(root));
            static_cast<void>(views[index]->Frame(dpi));
        }
        for (int flip = 0; flip < 2; ++flip)
        {
            patches[0]->Flip();
            patches[1]->Flip();
            const DxUi::Detail::WorkCounters work = incremental.Frame(dpi);
            {
                const RegionsSwitch off(false);
                fresh.view.Controls().Invalidate();
                static_cast<void>(fresh.Frame(dpi));
            }
            DxUi::WindowHostBitmapCapture a, b;
            Hr(incremental.view.DebugReadSurfaceForTest(a), "W4.10 fractional DPI region surface");
            Hr(fresh.view.DebugReadSurfaceForTest(b), "W4.10 fractional DPI whole surface");
            const std::string difference = DiffPixels(a.bgraPixels, b.bgraPixels, a.widthPx);
            std::cout << "  " << dpi << " DPI, flip " << flip + 1 << ": " << work.frame.paintRegionRects << " region rectangle, "
                      << (difference.empty() ? std::string("0 differences") : difference) << '\n';
            Check(work.frame.paintRegionRects == 1u && work.frame.paintRegionFull == 0u, "W4.10: a flip at a fractional DPI repaints its region");
            Check(difference.empty(), "W4.10 (A1): at a fractional DPI the region frame equals the whole repaint");
        }
    }
    std::cout << "PASS W4.10 fractional DPI: regions at 120.75, 150.5 and 144 DPI equal whole repaints\n";
}

// --- D1, F1, H1: whole frames with regions and culling on equal the same scene with them off.
inline std::string OnOff(GraphicsFixture& gpu, void (*build)(DxUi::Panel&, std::vector<DxUi::Control*>&), float dpi, uint64_t& culled, uint64_t& skipped)
{
    const auto render = [&](bool regions) -> DxUi::WindowHostBitmapCapture
    {
        const RegionsSwitch regionsSwitch(regions);
        SmallView probe(gpu);
        probe.width  = static_cast<UINT>(std::lround(480.0f * dpi / 96.0f));
        probe.height = static_cast<UINT>(std::lround(320.0f * dpi / 96.0f));
        auto root    = std::make_unique<DxUi::Panel>();
        std::vector<DxUi::Control*> focus;
        build(*root, focus);
        probe.view.Controls().SetRoot(std::move(root));
        DxUi::Detail::WorkCounters work = probe.Frame(dpi);
        if (! focus.empty())
        {
            probe.view.Controls().SetFocusControl(focus[0], false);
            static_cast<void>(probe.view.DispatchKey(VK_SHIFT, true));
            static_cast<void>(probe.view.DispatchKey(VK_SHIFT, false));
            probe.view.Controls().Invalidate();
            work = probe.Frame(dpi);
        }
        if (regions)
        {
            culled  = work.frame.paintCulled;
            skipped = work.frame.clipPushesSkipped;
        }
        DxUi::WindowHostBitmapCapture surface;
        Hr(probe.view.DebugReadSurfaceForTest(surface), "W4.10 regions on/off surface");
        return surface;
    };
    // The verifier would report the same difference first: this test names it itself.
    const DxUi::WindowHostBitmapCapture on = [&]
    {
        const bool verifying = DxUi::Detail::GetPaintRegionVerificationForTest();
        DxUi::Detail::SetPaintRegionVerificationForTest(false);
        auto surface = render(true);
        DxUi::Detail::SetPaintRegionVerificationForTest(verifying);
        return surface;
    }();
    const DxUi::WindowHostBitmapCapture off = render(false);
    return DiffPixels(on.bgraPixels, off.bgraPixels, on.widthPx);
}
// D1: the inner viewport encloses the outer one on three sides and ends 0.1 DIP above it: after the outer antialiased clip, the
// paint clip's last row is partly covered, and the inner clip must be pushed to cover it less.
inline void BuildNestedScroll(DxUi::Panel& root, std::vector<DxUi::Control*>&)
{
    auto* outer = root.AddChild<DxUi::ScrollPanel>();
    outer->SetBounds(D2D1::RectF(10, 10, 300, 200.3f));
    auto* inner = outer->AddChild<DxUi::ScrollPanel>();
    inner->SetBounds(D2D1::RectF(5, 5, 310, 200.2f));
    inner->AddChild<Patch>()->SetBounds(D2D1::RectF(0, 0, 320, 260));
}
// A whole pixel-aligned viewport inside the view: the inner clip is still skipped where it changes nothing.
inline void BuildAlignedNestedScroll(DxUi::Panel& root, std::vector<DxUi::Control*>&)
{
    auto* outer = root.AddChild<DxUi::ScrollPanel>();
    outer->SetBounds(D2D1::RectF(0, 0, 480, 320));
    auto* inner = outer->AddChild<DxUi::ScrollPanel>();
    inner->SetBounds(D2D1::RectF(-5, -5, 490, 330));
    inner->AddChild<Patch>()->SetBounds(D2D1::RectF(0, 0, 320, 260));
}
// F1: an undeclared consumer panel that composes its overlay pass through a command list drawn 200 DIP lower; a PopupLayer
// below it holds a Button above the view.
class OverlayImagePanel final : public DxUi::Panel
{
public:
    OverlayImagePanel()                                    = default;
    OverlayImagePanel(const OverlayImagePanel&)            = delete;
    OverlayImagePanel& operator=(const OverlayImagePanel&) = delete;
    OverlayImagePanel(OverlayImagePanel&&)                 = delete;
    OverlayImagePanel& operator=(OverlayImagePanel&&)      = delete;
    ~OverlayImagePanel() override                          = default;
    void PaintOverlay(DxUi::ControlHost& host) const override
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
        DxUi::Panel::PaintOverlay(host);
        dc->SetTarget(previous.get());
        if (FAILED(list->Close()))
            return;
        dc->DrawImage(list.get(), D2D1::Point2F(0.0f, 200.0f));
    }
};
inline void BuildOverlayImage(DxUi::Panel& root, std::vector<DxUi::Control*>&)
{
    auto* panel = root.AddChild<OverlayImagePanel>();
    panel->SetBounds(D2D1::RectF(0, 0, 400, 300));
    auto* layer = panel->AddChild<DxUi::PopupLayer>();
    layer->SetBounds(D2D1::RectF(0, -200, 400, 300));
    layer->AddChild<DxUi::Button>(L"Overlay above the view")->SetBounds(D2D1::RectF(20, -150, 200, -118));
}
inline void BuildSelectorOffViewPanel(DxUi::Panel& root, std::vector<DxUi::Control*>& focus)
{
    auto* button = root.AddChild<DxUi::Button>(L"Selector");
    button->SetVariant(DxUi::ButtonVariant::Selector);
    button->SetBounds(D2D1::RectF(-120, 100, -3.5f, 132));
    focus.push_back(button);
}

__declspec(noinline) inline void TestWholeFramesEqualRegionsOff(GraphicsFixture& gpu)
{
    struct Case final
    {
        const char* name;
        void (*build)(DxUi::Panel&, std::vector<DxUi::Control*>&);
        float dpi;
    };
    constexpr std::array<Case, 6u> cases{{{"D1 nested viewports, 96 DPI", &BuildNestedScroll, 96.0f},
                                          {"D1 nested viewports, 168 DPI", &BuildNestedScroll, 168.0f},
                                          {"D1 nested viewports, 150.5 DPI", &BuildNestedScroll, 150.5f},
                                          {"aligned nested viewports", &BuildAlignedNestedScroll, 96.0f},
                                          {"F1 undeclared overlay composer", &BuildOverlayImage, 96.0f},
                                          {"E1/H1 focused Selector outside the view", &BuildSelectorOffViewPanel, 96.0f}}};
    for (const Case& kind : cases)
    {
        uint64_t culled = 0u, skipped = 0u;
        const std::string difference = OnOff(gpu, kind.build, kind.dpi, culled, skipped);
        std::cout << "  " << kind.name << ": " << culled << " culled, " << skipped << " clips skipped, "
                  << (difference.empty() ? std::string("0 differences") : difference) << " against regions and culling off\n";
        Check(difference.empty(), "W4.10: a whole frame with culling equals the same scene with regions and culling off");
        if (kind.build == &BuildAlignedNestedScroll)
            Check(skipped == 2u, "W4.10: viewports that enclose a whole-pixel paint clip (the outer one and the inner one) are still not pushed");
    }
    std::cout << "PASS W4.10 whole frames with culling equal regions off (nested fractional viewports, an undeclared overlay composer, a "
                 "focused Selector outside the view)\n";
}

// The reach check of one control painted alone in the view; returns the failure text or an empty string.
[[nodiscard]] inline std::string ReachOutside(SmallView& probe, const DxUi::Control& control, float dpi)
{
    DxUi::WindowHostBitmapCapture surface;
    Hr(probe.view.DebugReadSurfaceForTest(surface), "W4.10 reach surface");
    D2D1_RECT_F extent{};
    if (! DxUi::PaintExtentAccess::Extent(probe.view.Controls(), control, extent))
        return {};
    std::string where = PaintReach::InkOutsideExtent(surface, extent, dpi);
    if (! where.empty())
    {
        DxUi::PaintOverhang drawn{};
        static_cast<void>(PaintReach::MeasureInkReach(surface, control.GetBounds(), dpi, drawn));
        where += std::format(" (ink reaches left {:.2f} top {:.2f} right {:.2f} bottom {:.2f} DIP)", drawn.left, drawn.top, drawn.right, drawn.bottom);
    }
    return where;
}

// --- E1: every Button variant and appearance, normal, hovered, pressed and focused with focus visible.
__declspec(noinline) inline void TestButtonVariantReach(GraphicsFixture& gpu)
{
    constexpr std::array<std::pair<DxUi::ButtonVariant, const char*>, 8u> variants{{{DxUi::ButtonVariant::Standard, "Standard"},
                                                                                    {DxUi::ButtonVariant::DropDown, "DropDown"},
                                                                                    {DxUi::ButtonVariant::Selector, "Selector"},
                                                                                    {DxUi::ButtonVariant::Split, "Split"},
                                                                                    {DxUi::ButtonVariant::Hyperlink, "Hyperlink"},
                                                                                    {DxUi::ButtonVariant::IconOnly, "IconOnly"},
                                                                                    {DxUi::ButtonVariant::Disclosure, "Disclosure"},
                                                                                    {DxUi::ButtonVariant::Repeat, "Repeat"}}};
    constexpr std::array<std::pair<DxUi::ButtonAppearance, const char*>, 6u> appearances{{{DxUi::ButtonAppearance::Filled, "Filled"},
                                                                                          {DxUi::ButtonAppearance::Subtle, "Subtle"},
                                                                                          {DxUi::ButtonAppearance::Outline, "Outline"},
                                                                                          {DxUi::ButtonAppearance::Tab, "Tab"},
                                                                                          {DxUi::ButtonAppearance::Caption, "Caption"},
                                                                                          {DxUi::ButtonAppearance::CaptionClose, "CaptionClose"}}};
    size_t checked = 0u;
    for (const float dpi : {96.0f, 120.0f, 168.0f})
        for (const auto& [variant, variantName] : variants)
            for (const auto& [appearance, appearanceName] : appearances)
                for (int state = 0; state < 4; ++state)
                {
                    SmallView probe(gpu);
                    probe.width              = static_cast<UINT>(std::lround(480.0f * dpi / 96.0f));
                    probe.height             = static_cast<UINT>(std::lround(320.0f * dpi / 96.0f));
                    auto root                = std::make_unique<DxUi::Panel>();
                    auto* button             = root->AddChild<DxUi::Button>(L"Choose");
                    const D2D1_RECT_F bounds = D2D1::RectF(140.3f, 100.6f, 260.3f, 132.6f);
                    button->SetVariant(variant);
                    button->SetAppearance(appearance);
                    button->SetBounds(bounds);
                    probe.view.Controls().SetRoot(std::move(root));
                    static_cast<void>(probe.Frame(dpi));
                    const auto point = D2D1::Point2F(std::round(200.3f * dpi / 96.0f), std::round(116.6f * dpi / 96.0f));
                    if (state == 3)
                    {
                        probe.view.Controls().SetFocusControl(button, false);
                        static_cast<void>(probe.view.DispatchKey(VK_SHIFT, true));
                        static_cast<void>(probe.view.DispatchKey(VK_SHIFT, false));
                    }
                    else if (state != 0)
                    {
                        static_cast<void>(probe.view.DispatchPointer(DxUi::PointerEvent{DxUi::PointerAction::Move, point.x, point.y}));
                        if (state == 2)
                            static_cast<void>(probe.view.DispatchPointer(DxUi::PointerEvent{DxUi::PointerAction::Down, point.x, point.y}));
                    }
                    static_cast<void>(probe.view.Controls().ClearTooltip());
                    static_cast<void>(probe.Frame(dpi));
                    const std::string where = ReachOutside(probe, *button, dpi);
                    if (state == 2)
                        static_cast<void>(probe.view.DispatchPointer(DxUi::PointerEvent{DxUi::PointerAction::Up, point.x, point.y}));
                    if (! where.empty())
                        std::cerr << "  Button " << variantName << ' ' << appearanceName << " state " << state << " at " << dpi << " DPI: " << where << '\n';
                    Check(where.empty(), "W4.10 (E1): every Button variant and appearance draws only within its painted extent");
                    ++checked;
                }
    std::cout << "PASS W4.10 Button reach: " << checked << " cases (8 variants, 6 appearances, normal, hovered, pressed, focus visible; 96, 120 and 168 DPI)\n";
}

// --- E2: a TabControl focused with focus visible, at every selection, with more tabs than fit and with one long title.
__declspec(noinline) inline void TestTabControlFocusReach(GraphicsFixture& gpu)
{
    size_t checked = 0u;
    for (const float dpi : {96.0f, 120.0f, 168.0f})
        for (int layout = 0; layout < 3; ++layout)
        {
            const size_t count = layout == 0 ? 8u : (layout == 1 ? 3u : 1u);
            for (size_t selected = 0u; selected < count; ++selected)
            {
                SmallView probe(gpu);
                probe.width              = static_cast<UINT>(std::lround(480.0f * dpi / 96.0f));
                probe.height             = static_cast<UINT>(std::lround(320.0f * dpi / 96.0f));
                auto root                = std::make_unique<DxUi::Panel>();
                auto* tabs               = root->AddChild<DxUi::TabControl>();
                const D2D1_RECT_F bounds = layout == 2 ? D2D1::RectF(140.3f, 100.6f, 220.3f, 200.6f) : D2D1::RectF(140.3f, 100.6f, 300.3f, 200.6f);
                for (size_t index = 0u; index < count; ++index)
                    tabs->AddTab<DxUi::Label>(layout == 2 ? std::wstring(L"A very long piece of text that cannot fit the box at all WWWWWWWW")
                                                          : std::format(L"Tab title {}", index),
                                              L"Page");
                tabs->SetBounds(bounds);
                tabs->SetSelectedIndex(selected);
                probe.view.Controls().SetRoot(std::move(root));
                static_cast<void>(probe.Frame(dpi));
                probe.view.Controls().SetFocusControl(tabs, false);
                static_cast<void>(probe.view.DispatchKey(VK_SHIFT, true));
                static_cast<void>(probe.view.DispatchKey(VK_SHIFT, false));
                static_cast<void>(probe.view.Controls().ClearTooltip());
                static_cast<void>(probe.Frame(dpi));
                const std::string where = ReachOutside(probe, *tabs, dpi);
                if (! where.empty())
                    std::cerr << "  TabControl layout " << layout << " tab " << selected << " at " << dpi << " DPI: " << where << '\n';
                Check(where.empty(), "W4.10 (E2): a focused TabControl draws its selected tab's ring only within its painted extent");
                ++checked;
            }
        }
    std::cout << "PASS W4.10 TabControl focus reach: " << checked << " cases (every selection of 8 tabs in 160 DIP and of 3, one long title)\n";
}

// --- Motion: every library control kind with motion on, sampled mid-flight after each transition, and the reach verifier's
// modes of right-to-left flow and high contrast. The frame verifier checks every region frame the samples make.
__declspec(noinline) inline void TestReachMidAnimation(GraphicsFixture& gpu)
{
    const std::vector<PaintRegionTests::ReachCase> cases = PaintRegionTests::ReachCases();
    enum class Mode : uint8_t
    {
        Motion,
        RightToLeft,
        HighContrast,
    };
    size_t checked = 0u, unbounded = 0u;
    for (const auto& [mode, modeName] :
         {std::pair{Mode::Motion, "motion"}, std::pair{Mode::RightToLeft, "right to left, motion"}, std::pair{Mode::HighContrast, "high contrast"}})
        for (const PaintRegionTests::ReachCase& kind : cases)
            for (const float dpi : {96.0f, 144.0f})
            {
                std::vector<std::shared_ptr<void>> models;
                SmallView probe(gpu);
                probe.width              = static_cast<UINT>(std::lround(480.0f * dpi / 96.0f));
                probe.height             = static_cast<UINT>(std::lround(360.0f * dpi / 96.0f));
                DxUi::ThemePalette theme = IncrementalParity::ParityTheme(false);
                theme.reducedMotion      = mode == Mode::HighContrast;
                theme.highContrast       = mode == Mode::HighContrast;
                probe.view.Controls().SetTheme(theme);
                auto root = std::make_unique<DxUi::Panel>();
                if (mode == Mode::RightToLeft)
                    root->SetFlowDirection(DxUi::FlowDirection::RightToLeft);
                DxUi::Control* control   = kind.add(*root, models);
                const D2D1_RECT_F bounds = D2D1::RectF(140.3f, 100.6f, 140.3f + kind.size.width, 100.6f + kind.size.height);
                control->SetBounds(bounds);
                probe.view.Controls().SetRoot(std::move(root));
                uint64_t now = 1'000u;
                static_cast<void>(probe.view.AdvanceAnimation(now));
                static_cast<void>(probe.Frame(dpi));
                const auto point =
                    D2D1::Point2F(std::round((bounds.left + bounds.right) * 0.5f * dpi / 96.0f), std::round((bounds.top + bounds.bottom) * 0.5f * dpi / 96.0f));
                const auto pointer = [&](DxUi::PointerAction action)
                { static_cast<void>(probe.view.DispatchPointer(DxUi::PointerEvent{action, point.x, point.y})); };
                // Each transition, then samples at 16, 48, 96, 160 and 400 ms after it: mid-flight and at rest.
                const std::array<void (*)(SmallView&, DxUi::Control&, const std::function<void(DxUi::PointerAction)>&), 6u> transitions{
                    [](SmallView&, DxUi::Control&, const std::function<void(DxUi::PointerAction)>& move) { move(DxUi::PointerAction::Move); },
                    [](SmallView&, DxUi::Control&, const std::function<void(DxUi::PointerAction)>& move) { move(DxUi::PointerAction::Down); },
                    [](SmallView&, DxUi::Control&, const std::function<void(DxUi::PointerAction)>& move) { move(DxUi::PointerAction::Up); },
                    [](SmallView& view, DxUi::Control&, const std::function<void(DxUi::PointerAction)>&)
                { static_cast<void>(view.view.DispatchPointer(DxUi::PointerEvent{DxUi::PointerAction::Move, 2.0f, 2.0f})); },
                    [](SmallView& view, DxUi::Control& target, const std::function<void(DxUi::PointerAction)>&)
                {
                    view.view.Controls().SetFocusControl(&target, false);
                    static_cast<void>(view.view.DispatchKey(VK_SHIFT, true));
                    static_cast<void>(view.view.DispatchKey(VK_SHIFT, false));
                },
                    [](SmallView& view, DxUi::Control&, const std::function<void(DxUi::PointerAction)>&)
                { view.view.Controls().SetFocusControl(nullptr, false); },
                };
                const std::function<void(DxUi::PointerAction)> move = pointer;
                for (const auto& transition : transitions)
                {
                    transition(probe, *control, move);
                    uint64_t sampled = now;
                    for (const uint64_t after : {16u, 48u, 96u, 160u, 400u})
                    {
                        sampled = now + after;
                        static_cast<void>(probe.view.AdvanceAnimation(sampled));
                        static_cast<void>(probe.view.Controls().ClearTooltip());
                        static_cast<void>(probe.Frame(dpi));
                        D2D1_RECT_F extent{};
                        if (! DxUi::PaintExtentAccess::Extent(probe.view.Controls(), *control, extent))
                        {
                            ++unbounded;
                            continue;
                        }
                        const std::string where = ReachOutside(probe, *control, dpi);
                        if (! where.empty())
                            std::cerr << "  " << kind.name << ' ' << modeName << " at " << dpi << " DPI, " << after << " ms after a transition: " << where
                                      << '\n';
                        Check(where.empty(), "W4.10: a library control draws only within its painted extent mid-animation too");
                        ++checked;
                    }
                    now = sampled;
                }
            }
    std::cout << "PASS W4.10 reach mid-animation, right to left and in high contrast: " << checked << " samples of " << cases.size() << " control kinds ("
              << unbounded << " unbounded samples skipped)\n";
}

// --- The states the first review did not reach, as region frames the frame verifier checks: a FlexPanel that wraps and one
// in right-to-left flow, a FormPanel, an italic and an emoji label, a hyperlink's underline, high contrast and motion.
template <bool HighContrast> void BuildUncoveredStates(EmbeddedParityScene& scene)
{
    DxUi::ThemePalette theme = IncrementalParity::ParityTheme(false);
    theme.highContrast       = HighContrast;
    theme.reducedMotion      = HighContrast;
    scene.Host().SetTheme(theme);
    auto root  = std::make_unique<DxUi::Panel>();
    auto* flex = root->AddChild<DxUi::FlexPanel>();
    Check(SUCCEEDED(flex->SetWrap(true)) && SUCCEEDED(flex->SetGap(4.0f)), "flex options");
    flex->SetFlowDirection(DxUi::FlowDirection::RightToLeft);
    for (int index = 0; index < 7; ++index)
        flex->AddChild<DxUi::Button>(std::format(L"Wrapped {}", index));
    flex->SetBounds(D2D1::RectF(10, 10, 250, 150));
    auto* form = root->AddChild<DxUi::FormPanel>();
    form->AddChild<DxUi::FormField>(L"name", L"Name");
    form->AddChild<DxUi::FormField>(L"mail", L"Mail");
    form->SetBounds(D2D1::RectF(260, 10, 470, 150));
    auto* emoji = root->AddChild<DxUi::Label>(L"Emoji \U0001F600\U0001F680 text");
    emoji->SetBounds(D2D1::RectF(10, 160, 200, 184));
    auto* link = root->AddChild<DxUi::Button>(L"A hyperlink");
    link->SetVariant(DxUi::ButtonVariant::Hyperlink);
    link->SetBounds(D2D1::RectF(210, 160, 330, 190));
    auto* field = root->AddChild<DxUi::TextField>(L"Field");
    field->SetBounds(D2D1::RectF(10, 200, 200, 232));
    auto* toggle = root->AddChild<DxUi::Toggle>(L"Toggle");
    toggle->SetBounds(D2D1::RectF(210, 200, 360, 228));
    scene.slots[0] = root.get();
    scene.slots[1] = flex;
    scene.slots[2] = emoji;
    scene.slots[3] = link;
    scene.slots[4] = field;
    scene.slots[5] = toggle;
    scene.slots[6] = form;
    scene.SetRoot(std::move(root));
}
inline void StepUncoveredStates(EmbeddedParityScene& scene, Rng& rng, size_t step)
{
    switch (rng.Below(7u))
    {
        case 0u:
            NoteStep("hover a random control");
            scene.Pointer(ParityPointer::Move, CenterOf(*scene.slots[1u + rng.Below(6u)]));
            break;
        case 1u:
            NoteStep("the emoji label's text");
            static_cast<DxUi::Label*>(scene.slots[2])->SetText(step % 2u == 0u ? L"Emoji \U0001F600 text" : L"Emoji \U0001F680\U0001F600\U0001F680 more");
            break;
        case 2u:
            NoteStep("focus the hyperlink, focus visible");
            scene.Host().SetFocusControl(scene.slots[3], false);
            scene.Key(VK_SHIFT);
            break;
        case 3u:
            NoteStep("press the toggle");
            scene.Pointer(ParityPointer::Move, CenterOf(*scene.slots[5]));
            scene.Pointer(ParityPointer::Down, CenterOf(*scene.slots[5]));
            scene.Pointer(ParityPointer::Up, CenterOf(*scene.slots[5]));
            break;
        case 4u:
            NoteStep("type into the field");
            scene.Host().SetFocusControl(scene.slots[4], false);
            scene.Character(L'x');
            break;
        case 5u:
            NoteStep("advance the clock 40 ms (mid-animation)");
            scene.Advance(40u);
            break;
        default:
            NoteStep("move the flex panel by a fractional amount");
            scene.slots[1]->SetBounds(D2D1::RectF(10.5f, 10.25f, step % 2u == 0u ? 230.0f : 250.0f, 150));
            break;
    }
}
inline constexpr std::array<uint64_t, 2u> kUncoveredSeeds{0x0410'3001u, 0x0410'3002u};

__declspec(noinline) inline void TestUncoveredStates(GraphicsFixture& gpu)
{
    const auto make = [&] { return std::make_unique<EmbeddedParityScene>(gpu.device.get()); };
    using Script    = IncrementalParity::ParityScript<EmbeddedParityScene>;
    const DxUi::Detail::WorkCounterScope scope;
    RunParityScript(Script{"uncovered-states", kUncoveredSeeds, 24u, &BuildUncoveredStates<false>, &StepUncoveredStates}, make, "embedded", kReplay);
    RunParityScript(
        Script{"uncovered-states-high-contrast", kUncoveredSeeds, 24u, &BuildUncoveredStates<true>, &StepUncoveredStates}, make, "embedded", kReplay);
    const DxUi::Detail::WorkCounters work = scope.Delta();
    std::cout << "  uncovered states: " << work.frame.paintRegionRects << " region rectangles, " << work.frame.paintCulled << " culled, "
              << work.frame.inkEscapes << " ink escapes\n";
    Check(work.frame.paintRegionRects > 0u, "W4.10: the uncovered-states scripts compare region frames");
    std::cout << "PASS W4.10 right-to-left wrapped flex, form, emoji, hyperlink underline, text field, high contrast and motion: 0 differences\n";
}

// --- X1: a PageHost's pages follow the culling rules of a panel's children (an undeclared page culls nothing below it, a
// declared page's child whose reach meets the clip is painted, a transition in flight stays right); the native suite runs
// the same script (TestNativeIncrementalParityPageHostCulling).
__declspec(noinline) inline void TestPageHostCulling(GraphicsFixture& gpu)
{
    const auto make = [&] { return std::make_unique<EmbeddedParityScene>(gpu.device.get()); };
    const DxUi::Detail::WorkCounterScope scope;
    RunParityScript(PaintRegionScript::PageHostCullingScript<EmbeddedParityScene>(), make, "embedded", kReplay);
    const DxUi::Detail::WorkCounters work = scope.Delta();
    std::cout << "  page-host culling: " << work.frame.paintRegionRects << " region rectangles, " << work.frame.paintCulled << " culled\n";
    Check(work.frame.paintCulled > 0u, "W4.10: the page-host script compares frames that culled a child");
    std::cout << "PASS W4.10 PageHost pages follow the culling rules (undeclared page, a reach into the view, a transition in flight)\n";
}

inline void Run(GraphicsFixture& gpu)
{
    TestReviewScripts(gpu);
    TestPageHostCulling(gpu);
    TestFractionalDpiRegions(gpu);
    TestWholeFramesEqualRegionsOff(gpu);
    TestButtonVariantReach(gpu);
    TestTabControlFocusReach(gpu);
    TestReachMidAnimation(gpu);
    TestUncoveredStates(gpu);
}
} // namespace PaintRegionReview

namespace PaintRegionTests
{
// Every paint-regions test: the gates and the first review's (Run), then the second review's.
inline void RunAll(GraphicsFixture& gpu)
{
    Run(gpu);
    PaintRegionReview::Run(gpu);
}
} // namespace PaintRegionTests
