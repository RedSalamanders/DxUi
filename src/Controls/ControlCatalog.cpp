#include <DxUi/ControlCatalog.h>
#include <DxUi/Failure.h>
#include <array>
#include <new>
namespace DxUi
{
std::span<const ControlDescriptor> GetControlCatalog() noexcept
{
    static constexpr std::array entries{ControlDescriptor{ControlKind::Panel, L"Panel"},
                                        ControlDescriptor{ControlKind::PageHost, L"PageHost"},
                                        ControlDescriptor{ControlKind::CardPanel, L"CardPanel"},
                                        ControlDescriptor{ControlKind::Separator, L"Separator"},
                                        ControlDescriptor{ControlKind::Label, L"Label"},
                                        ControlDescriptor{ControlKind::Button, L"Button"},
                                        ControlDescriptor{ControlKind::Toggle, L"Toggle"},
                                        ControlDescriptor{ControlKind::Checkbox, L"Checkbox"},
                                        ControlDescriptor{ControlKind::RadioButton, L"RadioButton"},
                                        ControlDescriptor{ControlKind::RadioButtons, L"RadioButtons"},
                                        ControlDescriptor{ControlKind::ProgressBar, L"ProgressBar"},
                                        ControlDescriptor{ControlKind::ThroughputGraph, L"ThroughputGraph"},
                                        ControlDescriptor{ControlKind::Slider, L"Slider"},
                                        ControlDescriptor{ControlKind::Toolbar, L"Toolbar"},
                                        ControlDescriptor{ControlKind::MenuBar, L"MenuBar"},
                                        ControlDescriptor{ControlKind::TabControl, L"TabControl"},
                                        ControlDescriptor{ControlKind::ColorSwatch, L"ColorSwatch"},
                                        ControlDescriptor{ControlKind::TextField, L"TextField"},
                                        ControlDescriptor{ControlKind::ComboBox, L"ComboBox"},
                                        ControlDescriptor{ControlKind::TagPicker, L"TagPicker"},
                                        ControlDescriptor{ControlKind::StatusStrip, L"StatusStrip"},
                                        ControlDescriptor{ControlKind::PopupLayer, L"PopupLayer"},
                                        ControlDescriptor{ControlKind::StackPanel, L"StackPanel"},
                                        ControlDescriptor{ControlKind::ScrollPanel, L"ScrollPanel"},
                                        ControlDescriptor{ControlKind::TooltipLayer, L"TooltipLayer"},
                                        ControlDescriptor{ControlKind::Tree, L"Tree"},
                                        ControlDescriptor{ControlKind::Grid, L"Grid"},
                                        ControlDescriptor{ControlKind::PageIndicator, L"PageIndicator"},
                                        ControlDescriptor{ControlKind::Splitter, L"Splitter"},
                                        ControlDescriptor{ControlKind::NumericStepper, L"NumericStepper"},
                                        ControlDescriptor{ControlKind::ColorPicker, L"ColorPicker"},
                                        ControlDescriptor{ControlKind::FlexPanel, L"FlexPanel"},
                                        ControlDescriptor{ControlKind::FormField, L"FormField"},
                                        ControlDescriptor{ControlKind::FormPanel, L"FormPanel"},
                                        ControlDescriptor{ControlKind::RichTextEditor, L"RichTextEditor"}};
    return entries;
}
namespace
{
template <class T> HRESULT ConstructControl(std::unique_ptr<Control>& result)
{
    std::unique_ptr<T> staged(new (std::nothrow) T);
    if (! staged)
        return E_OUTOFMEMORY;
    result = std::move(staged);
    return S_OK;
}
} // namespace
HRESULT CreateControl(ControlKind kind, std::unique_ptr<Control>& result) noexcept
{
    if (static_cast<unsigned>(kind) > static_cast<unsigned>(ControlKind::RichTextEditor))
        return E_INVALIDARG;
    if (! Failure::IsConfigured())
        return CO_E_NOTINITIALIZED;
    const Failure::Scope failureScope;
    {
        switch (kind)
        {
            case ControlKind::Panel: return ConstructControl<Panel>(result);
            case ControlKind::PageHost: return ConstructControl<PageHost>(result);
            case ControlKind::CardPanel: return ConstructControl<CardPanel>(result);
            case ControlKind::Separator: return ConstructControl<Separator>(result);
            case ControlKind::Label: return ConstructControl<Label>(result);
            case ControlKind::Button: return ConstructControl<Button>(result);
            case ControlKind::Toggle: return ConstructControl<Toggle>(result);
            case ControlKind::Checkbox: return ConstructControl<Checkbox>(result);
            case ControlKind::RadioButton: return ConstructControl<RadioButton>(result);
            case ControlKind::RadioButtons: return ConstructControl<RadioButtons>(result);
            case ControlKind::ProgressBar: return ConstructControl<ProgressBar>(result);
            case ControlKind::ThroughputGraph: return ConstructControl<ThroughputGraph>(result);
            case ControlKind::Slider: return ConstructControl<Slider>(result);
            case ControlKind::Toolbar: return ConstructControl<Toolbar>(result);
            case ControlKind::MenuBar: return ConstructControl<MenuBar>(result);
            case ControlKind::TabControl: return ConstructControl<TabControl>(result);
            case ControlKind::ColorSwatch: return ConstructControl<ColorSwatch>(result);
            case ControlKind::TextField: return ConstructControl<TextField>(result);
            case ControlKind::ComboBox: return ConstructControl<ComboBox>(result);
            case ControlKind::TagPicker: return ConstructControl<TagPicker>(result);
            case ControlKind::StatusStrip: return ConstructControl<StatusStrip>(result);
            case ControlKind::PopupLayer: return ConstructControl<PopupLayer>(result);
            case ControlKind::StackPanel: return ConstructControl<StackPanel>(result);
            case ControlKind::ScrollPanel: return ConstructControl<ScrollPanel>(result);
            case ControlKind::TooltipLayer: return ConstructControl<TooltipLayer>(result);
            case ControlKind::Tree: return ConstructControl<Tree>(result);
            case ControlKind::Grid: return ConstructControl<Grid>(result);
            case ControlKind::PageIndicator: return ConstructControl<PageIndicator>(result);
            case ControlKind::Splitter: return ConstructControl<Splitter>(result);
            case ControlKind::NumericStepper: return ConstructControl<NumericStepper>(result);
            case ControlKind::ColorPicker: return ConstructControl<ColorPicker>(result);
            case ControlKind::FlexPanel: return ConstructControl<FlexPanel>(result);
            case ControlKind::FormField: return ConstructControl<FormField>(result);
            case ControlKind::FormPanel: return ConstructControl<FormPanel>(result);
            case ControlKind::RichTextEditor: return ConstructControl<RichTextEditor>(result);
            default: return E_INVALIDARG;
        }
    }
}
} // namespace DxUi
