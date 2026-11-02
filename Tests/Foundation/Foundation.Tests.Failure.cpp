#include "Controls/DxUi.MarkdownMetadata.h"
#include "FailureHost.h"
#include "Support.Tests.DumpReader.h"
#include "Support.Tests.FatalChildren.h"
#include "Support.Tests.TestWindowActivationGuard.h"
#include "Support/ControlAllocation.h"
#include "Support/Failure.Tests.Hooks.h"
#include <DbgHelp.h>
#include <DxUi/DxUi.h>
#include <DxUi/Failure.h>
#include <DxUi/RichSemantic.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <wil/resource.h>

namespace DxUiFailureTests
{
namespace
{
std::wstring evidenceRoot;
std::atomic<unsigned> sequence{};
std::atomic<void*> allocationSink{};
constexpr DWORD TimeoutMs = 30000u;
std::array<FatalChildCase, 192u> additionalCases{};
std::size_t additionalCount{};
std::atomic<bool> registrationFailed{};
// Three process-shared LONGs: witness bits, a reporter HRESULT and the
// injecting thread ID, tying the allocation marker to the failure record.
// Installed before any child trigger; it remains mapped until process exit.
volatile LONG* childWitness{};
constexpr LONG WitnessInjectedAllocation = 1L;
constexpr LONG WitnessTriggerReached     = 2L;
constexpr LONG WitnessReporterReached    = 4L;
constexpr LONG WitnessReporterReturned   = 8L;
constexpr LONG WitnessTriggerReturned    = 16L;
constexpr DWORD WitnessBytes             = 3u * static_cast<DWORD>(sizeof(LONG));
constexpr DWORD ExpectedFatalExitCode    = 0xe0445855u;
void MarkWitness(LONG bits) noexcept
{
    if (childWitness)
        (void)InterlockedOr(&childWitness[0], bits);
}
const FatalChildCase* FindAdditional(std::wstring_view name) noexcept
{
    for (std::size_t index = 0u; index < additionalCount; ++index)
        if (name == additionalCases[index].name)
            return &additionalCases[index];
    return nullptr;
}
const FatalChildCase* FindAdditional(const char* name) noexcept
{
    for (std::size_t index = 0u; index < additionalCount; ++index)
    {
        const auto* wide   = additionalCases[index].name;
        const auto* narrow = name;
        while (*wide && *narrow && *wide == static_cast<unsigned char>(*narrow))
            ++wide, ++narrow;
        if (! *wide && ! *narrow)
            return &additionalCases[index];
    }
    return nullptr;
}

__declspec(noinline) void AllocationOrigin()
{
    DxUi::Failure::FailAllocation(1234u);
}
__declspec(noinline) void ControlAllocationOrigin()
{
    DxUi::Detail::ControlAllocationFailureScope failure(0u);
    MarkWitness(WitnessTriggerReached);
    const auto control = std::make_unique<DxUi::Separator>();
    allocationSink.store(control.get(), std::memory_order_relaxed);
}
std::atomic<unsigned> yamlAllocationOriginMarker{};
__declspec(noinline) void YamlAllocationOrigin()
{
    yamlAllocationOriginMarker.store(301u, std::memory_order_relaxed);
    DxUi::Failure::Detail::SetYamlNativeAllocationFailureForTest(0u);
    MarkWitness(WitnessTriggerReached);
    (void)DxUi::RichText::Detail::ValidateYamlMetadata(L"key: value\n");
    MarkWitness(WitnessTriggerReturned);
}
bool VerifyChildActivationGuard(DxUi::TestSupport::ScopedWindowActivationBlocker& blocker)
{
    const HWND foreground = GetForegroundWindow();
    wil::unique_hwnd window(
        CreateWindowExW(0, L"STATIC", L"DxUi fatal child guard", WS_OVERLAPPED, -32000, -32000, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr));
    if (! window || ! blocker.IsActive() || blocker.ProtectedTopLevelWindowCount() == 0u ||
        (GetWindowLongPtrW(window.get(), GWL_EXSTYLE) & WS_EX_NOACTIVATE) == 0)
        return false;
    (void)SetForegroundWindow(window.get());
    (void)SetFocus(window.get());
    return GetForegroundWindow() == foreground && GetFocus() != window.get();
}
__declspec(noinline) void ConcurrentPrimaryOrigin()
{
    DxUi::Failure::FailAllocation(777u);
}
[[noreturn]] __declspec(noinline) void UnknownOrigin()
{
    DxUi::Failure::Scope scope;
    throw 37;
}
__declspec(noinline) void BadAllocOrigin()
{
    DxUi::Failure::Scope scope;
    throw std::bad_alloc{};
}
__declspec(noinline) void StandardOrigin()
{
    DxUi::Failure::Scope scope;
    throw std::runtime_error("fatal test");
}
__declspec(noinline) void StaleCaughtOrigin()
{
    try
    {
        throw std::runtime_error("caught outside DxUi");
    }
    catch (const std::runtime_error&)
    {
    }
    DxUi::Failure::FailUnexpected();
}
__declspec(noinline) void NewHandlerOrigin()
{
    DxUi::Failure::Scope scope;
    const auto bytes = (std::numeric_limits<std::size_t>::max)() / 2u;
    auto* allocation = ::operator new(bytes);
    // Preserve an observable allocation in optimized probes without volatile
    // target semantics or using the pointer to access allocated storage.
    allocationSink.store(allocation, std::memory_order_relaxed);
    ::operator delete(allocation);
}
// Distinct observable literals keep these origin probes separate under /OPT:ICF
// and LTCG. This diagnostic marker does not synchronize application state.
std::atomic<unsigned> callbackOriginMarker{};
[[noreturn]] __declspec(noinline) void AccessibleAllocationCallbackOrigin()
{
    callbackOriginMarker.store(1u, std::memory_order_relaxed);
    throw std::bad_alloc{};
}
[[noreturn]] __declspec(noinline) void AccessibleRuntimeCallbackOrigin(const std::runtime_error& error)
{
    callbackOriginMarker.store(2u, std::memory_order_relaxed);
    throw error;
}
[[noreturn]] __declspec(noinline) void RichTextCallbackOrigin(const std::runtime_error& error)
{
    callbackOriginMarker.store(3u, std::memory_order_relaxed);
    throw error;
}
[[noreturn]] __declspec(noinline) void RichSelectionCallbackOrigin(const std::runtime_error& error)
{
    callbackOriginMarker.store(4u, std::memory_order_relaxed);
    throw error;
}
[[noreturn]] __declspec(noinline) void RichContextMenuCallbackOrigin(const std::runtime_error& error)
{
    callbackOriginMarker.store(5u, std::memory_order_relaxed);
    throw error;
}
[[noreturn]] __declspec(noinline) void RichPlanCallbackOrigin(const std::runtime_error& error)
{
    callbackOriginMarker.store(6u, std::memory_order_relaxed);
    throw error;
}
__declspec(noinline) void AccessibleInvokeOrigin(bool allocation)
{
    DxUi::WindowHost host;
    auto root    = std::make_unique<DxUi::Panel>();
    auto* button = root->AddChild<DxUi::Button>(L"Invoke");
    host.SetRoot(std::move(root));
    if (allocation)
        button->SetAccessibleInvokeResult([](DxUi::ControlHost&) -> HRESULT { AccessibleAllocationCallbackOrigin(); });
    else
        button->SetAccessibleInvokeResult([](DxUi::ControlHost&) -> HRESULT
        {
            const std::runtime_error error("child callback");
            AccessibleRuntimeCallbackOrigin(error);
        });
    static_cast<void>(button->InvokeAccessibleResult(host));
}
__declspec(noinline) void RichNotifyOrigin(bool selection)
{
    DxUi::RichTextEditor editor(L"ab");
    if (selection)
    {
        editor.SetOnSelectionChanged([]
        {
            const std::runtime_error error("child selection callback");
            RichSelectionCallbackOrigin(error);
        });
        static_cast<void>(editor.SetSelection({1u, 1u}));
    }
    else
    {
        editor.SetOnTextChanged([]
        {
            const std::runtime_error error("child text callback");
            RichTextCallbackOrigin(error);
        });
        static_cast<void>(editor.ReplaceSelection(L"x"));
    }
}
__declspec(noinline) void RichContextMenuOrigin()
{
    DxUi::WindowHost host;
    auto editor   = std::make_unique<DxUi::RichTextEditor>(L"word");
    auto* control = editor.get();
    host.SetRoot(std::move(editor));
    control->SetOnContextMenu([](POINT, bool)
    {
        const std::runtime_error error("child context menu callback");
        RichContextMenuCallbackOrigin(error);
    });
    static_cast<void>(control->OnContextMenu(host, true, D2D1::Point2F(0.0f, 0.0f)));
}
__declspec(noinline) void RichPlanNotifyOrigin()
{
    DxUi::RichTextEditor editor(L"abc");
    const auto snapshot = editor.Snapshot();
    const auto revision = editor.Revision();
    const std::array edits{DxUi::RichText::SourceEdit{{1u, 2u}, L"X"}};
    editor.SetOnTextChanged([]
    {
        const std::runtime_error error("child source-plan callback");
        RichPlanCallbackOrigin(error);
    });
    static_cast<void>(editor.ApplySourceEdits(snapshot, revision, edits, {2u, 2u}));
}
[[noreturn]] DWORD WINAPI WorkerOrigin(void*) noexcept
{
    UnknownOrigin();
}
struct LifecycleProbe
{
    DxUiSample::FailureHost::Host* host{};
    HANDLE entered{}, release{};
    HRESULT wrongThread{E_PENDING};
};
DWORD WINAPI LifecycleWorker(void* context) noexcept
{
    auto& probe = *static_cast<LifecycleProbe*>(context);
    DxUi::Failure::Scope scope;
    probe.wrongThread = probe.host->Uninstall();
    SetEvent(probe.entered);
    return WaitForSingleObject(probe.release, TimeoutMs) == WAIT_OBJECT_0 ? 0u : 1u;
}
HRESULT RecursiveReporter(void*, const DxUi::Failure::Record&) noexcept
{
    MarkWitness(WitnessReporterReached);
    DxUi::Failure::FailContract(E_FAIL);
}
HRESULT DumpFailureReporter(void* context, const DxUi::Failure::Record& record) noexcept
{
    MarkWitness(WitnessReporterReached);
    const HRESULT result = static_cast<DxUiSample::FailureHost::Host*>(context)->ReportRecord(record);
    if (childWitness)
        (void)InterlockedExchange(&childWitness[1], static_cast<LONG>(result));
    MarkWitness(WitnessReporterReturned);
    return result;
}
struct ConcurrentProbe
{
    DxUiSample::FailureHost::Host* writer{};
    HANDLE start{}, contender{};
};
DWORD WINAPI ConcurrentSecondary(void* context) noexcept
{
    const auto& probe = *static_cast<ConcurrentProbe*>(context);
    if (WaitForSingleObject(probe.start, TimeoutMs) != WAIT_OBJECT_0)
        return 1u;
    DxUi::Failure::FailAllocation(888u);
}
HRESULT ConcurrentReporter(void* context, const DxUi::Failure::Record& record) noexcept
{
    auto& probe = *static_cast<ConcurrentProbe*>(context);
    if (! SetEvent(probe.start) || WaitForSingleObject(probe.contender, 5000u) != WAIT_OBJECT_0)
        return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    return probe.writer->ReportRecord(record);
}
bool Retain(void*) noexcept
{
    return true;
}
void Release(void*) noexcept
{
}
void Enter(void*, const DxUi::Failure::Site&) noexcept
{
}
void Leave(void*) noexcept
{
}
void ReturningFatal(void*, const DxUi::Failure::Record&) noexcept
{
    MarkWitness(WitnessReporterReached | WitnessReporterReturned);
}

std::wstring Widen(const char* value)
{
    if (! value)
        return {};
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, nullptr, 0);
    if (count <= 1)
        return {};
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    if (! MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, result.data(), count))
        return {};
    result.pop_back();
    return result;
}
bool ParseNumber(std::string_view value, std::uint64_t& out, int base)
{
    const auto result = std::from_chars(value.data(), value.data() + value.size(), out, base);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size();
}
std::string_view Field(std::string_view text, std::string_view key)
{
    const auto at = text.find(key);
    if (at == std::string_view::npos || (at && text[at - 1u] != '\n'))
        return {};
    const auto first = at + key.size();
    const auto end   = text.find_first_of("\r\n", first);
    return text.substr(first, end == std::string_view::npos ? text.size() - first : end - first);
}
bool ReadRecord(const std::wstring& path, std::string& contents)
{
    wil::unique_hfile file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (! file)
        return false;
    LARGE_INTEGER size{};
    if (! GetFileSizeEx(file.get(), &size) || size.QuadPart <= 0 || size.QuadPart > 8192)
        return false;
    contents.resize(static_cast<std::size_t>(size.QuadPart));
    DWORD read{};
    return ReadFile(file.get(), contents.data(), static_cast<DWORD>(contents.size()), &read, nullptr) && read == contents.size();
}
bool CheckDump(const std::wstring& path, std::uint32_t thread, std::uint64_t contextIp)
{
    AcquireSRWLockExclusive(&DxUiSample::FailureHost::dbgHelpGate);
    const auto unlock = wil::scope_exit([] { ReleaseSRWLockExclusive(&DxUiSample::FailureHost::dbgHelpGate); });
    wil::unique_hfile file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (! file)
        return false;
    wil::unique_handle mapping(CreateFileMappingW(file.get(), nullptr, PAGE_READONLY, 0, 0, nullptr));
    if (! mapping)
        return false;
    wil::unique_mapview_ptr<void> view(MapViewOfFile(mapping.get(), FILE_MAP_READ, 0, 0, 0));
    if (! view)
        return false;
    PMINIDUMP_DIRECTORY directory{};
    void* stream{};
    ULONG size{};
    if (! MiniDumpReadDumpStream(view.get(), ExceptionStream, &directory, &stream, &size) || size < sizeof(MINIDUMP_EXCEPTION_STREAM))
        return false;
    const auto& exception = *static_cast<const MINIDUMP_EXCEPTION_STREAM*>(stream);
    if (exception.ThreadId != thread || exception.ThreadContext.DataSize < sizeof(CONTEXT))
        return false;
    LARGE_INTEGER total{};
    if (! GetFileSizeEx(file.get(), &total) || total.QuadPart < 0 || exception.ThreadContext.Rva > static_cast<std::uint64_t>(total.QuadPart) ||
        exception.ThreadContext.DataSize > static_cast<std::uint64_t>(total.QuadPart) - exception.ThreadContext.Rva)
        return false;
    CONTEXT context{};
    std::memcpy(&context, static_cast<const std::byte*>(view.get()) + exception.ThreadContext.Rva, sizeof(context));
#if defined(_M_X64)
    return context.Rip != 0 && context.Rip == contextIp;
#elif defined(_M_ARM64)
    return context.Pc != 0 && context.Pc == contextIp;
#else
    return false;
#endif
}
bool CheckEvidence(const std::wstring& prefix, std::wstring_view name)
{
    std::string record;
    if (! ReadRecord(prefix + L".record", record))
        return false;
    std::uint64_t thread{}, flags{}, ip{}, dumpResult{};
    if (! ParseNumber(Field(record, "thread="), thread, 10) || ! thread || ! ParseNumber(Field(record, "flags="), flags, 16) ||
        ! ParseNumber(Field(record, "context_ip="), ip, 16) || ! ip || ! ParseNumber(Field(record, "dump_result="), dumpResult, 16) || dumpResult != 0)
        return false;
    const auto* registered = FindAdditional(name);
    const bool firstChance = (! registered || registered->faultClass != FatalFaultClass::ContractViolation) &&
                             (name == L"unknown" || name == L"bad_alloc" || name == L"standard" || name == L"worker" ||
                              name.starts_with(L"accessible-invoke-") || name.starts_with(L"rich-") || name == L"context-menu-callback" || registered);
    if (firstChance && ! (flags & DxUi::Failure::FirstChanceContext))
        return false;
    if (registered && registered->faultClass == FatalFaultClass::ContractViolation)
    {
        std::uint64_t kind{}, result{};
        if (! ParseNumber(Field(record, "kind="), kind, 10) || kind != static_cast<std::uint64_t>(DxUi::Failure::Kind::ContractViolation) ||
            ! ParseNumber(Field(record, "result="), result, 16) || result != registered->expectedResult || (flags & DxUi::Failure::FirstChanceContext) != 0u)
            return false;
    }
    if (name == L"stale-caught" &&
        ((flags & DxUi::Failure::FirstChanceContext) || Field(record, "function=").find("StaleCaughtOrigin") == std::string_view::npos))
        return false;
    if (name == L"new_handler")
    {
        std::uint64_t kind{};
        if (! ParseNumber(Field(record, "kind="), kind, 10) || kind != static_cast<std::uint64_t>(DxUi::Failure::Kind::Allocation) ||
            (flags & DxUi::Failure::FirstChanceContext) != 0u)
            return false;
    }
    if (name == L"control-allocation")
    {
        std::uint64_t kind{}, requested{};
        if (! ParseNumber(Field(record, "kind="), kind, 10) || kind != static_cast<std::uint64_t>(DxUi::Failure::Kind::Allocation) ||
            ! ParseNumber(Field(record, "allocation_bytes="), requested, 10) || requested != sizeof(DxUi::Separator) ||
            (flags & DxUi::Failure::FirstChanceContext) != 0u)
            return false;
    }
    if (name == L"yaml-allocation")
    {
        std::uint64_t kind{}, requested{};
        if (! ParseNumber(Field(record, "kind="), kind, 10) || kind != static_cast<std::uint64_t>(DxUi::Failure::Kind::Allocation) ||
            ! ParseNumber(Field(record, "allocation_bytes="), requested, 10) || requested == 0u || (flags & DxUi::Failure::FirstChanceContext) != 0u)
            return false;
    }
    if (name == L"allocation" && Field(record, "function=").find("AllocationOrigin") == std::string_view::npos)
        return false;
    if (name == L"concurrent" &&
        (Field(record, "function=").find("ConcurrentPrimaryOrigin") == std::string_view::npos || Field(record, "allocation_bytes=") != "777"))
        return false;
    return CheckDump(prefix + L".dmp", static_cast<std::uint32_t>(thread), ip);
}
} // namespace

bool RegisterFatalChildCase(FatalChildCase entry) noexcept
{
    if (! entry.name || ! *entry.name || ! entry.expectedOriginSymbol || ! *entry.expectedOriginSymbol || ! entry.trigger ||
        additionalCount == additionalCases.size() || FindAdditional(entry.name))
    {
        registrationFailed.store(true, std::memory_order_relaxed);
        return false;
    }
    for (const auto* unit = entry.name; *unit; ++unit)
        if (! ((*unit >= L'a' && *unit <= L'z') || (*unit >= L'0' && *unit <= L'9') || *unit == L'-'))
        {
            registrationFailed.store(true, std::memory_order_relaxed);
            return false;
        }
    additionalCases[additionalCount++] = entry;
    return true;
}
void MarkInjectedAllocation() noexcept
{
    if (childWitness)
        (void)InterlockedExchange(&childWitness[2], static_cast<LONG>(GetCurrentThreadId()));
    MarkWitness(WitnessInjectedAllocation);
}
bool ConfigureFailureChildren(std::wstring_view outputDirectory)
{
    if (registrationFailed.load(std::memory_order_relaxed) || outputDirectory.empty() || outputDirectory.find(L'\0') != std::wstring_view::npos)
        return false;
    evidenceRoot.assign(outputDirectory);
    const auto attributes = GetFileAttributesW(evidenceRoot.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
int RunFailureChild(int argc, char** argv)
{
    const char* kind             = nullptr;
    const char* prefix           = nullptr;
    const char* parentActivation = nullptr;
    const char* witnessName      = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strncmp(argv[i], "--failure-child=", 16) == 0)
            kind = argv[i] + 16;
        else if (std::strncmp(argv[i], "--failure-prefix=", 17) == 0)
            prefix = argv[i] + 17;
        else if (std::strncmp(argv[i], "--failure-parent-activation=", 28) == 0)
            parentActivation = argv[i] + 28;
        else if (std::strncmp(argv[i], "--failure-witness=", 18) == 0)
            witnessName = argv[i] + 18;
    }
    if (! kind)
        return -1;
    if (registrationFailed.load(std::memory_order_relaxed))
        return 90;
    if (parentActivation && std::strcmp(parentActivation, "blocked") != 0 && std::strcmp(parentActivation, "unverified") != 0)
        return 89;
    // Child dispatch precedes the ordinary runner and cannot inherit its
    // thread-local CBT hook. Neither parent class proves an interactive lease.
    DxUi::TestSupport::ScopedWindowActivationBlocker activationBlocker;
    if (! activationBlocker.Start())
    {
        std::fputs("Fatal child refused: no-activation guard could not be installed.\n", stderr);
        return 89;
    }
    // Native callback probes use logical focus and a null-HWND IME session.
    // The guard remains active; real foreground assertions stay in their suite.
    const auto witnessPath = Widen(witnessName);
    if (witnessPath.empty())
        return 91;
    wil::unique_handle witnessMapping(OpenFileMappingW(FILE_MAP_WRITE, FALSE, witnessPath.c_str()));
    if (! witnessMapping)
        return 91;
    wil::unique_mapview_ptr<void> witnessView(MapViewOfFile(witnessMapping.get(), FILE_MAP_WRITE, 0, 0, WitnessBytes));
    if (! witnessView)
        return 91;
    childWitness    = static_cast<volatile LONG*>(witnessView.get());
    const auto path = Widen(prefix);
    if (path.empty())
        return 91;
    if (std::strcmp(kind, "missing-host") == 0)
    {
        MarkWitness(WitnessTriggerReached);
        const DxUi::Failure::Scope scope;
        return 96;
    }
    if (std::strcmp(kind, "returning") == 0)
    {
        int owner{};
        DxUi::Failure::HostServices services;
        services.context = &owner;
        services.retain  = Retain;
        services.release = Release;
        services.enter   = Enter;
        services.leave   = Leave;
        services.fatal   = ReturningFatal;
        if (FAILED(DxUi::Failure::RegisterModule(services)))
            return 92;
        MarkWitness(WitnessTriggerReached);
        AllocationOrigin();
    }
    if (std::strcmp(kind, "recursive") == 0)
    {
        DxUi::Failure::ProcessHooks hooks;
        if (FAILED(hooks.Install(RecursiveReporter, nullptr)))
            return 93;
        MarkWitness(WitnessTriggerReached);
        AllocationOrigin();
    }
    if (std::strcmp(kind, "concurrent") == 0)
    {
        DxUiSample::FailureHost::Host writer;
        wil::unique_handle start(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        wil::unique_handle contender(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (! start || ! contender || FAILED(writer.Start(path)))
            return 98;
        ConcurrentProbe probe{&writer, start.get(), contender.get()};
        DxUi::Failure::Detail::SetFatalContenderEventForTest(contender.get());
        wil::unique_handle worker(CreateThread(nullptr, 0, ConcurrentSecondary, &probe, 0, nullptr));
        if (! worker)
            return 99;
        DxUi::Failure::ProcessHooks hooks;
        if (FAILED(hooks.Install(ConcurrentReporter, &probe)))
            return 100;
        ConcurrentPrimaryOrigin();
    }
    if (std::strcmp(kind, "dump_failure") == 0)
    {
        DxUiSample::FailureHost::Host writer;
        DxUi::Failure::ProcessHooks hooks;
        if (FAILED(writer.Start(path)) || FAILED(hooks.Install(DumpFailureReporter, &writer)))
            return 94;
        MarkWitness(WitnessTriggerReached);
        AllocationOrigin();
    }
    DxUiSample::FailureHost::Host host;
    if (FAILED(host.Start(path)) || FAILED(host.Install()))
        return 94;
    if (std::strcmp(kind, "activation-guard") == 0)
    {
        if (! VerifyChildActivationGuard(activationBlocker))
            return 88;
        AllocationOrigin();
    }
    if (std::strcmp(kind, "allocation") == 0)
        AllocationOrigin();
    if (std::strcmp(kind, "control-allocation") == 0)
    {
        const DxUi::Failure::Scope scope;
        ControlAllocationOrigin();
    }
    if (std::strcmp(kind, "yaml-allocation") == 0)
    {
        const DxUi::Failure::Scope scope;
        YamlAllocationOrigin();
    }
    if (std::strcmp(kind, "unknown") == 0)
        UnknownOrigin();
    if (std::strcmp(kind, "bad_alloc") == 0)
        BadAllocOrigin();
    if (std::strcmp(kind, "standard") == 0)
        StandardOrigin();
    if (std::strcmp(kind, "stale-caught") == 0)
        StaleCaughtOrigin();
    if (std::strcmp(kind, "new_handler") == 0)
        NewHandlerOrigin();
    if (std::strcmp(kind, "accessible-invoke-runtime") == 0)
        AccessibleInvokeOrigin(false);
    if (std::strcmp(kind, "accessible-invoke-allocation") == 0)
        AccessibleInvokeOrigin(true);
    if (std::strcmp(kind, "rich-text-notify") == 0)
        RichNotifyOrigin(false);
    if (std::strcmp(kind, "rich-selection-notify") == 0)
        RichNotifyOrigin(true);
    if (std::strcmp(kind, "context-menu-callback") == 0)
        RichContextMenuOrigin();
    if (std::strcmp(kind, "rich-plan-notify") == 0)
        RichPlanNotifyOrigin();
    if (const auto* additional = FindAdditional(kind))
    {
        MarkWitness(WitnessTriggerReached);
        if (additional->scopeTrigger)
        {
            const DxUi::Failure::Scope triggerScope;
            additional->trigger();
        }
        else
            additional->trigger();
    }
    if (std::strcmp(kind, "worker") == 0)
    {
        wil::unique_handle thread(CreateThread(nullptr, 0, WorkerOrigin, nullptr, 0, nullptr));
        if (! thread || WaitForSingleObject(thread.get(), TimeoutMs) != WAIT_OBJECT_0)
            return 95;
    }
    return 96; // A fatal case must never return here.
}

int RunFailureChild(int argc, wchar_t** argv)
{
    std::vector<std::string> utf8;
    std::vector<char*> pointers;
    utf8.reserve(static_cast<std::size_t>(argc));
    pointers.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index)
    {
        const auto needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[index], -1, nullptr, 0, nullptr, nullptr);
        if (needed <= 0)
            return 97;
        std::string value(static_cast<std::size_t>(needed), '\0');
        if (! WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[index], -1, value.data(), needed, nullptr, nullptr))
            return 97;
        value.pop_back();
        utf8.push_back(std::move(value));
    }
    for (auto& value : utf8)
        pointers.push_back(value.data());
    return RunFailureChild(argc, pointers.data());
}

namespace
{
#if defined(__SANITIZE_ADDRESS__)
// ASan otherwise aborts on the impossible request before the real C++ new
// handler can run. Override only this child, retaining every parent option.
bool NewHandlerEnvironment(std::vector<wchar_t>& output)
{
    wchar_t* const environment = GetEnvironmentStringsW();
    if (! environment)
        return false;
    const auto release = wil::scope_exit([&] { FreeEnvironmentStringsW(environment); });
    std::vector<std::wstring> entries;
    bool found = false;
    for (const wchar_t* item = environment; *item; item += std::wcslen(item) + 1u)
    {
        std::wstring value(item);
        if (_wcsnicmp(value.c_str(), L"ASAN_OPTIONS=", 13u) == 0)
        {
            if (value.size() > 13u)
                value.push_back(L':');
            value += L"allocator_may_return_null=1";
            found = true;
        }
        entries.push_back(std::move(value));
    }
    if (! found)
        entries.emplace_back(L"ASAN_OPTIONS=allocator_may_return_null=1");
    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) { return _wcsicmp(left.c_str(), right.c_str()) < 0; });
    for (const auto& value : entries)
    {
        output.insert(output.end(), value.begin(), value.end());
        output.push_back(L'\0');
    }
    output.push_back(L'\0');
    return true;
}
#endif
bool CheckOriginSymbols(HANDLE child, const std::wstring& exe, std::string_view record, std::wstring_view name, const std::wstring& prefix)
{
    const auto* additional    = FindAdditional(name);
    std::string_view expected = additional                                             ? additional->expectedOriginSymbol
                                : name == L"context-menu-callback"                     ? "RichContextMenuCallbackOrigin"
                                : name == L"rich-plan-notify"                          ? "RichPlanCallbackOrigin"
                                : name == L"rich-text-notify"                          ? "RichTextCallbackOrigin"
                                : name == L"rich-selection-notify"                     ? "RichSelectionCallbackOrigin"
                                : name == L"accessible-invoke-allocation"              ? "AccessibleAllocationCallbackOrigin"
                                : name == L"accessible-invoke-runtime"                 ? "AccessibleRuntimeCallbackOrigin"
                                : name == L"allocation" || name == L"activation-guard" ? "AllocationOrigin"
                                : name == L"control-allocation"                        ? "ControlAllocationOrigin"
                                : name == L"yaml-allocation"                           ? "YamlAllocationOrigin"
                                : name == L"bad_alloc"                                 ? "BadAllocOrigin"
                                : name == L"standard"                                  ? "StandardOrigin"
                                : name == L"unknown" || name == L"worker"              ? "UnknownOrigin"
                                : name == L"new_handler"                               ? "NewHandlerOrigin"
                                : name == L"stale-caught"                              ? "StaleCaughtOrigin"
                                : name == L"concurrent"                                ? "ConcurrentPrimaryOrigin"
                                                                                       : "AccessibleInvokeOrigin";
    std::uint64_t thread{}, ip{};
    if (! ParseNumber(Field(record, "thread="), thread, 10) || ! ParseNumber(Field(record, "context_ip="), ip, 16))
        return false;
    AcquireSRWLockExclusive(&DxUiSample::FailureHost::dbgHelpGate);
    const auto unlock = wil::scope_exit([] { ReleaseSRWLockExclusive(&DxUiSample::FailureHost::dbgHelpGate); });
    if (! DumpReader::Verify(prefix + L".dmp", child, static_cast<DWORD>(thread), ip, exe, expected))
        return false;
    return ! additional || additional->faultClass != FatalFaultClass::InjectedAllocation ||
           DumpReader::Verify(prefix + L".dmp", child, static_cast<DWORD>(thread), ip, exe, "ThrowInjectedAllocation");
}
} // namespace

bool ExpectFatalChild(const wchar_t* caseName)
{
    if (evidenceRoot.empty() || ! caseName || ! *caseName)
        return false;
    for (const auto* c = caseName; *c; ++c)
        if (! ((*c >= L'a' && *c <= L'z') || (*c >= L'0' && *c <= L'9') || *c == L'_' || *c == L'-'))
            return false;
    std::array<wchar_t, 32768> executable{};
    const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (! length || length >= executable.size())
        return false;
    const std::wstring exe(executable.data(), length);
    const auto serial    = sequence.fetch_add(1u, std::memory_order_relaxed);
    const auto directory = evidenceRoot + L"\\" + std::to_wstring(GetCurrentProcessId()) + L"-" + caseName + L"-" + std::to_wstring(serial);
    if (! CreateDirectoryW(directory.c_str(), nullptr))
        return false;
    const auto witnessName = L"Local\\DxUiFatalWitness-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(serial);
    SetLastError(ERROR_SUCCESS);
    wil::unique_handle witnessMapping(CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, WitnessBytes, witnessName.c_str()));
    if (! witnessMapping || GetLastError() == ERROR_ALREADY_EXISTS)
        return false;
    wil::unique_mapview_ptr<void> witnessView(MapViewOfFile(witnessMapping.get(), FILE_MAP_ALL_ACCESS, 0, 0, WitnessBytes));
    if (! witnessView)
        return false;
    auto* const witness = static_cast<volatile LONG*>(witnessView.get());
    (void)InterlockedExchange(&witness[0], 0L);
    (void)InterlockedExchange(&witness[1], 0L);
    (void)InterlockedExchange(&witness[2], 0L);
    const bool failurePath = std::wstring_view(caseName) == L"dump_failure";
    const auto prefix      = directory + L"\\evidence";
    // Make only the dump destination fail; the sibling record remains writable
    // and reports the exact dump HRESULT for this deliberately failed write.
    if (failurePath && ! CreateDirectoryW((prefix + L".dmp").c_str(), nullptr))
        return false;
    const wchar_t* parentActivation = DxUi::TestSupport::ScopedWindowActivationBlocker::IsActiveForCurrentThread() ? L"blocked" : L"unverified";
    std::wstring command = L"\"" + exe + L"\" --failure-child=" + caseName + L" --failure-parent-activation=" + parentActivation + L" --failure-witness=" +
                           witnessName + L" --failure-prefix=\"" + prefix + L"\"";
    wil::unique_handle job(CreateJobObjectW(nullptr, nullptr));
    if (! job)
        return false;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (! SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
        return false;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION information{};
    DWORD creationFlags = CREATE_SUSPENDED | CREATE_NO_WINDOW;
    void* environment   = nullptr;
#if defined(__SANITIZE_ADDRESS__)
    std::vector<wchar_t> childEnvironment;
    if (std::wstring_view(caseName) == L"new_handler")
    {
        if (! NewHandlerEnvironment(childEnvironment))
            return false;
        creationFlags |= CREATE_UNICODE_ENVIRONMENT;
        environment = childEnvironment.data();
    }
#endif
    if (! CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, creationFlags, environment, nullptr, &startup, &information))
        return false;
    wil::unique_handle process(information.hProcess), thread(information.hThread);
    if (! AssignProcessToJobObject(job.get(), process.get()))
    {
        TerminateProcess(process.get(), ERROR_CANCELLED);
        WaitForSingleObject(process.get(), TimeoutMs);
        return false;
    }
    if (ResumeThread(thread.get()) == static_cast<DWORD>(-1))
    {
        job.reset();
        (void)WaitForSingleObject(process.get(), TimeoutMs);
        return false;
    }
    if (WaitForSingleObject(process.get(), TimeoutMs) != WAIT_OBJECT_0)
    {
        job.reset();
        (void)WaitForSingleObject(process.get(), TimeoutMs);
        return false;
    }
    DWORD exitCode{};
    if (! GetExitCodeProcess(process.get(), &exitCode))
        return false;
    if (exitCode == 89u)
    {
        std::fwprintf(stderr, L"Fatal child %ls was refused by the desktop activation guard or requires a verified foreground lease.\n", caseName);
        return false;
    }
    if (! (exitCode & 0x80000000u))
        return false;
    const std::wstring_view name(caseName);
    const auto bits = InterlockedCompareExchange(&witness[0], 0L, 0L);
    if (name == L"missing-host")
        return exitCode == ExpectedFatalExitCode && bits == WitnessTriggerReached &&
               GetFileAttributesW((prefix + L".record").c_str()) == INVALID_FILE_ATTRIBUTES &&
               GetFileAttributesW((prefix + L".dmp").c_str()) == INVALID_FILE_ATTRIBUTES;
    if ((name == L"control-allocation" || name == L"yaml-allocation") && (bits & WitnessTriggerReached) == 0L)
        return false;
    if (name == L"yaml-allocation" && (bits & WitnessTriggerReturned) != 0L)
        return false;
    if (name == L"returning" || name == L"recursive" || failurePath)
    {
        if (exitCode != ExpectedFatalExitCode || (bits & (WitnessTriggerReached | WitnessReporterReached)) != (WitnessTriggerReached | WitnessReporterReached))
            return false;
        if (name == L"returning" && (bits & WitnessReporterReturned) == 0L)
            return false;
        if (failurePath)
        {
            const HRESULT reportResult = static_cast<HRESULT>(InterlockedCompareExchange(&witness[1], 0L, 0L));
            std::fprintf(stderr, "Fatal dump failure reporter HRESULT: 0x%08lX\n", static_cast<unsigned long>(reportResult));
            std::string record;
            std::uint64_t dumpResult{};
            if ((bits & WitnessReporterReturned) == 0L || ! FAILED(reportResult) || ! ReadRecord(prefix + L".record", record) ||
                ! ParseNumber(Field(record, "dump_result="), dumpResult, 16) || dumpResult != static_cast<std::uint32_t>(reportResult))
                return false;
        }
        return true;
    }
    if (const auto* additional = FindAdditional(name);
        additional && additional->faultClass == FatalFaultClass::ContractViolation && exitCode != ExpectedFatalExitCode)
        return false;
    if (const auto* additional = FindAdditional(name))
        if ((bits & WitnessTriggerReached) == 0L || (additional->faultClass == FatalFaultClass::InjectedAllocation && (bits & WitnessInjectedAllocation) == 0L))
            return false;
    std::string record;
    if (! CheckEvidence(prefix, name) || ! ReadRecord(prefix + L".record", record))
        return false;
    if (const auto* additional = FindAdditional(name); additional && additional->faultClass == FatalFaultClass::InjectedAllocation)
    {
        std::uint64_t failureThread{};
        const auto injectedThread = static_cast<std::uint32_t>(InterlockedCompareExchange(&witness[2], 0L, 0L));
        if (! injectedThread || ! ParseNumber(Field(record, "thread="), failureThread, 10) || failureThread != injectedThread)
            return false;
    }
    return CheckOriginSymbols(process.get(), exe, record, name, prefix);
}

bool TestFailurePolicy()
{
    if (evidenceRoot.empty())
        return false;
    // This test is intentionally not run while another process owner is installed.
    DxUiSample::FailureHost::Host host;
    const auto prefix = evidenceRoot + L"\\healthy-" + std::to_wstring(GetCurrentProcessId());
    if (FAILED(host.Start(prefix)) || FAILED(host.Install()) || host.Install() != S_FALSE || ! host.Services())
        return false;
    const auto* services = host.Services();
    if (DxUi::Failure::RegisterModule(*services) != HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS))
        return false;
    {
        DxUi::Failure::Scope active;
        if (host.Uninstall() != HRESULT_FROM_WIN32(ERROR_BUSY))
            return false;
    }
    wil::unique_handle entered(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    wil::unique_handle release(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (! entered || ! release)
        return false;
    LifecycleProbe probe{&host, entered.get(), release.get()};
    wil::unique_handle worker(CreateThread(nullptr, 0, LifecycleWorker, &probe, 0, nullptr));
    if (! worker)
        return false;
    const auto finishWorker = wil::scope_exit([&]
    {
        SetEvent(release.get());
        WaitForSingleObject(worker.get(), TimeoutMs);
    });
    if (WaitForSingleObject(entered.get(), TimeoutMs) != WAIT_OBJECT_0 || probe.wrongThread != HRESULT_FROM_WIN32(ERROR_INVALID_THREAD_ID) ||
        host.Uninstall() != HRESULT_FROM_WIN32(ERROR_BUSY))
        return false;
    SetEvent(release.get());
    if (WaitForSingleObject(worker.get(), TimeoutMs) != WAIT_OBJECT_0)
        return false;
    if (FAILED(host.Uninstall()))
        return false;
    for (const auto* name : {L"allocation",
                             L"missing-host",
                             L"control-allocation",
                             L"yaml-allocation",
                             L"activation-guard",
                             L"unknown",
                             L"bad_alloc",
                             L"standard",
                             L"stale-caught",
                             L"new_handler",
                             L"worker",
                             L"concurrent",
                             L"returning",
                             L"recursive",
                             L"dump_failure"})
        if (! ExpectFatalChild(name))
            return false;
    return true;
}
} // namespace DxUiFailureTests
