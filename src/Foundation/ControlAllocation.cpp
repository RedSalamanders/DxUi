#include "Support/ControlAllocation.h"
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
struct AllocationHeader
{
    void* storage;
    size_t requested;
    size_t reserved;
};
thread_local size_t failureCountdown = (std::numeric_limits<size_t>::max)();

[[nodiscard]] void* TryAllocate(size_t bytes, size_t alignment) noexcept
{
    if (failureCountdown != (std::numeric_limits<size_t>::max)())
    {
        if (! failureCountdown)
            return nullptr;
        --failureCountdown;
    }
    if (! std::has_single_bit(alignment))
        return nullptr;
    alignment = (std::max)(alignment, alignof(AllocationHeader));
    if (alignment > (std::numeric_limits<size_t>::max)() - sizeof(AllocationHeader) ||
        bytes > (std::numeric_limits<size_t>::max)() - sizeof(AllocationHeader) - alignment)
        return nullptr;
    const auto reserved = bytes + sizeof(AllocationHeader) + alignment - 1u;
    wil::unique_process_heap_ptr<std::byte> storage(static_cast<std::byte*>(HeapAlloc(GetProcessHeap(), 0, reserved)));
    if (! storage)
        return nullptr;
    const auto address    = (reinterpret_cast<uintptr_t>(storage.get()) + sizeof(AllocationHeader) + alignment - 1u) & ~(alignment - 1u);
    auto* const header    = reinterpret_cast<AllocationHeader*>(address - sizeof(AllocationHeader));
    const auto actualSize = HeapSize(GetProcessHeap(), 0, storage.get());
    const auto accounted  = actualSize == static_cast<size_t>(-1) ? reserved : actualSize;
    std::construct_at(header, AllocationHeader{storage.get(), bytes, accounted});
    if (const auto sink = Diagnostics::nativeAllocationSink)
        sink({true, bytes, accounted});
    storage.release();
    return reinterpret_cast<void*>(address);
}

void Free(void* block) noexcept
{
    if (! block)
        return;
    const auto& header = *reinterpret_cast<const AllocationHeader*>(reinterpret_cast<uintptr_t>(block) - sizeof(AllocationHeader));
    wil::unique_process_heap_ptr<std::byte> storage(static_cast<std::byte*>(header.storage));
    if (const auto sink = Diagnostics::nativeAllocationSink)
        sink({false, header.requested, header.reserved});
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
    if (auto* const result = TryAllocate(bytes, __STDCPP_DEFAULT_NEW_ALIGNMENT__))
        return result;
    Failure::FailAllocation(bytes);
}
void* Control::operator new(size_t bytes, const std::nothrow_t&) noexcept
{
    return TryAllocate(bytes, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
}
void* Control::operator new(size_t bytes, std::align_val_t alignment)
{
    if (auto* const result = TryAllocate(bytes, static_cast<size_t>(alignment)))
        return result;
    Failure::FailAllocation(bytes);
}
void* Control::operator new(size_t bytes, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return TryAllocate(bytes, static_cast<size_t>(alignment));
}
void Control::operator delete(void* block) noexcept
{
    Free(block);
}
void Control::operator delete(void* block, const std::nothrow_t&) noexcept
{
    Free(block);
}
void Control::operator delete(void* block, std::align_val_t) noexcept
{
    Free(block);
}
void Control::operator delete(void* block, std::align_val_t, const std::nothrow_t&) noexcept
{
    Free(block);
}
} // namespace DxUi
