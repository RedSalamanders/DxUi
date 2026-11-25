#pragma once
// Unseen motion (plan item W3.4; Specs/Rendering/Rendering_Win32Host.md, "Unseen motion"; Specs/Rendering/
// Rendering_EmbeddedD3D11.md, "Animation request") on the embedded host. Included once from Embedded.Tests.Embedded.cpp after
// the animation request tests, the incremental-parity scene and the performance fixtures; uses Check and Hr from there.
//
// - TestLoneUnseenLoop: a view whose only motion is an indeterminate ProgressBar scrolled out reports no animation request and
//   ticks nothing; scrolled back, the preparation that shows it ticks it first (from the view's timeline, not from 0), asks the
//   application for the next frame without other input, and requests every frame again.
// - TestUnseenTransitionLandsOnce: a Slider transition that starts and ends unseen costs one Tick, at its settle time, where it
//   lands; nothing before. Prints what the landing costs (invalidations, preparations, pixels).
// - TestTransitionUnseenHalfway: a transition scrolled out half way and back is where the clock puts it, equal to a twin that
//   stayed seen: never back at its start.
// - TestSchedulingIgnoresThePaintRegionSwitch: the same steps with paint regions off and on tick, hold and catch up alike.
// - TestDisclosureLongDeltaLandsWhereTheClockSays: the application's clock is the motion clock, hitches included.
// - TestTreeRowsOutOfViewNeedNoTicks: a Tree chevron whose row is collapsed away or scrolled out of the Tree's viewport keeps no
//   frames; the paint that shows the row again asks for them.
// - TestCatchUpCostWithAThousandHeldRequests: frames test nothing while nothing that can show a control changed; a scroll
//   tests every motion request once.
// - TestUnseenCaretCost: what a focused caret scrolled out of view costs per flip today (W3.2's rules stay untouched).
// - TestFx11UnseenMotionGate: FX11's gate clauses.
// - TestUnseenMotionParity: the "unseen-motion" oracle script and its negative control.
#include "Controls/DxUi.Internal.h"
#include "Support.Tests.UnseenMotion.h"
#include "Support/RetainedCaches.Tests.Hooks.h"
#include "Support/WorkCounters.h"

#include <iostream>

namespace UnseenMotionTests
{
using DxUi::Detail::WorkCounters;
using DxUi::Detail::WorkCounterScope;
using Kind  = DxUi::EmbeddedAnimationRequest::Kind;
using Scene = EmbeddedAnimationRequestTests::View;

constexpr UINT kWidth  = 320u;
constexpr UINT kHeight = 200u;

// A ScrollPanel (0,0)-(300,120) of 1,000 DIP of content at the top of a view; its controls are placed by the tests.
struct ScrolledView
{
    Scene scene{1'000u};
    DxUi::ScrollPanel* scroll = nullptr;
    ScrolledView()
    {
        scroll = scene.root->AddChild<DxUi::ScrollPanel>();
        scroll->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 120.0f));
        scroll->SetContentHeight(1'000.0f);
    }
    ScrolledView(const ScrolledView&)            = delete;
    ScrolledView& operator=(const ScrolledView&) = delete;
    ScrolledView(ScrolledView&&)                 = delete;
    ScrolledView& operator=(ScrolledView&&)      = delete;
    ~ScrolledView()                              = default;
    [[nodiscard]] DxUi::EmbeddedHost& View() noexcept
    {
        return scene.view;
    }
    [[nodiscard]] DxUi::ControlHost& Host() noexcept
    {
        return scene.view.Controls();
    }
    void Frame()
    {
        Hr(scene.view.Prepare(kWidth, kHeight), "prepare the unseen-motion view");
    }
    DxUi::EmbeddedAnimationRequest Advance(uint64_t atMs)
    {
        return scene.view.AdvanceAnimation(atMs);
    }
    DxUi::ProgressBar* AddBar(DxUi::Panel& parent, float top)
    {
        auto* bar = parent.AddChild<DxUi::ProgressBar>();
        bar->SetBounds(D2D1::RectF(8.0f, top, 280.0f, top + 16.0f));
        bar->SetIndeterminate(true);
        return bar;
    }
    DxUi::Slider* AddSlider(DxUi::Panel& parent, float top)
    {
        auto* slider = parent.AddChild<DxUi::Slider>();
        slider->SetBounds(D2D1::RectF(8.0f, top, 280.0f, top + 32.0f));
        slider->SetMinimum(0.0);
        slider->SetMaximum(100.0);
        slider->SetValue(20.0);
        return slider;
    }
};

[[nodiscard]] inline bool SameSegment(const D2D1_RECT_F& a, const D2D1_RECT_F& b) noexcept
{
    return a.left == b.left && a.right == b.right && a.top == b.top && a.bottom == b.bottom;
}

// Runs the lone-loop steps on a view and its seen twin, both advanced at the same times; returns the scene's counters.
inline WorkCounters RunLoneLoop(bool verify)
{
    ScrolledView view;
    ScrolledView twin;
    auto* bar     = view.AddBar(*view.scroll, 8.0f);
    auto* twinBar = twin.AddBar(*twin.scroll, 8.0f);
    view.Frame();
    twin.Frame();
    const DxUi::ThemePalette theme = view.Host().GetTheme();
    uint64_t at                    = 2'000u;
    for (; at <= 2'300u; at += 20u)
    {
        static_cast<void>(view.Advance(at));
        static_cast<void>(twin.Advance(at));
        view.Frame();
        twin.Frame();
    }
    const WorkCounterScope scope;
    view.scroll->SetScrollOffset(600.0f);
    view.Frame();
    // The application advances at the time it would have slept to, then sleeps on what it is told.
    const WorkCounterScope held;
    const DxUi::EmbeddedAnimationRequest request = view.Advance(at);
    if (verify)
    {
        Check(held.Delta().frame.tickCalls == 0u, "W3.4: the advance after the loop scrolled out ticks nothing");
        Check(request.kind == Kind::None && view.View().GetAnimationRequest().kind == Kind::None,
              "W3.4 (answer 4): a view whose only motion is unseen reports no animation request");
    }
    // The application slept: the view's timeline stands where the last advance put it, as the twin's does (it is not advanced).
    const D2D1_RECT_F held0 = bar->DebugGetIndeterminateSegmentRect(theme);

    view.scroll->SetScrollOffset(0.0f);
    const size_t wakesBefore = view.scene.wakes.calls;
    const WorkCounterScope shown;
    view.Frame();
    if (verify)
    {
        const WorkCounters work = shown.Delta();
        Check(work.frame.tickCatchUps == 1u && work.frame.catchUpLayoutRepeats == 0u,
              "W3.4: the preparation that shows the loop ticks it once, before it paints");
        Check(view.scene.wakes.calls > wakesBefore, "W3.4 (addition C): the preparation asks the application for the next frame without other input");
        Check(view.View().GetAnimationRequest().kind != Kind::None, "W3.4: the shown loop asks for frames again");
        // The application slept, so the view's timeline stood still: the loop continues from where the timeline is, which
        // is where the twin that ticked all along is too; never from phase 0.
        twin.Frame();
        Check(SameSegment(bar->DebugGetIndeterminateSegmentRect(theme), twinBar->DebugGetIndeterminateSegmentRect(theme)),
              "W3.4: the first frame that shows the loop continues from the view's timeline");
        Check(SameSegment(held0, bar->DebugGetIndeterminateSegmentRect(theme)) && held0.right > held0.left,
              "with no time passed on the timeline, it shows the segment it showed when it left: no restart from phase 0");
    }
    return scope.Delta();
}

inline void TestLoneUnseenLoop()
{
    static_cast<void>(RunLoneLoop(true));
    std::cout << "PASS a lone unseen loop requests nothing and resumes from the view's timeline\n";
}

// Additions A: the scheduling of unseen motion does not depend on the paint-region switch.
inline void TestSchedulingIgnoresThePaintRegionSwitch()
{
    const WorkCounters on = RunLoneLoop(false);
    DxUi::Detail::SetPaintRegionsEnabledForTest(false);
    const auto restore     = wil::scope_exit([]() noexcept { DxUi::Detail::SetPaintRegionsEnabledForTest(true); });
    const WorkCounters off = RunLoneLoop(false);
    std::cout << "  regions on: " << on.frame.tickCalls << " ticks, " << on.frame.tickHeldUnseen << " held, " << on.frame.tickCatchUps
              << " caught up; off: " << off.frame.tickCalls << ", " << off.frame.tickHeldUnseen << ", " << off.frame.tickCatchUps << '\n';
    Check(on.frame.tickCalls == off.frame.tickCalls && on.frame.tickHeldUnseen == off.frame.tickHeldUnseen && on.frame.tickCatchUps == off.frame.tickCatchUps &&
              on.frame.tickHeldUnseen != 0u,
          "W3.4 (addition A): tick counts with paint regions off equal those with regions on");
    std::cout << "PASS unseen-motion scheduling ignores the paint-region switch\n";
}

inline void TestUnseenTransitionLandsOnce()
{
    ScrolledView view;
    auto* slider = view.AddSlider(*view.scroll, 40.0f);
    view.Frame();
    view.scroll->SetScrollOffset(600.0f);
    view.Frame();
    Check(slider->RequestValue(view.Host(), 80.0), "an unseen value animation starts");
    DxUi::EmbeddedAnimationRequest request = view.View().GetAnimationRequest();
    Check(request.kind == Kind::Deadline, "W3.4: an unseen transition asks for one deadline, not every frame");
    // After the idle pause the first advance only takes the baseline; the deadline is then the settle time.
    request = view.Advance(5'000u);
    Check(request.kind == Kind::Deadline && request.dueTickMs > 5'000u, "the deadline is the settle time on the application's clock");
    const uint64_t due = request.dueTickMs;
    uint64_t ticks = 0u, invalidations = 0u, preparations = 0u, pixels = 0u;
    for (uint64_t at = 5'016u; at < due; at += 16u)
    {
        const WorkCounterScope early;
        static_cast<void>(view.Advance(at));
        ticks += early.Delta().frame.tickCalls;
    }
    Check(ticks == 0u, "W3.4: nothing ticks an unseen transition before it settles");
    const WorkCounterScope landing;
    request = view.Advance(due);
    view.Frame();
    const WorkCounters work = landing.Delta();
    ticks                   = work.frame.tickCalls;
    invalidations           = work.frame.invalidations;
    preparations            = work.frame.preparations;
    pixels                  = work.frame.pixelsPainted;
    Check(ticks == 1u && slider->DebugGetDisplayedValue() == 80.0, "W3.4: one Tick at the settle time lands it at its target");
    Check(request.kind == Kind::None && view.View().GetAnimationRequest().kind == Kind::None, "and nothing is left requested");
    std::cout << "  embedded unseen landing (addition B, measured): " << ticks << " tick, " << invalidations << " invalidation(s) ("
              << work.accessibility.undeclaredInvalidations << " undeclared), " << preparations << " preparation(s), " << work.frame.paintRegionFull
              << " whole and " << work.frame.paintRegionRects << " region rectangle(s), " << pixels << " pixels painted\n";
    std::cout << "PASS an unseen transition lands with one Tick at its settle time\n";
}

inline void TestTransitionUnseenHalfway()
{
    ScrolledView view;
    auto* inside  = view.AddSlider(*view.scroll, 40.0f);
    auto* outside = view.AddSlider(*view.scene.root, 150.0f);
    view.Frame();
    static_cast<void>(view.Advance(3'000u));
    Check(inside->RequestValue(view.Host(), 80.0) && outside->RequestValue(view.Host(), 80.0), "two value animations start together");
    static_cast<void>(view.Advance(3'000u)); // the baseline after the pause
    static_cast<void>(view.Advance(3'016u));
    view.Frame();
    Check(inside->DebugGetDisplayedValue() == outside->DebugGetDisplayedValue() && inside->DebugGetDisplayedValue() > 20.0, "both are in flight together");
    view.scroll->SetScrollOffset(600.0f);
    view.Frame();
    const double heldAt = inside->DebugGetDisplayedValue();
    static_cast<void>(view.Advance(3'048u));
    static_cast<void>(view.Advance(3'080u));
    Check(inside->DebugGetDisplayedValue() == heldAt, "the unseen one is not ticked");
    view.scroll->SetScrollOffset(0.0f);
    view.Frame();
    const double shown = inside->DebugGetDisplayedValue();
    Check(shown == outside->DebugGetDisplayedValue() && shown > heldAt && shown < 80.0,
          "W3.4: shown again half way, it is where the clock puts it, equal to its seen twin, never back at its start");
    std::cout << "PASS a transition unseen half way resumes from the clock\n";
}

inline void TestDisclosureLongDeltaLandsWhereTheClockSays()
{
    // Two views, the same rotation; one advanced in 8 ms steps, the other by one long delta while the chevron is seen.
    float stepped = 0.0f, jumped = 0.0f;
    for (const bool longDelta : {false, true})
    {
        ScrolledView view;
        auto* button = view.scene.root->AddChild<DxUi::Button>(L"Details");
        button->SetBounds(D2D1::RectF(8.0f, 150.0f, 200.0f, 182.0f));
        button->SetDisclosureExpanded(false);
        view.Frame();
        button->SetDisclosureExpanded(true);
        static_cast<void>(view.Advance(4'000u)); // the baseline
        static_cast<void>(view.Advance(4'016u));
        if (longDelta)
            static_cast<void>(view.Advance(4'136u));
        else
            for (uint64_t at = 4'024u; at <= 4'136u; at += 8u)
                static_cast<void>(view.Advance(at));
        (longDelta ? jumped : stepped) = button->DebugGetDisclosureAnimationProgress();
    }
    std::cout << "  disclosure at 136 ms of 240: stepped " << stepped << ", one 120 ms delta " << jumped << '\n';
    Check(jumped == stepped && jumped > 0.0f && jumped < 1.0f,
          "W3.4 (answer 3): a 120 ms delta while seen lands the chevron where the application's clock says, as 8 ms steps do");
    std::cout << "PASS the disclosure chevron follows the application's clock\n";
}

// A tree of 40 rows; row 0 and row 2 are folders with no children (toggling them turns only their chevron); row 1 is a folder
// whose one child (row 2 while open) is such a folder. The delegate applies every toggle at once, or later when `deferred`.
class ChevronModel final : public DxUi::ITreeModel, public DxUi::ITreeDelegate
{
public:
    [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
    {
        return 40u - (parentOpen ? 0u : 1u);
    }
    void GetVisibleItem(size_t index, DxUi::TreeItemData& item) const override
    {
        item       = {};
        size_t row = index;
        if (! parentOpen && row >= 2u)
            ++row;
        item.id          = 100u + row;
        item.text        = L"Row " + std::to_wstring(row);
        item.hasChildren = row <= 2u;
        item.expanded    = row == 0u ? emptyOpen : row == 1u ? parentOpen : row == 2u ? childOpen : false;
        item.depth       = row == 2u ? 1u : 0u;
        if (row == 2u)
            item.parentId = 101u;
    }
    void OnTreeToggleExpanded(uint64_t itemId, bool expanded) override
    {
        if (deferred)
            return;
        if (itemId == 100u)
            emptyOpen = expanded;
        else if (itemId == 101u)
            parentOpen = expanded;
        else if (itemId == 102u)
            childOpen = expanded;
        if (tree)
            tree->NotifyDataChanged();
    }
    bool emptyOpen = false, parentOpen = true, childOpen = false, deferred = false;
    DxUi::Tree* tree = nullptr;
};

inline void Wheel(ScrolledView& view, float delta)
{
    DxUi::PointerEvent event{DxUi::PointerAction::Wheel, 150.0f, 60.0f};
    event.wheelDelta = delta;
    static_cast<void>(view.View().DispatchPointer(event));
}

inline void TestTreeRowsOutOfViewNeedNoTicks()
{
    ScrolledView view;
    ChevronModel model;
    view.scroll->SetVisible(false); // the Tree takes its place
    auto* tree = view.scene.root->AddChild<DxUi::Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 120.0f));
    tree->SetModel(&model);
    tree->SetDelegate(&model);
    model.tree = tree;
    view.Frame();
    static_cast<void>(view.Advance(6'000u));

    // Scrolled out of the Tree's own viewport: row 0's chevron turns, then the rows scroll by a page.
    Check(tree->RequestExpandedState(0u, true), "row 0's chevron turns");
    static_cast<void>(view.Advance(6'000u));
    static_cast<void>(view.Advance(6'016u));
    view.Frame();
    Wheel(view, -360.0f);
    view.Frame();
    Check(tree->GetFirstVisibleItemIndex() > 0u, "the rows scrolled");
    {
        const WorkCounterScope out;
        for (uint64_t at = 6'032u; at <= 6'200u; at += 16u)
        {
            if (view.View().GetAnimationRequest().kind == Kind::None)
                break;
            static_cast<void>(view.Advance(at));
        }
        Check(view.View().GetAnimationRequest().kind == Kind::None, "W3.4 (answer 6): a chevron scrolled out of the Tree's viewport keeps no frames");
        Check(out.Delta().frame.tickCalls <= 1u, "at most the tick that finds it gone");
    }
    // Scrolled back while it still turns: the paint that shows it asks for its ticks again.
    Wheel(view, 1'200.0f);
    view.Frame();
    Check(tree->GetFirstVisibleItemIndex() == 0u, "the rows scrolled back");
    Check(view.View().GetAnimationRequest().kind != Kind::None, "W3.4: the paint that shows the turning chevron asks for frames again");

    // Collapsed away: row 2's chevron turns, then its parent collapses with no slide (the delegate applies it later).
    for (uint64_t at = 6'216u; at < 6'800u && view.View().GetAnimationRequest().kind != Kind::None; at += 16u)
    {
        static_cast<void>(view.Advance(at));
        view.Frame();
    }
    Check(tree->RequestExpandedState(2u, true), "row 2's chevron turns");
    static_cast<void>(view.Advance(7'000u));
    static_cast<void>(view.Advance(7'016u));
    view.Frame();
    model.parentOpen = false;
    model.deferred   = true;
    tree->NotifyDataChanged();
    view.Frame();
    const WorkCounterScope collapsed;
    for (uint64_t at = 7'032u; at <= 7'200u && view.View().GetAnimationRequest().kind != Kind::None; at += 16u)
        static_cast<void>(view.Advance(at));
    Check(view.View().GetAnimationRequest().kind == Kind::None && collapsed.Delta().frame.tickCalls <= 1u,
          "W3.4 (answer 6): a chevron whose row was collapsed away keeps no frames");
    std::cout << "PASS Tree chevrons out of view need no ticks\n";
}

inline void TestCatchUpCostWithAThousandHeldRequests()
{
    ScrolledView view;
    for (size_t index = 0u; index < 1'000u; ++index)
        static_cast<void>(view.AddBar(*view.scroll, 8.0f + static_cast<float>(index % 40u) * 20.0f));
    auto* keeper = view.AddBar(*view.scene.root, 150.0f);
    view.Frame();
    static_cast<void>(view.Advance(8'000u));
    static_cast<void>(view.Advance(8'016u));
    view.Frame();
    view.scroll->SetScrollOffset(880.0f); // the end of the content: every bar is above the viewport
    view.Frame();
    {
        const WorkCounterScope frames;
        for (uint64_t at = 8'032u; at <= 8'512u; at += 16u)
        {
            static_cast<void>(view.Advance(at));
            view.Frame();
        }
        const WorkCounters work = frames.Delta();
        std::cout << "  1,000 held loops, 31 frames of a seen keeper: " << work.frame.tickCalls << " ticks, " << work.frame.motionSeenRefreshes
                  << " visibility tests, " << work.frame.tickCatchUps << " catch-ups\n";
        Check(work.frame.tickCalls == 31u && work.frame.motionSeenRefreshes == 0u,
              "W3.4 (addition E): with 1,000 held requests a frame that changes nothing that can show them tests none of them");
    }
    {
        const WorkCounterScope scroll;
        view.scroll->SetScrollOffset(870.0f);
        view.Frame();
        const WorkCounters work = scroll.Delta();
        std::cout << "  a scroll that shows none of them: " << work.frame.motionSeenRefreshes << " test of 1,001 requests, " << work.frame.tickCatchUps
                  << " catch-ups\n";
        Check(work.frame.motionSeenRefreshes == 1u && work.frame.tickCatchUps == 0u, "a scroll tests the requests once and shows none");
    }
    Check(keeper->IsIndeterminate(), "the keeper animates throughout");
    std::cout << "PASS the catch-up pass is gated on what can show a control\n";
}

inline void TestUnseenCaretCost()
{
    ScrolledView view;
    view.Host().DebugSetCaretBlinkSettingsForTest(DxUi::ControlHost::CaretBlinkSettingsForTest{530u, 0u});
    const auto restore = wil::scope_exit([&]() noexcept { view.Host().DebugSetCaretBlinkSettingsForTest(std::nullopt); });
    auto* field        = view.scroll->AddChild<DxUi::TextField>(L"caret");
    field->SetBounds(D2D1::RectF(8.0f, 40.0f, 280.0f, 72.0f));
    view.Frame();
    view.Host().SetFocusControl(field, false);
    view.scroll->SetScrollOffset(600.0f);
    view.Frame();
    DxUi::EmbeddedAnimationRequest request = view.Advance(9'000u);
    const WorkCounterScope flips;
    size_t count = 0u;
    for (; count < 10u && request.kind == Kind::Deadline; ++count)
    {
        request = view.Advance(request.dueTickMs);
        view.Frame();
    }
    const WorkCounters work = flips.Delta();
    Check(count == 10u, "an unseen focused caret keeps its deadlines (W3.2 untouched by W3.4)");
    std::cout << "  embedded unseen caret (answer 2, measured): " << count << " flips at 530 ms = " << (1'000.0 / 530.0) << " application wakes/s; "
              << work.frame.tickCalls << " ticks, " << work.frame.invalidations << " invalidations, " << work.frame.preparations << " preparations, "
              << work.frame.pixelsPainted << " pixels painted\n";
    std::cout << "PASS the unseen caret's cost is recorded\n";
}

// FX11's W3.4 gate: on the page slid out, 0 ticks after the slide settles (the page is destroyed with it); in the
// scrolled-out phase, the indeterminate ProgressBar ticks 0 times while out, the Slider lands with one Tick, and the frame that
// scrolls them back catches the bar up once.
inline void TestFx11UnseenMotionGate()
{
    using namespace PerformanceFixtures;
    const RunRecord run      = RunEmbedded(Fx11::kDefinition, Fx11::kVariants[0]);
    const PhaseRecord& slide = run.Phase("progress-slide-60hz");
    uint64_t afterSettle     = 0u;
    for (size_t step = Fx11::kProgressSettledStep; step < slide.steps.size(); ++step)
        afterSettle += slide.steps[step].work.frame.tickCalls;
    std::cout << "FX11 progress-slide-60hz: " << afterSettle << " ticks after the slide settles\n";
    Check(afterSettle == 0u, "W3.4: FX11 0 ticks from the ProgressBar on the page slid out, after the slide settles");

    const PhaseRecord& scrolled = run.Phase("unseen-60hz");
    uint64_t whileOut = 0u, catchUps = 0u;
    for (size_t step = Fx11::kUnseenOutStep + 1u; step < Fx11::kUnseenBackStep; ++step)
        whileOut += scrolled.steps[step].work.frame.tickCalls;
    catchUps = scrolled.steps[Fx11::kUnseenBackStep].work.frame.tickCatchUps;
    std::cout << "FX11 unseen-60hz: " << whileOut << " tick(s) while scrolled out (the Slider's landing), " << catchUps << " catch-up(s) on return\n";
    Check(whileOut == 1u, "W3.4: FX11 scrolled out: 0 ticks from the loop and one Tick for the transition, at its settle time");
    // The loop, and the Slider: its landing Tick asked for one more frame (its final repaint), which is held like a loop and
    // ticked once with the rest when seen.
    Check(catchUps == 2u, "W3.4: FX11 the frame that scrolls them back catches the loop up once, and the landed Slider's final frame");
}

inline void TestUnseenMotionParity(GraphicsFixture& gpu)
{
    const auto make = [&] { return std::make_unique<EmbeddedParity::EmbeddedParityScene>(gpu.device.get()); };
    const WorkCounterScope scope;
    RunParityScript(UnseenMotionScript::UnseenMotionScript<EmbeddedParity::EmbeddedParityScene>(), make, "embedded", EmbeddedParity::kReplay);
    const WorkCounters work = scope.Delta();
    std::cout << "  embedded unseen-motion oracle: " << work.frame.tickHeldUnseen << " held, " << work.frame.tickCatchUps << " caught up\n";
    Check(work.frame.tickHeldUnseen > 0u && work.frame.tickCatchUps > 0u, "W3.4: the embedded oracle compares frames that held and caught up motion");
    DxUi::Detail::SetMotionCatchUpDisabledForTest(true);
    const auto restore = wil::scope_exit([]() noexcept { DxUi::Detail::SetMotionCatchUpDisabledForTest(false); });
    const std::string difference =
        RunParityScriptOnce(UnseenMotionScript::UnseenMotionScript<EmbeddedParity::EmbeddedParityScene>(), UnseenMotionScript::kUnseenMotionSeeds[0], make);
    std::cout << "  embedded unseen-motion negative control: " << difference << '\n';
    Check(difference.find("pixels differ") != std::string::npos, "W3.4: motion shown one frame late differs from a scene that ticked it all along");
}

inline void RunAll(GraphicsFixture& gpu)
{
    TestLoneUnseenLoop();
    TestSchedulingIgnoresThePaintRegionSwitch();
    TestUnseenTransitionLandsOnce();
    TestTransitionUnseenHalfway();
    TestDisclosureLongDeltaLandsWhereTheClockSays();
    TestTreeRowsOutOfViewNeedNoTicks();
    TestCatchUpCostWithAThousandHeldRequests();
    TestUnseenCaretCost();
    TestFx11UnseenMotionGate();
    TestUnseenMotionParity(gpu);
}
} // namespace UnseenMotionTests
