#pragma once
// FX11 Motion (plan W0.4; proves G14 and G7): with motion enabled, a PageHost slide, a Tree expand and collapse, Toggle,
// Checkbox and Slider state changes and a ComboBox popup open and close, each sampled on the frozen fixture clock at 30, 60,
// 144 and 240 Hz. Records today's motion (the "before" of the W5 motion wave): per transition window, the ticks the host
// delivered, the frames rendered and the layouts and allocations of every animated frame, and the ticks still delivered
// after the transition settled.
//
// Shape: one measured phase per (transition, rate), named "<transition>-<rate>hz". Step 0 applies the state change and its
// frame; step k >= 1 advances the clock to FrameTimeMs(rate, k) (whole milliseconds, rounded from k * 1000 / rate, so 144 and
// 240 Hz alternate 6/7 and 4/5 ms) and renders. Each window lasts kWindowMs, beyond the longest transition today (Tree rows,
// 320 ms), so its last steps show what happens after settle. A last phase slides a page holding an indeterminate ProgressBar
// out at 60 Hz (the W3.4 case).
//
// State changes go through the controls' own requests, never real input: PageHost::SetPage, Tree::RequestExpandedState with
// a delegate that publishes the model change, Toggle and Checkbox SetChecked, Slider::RequestValue (the committed user edit),
// and F4 delivered to the ComboBox's own key handler, which opens and closes its in-host overlay popup (no popup window).
// Nothing takes focus or captures the pointer, so the fixture runs on both hosts. MenuPopup open and close needs a native
// popup window and is not part of this fixture.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx11
{
inline constexpr std::array kBudgets{std::string_view("G14"), std::string_view("G7")};

inline constexpr std::array kRates{30u, 60u, 144u, 240u};
inline constexpr uint64_t kWindowMs = 400u;
// The fixture clock time of step `step` of a window sampled at `rate` Hz, relative to the state change.
[[nodiscard]] constexpr uint64_t FrameTimeMs(unsigned rate, size_t step) noexcept
{
    return (static_cast<uint64_t>(step) * 2000u / rate + 1u) / 2u;
}
// Steps of one window: the state change and every sample up to kWindowMs.
[[nodiscard]] constexpr size_t WindowSteps(unsigned rate) noexcept
{
    return 1u + static_cast<size_t>((kWindowMs * rate + 999u) / 1000u);
}

// Transitions in their order within each rate block.
enum class Transition : uint8_t
{
    PageSlide,
    TreeExpand,
    TreeCollapse,
    ToggleOn,
    ToggleOff,
    CheckboxOn,
    CheckboxOff,
    SliderUp,
    SliderDown,
    PopupOpen,
    PopupClose,
    Count,
};
inline constexpr size_t kTransitions = static_cast<size_t>(Transition::Count);
inline constexpr size_t kWarm        = 0u;
// The first phase of each rate block is 1 + rateIndex * kTransitions; the ProgressBar phase comes last.
inline constexpr size_t kProgressSlide = 1u + kRates.size() * kTransitions;
inline constexpr size_t kProgressSteps = 1u + static_cast<size_t>(2u * kWindowMs * 60u / 1000u);

#define DXUI_FX11_RATE_PHASES(rate)                                                                                                                            \
    Phase{"page-slide-" #rate "hz", WindowSteps(rate), PhaseKind::Measured, "PageHost::SetPage to a new page"},                                                \
        Phase{"tree-expand-" #rate "hz", WindowSteps(rate), PhaseKind::Measured, "Tree::RequestExpandedState opens a folder of 6 rows"},                       \
        Phase{"tree-collapse-" #rate "hz", WindowSteps(rate), PhaseKind::Measured, "Tree::RequestExpandedState closes that folder"},                           \
        Phase{"toggle-on-" #rate "hz", WindowSteps(rate), PhaseKind::Measured, "Toggle::SetChecked(true)"},                                                    \
        Phase{"toggle-off-" #rate "hz", WindowSteps(rate), PhaseKind::Measured, "Toggle::SetChecked(false)"},                                                  \
        Phase{"checkbox-on-" #rate "hz", WindowSteps(rate), PhaseKind::Measured, "Checkbox::SetChecked(true)"},                                                \
        Phase{"checkbox-off-" #rate "hz", WindowSteps(rate), PhaseKind::Measured, "Checkbox::SetChecked(false)"},                                              \
        Phase{"slider-up-" #rate "hz", WindowSteps(rate), PhaseKind::Measured, "Slider::RequestValue from 20 to 80"},                                          \
        Phase{"slider-down-" #rate "hz", WindowSteps(rate), PhaseKind::Measured, "Slider::RequestValue from 80 to 20"},                                        \
        Phase{"popup-open-" #rate "hz", WindowSteps(rate), PhaseKind::Measured, "ComboBox F4 opens its in-host overlay popup"},                                \
        Phase{"popup-close-" #rate "hz", WindowSteps(rate), PhaseKind::Measured, "ComboBox F4 closes the popup"}

inline constexpr std::array kPhases{
    Phase{"warm", 8u, PhaseKind::Warmup, "the scene's first frames, 16 ms apart: resources and first layouts settle"},
    DXUI_FX11_RATE_PHASES(30),
    DXUI_FX11_RATE_PHASES(60),
    DXUI_FX11_RATE_PHASES(144),
    DXUI_FX11_RATE_PHASES(240),
    Phase{"progress-slide-60hz",
          kProgressSteps,
          PhaseKind::Measured,
          "60 Hz: a page holding an indeterminate ProgressBar slides in at step 0, slides out at step 24 (400 ms); ticks after step 40 come from what remains"},
};
#undef DXUI_FX11_RATE_PHASES
static_assert(kPhases.size() == kProgressSlide + 1u);

// The phase index of a transition at a rate (kRates index).
[[nodiscard]] constexpr size_t PhaseIndex(size_t rateIndex, Transition transition) noexcept
{
    return 1u + rateIndex * kTransitions + static_cast<size_t>(transition);
}

inline constexpr std::array kVariants{Variant{"default", 480u, 360u, 96.0f, false}};

// A two-level tree: three folders of six items; the first folder starts collapsed. The delegate publishes every toggle
// before it returns, so the Tree animates from the rows before the request to the rows after it.
class MotionTreeModel final : public DxUi::ITreeModel, public DxUi::ITreeDelegate
{
public:
    static constexpr size_t kFolders = 3u, kChildren = 6u;

    [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
    {
        size_t count = kFolders;
        for (const bool open : expanded_)
            count += open ? kChildren : 0u;
        return count;
    }
    void GetVisibleItem(size_t visibleIndex, DxUi::TreeItemData& item) const override
    {
        item = {};
        for (size_t folder = 0u; folder < kFolders; ++folder)
        {
            if (visibleIndex == 0u)
            {
                item.id          = FolderId(folder);
                item.text        = L"Folder " + std::to_wstring(folder + 1u);
                item.hasChildren = true;
                item.expanded    = expanded_[folder];
                return;
            }
            --visibleIndex;
            if (! expanded_[folder])
                continue;
            if (visibleIndex < kChildren)
            {
                item.id       = FolderId(folder) + 1u + visibleIndex;
                item.parentId = FolderId(folder);
                item.text     = L"Item " + std::to_wstring(folder + 1u) + L"." + std::to_wstring(visibleIndex + 1u);
                item.depth    = 1u;
                return;
            }
            visibleIndex -= kChildren;
        }
    }
    void OnTreeToggleExpanded(uint64_t itemId, bool expanded) override
    {
        for (size_t folder = 0u; folder < kFolders; ++folder)
            if (FolderId(folder) == itemId)
                expanded_[folder] = expanded;
        if (tree_)
            tree_->NotifyDataChanged();
    }
    void Bind(DxUi::Tree* tree) noexcept
    {
        tree_ = tree;
    }

private:
    [[nodiscard]] static constexpr uint64_t FolderId(size_t folder) noexcept
    {
        return 100u * (folder + 1u);
    }
    std::array<bool, kFolders> expanded_{false, true, true};
    DxUi::Tree* tree_ = nullptr;
};

class Scene final : public Scenario
{
public:
    HRESULT Build(Context& context) override
    {
        auto root  = std::make_unique<DxUi::Panel>();
        pageHost_  = root->AddChild<DxUi::PageHost>();
        tree_      = root->AddChild<DxUi::Tree>();
        toggle_    = root->AddChild<DxUi::Toggle>(L"Wi-Fi");
        checkbox_  = root->AddChild<DxUi::Checkbox>(L"Remember");
        slider_    = root->AddChild<DxUi::Slider>();
        comboBox_  = root->AddChild<DxUi::ComboBox>();
        auto* note = root->AddChild<DxUi::Label>(L"Motion fixture");
        pageHost_->SetBounds(D2D1::RectF(8.0f, 8.0f, 232.0f, 168.0f));
        tree_->SetBounds(D2D1::RectF(240.0f, 8.0f, 472.0f, 352.0f));
        toggle_->SetBounds(D2D1::RectF(8.0f, 176.0f, 232.0f, 204.0f));
        checkbox_->SetBounds(D2D1::RectF(8.0f, 208.0f, 232.0f, 236.0f));
        slider_->SetBounds(D2D1::RectF(8.0f, 240.0f, 232.0f, 268.0f));
        comboBox_->SetBounds(D2D1::RectF(8.0f, 272.0f, 232.0f, 300.0f));
        note->SetBounds(D2D1::RectF(8.0f, 320.0f, 232.0f, 344.0f));
        tree_->SetModel(&model_);
        tree_->SetDelegate(&model_);
        model_.Bind(tree_);
        slider_->SetMinimum(0.0);
        slider_->SetMaximum(100.0);
        slider_->SetValue(20.0);
        std::vector<DxUi::ComboBox::Item> items;
        for (size_t index = 0u; index < 6u; ++index)
            items.push_back({L"v" + std::to_wstring(index), L"Choice " + std::to_wstring(index + 1u)});
        comboBox_->SetItems(std::move(items));
        comboBox_->SetSelectedIndex(0u);
        pageHost_->SetPage(MakePage(false));
        context.controls.SetRoot(std::move(root));
        return S_OK;
    }

    void Step(Context& context, size_t phase, size_t step) override
    {
        if (phase == kWarm)
        {
            context.port.AdvanceTime(16u);
            return;
        }
        if (phase == kProgressSlide)
        {
            if (step > 0u)
                context.port.AdvanceTime(FrameTimeMs(60u, step) - FrameTimeMs(60u, step - 1u));
            if (step == 0u)
                pageHost_->SetPage(MakePage(true));
            else if (step == (kProgressSteps - 1u) / 2u)
                pageHost_->SetPage(MakePage(false));
            return;
        }
        const size_t rateIndex = ((phase - 1u) / kTransitions) % kRates.size();
        const auto transition  = static_cast<Transition>((phase - 1u) % kTransitions);
        const unsigned rate    = kRates[rateIndex];
        // The timing harness plays a phase beyond its declared steps: it repeats the window.
        const size_t local = step % WindowSteps(rate);
        if (local > 0u)
        {
            context.port.AdvanceTime(FrameTimeMs(rate, local) - FrameTimeMs(rate, local - 1u));
            return;
        }
        switch (transition)
        {
            case Transition::PageSlide: pageHost_->SetPage(MakePage(false)); break;
            case Transition::TreeExpand: static_cast<void>(tree_->RequestExpandedState(0u, true)); break;
            case Transition::TreeCollapse: static_cast<void>(tree_->RequestExpandedState(0u, false)); break;
            case Transition::ToggleOn: toggle_->SetChecked(true); break;
            case Transition::ToggleOff: toggle_->SetChecked(false); break;
            case Transition::CheckboxOn: checkbox_->SetChecked(true); break;
            case Transition::CheckboxOff: checkbox_->SetChecked(false); break;
            case Transition::SliderUp: static_cast<void>(slider_->RequestValue(context.controls, 80.0)); break;
            case Transition::SliderDown: static_cast<void>(slider_->RequestValue(context.controls, 20.0)); break;
            case Transition::PopupOpen: // F4 through the control's own key handler: opens without focus.
            case Transition::PopupClose: static_cast<void>(comboBox_->OnKeyDown(context.controls, VK_F4, 0u)); break;
            case Transition::Count:
            default: break;
        }
    }

private:
    // A page of a title, a line of text and a button; with `progress`, an indeterminate ProgressBar too.
    std::unique_ptr<DxUi::Control> MakePage(bool progress)
    {
        auto page = std::make_unique<DxUi::Panel>();
        page->AddChild<DxUi::Label>(L"Page " + std::to_wstring(++pages_))->SetBounds(D2D1::RectF(8.0f, 8.0f, 216.0f, 32.0f));
        page->AddChild<DxUi::Label>(L"Slides in from the trailing side")->SetBounds(D2D1::RectF(8.0f, 40.0f, 216.0f, 64.0f));
        page->AddChild<DxUi::Button>(L"Continue")->SetBounds(D2D1::RectF(8.0f, 72.0f, 120.0f, 104.0f));
        if (progress)
        {
            auto* bar = page->AddChild<DxUi::ProgressBar>();
            bar->SetIndeterminate(true);
            bar->SetBounds(D2D1::RectF(8.0f, 120.0f, 216.0f, 136.0f));
        }
        return page;
    }

    // The model outlives the tree that borrows it: the runner destroys the host first.
    MotionTreeModel model_;
    DxUi::PageHost* pageHost_ = nullptr;
    DxUi::Tree* tree_         = nullptr;
    DxUi::Toggle* toggle_     = nullptr;
    DxUi::Checkbox* checkbox_ = nullptr;
    DxUi::Slider* slider_     = nullptr;
    DxUi::ComboBox* comboBox_ = nullptr;
    size_t pages_             = 0u;
};

inline constexpr std::array kSources{std::string_view("Tests/Embedded/Embedded.Tests.PerformanceFixtureFx11.h")};

inline constexpr Definition kDefinition{
    .id         = "FX11",
    .title      = "Motion",
    .scenario   = "PageHost slide, Tree expand and collapse, toggle/checkbox/slider state changes, popup open and close, all with motion enabled, sampled at "
                  "30/60/144/240 Hz. Proves G14, G7. 480x360 at 96 DPI, dark theme, motion enabled; one phase per transition and rate, each a 400 ms window "
                  "on the frozen fixture clock (step 0 the state change, then one tick and frame per sample); a last 60 Hz phase slides out a page holding "
                  "an indeterminate ProgressBar. ComboBox popup only: MenuPopup needs a native window.",
    .budgets    = kBudgets,
    .hosts      = kBothHosts,
    .variants   = kVariants,
    .phases     = kPhases,
    .timedPhase = "page-slide-60hz",
    .sources    = kSources,
    .reducedMotion = false,
    .create        = []() -> std::unique_ptr<Scenario> { return std::make_unique<Scene>(); },
};
} // namespace PerformanceFixtures::Fx11
