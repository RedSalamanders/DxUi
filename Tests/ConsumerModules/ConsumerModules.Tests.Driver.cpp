#include "FailureHost.h"
#include "Support.Tests.ChildCrashCapture.h"
#include "Support.Tests.DumpReader.h"
#include "Support.Tests.FailureReports.h"
#include "Support/Failure.Tests.Hooks.h"
#include <objbase.h>

// UI Automation needs COM declarations even with lean Windows headers.
#include <UIAutomation.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <windows.h>

#include <DbgHelp.h>
#include <array>
#include <bit>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <initializer_list>
#include <limits>
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
bool HasModuleOrigin(HANDLE process, std::string_view record, std::string_view moduleName, std::string_view origin)
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
                            std::string_view(symbol->Name, symbol->NameLen).find(origin) != std::string_view::npos)
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
    DWORD childPid = 0, childExit = 0;
    const auto fail = [&](const char* stage, DWORD detail = 0u) noexcept
    {
        std::printf("DXUI_NATIVE_MODULE_FATAL_FAILURE scenario=%d stage=%s pid=%lu exit=%08lx detail=%08lx\n", kind, stage, childPid, childExit, detail);
        return false;
    };
    std::array<wchar_t, 32768> executable{};
    const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (! length || length >= executable.size())
        return fail("executable-path", GetLastError());
    const ULONGLONG started = GetTickCount64();
    const std::wstring eventName =
        L"Local\\DxUiFatalExit-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(started) + L"-" + std::to_wstring(kind);
    SetLastError(ERROR_SUCCESS);
    wil::unique_handle marker(CreateEventW(nullptr, TRUE, FALSE, eventName.c_str()));
    if (! marker || GetLastError() == ERROR_ALREADY_EXISTS)
        return fail("marker-create", GetLastError());
    std::wstring command = L"\"" + std::wstring(executable.data(), length) + L"\" --module-fatal=" + std::to_wstring(kind) + L" \"" + eventName + L"\"";
    wil::unique_handle job(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (! job || ! SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
        return fail("job-setup", GetLastError());
    std::wstring outputPrefix(executable.data(), length);
    const auto outputSlash = outputPrefix.find_last_of(L"\\/");
    if (outputSlash == std::wstring::npos)
        return fail("output-directory");
    outputPrefix.resize(outputSlash);
    const std::wstring executableDirectory = outputPrefix;
    std::array<wchar_t, DxUiSample::FailureHost::ProcessInstanceKeyCapacity> parentInstance{};
    const HRESULT parentIdentity = DxUiSample::FailureHost::GetProcessInstanceKey(GetCurrentProcess(), parentInstance);
    if (FAILED(parentIdentity))
        return fail("parent-instance", static_cast<DWORD>(parentIdentity));
    const std::wstring parentEvidence = executableDirectory + L"\\FailureEvidence\\" + parentInstance.data();
    outputPrefix                      = parentEvidence + L"\\child-" + std::to_wstring(kind) + L"-" + std::to_wstring(started);
    DxUiFailureTests::ChildOutputCapture output;
    const HRESULT captureResult = output.Initialize(outputPrefix + L".stdout", outputPrefix + L".stderr");
    if (FAILED(captureResult))
        return fail("output-setup", static_cast<DWORD>(captureResult));
    PROCESS_INFORMATION info{};
    if (! CreateProcessW(executable.data(),
                         command.data(),
                         nullptr,
                         nullptr,
                         TRUE,
                         CREATE_SUSPENDED | CREATE_NO_WINDOW | output.CreationFlags(),
                         nullptr,
                         nullptr,
                         &output.Startup()->StartupInfo,
                         &info))
        return fail("create-process", GetLastError());
    output.CloseParentHandles();
    childPid = info.dwProcessId;
    wil::unique_handle process(info.hProcess), thread(info.hThread);
    if (! AssignProcessToJobObject(job.get(), process.get()))
    {
        const DWORD error = GetLastError();
        TerminateProcess(process.get(), ERROR_CANCELLED);
        WaitForSingleObject(process.get(), 30000u);
        return fail("assign-job", error);
    }
    // Reproduce the retained PID-only directory that refused a real reused PID.
    // Seed before resume, retain its bytes, and require the new instance's dump elsewhere.
    constexpr std::string_view retainedText = "retained earlier process evidence\n";
    wil::unique_hfile retainedEvidence;
    if (kind == 7)
    {
        const std::wstring legacyDirectory = executableDirectory + L"\\FailureEvidence\\" + std::to_wstring(info.dwProcessId);
        if (! CreateDirectoryW(legacyDirectory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
            return fail("legacy-evidence-directory", GetLastError());
        const std::wstring retainedPath = legacyDirectory + L"\\retained-" + std::to_wstring(started) + L".record";
        retainedEvidence.reset(
            CreateFileW(retainedPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (! retainedEvidence)
            return fail("legacy-evidence-create", GetLastError());
        DWORD written{};
        if (! WriteFile(retainedEvidence.get(), retainedText.data(), static_cast<DWORD>(retainedText.size()), &written, nullptr))
            return fail("legacy-evidence-write", GetLastError());
        if (written != retainedText.size())
            return fail("legacy-evidence-write-size", written);
    }
    const ULONGLONG resumedAt = GetTickCount64();
    if (ResumeThread(thread.get()) == static_cast<DWORD>(-1))
        return fail("resume", GetLastError());
    const DWORD waited       = WaitForSingleObject(process.get(), 30000u);
    const DWORD waitError    = waited == WAIT_FAILED ? GetLastError() : ERROR_SUCCESS;
    const ULONGLONG waitedAt = GetTickCount64();
    const DWORD markerState  = WaitForSingleObject(marker.get(), 0u);
    std::printf("DXUI_NATIVE_MODULE_FATAL_TIMING scenario=%d pid=%lu start_ms=%llu setup_ms=%llu wait_ms=%llu elapsed_ms=%llu marker=%08lx wait=%08lx\n",
                kind,
                childPid,
                started,
                resumedAt - started,
                waitedAt - resumedAt,
                waitedAt - started,
                markerState,
                waited);
    if (waited != WAIT_OBJECT_0)
    {
        if (waited == WAIT_TIMEOUT)
        {
            const std::wstring path =
                parentEvidence + L"\\timeout-" + std::to_wstring(kind) + L"-" + std::to_wstring(childPid) + L"-" + std::to_wstring(waitedAt) + L".dmp";
            const HRESULT dumpResult = DxUiFailureTests::CaptureLiveHangDump(process.get(), childPid, path);
            std::printf("DXUI_NATIVE_MODULE_HANG_DUMP scenario=%d pid=%lu result=%08lx diagnostic_ms=%llu path=%ls\n",
                        kind,
                        childPid,
                        static_cast<unsigned long>(dumpResult),
                        GetTickCount64() - waitedAt,
                        path.c_str());
        }
        return fail("wait", waited == WAIT_FAILED ? waitError : waited);
    }
    if (! GetExitCodeProcess(process.get(), &childExit))
        return fail("exit-query", GetLastError());
    if (! (childExit & 0x80000000u))
        return fail("child-exit", childExit);
    if (markerState != WAIT_OBJECT_0)
        return fail("fatal-exit-marker", markerState);
    if (retainedEvidence)
    {
        LARGE_INTEGER beginning{};
        std::array<char, 64> retained{};
        DWORD read{};
        if (! SetFilePointerEx(retainedEvidence.get(), beginning, nullptr, FILE_BEGIN) ||
            ! ReadFile(retainedEvidence.get(), retained.data(), static_cast<DWORD>(retained.size()), &read, nullptr))
            return fail("legacy-evidence-preservation", GetLastError());
        if (std::string_view(retained.data(), read) != retainedText)
            return fail("legacy-evidence-content", read);
    }
    std::wstring directory(executable.data(), length);
    const auto slash = directory.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
        return fail("executable-directory");
    directory.resize(slash);
    const bool originInA  = kind <= 2 || (kind >= 5 && kind <= 6) || (kind >= 11 && kind <= 14);
    const auto modulePath = directory + (originInA ? L"\\NativeModuleA.dll" : L"\\NativeModuleB.dll");
    std::array<wchar_t, DxUiSample::FailureHost::ProcessInstanceKeyCapacity> childInstance{};
    const HRESULT childIdentity = DxUiSample::FailureHost::GetProcessInstanceKey(process.get(), childInstance);
    if (FAILED(childIdentity))
        return fail("child-instance", static_cast<DWORD>(childIdentity));
    directory += L"\\FailureEvidence\\" + std::wstring(childInstance.data()) + L"\\failure";
    wil::unique_hfile record(
        CreateFileW((directory + L".record").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (! record)
        return fail("record-open", GetLastError());
    std::array<char, 8192> text{};
    DWORD read{};
    if (! ReadFile(record.get(), text.data(), static_cast<DWORD>(text.size()), &read, nullptr) || ! read)
        return fail("record-read", GetLastError());
    const std::string_view content(text.data(), read);
    const auto moduleName = originInA ? "NativeModuleA.dll" : "NativeModuleB.dll";
    const auto origin     = kind <= 4 ? "DxUiModuleFatalWorker" : kind <= 12 ? "DxUiModuleReentrantOrigin" : "DxUiModuleEscapingNativeOrigin";
    std::uint64_t flags{};
    if (! ReadNumber(content, "flags=", 10, flags) || (flags & DxUi::Failure::FirstChanceContext) == 0 || (flags & DxUi::Failure::LiveFailureContext) != 0 ||
        content.find("exception_code=e06d7363") == std::string_view::npos || content.find(moduleName) == std::string_view::npos ||
        content.find("context_ip=") == std::string_view::npos || content.find("context_ip=0\r\n") != std::string_view::npos)
        return fail("record-content");
    std::uint64_t threadId{}, ip{};
    if (! ReadNumber(content, "thread=", 10, threadId) || ! ReadNumber(content, "context_ip=", 16, ip) || ! threadId || ! ip ||
        threadId > (std::numeric_limits<DWORD>::max)())
        return fail("record-context");
    AcquireSRWLockExclusive(&DxUiSample::FailureHost::dbgHelpGate);
    const auto unlock = wil::scope_exit([] { ReleaseSRWLockExclusive(&DxUiSample::FailureHost::dbgHelpGate); });
    // An ARM64 SEH filter runs on the handler path. Its live backtrace names
    // NativeExceptionFilter; the saved exception-context dump below must still
    // identify the original throw, independently of this capture-site check.
    const auto recordOrigin = kind <= 12 ? origin : "NativeExceptionFilter";
    if (! HasModuleOrigin(process.get(), content, moduleName, recordOrigin))
        return fail("record-symbol");
    wil::unique_hfile dump(CreateFileW((directory + L".dmp").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (! dump)
        return fail("dump-open", GetLastError());
    wil::unique_handle mapping(CreateFileMappingW(dump.get(), nullptr, PAGE_READONLY, 0, 0, nullptr));
    if (! mapping)
        return fail("dump-mapping", GetLastError());
    wil::unique_mapview_ptr<> view(MapViewOfFile(mapping.get(), FILE_MAP_READ, 0, 0, 0));
    if (! view)
        return fail("dump-view", GetLastError());
    PMINIDUMP_DIRECTORY streamDirectory{};
    void* stream{};
    ULONG size{};
    if (! MiniDumpReadDumpStream(view.get(), ExceptionStream, &streamDirectory, &stream, &size) || size < sizeof(MINIDUMP_EXCEPTION_STREAM))
        return fail("dump-stream", GetLastError());
    const auto& exception = *static_cast<const MINIDUMP_EXCEPTION_STREAM*>(stream);
    LARGE_INTEGER fileSize{};
    if (exception.ThreadId != threadId || exception.ThreadContext.DataSize < sizeof(CONTEXT) || ! GetFileSizeEx(dump.get(), &fileSize) ||
        fileSize.QuadPart < 0 || exception.ThreadContext.Rva > static_cast<std::uint64_t>(fileSize.QuadPart) ||
        exception.ThreadContext.DataSize > static_cast<std::uint64_t>(fileSize.QuadPart) - exception.ThreadContext.Rva)
        return fail("dump-context");
    CONTEXT context{};
    std::memcpy(&context, static_cast<const std::byte*>(view.get()) + exception.ThreadContext.Rva, sizeof(context));
#if defined(_M_X64)
    const bool contextMatches = context.Rip == ip;
#elif defined(_M_ARM64)
    const bool contextMatches = context.Pc == ip;
#else
    return fail("unsupported-architecture");
#endif
    if (! contextMatches)
        return fail("dump-context-mismatch");
    if (! DxUiFailureTests::DumpReader::Verify(directory + L".dmp", process.get(), static_cast<DWORD>(threadId), ip, modulePath, origin))
        return fail("dump-unwind-or-symbol");
    return true;
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    DxUiTestFailureReports::RouteAwayFromDialogs();
    wil::unique_handle fatalExitMarker;
    const auto clearMarker = wil::scope_exit([]() noexcept { DxUi::Failure::Detail::SetFatalExitEventForTest(nullptr); });
    if (argc == 3 && std::wstring_view(argv[1]).starts_with(L"--module-fatal="))
    {
        fatalExitMarker.reset(OpenEventW(EVENT_MODIFY_STATE, FALSE, argv[2]));
        if (! fatalExitMarker)
            return 16;
        DxUi::Failure::Detail::SetFatalExitEventForTest(fatalExitMarker.get());
    }
    DxUiSample::FailureHost::Host failureHost;
    const HRESULT hostStart = failureHost.StartBesideExecutable();
    if (FAILED(hostStart))
    {
        std::printf("DXUI_NATIVE_MODULE_HOST_FAILURE pid=%lu result=%08lx\n", GetCurrentProcessId(), static_cast<unsigned long>(hostStart));
        return 9;
    }
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
    bool firstRegistered   = true;
    const auto detachFirst = wil::scope_exit([&]() noexcept
    {
        if (firstRegistered)
            static_cast<void>(firstDetach());
    });
    if (FAILED(secondAttach(failureHost.Services())))
        return 14;
    bool secondRegistered   = true;
    const auto detachSecond = wil::scope_exit([&]() noexcept
    {
        if (secondRegistered)
            static_cast<void>(secondDetach());
    });
    if (failureHost.Uninstall() != HRESULT_FROM_WIN32(ERROR_BUSY))
        return 15;
    if (argc == 3 && std::wstring_view(argv[1]).starts_with(L"--module-fatal="))
    {
        wchar_t* end{};
        const long scenario = std::wcstol(argv[1] + 15, &end, 10);
        if (! end || *end != L'\0' || scenario < 1 || scenario > 16)
            return 16;
        const int kind = scenario % 2 ? 1 : 2;
        if (scenario <= 4)
        {
            using Fatal      = void (*)(int) noexcept;
            const auto fatal = std::bit_cast<Fatal>(GetProcAddress(scenario <= 2 ? first.get() : second.get(), "DxUiModuleFatal"));
            if (! fatal)
                return 16;
            fatal(kind);
        }
        else if (scenario <= 12)
        {
            using Invoke           = void (*)(int) noexcept;
            using Reenter          = void (*)(int, Invoke) noexcept;
            const bool outerInA    = scenario <= 6 || (scenario >= 9 && scenario <= 10);
            const bool crossModule = scenario >= 9;
            const auto outer       = outerInA ? first.get() : second.get();
            const auto inner       = outerInA ? second.get() : first.get();
            const auto reenter     = std::bit_cast<Reenter>(GetProcAddress(outer, "DxUiModuleNativeReentry"));
            const auto bridge      = crossModule ? std::bit_cast<Invoke>(GetProcAddress(inner, "DxUiModuleInvokeReentrant")) : nullptr;
            if (! reenter || (crossModule && ! bridge))
                return 16;
            reenter(kind, bridge);
        }
        else
        {
            using Escaping      = void (*)(int) noexcept;
            const auto escaping = std::bit_cast<Escaping>(GetProcAddress(scenario <= 14 ? first.get() : second.get(), "DxUiModuleEscapingNative"));
            if (! escaping)
                return 16;
            escaping(kind);
        }
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
    using NativeCaught      = HRESULT (*)() noexcept;
    const auto firstCaught  = std::bit_cast<NativeCaught>(GetProcAddress(first.get(), "DxUiModuleNativeCaught"));
    const auto secondCaught = std::bit_cast<NativeCaught>(GetProcAddress(second.get(), "DxUiModuleNativeCaught"));
    const int nativePassed  = (firstCaught && firstCaught() == E_ACCESSDENIED ? 1 : 0) + (secondCaught && secondCaught() == E_ACCESSDENIED ? 1 : 0);
    std::printf("DXUI_NATIVE_MODULE_NATIVE_ALLOWED executed=2 passed=%d\n", nativePassed);
    int fatalPassed = 0;
    for (int scenario = 1; scenario <= 16; ++scenario)
        if (FatalModuleChild(scenario))
            ++fatalPassed;
    std::printf("DXUI_NATIVE_MODULE_FATAL executed=16 passed=%d\n", fatalPassed);
    using PrepareEpilogue = int (*)(HANDLE, HANDLE) noexcept;
    using ClearEpilogue   = void (*)() noexcept;
    const auto epilogue   = [&](wil::unique_hmodule& module, Detach detach, bool& registered, const wchar_t* name)
    {
        wil::unique_handle entered(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        wil::unique_handle release(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (! entered || ! release)
            return false;
        const auto prepare = std::bit_cast<PrepareEpilogue>(GetProcAddress(module.get(), "DxUiModulePrepareNotificationEpilogue"));
        const auto clear   = std::bit_cast<ClearEpilogue>(GetProcAddress(module.get(), "DxUiModuleClearNotificationEpilogue"));
        if (! prepare || ! clear)
            return false;
        wil::unique_hmodule verificationPin;
        bool gateCleared       = false;
        const auto clearWorker = [&]() noexcept
        {
            if (gateCleared)
                return;
            SetEvent(release.get());
            if (module || verificationPin)
            {
                wil::unique_handle done(CreateEventW(nullptr, TRUE, FALSE, nullptr));
                if (! done)
                    DxUi::Failure::FailContract(HRESULT_FROM_WIN32(GetLastError()));
                std::jthread clearer([&]() noexcept
                {
                    clear();
                    SetEvent(done.get());
                });
                const DWORD wait = WaitForSingleObject(done.get(), 30000u);
                if (wait != WAIT_OBJECT_0)
                    DxUi::Failure::FailContract(wait == WAIT_FAILED ? HRESULT_FROM_WIN32(GetLastError()) : HRESULT_FROM_WIN32(ERROR_TIMEOUT));
                clearer.join();
            }
            gateCleared = true;
        };
        const auto releaseWorker = wil::scope_exit([&]() noexcept { clearWorker(); });
        int result               = -1;
        HRESULT cleanupResult    = E_UNEXPECTED;
        HRESULT detachResult     = E_UNEXPECTED;
        std::jthread ui([&]
        {
            const HRESULT apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            if (SUCCEEDED(apartment))
            {
                const auto uninitialize = wil::scope_exit([]() noexcept { CoUninitialize(); });
                result                  = prepare(entered.get(), release.get());
                if (result == 0)
                {
                    // Process owner retires UIA registrations on the owner STA,
                    // without wrapping its message pump in a failure scope.
                    cleanupResult = DxUi::Failure::InvokeNative(UiaDisconnectAllProviders);
                    if (SUCCEEDED(cleanupResult))
                    {
                        const ULONGLONG deadline = GetTickCount64() + 10000u;
                        do
                        {
                            detachResult = detach();
                            if (detachResult != HRESULT_FROM_WIN32(ERROR_BUSY))
                                break;
                            MSG message{};
                            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                            {
                                TranslateMessage(&message);
                                DispatchMessageW(&message);
                            }
                            MsgWaitForMultipleObjectsEx(0u, nullptr, 20u, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                        } while (GetTickCount64() < deadline);
                    }
                }
            }
        });
        ui.join();
        if (result != 0 || WaitForSingleObject(entered.get(), 0u) != WAIT_OBJECT_0)
        {
            std::printf("DXUI_NATIVE_MODULE_EPILOGUE_FAILURE module=%ls pid=%lu stage=fixture result=%d\n", name, GetCurrentProcessId(), result);
            std::fflush(stdout);
            DxUi::Failure::FailContract(E_FAIL);
        }
        if (FAILED(cleanupResult) || FAILED(detachResult))
        {
            std::printf("DXUI_NATIVE_MODULE_EPILOGUE_FAILURE module=%ls pid=%lu stage=cleanup-or-detach cleanup=%08lx detach=%08lx\n",
                        name,
                        GetCurrentProcessId(),
                        static_cast<unsigned long>(cleanupResult),
                        static_cast<unsigned long>(detachResult));
            std::fflush(stdout);
            DxUi::Failure::FailContract(FAILED(cleanupResult) ? cleanupResult : detachResult);
        }
        registered = false;
        module.reset();
        HMODULE retainedModule{};
        if (! GetModuleHandleExW(0u, name, &retainedModule))
        {
            std::printf("DXUI_NATIVE_MODULE_EPILOGUE_FAILURE module=%ls pid=%lu stage=unmapped-before-release error=%lu\n",
                        name,
                        GetCurrentProcessId(),
                        GetLastError());
            std::fflush(stdout);
            DxUi::Failure::FailContract(E_UNEXPECTED);
        }
        verificationPin.reset(retainedModule);
        if (WaitForSingleObject(entered.get(), 0u) != WAIT_OBJECT_0)
        {
            std::printf("DXUI_NATIVE_MODULE_EPILOGUE_FAILURE module=%ls pid=%lu stage=gate-expired\n", name, GetCurrentProcessId());
            std::fflush(stdout);
            DxUi::Failure::FailContract(HRESULT_FROM_WIN32(ERROR_TIMEOUT));
        }
        clearWorker();
        verificationPin.reset();
        const ULONGLONG deadline = GetTickCount64() + 10000u;
        while (GetModuleHandleW(name) && GetTickCount64() < deadline)
            Sleep(10u);
        if (GetModuleHandleW(name))
        {
            std::printf("DXUI_NATIVE_MODULE_EPILOGUE_FAILURE module=%ls pid=%lu stage=still-mapped\n", name, GetCurrentProcessId());
            std::fflush(stdout);
            DxUi::Failure::FailContract(E_UNEXPECTED);
        }
        return true;
    };
    int epiloguePassed = 0;
    epiloguePassed += epilogue(first, firstDetach, firstRegistered, L"NativeModuleA.dll") ? 1 : 0;
    epiloguePassed += epilogue(second, secondDetach, secondRegistered, L"NativeModuleB.dll") ? 1 : 0;
    std::printf("DXUI_NATIVE_MODULE_EPILOGUE executed=2 passed=%d\n", epiloguePassed);
    return executed == 12 && passed == executed && nativePassed == 2 && fatalPassed == 16 && epiloguePassed == 2 ? 0 : 12;
}
