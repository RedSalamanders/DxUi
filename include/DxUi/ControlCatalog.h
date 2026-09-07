#pragma once
#include "DxUi.h"
namespace DxUi
{
enum class ControlKind : uint8_t
{
    Panel,
    PageHost,
    CardPanel,
    Label,
    Button,
    Toggle,
    Checkbox,
    RadioButton,
    RadioButtons,
    ProgressBar,
    ThroughputGraph,
    Slider,
    Toolbar,
    MenuBar,
    TabControl,
    ColorSwatch,
    TextField,
    ComboBox,
    TagPicker,
    StatusStrip,
    PopupLayer,
    StackPanel,
    ScrollPanel,
    TooltipLayer,
    Tree,
    Grid,
    PageIndicator
};
struct ControlDescriptor
{
    ControlKind kind;
    std::wstring_view name;
};
[[nodiscard]] std::span<const ControlDescriptor> GetControlCatalog() noexcept;
// E_INVALIDARG for an unknown kind; failure leaves the caller's existing result untouched.
HRESULT CreateControl(ControlKind kind, std::unique_ptr<Control>& result) noexcept;
} // namespace DxUi
