#pragma once

#include "FailureHost.h"
#include <DbgHelp.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <string>
#include <wil/resource.h>
#include <windows.h>

namespace DxUiFailureTests
{
namespace Detail
{
struct HangDumpContext
{
    HangDumpContext(HANDLE child, HANDLE file, DWORD id) noexcept : process(child), output(file), processId(id)
    {
    }
    HangDumpContext(const HangDumpContext&)            = delete;
    HangDumpContext& operator=(const HangDumpContext&) = delete;
    HangDumpContext(HangDumpContext&&)                 = delete;
    HangDumpContext& operator=(HangDumpContext&&)      = delete;
    HANDLE process;
    HANDLE output;
    DWORD processId;
    std::atomic<HRESULT> result{E_PENDING};
};
inline DWORD WINAPI WriteHangDump(void* parameter) noexcept
{
    const DxUi::Failure::Scope failureScope;
    auto& context  = *static_cast<HangDumpContext*>(parameter);
    HRESULT result = HRESULT_FROM_WIN32(ERROR_BUSY);
    if (TryAcquireSRWLockExclusive(&DxUiSample::FailureHost::dbgHelpGate))
    {
        const auto unlock = wil::scope_exit([] { ReleaseSRWLockExclusive(&DxUiSample::FailureHost::dbgHelpGate); });
        result            = DxUi::Failure::InvokeNative(MiniDumpWriteDump,
                                                        context.process,
                                                        context.processId,
                                                        context.output,
                                                        static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules),
                                                        nullptr,
                                                        nullptr,
                                                        nullptr)
                                ? S_OK
                                : HRESULT_FROM_WIN32(GetLastError());
    }
    context.result.store(result, std::memory_order_release);
    return 0;
}
} // namespace Detail

// A timed-out writer must never outlive its borrowed process, output or stack context.
[[nodiscard]] inline HRESULT CaptureLiveHangDump(HANDLE process, DWORD pid, const std::wstring& path) noexcept
{
    wil::unique_hfile output(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (! output)
        return HRESULT_FROM_WIN32(GetLastError());
    Detail::HangDumpContext context{process, output.get(), pid};
    wil::unique_handle worker(CreateThread(nullptr, 0, Detail::WriteHangDump, &context, 0, nullptr));
    if (! worker)
        return HRESULT_FROM_WIN32(GetLastError());
    const DWORD completed = WaitForSingleObject(worker.get(), 5000u);
    if (completed != WAIT_OBJECT_0)
    {
        const HRESULT error = completed == WAIT_TIMEOUT ? HRESULT_FROM_WIN32(ERROR_TIMEOUT) : HRESULT_FROM_WIN32(GetLastError());
        std::fprintf(stderr, "Live child dump worker did not complete: pid=%lu result=%08lx path=%ls\n", pid, static_cast<unsigned long>(error), path.c_str());
        std::fflush(stderr);
        DxUi::Failure::FailContract(error);
    }
    return context.result.load(std::memory_order_acquire);
}

// Only the three test-owned standard handles are inheritable by the child.
class ChildOutputCapture final
{
public:
    ChildOutputCapture() noexcept                            = default;
    ~ChildOutputCapture()                                    = default;
    ChildOutputCapture(const ChildOutputCapture&)            = delete;
    ChildOutputCapture& operator=(const ChildOutputCapture&) = delete;
    ChildOutputCapture(ChildOutputCapture&&)                 = delete;
    ChildOutputCapture& operator=(ChildOutputCapture&&)      = delete;

    [[nodiscard]] HRESULT Initialize(const std::wstring& stdoutPath, const std::wstring& stderrPath) noexcept
    {
        if (_attributes || _input || _output || _error)
            return E_UNEXPECTED;
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        _input.reset(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (! _input)
            return HRESULT_FROM_WIN32(GetLastError());
        _output.reset(CreateFileW(stdoutPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (! _output)
            return HRESULT_FROM_WIN32(GetLastError());
        _error.reset(CreateFileW(stderrPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (! _error)
            return HRESULT_FROM_WIN32(GetLastError());
        SIZE_T bytes{};
        static_cast<void>(InitializeProcThreadAttributeList(nullptr, 1u, 0u, &bytes));
        if (! bytes)
            return E_UNEXPECTED;
        _storage.reset(static_cast<std::byte*>(HeapAlloc(GetProcessHeap(), 0, bytes)));
        if (! _storage)
            return E_OUTOFMEMORY;
        auto* const attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(_storage.get());
        if (! InitializeProcThreadAttributeList(attributes, 1u, 0u, &bytes))
            return HRESULT_FROM_WIN32(GetLastError());
        _attributes.reset(attributes);
        _inherited = {_input.get(), _output.get(), _error.get()};
        if (! UpdateProcThreadAttribute(_attributes.get(), 0u, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, _inherited.data(), sizeof(_inherited), nullptr, nullptr))
            return HRESULT_FROM_WIN32(GetLastError());
        _startup.StartupInfo.cb         = sizeof(_startup);
        _startup.StartupInfo.dwFlags    = STARTF_USESTDHANDLES;
        _startup.StartupInfo.hStdInput  = _input.get();
        _startup.StartupInfo.hStdOutput = _output.get();
        _startup.StartupInfo.hStdError  = _error.get();
        _startup.lpAttributeList        = _attributes.get();
        return S_OK;
    }
    [[nodiscard]] STARTUPINFOEXW* Startup() noexcept
    {
        return &_startup;
    }
    [[nodiscard]] DWORD CreationFlags() const noexcept
    {
        return EXTENDED_STARTUPINFO_PRESENT;
    }
    void CloseParentHandles() noexcept
    {
        _attributes.reset();
        _startup.lpAttributeList = nullptr;
        _input.reset();
        _output.reset();
        _error.reset();
    }

private:
    wil::unique_hfile _input, _output, _error;
    wil::unique_process_heap_ptr<std::byte> _storage;
    std::array<HANDLE, 3> _inherited{};
    wil::unique_any<LPPROC_THREAD_ATTRIBUTE_LIST, decltype(&DeleteProcThreadAttributeList), DeleteProcThreadAttributeList> _attributes;
    STARTUPINFOEXW _startup{};
};
} // namespace DxUiFailureTests
