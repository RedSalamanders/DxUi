#include <DxUi/ControlCatalog.h>
#include <array>
#include <new>
namespace DxUi
{
std::span<const ControlDescriptor> GetControlCatalog() noexcept
{
    static constexpr std::array entries{ControlDescriptor{ControlKind::Panel, L"Panel"},
                                        ControlDescriptor{ControlKind::PageHost, L"PageHost"},
                                        ControlDescriptor{ControlKind::CardPanel, L"CardPanel"},
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
                                        ControlDescriptor{ControlKind::PageIndicator, L"PageIndicator"}};
    return entries;
}
HRESULT CreateControl(ControlKind kind, std::unique_ptr<Control>& result) noexcept
{
    try
    {
        switch (kind)
        {
            case ControlKind::Panel: result = std::make_unique<Panel>(); return S_OK;
            case ControlKind::PageHost: result = std::make_unique<PageHost>(); return S_OK;
            case ControlKind::CardPanel: result = std::make_unique<CardPanel>(); return S_OK;
            case ControlKind::Label: result = std::make_unique<Label>(); return S_OK;
            case ControlKind::Button: result = std::make_unique<Button>(); return S_OK;
            case ControlKind::Toggle: result = std::make_unique<Toggle>(); return S_OK;
            case ControlKind::Checkbox: result = std::make_unique<Checkbox>(); return S_OK;
            case ControlKind::RadioButton: result = std::make_unique<RadioButton>(); return S_OK;
            case ControlKind::RadioButtons: result = std::make_unique<RadioButtons>(); return S_OK;
            case ControlKind::ProgressBar: result = std::make_unique<ProgressBar>(); return S_OK;
            case ControlKind::ThroughputGraph: result = std::make_unique<ThroughputGraph>(); return S_OK;
            case ControlKind::Slider: result = std::make_unique<Slider>(); return S_OK;
            case ControlKind::Toolbar: result = std::make_unique<Toolbar>(); return S_OK;
            case ControlKind::MenuBar: result = std::make_unique<MenuBar>(); return S_OK;
            case ControlKind::TabControl: result = std::make_unique<TabControl>(); return S_OK;
            case ControlKind::ColorSwatch: result = std::make_unique<ColorSwatch>(); return S_OK;
            case ControlKind::TextField: result = std::make_unique<TextField>(); return S_OK;
            case ControlKind::ComboBox: result = std::make_unique<ComboBox>(); return S_OK;
            case ControlKind::TagPicker: result = std::make_unique<TagPicker>(); return S_OK;
            case ControlKind::StatusStrip: result = std::make_unique<StatusStrip>(); return S_OK;
            case ControlKind::PopupLayer: result = std::make_unique<PopupLayer>(); return S_OK;
            case ControlKind::StackPanel: result = std::make_unique<StackPanel>(); return S_OK;
            case ControlKind::ScrollPanel: result = std::make_unique<ScrollPanel>(); return S_OK;
            case ControlKind::TooltipLayer: result = std::make_unique<TooltipLayer>(); return S_OK;
            case ControlKind::Tree: result = std::make_unique<Tree>(); return S_OK;
            case ControlKind::Grid: result = std::make_unique<Grid>(); return S_OK;
            case ControlKind::PageIndicator: result = std::make_unique<PageIndicator>(); return S_OK;
            default: return E_INVALIDARG;
        }
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
}
} // namespace DxUi
