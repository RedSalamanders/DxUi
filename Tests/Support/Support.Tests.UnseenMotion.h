#pragma once
// Unseen motion (plan item W3.4; Specs/Rendering/Rendering_Win32Host.md, "Unseen motion"): the "unseen-motion" script of the
// incremental-versus-fresh oracle, shared by the embedded suite (Tests/Embedded/Embedded.Tests.UnseenMotion.h) and the native
// suite (Tests/Controls/Controls.Tests.IncrementalParity.cpp). Include after Support.Tests.IncrementalParity.h.
//
// The incremental scene holds the motion it cannot see: an indeterminate ProgressBar, a Slider's value, a ThroughputGraph's
// latest value and a disclosure chevron inside a ScrollPanel that scrolls them out and back, and a ProgressBar on a TabControl
// page that is unselected and selected again. The fresh scene holds nothing (MotionScheduleAccess::IsSeen is true while the
// oracle prepares it), so it ticks every frame on the same clock: the two frames agree only if motion shown again is at its
// clock-correct state in the first frame that shows it, a loop in phase and a transition finished or mid-way, never restarted.
// A ProgressBar outside the ScrollPanel animates throughout, so an embedded view's timeline advances in both scenes alike.
#include <DxUi/DxUi.h>

#include <array>
#include <memory>

namespace UnseenMotionScript
{
using namespace IncrementalParity;

enum UnseenSlot : size_t
{
    kUnseenRoot,
    kKeeper,
    kScroll,
    kScrolledBar,
    kScrolledSlider,
    kScrolledGraph,
    kScrolledDisclosure,
    kTabs,
    kTabBar,
    kUnseenSlotCount,
};
static_assert(kUnseenSlotCount <= kParitySlots);

inline constexpr float kOutOfViewOffsetDip = 900.0f;

template <typename Scene> void BuildUnseenMotionScene(Scene& scene)
{
    // A native scene's window is hidden by default, and a hidden host ticks nothing: this script needs it shown (without
    // activation, off screen).
    if constexpr (requires { scene.ShowWithoutActivation(); })
        scene.ShowWithoutActivation();
    DxUi::ThemePalette theme = ParityTheme(false);
    theme.reducedMotion      = false;
    scene.Host().SetTheme(theme);
    auto root          = std::make_unique<DxUi::Panel>();
    auto& slots        = scene.slots;
    slots[kUnseenRoot] = root.get();
    auto* keeper       = root->AddChild<DxUi::ProgressBar>();
    keeper->SetBounds(D2D1::RectF(12, 12, 300, 28));
    keeper->SetIndeterminate(true);
    slots[kKeeper] = keeper;

    auto* scroll = root->AddChild<DxUi::ScrollPanel>();
    scroll->SetBounds(D2D1::RectF(12, 40, 320, 300));
    auto* bar = scroll->AddChild<DxUi::ProgressBar>();
    bar->SetBounds(D2D1::RectF(20, 52, 290, 68));
    bar->SetIndeterminate(true);
    auto* slider = scroll->AddChild<DxUi::Slider>();
    slider->SetBounds(D2D1::RectF(20, 80, 290, 112));
    slider->SetMinimum(0.0);
    slider->SetMaximum(100.0);
    slider->SetValue(20.0);
    auto* graph = scroll->AddChild<DxUi::ThroughputGraph>();
    graph->SetBounds(D2D1::RectF(20, 120, 290, 220));
    const std::array<DxUi::ThroughputGraphSample, 2u> samples{DxUi::ThroughputGraphSample{.value = 1.0}, DxUi::ThroughputGraphSample{.value = 2.0}};
    graph->SetSamples(samples);
    auto* disclosure = scroll->AddChild<DxUi::Button>(L"Details");
    disclosure->SetBounds(D2D1::RectF(20, 232, 200, 264));
    disclosure->SetDisclosureExpanded(false);
    scroll->SetContentHeight(1'400.0f);
    slots[kScroll]             = scroll;
    slots[kScrolledBar]        = bar;
    slots[kScrolledSlider]     = slider;
    slots[kScrolledGraph]      = graph;
    slots[kScrolledDisclosure] = disclosure;

    auto* tabs = root->AddChild<DxUi::TabControl>();
    tabs->SetBounds(D2D1::RectF(340, 40, 640, 300));
    auto* busy   = tabs->AddTab<DxUi::Panel>(L"Busy");
    auto* tabBar = busy->AddChild<DxUi::ProgressBar>();
    tabBar->SetBounds(D2D1::RectF(360, 120, 620, 136));
    tabBar->SetIndeterminate(true);
    static_cast<void>(tabs->AddTab<DxUi::Panel>(L"Other"));
    slots[kTabs]   = tabs;
    slots[kTabBar] = tabBar;
    scene.SetRoot(std::move(root));
}

// Starts a transition on every scrolled control from where it stands.
template <typename Scene> void StartScrolledTransitions(Scene& scene, Rng& rng)
{
    auto& slots         = scene.slots;
    auto* slider        = static_cast<DxUi::Slider*>(slots[kScrolledSlider]);
    auto* graph         = static_cast<DxUi::ThroughputGraph*>(slots[kScrolledGraph]);
    auto* button        = static_cast<DxUi::Button*>(slots[kScrolledDisclosure]);
    const double target = slider->GetValue() < 50.0 ? 80.0 : 20.0;
    static_cast<void>(slider->RequestValue(scene.Host(), target));
    const std::array<DxUi::ThroughputGraphSample, 2u> samples{DxUi::ThroughputGraphSample{.value = 1.0},
                                                              DxUi::ThroughputGraphSample{.value = 1.0 + static_cast<double>(rng.Below(40u))}};
    graph->SetSamples(samples);
    button->SetDisclosureExpanded(! button->GetDisclosureExpanded().value_or(false));
}

template <typename Scene> void ScrollTo(Scene& scene, float offsetDip)
{
    static_cast<DxUi::ScrollPanel*>(scene.slots[kScroll])->SetScrollOffset(offsetDip);
}

template <typename Scene> void SelectTab(Scene& scene, size_t index)
{
    static_cast<DxUi::TabControl*>(scene.slots[kTabs])->SetSelectedIndex(index);
}

template <typename Scene> void StepUnseenMotion(Scene& scene, Rng& rng, size_t step)
{
    // A fixed opening covers each case once; random steps follow.
    switch (step)
    {
        case 1u:
            NoteStep("transitions start, seen");
            StartScrolledTransitions(scene, rng);
            scene.Advance(16u);
            return;
        case 2u:
            NoteStep("in flight");
            scene.Advance(40u);
            return;
        case 3u:
            NoteStep("scrolled out half way");
            ScrollTo(scene, kOutOfViewOffsetDip);
            scene.Advance(16u);
            return;
        case 4u:
            NoteStep("unseen, the clock runs");
            scene.Advance(30u + rng.Below(60u));
            return;
        case 5u:
            NoteStep("scrolled back: mid-way or finished, from the clock");
            ScrollTo(scene, 0.0f);
            return;
        case 6u:
            NoteStep("transitions start and the panel scrolls out at once");
            StartScrolledTransitions(scene, rng);
            ScrollTo(scene, kOutOfViewOffsetDip);
            return;
        case 7u:
        case 8u:
        case 9u:
            NoteStep("unseen while they settle");
            scene.Advance(120u);
            return;
        case 10u:
            NoteStep("scrolled back after they landed");
            ScrollTo(scene, 0.0f);
            return;
        case 11u:
            NoteStep("the busy tab is unselected");
            SelectTab(scene, 1u);
            scene.Advance(16u);
            return;
        case 12u:
            NoteStep("the clock runs while it is hidden");
            scene.Advance(170u + rng.Below(400u));
            return;
        case 13u:
            NoteStep("the busy tab is selected again");
            SelectTab(scene, 0u);
            return;
        default: break;
    }
    switch (rng.Below(6u))
    {
        case 0u:
            NoteStep("advance");
            scene.Advance(8u + rng.Below(120u));
            break;
        case 1u:
            NoteStep("scroll out");
            ScrollTo(scene, 300.0f + static_cast<float>(rng.Below(600u)));
            break;
        case 2u:
            NoteStep("scroll back part way or whole");
            ScrollTo(scene, rng.Below(2u) == 0u ? 0.0f : static_cast<float>(rng.Below(120u)));
            break;
        case 3u:
            NoteStep("transitions start");
            StartScrolledTransitions(scene, rng);
            break;
        case 4u:
            NoteStep("tab switch");
            SelectTab(scene, rng.Below(2u));
            break;
        default:
            NoteStep("long advance");
            scene.Advance(200u + rng.Below(800u));
            break;
    }
}

inline constexpr std::array<uint64_t, 2u> kUnseenMotionSeeds{0x5EED0340A1u, 0x5EED0340B2u};
template <typename Scene> [[nodiscard]] ParityScript<Scene> UnseenMotionScript()
{
    return {"unseen-motion", kUnseenMotionSeeds, 40u, &BuildUnseenMotionScene<Scene>, &StepUnseenMotion<Scene>};
}
} // namespace UnseenMotionScript
