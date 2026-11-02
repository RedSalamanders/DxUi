#include "Support/Failure.Tests.Hooks.h"
#include <DxUi/Configuration.h>
#include <DxUi/Failure.h>
#include <atomic>
#include <cstring>
#include <exception>
#include <intrin.h>
#include <limits>
#include <new>
#include <utility>
#include <wil/resource.h>

namespace DxUi::Failure
{
namespace
{
constexpr uint64_t Closed          = uint64_t{1} << 63;
constexpr DWORD CppExceptionCode   = 0xe06d7363u;
constexpr DWORD FatalExceptionCode = 0xe0445855u;
std::atomic<const HostServices*> moduleServices{nullptr};
std::atomic<uint64_t> moduleGate{Closed};
std::atomic_flag moduleUpdate = ATOMIC_FLAG_INIT;
struct ModuleThread
{
    uint32_t depth               = 0;
    const HostServices* services = nullptr;
};
thread_local ModuleThread moduleThread;

void CopyText(char* destination, size_t capacity, const char* source) noexcept
{
    if (! capacity)
        return;
    size_t index = 0;
    if (source)
        while (index + 1 < capacity && source[index])
        {
            destination[index] = source[index];
            ++index;
        }
    destination[index] = '\0';
}

void* InstructionPointer(const CONTEXT& context) noexcept
{
#if defined(_M_ARM64)
    return reinterpret_cast<void*>(context.Pc);
#else
    return reinterpret_cast<void*>(context.Rip);
#endif
}

void Capture(Record& record, Kind kind, HRESULT result, size_t bytes, Site site, uint32_t flags) noexcept
{
    record.kind           = kind;
    record.flags          = flags;
    record.result         = result;
    record.requestedBytes = bytes;
    record.threadId       = GetCurrentThreadId();
    record.line           = site.line;
    record.column         = site.column;
    CopyText(record.file, sizeof(record.file), site.file);
    CopyText(record.function, sizeof(record.function), site.function);
    record.exception.ExceptionCode  = FatalExceptionCode;
    record.exception.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
    record.frameCount               = RtlCaptureStackBackTrace(0, static_cast<ULONG>(MaximumFrames), record.frames, nullptr);
}

[[noreturn]] void HardFail(const Record& record) noexcept
{
    EXCEPTION_RECORD exception = record.exception;
    CONTEXT context            = record.context;
    exception.ExceptionRecord  = nullptr;
    if (! exception.ExceptionCode)
        exception.ExceptionCode = FatalExceptionCode;
    exception.ExceptionFlags |= EXCEPTION_NONCONTINUABLE;
    if (! exception.ExceptionAddress)
        exception.ExceptionAddress = InstructionPointer(context);
    RaiseFailFastException(&exception, &context, 0);
    // No application continuation is possible even if an instrumented platform
    // unexpectedly returns from its documented non-returning fail-fast API.
    __fastfail(FAST_FAIL_FATAL_APP_EXIT);
}

Site Location(std::source_location location) noexcept
{
    return {location.file_name(), location.function_name(), location.line(), location.column()};
}

const HostServices* AcquireModule() noexcept
{
    auto gate = moduleGate.load(std::memory_order_acquire);
    for (;;)
    {
        if ((gate & Closed) || gate == Closed - 1)
            return nullptr;
        if (moduleGate.compare_exchange_weak(gate, gate + 1, std::memory_order_acquire, std::memory_order_relaxed))
            return moduleServices.load(std::memory_order_acquire);
    }
}

[[noreturn]] void Fail(Kind kind, HRESULT result, size_t bytes, Site site, uint32_t flags) noexcept
{
    Record record;
    Capture(record, kind, result, bytes, site, flags);
    // Capture in this non-returning frame, not in the metadata helper. Its
    // stack remains alive while the host's dedicated thread writes the dump.
    RtlCaptureContext(&record.context);
    record.flags |= LiveFailureContext;
    record.exception.ExceptionAddress = InstructionPointer(record.context);
    const auto* services              = moduleThread.services;
    if (! services)
        services = AcquireModule();
    if (services && services->fatal)
        services->fatal(services->context, record);
    // A missing or returning host table must never turn a fatal call into an
    // ordinary error. This fallback does not claim an application dump exists.
    HardFail(record);
}
} // namespace

namespace Detail
{
#if DXUI_ENABLE_DIAGNOSTICS
std::atomic<HANDLE> fatalContenderEvent{nullptr};
void SetFatalContenderEventForTest(HANDLE event) noexcept
{
    fatalContenderEvent.store(event, std::memory_order_release);
}
#endif
using VectoredRegistration = wil::unique_any<PVOID, decltype(&RemoveVectoredExceptionHandler), RemoveVectoredExceptionHandler>;
struct ProcessState
{
    ProcessState() noexcept                      = default;
    ProcessState(const ProcessState&)            = delete;
    ProcessState& operator=(const ProcessState&) = delete;
    ProcessState(ProcessState&&)                 = delete;
    ProcessState& operator=(ProcessState&&)      = delete;
    HostServices services;
    Reporter reporter     = nullptr;
    void* reporterContext = nullptr;
    VectoredRegistration observer;
    std::new_handler previousNew             = nullptr;
    std::terminate_handler previousTerminate = nullptr;
    std::atomic<uint64_t> registrations{0};
    std::atomic<uint32_t> fatalState{0};
    DWORD fatalThread = 0;
    wil::unique_handle reportComplete;
    DWORD installerThread = 0;
    Record firstFailure;
};

void ProcessStateDeleter::operator()(ProcessState* state) const noexcept
{
    if (! state)
        return;
    // The process-heap owner releases the original storage after C++ member
    // destruction; no owning raw Windows allocation escapes this deleter.
    wil::unique_process_heap_ptr<std::byte> memory(reinterpret_cast<std::byte*>(state));
    std::destroy_at(state);
}
} // namespace Detail

namespace
{
std::atomic<Detail::ProcessState*> processOwner{nullptr};
// A reader must acquire this gate before loading processOwner. Closing it
// prevents reclamation while a CRT callback has already obtained the owner.
std::atomic<uint64_t> processReaders{Closed};
struct ProcessReader
{
    Detail::ProcessState* state = nullptr;
    bool acquired               = false;
    ProcessReader() noexcept
    {
        auto gate = processReaders.load(std::memory_order_acquire);
        while (! (gate & Closed) && gate != Closed - 1)
        {
            if (processReaders.compare_exchange_weak(gate, gate + 1, std::memory_order_acquire, std::memory_order_relaxed))
            {
                acquired = true;
                state    = processOwner.load(std::memory_order_acquire);
                return;
            }
        }
    }
    ~ProcessReader()
    {
        if (acquired)
            processReaders.fetch_sub(1, std::memory_order_release);
    }
    ProcessReader(const ProcessReader&)            = delete;
    ProcessReader& operator=(const ProcessReader&) = delete;
};
struct ProcessThread
{
    uint32_t depth               = 0;
    uint32_t nativeBoundaryDepth = 0;
    NativeCallToken* nativeToken = nullptr;
    Site site;
    bool observed = false;
    Record lastException;
    std::terminate_handler previousTerminate = nullptr;
};
thread_local ProcessThread processThread;
[[noreturn]] void Terminated() noexcept;

// The CRT erases noexcept from its returned handler type even though termination
// handlers may never throw. Restoring that exact borrowed pointer invokes no
// callback. Scope the MSVC C-linkage diagnostic to this assignment alone.
// Owner: DxUi maintainers; review: 2027-01-31.
void RestoreTerminate(std::terminate_handler previous) noexcept
{
#pragma warning(push)
#pragma warning(disable : 5039)
    std::set_terminate(previous);
#pragma warning(pop)
}

[[noreturn]] void Dispatch(Detail::ProcessState& state, Record record) noexcept
{
    uint32_t expected = 0;
    if (! state.fatalState.compare_exchange_strong(expected, 1, std::memory_order_acq_rel))
    {
        const auto published = state.fatalState.load(std::memory_order_acquire);
        if (published == 2 && state.fatalThread == GetCurrentThreadId())
            HardFail(state.firstFailure);
#if DXUI_ENABLE_DIAGNOSTICS
        if (const auto event = Detail::fatalContenderEvent.load(std::memory_order_acquire))
            SetEvent(event);
#endif
        // The winner has at most 15 seconds in the supplied host reporter.
        // A contender waits without spinning so it cannot cut that dump short.
        static_cast<void>(WaitForSingleObject(state.reportComplete.get(), 16000u));
        if (state.fatalState.load(std::memory_order_acquire) == 2)
            HardFail(state.firstFailure);
        HardFail(record);
    }
    state.fatalThread  = GetCurrentThreadId();
    state.firstFailure = record;
    state.fatalState.store(2, std::memory_order_release);
    if (state.reporter)
        record.dumpResult = state.reporter(state.reporterContext, record);
    SetEvent(state.reportComplete.get());
    HardFail(record);
}

bool Retain(void* context) noexcept
{
    auto& state = *static_cast<Detail::ProcessState*>(context);
    auto count  = state.registrations.load(std::memory_order_acquire);
    for (;;)
    {
        if ((count & Closed) || count == Closed - 1)
            return false;
        if (state.registrations.compare_exchange_weak(count, count + 1, std::memory_order_acq_rel))
            return true;
    }
}

void Release(void* context) noexcept
{
    static_cast<Detail::ProcessState*>(context)->registrations.fetch_sub(1, std::memory_order_release);
}

void Enter(void*, const Site& site) noexcept
{
    if (processThread.depth == (std::numeric_limits<uint32_t>::max)())
        FailContract(E_UNEXPECTED);
    const auto previousDepth = processThread.depth++;
    if (previousDepth == processThread.nativeBoundaryDepth)
        processThread.site = site;
    if (! previousDepth)
    {
        processThread.observed = false;
        // MSVC maintains termination handlers per thread. Bind workers for the
        // duration of their outer host-owned scope, then restore their owner.
        processThread.previousTerminate = std::set_terminate(Terminated);
    }
}

void Leave(void*) noexcept
{
    if (! processThread.depth || processThread.depth <= processThread.nativeBoundaryDepth)
        FailContract(E_UNEXPECTED);
    if (! --processThread.depth)
    {
        processThread.site     = {};
        processThread.observed = false;
        if (std::get_terminate() == Terminated)
            RestoreTerminate(processThread.previousTerminate);
    }
}

void BeginNative(void*, NativeCallToken* token) noexcept
{
    if (! token || ! processThread.depth)
        FailContract(E_UNEXPECTED);
    token->previousSite               = processThread.site;
    token->previousBoundaryDepth      = processThread.nativeBoundaryDepth;
    token->enteredDepth               = processThread.depth;
    token->previous                   = processThread.nativeToken;
    processThread.nativeToken         = token;
    processThread.nativeBoundaryDepth = processThread.depth;
}

void EndNative(void*, NativeCallToken* token) noexcept
{
    if (! token || processThread.nativeToken != token || processThread.depth != token->enteredDepth || processThread.nativeBoundaryDepth != token->enteredDepth)
        FailContract(E_UNEXPECTED);
    processThread.site                = token->previousSite;
    processThread.nativeBoundaryDepth = token->previousBoundaryDepth;
    processThread.nativeToken         = token->previous;
}

[[noreturn]] void HostFatal(void* context, const Record& incoming) noexcept
{
    auto& state = *static_cast<Detail::ProcessState*>(context);
    if (incoming.size != sizeof(Record) || incoming.version != ProtocolVersion)
    {
        Record invalid;
        Capture(invalid, Kind::ContractViolation, E_INVALIDARG, 0, Location(std::source_location::current()), 0);
        RtlCaptureContext(&invalid.context);
        invalid.flags |= LiveFailureContext;
        invalid.exception.ExceptionAddress = InstructionPointer(invalid.context);
        Dispatch(state, invalid);
    }
    Record record = incoming;
    if ((incoming.flags & PreferObservedException) && processThread.observed)
    {
        record                = processThread.lastException;
        record.kind           = incoming.kind;
        record.result         = incoming.result;
        record.requestedBytes = incoming.requestedBytes;
        if (incoming.file[0])
        {
            std::memcpy(record.file, incoming.file, sizeof(record.file));
            std::memcpy(record.function, incoming.function, sizeof(record.function));
            record.line   = incoming.line;
            record.column = incoming.column;
        }
    }
    Dispatch(state, record);
}

LONG CALLBACK Observe(PEXCEPTION_POINTERS pointers) noexcept
{
    // Foreign caught exceptions are not candidates for a later DxUi failure.
    if (processThread.depth <= processThread.nativeBoundaryDepth || ! pointers || ! pointers->ExceptionRecord || ! pointers->ContextRecord ||
        pointers->ExceptionRecord->ExceptionCode != CppExceptionCode)
        return EXCEPTION_CONTINUE_SEARCH;
    ProcessReader owner;
    if (! owner.state)
        return EXCEPTION_CONTINUE_SEARCH;
    Record record;
    Capture(record, Kind::UnexpectedException, E_FAIL, 0, processThread.site, FirstChanceContext);
    record.context                   = *pointers->ContextRecord;
    record.exception                 = *pointers->ExceptionRecord;
    record.exception.ExceptionRecord = nullptr;
    processThread.lastException      = record;
    processThread.observed           = true;
    Dispatch(*owner.state, record);
}

[[noreturn]] void AllocationFailed() noexcept
{
    Record record;
    Capture(record, Kind::Allocation, E_OUTOFMEMORY, 0, processThread.site, 0);
    RtlCaptureContext(&record.context);
    record.flags |= LiveFailureContext;
    record.exception.ExceptionAddress = InstructionPointer(record.context);
    ProcessReader owner;
    if (owner.state)
        Dispatch(*owner.state, record);
    HardFail(record);
}

[[noreturn]] void Terminated() noexcept
{
    Record record;
    if (std::uncaught_exceptions() && processThread.observed)
        record = processThread.lastException;
    else
    {
        Capture(record, Kind::UnexpectedException, E_UNEXPECTED, 0, processThread.site, 0);
        RtlCaptureContext(&record.context);
        record.flags |= LiveFailureContext;
        record.exception.ExceptionAddress = InstructionPointer(record.context);
    }
    ProcessReader owner;
    if (owner.state)
        Dispatch(*owner.state, record);
    HardFail(record);
}
} // namespace

namespace Detail
{
int NativeExceptionFilter(EXCEPTION_POINTERS* pointers) noexcept
{
    if (! pointers || ! pointers->ExceptionRecord || ! pointers->ContextRecord || pointers->ExceptionRecord->ExceptionCode != CppExceptionCode)
        return EXCEPTION_CONTINUE_SEARCH;
    Record record;
    Capture(record, Kind::UnexpectedException, E_FAIL, 0, processThread.site, FirstChanceContext);
    record.context                   = *pointers->ContextRecord;
    record.exception                 = *pointers->ExceptionRecord;
    record.exception.ExceptionRecord = nullptr;
    // The trampoline may live in a separately linked module. Its active
    // Scope pins the registered host table even when this module has no
    // processOwner of its own.
    const auto* services = moduleThread.services;
    if (services && services->fatal)
        services->fatal(services->context, record);
    HardFail(record);
}
} // namespace Detail

HRESULT RegisterModule(const HostServices& services) noexcept
{
    if (services.size != sizeof(HostServices) || services.version != ProtocolVersion || ! services.context || ! services.retain || ! services.release ||
        ! services.enter || ! services.leave || ! services.beginNative || ! services.endNative || ! services.fatal)
        return E_INVALIDARG;
    if (moduleUpdate.test_and_set(std::memory_order_acquire))
        return HRESULT_FROM_WIN32(ERROR_BUSY);
    const auto clear    = wil::scope_exit([] { moduleUpdate.clear(std::memory_order_release); });
    const auto* current = moduleServices.load(std::memory_order_acquire);
    if (current)
        return HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
    if (moduleGate.load(std::memory_order_acquire) != Closed)
        return HRESULT_FROM_WIN32(ERROR_BUSY);
    if (! services.retain(services.context))
        return HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
    moduleServices.store(&services, std::memory_order_relaxed);
    moduleGate.store(0, std::memory_order_release);
    return S_OK;
}

HRESULT UnregisterModule(const HostServices& services) noexcept
{
    if (moduleUpdate.test_and_set(std::memory_order_acquire))
        return HRESULT_FROM_WIN32(ERROR_BUSY);
    const auto clear = wil::scope_exit([] { moduleUpdate.clear(std::memory_order_release); });
    if (moduleServices.load(std::memory_order_acquire) != &services)
        return E_INVALIDARG;
    uint64_t expectedGate = 0;
    if (! moduleGate.compare_exchange_strong(expectedGate, Closed, std::memory_order_acq_rel))
        return HRESULT_FROM_WIN32(ERROR_BUSY);
    moduleServices.store(nullptr, std::memory_order_release);
    services.release(services.context);
    return S_OK;
}

bool IsConfigured() noexcept
{
    return ! (moduleGate.load(std::memory_order_acquire) & Closed);
}

Scope::Scope(std::source_location location) noexcept : Scope(std::nothrow, location)
{
    if (! _services)
        Fail(Kind::MissingHost, CO_E_NOTINITIALIZED, 0, Location(location), 0);
}
Scope::Scope(std::nothrow_t, std::source_location location) noexcept
{
    if (moduleThread.depth == (std::numeric_limits<uint32_t>::max)())
        FailContract(E_UNEXPECTED);
    _outer    = moduleThread.depth == 0u;
    _services = _outer ? AcquireModule() : moduleThread.services;
    if (! _services)
        return;
    ++moduleThread.depth;
    if (_outer)
        moduleThread.services = _services;
    _services->enter(_services->context, Location(location));
}

Scope::~Scope()
{
    if (! _services)
        return;
    if (! moduleThread.depth)
        FailContract(E_UNEXPECTED);
    _services->leave(_services->context);
    --moduleThread.depth;
    if (_outer)
    {
        moduleThread.services = nullptr;
        moduleGate.fetch_sub(1, std::memory_order_release);
    }
}

NativeCallScope::NativeCallScope() noexcept
{
    _services = moduleThread.services;
    if (! moduleThread.depth || ! _services)
        FailContract(E_UNEXPECTED);
    _services->beginNative(_services->context, &_token);
}
NativeCallScope::~NativeCallScope()
{
    _services->endNative(_services->context, &_token);
}

ObjectLifetime::ObjectLifetime(std::source_location location) noexcept : _services(AcquireModule())
{
    if (! _services)
        Fail(Kind::MissingHost, CO_E_NOTINITIALIZED, 0, Location(location), 0);
}
ObjectLifetime::~ObjectLifetime()
{
    moduleGate.fetch_sub(1, std::memory_order_release);
}

[[noreturn]] void FailAllocation(size_t requestedBytes, std::source_location location) noexcept
{
    Fail(Kind::Allocation, E_OUTOFMEMORY, requestedBytes, Location(location), PreferObservedException);
}

[[noreturn]] void FailUnexpected(std::source_location location) noexcept
{
    Fail(Kind::UnexpectedException, E_FAIL, 0, Location(location), PreferObservedException);
}

[[noreturn]] void FailContract(HRESULT result, std::source_location location) noexcept
{
    Fail(Kind::ContractViolation, result, 0, Location(location), 0);
}

ProcessHooks::~ProcessHooks()
{
    if (FAILED(Uninstall()))
        FailContract(HRESULT_FROM_WIN32(ERROR_BUSY));
}

HRESULT ProcessHooks::Install(Reporter reporter, void* context) noexcept
{
    if (! reporter)
        return E_INVALIDARG;
    if (_state)
        return S_FALSE;
    wil::unique_process_heap_ptr<std::byte> storage(static_cast<std::byte*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Detail::ProcessState))));
    if (! storage)
        return E_OUTOFMEMORY;
    std::unique_ptr<Detail::ProcessState, Detail::ProcessStateDeleter> staged(std::construct_at(reinterpret_cast<Detail::ProcessState*>(storage.get())));
    storage.release();
    staged->reporter        = reporter;
    staged->reporterContext = context;
    staged->reportComplete.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (! staged->reportComplete)
        return HRESULT_FROM_WIN32(GetLastError());
    staged->installerThread        = GetCurrentThreadId();
    staged->services.context       = staged.get();
    staged->services.retain        = Retain;
    staged->services.release       = Release;
    staged->services.enter         = Enter;
    staged->services.leave         = Leave;
    staged->services.beginNative   = BeginNative;
    staged->services.endNative     = EndNative;
    staged->services.fatal         = HostFatal;
    Detail::ProcessState* expected = nullptr;
    if (! processOwner.compare_exchange_strong(expected, staged.get(), std::memory_order_acq_rel))
        return HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
    processReaders.store(0, std::memory_order_release);
    const auto rollback = wil::scope_exit([&]
    {
        if (! _state)
        {
            processReaders.store(Closed, std::memory_order_release);
            processOwner.store(nullptr, std::memory_order_release);
        }
    });
    staged->observer.reset(AddVectoredExceptionHandler(1, Observe));
    if (! staged->observer)
        return HRESULT_FROM_WIN32(GetLastError());
    const auto registered = RegisterModule(staged->services);
    if (FAILED(registered))
        return registered;
    staged->previousNew       = std::set_new_handler(AllocationFailed);
    staged->previousTerminate = std::set_terminate(Terminated);
    _state                    = std::move(staged);
    return S_OK;
}

HRESULT ProcessHooks::Uninstall() noexcept
{
    if (! _state)
        return S_FALSE;
    if (_state->installerThread != GetCurrentThreadId())
        return HRESULT_FROM_WIN32(ERROR_INVALID_THREAD_ID);
    const auto count = _state->registrations.fetch_or(Closed, std::memory_order_acq_rel);
    if (count != 1)
    {
        _state->registrations.fetch_and(~Closed, std::memory_order_release);
        return HRESULT_FROM_WIN32(ERROR_BUSY);
    }
    if (moduleUpdate.test_and_set(std::memory_order_acquire))
    {
        _state->registrations.fetch_and(~Closed, std::memory_order_release);
        return HRESULT_FROM_WIN32(ERROR_BUSY);
    }
    const auto clear      = wil::scope_exit([] { moduleUpdate.clear(std::memory_order_release); });
    uint64_t expectedGate = 0;
    if (moduleServices.load(std::memory_order_acquire) != &_state->services ||
        ! moduleGate.compare_exchange_strong(expectedGate, Closed, std::memory_order_acq_rel))
    {
        _state->registrations.fetch_and(~Closed, std::memory_order_release);
        return HRESULT_FROM_WIN32(ERROR_BUSY);
    }
    const auto readers = processReaders.fetch_or(Closed, std::memory_order_acq_rel);
    if (readers != 0)
    {
        processReaders.fetch_and(~Closed, std::memory_order_release);
        moduleGate.store(0, std::memory_order_release);
        _state->registrations.fetch_and(~Closed, std::memory_order_release);
        return HRESULT_FROM_WIN32(ERROR_BUSY);
    }
    moduleServices.store(nullptr, std::memory_order_release);
    _state->services.release(_state->services.context);
    if (std::get_new_handler() == AllocationFailed)
        std::set_new_handler(_state->previousNew);
    if (std::get_terminate() == Terminated)
        RestoreTerminate(_state->previousTerminate);
    _state->observer.reset();
    processOwner.store(nullptr, std::memory_order_release);
    _state.reset();
    return S_OK;
}

const HostServices* ProcessHooks::Services() const noexcept
{
    return _state ? &_state->services : nullptr;
}

ModuleRegistration::~ModuleRegistration()
{
    if (FAILED(Detach()))
        FailContract(HRESULT_FROM_WIN32(ERROR_BUSY));
}

HRESULT ModuleRegistration::Attach(const HostServices& services) noexcept
{
    if (_services)
        return _services == &services ? S_FALSE : HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
    const auto result = RegisterModule(services);
    if (SUCCEEDED(result))
        _services = &services;
    return result;
}

HRESULT ModuleRegistration::Detach() noexcept
{
    if (! _services)
        return S_FALSE;
    const auto result = UnregisterModule(*_services);
    if (SUCCEEDED(result))
        _services = nullptr;
    return result;
}
} // namespace DxUi::Failure
