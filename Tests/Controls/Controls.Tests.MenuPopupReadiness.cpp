#include "Controls.Tests.MenuPopupReadiness.h"

#include "Controls.Tests.DxUiTestHelpers.h"

void TestMenuPopupDiscoveryRejectsHiddenInitializationWindows()
{
    // Never switch the input desktop or take the person's foreground, focus or pointer.
    const std::wstring desktopName = std::format(L"DxUi.PopupReadiness.{}", GetCurrentProcessId());
    const wil::unique_hdesk desktop(CreateDesktopW(desktopName.c_str(), nullptr, nullptr, 0, GENERIC_ALL, nullptr));
    Require(desktop != nullptr, "create a private popup-readiness desktop");
    std::thread worker([&]
    {
        Require(SetThreadDesktop(desktop.get()) != FALSE, "attach the readiness fixture to its non-input desktop");
        const auto makeWindow = [](HWND owner)
        {
            return wil::unique_hwnd(
                CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"STATIC", L"", WS_POPUP, 0, 0, 1, 1, owner, nullptr, GetModuleHandleW(nullptr), nullptr));
        };
        const auto owner      = makeWindow(nullptr);
        const auto otherOwner = makeWindow(nullptr);
        const auto popup      = makeWindow(owner.get());
        const auto unrelated  = makeWindow(otherOwner.get());
        Require(owner && otherOwner && popup && unrelated, "create owned measurement windows");
        ShowWindow(owner.get(), SW_SHOWNOACTIVATE);
        ShowWindow(otherOwner.get(), SW_SHOWNOACTIVATE);
        ShowWindow(unrelated.get(), SW_SHOWNOACTIVATE);

        // No timing assumption: while initialization is held at the hidden stage, even a
        // matching class and owner cannot satisfy discovery. The old helper returned popup here.
        Require(! FindOwnedContextMenuPopupWindow(owner.get(), L"STATIC"), "ignore the hidden measurement HWND and another owner's visible popup");
        Require(! WaitForOwnedContextMenuPopupWindow(owner.get(), std::chrono::milliseconds(0), L"STATIC"),
                "a never-ready popup does not become a successful discovery");
        Require(! FindOwnedContextMenuPopupWindow(nullptr, L"STATIC"), "an absent owner cannot match an unowned window");

        ShowWindow(popup.get(), SW_SHOWNOACTIVATE);
        Require(IsWindowVisible(popup.get()) != FALSE, "release the synthetic initialization stage");
        Require(WaitForOwnedContextMenuPopupWindow(owner.get(), std::chrono::milliseconds(0), L"STATIC") == popup.get(),
                "the same HWND becomes discoverable only after initialization reveals it");
        Require(! FindOwnedContextMenuPopupWindow(owner.get(), L"BUTTON"), "discovery preserves its window-class filter");

        ShowWindow(popup.get(), SW_HIDE);
        Require(! FindOwnedContextMenuPopupWindow(owner.get(), L"STATIC"), "a hidden popup is no longer ready");
    });
    worker.join();
}
