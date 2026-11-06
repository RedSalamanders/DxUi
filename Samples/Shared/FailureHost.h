#pragma once

#include <DxUi/Failure.h>
#include <array>
#include <atomic>
#include <span>
#include <string_view>
#include <wil/resource.h>

namespace DxUiSample::FailureHost
{
inline constexpr std::size_t ProcessInstanceKeyCapacity = 32u;
// Uses the held process handle, not a later lookup of a potentially reused PID.
// Failure preserves the destination; no allocation or diagnostic hook is needed.
[[nodiscard]] HRESULT GetProcessInstanceKey(HANDLE process, std::span<wchar_t> destination) noexcept;

// DbgHelp is process-global and not thread-safe. A real application should
// share this gate with every one of its own DbgHelp users.
inline constinit SRWLOCK dbgHelpGate = SRWLOCK_INIT;
// Application-owned example. Start before installing hooks or entering DxUi.
// The host and its borrowed services must outlive every registered module.
class Host final
{
public:
    Host() noexcept = default;
    ~Host();
    Host(const Host&)            = delete;
    Host& operator=(const Host&) = delete;
    [[nodiscard]] HRESULT Start(std::wstring_view outputPrefix) noexcept;
    [[nodiscard]] HRESULT StartBesideExecutable() noexcept;
    [[nodiscard]] HRESULT Install() noexcept;
    [[nodiscard]] HRESULT Uninstall() noexcept;
    // Application shutdown only, after stopping new work and retiring every view,
    // client, worker and DLL. May dispatch owner messages while UIA releases peers.
    // Optional process-owner cleanup may repeat while native releases are in flight.
    [[nodiscard]] HRESULT UninstallWhenIdle(DWORD timeoutMs = 5000u, HRESULT (*cleanup)() noexcept = nullptr) noexcept;
    [[nodiscard]] const DxUi::Failure::HostServices* Services() const noexcept;
    // Reuse this prestarted writer behind an application-provided reporter.
    [[nodiscard]] HRESULT ReportRecord(const DxUi::Failure::Record& record) noexcept;

private:
    static DWORD WINAPI WorkerEntry(void* context) noexcept;
    static HRESULT Report(void* context, const DxUi::Failure::Record& record) noexcept;
    DWORD Worker() noexcept;
    HRESULT WriteEvidence() noexcept;

    std::array<wchar_t, 1024> prefix_{};
    DxUi::Failure::ProcessHooks hooks_;
    wil::unique_handle request_, stop_, done_, thread_;
    DxUi::Failure::Record record_{};
    std::atomic<LONG> reporting_{};
    HRESULT result_{E_PENDING};
};
} // namespace DxUiSample::FailureHost
