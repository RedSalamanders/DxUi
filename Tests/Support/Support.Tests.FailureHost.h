#pragma once

#include "FailureHost.h"
#include "Support.Tests.FatalChildren.h"
#include <array>
#include <cstdio>
#include <string>

namespace DxUiFailureTests
{
[[nodiscard]] HRESULT DisconnectTestProcessUiAutomation() noexcept;
namespace Detail
{
inline std::wstring evidenceDirectory;
inline DxUiSample::FailureHost::Host ordinaryHost;
inline bool ordinaryInstalled{};
} // namespace Detail

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
        return disconnected;
    const auto result = Detail::ordinaryHost.UninstallWhenIdle(5000u, DisconnectTestProcessUiAutomation);
    if (SUCCEEDED(result))
        Detail::ordinaryInstalled = false;
    return result;
}
} // namespace DxUiFailureTests
