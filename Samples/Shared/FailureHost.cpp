#include "FailureHost.h"
#include <DbgHelp.h>
#include <algorithm>
#include <charconv>
#include <cstring>

namespace DxUiSample::FailureHost
{
namespace
{
constexpr DWORD ReportTimeoutMs = 15000u;
HRESULT LastError() noexcept
{
    return HRESULT_FROM_WIN32(GetLastError());
}

// Fixed storage and WriteFile keep the reporting path independent of the C++ heap.
struct Text
{
    std::array<char, 8192> bytes{};
    std::size_t used{};
    void Add(std::string_view value) noexcept
    {
        const auto count = (std::min)(value.size(), bytes.size() - used);
        std::memcpy(bytes.data() + used, value.data(), count);
        used += count;
    }
    void Number(std::uint64_t value, int base = 10) noexcept
    {
        const auto converted = std::to_chars(bytes.data() + used, bytes.data() + bytes.size(), value, base);
        if (converted.ec == std::errc{})
            used = static_cast<std::size_t>(converted.ptr - bytes.data());
    }
    void Line(std::string_view key, std::uint64_t value, int base = 10) noexcept
    {
        Add(key);
        Number(value, base);
        Add("\r\n");
    }
};
} // namespace

HRESULT GetProcessInstanceKey(HANDLE process, std::span<wchar_t> destination) noexcept
{
    if (destination.size() < ProcessInstanceKeyCapacity)
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    const DWORD processId = GetProcessId(process);
    if (! processId)
        return LastError();
    FILETIME created{}, exited{}, kernel{}, user{};
    if (! GetProcessTimes(process, &created, &exited, &kernel, &user))
        return LastError();
    ULARGE_INTEGER creation{};
    creation.LowPart  = created.dwLowDateTime;
    creation.HighPart = created.dwHighDateTime;
    if (! creation.QuadPart)
        return E_UNEXPECTED;
    std::array<wchar_t, ProcessInstanceKeyCapacity> key{};
    if (_ultow_s(processId, key.data(), key.size(), 10) != 0)
        return E_FAIL;
    const auto separator = std::char_traits<wchar_t>::length(key.data());
    key[separator]       = L'-';
    if (_ui64tow_s(creation.QuadPart, key.data() + separator + 1u, key.size() - separator - 1u, 10) != 0)
        return E_FAIL;
    std::copy_n(key.data(), std::char_traits<wchar_t>::length(key.data()) + 1u, destination.data());
    return S_OK;
}

Host::~Host()
{
    const auto result = Uninstall();
    if (FAILED(result))
        DxUi::Failure::FailContract(result);
    if (stop_)
        SetEvent(stop_.get());
    if (thread_)
        WaitForSingleObject(thread_.get(), INFINITE);
}
HRESULT Host::StartBesideExecutable() noexcept
{
    std::array<wchar_t, 1024> path{};
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (! length || length >= path.size())
        return HRESULT_FROM_WIN32(length ? ERROR_INSUFFICIENT_BUFFER : GetLastError());
    auto separator = length;
    while (separator && path[separator - 1u] != L'\\' && path[separator - 1u] != L'/')
        --separator;
    if (! separator)
        return HRESULT_FROM_WIN32(ERROR_BAD_PATHNAME);
    constexpr std::wstring_view folder = L"FailureEvidence", suffix = L"\\failure";
    std::array<wchar_t, ProcessInstanceKeyCapacity> instance{};
    const HRESULT identity = GetProcessInstanceKey(GetCurrentProcess(), instance);
    if (FAILED(identity))
        return identity;
    const std::wstring_view key(instance.data());
    if (separator + folder.size() + 1u + key.size() + suffix.size() >= path.size())
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    std::copy(folder.begin(), folder.end(), path.begin() + separator);
    auto cursor  = separator + folder.size();
    path[cursor] = L'\0';
    if (! CreateDirectoryW(path.data(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return LastError();
    const auto attributes = GetFileAttributesW(path.data());
    if (attributes == INVALID_FILE_ATTRIBUTES || ! (attributes & FILE_ATTRIBUTE_DIRECTORY))
        return HRESULT_FROM_WIN32(ERROR_DIRECTORY);
    path[cursor++] = L'\\';
    std::copy(key.begin(), key.end(), path.data() + cursor);
    cursor += key.size();
    path[cursor] = L'\0';
    if (! CreateDirectoryW(path.data(), nullptr))
        return LastError();
    std::copy(suffix.begin(), suffix.end(), path.data() + cursor);
    cursor += suffix.size();
    const auto started = Start(std::wstring_view(path.data(), cursor));
    return FAILED(started) ? started : Install();
}
HRESULT Host::Start(std::wstring_view outputPrefix) noexcept
{
    if (thread_ || outputPrefix.empty() || outputPrefix.size() + 8u >= prefix_.size() || outputPrefix.find(L'\0') != std::wstring_view::npos)
        return E_INVALIDARG;
    // DbgCore loads PSAPI while enumerating the target's modules. Doing that
    // after a self-dump suspends target threads can wait on their loader work
    // forever. Prepare and retain this system dependency before hooks/workers;
    // the fatal path must not be its first load. Dumps remain best effort.
    dumpPsApi_.reset(LoadLibraryExW(L"psapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
    if (! dumpPsApi_)
        return LastError();
    std::copy(outputPrefix.begin(), outputPrefix.end(), prefix_.begin());
    prefix_[outputPrefix.size()] = L'\0';
    request_.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    if (! request_)
        return LastError();
    stop_.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (! stop_)
        return LastError();
    done_.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (! done_)
        return LastError();
    thread_.reset(CreateThread(nullptr, 0, WorkerEntry, this, 0, nullptr));
    if (! thread_)
        return LastError();
    return S_OK;
}
HRESULT Host::Install() noexcept
{
    if (! thread_)
        return E_UNEXPECTED;
    return hooks_.Install(Report, this);
}
HRESULT Host::Uninstall() noexcept
{
    return hooks_.Uninstall();
}
HRESULT Host::UninstallWhenIdle(DWORD timeoutMs, HRESULT (*cleanup)() noexcept) noexcept
{
    if (timeoutMs == INFINITE)
        return E_INVALIDARG;
    bool sawQuit            = false;
    int quitCode            = 0;
    const auto restoreQuit  = wil::scope_exit([&]() noexcept
    {
        if (sawQuit)
            PostQuitMessage(quitCode);
    });
    const ULONGLONG started = GetTickCount64();
    for (;;)
    {
        const HRESULT result = Uninstall();
        if (result != HRESULT_FROM_WIN32(ERROR_BUSY))
            return result;
        const ULONGLONG elapsed = GetTickCount64() - started;
        if (elapsed >= timeoutMs)
            return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
        if (cleanup)
        {
            const HRESULT cleaned = cleanup();
            if (FAILED(cleaned))
                return cleaned;
        }
        // Shutdown only: give COM/UIA a bounded opportunity to complete its
        // asynchronous releases. Never put a Failure::Scope around a message pump.
        const ULONGLONG afterCleanup = GetTickCount64() - started;
        if (afterCleanup >= timeoutMs)
            return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
        const DWORD wait   = static_cast<DWORD>((std::min)(ULONGLONG{50u}, static_cast<ULONGLONG>(timeoutMs) - afterCleanup));
        const DWORD waited = MsgWaitForMultipleObjectsEx(0u, nullptr, wait, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (waited == WAIT_FAILED)
            return LastError();
        if (waited == WAIT_OBJECT_0)
        {
            MSG message{};
            for (size_t count = 0u; count < 64u && PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE); ++count)
            {
                if (message.message == WM_QUIT)
                {
                    if (! sawQuit)
                        quitCode = static_cast<int>(message.wParam);
                    sawQuit = true;
                    continue;
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
    }
}
const DxUi::Failure::HostServices* Host::Services() const noexcept
{
    return hooks_.Services();
}
HRESULT Host::ReportRecord(const DxUi::Failure::Record& record) noexcept
{
    return Report(this, record);
}
DWORD WINAPI Host::WorkerEntry(void* context) noexcept
{
    return static_cast<Host*>(context)->Worker();
}
DWORD Host::Worker() noexcept
{
    const HANDLE handles[] = {stop_.get(), request_.get()};
    for (;;)
    {
        const auto wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (wait != WAIT_OBJECT_0 + 1u)
            return wait == WAIT_OBJECT_0 ? 0u : GetLastError();
        result_ = WriteEvidence();
        SetEvent(done_.get());
    }
}
HRESULT Host::Report(void* context, const DxUi::Failure::Record& record) noexcept
{
    auto& host    = *static_cast<Host*>(context);
    LONG expected = 0;
    if (! host.reporting_.compare_exchange_strong(expected, 1, std::memory_order_acq_rel))
        return HRESULT_FROM_WIN32(ERROR_BUSY);
    host.record_ = record;
    // The input chain may point at transient exception state. The root and its
    // machine context are copied; do not dereference an uncopied nested chain.
    host.record_.exception.ExceptionRecord = nullptr;
    if (! ResetEvent(host.done_.get()) || ! SetEvent(host.request_.get()))
        return LastError();
    const auto wait = WaitForSingleObject(host.done_.get(), ReportTimeoutMs);
    if (wait == WAIT_TIMEOUT)
        return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    if (wait != WAIT_OBJECT_0)
        return LastError();
    return host.result_;
}
HRESULT Host::WriteEvidence() noexcept
{
    std::array<wchar_t, 1032> path{};
    const auto length = std::char_traits<wchar_t>::length(prefix_.data());
    std::copy_n(prefix_.data(), length, path.data());
    std::copy_n(L".dmp", 5u, path.data() + length);
    wil::unique_hfile dump(CreateFileW(path.data(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    HRESULT dumpResult = dump ? S_OK : LastError();
    if (dump && ! TryAcquireSRWLockExclusive(&dbgHelpGate))
        dumpResult = HRESULT_FROM_WIN32(ERROR_BUSY);
    else if (dump)
    {
        const auto unlock = wil::scope_exit([] { ReleaseSRWLockExclusive(&dbgHelpGate); });
        EXCEPTION_POINTERS pointers{&record_.exception, &record_.context};
        MINIDUMP_EXCEPTION_INFORMATION info{record_.threadId, &pointers, FALSE};
        if (! MiniDumpWriteDump(GetCurrentProcess(),
                                GetCurrentProcessId(),
                                dump.get(),
                                static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules),
                                &info,
                                nullptr,
                                nullptr))
            dumpResult = LastError();
    }
    std::copy_n(L".record", 8u, path.data() + length);
    wil::unique_hfile output(CreateFileW(path.data(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (! output)
        return LastError();
    Text text;
    text.Line("protocol=", record_.version);
    text.Line("process=", GetCurrentProcessId());
    text.Line("thread=", record_.threadId);
    text.Line("kind=", static_cast<std::uint32_t>(record_.kind));
    text.Line("flags=", record_.flags, 16);
    text.Line("result=", static_cast<std::uint32_t>(record_.result), 16);
    text.Line("dump_result=", static_cast<std::uint32_t>(dumpResult), 16);
    text.Line("allocation_bytes=", record_.requestedBytes);
    text.Line("line=", record_.line);
    text.Add("file=");
    text.Add(std::string_view(record_.file, strnlen_s(record_.file, sizeof(record_.file))));
    text.Add("\r\n");
    text.Add("function=");
    text.Add(std::string_view(record_.function, strnlen_s(record_.function, sizeof(record_.function))));
    text.Add("\r\n");
    text.Line("exception_code=", record_.exception.ExceptionCode, 16);
    text.Line("exception_address=", reinterpret_cast<std::uintptr_t>(record_.exception.ExceptionAddress), 16);
    text.Line("context_flags=", record_.context.ContextFlags, 16);
#if defined(_M_X64)
    text.Line("context_ip=", record_.context.Rip, 16);
    text.Line("context_sp=", record_.context.Rsp, 16);
#elif defined(_M_ARM64)
    text.Line("context_ip=", record_.context.Pc, 16);
    text.Line("context_sp=", record_.context.Sp, 16);
#endif
    for (std::size_t index = 0; index < (std::min)(static_cast<std::size_t>(record_.frameCount), DxUi::Failure::MaximumFrames); ++index)
    {
        const auto address = reinterpret_cast<std::uintptr_t>(record_.frames[index]);
        text.Line("frame=", address, 16);
        HMODULE module{};
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(record_.frames[index]),
                               &module))
        {
            const auto base = reinterpret_cast<std::uintptr_t>(module);
            text.Line("module_base=", base, 16);
            text.Line("module_offset=", address - base, 16);
            std::array<wchar_t, 1024> modulePath{};
            const auto count = GetModuleFileNameW(module, modulePath.data(), static_cast<DWORD>(modulePath.size()));
            if (count && count < modulePath.size())
            {
                std::array<char, 4096> utf8{};
                const auto bytes =
                    WideCharToMultiByte(CP_UTF8, 0, modulePath.data(), static_cast<int>(count), utf8.data(), static_cast<int>(utf8.size()), nullptr, nullptr);
                if (bytes > 0)
                {
                    text.Add("module_path=");
                    text.Add({utf8.data(), static_cast<std::size_t>(bytes)});
                    text.Add("\r\n");
                }
            }
        }
    }
    DWORD written{};
    if (! WriteFile(output.get(), text.bytes.data(), static_cast<DWORD>(text.used), &written, nullptr))
        return LastError();
    if (written != text.used)
        return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
    return dumpResult;
}
} // namespace DxUiSample::FailureHost
