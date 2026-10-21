#pragma once

// The benchmark's instrument: the replaced global allocation functions and the counters through which the benchmarks read
// them. It is a benchmark input (Tools/PerformanceComparison.psm1), so a receipt's benchmarkSha256 covers it and a
// paired run copies it onto both trees, which then count with one instrument. Include it from exactly one translation
// unit, Embedded.Tests.Embedded.cpp: a replacement allocation function is defined once per program.

#include <cstddef>
#include <cstdlib>
#include <malloc.h>
#include <new>

#include <algorithm>
#include <limits>

// Counts library C++ heap calls within the deliberately isolated composition loop, not driver allocations. The bytes they ask
// for are counted beside them, for the opt-in measurement of Embedded.Tests.GridSelectionBenchmark.h. That measurement also asks what this
// thread's heap holds at a moment: while countLiveBytes is set, liveBytes is the sizes of the blocks this thread has allocated
// less those it has freed (the plain forms only, which every standard container uses), read as a difference between two points.
static thread_local bool countAllocations  = false;
static thread_local size_t allocations     = 0;
static thread_local size_t allocationBytes = 0;
static thread_local bool countLiveBytes    = false;
static thread_local ptrdiff_t liveBytes    = 0;
// Opt-in replacement-peak observation for geometry/parser resource fixtures. Dormant
// for canonical benchmarks and all earlier live-storage measurements.
static thread_local ptrdiff_t* liveBytesPeak = nullptr;
// Test-only fault injection is dormant outside an explicit operation sweep.
static thread_local size_t failAllocationAfter = std::numeric_limits<size_t>::max();
static void FailAllocationIfRequested()
{
    if (failAllocationAfter != std::numeric_limits<size_t>::max())
    {
        if (! failAllocationAfter)
            throw std::bad_alloc();
        --failAllocationAfter;
    }
}
void* operator new(size_t bytes)
{
    FailAllocationIfRequested();
    if (countAllocations)
    {
        ++allocations;
        allocationBytes += bytes;
    }
    if (auto* p = malloc(bytes ? bytes : 1))
    {
        if (countLiveBytes)
        {
            liveBytes += static_cast<ptrdiff_t>(_msize(p));
            if (liveBytesPeak)
                *liveBytesPeak = std::max(*liveBytesPeak, liveBytes);
        }
        return p;
    }
    throw std::bad_alloc();
}
void* operator new[](size_t bytes)
{
    return ::operator new(bytes);
}
static void FreeBlock(void* p) noexcept
{
    if (p && countLiveBytes)
        liveBytes -= static_cast<ptrdiff_t>(_msize(p));
    free(p);
}
void operator delete(void* p) noexcept
{
    FreeBlock(p);
}
void operator delete[](void* p) noexcept
{
    FreeBlock(p);
}
void operator delete(void* p, size_t) noexcept
{
    FreeBlock(p);
}
void operator delete[](void* p, size_t) noexcept
{
    FreeBlock(p);
}
void* operator new(size_t bytes, std::align_val_t alignment)
{
    FailAllocationIfRequested();
    if (countAllocations)
    {
        ++allocations;
        allocationBytes += bytes;
    }
    if (auto* p = _aligned_malloc(bytes ? bytes : 1, static_cast<size_t>(alignment)))
        return p;
    throw std::bad_alloc();
}
void* operator new[](size_t b, std::align_val_t a)
{
    return ::operator new(b, a);
}
void operator delete(void* p, std::align_val_t) noexcept
{
    _aligned_free(p);
}
void operator delete[](void* p, std::align_val_t) noexcept
{
    _aligned_free(p);
}
void operator delete(void* p, size_t, std::align_val_t) noexcept
{
    _aligned_free(p);
}
void operator delete[](void* p, size_t, std::align_val_t) noexcept
{
    _aligned_free(p);
}
