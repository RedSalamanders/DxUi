// This translation unit consumes only supported headers, never src/Controls internals.
#include <DxUi/AccessibilityTextUnits.h>
#include <DxUi/NativeMenuInterop.h>
#include <DxUi/PointerInput.h>
#include <DxUi/Typography.h>
#include <type_traits>

static_assert(! std::is_copy_constructible_v<DxUi::NativeMenuBarHost>);
static_assert(DxUi::MenuBar::kDefaultHeightDip == 30.0f);

bool TestFontAvailabilityInvalidation()
{
    using namespace DxUi::Typography;
    wil::com_ptr<IDWriteFactory> first;
    wil::com_ptr<IDWriteFactory> second;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory), first.put_unknown())) ||
        FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory), second.put_unknown())))
    {
        return false;
    }
    InvalidateFontFamilyAvailability(nullptr);
    const auto cleanup      = wil::scope_exit([]() noexcept { InvalidateFontFamilyAvailability(nullptr); });
    constexpr PCWSTR family = L"Segoe UI";
    if (! IsFontFamilyAvailable(first.get(), family) || ! IsFontFamilyAvailable(second.get(), family))
    {
        return false;
    }
    // Simulate two cached negative answers from before a font installation.
    // Real DirectWrite queries must replace only the invalidated factory's answer.
    {
        std::scoped_lock lock(GetTypographyMeasurementCacheMutex());
        for (auto& entry : GetTypographyFontFamilyCache())
        {
            entry.available = false;
        }
    }
    if (IsFontFamilyAvailable(first.get(), family) || IsFontFamilyAvailable(second.get(), family))
    {
        return false;
    }
    InvalidateFontFamilyAvailability(first.get());
    if (! IsFontFamilyAvailable(first.get(), family) || IsFontFamilyAvailable(second.get(), family))
    {
        return false;
    }
    InvalidateFontFamilyAvailability(nullptr);
    return IsFontFamilyAvailable(first.get(), family) && IsFontFamilyAvailable(second.get(), family) && ! IsFontFamilyAvailable(nullptr, family) &&
           ! IsFontFamilyAvailable(first.get(), L"");
}

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
