#include "FailureHost.h"
#include <objbase.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <windows.h>

#include <DbgHelp.h>
#include <array>
#include <bit>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <string_view>
#include <thread>

extern "C" int DxUiModuleProbe() noexcept;

namespace
{
bool ReadNumber(std::string_view content, std::string_view key, int base, std::uint64_t& value)
{
    const auto position = content.find(key);
    if (position == std::string_view::npos || (position && content[position - 1u] != '\n'))
        return false;
    const auto begin = position + key.size();
    auto end         = content.find_first_of("\r\n", begin);
    if (end == std::string_view::npos)
        end = content.size();
    const auto result = std::from_chars(content.data() + begin, content.data() + end, value, base);
    return result.ec == std::errc{} && result.ptr == content.data() + end;
}
bool HasModuleOrigin(HANDLE process, std::string_view record, std::string_view moduleName)
{
    if (! SymInitializeW(process, nullptr, FALSE))
        return false;
    const auto cleanup = wil::scope_exit([&] { SymCleanup(process); });
    std::uint64_t frame{}, base{};
    bool hasFrame = false, hasBase = false;
    for (std::size_t offset = 0; offset < record.size();)
    {
        const auto end = record.find('\n', offset);
        auto line      = record.substr(offset, end == std::string_view::npos ? record.size() - offset : end - offset);
        if (! line.empty() && line.back() == '\r')
            line.remove_suffix(1u);
        if (line.starts_with("frame="))
        {
            hasFrame = ReadNumber(line, "frame=", 16, frame);
            hasBase  = false;
        }
        else if (line.starts_with("module_base="))
            hasBase = ReadNumber(line, "module_base=", 16, base);
        else if (line.starts_with("module_path=") && line.ends_with(moduleName) && hasFrame && hasBase)
        {
            const auto path  = line.substr(12u);
            const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), nullptr, 0);
            if (count > 0)
            {
                std::wstring wide(static_cast<std::size_t>(count), L'\0');
                if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), wide.data(), count))
                {
                    SetLastError(ERROR_SUCCESS);
                    const auto loaded = SymLoadModuleExW(process, nullptr, wide.c_str(), nullptr, base, 0, nullptr, 0);
                    IMAGEHLP_MODULEW64 info{};
                    info.SizeOfStruct = sizeof(info);
                    if ((loaded || GetLastError() == ERROR_SUCCESS) && SymGetModuleInfoW64(process, base, &info) && info.SymType == SymPdb &&
                        ! info.PdbUnmatched && info.LoadedPdbName[0])
                    {
                        alignas(SYMBOL_INFO) std::array<std::byte, sizeof(SYMBOL_INFO) + MAX_SYM_NAME> storage{};
                        auto* symbol         = reinterpret_cast<SYMBOL_INFO*>(storage.data());
                        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                        symbol->MaxNameLen   = MAX_SYM_NAME;
                        DWORD64 displacement{};
                        if (SymFromAddr(process, frame, &displacement, symbol) &&
                            std::string_view(symbol->Name, symbol->NameLen).find("DxUiModuleFatalWorker") != std::string_view::npos)
                            return true;
                    }
                }
            }
        }
        if (end == std::string_view::npos)
            break;
        offset = end + 1u;
    }
    return false;
}
bool FatalModuleChild(int kind)
{
    std::array<wchar_t, 32768> executable{};
    const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (! length || length >= executable.size())
        return false;
    std::wstring command = L"\"" + std::wstring(executable.data(), length) + L"\" --module-fatal=" + std::to_wstring(kind);
    wil::unique_handle job(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (! job || ! SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
        return false;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    if (! CreateProcessW(executable.data(), command.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info))
        return false;
    wil::unique_handle process(info.hProcess), thread(info.hThread);
    if (! AssignProcessToJobObject(job.get(), process.get()))
    {
        TerminateProcess(process.get(), ERROR_CANCELLED);
        WaitForSingleObject(process.get(), 30000u);
        return false;
    }
    if (ResumeThread(thread.get()) == static_cast<DWORD>(-1) || WaitForSingleObject(process.get(), 30000u) != WAIT_OBJECT_0)
        return false;
    DWORD code{};
    if (! GetExitCodeProcess(process.get(), &code) || ! (code & 0x80000000u))
        return false;
    std::wstring directory(executable.data(), length);
    const auto slash = directory.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
        return false;
    directory.resize(slash);
    directory += L"\\FailureEvidence\\" + std::to_wstring(info.dwProcessId) + L"\\failure";
    wil::unique_hfile record(
        CreateFileW((directory + L".record").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (! record)
        return false;
    std::array<char, 8192> text{};
    DWORD read{};
    if (! ReadFile(record.get(), text.data(), static_cast<DWORD>(text.size()), &read, nullptr) || ! read)
        return false;
    const std::string_view content(text.data(), read);
    const auto moduleName = kind <= 2 ? "NativeModuleA.dll" : "NativeModuleB.dll";
    std::uint64_t flags{};
    if (! ReadNumber(content, "flags=", 10, flags) || (flags & DxUi::Failure::FirstChanceContext) == 0 || (flags & DxUi::Failure::LiveFailureContext) != 0 ||
        content.find("exception_code=e06d7363") == std::string_view::npos || content.find(moduleName) == std::string_view::npos ||
        content.find("context_ip=") == std::string_view::npos || content.find("context_ip=0\r\n") != std::string_view::npos)
        return false;
    std::uint64_t threadId{}, ip{};
    if (! ReadNumber(content, "thread=", 10, threadId) || ! ReadNumber(content, "context_ip=", 16, ip) || ! threadId || ! ip ||
        ! HasModuleOrigin(process.get(), content, moduleName))
        return false;
    wil::unique_hfile dump(CreateFileW((directory + L".dmp").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (! dump)
        return false;
    wil::unique_handle mapping(CreateFileMappingW(dump.get(), nullptr, PAGE_READONLY, 0, 0, nullptr));
    if (! mapping)
        return false;
    wil::unique_mapview_ptr<> view(MapViewOfFile(mapping.get(), FILE_MAP_READ, 0, 0, 0));
    if (! view)
        return false;
    PMINIDUMP_DIRECTORY streamDirectory{};
    void* stream{};
    ULONG size{};
    if (! MiniDumpReadDumpStream(view.get(), ExceptionStream, &streamDirectory, &stream, &size) || size < sizeof(MINIDUMP_EXCEPTION_STREAM))
        return false;
    const auto& exception = *static_cast<const MINIDUMP_EXCEPTION_STREAM*>(stream);
    LARGE_INTEGER fileSize{};
    if (exception.ThreadId != threadId || exception.ThreadContext.DataSize < sizeof(CONTEXT) || ! GetFileSizeEx(dump.get(), &fileSize) ||
        fileSize.QuadPart < 0 || exception.ThreadContext.Rva > static_cast<std::uint64_t>(fileSize.QuadPart) ||
        exception.ThreadContext.DataSize > static_cast<std::uint64_t>(fileSize.QuadPart) - exception.ThreadContext.Rva)
        return false;
    CONTEXT context{};
    std::memcpy(&context, static_cast<const std::byte*>(view.get()) + exception.ThreadContext.Rva, sizeof(context));
#if defined(_M_X64)
    return context.Rip == ip;
#elif defined(_M_ARM64)
    return context.Pc == ip;
#else
    return false;
#endif
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    DxUiSample::FailureHost::Host failureHost;
    if (FAILED(failureHost.StartBesideExecutable()))
        return 9;
    wil::unique_hmodule first(LoadLibraryW(L"NativeModuleA.dll"));
    wil::unique_hmodule second(LoadLibraryW(L"NativeModuleB.dll"));
    if (! first || ! second)
        return 10;
    using Probe             = int (*)() noexcept;
    const auto firstProbe   = std::bit_cast<Probe>(GetProcAddress(first.get(), "DxUiModuleProbe"));
    const auto secondProbe  = std::bit_cast<Probe>(GetProcAddress(second.get(), "DxUiModuleProbe"));
    using Attach            = HRESULT (*)(const DxUi::Failure::HostServices*) noexcept;
    using Detach            = HRESULT (*)() noexcept;
    const auto firstAttach  = std::bit_cast<Attach>(GetProcAddress(first.get(), "DxUiModuleAttach"));
    const auto secondAttach = std::bit_cast<Attach>(GetProcAddress(second.get(), "DxUiModuleAttach"));
    const auto firstDetach  = std::bit_cast<Detach>(GetProcAddress(first.get(), "DxUiModuleDetach"));
    const auto secondDetach = std::bit_cast<Detach>(GetProcAddress(second.get(), "DxUiModuleDetach"));
    if (! firstProbe || ! secondProbe || ! firstAttach || ! secondAttach || ! firstDetach || ! secondDetach)
        return 11;
    if (FAILED(firstAttach(failureHost.Services())))
        return 13;
    const auto detachFirst = wil::scope_exit([&]() noexcept { static_cast<void>(firstDetach()); });
    if (FAILED(secondAttach(failureHost.Services())))
        return 14;
    const auto detachSecond = wil::scope_exit([&]() noexcept { static_cast<void>(secondDetach()); });
    if (failureHost.Uninstall() != HRESULT_FROM_WIN32(ERROR_BUSY))
        return 15;
    if (argc == 2 && std::wstring_view(argv[1]).starts_with(L"--module-fatal=") && argv[1][15] >= L'1' && argv[1][15] <= L'4' && argv[1][16] == L'\0')
    {
        using Fatal        = void (*)(int) noexcept;
        const int scenario = argv[1][15] - L'0';
        const auto fatal   = std::bit_cast<Fatal>(GetProcAddress(scenario <= 2 ? first.get() : second.get(), "DxUiModuleFatal"));
        if (! fatal)
            return 16;
        fatal(scenario % 2 ? 1 : 2);
        return 17;
    }
    int executed = 0;
    int passed   = 0;
    // Join before unloading: all module-owned UI-thread resources must finish before DLL teardown.
    for (int generation = 0; generation != 2; ++generation)
    {
        std::jthread ui([&]()
        {
            const auto com = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
            for (int round = 0; round != 2; ++round)
            {
                for (const auto probe : {DxUiModuleProbe, firstProbe, secondProbe})
                {
                    const int result = probe();
                    ++executed;
                    if (result == 0)
                        ++passed;
                }
            }
        });
        ui.join();
    }
    std::printf("DXUI_NATIVE_MODULE_PROBES executed=%d passed=%d\n", executed, passed);
    int fatalPassed = 0;
    for (int scenario = 1; scenario <= 4; ++scenario)
        if (FatalModuleChild(scenario))
            ++fatalPassed;
    std::printf("DXUI_NATIVE_MODULE_FATAL executed=4 passed=%d\n", fatalPassed);
    return executed == 12 && passed == executed && fatalPassed == 4 ? 0 : 12;
}
