#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <source_location>
#include <type_traits>
#include <windows.h>

namespace DxUi::Failure
{
inline constexpr uint32_t ProtocolVersion = 2;
inline constexpr size_t MaximumFrames     = 64;

enum class Kind : uint32_t
{
    Allocation,
    UnexpectedException,
    ContractViolation,
    MissingHost,
    RecursiveFailure
};

enum RecordFlags : uint32_t
{
    PreferObservedException = 1u,
    FirstChanceContext      = 2u,
    LiveFailureContext      = 4u
};

// Borrowed only during Enter. Source strings are compiler-owned literals in the
// calling module, whose registration and active scope keep that module alive.
struct Site
{
    const char* file     = nullptr;
    const char* function = nullptr;
    uint32_t line        = 0;
    uint32_t column      = 0;
};

// Host-owned state saved across one audited native ABI call. Tokens are used
// only synchronously on the entering thread and must be ended in LIFO order.
struct NativeCallToken
{
    Site previousSite;
    uint32_t previousBoundaryDepth = 0;
    uint32_t enteredDepth          = 0;
    NativeCallToken* previous      = nullptr;
};

// No owning C++ state crosses the host/module boundary. Both modules must use
// the same protocol, architecture and headers. The record is borrowed only for
// the synchronous fatal/report call; an asynchronous dumper must copy it first.
struct Record
{
    uint32_t size    = sizeof(Record);
    uint32_t version = ProtocolVersion;
    Kind kind        = Kind::UnexpectedException;
    uint32_t flags   = 0;
    // The Windows context follows the fixed protocol prefix at its natural
    // alignment on both supported architectures, without implicit ABI padding.
    CONTEXT context{};
    EXCEPTION_RECORD exception{};
    void* frames[MaximumFrames]{};
    size_t requestedBytes = 0;
    HRESULT result        = E_FAIL;
    HRESULT dumpResult    = E_PENDING;
    DWORD threadId        = 0;
    uint32_t line         = 0;
    uint32_t column       = 0;
    uint32_t frameCount   = 0;
    char file[192]{};
    char function[192]{};
    uint32_t reserved[2]{};
};

using Reporter = HRESULT (*)(void* context, const Record& record) noexcept;

// A host-owned immutable function table. Register it in every module linking
// DxUi, outside DllMain and before using the library. Unregister/quiesce those
// modules before removing the process owner. Callbacks never throw or allocate.
struct HostServices
{
    uint32_t size                                         = sizeof(HostServices);
    uint32_t version                                      = ProtocolVersion;
    void* context                                         = nullptr;
    bool (*retain)(void*) noexcept                        = nullptr;
    void (*release)(void*) noexcept                       = nullptr;
    void (*enter)(void*, const Site&) noexcept            = nullptr;
    void (*leave)(void*) noexcept                         = nullptr;
    void (*beginNative)(void*, NativeCallToken*) noexcept = nullptr;
    void (*endNative)(void*, NativeCallToken*) noexcept   = nullptr;
    void (*fatal)(void*, const Record&) noexcept          = nullptr;
};
static_assert(std::is_trivially_copyable_v<Record> && std::is_standard_layout_v<Record>);
static_assert(offsetof(Record, size) == 0u && offsetof(Record, version) == sizeof(uint32_t));
static_assert(offsetof(Record, context) == 16u && alignof(Record) == alignof(CONTEXT));
static_assert(std::is_trivially_copyable_v<HostServices> && std::is_standard_layout_v<HostServices>);

[[nodiscard]] HRESULT RegisterModule(const HostServices& services) noexcept;
[[nodiscard]] HRESULT UnregisterModule(const HostServices& services) noexcept;
[[nodiscard]] bool IsConfigured() noexcept;

class Scope final
{
public:
    explicit Scope(std::source_location location = std::source_location::current()) noexcept;
    // Inactive only after registration closes; callers may then perform only
    // allocation-free, callback-free teardown without entering DxUi work.
    explicit Scope(std::nothrow_t, std::source_location location = std::source_location::current()) noexcept;
    ~Scope();
    Scope(const Scope&)            = delete;
    Scope& operator=(const Scope&) = delete;
    [[nodiscard]] bool IsActive() const noexcept
    {
        return _services != nullptr;
    }

private:
    const HostServices* _services = nullptr;
    bool _outer                   = false;
};

[[noreturn]] void FailAllocation(size_t requestedBytes = 0, std::source_location location = std::source_location::current()) noexcept;
[[noreturn]] void FailUnexpected(std::source_location location = std::source_location::current()) noexcept;
[[noreturn]] void FailContract(HRESULT result, std::source_location location = std::source_location::current()) noexcept;

// Surround only a synchronous, audited native ABI call that handles its own
// exceptions. Requires a live Scope; nested DxUi scopes re-enable observation.
class NativeCallScope final
{
public:
    NativeCallScope() noexcept;
    ~NativeCallScope();
    NativeCallScope(const NativeCallScope&)            = delete;
    NativeCallScope& operator=(const NativeCallScope&) = delete;

private:
    const HostServices* _services = nullptr;
    NativeCallToken _token;
};

// Pins this module's registration while an object may own callback-capable
// state. Construct before other members and destroy after them; release may
// occur on any thread. This does not enter a thread-local failure Scope.
class ObjectLifetime final
{
public:
    explicit ObjectLifetime(std::source_location location = std::source_location::current()) noexcept;
    ~ObjectLifetime();
    ObjectLifetime(const ObjectLifetime&)            = delete;
    ObjectLifetime& operator=(const ObjectLifetime&) = delete;
    ObjectLifetime(ObjectLifetime&&)                 = delete;
    ObjectLifetime& operator=(ObjectLifetime&&)      = delete;

private:
    const HostServices* _services = nullptr;
};

namespace Detail
{
// Called only from an SEH filter while its exception pointers remain live.
int NativeExceptionFilter(EXCEPTION_POINTERS* pointers) noexcept;

// No C++ owner or nontrivial destructor may be introduced in this function:
// the filter must inspect an escaping native exception before stack unwinding.
template <typename Function, typename... Args>
__declspec(noinline) auto NativeInvokeTrampoline(Function function, Args... args) noexcept -> decltype(function(args...))
{
    using Result = decltype(function(args...));
    __try
    {
        if constexpr (std::is_void_v<Result>)
        {
            function(args...);
            return;
        }
        else
            return function(args...);
    }
    __except (NativeExceptionFilter(GetExceptionInformation()))
    {
        __assume(0);
    }
}
} // namespace Detail

// Arguments are evaluated before entry. Only pass an audited native function
// pointer and trivially copyable arguments/results; caught native exceptions
// remain native, while any escaping C++ exception is fatal before unwind.
// Source-location metadata may describe the boundary rather than the throw;
// the failure record retains the original exception and machine context.
template <typename Function, typename... Args> auto InvokeNative(Function function, Args... args) noexcept -> decltype(function(args...))
{
    using Result = decltype(function(args...));
    static_assert(std::is_pointer_v<Function> && std::is_function_v<std::remove_pointer_t<Function>>);
    static_assert(((std::is_trivially_copyable_v<Args> && std::is_trivially_destructible_v<Args>) && ...));
    static_assert(std::is_void_v<Result> || (std::is_trivially_copyable_v<Result> && std::is_trivially_destructible_v<Result>));
    const Scope scope;
    if (! function)
        FailContract(E_INVALIDARG);
    const NativeCallScope native;
    if constexpr (std::is_void_v<Result>)
        Detail::NativeInvokeTrampoline(function, args...);
    else
        return Detail::NativeInvokeTrampoline(function, args...);
}

namespace Detail
{
struct ProcessState;
struct ProcessStateDeleter
{
    void operator()(ProcessState* state) const noexcept;
};
} // namespace Detail

// Explicit application-owned installation. This installs a first-chance C++
// observer and CRT new/terminate handlers, preserving/restoring the previous
// handlers. The observer terminates unexpected throws while a registered DxUi
// scope is active, before unwinding. Native HRESULT refusals are unaffected.
// Install and uninstall on the same thread, before constructing controls and
// after stopping new entries, workers and registered DLLs. An active scope or
// callback makes Uninstall return BUSY; a wrong thread is rejected. Direct
// stack or make_unique construction must be enclosed in a host-owned Scope.
// Never install implicitly from a plugin.
class ProcessHooks final
{
public:
    ProcessHooks() noexcept = default;
    ~ProcessHooks();
    ProcessHooks(const ProcessHooks&)            = delete;
    ProcessHooks& operator=(const ProcessHooks&) = delete;
    [[nodiscard]] HRESULT Install(Reporter reporter, void* context = nullptr) noexcept;
    [[nodiscard]] HRESULT Uninstall() noexcept;
    [[nodiscard]] const HostServices* Services() const noexcept;

private:
    std::unique_ptr<Detail::ProcessState, Detail::ProcessStateDeleter> _state;
};

class ModuleRegistration final
{
public:
    ModuleRegistration() noexcept = default;
    ~ModuleRegistration();
    ModuleRegistration(const ModuleRegistration&)            = delete;
    ModuleRegistration& operator=(const ModuleRegistration&) = delete;
    [[nodiscard]] HRESULT Attach(const HostServices& services) noexcept;
    [[nodiscard]] HRESULT Detach() noexcept;

private:
    const HostServices* _services = nullptr;
};
} // namespace DxUi::Failure
