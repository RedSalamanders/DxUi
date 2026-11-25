#include "Controls.Tests.SharedDeviceStack.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls/DxUi.Internal.h"
#include "Controls/DxUi.RetainedCaches.h"
#include "GraphicsFixture.h"
#include "Support/WorkCounters.h"

#include <DxUi/Embedded.h>

#include <format>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// W2.4's G10 clause without focus: the FX4 sessions' device lifetime, with the menu's own popup window replaced by a hidden
// window attached exactly as MenuPopup attaches (a composition swap chain) and rendered with RenderInitialFrameForShow, so
// nothing is shown or activated. The real FX4 ten-session count stays the interactive Menu suite's.
namespace
{
using DxUi::Detail::WorkCounters;
using DxUi::Detail::WorkCounterScope;

struct SessionWork
{
    uint64_t d3d11Devices       = 0u;
    uint64_t d2dDevices         = 0u;
    uint64_t compositionDevices = 0u;
};

// One popup session on the calling thread: attach, first frame, detach and destroy.
[[nodiscard]] SessionWork RunPopupSession(bool forceWarp = true)
{
    const WorkCounterScope scope;
    {
        AttachedHostWindow popup(DxUi::WindowHost::PresentationMode::CompositionSwapChain, forceWarp);
        auto root = std::make_unique<DxUi::Panel>();
        root->AddChild<DxUi::Label>(L"Session item")->SetBounds(D2D1::RectF(4.0f, 4.0f, 116.0f, 28.0f));
        popup.Host().SetRoot(std::move(root));
        SetWindowPos(popup.Hwnd(), nullptr, 0, 0, 120, 90, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        Require(popup.Host().RenderInitialFrameForShow(), "a popup session renders its first frame");
        Require(IsWindowVisible(popup.Hwnd()) == FALSE, "a popup session's window is never shown");
    }
    const WorkCounters work = scope.Delta();
    return {work.resources.d3d11Device.created, work.frame.d2dDevices, work.frame.compositionDevices};
}

// An application's EmbeddedHost on the calling thread, on its own WARP device.
struct EmbeddedView final
{
    EmbeddedView()
    {
        Require(SUCCEEDED(gpu.Create(true)) && SUCCEEDED(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics)), "the application's WARP device");
        Require(SUCCEEDED(view.Attach(graphics, StoppedTestClock())), "the application's EmbeddedHost attaches");
    }
    ~EmbeddedView()
    {
        view.Detach();
    }
    EmbeddedView(const EmbeddedView&)            = delete;
    EmbeddedView& operator=(const EmbeddedView&) = delete;
    EmbeddedView(EmbeddedView&&)                 = delete;
    EmbeddedView& operator=(EmbeddedView&&)      = delete;

    GraphicsFixture gpu;
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    DxUi::EmbeddedHost view;
};

// Runs `body` on a new thread (nothing of the test thread's own hosts is shared) and reports its failure, if any.
void OnCleanThread(const std::function<std::string()>& body, const char* context)
{
    std::string failure;
    DWORD threadId = 0u;
    std::thread thread([&]
    {
        threadId = GetCurrentThreadId();
        failure  = body();
    });
    thread.join();
    if (! failure.empty())
        std::cerr << "  " << context << ": " << failure << '\n';
    Require(failure.empty(), context);
    Require(! DxUi::HasSharedDeviceBucketForTest(threadId), "nothing of the thread's shared device stack outlives the thread");
}

[[nodiscard]] std::string Expect(bool condition, std::string_view what)
{
    return condition ? std::string() : std::string(what);
}
} // namespace

void TestSharedDeviceStackKeptAcrossEmbeddedOnlyMenuSessions()
{
    OnCleanThread(
        []() -> std::string
    {
        const DWORD thread = GetCurrentThreadId();
        EmbeddedView application;
        if (DxUi::HasSharedDeviceStackForTest(thread))
            return "an EmbeddedHost created a shared native device stack";
        std::vector<SessionWork> sessions;
        for (int session = 0; session < 11; ++session)
            sessions.push_back(RunPopupSession());
        uint64_t later = 0u;
        for (size_t session = 1u; session < sessions.size(); ++session)
            later += sessions[session].d3d11Devices + sessions[session].d2dDevices + sessions[session].compositionDevices;
        std::cout << std::format("  embedded-only menu sessions: first {} D3D11, {} Direct2D, {} DirectComposition devices; sessions 2 to 11: {} devices\n",
                                 sessions[0].d3d11Devices,
                                 sessions[0].d2dDevices,
                                 sessions[0].compositionDevices,
                                 later);
        if (sessions[0].d3d11Devices != 1u || sessions[0].compositionDevices != 1u)
            return "the first session creates the thread's stack: one D3D11 and one DirectComposition device";
        if (later != 0u)
            return "sessions 2 to 11 of an embedded-only application create no D3D11, Direct2D or DirectComposition device";
        if (DxUi::RetainedSharedDeviceStacks(thread) != 1u)
            return "between sessions the thread holds exactly one retained stack";
        application.view.Detach();
        return Expect(! DxUi::HasSharedDeviceBucketForTest(thread), "the stack is released when the last EmbeddedHost detaches");
    },
        "G10: eleven embedded-only menu sessions");
}

void TestSharedDeviceStackReleasedAtLastDetachWithoutEmbeddedHost()
{
    OnCleanThread(
        []() -> std::string
    {
        const DWORD thread = GetCurrentThreadId();
        for (int session = 0; session < 3; ++session)
        {
            const SessionWork work = RunPopupSession();
            if (work.d3d11Devices != 1u || work.compositionDevices != 1u)
                return "without an EmbeddedHost every session creates its stack, as before W2.4";
            if (DxUi::HasSharedDeviceBucketForTest(thread))
                return "without an EmbeddedHost the stack is released at the last detach";
        }
        return {};
    },
        "no EmbeddedHost: released at the last detach");
}

void TestSharedDeviceStackReleasedWithTheLastEmbeddedHost()
{
    OnCleanThread(
        []() -> std::string
    {
        const DWORD thread = GetCurrentThreadId();
        auto first         = std::make_unique<EmbeddedView>();
        auto second        = std::make_unique<EmbeddedView>();
        static_cast<void>(RunPopupSession());
        first.reset();
        if (! DxUi::HasSharedDeviceStackForTest(thread))
            return "the stack survives while a second EmbeddedHost is still attached";
        if (RunPopupSession().d3d11Devices != 0u)
            return "a session with one EmbeddedHost left reuses the stack";
        second.reset();
        return Expect(! DxUi::HasSharedDeviceBucketForTest(thread), "the last EmbeddedHost's detach releases the stack");
    },
        "two EmbeddedHosts");
}

void TestSharedDeviceStackDeviceLossWhileIdle()
{
    OnCleanThread(
        []() -> std::string
    {
        const DWORD thread = GetCurrentThreadId();
        EmbeddedView application;
        static_cast<void>(RunPopupSession());
        DxUi::MarkRetainedSharedDeviceStackLostForTest(thread);
        const SessionWork recovered = RunPopupSession();
        if (recovered.d3d11Devices != 1u || recovered.compositionDevices != 1u)
            return "after a device loss while idle the next session creates exactly one stack";
        return Expect(RunPopupSession().d3d11Devices == 0u, "and the session after it reuses that stack");
    },
        "device loss while idle");
}

void TestSharedDeviceStackRegistryRowCountsAndDrops()
{
    OnCleanThread(
        []() -> std::string
    {
        const DWORD thread = GetCurrentThreadId();
        EmbeddedView application;
        const auto rowEntries = [&]() -> size_t
        {
            std::vector<DxUi::RetainedCacheCount> counts;
            DxUi::RetainedCacheAccess::Count(application.view.Controls(), counts);
            for (const DxUi::RetainedCacheCount& count : counts)
                if (count.name == "SharedDeviceStack.retained")
                    return count.entries;
            return SIZE_MAX;
        };
        if (rowEntries() != 0u)
            return "the registry counts no stack before the first popup";
        static_cast<void>(RunPopupSession());
        if (rowEntries() != 1u)
            return "the registry counts the retained stack";
        application.view.Controls().DebugDropRetainedStateForTest();
        if (rowEntries() != 0u || DxUi::HasSharedDeviceStackForTest(thread))
            return "the registry's drop releases the retained stack";
        if (RunPopupSession().d3d11Devices != 1u)
            return "after the drop the next popup creates a stack again";
        // The application's own release (DxUi::ReleaseRetainedNativeDeviceStack) does the same.
        if (! DxUi::ReleaseRetainedNativeDeviceStack() || DxUi::HasSharedDeviceStackForTest(thread))
            return "ReleaseRetainedNativeDeviceStack releases the kept stack";
        return Expect(! DxUi::ReleaseRetainedNativeDeviceStack(), "ReleaseRetainedNativeDeviceStack reports that nothing was kept");
    },
        "registry row");
}

void TestSharedDeviceStackNativeHostUnaffected()
{
    OnCleanThread(
        []() -> std::string
    {
        const DWORD thread = GetCurrentThreadId();
        EmbeddedView application;
        {
            // A native host attached on the thread: popups share its stack, as before W2.4, and the registry counts nothing.
            AttachedHostWindow window(DxUi::WindowHost::PresentationMode::CompositionSwapChain, true);
            SetWindowPos(window.Hwnd(), nullptr, 0, 0, 160, 120, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            if (! window.Host().RenderInitialFrameForShow())
                return "the native host renders";
            if (RunPopupSession().d3d11Devices != 0u)
                return "a popup beside an attached native host uses its stack";
            if (DxUi::RetainedSharedDeviceStacks(thread) != 0u)
                return "a stack a native host uses is not a retained resource";
            DxUi::DropRetainedSharedDeviceStack(thread);
            if (DxUi::ReleaseRetainedNativeDeviceStack())
                return "ReleaseRetainedNativeDeviceStack does nothing while a native host is attached";
            if (! DxUi::HasSharedDeviceStackForTest(thread))
                return "the drop never releases a stack a native host uses";
        }
        if (DxUi::RetainedSharedDeviceStacks(thread) != 1u)
            return "the native host's detach keeps the stack while the EmbeddedHost is attached";
        // A native host with the other device policy starts from no stack, as on a thread that never kept one (no
        // ERROR_INVALID_STATE for a policy the retained stack was made under).
        const WorkCounterScope scope;
        AttachedHostWindow hardware(DxUi::WindowHost::PresentationMode::CompositionSwapChain, false);
        SetWindowPos(hardware.Hwnd(), nullptr, 0, 0, 160, 120, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        if (! hardware.Host().RenderInitialFrameForShow())
            return "a native host with another device policy attaches and renders";
        return Expect(scope.Delta().resources.d3d11Device.created == 1u, "it creates its own stack, as without a retained one");
    },
        "a native host on the thread");
}
