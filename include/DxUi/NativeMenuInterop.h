#pragma once

#include "DxUi.h"
#include "FocusRestore.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace DxUi
{
inline constexpr wchar_t kNativeMenuBarHostWindowClassName[] = L"DxUi.NativeMenuBar";

struct NativeMenuFlyoutOptions final
{
    bool includeAcceleratorText = true;
    bool omitEmptySubmenus      = false;
    bool trimSeparators         = false;
    std::span<const int> excludedCommandIds{};
};

void SplitNativeMenuText(std::wstring_view raw, std::wstring& outText, std::wstring& outShortcut) noexcept;

[[nodiscard]] std::wstring StripNativeMenuMnemonicMarkers(std::wstring_view text);

[[nodiscard]] wchar_t FindNativeMenuMnemonic(std::wstring_view text) noexcept;

[[nodiscard]] bool TryGetNativeMenuItemPresentationText(
    HMENU menu, UINT position, const MENUITEMINFOW& itemInfo, std::wstring& outText, std::wstring& outShortcut) noexcept;

[[nodiscard]] bool IsNativeMenuCommandExcluded(int commandId, const NativeMenuFlyoutOptions& options) noexcept;

void TrimNativeMenuFlyoutSeparators(std::vector<MenuFlyoutItem>& items) noexcept;

[[nodiscard]] std::vector<MenuFlyoutItem> ConvertNativeHMenuToFlyoutItems(HMENU menu, const NativeMenuFlyoutOptions& options = {}) noexcept;

[[nodiscard]] POINT ResolveNativeContextMenuScreenPoint(HWND hwnd, LPARAM lParam) noexcept;

[[nodiscard]] std::optional<int> ShowNativeHMenuContextMenu(
    HWND ownerHwnd, POINT screenPoint, HMENU menu, const ThemePalette& theme, const ContextMenuSessionCallbacks& sessionCallbacks = {}) noexcept;

[[nodiscard]] std::optional<int> ShowNativeHMenuContextMenu(HWND ownerHwnd,
                                                            POINT screenPoint,
                                                            HMENU menu,
                                                            const ThemePalette& theme,
                                                            const NativeMenuFlyoutOptions& options,
                                                            const ContextMenuSessionCallbacks& sessionCallbacks = {}) noexcept;

[[nodiscard]] std::vector<MenuBarItem> BuildNativeMenuBarItems(HMENU menu) noexcept;

class NativeMenuBarHost
{
public:
    using RefreshMenuStateCallback = std::function<void()>;

    NativeMenuBarHost()                                    = default;
    NativeMenuBarHost(const NativeMenuBarHost&)            = delete;
    NativeMenuBarHost& operator=(const NativeMenuBarHost&) = delete;

    ~NativeMenuBarHost();

    void SetTheme(const ThemePalette& theme) noexcept;

    void SetHeightDip(int heightDip) noexcept;

    void SetRefreshMenuStateCallback(RefreshMenuStateCallback callback);

    void SetOnTabBoundary(std::function<bool(bool reverse)> onTabBoundary);

    void SetOnEscape(std::function<bool()> onEscape);

    [[nodiscard]] bool Attach(HINSTANCE instance, HWND ownerWindow, HMENU menu, HWND commandTarget = nullptr) noexcept;

    void Detach() noexcept;

    void SyncMenuModel() noexcept;

    void UpdateLayout() noexcept;

    [[nodiscard]] int GetVisibleHeightPx() const noexcept;

    [[nodiscard]] bool FocusFirstItem() noexcept;

    [[nodiscard]] bool ActivateMnemonic(wchar_t mnemonic) noexcept;

    [[nodiscard]] HWND GetHwnd() const noexcept;

private:
    [[nodiscard]] static bool EnsureWindowClass(HINSTANCE instance) noexcept;

    void CaptureFocusRestoreTarget() noexcept;

    void RestoreCapturedFocus() noexcept;

    [[nodiscard]] bool SyncMenuModelInternal(bool invokeRefresh) noexcept;

    [[nodiscard]] std::optional<size_t> HitTestScreenPoint(POINT screenPoint) const noexcept;

    [[nodiscard]] std::optional<POINT> GetItemAnchorScreenPoint(size_t index) const noexcept;

    [[nodiscard]] std::optional<size_t> FindNextEnabledItem(size_t currentIndex, bool forward) const noexcept;

    [[nodiscard]] std::optional<ContextMenuRootSwitchRequest> BuildRootSwitchRequest(size_t index) noexcept;

    void OpenPopup(size_t index, POINT screenPoint, bool keyboardInvocation) noexcept;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept;

    ThemePalette _theme = MakeDefaultThemePalette(false);
    RefreshMenuStateCallback _refreshMenuState;
    ControlHost _host;
    MenuBar* _menuBar = nullptr;
    wil::unique_hwnd _hwnd;
    HWND _ownerWindow      = nullptr;
    HWND _commandTarget    = nullptr;
    HMENU _menu            = nullptr;
    HWND _focusRestoreHwnd = nullptr;
    int _heightDip         = static_cast<int>(MenuBar::kDefaultHeightDip);
};
} // namespace DxUi
