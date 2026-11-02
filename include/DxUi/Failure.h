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
#include <source_location>
#include <type_traits>
#include <windows.h>

namespace DxUi::Failure
{
inline constexpr uint32_t ProtocolVersion = 1;
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

// No owning C++ state crosses the host/module boundary. Both modules must use
// the same protocol, architecture and headers. The record is borrowed only for
// the synchronous fatal/report call; an asynchronous dumper must copy it first.
struct Record
{
    uint32_t size         = sizeof(Record);
    uint32_t version      = ProtocolVersion;
    Kind kind             = Kind::UnexpectedException;
    uint32_t flags        = 0;
    HRESULT result        = E_FAIL;
    HRESULT dumpResult    = E_PENDING;
    size_t requestedBytes = 0;
    DWORD threadId        = 0;
    uint32_t line         = 0;
    uint32_t column       = 0;
    char file[192]{};
    char function[192]{};
    EXCEPTION_RECORD exception{};
    CONTEXT context{};
    uint32_t frameCount = 0;
    void* frames[MaximumFrames]{};
};

using Reporter = HRESULT (*)(void* context, const Record& record) noexcept;

// A host-owned immutable function table. Register it in every module linking
// DxUi, outside DllMain and before using the library. Unregister/quiesce those
// modules before removing the process owner. Callbacks never throw or allocate.
struct HostServices
{
    uint32_t size                                = sizeof(HostServices);
    uint32_t version                             = ProtocolVersion;
    void* context                                = nullptr;
    bool (*retain)(void*) noexcept               = nullptr;
    void (*release)(void*) noexcept              = nullptr;
    void (*enter)(void*, const Site&) noexcept   = nullptr;
    void (*leave)(void*) noexcept                = nullptr;
    void (*fatal)(void*, const Record&) noexcept = nullptr;
};
static_assert(std::is_trivially_copyable_v<Record> && std::is_standard_layout_v<Record>);
static_assert(std::is_trivially_copyable_v<HostServices> && std::is_standard_layout_v<HostServices>);

[[nodiscard]] HRESULT RegisterModule(const HostServices& services) noexcept;
[[nodiscard]] HRESULT UnregisterModule(const HostServices& services) noexcept;
[[nodiscard]] bool IsConfigured() noexcept;

class Scope final
{
public:
    explicit Scope(std::source_location location = std::source_location::current()) noexcept;
    ~Scope();
    Scope(const Scope&)            = delete;
    Scope& operator=(const Scope&) = delete;

private:
    const HostServices* _services = nullptr;
    bool _outer                   = false;
};

[[noreturn]] void FailAllocation(size_t requestedBytes = 0, std::source_location location = std::source_location::current()) noexcept;
[[noreturn]] void FailUnexpected(std::source_location location = std::source_location::current()) noexcept;
[[noreturn]] void FailContract(HRESULT result, std::source_location location = std::source_location::current()) noexcept;

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
