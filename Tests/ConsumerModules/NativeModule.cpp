#include <DxUi/DxUi.h>

#include <array>
#include <cstdio>
#include <wil/win32_helpers.h>

namespace
{
class AnimationProbe final : public DxUi::Control
{
public:
    unsigned ticks = 0u;
    void Paint(DxUi::ControlHost&) const override
    {
    }
    bool Tick(DxUi::ControlHost&, uint64_t) override
    {
        ++ticks;
        return true;
    }
};

bool WindowProcedureBelongsToModule(HWND hwnd, HINSTANCE expected) noexcept
{
    HMODULE actual       = nullptr;
    const auto procedure = GetClassLongPtrW(hwnd, GCLP_WNDPROC);
    return procedure != 0 &&
           GetModuleHandleExW(
               GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(procedure), &actual) != FALSE &&
           actual == expected && reinterpret_cast<HINSTANCE>(GetClassLongPtrW(hwnd, GCLP_HMODULE)) == expected;
}
} // namespace

// The fixture crosses the module boundary using only an integer result. Each copy owns its HWNDs,
// controls, callbacks and allocations, just as an independently linked product plugin must.
extern "C" __declspec(dllexport) int DxUiModuleProbe()
{
    const HINSTANCE instance = wil::GetModuleInstanceHandle();
    wil::unique_hwnd owner(CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", L"DxUi module fixture", WS_POPUP, 160, 160, 320, 240, nullptr, nullptr, instance, nullptr));
    if (! owner)
        return 1;
    ShowWindow(owner.get(), SW_SHOWNOACTIVATE);
    DxUi::ControlHost host;
    if (! host.Attach(owner.get()))
        return 2;
    const auto detach  = wil::scope_exit([&]() noexcept { host.Detach(); });
    auto probe         = std::make_unique<AnimationProbe>();
    auto* probePointer = probe.get();
    host.SetRoot(std::move(probe));
    host.RequestAnimation();
    const ULONGLONG deadline = GetTickCount64() + 1000u;
    while (probePointer->ticks == 0u && GetTickCount64() < deadline)
    {
        MSG message{};
        for (unsigned count = 0u; count < 128u && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE); ++count)
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (probePointer->ticks == 0u)
            MsgWaitForMultipleObjectsEx(0, nullptr, 20u, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
    const bool animationWorks = host.DebugHasActiveAnimationSubscription() && probePointer->ticks > 0u;
    bool dispatcherOwned      = false;
    for (HWND window = FindWindowExW(HWND_MESSAGE, nullptr, L"DxUi.AnimationDispatcher", nullptr); window;
         window      = FindWindowExW(HWND_MESSAGE, window, L"DxUi.AnimationDispatcher", nullptr))
    {
        if (GetWindowThreadProcessId(window, nullptr) == GetCurrentThreadId() && WindowProcedureBelongsToModule(window, instance))
            dispatcherOwned = true;
    }
    const std::array items{DxUi::MenuFlyoutItem{.text = L"Module-local command", .commandId = 73}};
    bool closed = false;
    std::optional<int> command;
    if (! DxUi::ContextMenu::ShowAsync(owner.get(),
                                       POINT{180, 180},
                                       items,
                                       DxUi::MakeDefaultThemePalette(false),
                                       [&](std::optional<int> selected) noexcept
    {
        closed  = true;
        command = selected;
    }))
        return 3;
    HWND popup = nullptr;
    for (HWND window = FindWindowExW(nullptr, nullptr, L"DxUi_ContextMenu", nullptr); window;
         window      = FindWindowExW(nullptr, window, L"DxUi_ContextMenu", nullptr))
    {
        if (GetWindow(window, GW_OWNER) == owner.get())
        {
            popup = window;
            break;
        }
    }
    if (! popup)
    {
        owner.reset(); // Dismiss the owned asynchronous session before its callback storage goes away.
        return 4;
    }
    const bool popupOwned = WindowProcedureBelongsToModule(popup, instance);
    SendMessageW(popup, WM_KEYDOWN, VK_HOME, 0);
    SendMessageW(popup, WM_KEYDOWN, VK_RETURN, 0);
    const bool invoked = closed && command == 73 && ! IsWindow(popup);
    if (IsWindow(popup))
        SendMessageW(popup, WM_KEYDOWN, VK_ESCAPE, 0);
    std::printf("module=%p animation=%d dispatcher=%d popup=%d invoked=%d ticks=%u\n",
                static_cast<void*>(instance),
                animationWorks,
                dispatcherOwned,
                popupOwned,
                invoked,
                probePointer->ticks);
    return animationWorks && dispatcherOwned && popupOwned && invoked ? 0 : 5;
}
