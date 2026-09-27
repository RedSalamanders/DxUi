#pragma once

#include <array>
#include <cstdint>
#include <ostream>
#include <wil/resource.h>

namespace DxUiTestSupport
{
// Shared form of the grid branch's opt-in heap diagnostic. Never called by
// production code or timed rendering. Only one process-owned heap is locked at
// once; enumeration uses bounded stack storage and does no allocation/output.
template <typename Check> void WriteHeapDiagnostic(std::ostream& output, Check&& check)
{
    struct Sample
    {
        uint64_t busy = 0u, free = 0u, overhead = 0u, committed = 0u, uncommitted = 0u;
        DWORD error = ERROR_SUCCESS;
    };
    std::array<HANDLE, 128> heaps{};
    std::array<Sample, 128> samples{};
    const DWORD count = GetProcessHeaps(static_cast<DWORD>(heaps.size()), heaps.data());
    check(count > 0u && count <= heaps.size(), "diagnostic heap enumeration fits bounded storage");
    if (count == 0u || count > heaps.size())
        return;
    for (DWORD i = 0u; i < count; ++i)
    {
        auto& sample = samples[i];
        if (HeapLock(heaps[i]))
        {
            const auto unlock = wil::scope_exit([&] { HeapUnlock(heaps[i]); });
            PROCESS_HEAP_ENTRY entry{};
            while (HeapWalk(heaps[i], &entry))
            {
                if ((entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) != 0u)
                {
                    sample.busy += entry.cbData;
                    sample.overhead += entry.cbOverhead;
                }
                else if ((entry.wFlags & PROCESS_HEAP_REGION) != 0u)
                {
                    sample.committed += entry.Region.dwCommittedSize;
                    sample.uncommitted += entry.Region.dwUnCommittedSize;
                }
                else if ((entry.wFlags & PROCESS_HEAP_UNCOMMITTED_RANGE) == 0u)
                {
                    sample.free += entry.cbData;
                    sample.overhead += entry.cbOverhead;
                }
            }
            sample.error = GetLastError();
            if (sample.error == ERROR_NO_MORE_ITEMS)
                sample.error = ERROR_SUCCESS;
        }
        else
            sample.error = GetLastError();
    }
    output << ",\"heaps\":[";
    for (DWORD i = 0u; i < count; ++i)
    {
        if (i != 0u)
            output << ',';
        const auto& sample = samples[i];
        output << "{\"heap\":" << reinterpret_cast<uintptr_t>(heaps[i]) << ",\"busyBytes\":" << sample.busy << ",\"freeBytes\":" << sample.free
               << ",\"entryOverheadBytes\":" << sample.overhead << ",\"regionCommittedBytes\":" << sample.committed
               << ",\"regionUncommittedBytes\":" << sample.uncommitted << ",\"error\":" << sample.error << '}';
    }
    output << ']';
}
} // namespace DxUiTestSupport
