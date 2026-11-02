#pragma once

#include "FailureHost.h"
#include "Support.Tests.FatalChildren.h"
#include <array>
#include <string>

namespace DxUiFailureTests
{
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
    root += L"\\" + std::to_wstring(GetCurrentProcessId());
    if (! CreateDirectoryW(root.c_str(), nullptr))
        return HRESULT_FROM_WIN32(GetLastError());
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
    const auto result = Detail::ordinaryHost.Uninstall();
    if (SUCCEEDED(result))
        Detail::ordinaryInstalled = false;
    return result;
}
} // namespace DxUiFailureTests
