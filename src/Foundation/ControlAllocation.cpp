#include "Support/ControlAllocation.h"
#include "Support/WorkCounters.h"
#include <DxUi/Diagnostics.h>
#include <DxUi/DxUi.h>
#include <DxUi/Failure.h>
#include <algorithm>
#include <bit>
#include <cstdint>
#include <exception>
#include <limits>
#include <utility>
#include <wil/resource.h>

namespace DxUi
{
namespace
{
// Default alignment: the 16 bytes immediately below the object. accounted is the heap's block size when an allocation
// sink was set at allocation and 0 when it was not (the block was never reported, so its free is not reported either).
struct Prefix
{
    size_t requested;
    size_t accounted;
};
// Over-aligned storage: a back-pointer to the heap block precedes the prefix fields, below an address rounded up inside
// the block.
struct AlignedHeader
{
    void* storage;
    size_t requested;
    size_t accounted;
};
// HeapAlloc blocks are aligned to MEMORY_ALLOCATION_ALIGNMENT (16 bytes on x64 and ARM64), so the object that follows a
// 16-byte prefix keeps the default new alignment without a back-pointer or slack.
static_assert(sizeof(Prefix) == 16u);
static_assert(MEMORY_ALLOCATION_ALIGNMENT % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == 0u);
static_assert(sizeof(Prefix) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == 0u);
static_assert(__STDCPP_DEFAULT_NEW_ALIGNMENT__ <= MEMORY_ALLOCATION_ALIGNMENT);
thread_local size_t failureCountdown = (std::numeric_limits<size_t>::max)();

[[nodiscard]] bool InjectedFailure() noexcept
{
    if (failureCountdown != (std::numeric_limits<size_t>::max)())
    {
        if (! failureCountdown)
            return true;
        --failureCountdown;
    }
    return false;
}

// The heap's size for a block: read only on behalf of an allocation sink.
[[nodiscard]] size_t ReadAccountedSize(void* storage, size_t reserved) noexcept
{
    ++Detail::Work().memory.controlHeapSizeReads;
    const auto actualSize = HeapSize(GetProcessHeap(), 0, storage);
    return actualSize == static_cast<size_t>(-1) ? reserved : actualSize;
}

[[nodiscard]] void* TryAllocateDefault(size_t bytes) noexcept
{
    if (InjectedFailure())
        return nullptr;
    if (bytes > (std::numeric_limits<size_t>::max)() - sizeof(Prefix))
        return nullptr;
    const auto reserved = bytes + sizeof(Prefix);
    wil::unique_process_heap_ptr<std::byte> storage(static_cast<std::byte*>(HeapAlloc(GetProcessHeap(), 0, reserved)));
    if (! storage)
        return nullptr;
#if defined(_DEBUG)
    if (reinterpret_cast<uintptr_t>(storage.get()) % __STDCPP_DEFAULT_NEW_ALIGNMENT__)
        __fastfail(FAST_FAIL_INVALID_ARG);
#endif
    const auto sink      = Diagnostics::nativeAllocationSink;
    const auto accounted = sink ? ReadAccountedSize(storage.get(), reserved) : size_t{0};
    std::construct_at(reinterpret_cast<Prefix*>(storage.get()), Prefix{bytes, accounted});
    if (sink)
        sink({true, bytes, accounted});
    return storage.release() + sizeof(Prefix);
}

void FreeDefault(void* block) noexcept
{
    if (! block)
        return;
    auto* const prefix = reinterpret_cast<Prefix*>(static_cast<std::byte*>(block) - sizeof(Prefix));
    const auto record  = *prefix;
    const wil::unique_process_heap_ptr<std::byte> storage(reinterpret_cast<std::byte*>(prefix));
    if (record.accounted)
        if (const auto sink = Diagnostics::nativeAllocationSink)
            sink({false, record.requested, record.accounted});
}

[[nodiscard]] void* TryAllocateAligned(size_t bytes, size_t alignment) noexcept
{
    if (InjectedFailure())
        return nullptr;
    if (! std::has_single_bit(alignment))
        return nullptr;
    alignment = (std::max)(alignment, alignof(AlignedHeader));
    if (alignment > (std::numeric_limits<size_t>::max)() - sizeof(AlignedHeader) ||
        bytes > (std::numeric_limits<size_t>::max)() - sizeof(AlignedHeader) - alignment)
        return nullptr;
    const auto reserved = bytes + sizeof(AlignedHeader) + alignment - 1u;
    wil::unique_process_heap_ptr<std::byte> storage(static_cast<std::byte*>(HeapAlloc(GetProcessHeap(), 0, reserved)));
    if (! storage)
        return nullptr;
    const auto address   = (reinterpret_cast<uintptr_t>(storage.get()) + sizeof(AlignedHeader) + alignment - 1u) & ~(alignment - 1u);
    auto* const header   = reinterpret_cast<AlignedHeader*>(address - sizeof(AlignedHeader));
    const auto sink      = Diagnostics::nativeAllocationSink;
    const auto accounted = sink ? ReadAccountedSize(storage.get(), reserved) : size_t{0};
    std::construct_at(header, AlignedHeader{storage.get(), bytes, accounted});
    if (sink)
        sink({true, bytes, accounted});
    storage.release();
    return reinterpret_cast<void*>(address);
}

void FreeAligned(void* block) noexcept
{
    if (! block)
        return;
    const auto& header = *reinterpret_cast<const AlignedHeader*>(reinterpret_cast<uintptr_t>(block) - sizeof(AlignedHeader));
    const auto record  = header;
    const wil::unique_process_heap_ptr<std::byte> storage(static_cast<std::byte*>(record.storage));
    if (record.accounted)
        if (const auto sink = Diagnostics::nativeAllocationSink)
            sink({false, record.requested, record.accounted});
}
} // namespace

namespace Detail
{
size_t SetControlAllocationFailureCountdown(size_t count) noexcept
{
    return std::exchange(failureCountdown, count);
}
} // namespace Detail

Control::Control() noexcept
try
{
    if (! Failure::IsConfigured())
        Failure::FailContract(CO_E_NOTINITIALIZED);
}
catch (const std::bad_alloc&)
{
    // Member construction has no result channel. The process observer retained
    // its original exception context; this must never become a partial control.
    Failure::FailAllocation();
}
catch (const std::exception&)
{
    Failure::FailUnexpected();
}

void* Control::operator new(size_t bytes)
{
    if (auto* const result = TryAllocateDefault(bytes))
        return result;
    Failure::FailAllocation(bytes);
}
void* Control::operator new(size_t bytes, const std::nothrow_t&) noexcept
{
    return TryAllocateDefault(bytes);
}
void* Control::operator new(size_t bytes, std::align_val_t alignment)
{
    if (auto* const result = TryAllocateAligned(bytes, static_cast<size_t>(alignment)))
        return result;
    Failure::FailAllocation(bytes);
}
void* Control::operator new(size_t bytes, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return TryAllocateAligned(bytes, static_cast<size_t>(alignment));
}
void Control::operator delete(void* block) noexcept
{
    FreeDefault(block);
}
void Control::operator delete(void* block, const std::nothrow_t&) noexcept
{
    FreeDefault(block);
}
void Control::operator delete(void* block, std::align_val_t) noexcept
{
    FreeAligned(block);
}
void Control::operator delete(void* block, std::align_val_t, const std::nothrow_t&) noexcept
{
    FreeAligned(block);
}
} // namespace DxUi
