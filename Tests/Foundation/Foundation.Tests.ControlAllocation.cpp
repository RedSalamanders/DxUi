#include "Support/WorkCounters.h"
#include <DxUi/ControlCatalog.h>
#include <DxUi/Diagnostics.h>
#include <DxUi/Failure.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <new>
#include <vector>

// Control object storage (src/Foundation/ControlAllocation.cpp): the default-alignment path keeps a 16-byte prefix, the
// over-aligned path keeps its header, every delete frees what its matching new allocated, and the accounting sink sees each
// block it was installed for exactly once on each side. Specs/Core/Core_PerformanceAndResources.md, "Control object
// allocation".
#pragma warning(push)
#pragma warning(disable : 4324) // explicit padding is the subject of these tests
namespace
{
static_assert(__STDCPP_DEFAULT_NEW_ALIGNMENT__ == 16u, "the default-alignment path is specified for 16-byte blocks");

struct Event
{
    bool allocating;
    size_t requested;
    size_t reserved;
};
std::vector<Event>* observed = nullptr;
void Observe(const DxUi::Diagnostics::NativeAllocationRecord& record) noexcept
{
    if (observed)
        observed->push_back({record.allocating, record.requestedBytes, record.reservedBytes});
}

// sizeof is a multiple of 16, so the block size the heap reports has no rounding left over: reserved - requested is
// the per-allocation overhead.
class alignas(16) SixteenControl final : public DxUi::Control
{
public:
    SixteenControl()                                 = default;
    SixteenControl(const SixteenControl&)            = delete;
    SixteenControl& operator=(const SixteenControl&) = delete;
    SixteenControl(SixteenControl&&)                 = delete;
    SixteenControl& operator=(SixteenControl&&)      = delete;

private:
    void Paint(DxUi::ControlHost&) const override
    {
    }
    std::byte payload[64]{};
};
// A distinct size, so one request cannot be mistaken for another.
class alignas(16) LargerControl final : public DxUi::Control
{
public:
    LargerControl()                                = default;
    LargerControl(const LargerControl&)            = delete;
    LargerControl& operator=(const LargerControl&) = delete;
    LargerControl(LargerControl&&)                 = delete;
    LargerControl& operator=(LargerControl&&)      = delete;

private:
    void Paint(DxUi::ControlHost&) const override
    {
    }
    std::byte payload[256]{};
};
class alignas(64) OverAlignedControl final : public DxUi::Control
{
public:
    OverAlignedControl()                                     = default;
    OverAlignedControl(const OverAlignedControl&)            = delete;
    OverAlignedControl& operator=(const OverAlignedControl&) = delete;
    OverAlignedControl(OverAlignedControl&&)                 = delete;
    OverAlignedControl& operator=(OverAlignedControl&&)      = delete;

private:
    void Paint(DxUi::ControlHost&) const override
    {
    }
    std::byte payload[40]{};
};

class SinkScope final
{
public:
    explicit SinkScope(std::vector<Event>& events) noexcept : _previous(DxUi::Diagnostics::nativeAllocationSink), _previousEvents(observed)
    {
        observed                                = &events;
        DxUi::Diagnostics::nativeAllocationSink = Observe;
    }
    ~SinkScope()
    {
        DxUi::Diagnostics::nativeAllocationSink = _previous;
        observed                                = _previousEvents;
    }
    SinkScope(const SinkScope&)            = delete;
    SinkScope& operator=(const SinkScope&) = delete;

private:
    DxUi::Diagnostics::NativeAllocationSink _previous;
    std::vector<Event>* _previousEvents;
};
// Runs with no sink installed, whatever the process had.
class NoSinkScope final
{
public:
    NoSinkScope() noexcept : _previous(DxUi::Diagnostics::nativeAllocationSink)
    {
        DxUi::Diagnostics::nativeAllocationSink = nullptr;
    }
    ~NoSinkScope()
    {
        DxUi::Diagnostics::nativeAllocationSink = _previous;
    }
    NoSinkScope(const NoSinkScope&)            = delete;
    NoSinkScope& operator=(const NoSinkScope&) = delete;

private:
    DxUi::Diagnostics::NativeAllocationSink _previous;
};

bool Fail(const char* what)
{
    std::fprintf(stderr, "control allocation: %s\n", what);
    return false;
}
[[nodiscard]] bool IsAligned(const void* pointer, size_t alignment) noexcept
{
    return reinterpret_cast<uintptr_t>(pointer) % alignment == 0u;
}
// Live bytes and live blocks the events leave: allocations add, frees subtract.
struct Totals
{
    long long bytes     = 0;
    long long blocks    = 0;
    long long requested = 0;
};
Totals Sum(const std::vector<Event>& events) noexcept
{
    Totals totals;
    for (const auto& event : events)
    {
        const long long sign = event.allocating ? 1 : -1;
        totals.bytes += sign * static_cast<long long>(event.reserved);
        totals.blocks += sign;
        totals.requested += sign * static_cast<long long>(event.requested);
    }
    return totals;
}

bool DefaultPathOverheadIsSixteenBytes()
{
    std::vector<Event> events;
    events.reserve(16);
    {
        const SinkScope sink(events);
        const DxUi::Failure::Scope construction;
        auto* const first  = new SixteenControl;
        auto* const larger = new LargerControl;
        delete first;
        delete larger;
    }
    if (events.size() != 4u || ! events[0].allocating || events[2].allocating)
        return Fail("overhead: expected two allocations and two frees");
    if (events[0].requested != sizeof(SixteenControl) || events[0].reserved - events[0].requested != 16u || events[1].requested != sizeof(LargerControl) ||
        events[1].reserved - events[1].requested != 16u)
        return Fail("overhead: the default path must account its request plus exactly 16 bytes");
    if (events[2].requested != sizeof(SixteenControl) || events[2].reserved != events[0].reserved || events[3].requested != sizeof(LargerControl) ||
        events[3].reserved != events[1].reserved)
        return Fail("overhead: each free must report what its allocation reported");
    const auto totals = Sum(events);
    return totals.bytes == 0 && totals.blocks == 0 && totals.requested == 0 ? true : Fail("overhead: totals do not return to zero");
}

bool AlignmentOfReturnedPointers()
{
    const DxUi::Failure::Scope construction;
    {
        const std::unique_ptr<SixteenControl> control(new SixteenControl);
        const std::unique_ptr<LargerControl> larger(new LargerControl);
        const std::unique_ptr<SixteenControl> nothrow(new (std::nothrow) SixteenControl);
        if (! IsAligned(control.get(), __STDCPP_DEFAULT_NEW_ALIGNMENT__) || ! IsAligned(larger.get(), __STDCPP_DEFAULT_NEW_ALIGNMENT__) || ! nothrow ||
            ! IsAligned(nothrow.get(), __STDCPP_DEFAULT_NEW_ALIGNMENT__))
            return Fail("alignment: a default-alignment control is not 16-byte aligned");
    }
    // Every kind the factory builds, of every size the library has.
    for (const auto& descriptor : DxUi::GetControlCatalog())
    {
        std::unique_ptr<DxUi::Control> created;
        if (FAILED(DxUi::CreateControl(descriptor.kind, created)) || ! created)
            return Fail("alignment: CreateControl failed");
        if (! IsAligned(created.get(), __STDCPP_DEFAULT_NEW_ALIGNMENT__))
            return Fail("alignment: a library control is not 16-byte aligned");
    }
    // Many allocations in a row, so heap block placement varies.
    std::vector<std::unique_ptr<SixteenControl>> many;
    many.reserve(512);
    for (size_t i = 0; i != 512u; ++i)
    {
        many.emplace_back(new SixteenControl);
        if (! IsAligned(many.back().get(), __STDCPP_DEFAULT_NEW_ALIGNMENT__))
            return Fail("alignment: a control in a run of allocations is not 16-byte aligned");
    }
    {
        const std::unique_ptr<OverAlignedControl> over(new OverAlignedControl);
        const std::unique_ptr<OverAlignedControl> overNothrow(new (std::nothrow) OverAlignedControl);
        if (! IsAligned(over.get(), alignof(OverAlignedControl)) || ! overNothrow || ! IsAligned(overNothrow.get(), alignof(OverAlignedControl)))
            return Fail("alignment: an over-aligned control is not aligned as declared");
    }
    return true;
}

// Each new forwards to the delete of its own family: the block is written end to end, freed through the matching
// overload, and the sink sees one allocation and one equal free.
bool RoundTripEveryOverloadPair()
{
    constexpr size_t bytes = 200;
    std::vector<Event> events;
    events.reserve(16);
    const SinkScope sink(events);
    {
        auto* const block = static_cast<std::byte*>(DxUi::Control::operator new(bytes));
        if (! block || ! IsAligned(block, __STDCPP_DEFAULT_NEW_ALIGNMENT__))
            return Fail("round trip: new did not return an aligned block");
        std::fill_n(block, bytes, std::byte{0x5A});
        DxUi::Control::operator delete(block);
    }
    {
        auto* const block = static_cast<std::byte*>(DxUi::Control::operator new(bytes, std::nothrow));
        if (! block || ! IsAligned(block, __STDCPP_DEFAULT_NEW_ALIGNMENT__))
            return Fail("round trip: nothrow new did not return an aligned block");
        std::fill_n(block, bytes, std::byte{0x5A});
        DxUi::Control::operator delete(block, std::nothrow);
    }
    {
        auto* const block = static_cast<std::byte*>(DxUi::Control::operator new(bytes, std::align_val_t{64}));
        if (! block || ! IsAligned(block, 64u))
            return Fail("round trip: aligned new did not return an aligned block");
        std::fill_n(block, bytes, std::byte{0x5A});
        DxUi::Control::operator delete(block, std::align_val_t{64});
    }
    {
        auto* const block = static_cast<std::byte*>(DxUi::Control::operator new(bytes, std::align_val_t{128}, std::nothrow));
        if (! block || ! IsAligned(block, 128u))
            return Fail("round trip: aligned nothrow new did not return an aligned block");
        std::fill_n(block, bytes, std::byte{0x5A});
        DxUi::Control::operator delete(block, std::align_val_t{128}, std::nothrow);
    }
    {
        // An explicit alignment no larger than the default still belongs to the aligned family.
        auto* const block = static_cast<std::byte*>(DxUi::Control::operator new(bytes, std::align_val_t{8}));
        if (! block || ! IsAligned(block, 8u))
            return Fail("round trip: small aligned new did not return an aligned block");
        std::fill_n(block, bytes, std::byte{0x5A});
        DxUi::Control::operator delete(block, std::align_val_t{8});
    }
    // A null pointer is a no-op for every form.
    DxUi::Control::operator delete(nullptr);
    DxUi::Control::operator delete(nullptr, std::nothrow);
    DxUi::Control::operator delete(nullptr, std::align_val_t{64});
    DxUi::Control::operator delete(nullptr, std::align_val_t{64}, std::nothrow);
    if (events.size() != 10u)
        return Fail("round trip: expected five allocations and five frees");
    for (size_t pair = 0; pair != 5u; ++pair)
    {
        const auto& allocation = events[pair * 2u];
        const auto& release    = events[pair * 2u + 1u];
        if (! allocation.allocating || release.allocating || allocation.requested != bytes || release.requested != bytes ||
            allocation.reserved != release.reserved || allocation.reserved < bytes)
            return Fail("round trip: a free did not report its allocation");
    }
    // The default family costs 16 bytes; the aligned family keeps its larger header.
    if (events[0].reserved - bytes != 16u || events[2].reserved - bytes != 16u || events[4].reserved - bytes < 24u)
        return Fail("round trip: unexpected per-family overhead");
    const auto totals = Sum(events);
    return totals.bytes == 0 && totals.blocks == 0 ? true : Fail("round trip: totals do not return to zero");
}

bool OverAlignedTypeKeepsItsHeaderAndIsRecognisedAtDelete()
{
    std::vector<Event> events;
    events.reserve(8);
    {
        const SinkScope sink(events);
        const DxUi::Failure::Scope construction;
        auto* const over = new OverAlignedControl;
        if (! IsAligned(over, alignof(OverAlignedControl)))
            return Fail("over-aligned: not aligned as declared");
        delete over;
        std::unique_ptr<DxUi::Control> viaBase(new OverAlignedControl);
        viaBase.reset();
    }
    if (events.size() != 4u)
        return Fail("over-aligned: expected two allocations and two frees");
    // Today's header: a 24-byte header and the alignment's slack in front of the object.
    if (events[0].requested != sizeof(OverAlignedControl) || events[0].reserved < events[0].requested + 24u + alignof(OverAlignedControl) - 1u ||
        events[1].reserved != events[0].reserved || events[2].reserved != events[0].reserved || events[3].reserved != events[0].reserved)
        return Fail("over-aligned: header and slack are not the old ones, or a free differs from its allocation");
    const auto totals = Sum(events);
    return totals.bytes == 0 && totals.blocks == 0 ? true : Fail("over-aligned: totals do not return to zero");
}

bool SinkSetBeforeAfterAndToggled()
{
    const DxUi::Failure::Scope construction;
    // No sink at allocation, sink at free: the block was never reported, so its free is not reported either.
    {
        SixteenControl* control = nullptr;
        {
            const NoSinkScope none;
            control = new SixteenControl;
        }
        std::vector<Event> events;
        {
            const SinkScope sink(events);
            delete control;
        }
        if (! events.empty())
            return Fail("sink after allocation: a free was reported for a block that was never reported");
    }
    // Sink at allocation, none at free: the allocation is reported; with no sink nothing can be reported at the free.
    {
        std::vector<Event> events;
        SixteenControl* control = nullptr;
        {
            const SinkScope sink(events);
            control = new SixteenControl;
        }
        {
            const NoSinkScope none;
            delete control;
        }
        if (events.size() != 1u || ! events[0].allocating || events[0].requested != sizeof(SixteenControl))
            return Fail("sink cleared before free: expected exactly the allocation");
    }
    // A sink that is replaced between allocation and free receives the free of a block the first one reported.
    {
        std::vector<Event> first;
        std::vector<Event> second;
        SixteenControl* control = nullptr;
        {
            const SinkScope sink(first);
            control = new SixteenControl;
        }
        {
            const SinkScope sink(second);
            delete control;
        }
        if (first.size() != 1u || second.size() != 1u || second[0].allocating || second[0].reserved != first[0].reserved ||
            second[0].requested != first[0].requested)
            return Fail("sink replaced before free: the free must carry the allocation's request and accounted size");
    }
    // Toggled around a run of blocks: only the blocks allocated under a sink are ever reported, and what is reported
    // balances exactly when the sink is present at both ends.
    {
        std::vector<Event> events;
        std::vector<SixteenControl*> reported;
        std::vector<SixteenControl*> unreported;
        {
            const SinkScope sink(events);
            for (size_t i = 0; i != 8u; ++i)
                reported.push_back(new SixteenControl);
            {
                const NoSinkScope none;
                for (size_t i = 0; i != 5u; ++i)
                    unreported.push_back(new SixteenControl);
            }
            for (size_t i = 0; i != 3u; ++i)
                reported.push_back(new SixteenControl);
            for (auto* const control : unreported)
                delete control;
            for (auto* const control : reported)
                delete control;
        }
        const auto totals = Sum(events);
        if (events.size() != 22u || totals.blocks != 0 || totals.bytes != 0 || totals.requested != 0)
            return Fail("toggled sink: reported blocks must balance exactly and unreported ones must not appear");
    }
    // Over-aligned storage follows the same rule.
    {
        OverAlignedControl* unreported = nullptr;
        {
            const NoSinkScope none;
            unreported = new OverAlignedControl;
        }
        std::vector<Event> events;
        {
            const SinkScope sink(events);
            delete unreported;
            auto* const reported = new OverAlignedControl;
            delete reported;
        }
        const auto totals = Sum(events);
        if (events.size() != 2u || totals.blocks != 0 || totals.bytes != 0)
            return Fail("over-aligned toggled sink: totals must stay exact");
    }
    return true;
}

bool HeapSizeIsReadOnlyForASink()
{
    const DxUi::Failure::Scope construction;
    {
        const NoSinkScope none;
        const DxUi::Detail::WorkCounterScope work;
        std::vector<std::unique_ptr<SixteenControl>> blocks;
        for (size_t i = 0; i != 64u; ++i)
            blocks.emplace_back(new SixteenControl);
        blocks.clear();
        const std::unique_ptr<OverAlignedControl> over(new OverAlignedControl);
        if (work.Delta().memory.controlHeapSizeReads != 0u)
            return Fail("HeapSize: read although no sink is installed");
    }
    {
        std::vector<Event> events;
        events.reserve(300);
        const SinkScope sink(events);
        const DxUi::Detail::WorkCounterScope work;
        std::vector<std::unique_ptr<SixteenControl>> blocks;
        for (size_t i = 0; i != 64u; ++i)
            blocks.emplace_back(new SixteenControl);
        blocks.clear();
        // Frees read nothing: the prefix already holds the accounted size.
        if (work.Delta().memory.controlHeapSizeReads != 64u)
            return Fail("HeapSize: expected exactly one read per block allocated under a sink");
        const std::unique_ptr<OverAlignedControl> over(new OverAlignedControl);
        if (work.Delta().memory.controlHeapSizeReads != 65u)
            return Fail("HeapSize: the over-aligned path must read it once per block under a sink");
    }
    return true;
}
} // namespace
#pragma warning(pop)

bool TestControlAllocation()
{
    bool passed = true;
    passed      = DefaultPathOverheadIsSixteenBytes() && passed;
    passed      = AlignmentOfReturnedPointers() && passed;
    passed      = RoundTripEveryOverloadPair() && passed;
    passed      = OverAlignedTypeKeepsItsHeaderAndIsRecognisedAtDelete() && passed;
    passed      = SinkSetBeforeAfterAndToggled() && passed;
    passed      = HeapSizeIsReadOnlyForASink() && passed;
    return passed;
}
