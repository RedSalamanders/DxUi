#pragma once
// FX12 Dense screen (plan W0.4): 2,000 controls filling 1920x1080 DIP (3840x2160 at 192 DPI in variant uhd-192dpi), a
// 10,000-control scrolling form (variant form-10k), and the dense screen using 120 distinct text-format keys (variant
// typography-120). Variants form-layout (200 rows) and form-layout-10k (2,000 rows; form-layout-10k-uhd at 3840x2160 and
// 192 DPI) arrange the form's rows with the library's FlexPanels (W6.2), with two more phases: a label that grows and shrinks, and a column width change. Full
// frames, one hover change, one label text change and wheel scrolling. G5 (full-frame CPU and GPU time at 4K) is a hardware timing verdict and stays "not
// measured" in WARP receipts; they record the work of each frame and the scene's tracked bytes, from which per-control memory follows (G13).
#include "Embedded.Tests.PerformanceFixture.h"

#include <format>

namespace PerformanceFixtures::Fx12
{
inline constexpr size_t kDenseColumns          = 40u;
inline constexpr size_t kDenseRows             = 50u;
inline constexpr size_t kDenseControls         = kDenseColumns * kDenseRows; // 2,000
inline constexpr size_t kFormRows              = 2'000u;
inline constexpr size_t kFormRowControls       = 5u; // label, text field, checkbox, toggle, button: 10,000 controls
inline constexpr float kFormRowDip             = 32.0f;
inline constexpr size_t kTypographyKeys        = 120u;
inline constexpr size_t kFormLayoutRows        = 200u; // form-layout: 1,000 controls
inline constexpr size_t kFormLayoutSectionRows = 100u;
inline constexpr float kFormLayoutGapDip       = 4.0f;

inline constexpr std::array kBudgets{std::string_view("G5"), std::string_view("G15"), std::string_view("G13"), std::string_view("G2")};
inline constexpr std::array kVariants{
    // The suites' variant: the 2,000-control screen at 96 DPI, the same DIP layout as uhd-192dpi.
    Variant{"default", 1920u, 1080u, 96.0f, false, false},
    // Labels and small Trees of the dense screen use 120 distinct (size, weight) typographies; Tree rows request theirs
    // on every paint (W2.8).
    Variant{"typography-120", 1920u, 1080u, 96.0f, false, false},
    // G5 and G4 at 4K and 192 DPI; G5 itself is timed on hardware only (W0.7).
    Variant{"uhd-192dpi", 3840u, 2160u, 192.0f, false, true},
    // 10,000 controls in one ScrollPanel: G15, G13 per control, W4.8, W8.3, W9.6, W9.7.
    Variant{"form-10k", 1920u, 1080u, 96.0f, false, true},
    // The same rows laid out by the library (W6.2): 200 rows (1,000 controls) in suite runs, measured by FlexPanels.
    Variant{"form-layout", 1920u, 1080u, 96.0f, false, false},
    // 2,000 rows (10,000 controls) laid out by the library: G15 and the W6.2, W6.3 and W4.8 baselines at full size.
    Variant{"form-layout-10k", 1920u, 1080u, 96.0f, false, true},
    // form-layout-10k at 3840x2160 and 192 DPI, the same DIP layout: the hardware attribution receipt (W0.7).
    Variant{"form-layout-10k-uhd", 3840u, 2160u, 192.0f, true, true},
};
inline constexpr size_t kWarm  = 0u;
inline constexpr size_t kFull  = 1u;
inline constexpr size_t kHover = 2u;
inline constexpr size_t kValue = 3u;
inline constexpr size_t kWheel = 4u;
inline constexpr size_t kSize  = 5u;
inline constexpr size_t kWidth = 6u;
inline constexpr std::array kPhases{
    Phase{"warm", 3u, PhaseKind::Warmup, "an unchanged frame, then the pointer moves onto the first and the second hover button"},
    Phase{"full", 4u, PhaseKind::Measured, "ControlHost::Invalidate with nothing changed: a full frame"},
    Phase{"hover", 6u, PhaseKind::Measured, "the pointer moves between two adjacent buttons: one hover change per frame"},
    Phase{"value", 4u, PhaseKind::Measured, "one Label's text changes, its bounds unchanged"},
    Phase{"scroll",
          6u,
          PhaseKind::Measured,
          "one wheel notch down over the form (form-10k); the dense screen has nothing to scroll, so its frames are unchanged"},
    Phase{"size", 4u, PhaseKind::Measured, "one Label's text grows and shrinks, moving its row (form-layout variants; elsewhere unchanged frames)"},
    Phase{"width", 4u, PhaseKind::Measured, "the form column narrows by 40 DIP and widens back (form-layout variants; elsewhere unchanged frames)"},
};
inline constexpr std::array kSources{std::string_view("Tests/Embedded/Embedded.Tests.PerformanceFixtureFx12.h")};

// Distinct (size, weight) pairs: 24 sizes from 9 to 20.5 DIP and five weights from 300 to 700.
[[nodiscard]] inline float TypographySize(size_t key) noexcept
{
    return 9.0f + static_cast<float>(key % 24u) * 0.5f;
}
[[nodiscard]] inline DWRITE_FONT_WEIGHT TypographyWeight(size_t key) noexcept
{
    return static_cast<DWRITE_FONT_WEIGHT>(300u + (key / 24u % 5u) * 100u);
}

class TinyTreeModel final : public DxUi::ITreeModel
{
public:
    [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
    {
        return 2u;
    }
    void GetVisibleItem(size_t index, DxUi::TreeItemData& item) const override
    {
        item.id          = index;
        item.depth       = static_cast<uint32_t>(index);
        item.parentId    = index == 0u ? std::nullopt : std::optional<uint64_t>(0u);
        item.hasChildren = index == 0u;
        item.expanded    = index == 0u;
        item.text        = index == 0u ? L"Node" : L"Leaf";
    }
};

class Scene final : public Scenario
{
public:
    HRESULT Build(Context& context) override
    {
        const float width  = static_cast<float>(context.variant.width) * 96.0f / context.variant.dpi;
        const float height = static_cast<float>(context.variant.height) * 96.0f / context.variant.dpi;
        auto root          = std::make_unique<DxUi::Panel>();
        if (context.variant.name == "form-10k")
            BuildForm(*root, width, height);
        else if (context.variant.name == "form-layout")
            BuildFormLayout(*root, width, height, kFormLayoutRows);
        else if (context.variant.name == "form-layout-10k" || context.variant.name == "form-layout-10k-uhd")
            BuildFormLayout(*root, width, height, kFormRows);
        else
            BuildDense(*root, width, height, context.variant.name == "typography-120");
        context.controls.SetRoot(std::move(root));
        return S_OK;
    }
    void Step(Context& context, size_t phase, size_t step) override
    {
        switch (phase)
        {
            case kWarm:
                if (step > 0u)
                    MovePointer(context, step % 2u == 1u ? *hoverA_ : *hoverB_);
                break;
            case kFull: context.controls.Invalidate(); break;
            case kHover: MovePointer(context, step % 2u == 0u ? *hoverA_ : *hoverB_); break;
            case kValue: valueLabel_->SetText(std::format(L"Value {}", step % 2u == 0u ? 1 : 2)); break;
            case kWheel:
                if (scroll_)
                {
                    const D2D1_RECT_F bounds = scroll_->GetBounds();
                    const float scale        = context.variant.dpi / 96.0f;
                    DxUi::PointerEvent event{
                        DxUi::PointerAction::Wheel, (bounds.left + bounds.right) * 0.5f * scale, (bounds.top + bounds.bottom) * 0.5f * scale};
                    event.wheelDelta = -120.0f;
                    static_cast<void>(context.port.Pointer(event));
                }
                break;
            case kSize:
                if (sizeLabel_)
                    sizeLabel_->SetText(step % 2u == 0u ? L"Field 0003 with a caption long enough to move its row" : L"Field 0003");
                break;
            case kWidth:
                if (column_)
                {
                    D2D1_RECT_F bounds = column_->GetBounds();
                    bounds.right       = columnRight_ - (step % 2u == 0u ? 40.0f : 0.0f);
                    column_->SetBounds(bounds);
                }
                break;
            default: break;
        }
    }

private:
    static void MovePointer(Context& context, const DxUi::Control& control)
    {
        const D2D1_RECT_F bounds = control.GetBounds();
        const float scale        = context.variant.dpi / 96.0f;
        static_cast<void>(
            context.port.Pointer({DxUi::PointerAction::Move, (bounds.left + bounds.right) * 0.5f * scale, (bounds.top + bounds.bottom) * 0.5f * scale}));
    }

    // 40 columns by 50 rows of cells, cycling label, label, label, button, checkbox, toggle, progress bar, slider (with
    // typography: a two-row Tree in place of the first label, and every label and Tree typed by one of 120 typographies).
    void BuildDense(DxUi::Panel& root, float width, float height, bool typography)
    {
        const float cellWidth  = (width - 16.0f) / static_cast<float>(kDenseColumns);
        const float cellHeight = (height - 16.0f) / static_cast<float>(kDenseRows);
        size_t trees           = 0u;
        size_t labels          = 0u;
        for (size_t index = 0u; index < kDenseControls; ++index)
        {
            const float x            = 8.0f + static_cast<float>(index % kDenseColumns) * cellWidth;
            const float y            = 8.0f + static_cast<float>(index / kDenseColumns) * cellHeight;
            const D2D1_RECT_F bounds = D2D1::RectF(x + 1.0f, y + 1.0f, x + cellWidth - 1.0f, y + cellHeight - 1.0f);
            DxUi::Control* control   = nullptr;
            switch (index % 8u)
            {
                case 0u:
                    if (typography)
                    {
                        auto* tree = root.AddChild<DxUi::Tree>();
                        tree->SetModel(&tinyTree_);
                        const auto key = trees++ % kTypographyKeys;
                        static_cast<void>(
                            tree->SetTypographyOverride(DxUi::Tree::TypographyOverride{TypographySize(key), TypographyWeight(key), TypographyWeight(key)}));
                        control = tree;
                        break;
                    }
                    [[fallthrough]];
                case 1u:
                case 2u:
                {
                    auto* label = root.AddChild<DxUi::Label>(std::format(L"L{}", index));
                    if (typography)
                    {
                        const auto key = labels++ % kTypographyKeys;
                        static_cast<void>(label->SetTypographyOverride(DxUi::LabelTypography{TypographySize(key), TypographyWeight(key), 0.0f}));
                    }
                    if (! valueLabel_)
                        valueLabel_ = label;
                    control = label;
                    break;
                }
                case 3u:
                {
                    auto* button = root.AddChild<DxUi::Button>(std::format(L"B{}", index));
                    if (! hoverA_)
                        hoverA_ = button;
                    else if (! hoverB_)
                        hoverB_ = button;
                    control = button;
                    break;
                }
                case 4u:
                {
                    auto* checkbox = root.AddChild<DxUi::Checkbox>(L"C");
                    checkbox->SetChecked(index % 3u == 0u);
                    control = checkbox;
                    break;
                }
                case 5u:
                {
                    auto* toggle = root.AddChild<DxUi::Toggle>();
                    toggle->SetChecked(index % 3u == 1u);
                    control = toggle;
                    break;
                }
                case 6u:
                {
                    auto* progress = root.AddChild<DxUi::ProgressBar>();
                    progress->SetValue(static_cast<double>(index % 100u));
                    control = progress;
                    break;
                }
                default:
                {
                    auto* slider = root.AddChild<DxUi::Slider>();
                    slider->SetValue(static_cast<double>((index * 7u) % 100u));
                    control = slider;
                    break;
                }
            }
            control->SetBounds(bounds);
        }
    }

    // One ScrollPanel over 2,000 rows of label, text field, checkbox, toggle and button, positioned in content space.
    void BuildForm(DxUi::Panel& root, float width, float height)
    {
        scroll_ = root.AddChild<DxUi::ScrollPanel>();
        scroll_->SetBounds(D2D1::RectF(8.0f, 8.0f, width - 8.0f, height - 8.0f));
        for (size_t row = 0u; row < kFormRows; ++row)
        {
            const float top    = 8.0f + 4.0f + static_cast<float>(row) * kFormRowDip;
            const float bottom = top + kFormRowDip - 4.0f;
            auto* label        = scroll_->AddChild<DxUi::Label>(std::format(L"Field {:04}", row));
            label->SetBounds(D2D1::RectF(16.0f, top, 176.0f, bottom));
            if (row == 2u)
                valueLabel_ = label;
            scroll_->AddChild<DxUi::TextField>(std::format(L"Value of field {}", row))->SetBounds(D2D1::RectF(184.0f, top, 424.0f, bottom));
            auto* checkbox = scroll_->AddChild<DxUi::Checkbox>(L"Enabled");
            checkbox->SetChecked(row % 2u == 0u);
            checkbox->SetBounds(D2D1::RectF(432.0f, top, 552.0f, bottom));
            auto* toggle = scroll_->AddChild<DxUi::Toggle>(L"Sync");
            toggle->SetChecked(row % 3u == 0u);
            toggle->SetBounds(D2D1::RectF(560.0f, top, 680.0f, bottom));
            auto* button = scroll_->AddChild<DxUi::Button>(L"Apply");
            button->SetBounds(D2D1::RectF(688.0f, top, 788.0f, bottom));
            if (row == 0u)
                hoverA_ = button;
            else if (row == 1u)
                hoverB_ = button;
        }
        scroll_->SetContentHeight(static_cast<float>(kFormRows) * kFormRowDip + 8.0f);
    }

    // The same rows arranged by the library, as a real long form would be: a column FlexPanel of sections (100 rows each, as
    // a FlexPanel holds at most 1,024 children) of row FlexPanels, inside the ScrollPanel. Labels keep a 160 DIP minimum and
    // the text field grows. Row 2's label shows "Value 0", so the value phase changes text without changing its size.
    void BuildFormLayout(DxUi::Panel& root, float width, float height, size_t rows)
    {
        scroll_ = root.AddChild<DxUi::ScrollPanel>();
        scroll_->SetBounds(D2D1::RectF(8.0f, 8.0f, width - 8.0f, height - 8.0f));
        column_ = scroll_->AddChild<DxUi::FlexPanel>();
        static_cast<void>(column_->SetGap(kFormLayoutGapDip));
        DxUi::FlexPanel* section = nullptr;
        for (size_t row = 0u; row < rows; ++row)
        {
            if (row % kFormLayoutSectionRows == 0u)
            {
                section = column_->AddChild<DxUi::FlexPanel>();
                static_cast<void>(section->SetGap(kFormLayoutGapDip));
            }
            auto* line = section->AddChild<DxUi::FlexPanel>();
            static_cast<void>(line->SetOrientation(DxUi::StackOrientation::Horizontal));
            static_cast<void>(line->SetGap(8.0f));
            auto* label = line->AddChild<DxUi::Label>(row == 2u ? std::wstring(L"Value 0") : std::format(L"Field {:04}", row));
            static_cast<void>(line->SetChildOptions(label, DxUi::FlexChildOptions{.constraints = {.minimumDip = D2D1::SizeF(160.0f, 0.0f)}}));
            if (row == 2u)
                valueLabel_ = label;
            else if (row == 3u)
                sizeLabel_ = label;
            auto* field = line->AddChild<DxUi::TextField>(std::format(L"Value of field {}", row));
            static_cast<void>(line->SetChildOptions(field, DxUi::FlexChildOptions{.constraints = {.minimumDip = D2D1::SizeF(120.0f, 0.0f)}, .grow = 1.0f}));
            auto* checkbox = line->AddChild<DxUi::Checkbox>(L"Enabled");
            checkbox->SetChecked(row % 2u == 0u);
            auto* toggle = line->AddChild<DxUi::Toggle>(L"Sync");
            toggle->SetChecked(row % 3u == 0u);
            auto* button = line->AddChild<DxUi::Button>(L"Apply");
            if (row == 0u)
                hoverA_ = button;
            else if (row == 1u)
                hoverB_ = button;
        }
        const size_t sections     = (rows + kFormLayoutSectionRows - 1u) / kFormLayoutSectionRows;
        const float contentHeight = static_cast<float>(rows) * kFormRowDip + static_cast<float>(rows - sections) * kFormLayoutGapDip +
                                    static_cast<float>(sections - 1u) * kFormLayoutGapDip;
        columnRight_              = width - 24.0f;
        column_->SetBounds(D2D1::RectF(16.0f, 12.0f, columnRight_, 12.0f + contentHeight));
        scroll_->SetContentHeight(contentHeight + 16.0f);
    }

    TinyTreeModel tinyTree_;
    DxUi::ScrollPanel* scroll_ = nullptr;
    DxUi::Label* valueLabel_   = nullptr;
    DxUi::Label* sizeLabel_    = nullptr;
    DxUi::FlexPanel* column_   = nullptr;
    float columnRight_         = 0.0f;
    DxUi::Button* hoverA_      = nullptr;
    DxUi::Button* hoverB_      = nullptr;
};

inline constexpr Definition kDefinition{
    .id         = "FX12",
    .title      = "Dense screen",
    .scenario   = "Variants default and uhd-192dpi: 2,000 controls (a 40 x 50 grid of cells cycling three Labels, Button, Checkbox, Toggle, ProgressBar, "
                  "Slider) filling 1920x1080 DIP, at 1920x1080 and 96 DPI or at 3840x2160 and 192 DPI. typography-120: the same screen with a two-row "
                  "Tree in place of every eighth cell's Label and every Label and Tree typed by one of 120 distinct (size, weight) typographies. form-10k: "
                  "10,000 controls in one ScrollPanel, 2,000 rows of Label, TextField, Checkbox, Toggle and Button, absolutely positioned. form-layout, "
                  "form-layout-10k and form-layout-10k-uhd (3840x2160, 192 DPI): "
                  "the same rows (200 or 2,000) arranged by the library, a column FlexPanel of 100-row section FlexPanels of row FlexPanels inside the "
                  "ScrollPanel. Reduced motion. Phases: "
                  "full frames, one hover change, one Label text change, wheel scrolling, a Label that grows and shrinks, a column width change. Per-control "
                  "memory is the built sample's tracked live bytes over "
                  "the control count. G5 is a hardware timing verdict: not measured here.",
    .budgets    = kBudgets,
    .hosts      = kBothHosts,
    .variants   = kVariants,
    .phases     = kPhases,
    .timedPhase = "full",
    .sources    = kSources,
    .create     = []() -> std::unique_ptr<Scenario> { return std::make_unique<Scene>(); },
    // Every "full" step invalidates an unchanged screen: the timing harness repeats it instead of rebuilding 2,000
    // controls every four frames.
    .timedPhaseRepeats = true,
};
} // namespace PerformanceFixtures::Fx12
