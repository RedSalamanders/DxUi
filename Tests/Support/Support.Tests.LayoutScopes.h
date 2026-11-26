#pragma once
// Oracle script "layout-scopes" (plan W6.3; Specs/UI/UI_ControlsAndLayout.md, "Layout invalidation scopes"), shared by the
// native suite (Tests/Controls/Controls.Tests.IncrementalParity.cpp) and the embedded suite
// (Tests/Embedded/Embedded.Tests.IncrementalParity.h). A preparation pass now enters only the subtrees that hold pending work,
// and a measure invalidation whose re-measured size is unchanged under the same constraints stops at its control. Each step
// below is one way that could leave a control at a stale size or position; the fresh scene lays out every intrinsic owner
// from dropped measure caches, so any stale bound shows as a pixel or bounds difference.
//
// Scene: a ScrollPanel holding a column FlexPanel (its bounds set by the scene) of two section FlexPanels of four row
// FlexPanels; each row holds a Label with a 120 DIP minimum, a Button and a growing TextField. Beside it, a Grid, a Tree, a
// FlexPanel whose child is arranged at a declared size (a layout root), and a FlexPanel with a Label that rewrites another
// Label's text from OnBoundsChanged (a change made while a layout pass runs).
#include "Support.Tests.IncrementalParity.h"

namespace IncrementalParity::LayoutScopes
{
// Labels are never copied. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
// Rewrites its partner's text to its own arranged width whenever a layout pass moves it.
class EchoLabel final : public DxUi::Label
{
public:
    explicit EchoLabel(std::wstring text) : DxUi::Label(std::move(text))
    {
    }
    DxUi::Label* partner = nullptr;
    void OnBoundsChanged() noexcept override
    {
        DxUi::Label::OnBoundsChanged();
        if (partner)
        {
            const D2D1_RECT_F bounds = GetBounds();
            partner->SetText(std::format(L"Echo {:.0f}", bounds.right - bounds.left));
        }
    }
};
#pragma warning(pop)

enum Slot : size_t
{
    kRoot,
    kScroll,
    kColumn,
    kSectionA,
    kSectionB,
    kRowFirst,                    // kRowFirst .. kRowFirst + 7: the eight rows, four per section
    kLabelFirst = kRowFirst + 8u, // their Labels
    kGrid       = kLabelFirst + 8u,
    kTree,
    kFixedPanel,
    kFixedLabel,
    kEchoPanel,
    kEcho,
    kEchoPartner,
    kPageHost,
    kSlotCount,
};
static_assert(kSlotCount <= kParitySlots);

template <typename Scene> void BuildLayoutScopesScene(Scene& scene)
{
    auto grid = std::make_shared<ParityGridModel>();
    auto tree = std::make_shared<ParityTreeModel>();
    scene.models.push_back(grid);
    scene.models.push_back(tree);
    scene.Host().SetTheme(ParityTheme(false));
    auto& slots    = scene.slots;
    auto root      = std::make_unique<DxUi::Panel>();
    slots[kRoot]   = root.get();
    auto* scroll   = root->AddChild<DxUi::ScrollPanel>();
    slots[kScroll] = scroll;
    scroll->SetBounds(D2D1::RectF(8.0f, 8.0f, 400.0f, 300.0f));
    auto* column   = scroll->AddChild<DxUi::FlexPanel>();
    slots[kColumn] = column;
    static_cast<void>(column->SetGap(4.0f));
    for (size_t section = 0u; section < 2u; ++section)
    {
        auto* sectionPanel         = column->AddChild<DxUi::FlexPanel>();
        slots[kSectionA + section] = sectionPanel;
        static_cast<void>(sectionPanel->SetGap(4.0f));
        for (size_t index = 0u; index < 4u; ++index)
        {
            const size_t row = section * 4u + index;
            auto* line       = sectionPanel->AddChild<DxUi::FlexPanel>();
            static_cast<void>(line->SetOrientation(DxUi::StackOrientation::Horizontal));
            static_cast<void>(line->SetGap(6.0f));
            auto* label = line->AddChild<DxUi::Label>(std::format(L"Row {} \u00E9", row));
            static_cast<void>(line->SetChildOptions(label, DxUi::FlexChildOptions{.constraints = {.minimumDip = D2D1::SizeF(120.0f, 0.0f)}}));
            line->AddChild<DxUi::Button>(std::format(L"Go {}", row));
            auto* field = line->AddChild<DxUi::TextField>(std::format(L"Value {}", row));
            static_cast<void>(line->SetChildOptions(field, DxUi::FlexChildOptions{.constraints = {.minimumDip = D2D1::SizeF(60.0f, 0.0f)}, .grow = 1.0f}));
            slots[kRowFirst + row]   = line;
            slots[kLabelFirst + row] = label;
        }
    }
    // A PageHost in the column, at a declared size, receives the rows the "reparent" step takes from a section.
    auto* pages = column->AddChild<DxUi::PageHost>();
    static_cast<void>(column->SetChildOptions(pages, DxUi::FlexChildOptions{.explicitSizeDip = D2D1::SizeF(360.0f, 36.0f)}));
    slots[kPageHost] = pages;
    column->SetBounds(D2D1::RectF(16.0f, 12.0f, 380.0f, 12.0f + 8.0f * 36.0f));
    scroll->SetContentHeight(8.0f * 36.0f + 24.0f);
    auto* gridControl = root->AddChild<DxUi::Grid>();
    gridControl->SetModel(grid.get());
    gridControl->SetBounds(D2D1::RectF(410.0f, 8.0f, 650.0f, 180.0f));
    slots[kGrid]      = gridControl;
    auto* treeControl = root->AddChild<DxUi::Tree>();
    treeControl->SetModel(tree.get());
    treeControl->SetBounds(D2D1::RectF(410.0f, 188.0f, 650.0f, 330.0f));
    slots[kTree] = treeControl;
    auto* fixed  = root->AddChild<DxUi::FlexPanel>();
    static_cast<void>(fixed->SetOrientation(DxUi::StackOrientation::Horizontal));
    static_cast<void>(fixed->SetGap(6.0f));
    auto* fixedLabel = fixed->AddChild<DxUi::Label>(L"Declared size");
    static_cast<void>(fixed->SetChildOptions(fixedLabel, DxUi::FlexChildOptions{.explicitSizeDip = D2D1::SizeF(110.0f, 24.0f)}));
    fixed->AddChild<DxUi::Button>(L"After the declared size");
    fixed->SetBounds(D2D1::RectF(8.0f, 310.0f, 400.0f, 340.0f));
    slots[kFixedPanel] = fixed;
    slots[kFixedLabel] = fixedLabel;
    auto* echoPanel    = root->AddChild<DxUi::FlexPanel>();
    static_cast<void>(echoPanel->SetOrientation(DxUi::StackOrientation::Horizontal));
    static_cast<void>(echoPanel->SetGap(6.0f));
    auto* partner = echoPanel->AddChild<DxUi::Label>(L"Echo");
    auto* echo    = echoPanel->AddChild<EchoLabel>(L"Echo source");
    static_cast<void>(echoPanel->SetChildOptions(echo, DxUi::FlexChildOptions{.grow = 1.0f}));
    echo->partner = partner;
    echoPanel->SetBounds(D2D1::RectF(8.0f, 348.0f, 400.0f, 376.0f));
    slots[kEchoPanel]   = echoPanel;
    slots[kEcho]        = echo;
    slots[kEchoPartner] = partner;
    scene.SetRoot(std::move(root));
}

template <typename Scene> void StepLayoutScopes(Scene& scene, Rng& rng, size_t step)
{
    auto& slots = scene.slots;
    // A row moved into the PageHost is no longer in a slot; its neighbour's Label stands in (one of the eight always remains,
    // since a section keeps its last row).
    size_t row = rng.Below(8u);
    while (! slots[kLabelFirst + row])
        row = (row + 1u) % 8u;
    const auto label = [&](size_t index) { return static_cast<DxUi::Label*>(slots[kLabelFirst + index]); };
    switch (rng.Below(12u))
    {
        case 0u:
            NoteStep("same-size Label text");
            // Short text inside the 120 DIP minimum: the re-measured size is unchanged.
            label(row)->SetText(std::format(L"R{}", rng.Below(10u)));
            break;
        case 1u:
            NoteStep("different-size Label text");
            label(row)->SetText(rng.Below(2u) ? std::format(L"Row {} with a caption long enough to grow it {}", row, step) : std::format(L"Row {}", row));
            break;
        case 2u:
            NoteStep("multiline Label text in a nested panel");
            label(row)->SetMultiline(true);
            label(row)->SetText(rng.Below(2u) ? L"Two lines\nof text" : L"One line");
            break;
        case 3u:
        {
            NoteStep("visibility");
            constexpr std::array<size_t, 4u> pick{kRowFirst + 1u, kRowFirst + 5u, kLabelFirst + 2u, kSectionB};
            DxUi::Control* control = slots[pick[rng.Below(static_cast<uint32_t>(pick.size()))]];
            if (! control)
                control = slots[kRowFirst + row];
            control->SetVisible(! control->IsVisible());
            break;
        }
        case 4u:
        {
            NoteStep("ScrollPanel content height and column height");
            auto* scroll       = static_cast<DxUi::ScrollPanel*>(slots[kScroll]);
            const float add    = static_cast<float>(rng.Below(4u)) * 40.0f;
            D2D1_RECT_F bounds = slots[kColumn]->GetBounds();
            bounds.bottom      = bounds.top + 8.0f * 36.0f + add;
            slots[kColumn]->SetBounds(bounds);
            scroll->SetContentHeight(8.0f * 36.0f + 24.0f + add);
            scene.Pointer(ParityPointer::Wheel, CenterOf(*scroll), rng.Below(2u) ? -120.0f : 120.0f);
            break;
        }
        case 5u:
            NoteStep("Grid and Tree data");
            static_cast<ParityGridModel*>(scene.models[0].get())->serial += 1u;
            static_cast<DxUi::Grid*>(slots[kGrid])->NotifyDataChanged();
            static_cast<ParityTreeModel*>(scene.models[1].get())->serial += 1u;
            static_cast<DxUi::Tree*>(slots[kTree])->NotifyDataChanged();
            break;
        case 6u:
        {
            NoteStep("dpi");
            constexpr std::array<UINT, 3u> dpis{96u, 144u, 192u};
            scene.SetDpi(dpis[rng.Below(3u)]);
            break;
        }
        case 7u:
        {
            NoteStep("flow direction");
            DxUi::Control* column = slots[kColumn];
            column->SetFlowDirection(column->IsRightToLeft() ? DxUi::FlowDirection::LeftToRight : DxUi::FlowDirection::RightToLeft);
            break;
        }
        case 8u:
        {
            NoteStep("reparent a row into the PageHost");
            // The row leaves its section (which closes up) and becomes the PageHost's page: a FlexPanel laid out by a new parent.
            auto* from = static_cast<DxUi::FlexPanel*>(slots[rng.Below(2u) ? kSectionA : kSectionB]);
            if (from->DebugChildCount() > 1u)
            {
                std::unique_ptr<DxUi::Control> taken = from->TakeChild(0u);
                static_cast<void>(from->CompactChildren());
                for (size_t index = kRowFirst; index < kRowFirst + 8u; ++index)
                    if (slots[index] == taken.get())
                    {
                        // The row and its Label are no longer reached through their slots after the page that holds them goes.
                        slots[index]                             = nullptr;
                        slots[kLabelFirst + (index - kRowFirst)] = nullptr;
                    }
                if (taken)
                    static_cast<DxUi::PageHost*>(slots[kPageHost])->SetPage(std::move(taken));
            }
            break;
        }
        case 9u:
            NoteStep("declared-size Label text");
            static_cast<DxUi::Label*>(slots[kFixedLabel])->SetText(rng.Below(2u) ? L"Declared size with a much longer caption" : L"Declared");
            break;
        case 10u:
        {
            NoteStep("a change made during a layout pass");
            // The echo panel narrows or widens: arranging the EchoLabel rewrites its partner's text, so that pass invalidates again.
            D2D1_RECT_F bounds = slots[kEchoPanel]->GetBounds();
            bounds.right       = 300.0f + static_cast<float>(rng.Below(4u)) * 30.0f;
            slots[kEchoPanel]->SetBounds(bounds);
            break;
        }
        default:
            NoteStep("row field text and font");
            label(row)->SetFontRole(rng.Below(2u) ? DxUi::FontRole::Small : DxUi::FontRole::Body);
            break;
    }
}

inline constexpr std::array<uint64_t, 3u> kLayoutScopesSeeds{0x06030001u, 0x06030002u, 0x06030003u};

template <typename Scene> [[nodiscard]] ParityScript<Scene> LayoutScopesScript()
{
    return {"layout-scopes", kLayoutScopesSeeds, 40u, &BuildLayoutScopesScene<Scene>, &StepLayoutScopes<Scene>};
}
} // namespace IncrementalParity::LayoutScopes
