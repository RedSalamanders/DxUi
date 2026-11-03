#pragma once

#include "FailureHost.h"
#include "Support.Tests.FatalChildren.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <memory>
#include <objbase.h>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace DxUiFailureTests
{
[[nodiscard]] HRESULT DisconnectTestProcessUiAutomation() noexcept;
namespace Detail
{
inline std::wstring evidenceDirectory;
inline DxUiSample::FailureHost::Host ordinaryHost;
inline bool ordinaryInstalled{};
struct RetainedAccessibilitySta
{
    RetainedAccessibilitySta()                                           = default;
    RetainedAccessibilitySta(const RetainedAccessibilitySta&)            = delete;
    RetainedAccessibilitySta& operator=(const RetainedAccessibilitySta&) = delete;
    RetainedAccessibilitySta(RetainedAccessibilitySta&&)                 = delete;
    RetainedAccessibilitySta& operator=(RetainedAccessibilitySta&&)      = delete;

    wil::unique_event_nothrow finished;
    wil::unique_event_nothrow stop;
    wil::unique_event_nothrow pumped;
    std::atomic<HRESULT> started{E_PENDING};
    std::atomic<DWORD> threadId{};
    std::jthread thread;
};
// The control runner owns these threads on its main thread. Each completed
// case keeps its distinct registering STA pumping until process UIA/GIT drain.
inline std::vector<std::unique_ptr<RetainedAccessibilitySta>> retainedAccessibilityStas;
} // namespace Detail

// The callable owns and destroys its views/clients before returning. Only
// then does this helper signal the caller and keep that exact STA alive.
template <typename Callback> [[nodiscard]] HRESULT RunRetainedAccessibilitySta(Callback callback)
{
    const DxUi::Failure::Scope failureScope;
    auto entry = std::make_unique<Detail::RetainedAccessibilitySta>();
    for (auto* event : std::array{std::addressof(entry->finished), std::addressof(entry->stop), std::addressof(entry->pumped)})
    {
        event->reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (! *event)
        {
            const DWORD error = GetLastError();
            return error ? HRESULT_FROM_WIN32(error) : E_FAIL;
        }
    }
    auto* const state = entry.get();
    Detail::retainedAccessibilityStas.push_back(std::move(entry));
    state->thread = std::jthread([state, callback = std::move(callback)]() mutable
    {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        state->started.store(initialized, std::memory_order_release);
        if (FAILED(initialized))
        {
            if (! SetEvent(state->finished.get()))
                DxUi::Failure::FailContract(HRESULT_FROM_WIN32(GetLastError()));
            return;
        }
        const auto apartment = wil::scope_exit([]() noexcept { CoUninitialize(); });
        callback();
        MSG initial{};
        static_cast<void>(PeekMessageW(&initial, nullptr, 0u, 0u, PM_NOREMOVE)); // Ensure a thread queue exists.
        state->threadId.store(GetCurrentThreadId(), std::memory_order_release);
        if (! SetEvent(state->finished.get()))
            DxUi::Failure::FailContract(HRESULT_FROM_WIN32(GetLastError()));
        const HANDLE stop = state->stop.get();
        for (;;)
        {
            const DWORD waited = MsgWaitForMultipleObjectsEx(1u, &stop, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (waited == WAIT_OBJECT_0)
                return;
            if (waited != WAIT_OBJECT_0 + 1u)
                DxUi::Failure::FailContract(waited == WAIT_FAILED ? HRESULT_FROM_WIN32(GetLastError()) : E_UNEXPECTED);
            MSG message{};
            for (size_t count = 0u; count < 64u && PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE); ++count)
            {
                // A completed fixture must remain alive even if its test posted WM_QUIT.
                if (message.message == WM_NULL && message.wParam == reinterpret_cast<WPARAM>(state))
                {
                    if (! SetEvent(state->pumped.get()))
                        DxUi::Failure::FailContract(HRESULT_FROM_WIN32(GetLastError()));
                }
                else if (message.message != WM_QUIT)
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }
        }
    });
    // The configurable runner watchdog owns the case deadline; avoid a competing timeout.
    const DWORD completed = WaitForSingleObject(state->finished.get(), INFINITE);
    if (completed != WAIT_OBJECT_0)
        DxUi::Failure::FailContract(HRESULT_FROM_WIN32(GetLastError()));
    const HRESULT initialized = state->started.load(std::memory_order_acquire);
    if (FAILED(initialized))
        return initialized;
    if (! PostThreadMessageW(state->threadId.load(std::memory_order_acquire), WM_NULL, reinterpret_cast<WPARAM>(state), 0))
        DxUi::Failure::FailContract(HRESULT_FROM_WIN32(GetLastError()));
    const DWORD pumped = WaitForSingleObject(state->pumped.get(), 5000u);
    if (pumped != WAIT_OBJECT_0)
        DxUi::Failure::FailContract(pumped == WAIT_TIMEOUT ? HRESULT_FROM_WIN32(ERROR_TIMEOUT) : HRESULT_FROM_WIN32(GetLastError()));
    return initialized;
}

inline void StopRetainedAccessibilityStas() noexcept
{
    const DxUi::Failure::Scope failureScope;
    for (const auto& entry : Detail::retainedAccessibilityStas)
        if (! SetEvent(entry->stop.get()))
            DxUi::Failure::FailContract(HRESULT_FROM_WIN32(GetLastError()));
    const ULONGLONG started = GetTickCount64();
    for (const auto& entry : Detail::retainedAccessibilityStas)
    {
        if (! entry->thread.joinable())
            continue;
        const ULONGLONG elapsed = GetTickCount64() - started;
        if (elapsed >= 5000u)
            DxUi::Failure::FailContract(HRESULT_FROM_WIN32(ERROR_TIMEOUT));
        const DWORD waited = WaitForSingleObject(entry->thread.native_handle(), static_cast<DWORD>(5000u - elapsed));
        if (waited != WAIT_OBJECT_0)
            DxUi::Failure::FailContract(waited == WAIT_TIMEOUT ? HRESULT_FROM_WIN32(ERROR_TIMEOUT) : HRESULT_FROM_WIN32(GetLastError()));
        entry->thread.join();
    }
    Detail::retainedAccessibilityStas.clear();
}

// Call after child dispatch and before TestFailurePolicy. Creates a process-owned
// directory adjacent to this profile's test executable; never deletes evidence.
[[nodiscard]] inline HRESULT ConfigureFailureEvidence()
{
    if (! Detail::evidenceDirectory.empty())
        return S_FALSE;
    std::array<wchar_t, 32768> path{};
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (! length || length >= path.size())
        return HRESULT_FROM_WIN32(length ? ERROR_INSUFFICIENT_BUFFER : GetLastError());
    std::wstring root(path.data(), length);
    const auto separator = root.find_last_of(L"\\/");
    if (separator == std::wstring::npos)
        return HRESULT_FROM_WIN32(ERROR_BAD_PATHNAME);
    root.resize(separator);
    root += L"\\FailureEvidence";
    if (! CreateDirectoryW(root.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return HRESULT_FROM_WIN32(GetLastError());
    const auto attributes = GetFileAttributesW(root.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || ! (attributes & FILE_ATTRIBUTE_DIRECTORY))
        return HRESULT_FROM_WIN32(ERROR_DIRECTORY);
    // Preserve earlier evidence when Windows reuses a PID during repeated runs.
    root += L"\\" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    if (! CreateDirectoryW(root.c_str(), nullptr))
    {
        const DWORD error = GetLastError();
        std::fprintf(stderr, "Cannot create failure evidence for process %lu: Win32 %lu\n", GetCurrentProcessId(), error);
        return HRESULT_FROM_WIN32(error);
    }
    if (! ConfigureFailureChildren(root))
        return E_FAIL;
    Detail::evidenceDirectory = std::move(root);
    return S_OK;
}

// Call after TestFailurePolicy, before constructing controls or starting workers.
[[nodiscard]] inline HRESULT InstallFailureHost()
{
    if (Detail::ordinaryInstalled)
        return S_FALSE;
    if (Detail::evidenceDirectory.empty())
        return E_UNEXPECTED;
    const auto started = Detail::ordinaryHost.Start(Detail::evidenceDirectory + L"\\ordinary");
    if (FAILED(started))
        return started;
    const auto installed = Detail::ordinaryHost.Install();
    if (SUCCEEDED(installed))
        Detail::ordinaryInstalled = true;
    return installed;
}

// Call only after all DxUi objects, workers and module registrations are gone.
[[nodiscard]] inline HRESULT UninstallFailureHost() noexcept
{
    if (! Detail::ordinaryInstalled)
        return S_FALSE;
    // This test executable owns the process. Release Windows' retained provider
    // resources after its views/clients stop, while the failure host is still live.
    // A library or a single plugin must never perform this process-wide cleanup.
    const auto disconnected = DisconnectTestProcessUiAutomation();
    if (FAILED(disconnected))
    {
        if (! Detail::retainedAccessibilityStas.empty())
            DxUi::Failure::FailContract(disconnected);
        return disconnected;
    }
    const auto result = Detail::ordinaryHost.UninstallWhenIdle(5000u, DisconnectTestProcessUiAutomation);
    if (FAILED(result) && ! Detail::retainedAccessibilityStas.empty())
        DxUi::Failure::FailContract(result);
    if (SUCCEEDED(result))
    {
        if (! Detail::retainedAccessibilityStas.empty())
        {
            // The first uninstall proves provider pins have drained. Restore the
            // reporter before apartment teardown and any shutdown failure.
            const HRESULT reinstalled = Detail::ordinaryHost.Install();
            if (FAILED(reinstalled))
                DxUi::Failure::FailContract(reinstalled);
            StopRetainedAccessibilityStas();
            const HRESULT finalResult = Detail::ordinaryHost.UninstallWhenIdle(5000u, DisconnectTestProcessUiAutomation);
            if (FAILED(finalResult))
                DxUi::Failure::FailContract(finalResult);
        }
        Detail::ordinaryInstalled = false;
    }
    return result;
}
} // namespace DxUiFailureTests
