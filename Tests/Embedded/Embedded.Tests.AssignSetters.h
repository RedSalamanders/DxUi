#pragma once
// One way to assign a property (plan W2.10; UI_ControlsAndLayout.md, "Assigning a property"), on FX5: the ComplexUi scene of
// Samples/ComplexUi/ComplexUiScene.h, on its embedded host, with UI Automation's system-listener override on so that a host
// would build at a mutation. Every public setter that assigns through Control::Assign is a row of kRows, named as the tooling
// inventory names it (Tools/AssignSetters.psm1 fails a setter of kind Assign that no row names). For each row a control of
// the class is attached to the scene's root, the setter is given value A, then value B, then B again:
// - B again (unchanged) causes 0 invalidations, 0 measure invalidations, 0 hit-revision moves and 0 accessibility mutations;
// - B (changed) causes what the row's Affects declares: an invalidation for Paint, a measure invalidation for Layout, a
//   hit-revision move for Hit, an accessibility mutation for a semantic aspect (with an invalidation: an embedded view such as
//   this one publishes at its next preparation); and, unless the row's control is a composite
//   whose parts route their own changes, none of what it does not declare.
// Included by Embedded.Tests.Embedded.cpp, which defines Check and Hr.
#include "ComplexUiScene.h"
#include "Controls/DxUi.Internal.h"
#include "Support/WorkCounters.h"

#include <array>
#include <set>
#include <string>
#include <string_view>

namespace AssignSetterGate
{
using DxUi::Control;
using Affects = DxUi::Control::Affects;

// Supporting objects some rows refer to, made once per run.
struct Shared final
{
    DxUi::RadioButtons* group = nullptr;
};
inline Shared g_shared{};

struct Row final
{
    std::string_view setter;
    Affects affects = Affects::None;
    // A composite whose parts (children) assign and route their own properties: only what Affects declares is checked.
    bool parts                                      = false;
    Control* (*create)(ComplexUiScene& scene)       = nullptr;
    void (*set)(Control& control, bool secondValue) = nullptr;
};

[[nodiscard]] inline DxUi::Panel& Root(ComplexUiScene& scene)
{
    auto* const root = dynamic_cast<DxUi::Panel*>(scene.view.Controls().GetRoot());
    Check(root != nullptr, "FX5 root is a panel");
    return *root;
}

template <typename T> [[nodiscard]] T* Add(ComplexUiScene& scene, D2D1_RECT_F bounds = D2D1::RectF(16, 16, 416, 216))
{
    T* const control = Root(scene).AddChild<T>();
    Check(control != nullptr, "row control attached to the FX5 root");
    control->SetBounds(bounds);
    return control;
}

template <typename T> [[nodiscard]] T& As(Control& control)
{
    auto* const typed = dynamic_cast<T*>(&control);
    Check(typed != nullptr, "row control has the row's class");
    return *typed;
}

inline constexpr Affects P   = Affects::Paint;
inline constexpr Affects L   = Affects::Layout;
inline constexpr Affects H   = Affects::Hit;
inline constexpr Affects N   = Affects::SemanticName;
inline constexpr Affects V   = Affects::SemanticValue;
inline constexpr Affects S   = Affects::SemanticState;
inline constexpr Affects Non = Affects::None;

inline Control* NewLabel(ComplexUiScene& scene)
{
    return Add<DxUi::Label>(scene);
}
inline Control* NewButton(ComplexUiScene& scene)
{
    return Add<DxUi::Button>(scene);
}
inline Control* NewTextField(ComplexUiScene& scene)
{
    return Add<DxUi::TextField>(scene);
}
inline Control* NewPasswordField(ComplexUiScene& scene)
{
    auto* const field = Add<DxUi::TextField>(scene);
    field->SetMasked(true);
    field->SetPasswordRevealMode(DxUi::PasswordRevealMode::Peek);
    return field;
}
inline Control* NewComboBox(ComplexUiScene& scene)
{
    auto* const combo = Add<DxUi::ComboBox>(scene);
    combo->SetItems({{L"a", L"A"}, {L"b", L"B"}, {L"c", L"C"}});
    return combo;
}
inline Control* NewTree(ComplexUiScene& scene)
{
    auto* const tree = Add<DxUi::Tree>(scene);
    tree->SetModel(&scene.model);
    return tree;
}
inline Control* NewGrid(ComplexUiScene& scene)
{
    auto* const grid = Add<DxUi::Grid>(scene);
    grid->SetModel(&scene.model);
    return grid;
}
inline Control* NewTabs(ComplexUiScene& scene)
{
    auto* const tabs = Add<DxUi::TabControl>(scene);
    static_cast<void>(tabs->AddTab<DxUi::Label>(L"One"));
    static_cast<void>(tabs->AddTab<DxUi::Label>(L"Two"));
    return tabs;
}
inline Control* NewScrollPanel(ComplexUiScene& scene)
{
    auto* const panel = Add<DxUi::ScrollPanel>(scene, D2D1::RectF(16, 16, 216, 116));
    panel->SetContentHeight(1000.0f);
    return panel;
}

inline const std::array<double, 1> kSamplesA{1.0};
inline const std::array<double, 2> kSamplesB{1.0, 2.0};
[[nodiscard]] inline std::array<DxUi::ThroughputGraphSample, 1> GraphSamples(double value)
{
    std::array<DxUi::ThroughputGraphSample, 1> samples{};
    samples[0].value = value;
    return samples;
}

// clang-format off
inline const std::array kRows{
    // Control (on a Label).
    Row{"Control::SetFocusable", S, false, NewLabel, [](Control& c, bool b) { c.SetFocusable(b); }},
    Row{"Control::SetMnemonic", P, false, NewLabel, [](Control& c, bool b) { c.SetMnemonic(b ? L'b' : L'a'); }},
    Row{"Control::SetConnectedAnimationKey", Non, false, NewLabel, [](Control& c, bool b) { c.SetConnectedAnimationKey(b ? L"b" : L"a"); }},
    Row{"Control::SetTooltipText", Non, false, NewLabel, [](Control& c, bool b) { c.SetTooltipText(b ? L"b" : L"a"); }},
    Row{"Control::SetAccessibleName", N, false, NewLabel, [](Control& c, bool b) { c.SetAccessibleName(b ? L"b" : L"a"); }},
    Row{"Control::SetAccessibleHelpText", N, false, NewLabel, [](Control& c, bool b) { c.SetAccessibleHelpText(b ? L"b" : L"a"); }},
    Row{"Control::SetAccessibleAutomationId", N, false, NewLabel, [](Control& c, bool b) { c.SetAccessibleAutomationId(b ? L"b" : L"a"); }},
    Row{"Control::SetAccessibilityRole", S, false, NewLabel, [](Control& c, bool b) { c.SetAccessibilityRole(b ? DxUi::AccessibilityRole::Status : DxUi::AccessibilityRole::Default); }},
    // CardPanel, Label, Separator.
    Row{"CardPanel::SetCornerRadius", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::CardPanel>(s); }, [](Control& c, bool b) { As<DxUi::CardPanel>(c).SetCornerRadius(b ? 6.0f : 4.0f); }},
    Row{"Label::SetText", L | P | N, false, NewLabel, [](Control& c, bool b) { As<DxUi::Label>(c).SetText(b ? L"Second" : L"First"); }},
    Row{"Label::SetFontRole", L | P, false, NewLabel, [](Control& c, bool b) { As<DxUi::Label>(c).SetFontRole(b ? DxUi::FontRole::Title : DxUi::FontRole::Small); }},
    Row{"Label::SetAlignment", P, false, NewLabel, [](Control& c, bool b) { As<DxUi::Label>(c).SetAlignment(b ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_TRAILING); }},
    Row{"Label::SetMultiline", L | P, false, NewLabel, [](Control& c, bool b) { As<DxUi::Label>(c).SetMultiline(b); }},
    Row{"Label::SetTextColor", P, false, NewLabel, [](Control& c, bool b) { As<DxUi::Label>(c).SetTextColor(D2D1::ColorF(b ? 0.5f : 0.25f, 0.0f, 0.0f, 1.0f)); }},
    Row{"Label::SetTypographyOverride", L | P, false, NewLabel, [](Control& c, bool b) { static_cast<void>(As<DxUi::Label>(c).SetTypographyOverride(DxUi::LabelTypography{.fontSizeDip = b ? 20.0f : 18.0f})); }},
    Row{"Label::SetMnemonicTarget", N, false, NewLabel, [](Control& c, bool b) { As<DxUi::Label>(c).SetMnemonicTarget(b ? &c : nullptr); }},
    Row{"Separator::SetOrientation", L | P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::Separator>(s); }, [](Control& c, bool b) { As<DxUi::Separator>(c).SetOrientation(b ? DxUi::SeparatorOrientation::Vertical : DxUi::SeparatorOrientation::Horizontal); }},
    // Button and its toggles.
    Row{"Button::SetText", L | P | N, false, NewButton, [](Control& c, bool b) { As<DxUi::Button>(c).SetText(b ? L"Second" : L"First"); }},
    Row{"Button::SetMultiline", L | P, false, NewButton, [](Control& c, bool b) { As<DxUi::Button>(c).SetMultiline(b); }},
    Row{"Button::SetContentFontSizeDip", L | P, false, NewButton, [](Control& c, bool b) { static_cast<void>(As<DxUi::Button>(c).SetContentFontSizeDip(b ? 18.0f : 16.0f)); }},
    Row{"Button::SetPrimary", P, false, NewButton, [](Control& c, bool b) { As<DxUi::Button>(c).SetPrimary(b); }},
    Row{"Button::SetThemePreview", P, false, NewButton, [](Control& c, bool b) { DxUi::ButtonThemePreview preview{}; preview.swatches[0] = D2D1::ColorF(b ? 0.5f : 0.25f, 0.0f, 0.0f, 1.0f); As<DxUi::Button>(c).SetThemePreview(preview); }},
    Row{"Button::SetToggleState", P | V, false, NewButton, [](Control& c, bool b) { As<DxUi::Button>(c).SetToggleState(b); }},
    Row{"Button::SetVariant", L | P, false, NewButton, [](Control& c, bool b) { As<DxUi::Button>(c).SetVariant(b ? DxUi::ButtonVariant::DropDown : DxUi::ButtonVariant::Standard); }},
    Row{"Button::SetAppearance", P, false, NewButton, [](Control& c, bool b) { As<DxUi::Button>(c).SetAppearance(b ? DxUi::ButtonAppearance::Outline : DxUi::ButtonAppearance::Subtle); }},
    Row{"Button::SetLeadingIcon", L | P, false, NewButton, [](Control& c, bool b) { As<DxUi::Button>(c).SetLeadingIcon(b ? L"\xE710" : L"\xE711"); }},
    Row{"Button::SetShortcutText", L | P, false, NewButton, [](Control& c, bool b) { As<DxUi::Button>(c).SetShortcutText(b ? L"Ctrl+B" : L"Ctrl+A"); }},
    Row{"Button::SetDisclosureCollapsedDirection", P, false, NewButton, [](Control& c, bool b) { As<DxUi::Button>(c).SetDisclosureCollapsedDirection(b ? DxUi::ChevronDirection::Left : DxUi::ChevronDirection::Right); }},
    Row{"Button::SetPressedVisual", P, false, NewButton, [](Control& c, bool b) { As<DxUi::Button>(c).SetPressedVisual(b); }},
    Row{"Toggle::SetChecked", P | S, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::Toggle>(s); }, [](Control& c, bool b) { As<DxUi::Toggle>(c).SetChecked(b); }},
    Row{"Toggle::SetStateLabels", L | P | N, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::Toggle>(s); }, [](Control& c, bool b) { As<DxUi::Toggle>(c).SetStateLabels(b ? L"Off2" : L"Off", L"On"); }},
    Row{"Checkbox::SetIndeterminate", P | S, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::Checkbox>(s); }, [](Control& c, bool b) { As<DxUi::Checkbox>(c).SetIndeterminate(b); }},
    Row{"RadioButton::SetGroup", Non, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::RadioButton>(s); }, [](Control& c, bool b) { As<DxUi::RadioButton>(c).SetGroup(b ? g_shared.group : nullptr); }},
    Row{"RadioButtons::SetHeader", P | N, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::RadioButtons>(s); }, [](Control& c, bool b) { As<DxUi::RadioButtons>(c).SetHeader(b ? L"Second" : L"First"); }},
    // Range and value controls.
    Row{"ProgressBar::SetValue", P | V, false, [](ComplexUiScene& s) -> Control* { return s.progress[0]; }, [](Control& c, bool b) { As<DxUi::ProgressBar>(c).SetValue(b ? 70.0 : 60.0); }},
    Row{"ProgressBar::SetMinimum", P | V, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ProgressBar>(s); }, [](Control& c, bool b) { As<DxUi::ProgressBar>(c).SetMinimum(b ? 2.0 : 1.0); }},
    Row{"ProgressBar::SetMaximum", P | V, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ProgressBar>(s); }, [](Control& c, bool b) { As<DxUi::ProgressBar>(c).SetMaximum(b ? 200.0 : 150.0); }},
    Row{"ProgressBar::SetIndeterminate", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ProgressBar>(s); }, [](Control& c, bool b) { As<DxUi::ProgressBar>(c).SetIndeterminate(b); }},
    Row{"ProgressBar::SetAnimationPaused", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ProgressBar>(s); }, [](Control& c, bool b) { As<DxUi::ProgressBar>(c).SetAnimationPaused(b); }},
    Row{"ProgressBar::SetTrackHeightDip", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ProgressBar>(s); }, [](Control& c, bool b) { As<DxUi::ProgressBar>(c).SetTrackHeightDip(b ? 6.0f : 5.0f); }},
    Row{"ProgressBar::SetSegmentedValues", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ProgressBar>(s); }, [](Control& c, bool b) { As<DxUi::ProgressBar>(c).SetSegmentedValues(30.0, b ? 50.0 : 40.0, D2D1::ColorF(0.2f, 0.4f, 0.6f, 1.0f)); }},
    Row{"PageIndicator::SetPageCount", P | H | V, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::PageIndicator>(s); }, [](Control& c, bool b) { As<DxUi::PageIndicator>(c).SetPageCount(b ? 6u : 5u); }},
    Row{"PageIndicator::SetSelectedIndex", P | V, false, [](ComplexUiScene& s) -> Control* { auto* indicator = Add<DxUi::PageIndicator>(s); indicator->SetPageCount(5u); return indicator; }, [](Control& c, bool b) { As<DxUi::PageIndicator>(c).SetSelectedIndex(b ? 2u : 1u); }},
    Row{"Slider::SetOrientation", P | H, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::Slider>(s); }, [](Control& c, bool b) { As<DxUi::Slider>(c).SetOrientation(b ? DxUi::SliderOrientation::Vertical : DxUi::SliderOrientation::Horizontal); }},
    Row{"Slider::SetMinimum", P | V, false, [](ComplexUiScene& s) -> Control* { return s.sliders[1]; }, [](Control& c, bool b) { As<DxUi::Slider>(c).SetMinimum(b ? -2.0 : -1.0); }},
    Row{"Slider::SetMaximum", P | V, false, [](ComplexUiScene& s) -> Control* { return s.sliders[2]; }, [](Control& c, bool b) { As<DxUi::Slider>(c).SetMaximum(b ? 200.0 : 150.0); }},
    Row{"Slider::SetValue", P | V, false, [](ComplexUiScene& s) -> Control* { return s.sliders[0]; }, [](Control& c, bool b) { As<DxUi::Slider>(c).SetValue(b ? 70.0 : 60.0); }},
    Row{"Slider::SetStep", V, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::Slider>(s); }, [](Control& c, bool b) { As<DxUi::Slider>(c).SetStep(b ? 2.0 : 0.5); }},
    Row{"Slider::SetLargeStep", V, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::Slider>(s); }, [](Control& c, bool b) { As<DxUi::Slider>(c).SetLargeStep(b ? 20.0 : 15.0); }},
    Row{"Slider::SetTickMarks", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::Slider>(s); }, [](Control& c, bool b) { As<DxUi::Slider>(c).SetTickMarks(b ? std::vector<double>{0.0, 0.5} : std::vector<double>{0.25}); }},
    Row{"ThroughputGraph::SetSamples", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ThroughputGraph>(s); }, [](Control& c, bool b) { const auto samples = GraphSamples(b ? 2.0 : 1.0); As<DxUi::ThroughputGraph>(c).SetSamples(samples); }},
    Row{"ThroughputGraph::SetSecondarySamples", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ThroughputGraph>(s); }, [](Control& c, bool b) { As<DxUi::ThroughputGraph>(c).SetSecondarySamples(b ? std::span<const double>(kSamplesB) : std::span<const double>(kSamplesA)); }},
    Row{"ThroughputGraph::SetSecondarySeriesColor", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ThroughputGraph>(s); }, [](Control& c, bool b) { As<DxUi::ThroughputGraph>(c).SetSecondarySeriesColor(D2D1::ColorF(b ? 0.5f : 0.25f, 0.0f, 0.0f, 1.0f)); }},
    Row{"ThroughputGraph::SetLimit", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ThroughputGraph>(s); }, [](Control& c, bool b) { As<DxUi::ThroughputGraph>(c).SetLimit(b ? 2.0 : 1.0); }},
    Row{"ThroughputGraph::SetCurrentValueMarker", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ThroughputGraph>(s); }, [](Control& c, bool b) { As<DxUi::ThroughputGraph>(c).SetCurrentValueMarker(b ? 2.0 : 1.0, L"now", L"MB/s"); }},
    Row{"ThroughputGraph::SetOverlayText", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ThroughputGraph>(s); }, [](Control& c, bool b) { As<DxUi::ThroughputGraph>(c).SetOverlayText(b ? L"b" : L"a"); }},
    Row{"ThroughputGraph::SetRainbowMode", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ThroughputGraph>(s); }, [](Control& c, bool b) { As<DxUi::ThroughputGraph>(c).SetRainbowMode(b); }},
    Row{"ThroughputGraph::SetPerStreamBands", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ThroughputGraph>(s); }, [](Control& c, bool b) { As<DxUi::ThroughputGraph>(c).SetPerStreamBands(b); }},
    Row{"ThroughputGraph::SetTransitionDuration", Non, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ThroughputGraph>(s); }, [](Control& c, bool b) { As<DxUi::ThroughputGraph>(c).SetTransitionDuration(b ? 300u : 200u); }},
    Row{"Splitter::SetSubtle", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::Splitter>(s); }, [](Control& c, bool b) { As<DxUi::Splitter>(c).SetSubtle(b); }},
    Row{"Splitter::SetOrientation", P | H | V, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::Splitter>(s); }, [](Control& c, bool b) { As<DxUi::Splitter>(c).SetOrientation(b ? DxUi::SplitterOrientation::Horizontal : DxUi::SplitterOrientation::Vertical); }},
    Row{"Splitter::SetThickness", P | H | V, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::Splitter>(s); }, [](Control& c, bool b) { As<DxUi::Splitter>(c).SetThickness(b ? 6.0f : 5.0f); }},
    Row{"Splitter::SetMinimumFirstPane", P | H | V, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::Splitter>(s); }, [](Control& c, bool b) { As<DxUi::Splitter>(c).SetMinimumFirstPane(b ? 20.0f : 10.0f); }},
    Row{"Splitter::SetMinimumSecondPane", P | H | V, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::Splitter>(s); }, [](Control& c, bool b) { As<DxUi::Splitter>(c).SetMinimumSecondPane(b ? 20.0f : 10.0f); }},
    Row{"Splitter::SetPosition", P | H | V, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::Splitter>(s); }, [](Control& c, bool b) { As<DxUi::Splitter>(c).SetPosition(b ? 150.0f : 100.0f); }},
    Row{"NumericStepper::SetLabel", L | P, true, [](ComplexUiScene& s) -> Control* { return Add<DxUi::NumericStepper>(s); }, [](Control& c, bool b) { As<DxUi::NumericStepper>(c).SetLabel(b ? L"Width" : L"Height", 40.0f); }},
    Row{"NumericStepper::SetUnit", L | P, true, [](ComplexUiScene& s) -> Control* { return Add<DxUi::NumericStepper>(s); }, [](Control& c, bool b) { As<DxUi::NumericStepper>(c).SetUnit(b ? L"px" : L"pt", 20.0f); }},
    Row{"NumericStepper::SetMinimum", P, true, [](ComplexUiScene& s) -> Control* { return Add<DxUi::NumericStepper>(s); }, [](Control& c, bool b) { As<DxUi::NumericStepper>(c).SetMinimum(b ? -2.0 : -1.0); }},
    Row{"NumericStepper::SetMaximum", P, true, [](ComplexUiScene& s) -> Control* { return Add<DxUi::NumericStepper>(s); }, [](Control& c, bool b) { As<DxUi::NumericStepper>(c).SetMaximum(b ? 200.0 : 150.0); }},
    Row{"NumericStepper::SetStep", Non, true, [](ComplexUiScene& s) -> Control* { return Add<DxUi::NumericStepper>(s); }, [](Control& c, bool b) { As<DxUi::NumericStepper>(c).SetStep(b ? 2.0 : 0.5); }},
    Row{"NumericStepper::SetLargeStep", Non, true, [](ComplexUiScene& s) -> Control* { return Add<DxUi::NumericStepper>(s); }, [](Control& c, bool b) { As<DxUi::NumericStepper>(c).SetLargeStep(b ? 20.0 : 15.0); }},
    Row{"NumericStepper::SetDecimals", P, true, [](ComplexUiScene& s) -> Control* { return Add<DxUi::NumericStepper>(s); }, [](Control& c, bool b) { As<DxUi::NumericStepper>(c).SetDecimals(static_cast<uint8_t>(b ? 3u : 2u)); }},
    Row{"ColorPicker::SetColor", P, true, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ColorPicker>(s, D2D1::RectF(16, 16, 416, 416)); }, [](Control& c, bool b) { As<DxUi::ColorPicker>(c).SetColor(b ? 0xFF336699u : 0xFF112233u); }},
    Row{"ColorPicker::SetCurrentColor", P, true, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ColorPicker>(s, D2D1::RectF(16, 16, 416, 416)); }, [](Control& c, bool b) { As<DxUi::ColorPicker>(c).SetCurrentColor(b ? 0xFF336699u : 0xFF112233u); }},
    Row{"ColorSwatch::SetSwatchValue", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::ColorSwatch>(s); }, [](Control& c, bool b) { As<DxUi::ColorSwatch>(c).SetSwatchValue(b ? 0xFF336699u : 0xFF112233u); }},
    // Text input.
    Row{"TextField::SetFontRole", L | P | V, false, NewTextField, [](Control& c, bool b) { As<DxUi::TextField>(c).SetFontRole(b ? DxUi::FontRole::Title : DxUi::FontRole::Small); }},
    Row{"TextField::SetMasked", L | P | S, false, NewTextField, [](Control& c, bool b) { As<DxUi::TextField>(c).SetMasked(b); }},
    Row{"TextField::SetInputScope", Non, false, NewTextField, [](Control& c, bool b) { As<DxUi::TextField>(c).SetInputScope(b ? DxUi::TextInputScope::Url : DxUi::TextInputScope::Number); }},
    Row{"TextField::SetPasswordRevealMode", L | P | S, false, NewPasswordField, [](Control& c, bool b) { As<DxUi::TextField>(c).SetPasswordRevealMode(b ? DxUi::PasswordRevealMode::Hidden : DxUi::PasswordRevealMode::Peek); }},
    Row{"TextField::SetPasswordRevealState", P | S, false, NewPasswordField, [](Control& c, bool b) { As<DxUi::TextField>(c).SetPasswordRevealState(b ? DxUi::PasswordRevealState::Visible : DxUi::PasswordRevealState::Hidden); }},
    Row{"TextField::SetPasswordMaskLengthPolicy", P, false, NewPasswordField, [](Control& c, bool b) { As<DxUi::TextField>(c).SetPasswordMaskLengthPolicy(b ? DxUi::PasswordMaskLengthPolicy::Concealed : DxUi::PasswordMaskLengthPolicy::Exact); }},
    Row{"TextField::SetPasswordRevealAccessibleName", N, false, NewPasswordField, [](Control& c, bool b) { As<DxUi::TextField>(c).SetPasswordRevealAccessibleName(b ? L"b" : L"a"); }},
    Row{"TextField::SetPlaceholder", P, false, NewTextField, [](Control& c, bool b) { As<DxUi::TextField>(c).SetPlaceholder(b ? L"b" : L"a"); }},
    Row{"TextField::SetMultiline", L | P | S, false, NewTextField, [](Control& c, bool b) { As<DxUi::TextField>(c).SetMultiline(b); }},
    Row{"TextField::SetClearButtonEnabled", L | P, false, NewTextField, [](Control& c, bool b) { As<DxUi::TextField>(c).SetClearButtonEnabled(b); }},
    Row{"TextField::SetCaretColor", P, false, NewTextField, [](Control& c, bool b) { As<DxUi::TextField>(c).SetCaretColor(D2D1::ColorF(b ? 0.5f : 0.25f, 0.0f, 0.0f, 1.0f)); }},
    Row{"TextField::SetHorizontalTextPadding", L | P, false, NewTextField, [](Control& c, bool b) { As<DxUi::TextField>(c).SetHorizontalTextPadding(b ? 6.0f : 5.0f, 4.0f); }},
    Row{"TextField::SetVerticalTextPadding", L | P, false, NewTextField, [](Control& c, bool b) { As<DxUi::TextField>(c).SetVerticalTextPadding(b ? 6.0f : 5.0f, 4.0f); }},
    Row{"TextField::SetReadOnly", P | S, false, NewTextField, [](Control& c, bool b) { As<DxUi::TextField>(c).SetReadOnly(b); }},
    Row{"ComboBox::SetVariant", P | H | S, false, NewComboBox, [](Control& c, bool b) { As<DxUi::ComboBox>(c).SetVariant(b ? DxUi::ComboBoxVariant::Edit : DxUi::ComboBoxVariant::Window); }},
    Row{"ComboBox::SetChromeVisible", P, false, NewComboBox, [](Control& c, bool b) { As<DxUi::ComboBox>(c).SetChromeVisible(! b); }},
    Row{"ComboBox::SetMaxVisibleItems", P | H, false, NewComboBox, [](Control& c, bool b) { As<DxUi::ComboBox>(c).SetMaxVisibleItems(b ? 6u : 5u); }},
    Row{"ComboBox::SetNoMatchesText", P | H, false, NewComboBox, [](Control& c, bool b) { As<DxUi::ComboBox>(c).SetNoMatchesText(b ? L"b" : L"a"); }},
    Row{"ComboBox::SetMinimumPopupItemHeight", P | H, false, NewComboBox, [](Control& c, bool b) { As<DxUi::ComboBox>(c).SetMinimumPopupItemHeight(b ? 24.0f : 20.0f); }},
    Row{"ComboBox::SetAutoOpenOnTextInput", Non, false, NewComboBox, [](Control& c, bool b) { As<DxUi::ComboBox>(c).SetAutoOpenOnTextInput(b); }},
    Row{"ComboBox::SetItems", P | H | V, false, NewComboBox, [](Control& c, bool b) { As<DxUi::ComboBox>(c).SetItems(b ? std::vector<DxUi::ComboBox::Item>{{L"x", L"X"}, {L"y", L"Y"}} : std::vector<DxUi::ComboBox::Item>{{L"x", L"X"}}); }},
    Row{"ComboBox::SetSelectedIndex", P | V, false, NewComboBox, [](Control& c, bool b) { As<DxUi::ComboBox>(c).SetSelectedIndex(b ? 2u : 1u); }},
    Row{"ComboBox::SetPlaceholder", P, false, NewComboBox, [](Control& c, bool b) { As<DxUi::ComboBox>(c).SetPlaceholder(b ? L"b" : L"a"); }},
    Row{"TagPicker::SetOptions", P, true, [](ComplexUiScene& s) -> Control* { return Add<DxUi::TagPicker>(s); }, [](Control& c, bool b) { As<DxUi::TagPicker>(c).SetOptions(L"All", b ? std::vector<std::wstring>{L"a", L"b"} : std::vector<std::wstring>{L"a"}); }},
    Row{"TagPicker::SetSelectedValues", P, true, [](ComplexUiScene& s) -> Control* { auto* picker = Add<DxUi::TagPicker>(s); picker->SetOptions(L"All", {L"a", L"b", L"c"}); return picker; }, [](Control& c, bool b) { As<DxUi::TagPicker>(c).SetSelectedValues({b ? L"b" : L"a"}); }},
    // Rich text: the editor's own flags.
    Row{"RichTextEditor::SetFocusModeEnabled", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::RichTextEditor>(s); }, [](Control& c, bool b) { As<DxUi::RichTextEditor>(c).SetFocusModeEnabled(b); }},
    Row{"RichTextEditor::SetSearchShortcutsEnabled", Non, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::RichTextEditor>(s); }, [](Control& c, bool b) { As<DxUi::RichTextEditor>(c).SetSearchShortcutsEnabled(! b); }},
    Row{"RichTextEditor::SetMarkdownShortcutsEnabled", Non, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::RichTextEditor>(s); }, [](Control& c, bool b) { As<DxUi::RichTextEditor>(c).SetMarkdownShortcutsEnabled(! b); }},
    Row{"RichTextEditor::SetAutoPairEnabled", Non, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::RichTextEditor>(s); }, [](Control& c, bool b) { As<DxUi::RichTextEditor>(c).SetAutoPairEnabled(! b); }},
    Row{"RichTextEditor::SetSmartTypographyEnabled", Non, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::RichTextEditor>(s); }, [](Control& c, bool b) { As<DxUi::RichTextEditor>(c).SetSmartTypographyEnabled(! b); }},
    Row{"RichTextEditor::SetMarkdownAutoFormatEnabled", Non, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::RichTextEditor>(s); }, [](Control& c, bool b) { As<DxUi::RichTextEditor>(c).SetMarkdownAutoFormatEnabled(! b); }},
    // Menus, tabs and status.
    Row{"MenuBar::SetItems", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::MenuBar>(s); }, [](Control& c, bool b) { As<DxUi::MenuBar>(c).SetItems(b ? std::vector<DxUi::MenuBarItem>{{.text = L"File"}, {.text = L"Edit"}} : std::vector<DxUi::MenuBarItem>{{.text = L"File"}}); }},
    Row{"MenuBar::SetSelectedIndex", P, false, [](ComplexUiScene& s) -> Control* { auto* bar = Add<DxUi::MenuBar>(s); bar->SetItems({{.text = L"File"}, {.text = L"Edit"}, {.text = L"View"}}); return bar; }, [](Control& c, bool b) { As<DxUi::MenuBar>(c).SetSelectedIndex(b ? 2u : 1u); }},
    Row{"MenuBar::SetRetainSelectedIndexOnFocusLost", Non, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::MenuBar>(s); }, [](Control& c, bool b) { As<DxUi::MenuBar>(c).SetRetainSelectedIndexOnFocusLost(b); }},
    Row{"TabControl::SetTabTitle", P | N, true, NewTabs, [](Control& c, bool b) { As<DxUi::TabControl>(c).SetTabTitle(0u, b ? L"First2" : L"First"); }},
    Row{"TabControl::SetTabTooltip", Non, false, NewTabs, [](Control& c, bool b) { As<DxUi::TabControl>(c).SetTabTooltip(0u, b ? L"b" : L"a"); }},
    Row{"TabControl::SetTabClosable", P, true, NewTabs, [](Control& c, bool b) { As<DxUi::TabControl>(c).SetTabClosable(0u, b); }},
    Row{"TabControl::SetTabVisible", P | S, true, NewTabs, [](Control& c, bool b) { As<DxUi::TabControl>(c).SetTabVisible(1u, b); }},
    Row{"TabControl::SetTabReorderingEnabled", Non, false, NewTabs, [](Control& c, bool b) { As<DxUi::TabControl>(c).SetTabReorderingEnabled(b); }},
    Row{"TabControl::SetSelectedIndex", P | S, true, NewTabs, [](Control& c, bool b) { As<DxUi::TabControl>(c).SetSelectedIndex(b ? 1u : 0u); }},
    Row{"StatusStrip::SetText", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::StatusStrip>(s); }, [](Control& c, bool b) { As<DxUi::StatusStrip>(c).SetText(b ? L"b" : L"a"); }},
    Row{"StatusStrip::SetFontRole", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::StatusStrip>(s); }, [](Control& c, bool b) { As<DxUi::StatusStrip>(c).SetFontRole(b ? DxUi::FontRole::Body : DxUi::FontRole::Title); }},
    Row{"StatusStrip::SetBlendWithWindowBackground", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::StatusStrip>(s); }, [](Control& c, bool b) { As<DxUi::StatusStrip>(c).SetBlendWithWindowBackground(b); }},
    Row{"StatusStrip::SetSections", P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::StatusStrip>(s); }, [](Control& c, bool b) { As<DxUi::StatusStrip>(c).SetSections(b ? std::vector<DxUi::StatusStrip::Section>{{.text = L"a"}, {.text = L"b"}} : std::vector<DxUi::StatusStrip::Section>{{.text = L"a"}}); }},
    Row{"StatusStrip::SetSectionText", P, false, [](ComplexUiScene& s) -> Control* { auto* strip = Add<DxUi::StatusStrip>(s); strip->SetSections({{.text = L"a"}}); return strip; }, [](Control& c, bool b) { As<DxUi::StatusStrip>(c).SetSectionText(0u, b ? L"c" : L"b"); }},
    // Layout panels.
    Row{"StackPanel::SetOrientation", L | P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::StackPanel>(s); }, [](Control& c, bool b) { As<DxUi::StackPanel>(c).SetOrientation(b ? DxUi::StackOrientation::Horizontal : DxUi::StackOrientation::Vertical); }},
    Row{"StackPanel::SetGap", L | P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::StackPanel>(s); }, [](Control& c, bool b) { As<DxUi::StackPanel>(c).SetGap(b ? 6.0f : 5.0f); }},
    Row{"StackPanel::SetPadding", L | P, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::StackPanel>(s); }, [](Control& c, bool b) { As<DxUi::StackPanel>(c).SetPadding(b ? 6.0f : 5.0f, 1.0f, 2.0f, 3.0f); }},
    Row{"StackPanel::SetChildExtent", L | P, false, [](ComplexUiScene& s) -> Control* { auto* stack = Add<DxUi::StackPanel>(s); static_cast<void>(stack->AddChild<DxUi::Label>(L"Child")); return stack; }, [](Control& c, bool b) { auto& stack = As<DxUi::StackPanel>(c); stack.SetChildExtent(stack.GetChildren()[0].get(), b ? 30.0f : 20.0f); }},
    Row{"FlexPanel::SetOrientation", L, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::FlexPanel>(s); }, [](Control& c, bool b) { static_cast<void>(As<DxUi::FlexPanel>(c).SetOrientation(b ? DxUi::StackOrientation::Horizontal : DxUi::StackOrientation::Vertical)); }},
    Row{"FlexPanel::SetGap", L, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::FlexPanel>(s); }, [](Control& c, bool b) { static_cast<void>(As<DxUi::FlexPanel>(c).SetGap(b ? 6.0f : 5.0f)); }},
    Row{"FlexPanel::SetPadding", L, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::FlexPanel>(s); }, [](Control& c, bool b) { static_cast<void>(As<DxUi::FlexPanel>(c).SetPadding(b ? 6.0f : 5.0f, 1.0f, 2.0f, 3.0f)); }},
    Row{"FlexPanel::SetWrap", L, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::FlexPanel>(s); }, [](Control& c, bool b) { static_cast<void>(As<DxUi::FlexPanel>(c).SetWrap(b)); }},
    Row{"FlexPanel::SetChildOptions", L, false, [](ComplexUiScene& s) -> Control* { auto* flex = Add<DxUi::FlexPanel>(s); static_cast<void>(flex->AddChild<DxUi::Label>(L"Child")); return flex; }, [](Control& c, bool b) { auto& flex = As<DxUi::FlexPanel>(c); static_cast<void>(flex.SetChildOptions(flex.GetChildren()[0].get(), DxUi::FlexChildOptions{.grow = b ? 2.0f : 1.0f})); }},
    Row{"FormField::SetRequired", Non, true, [](ComplexUiScene& s) -> Control* { return Root(s).AddChild<DxUi::FormField>(L"id", L"Label"); }, [](Control& c, bool b) { static_cast<void>(As<DxUi::FormField>(c).SetRequired(b)); }},
    Row{"FormField::SetEditorPreferredSize", L, true, [](ComplexUiScene& s) -> Control* { return Root(s).AddChild<DxUi::FormField>(L"id", L"Label"); }, [](Control& c, bool b) { static_cast<void>(As<DxUi::FormField>(c).SetEditorPreferredSize(D2D1::SizeF(b ? 240.0f : 200.0f, 32.0f))); }},
    Row{"FormPanel::SetColumns", L, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::FormPanel>(s); }, [](Control& c, bool b) { static_cast<void>(As<DxUi::FormPanel>(c).SetColumns(b ? 2u : 1u)); }},
    Row{"FormPanel::SetLabelPlacement", L, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::FormPanel>(s); }, [](Control& c, bool b) { static_cast<void>(As<DxUi::FormPanel>(c).SetLabelPlacement(b ? DxUi::LabelPlacement::Beside : DxUi::LabelPlacement::Above)); }},
    Row{"FormPanel::SetGap", L, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::FormPanel>(s); }, [](Control& c, bool b) { static_cast<void>(As<DxUi::FormPanel>(c).SetGap(b ? 6.0f : 5.0f, 4.0f)); }},
    Row{"FormPanel::SetPadding", L, false, [](ComplexUiScene& s) -> Control* { return Add<DxUi::FormPanel>(s); }, [](Control& c, bool b) { static_cast<void>(As<DxUi::FormPanel>(c).SetPadding(b ? 6.0f : 5.0f)); }},
    Row{"ScrollPanel::SetContentHeight", P, false, NewScrollPanel, [](Control& c, bool b) { As<DxUi::ScrollPanel>(c).SetContentHeight(b ? 2000.0f : 1500.0f); }},
    Row{"ScrollPanel::SetScrollOffset", P | H, false, NewScrollPanel, [](Control& c, bool b) { As<DxUi::ScrollPanel>(c).SetScrollOffset(b ? 20.0f : 10.0f); }},
    Row{"ScrollPanel::SetScrollStepDip", Non, false, NewScrollPanel, [](Control& c, bool b) { As<DxUi::ScrollPanel>(c).SetScrollStepDip(b ? 30.0f : 20.0f); }},
    Row{"ScrollPanel::SetInternalScrollbarEnabled", P, false, NewScrollPanel, [](Control& c, bool b) { As<DxUi::ScrollPanel>(c).SetInternalScrollbarEnabled(b); }},
    // Lists.
    Row{"Tree::SetAppearance", P, false, NewTree, [](Control& c, bool b) { As<DxUi::Tree>(c).SetAppearance(DxUi::TreeAppearance{.drawBorder = b}); }},
    Row{"Tree::SetEmptyStateText", P, false, NewTree, [](Control& c, bool b) { As<DxUi::Tree>(c).SetEmptyStateText(b ? L"b" : L"a"); }},
    Row{"Tree::SetInvokeOnSingleClick", Non, false, NewTree, [](Control& c, bool b) { As<DxUi::Tree>(c).SetInvokeOnSingleClick(b); }},
    Row{"Tree::SetTypographyOverride", P, false, NewTree, [](Control& c, bool b) { static_cast<void>(As<DxUi::Tree>(c).SetTypographyOverride(DxUi::Tree::TypographyOverride{.fontSizeDip = b ? 20.0f : 18.0f})); }},
    Row{"Tree::SetReorderEnabled", Non, false, NewTree, [](Control& c, bool b) { As<DxUi::Tree>(c).SetReorderEnabled(b); }},
    Row{"Tree::SetMultiSelectEnabled", P | S, false, NewTree, [](Control& c, bool b) { As<DxUi::Tree>(c).SetMultiSelectEnabled(b); }},
    Row{"Grid::SetVisualMode", P, false, NewGrid, [](Control& c, bool b) { As<DxUi::Grid>(c).SetVisualMode(b ? DxUi::GridVisualMode::FolderView : DxUi::GridVisualMode::Standard); }},
    Row{"Grid::SetCellTextFontRole", P, false, NewGrid, [](Control& c, bool b) { As<DxUi::Grid>(c).SetCellTextFontRole(b ? DxUi::FontRole::Title : DxUi::FontRole::Small); }},
    Row{"Grid::SetIconSizeDip", P, false, NewGrid, [](Control& c, bool b) { As<DxUi::Grid>(c).SetIconSizeDip(b ? 24.0f : 20.0f); }},
    Row{"Grid::SetLineClamp", P, false, NewGrid, [](Control& c, bool b) { As<DxUi::Grid>(c).SetLineClamp(b ? 3u : 2u); }},
    Row{"Grid::SetEmptyStateText", P, false, NewGrid, [](Control& c, bool b) { As<DxUi::Grid>(c).SetEmptyStateText(b ? L"b" : L"a"); }},
    Row{"Grid::SetHeaderBusy", P, false, NewGrid, [](Control& c, bool b) { As<DxUi::Grid>(c).SetHeaderBusy(b); }},
    Row{"Grid::SetHeaderBusyColumn", P, false, NewGrid, [](Control& c, bool b) { As<DxUi::Grid>(c).SetHeaderBusyColumn(b ? 2u : 1u); }},
    Row{"Grid::SetSortSpec", P, false, NewGrid, [](Control& c, bool b) { As<DxUi::Grid>(c).SetSortSpec(DxUi::GridSortSpec{.columnIndex = b ? 2u : 1u, .direction = DxUi::SortDirection::Ascending}); }},
};
// clang-format on

struct Delta final
{
    uint64_t invalidations = 0u, measures = 0u, hits = 0u, mutations = 0u, builds = 0u;
};

[[nodiscard]] inline Delta Read(const DxUi::Detail::WorkCounters& work) noexcept
{
    return Delta{work.frame.invalidations,
                 work.layout.measureInvalidations,
                 work.frame.hitRevisionMoves,
                 work.accessibility.mutations,
                 work.accessibility.snapshotBuilds};
}

[[nodiscard]] inline std::string Describe(std::string_view setter, std::string_view what, const Delta& delta)
{
    return std::string(setter) + ": " + std::string(what) + " (invalidations " + std::to_string(delta.invalidations) + ", measure invalidations " +
           std::to_string(delta.measures) + ", hit-revision moves " + std::to_string(delta.hits) + ", accessibility mutations " +
           std::to_string(delta.mutations) + ")";
}

// The gate: every row, twice to the same value, on FX5's scene and host.
__declspec(noinline) inline void TestFx5EverySetterAssignsOnce(GraphicsFixture& gpu)
{
    const std::optional<bool> outerListeners = DxUi::DebugGetAccessibilitySystemListenersForTest();
    DxUi::DebugSetAccessibilitySystemListenersForTest(true);
    const auto restoreListeners = wil::scope_exit([&] { DxUi::DebugSetAccessibilitySystemListenersForTest(outerListeners); });

    ComplexUiScene scene;
    Hr(scene.Initialize(gpu.device.get()), "FX5 scene for the assignment gate");
    Hr(scene.view.Prepare(1280u, 720u, 96.0f), "FX5 first frame");
    g_shared.group = Root(scene).AddChild<DxUi::RadioButtons>();

    std::set<std::string_view> names;
    size_t exercised = 0u;
    for (const Row& row : kRows)
    {
        Check(names.insert(row.setter).second, (std::string(row.setter) + ": one row per setter").c_str());
        Control* const control = row.create(scene);
        Check(control != nullptr && DxUi::IsControlInTree(scene.view.Controls().GetRoot(), control),
              (std::string(row.setter) + ": control attached to the FX5 host").c_str());
        row.set(*control, false);

        DxUi::Detail::WorkCounterScope scope;
        row.set(*control, true);
        const Delta changed = Read(scope.Delta());
        scope.Restart();
        row.set(*control, true);
        const Delta same = Read(scope.Delta());

        Check(same.invalidations == 0u && same.measures == 0u && same.hits == 0u && same.mutations == 0u && same.builds == 0u,
              Describe(row.setter, "an unchanged value routes nothing", same).c_str());

        const Affects affects = row.affects;
        const bool paint      = HasAffects(affects, Affects::Paint);
        const bool layout     = HasAffects(affects, Affects::Layout);
        const bool hit        = HasAffects(affects, Affects::Hit);
        const bool semantic   = HasAffects(affects, Affects::SemanticName | Affects::SemanticValue | Affects::SemanticState);
        Check(! paint || changed.invalidations > 0u, Describe(row.setter, "a changed value invalidates (Paint)", changed).c_str());
        Check(! layout || changed.measures > 0u, Describe(row.setter, "a changed value invalidates the measure (Layout)", changed).c_str());
        Check(! hit || changed.hits > 0u, Describe(row.setter, "a changed value moves the hit revision (Hit)", changed).c_str());
        Check(! semantic || (changed.mutations > 0u && changed.invalidations > 0u),
              Describe(row.setter, "a changed value raises an accessibility mutation (Semantic)", changed).c_str());
        if (! row.parts)
        {
            // An embedded view publishes a semantic change at its next preparation, so the change also invalidates it.
            Check(paint || layout || semantic || changed.invalidations == 0u, Describe(row.setter, "no invalidation beyond Affects", changed).c_str());
            Check(layout || changed.measures == 0u, Describe(row.setter, "no measure invalidation beyond Affects", changed).c_str());
            Check(hit || changed.hits == 0u, Describe(row.setter, "no hit-revision move beyond Affects", changed).c_str());
            Check(semantic || changed.mutations == 0u, Describe(row.setter, "no accessibility mutation beyond Affects", changed).c_str());
        }
        ++exercised;
    }
    Check(exercised == kRows.size(), "every row of the assignment gate ran");
    // The scene still renders after every row: its next frame prepares without failure.
    Hr(scene.view.Prepare(1280u, 720u, 96.0f), "FX5 frame after the assignment gate");
    std::cout << "assignment gate: " << exercised << " setters, each unchanged value routed nothing\n";
}

// Assign itself: equality per type, what it stores, and the one-step routing order.
__declspec(noinline) inline void TestAssignEquality()
{
    using DxUi::AssignEquality::Same;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    Check(Same(nan, nan), "a NaN assigned again is no change");
    Check(Same(0.0, -0.0) && Same(-0.0f, 0.0f), "negative and positive zero are the same value");
    Check(! Same(1.0, 1.0 + 1e-15), "floating point compares exactly");
    Check(Same(std::wstring(L"text"), std::wstring_view(L"text")) && ! Same(std::wstring(L"text"), std::wstring_view(L"Text")), "strings by content");
    Check(Same(std::optional<float>{}, std::optional<float>{}) && ! Same(std::optional<float>{}, std::optional<float>{0.0f}),
          "optionals by presence then value");
    Check(Same(D2D1::ColorF(0.1f, 0.2f, 0.3f, 0.4f), D2D1::ColorF(0.1f, 0.2f, 0.3f, 0.4f)) &&
              ! Same(D2D1::ColorF(0.1f, 0.2f, 0.3f, 0.4f), D2D1::ColorF(0.1f, 0.2f, 0.3f, 0.5f)),
          "colours member by member");
    const std::array<double, 2> span{1.0, 2.0};
    Check(Same(std::vector<double>{1.0, 2.0}, std::span<const double>(span)) && ! Same(std::vector<double>{1.0}, std::span<const double>(span)),
          "a container and a range element by element");
    float a = 1.0f, b = 2.0f;
    Check(Same(std::tie(a, b), std::tuple<float, float>(1.0f, 2.0f)) && ! Same(std::tie(a, b), std::tuple<float, float>(1.0f, 3.0f)), "tied fields as a tuple");
    int x = 0;
    Check(! Same(&x, static_cast<int*>(nullptr)), "pointers by identity");
}
} // namespace AssignSetterGate
