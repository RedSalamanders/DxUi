#include <DxUi/DxUi.h>
#include <DxUi/Failure.h>

#include <array>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <wil/result.h>
#include <wil/win32_helpers.h>

namespace
{
DxUi::Failure::ModuleRegistration moduleRegistration;
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

static int RunModuleProbe()
{
    DxUi::Failure::Scope failureScope;
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

// The executable owns the host. Each DLL registers explicitly after loading,
// then detaches after all module work has joined and before FreeLibrary.
extern "C" __declspec(dllexport) HRESULT DxUiModuleAttach(const DxUi::Failure::HostServices* services) noexcept
{
    return services ? moduleRegistration.Attach(*services) : E_INVALIDARG;
}
extern "C" __declspec(dllexport) HRESULT DxUiModuleDetach() noexcept
{
    return moduleRegistration.Detach();
}
__declspec(noinline) void DxUiModuleFatalWorker(int kind)
{
    DxUi::Failure::Scope workerScope;
    if (kind == 1)
        throw std::bad_alloc{};
    throw 17;
}
extern "C" __declspec(dllexport) void DxUiModuleFatal(int kind) noexcept
{
    DxUi::Failure::Scope failureScope;
    std::jthread worker([kind] { DxUiModuleFatalWorker(kind); });
    worker.join();
}

// A real DxUi callback entered while a synthetic native ABI is on the stack.
// This origin must remain present in the owning module's matched PDB.
__declspec(noinline) void DxUiModuleReentrantOrigin(int kind)
{
    if (kind == 1)
        throw std::bad_alloc{};
    throw 19;
}
extern "C" __declspec(dllexport) void DxUiModuleInvokeReentrant(int kind) noexcept
{
    const DxUi::Failure::Scope entryScope;
    DxUi::ControlHost host;
    AnimationProbe control;
    control.SetAccessibleInvoke([kind](DxUi::ControlHost&) { DxUiModuleReentrantOrigin(kind); });
    static_cast<void>(control.InvokeAccessibleResult(host));
}
using NativeReentry = void (*)(int) noexcept;
extern "C" __declspec(dllexport) void DxUiModuleNativeReentry(int kind, NativeReentry bridge) noexcept
{
    const DxUi::Failure::Scope outerScope;
    // Prepare this module's callback before entering the native allowance.
    DxUi::ControlHost host;
    AnimationProbe control;
    control.SetAccessibleInvoke([kind](DxUi::ControlHost&) { DxUiModuleReentrantOrigin(kind); });
    const DxUi::Failure::NativeCallScope nativeScope;
    if (bridge)
        bridge(kind);
    else
        static_cast<void>(control.InvokeAccessibleResult(host));
}
__declspec(noinline) HRESULT DxUiModuleSimulatedNativeAbi(HRESULT result)
{
    try
    {
        if (FAILED(result))
            THROW_HR(result);
        return result;
    }
    catch (const wil::ResultException& error)
    {
        return error.GetErrorCode();
    }
}
extern "C" __declspec(dllexport) HRESULT DxUiModuleNativeCaught() noexcept
{
    return DxUi::Failure::InvokeNative(&DxUiModuleSimulatedNativeAbi, E_ACCESSDENIED);
}
// An escaping exception has no nested DxUi Scope in the native function.
// The invoking module's SEH filter must report this origin through the EXE host.
__declspec(noinline) int DxUiModuleEscapingNativeOrigin(int kind)
{
    if (kind == 1)
        throw std::bad_alloc{};
    if (kind == 2)
        throw 23;
    return kind;
}
extern "C" __declspec(dllexport) void DxUiModuleEscapingNative(int kind) noexcept
{
    static_cast<void>(DxUi::Failure::InvokeNative(&DxUiModuleEscapingNativeOrigin, kind));
}

// Each module owns its HWNDs, controls and callback storage. Only a POD result crosses the ABI.
extern "C" __declspec(dllexport) int DxUiModuleProbe() noexcept
{
    try
    {
        return RunModuleProbe();
    }
    catch (const std::bad_alloc&)
    {
        std::terminate();
    }
    catch (const std::exception& error)
    {
        // Report fixture setup failures without allowing an exception through the plugin ABI.
        std::fprintf(stderr, "Module fixture failed: %s\n", error.what());
        return 6;
    }
}
