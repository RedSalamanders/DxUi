// This translation unit consumes only supported headers, never src/Controls internals.
#include <DxUi/AccessibilityTextUnits.h>
#include <DxUi/NativeMenuInterop.h>
#include <DxUi/PointerInput.h>
#include <DxUi/Typography.h>
#include <type_traits>

static_assert(! std::is_copy_constructible_v<DxUi::NativeMenuBarHost>);
static_assert(DxUi::MenuBar::kDefaultHeightDip == 30.0f);

bool TestPublicHelpers()
{
    DxUi::NativeMenuBarHost menu;
    menu.SetHeightDip(32);
    HWND focus = nullptr;
    DxUi::CaptureFocusRestoreTarget(nullptr, nullptr, focus);
    return ! menu.GetHwnd() && ! DxUi::RestoreCapturedFocus(focus) && DxUi::PointerInputKindFromMessage(WM_MOUSEMOVE) == DxUi::PointerInputKind::Move &&
           DxUi::NormalizeAccessibilityTextUnit(TextUnit_Format) == TextUnit_Word &&
           DxUi::Typography::GetUiTextFamilyForSizeDip(13) == DxUi::Typography::kSegoeUiVariableTextFamily;
}
