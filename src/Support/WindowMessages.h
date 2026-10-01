#pragma once
#include <windows.h>

// DxUi's private window messages. Each one is registered by name with RegisterWindowMessageW, so its value lies in
// 0xC000-0xFFFF and belongs to that name for the whole session: no application message (WM_USER or WM_APP based) can
// share it, and an application reserves no value for DxUi. A window procedure forwards every message, registered ones
// included, to HandleMessage. A name reads RedSalamanders.DxUi.<Component>.<Purpose>.v<N>; a message whose parameters
// change takes the next version, so modules that link different DxUi revisions never read each other's parameters.
//
// An accessor registers its message the first time it is called and returns that cached result afterwards, so no
// message path registers anything. A failed registration stays failed and is 0, which is WM_NULL: a sender never posts
// or sends it, and Matches never recognizes it, so no receiver takes WM_NULL for one of DxUi's messages.
namespace DxUi::WndMsg
{
// A registered message, or 0 when its registration failed. It does not compare with a message: a receiver asks Matches,
// and a sender posts or sends `value` only once it has checked that the message is nonzero.
struct RegisteredMessage final
{
    UINT value = 0u;

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return value != 0u;
    }

    [[nodiscard]] constexpr bool Matches(UINT message) const noexcept
    {
        return value != 0u && message == value;
    }
};

// RegisterWindowMessageW returns a value from 0xC000-0xFFFF, or 0 when it fails; nothing else counts as registered.
[[nodiscard]] inline RegisteredMessage Register(const wchar_t* name) noexcept
{
    const UINT message = RegisterWindowMessageW(name);
    return RegisteredMessage{message >= 0xC000u && message <= 0xFFFFu ? message : 0u};
}

// Messages to an application window that a ControlHost is attached to, which its procedure passes to HandleMessage.

// Posted when the window gains focus; its dispatch ends that message-loop turn. wParam names the turn.
[[nodiscard]] inline RegisteredMessage WindowHostFocusGainTurnEnd() noexcept
{
    static const RegisteredMessage message = Register(L"RedSalamanders.DxUi.WindowHost.FocusGainTurnEnd.v1");
    return message;
}

// Sent by ShutdownAllWindowHostsForProcessExit to detach a host on its own thread.
[[nodiscard]] inline RegisteredMessage WindowHostProcessExitDetach() noexcept
{
    static const RegisteredMessage message = Register(L"RedSalamanders.DxUi.WindowHost.ProcessExitDetach.v1");
    return message;
}

// Posted to run a UI Automation provider's action on the window's thread; lParam is a posted-payload token.
[[nodiscard]] inline RegisteredMessage AccessibilityUiThreadAction() noexcept
{
    static const RegisteredMessage message = Register(L"RedSalamanders.DxUi.Accessibility.UiThreadAction.v1");
    return message;
}

// Sent from another thread to create the window's root UI Automation provider; lParam is where it is stored.
[[nodiscard]] inline RegisteredMessage AccessibilityCreateProvider() noexcept
{
    static const RegisteredMessage message = Register(L"RedSalamanders.DxUi.Accessibility.CreateProvider.v1");
    return message;
}

// Posted by ContextMenu::PostMenuBarHover to a window of a modal menu's thread. That menu's loop takes it from any of
// the thread's windows and calls switchRootFromMenuBarHover with the hovered item (wParam) and sequence (lParam).
[[nodiscard]] inline RegisteredMessage ContextMenuRootHoverChanged() noexcept
{
    static const RegisteredMessage message = Register(L"RedSalamanders.DxUi.ContextMenu.RootHoverChanged.v1");
    return message;
}

// Messages DxUi posts or sends to its own menu popup windows.

// Invokes a row for a UI Automation client; lParam is a posted-payload token naming the popup and the row.
[[nodiscard]] inline RegisteredMessage MenuPopupAccessibleInvoke() noexcept
{
    static const RegisteredMessage message = Register(L"RedSalamanders.DxUi.MenuPopup.AccessibleInvoke.v1");
    return message;
}

// Repaints a popup once a row's accessibility element took the focus; lParam is a posted-payload token naming the popup
// and the row.
[[nodiscard]] inline RegisteredMessage MenuPopupAccessibleFocus() noexcept
{
    static const RegisteredMessage message = Register(L"RedSalamanders.DxUi.MenuPopup.AccessibleFocus.v1");
    return message;
}

// Finalizes an asynchronous session that a failed DPI reflow dismissed inside a window operation.
[[nodiscard]] inline RegisteredMessage MenuPopupDeferredFinalize() noexcept
{
    static const RegisteredMessage message = Register(L"RedSalamanders.DxUi.MenuPopup.DeferredFinalize.v1");
    return message;
}

// Lets the window under the pointer choose its cursor, outside any menu handler (see ApplyMenuPointerCursor).
[[nodiscard]] inline RegisteredMessage MenuPopupForwardCursor() noexcept
{
    static const RegisteredMessage message = Register(L"RedSalamanders.DxUi.MenuPopup.ForwardCursor.v1");
    return message;
}

// Diagnostics probes (DXUI_ENABLE_DIAGNOSTICS) that read or set a popup's state from another thread.

[[nodiscard]] inline RegisteredMessage MenuPopupDebugCaptureBitmap() noexcept
{
    static const RegisteredMessage message = Register(L"RedSalamanders.DxUi.MenuPopup.DebugCaptureBitmap.v1");
    return message;
}

[[nodiscard]] inline RegisteredMessage MenuPopupDebugGetItemText() noexcept
{
    static const RegisteredMessage message = Register(L"RedSalamanders.DxUi.MenuPopup.DebugGetItemText.v1");
    return message;
}

[[nodiscard]] inline RegisteredMessage MenuPopupDebugSetBackdrop() noexcept
{
    static const RegisteredMessage message = Register(L"RedSalamanders.DxUi.MenuPopup.DebugSetBackdrop.v1");
    return message;
}

[[nodiscard]] inline RegisteredMessage MenuPopupDebugGetState() noexcept
{
    static const RegisteredMessage message = Register(L"RedSalamanders.DxUi.MenuPopup.DebugGetState.v1");
    return message;
}

[[nodiscard]] inline RegisteredMessage MenuPopupDebugGetItemRect() noexcept
{
    static const RegisteredMessage message = Register(L"RedSalamanders.DxUi.MenuPopup.DebugGetItemRect.v1");
    return message;
}

[[nodiscard]] inline RegisteredMessage MenuPopupDebugGetItemPaint() noexcept
{
    static const RegisteredMessage message = Register(L"RedSalamanders.DxUi.MenuPopup.DebugGetItemPaint.v1");
    return message;
}

// Posted by TextInputServices to the application window it is attached to, to grant its coalesced deferred TSF lock;
// wParam is the service's dispatch cookie.
[[nodiscard]] inline RegisteredMessage TextInputServicesDeferredLock() noexcept
{
    static const RegisteredMessage message = Register(L"RedSalamanders.DxUi.TextInputServices.DeferredLock.v1");
    return message;
}
} // namespace DxUi::WndMsg
