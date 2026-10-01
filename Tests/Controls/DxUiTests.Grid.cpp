#include "DxUiTestHelpers.h"

#include <cmath>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <tuple>
#include <type_traits>
#include <unordered_set>

namespace
{

template <typename Metrics, typename = void> struct HasVisibleCellDataReadCount : std::false_type
{
};

template <typename Metrics>
struct HasVisibleCellDataReadCount<Metrics, std::void_t<decltype(std::declval<Metrics>().visibleCellDataReadCount)>> : std::true_type
{
};

template <typename Metrics> [[nodiscard]] std::optional<uint64_t> TryGetVisibleCellDataReadCount(const Metrics& metrics)
{
    if constexpr (HasVisibleCellDataReadCount<Metrics>::value)
    {
        return metrics.visibleCellDataReadCount;
    }
    else
    {
        return std::nullopt;
    }
}

void TestSortCycle()
{
    using DxUi::NextSortDirection;
    using DxUi::SortDirection;

    Require(NextSortDirection(SortDirection::None) == SortDirection::Ascending, "sort cycle none->ascending");
    Require(NextSortDirection(SortDirection::Ascending) == SortDirection::Descending, "sort cycle ascending->descending");
    Require(NextSortDirection(SortDirection::Descending) == SortDirection::None, "sort cycle descending->none");
}

void TestVisibleSpan()
{
    using DxUi::ComputeVisibleSpan;

    const auto span = ComputeVisibleSpan(1'000'000u, 24.0f, 240.0f, 120.0f);
    Require(span.beginIndex == 10u, "visible span begin index");
    Require(span.endIndex == 16u, "visible span end index");
    Require(span.offsetDip == 0.0f, "visible span offset");
}

void TestSelectionModel()
{
    using DxUi::GridSelectionModel;

    GridSelectionModel selection;
    selection.SetSingle(10u);
    Require(selection.GetCount() == 1u, "selection single count");
    Require(selection.IsSelected(10u), "selection single id");

    selection.Toggle(20u);
    Require(selection.GetCount() == 2u, "selection toggle add");
    selection.Toggle(10u);
    Require(! selection.IsSelected(10u), "selection toggle remove");

    const std::vector<uint64_t> ordered{5u, 6u, 7u, 8u, 9u, 10u};
    selection.SetRange(ordered, 6u, 9u);
    Require(selection.GetCount() == 4u, "selection range count");
    Require(selection.IsSelected(6u) && selection.IsSelected(9u), "selection range endpoints");

    selection.PreserveOrdered(std::vector<uint64_t>{9u, 7u, 6u, 12u});
    const auto preserved = selection.GetOrderedSelection();
    Require(preserved.size() == 3u, "selection preserve size");
    Require(preserved[0] == 9u && preserved[1] == 7u && preserved[2] == 6u, "selection preserve order");
}

// The exception contract of the selection model is unchanged by its ascending copy: the mutators that set one id or flip one stay
// noexcept, the two that take a list may throw, and the questions (IsSelected above all, which a paint asks) never throw.
static_assert(noexcept(std::declval<DxUi::GridSelectionModel&>().Clear()));
static_assert(noexcept(std::declval<DxUi::GridSelectionModel&>().SetSingle(0u)));
static_assert(noexcept(std::declval<DxUi::GridSelectionModel&>().Toggle(0u)));
static_assert(! noexcept(std::declval<DxUi::GridSelectionModel&>().SetRange(std::declval<const std::vector<uint64_t>&>(), 0u, 0u)));
static_assert(! noexcept(std::declval<DxUi::GridSelectionModel&>().PreserveOrdered(std::declval<const std::vector<uint64_t>&>())));
static_assert(noexcept(std::declval<const DxUi::GridSelectionModel&>().IsSelected(0u)));
static_assert(noexcept(std::declval<const DxUi::GridSelectionModel&>().GetAnchor()));
static_assert(noexcept(std::declval<const DxUi::GridSelectionModel&>().GetCount()));
static_assert(noexcept(std::declval<const DxUi::GridSelectionModel&>().GetOrderedSelection()));

// The selection model as it behaved before its membership test became a binary search: one vector in selection order, every
// question answered by scanning it. The tests below hold the model to this copy of that logic, operation by operation.
struct LinearSelectionReference
{
    std::vector<uint64_t> ids;
    std::optional<uint64_t> anchor;

    void Clear()
    {
        ids.clear();
        anchor.reset();
    }

    void SetSingle(uint64_t rowId)
    {
        ids.assign(1u, rowId);
        anchor = rowId;
    }

    void Toggle(uint64_t rowId)
    {
        const auto it = std::ranges::find(ids, rowId);
        if (it != ids.end())
        {
            ids.erase(it);
            if (anchor == rowId)
            {
                anchor = ids.empty() ? std::optional<uint64_t>() : std::optional<uint64_t>(ids.front());
            }
            return;
        }

        ids.push_back(rowId);
        if (! anchor)
        {
            anchor = rowId;
        }
    }

    void SetRange(const std::vector<uint64_t>& orderedRowIds, uint64_t anchorRowId, uint64_t currentRowId)
    {
        const auto anchorIt  = std::ranges::find(orderedRowIds, anchorRowId);
        const auto currentIt = std::ranges::find(orderedRowIds, currentRowId);
        if (anchorIt == orderedRowIds.end() || currentIt == orderedRowIds.end())
        {
            SetSingle(currentRowId);
            return;
        }

        const auto [first, last] = std::minmax(anchorIt, currentIt);
        ids.assign(first, last + 1);
        anchor = anchorRowId;
    }

    void PreserveOrdered(const std::vector<uint64_t>& orderedRowIds)
    {
        if (ids.empty())
        {
            return;
        }

        const std::unordered_set<uint64_t> wanted(ids.begin(), ids.end());
        ids.clear();
        for (const uint64_t rowId : orderedRowIds)
        {
            if (wanted.contains(rowId))
            {
                ids.push_back(rowId);
            }
        }

        if (anchor && ! std::ranges::contains(ids, anchor.value()))
        {
            anchor = ids.empty() ? std::optional<uint64_t>() : std::optional<uint64_t>(ids.front());
        }
    }

    [[nodiscard]] bool IsSelected(uint64_t rowId) const
    {
        return std::ranges::find(ids, rowId) != ids.end();
    }
};

// SplitMix64 from a fixed seed: the operations of a run, and so a failure, repeat exactly on every toolchain.
class SelectionRandom
{
public:
    explicit SelectionRandom(uint64_t seed) noexcept : _state(seed)
    {
    }

    [[nodiscard]] uint64_t Next() noexcept
    {
        _state += 0x9E3779B97F4A7C15ull;
        uint64_t mixed = _state;
        mixed          = (mixed ^ (mixed >> 30u)) * 0xBF58476D1CE4E5B9ull;
        mixed          = (mixed ^ (mixed >> 27u)) * 0x94D049BB133111EBull;
        return mixed ^ (mixed >> 31u);
    }

    [[nodiscard]] size_t Below(size_t limit) noexcept
    {
        return static_cast<size_t>(Next() % limit);
    }

    void Shuffle(std::vector<uint64_t>& values) noexcept
    {
        for (size_t index = values.size(); index > 1u; --index)
        {
            std::swap(values[index - 1u], values[Below(index)]);
        }
    }

private:
    uint64_t _state;
};

// The two sizes at which the model changes how it works, as Core_PerformanceAndResources.md states them: a selection of at most
// kScanLimit ids is answered by a scan and a larger one by a binary search, and the buffer of a selection that held room for more than
// kReleaseLimit ids is given back when the selection that replaces it needs at most half of that room.
constexpr size_t kScanLimit    = 1024u;
constexpr size_t kReleaseLimit = 4096u;

// Whether the model's two buffers hold at least their ids, and, when `roomWasGivenBack` is required (right after a mutator that
// replaces the selection), neither keeps more than the documented room: more than kReleaseLimit ids' worth of capacity for a
// selection of at most half of it.
[[nodiscard]] bool BuffersFollowTheRoomRule(const DxUi::GridSelectionModel& model, bool roomWasGivenBack)
{
    const DxUi::GridSelectionBufferDebugState buffers = model.DebugGetBuffers();
    const size_t count                                = model.GetCount();
    if (buffers.orderedIds < count || buffers.sortedIds < count)
    {
        return false;
    }
    return ! roomWasGivenBack ||
           ((buffers.orderedIds <= kReleaseLimit || count > buffers.orderedIds / 2u) && (buffers.sortedIds <= kReleaseLimit || count > buffers.sortedIds / 2u));
}

// What a randomized run reached, so that a test can require that its operations did cover the cases it exists for.
struct SelectionRunCoverage
{
    size_t rangesOverRepeatedIds         = 0u;
    size_t rangesFallingBackToOneId      = 0u;
    size_t preservesThatDropIds          = 0u;
    size_t preservesThatChangeNothing    = 0u;
    size_t togglesThatMoveTheAnchor      = 0u;
    size_t togglesThatLeaveACopy         = 0u;
    size_t selectionsOfHalfTheIds        = 0u;
    size_t selectionsWithinTheScanLimit  = 0u; // Selections of kScanLimit ids or fewer, not counting the empty selection.
    size_t selectionsPastTheScanLimit    = 0u;
    size_t selectionsPastTheReleaseLimit = 0u;
    size_t clearsThatGaveRoomBack        = 0u;
    size_t singlesThatGaveRoomBack       = 0u;
    size_t rangesThatGaveRoomBack        = 0u;
    size_t rangesThatKeptRoom            = 0u; // A range over more than half the room of a large buffer, which reuses it.
    size_t preservesThatGaveRoomBack     = 0u;
};

[[nodiscard]] bool HasRepeatedId(const std::vector<uint64_t>& ids)
{
    const std::unordered_set<uint64_t> distinct(ids.begin(), ids.end());
    return distinct.size() != ids.size();
}

// Applies `operationCount` random mutators to a GridSelectionModel and to the linear reference over `universeSize` scattered ids
// (plus the largest uint64_t), and after each one requires that the model answers as the reference does: its count, its order,
// its anchor and, for every id of the universe and a few others, IsSelected, which must also agree with membership in the model's
// own GetOrderedSelection. Lists given to SetRange and PreserveOrdered are shuffled, ascending, descending, or repeat ids. After
// each mutator that replaces the selection, the model must also keep no more room than the documented rule allows.
//
// A `wide` run is for universes of thousands of ids, whose selections reach past kScanLimit and kReleaseLimit. Its lists are long,
// a third of its ranges run from the first id of a list to the last (as Ctrl+A does) and a third span a few ids (as the click that
// follows it does), and it asks IsSelected of a sample of ids after each operation and of every id after every 25th.
[[nodiscard]] SelectionRunCoverage RunRandomSelectionOperations(uint64_t seed, size_t universeSize, size_t operationCount, bool wide = false)
{
    using DxUi::GridSelectionModel;

    SelectionRandom random(seed);
    // Draws the sample of a wide run's questions, so that asking them cannot change the operations of the run.
    SelectionRandom sampler(seed ^ 0x5A17ull);
    std::vector<uint64_t> universe;
    for (size_t index = 0u; index < universeSize; ++index)
    {
        universe.push_back(static_cast<uint32_t>(static_cast<uint32_t>(index) * 2654435761u));
    }
    universe.push_back((std::numeric_limits<uint64_t>::max)());
    std::vector<uint64_t> probes(universe);
    probes.push_back((std::numeric_limits<uint64_t>::max)() - 1u);
    probes.push_back(uint64_t{1} << 40u);

    GridSelectionModel model;
    LinearSelectionReference reference;
    SelectionRunCoverage coverage;
    const char* operation = "construction";
    size_t step           = 0u;
    // Whether the last operation replaced the selection (as Toggle does not), so that the room rule must hold after it.
    bool replaced = false;

    const auto fail = [&](const char* property)
    {
        const std::string message =
            std::format("selection model {} after {} (step {} of a run over {} ids, seed {:#x})", property, operation, step, universe.size(), seed);
        Require(false, message.c_str());
    };

    const auto verify = [&]
    {
        const std::span<const uint64_t> ordered = model.GetOrderedSelection();
        if (model.GetCount() != reference.ids.size())
        {
            fail("keeps the linear reference's count");
        }
        if (! std::ranges::equal(ordered, reference.ids))
        {
            fail("keeps the linear reference's selection order");
        }
        if (model.GetAnchor() != reference.anchor)
        {
            fail("keeps the linear reference's anchor");
        }
        if (! BuffersFollowTheRoomRule(model, replaced))
        {
            fail(replaced ? "keeps more room for a smaller selection than the room rule allows" : "holds fewer ids' room than ids");
        }
        if (wide)
        {
            const std::unordered_set<uint64_t> members(reference.ids.begin(), reference.ids.end());
            const auto ask = [&](uint64_t id)
            {
                if (model.IsSelected(id) != members.contains(id))
                {
                    fail("answers IsSelected differently from the linear reference");
                }
            };
            if (step % 25u == 0u)
            {
                for (const uint64_t id : probes)
                {
                    ask(id);
                }
            }
            else
            {
                for (size_t sample = 0u; sample < 40u; ++sample)
                {
                    ask(probes[sampler.Below(probes.size())]);
                }
                for (size_t sample = 0u; sample < 40u && ! ordered.empty(); ++sample)
                {
                    ask(ordered[sampler.Below(ordered.size())]);
                }
                if (! ordered.empty())
                {
                    ask(ordered.front());
                    ask(ordered.back());
                }
            }
        }
        else
        {
            for (const uint64_t id : probes)
            {
                const bool answer = model.IsSelected(id);
                if (answer != (std::ranges::find(ordered, id) != ordered.end()))
                {
                    fail("answers IsSelected differently from membership in its GetOrderedSelection");
                }
                if (answer != reference.IsSelected(id))
                {
                    fail("answers IsSelected differently from the linear reference");
                }
            }
        }
        const size_t count = reference.ids.size();
        if (count >= universe.size() / 2u)
        {
            ++coverage.selectionsOfHalfTheIds;
        }
        if (count > 0u && count <= kScanLimit)
        {
            ++coverage.selectionsWithinTheScanLimit;
        }
        if (count > kScanLimit)
        {
            ++coverage.selectionsPastTheScanLimit;
        }
        if (count > kReleaseLimit)
        {
            ++coverage.selectionsPastTheReleaseLimit;
        }
    };

    // A list in the order of some model's rows: shuffled, ascending or descending, or, as a model that gave one stable id to two
    // rows would, drawing the same id again. A wide run's lists hold 60 to 100 percent of the universe.
    const auto makeList = [&]
    {
        std::vector<uint64_t> list;
        const size_t length = wide ? universe.size() * 8u / 10u + random.Below(universe.size() * 2u / 10u + 1u) : 1u + random.Below(universe.size());
        const size_t mode   = random.Below(4u);
        if (mode == 3u)
        {
            for (size_t index = 0u; index < length; ++index)
            {
                list.push_back(universe[random.Below(universe.size())]);
            }
            return list;
        }
        list = universe;
        random.Shuffle(list);
        list.resize(length);
        if (mode == 1u)
        {
            std::ranges::sort(list);
        }
        else if (mode == 2u)
        {
            std::ranges::sort(list, std::greater<>());
        }
        return list;
    };

    // The mix of operations, as the share of a hundred below which each kind is chosen. A wide run makes more ranges and Clears, so
    // that a selection past the release limit is followed by each of the ways of replacing it.
    const size_t toggleBelow   = wide ? 25u : 38u;
    const size_t rangeBelow    = wide ? 50u : 58u;
    const size_t preserveBelow = wide ? 65u : 73u;
    const size_t singleBelow   = wide ? 85u : 90u;

    for (step = 0u; step < operationCount; ++step)
    {
        const size_t choice                                  = random.Below(100u);
        const std::optional<uint64_t> anchorBefore           = reference.anchor;
        const size_t countBefore                             = reference.ids.size();
        const DxUi::GridSelectionBufferDebugState roomBefore = model.DebugGetBuffers();
        const bool hadRoomToGiveBack                         = roomBefore.orderedIds > kReleaseLimit;
        replaced                                             = true;
        if (choice < toggleBelow)
        {
            operation = "Toggle";
            replaced  = false;
            // Most toggles name an id that is selected, so removal, the anchor's removal and a copy's removal all recur.
            const uint64_t id =
                (! reference.ids.empty() && random.Below(5u) < 3u) ? reference.ids[random.Below(reference.ids.size())] : probes[random.Below(probes.size())];
            const bool selectedBefore = reference.IsSelected(id);
            model.Toggle(id);
            reference.Toggle(id);
            if (selectedBefore && anchorBefore == id && reference.anchor != anchorBefore)
            {
                ++coverage.togglesThatMoveTheAnchor;
            }
            if (selectedBefore && reference.IsSelected(id))
            {
                ++coverage.togglesThatLeaveACopy;
            }
        }
        else if (choice < rangeBelow)
        {
            operation                        = "SetRange";
            const std::vector<uint64_t> list = makeList();
            const auto pick                  = [&]
            {
                // One time in eight an id that need not be in the list, which makes the range fall back to that one id.
                return random.Below(8u) == 0u ? probes[random.Below(probes.size())] : list[random.Below(list.size())];
            };
            uint64_t anchorId  = 0u;
            uint64_t currentId = 0u;
            const size_t shape = wide ? random.Below(3u) : 2u;
            if (shape == 0u)
            {
                // Ctrl+A over the list.
                anchorId  = list.front();
                currentId = list.back();
            }
            else if (shape == 1u)
            {
                // The click on a row near another that follows a larger selection.
                const size_t at = random.Below(list.size());
                anchorId        = list[at];
                currentId       = list[(std::min)(list.size() - 1u, at + random.Below(40u))];
            }
            else
            {
                anchorId  = pick();
                currentId = pick();
            }
            if (! std::ranges::contains(list, anchorId) || ! std::ranges::contains(list, currentId))
            {
                ++coverage.rangesFallingBackToOneId;
            }
            model.SetRange(list, anchorId, currentId);
            reference.SetRange(list, anchorId, currentId);
            if (HasRepeatedId(reference.ids))
            {
                ++coverage.rangesOverRepeatedIds;
            }
            const DxUi::GridSelectionBufferDebugState roomAfter = model.DebugGetBuffers();
            if (hadRoomToGiveBack && roomAfter.orderedIds < roomBefore.orderedIds)
            {
                ++coverage.rangesThatGaveRoomBack;
            }
            if (hadRoomToGiveBack && reference.ids.size() > roomBefore.orderedIds / 2u && roomAfter == roomBefore)
            {
                ++coverage.rangesThatKeptRoom;
            }
        }
        else if (choice < preserveBelow)
        {
            operation                             = "PreserveOrdered";
            const std::vector<uint64_t> idsBefore = reference.ids;
            std::vector<uint64_t> list;
            const size_t listMode = random.Below(3u);
            if (listMode == 0u)
            {
                list = makeList();
            }
            else if (listMode == 1u)
            {
                // What a data change gives: the selection in a new order, minus some rows, plus rows that were not selected.
                std::vector<uint64_t> shuffled(reference.ids);
                random.Shuffle(shuffled);
                for (const uint64_t id : shuffled)
                {
                    if (random.Below(4u) != 0u)
                    {
                        list.push_back(id);
                    }
                }
                for (size_t extra = 0u; extra < 3u; ++extra)
                {
                    list.push_back(probes[random.Below(probes.size())]);
                }
                random.Shuffle(list);
            }
            else
            {
                // The usual data change: the selection as it is, with rows that were not selected among it.
                list = reference.ids;
                for (size_t extra = 0u; extra < 3u; ++extra)
                {
                    const uint64_t id = probes[random.Below(probes.size())];
                    if (! reference.IsSelected(id))
                    {
                        list.insert(list.begin() + static_cast<std::ptrdiff_t>(random.Below(list.size() + 1u)), id);
                    }
                }
            }
            model.PreserveOrdered(list);
            reference.PreserveOrdered(list);
            if (reference.ids.size() < countBefore)
            {
                ++coverage.preservesThatDropIds;
            }
            if (! idsBefore.empty() && reference.ids == idsBefore)
            {
                ++coverage.preservesThatChangeNothing;
            }
            // A PreserveOrdered that changes nothing leaves the buffers as they are; one that changes the selection replaces them.
            replaced = reference.ids != idsBefore;
            if (hadRoomToGiveBack && model.DebugGetBuffers().orderedIds < roomBefore.orderedIds)
            {
                ++coverage.preservesThatGaveRoomBack;
            }
        }
        else if (choice < singleBelow)
        {
            operation         = "SetSingle";
            const uint64_t id = probes[random.Below(probes.size())];
            model.SetSingle(id);
            reference.SetSingle(id);
            if (hadRoomToGiveBack && model.DebugGetBuffers().orderedIds < roomBefore.orderedIds)
            {
                ++coverage.singlesThatGaveRoomBack;
            }
        }
        else
        {
            operation = "Clear";
            model.Clear();
            reference.Clear();
            if (hadRoomToGiveBack && model.DebugGetBuffers() == DxUi::GridSelectionBufferDebugState{})
            {
                ++coverage.clearsThatGaveRoomBack;
            }
        }
        verify();
    }
    return coverage;
}

void AddSelectionCoverage(SelectionRunCoverage& total, const SelectionRunCoverage& run)
{
    total.rangesOverRepeatedIds += run.rangesOverRepeatedIds;
    total.rangesFallingBackToOneId += run.rangesFallingBackToOneId;
    total.preservesThatDropIds += run.preservesThatDropIds;
    total.preservesThatChangeNothing += run.preservesThatChangeNothing;
    total.togglesThatMoveTheAnchor += run.togglesThatMoveTheAnchor;
    total.togglesThatLeaveACopy += run.togglesThatLeaveACopy;
    total.selectionsOfHalfTheIds += run.selectionsOfHalfTheIds;
    total.selectionsWithinTheScanLimit += run.selectionsWithinTheScanLimit;
    total.selectionsPastTheScanLimit += run.selectionsPastTheScanLimit;
    total.selectionsPastTheReleaseLimit += run.selectionsPastTheReleaseLimit;
    total.clearsThatGaveRoomBack += run.clearsThatGaveRoomBack;
    total.singlesThatGaveRoomBack += run.singlesThatGaveRoomBack;
    total.rangesThatGaveRoomBack += run.rangesThatGaveRoomBack;
    total.rangesThatKeptRoom += run.rangesThatKeptRoom;
    total.preservesThatGaveRoomBack += run.preservesThatGaveRoomBack;
}

void TestSelectionModelMatchesTheLinearReferenceOverRandomOperations()
{
    SelectionRunCoverage total;
    // A crowded universe of a dozen ids (every operation collides with the last), a mid-sized one, and a larger one whose
    // selections grow past a hundred ids; then one of two ids, where every list is nearly a repeat.
    for (const auto& [seed, ids, operations] : {std::tuple{0x5E1EC7ull, size_t{12}, size_t{4000}},
                                                std::tuple{0xA11CE5ull, size_t{48}, size_t{4000}},
                                                std::tuple{0xB0B5ull, size_t{257}, size_t{3000}},
                                                std::tuple{0xC0FFEEull, size_t{1}, size_t{500}}})
    {
        AddSelectionCoverage(total, RunRandomSelectionOperations(seed, ids, operations));
    }
    // The runs are only as strong as the cases they reach.
    Require(total.rangesOverRepeatedIds > 0u, "randomized selection runs reach a range over a list that repeats an id");
    Require(total.rangesFallingBackToOneId > 0u, "randomized selection runs reach a range whose anchor or current id is not in the list");
    Require(total.preservesThatDropIds > 0u, "randomized selection runs reach a PreserveOrdered that drops selected ids");
    Require(total.preservesThatChangeNothing > 0u, "randomized selection runs reach a PreserveOrdered that leaves the selection as it was");
    Require(total.togglesThatMoveTheAnchor > 0u, "randomized selection runs reach a Toggle that removes the anchor");
    Require(total.togglesThatLeaveACopy > 0u, "randomized selection runs reach a Toggle that leaves another copy of its id selected");
    Require(total.selectionsOfHalfTheIds > 0u, "randomized selection runs reach selections of half the ids or more");
    Require(total.selectionsWithinTheScanLimit > 0u, "randomized selection runs reach selections that the model scans");
}

// The same differential run over universes whose selections grow past the sizes at which the model changes how it works: 1,300 ids,
// where Ctrl+A leaves the scan limit (1,024) a few hundred ids behind, and 5,200, where a selection passes the release limit (4,096)
// and the clicks that follow it give the room back. Selections of 1,025 ids and more are answered by a binary search of the ascending
// copy, which the order of the ids in the selection (a shuffle of it) does not help, so the runs would fail if it searched the wrong copy.
void TestSelectionModelMatchesTheLinearReferenceOnBothSidesOfItsScanAndReleaseLimits()
{
    SelectionRunCoverage total;
    for (const auto& [seed, ids, operations] : {std::tuple{0x71DE5ull, size_t{1300}, size_t{700}}, std::tuple{0x1A61Eull, size_t{5200}, size_t{500}}})
    {
        AddSelectionCoverage(total, RunRandomSelectionOperations(seed, ids, operations, true));
    }
    Require(total.selectionsWithinTheScanLimit > 0u, "wide randomized selection runs reach selections that the model scans");
    Require(total.selectionsPastTheScanLimit > 0u, "wide randomized selection runs reach selections that the model searches");
    Require(total.selectionsPastTheReleaseLimit > 0u, "wide randomized selection runs reach selections larger than the release limit");
    Require(total.clearsThatGaveRoomBack > 0u, "wide randomized selection runs reach a Clear that gives room back");
    Require(total.singlesThatGaveRoomBack > 0u, "wide randomized selection runs reach a SetSingle that gives room back");
    Require(total.rangesThatGaveRoomBack > 0u, "wide randomized selection runs reach a SetRange that gives room back");
    Require(total.rangesThatKeptRoom > 0u, "wide randomized selection runs reach a SetRange that reuses the room of a large buffer");
    Require(total.preservesThatGaveRoomBack > 0u, "wide randomized selection runs reach a PreserveOrdered that gives room back");
    Require(total.preservesThatDropIds > 0u, "wide randomized selection runs reach a PreserveOrdered that drops selected ids");
    Require(total.togglesThatMoveTheAnchor > 0u, "wide randomized selection runs reach a Toggle that removes the anchor");
}

void TestSelectionModelKeepsEveryOccurrenceOfAnIdThatARangeRepeats()
{
    using DxUi::GridSelectionModel;

    // A list that gives one id to two rows. The range from the first 2 to the 4 holds the second 2 as well, and the model keeps
    // both, as it always has: the count, the order and the anchor are those of the slice of the list.
    GridSelectionModel selection;
    const std::vector<uint64_t> list{1u, 2u, 3u, 2u, 4u};
    selection.SetRange(list, 2u, 4u);
    Require(std::ranges::equal(selection.GetOrderedSelection(), std::vector<uint64_t>{2u, 3u, 2u, 4u}),
            "a range over a repeated id keeps every occurrence in list order");
    Require(selection.GetCount() == 4u, "a range over a repeated id counts every occurrence");
    Require(selection.IsSelected(2u) && selection.IsSelected(3u) && selection.IsSelected(4u), "a range over a repeated id selects the ids of its slice");
    Require(! selection.IsSelected(1u), "a range over a repeated id leaves out the ids before its slice");
    Require(selection.GetAnchor() == std::optional<uint64_t>(2u), "a range over a repeated id anchors at the id it started from");

    selection.Toggle(2u);
    Require(std::ranges::equal(selection.GetOrderedSelection(), std::vector<uint64_t>{3u, 2u, 4u}), "toggling a repeated id removes its first occurrence only");
    Require(selection.IsSelected(2u), "an id held twice stays selected when one occurrence is toggled off");
    Require(selection.GetAnchor() == std::optional<uint64_t>(3u), "toggling off the anchor's id while a copy remains moves the anchor to the first id");

    selection.Toggle(2u);
    Require(std::ranges::equal(selection.GetOrderedSelection(), std::vector<uint64_t>{3u, 4u}), "toggling the second occurrence of a repeated id removes it");
    Require(! selection.IsSelected(2u), "an id is not selected once its last occurrence is toggled off");
    Require(selection.GetAnchor() == std::optional<uint64_t>(3u), "toggling an id other than the anchor leaves the anchor");

    // PreserveOrdered keeps each occurrence a list gives of a selected id, in the list's order.
    selection.SetSingle(7u);
    selection.Toggle(8u);
    selection.PreserveOrdered(std::vector<uint64_t>{8u, 7u, 8u, 9u});
    Require(std::ranges::equal(selection.GetOrderedSelection(), std::vector<uint64_t>{8u, 7u, 8u}),
            "PreserveOrdered keeps every occurrence its list gives of a selected id");
    selection.Toggle(8u);
    Require(selection.IsSelected(8u) && selection.GetCount() == 2u, "an id PreserveOrdered kept twice stays selected after one toggle");
}

void TestSelectionModelRangeRunsBothWaysAndFallsBackToTheCurrentId()
{
    using DxUi::GridSelectionModel;

    GridSelectionModel selection;
    const std::vector<uint64_t> list{5u, 6u, 7u, 8u};

    selection.SetRange(list, 8u, 6u);
    Require(std::ranges::equal(selection.GetOrderedSelection(), std::vector<uint64_t>{6u, 7u, 8u}), "a range made backwards keeps the list's order");
    Require(selection.GetAnchor() == std::optional<uint64_t>(8u), "a range made backwards anchors at the id it started from");
    Require(! selection.IsSelected(5u) && selection.IsSelected(6u) && selection.IsSelected(7u) && selection.IsSelected(8u),
            "a range made backwards selects its slice");

    selection.SetRange(list, 7u, 7u);
    Require(std::ranges::equal(selection.GetOrderedSelection(), std::vector<uint64_t>{7u}), "a range whose ends are one id selects that id");
    Require(! selection.IsSelected(6u) && ! selection.IsSelected(8u), "a range replaces the earlier selection, its ascending copy included");

    selection.SetRange(list, 99u, 6u);
    Require(std::ranges::equal(selection.GetOrderedSelection(), std::vector<uint64_t>{6u}), "a range whose anchor is not in the list selects the current id");
    Require(selection.GetAnchor() == std::optional<uint64_t>(6u), "a range whose anchor is not in the list anchors at the current id");
    Require(! selection.IsSelected(7u), "a range that falls back to one id drops the earlier selection");

    selection.SetRange(list, 5u, 99u);
    Require(std::ranges::equal(selection.GetOrderedSelection(), std::vector<uint64_t>{99u}), "a range whose current id is not in the list selects that id");
    Require(selection.IsSelected(99u) && ! selection.IsSelected(5u) && ! selection.IsSelected(6u),
            "a range that falls back to an id outside the list selects only that id");
    Require(selection.GetAnchor() == std::optional<uint64_t>(99u), "a range whose current id is not in the list anchors at it");
}

void TestSelectionModelAnchorLeavesWithItsToggleAndMovesToTheFirstRemainingId()
{
    using DxUi::GridSelectionModel;

    GridSelectionModel selection;
    selection.SetSingle(10u);
    selection.Toggle(20u);
    selection.Toggle(30u);
    Require(selection.GetAnchor() == std::optional<uint64_t>(10u), "toggling ids in leaves the first selected id as the anchor");

    selection.Toggle(30u);
    Require(selection.GetAnchor() == std::optional<uint64_t>(10u) && ! selection.IsSelected(30u), "toggling off an id other than the anchor leaves the anchor");

    selection.Toggle(10u);
    Require(selection.GetAnchor() == std::optional<uint64_t>(20u), "toggling off the anchor moves it to the first remaining id");
    Require(! selection.IsSelected(10u) && selection.IsSelected(20u), "toggling off the anchor removes only its id");

    selection.Toggle(20u);
    Require(! selection.GetAnchor().has_value() && selection.GetCount() == 0u, "toggling off the last id leaves no anchor");
    Require(! selection.IsSelected(20u), "toggling off the last id leaves nothing selected");

    selection.Toggle(7u);
    selection.Toggle(8u);
    Require(selection.GetAnchor() == std::optional<uint64_t>(7u), "the first id toggled into an empty selection becomes the anchor");

    // The anchor moves to the first id of the selection order, not to a neighbor or the smallest id.
    selection.SetRange(std::vector<uint64_t>{4u, 3u, 2u, 1u}, 2u, 4u);
    Require(std::ranges::equal(selection.GetOrderedSelection(), std::vector<uint64_t>{4u, 3u, 2u}) && selection.GetAnchor() == std::optional<uint64_t>(2u),
            "a range selects its slice and anchors at its starting id");
    selection.Toggle(2u);
    Require(selection.GetAnchor() == std::optional<uint64_t>(4u), "toggling off the anchor moves it to the first id in selection order");
}

void TestSelectionModelPreserveOrderedKeepsSelectedIdsInTheGivenOrder()
{
    using DxUi::GridSelectionModel;

    GridSelectionModel selection;
    selection.SetRange(std::vector<uint64_t>{1u, 2u, 3u, 4u, 5u, 6u}, 3u, 6u);
    Require(selection.GetAnchor() == std::optional<uint64_t>(3u), "the range anchors at its starting id");

    // The usual data change leaves the selection as it was: its ids in its order, among rows that were never selected.
    selection.PreserveOrdered(std::vector<uint64_t>{1u, 3u, 2u, 4u, 5u, 9u, 6u});
    Require(std::ranges::equal(selection.GetOrderedSelection(), std::vector<uint64_t>{3u, 4u, 5u, 6u}),
            "PreserveOrdered leaves a selection that its list does not change");
    Require(selection.IsSelected(3u) && selection.IsSelected(4u) && selection.IsSelected(5u) && selection.IsSelected(6u),
            "PreserveOrdered that changes nothing keeps every id selected");
    Require(! selection.IsSelected(1u) && ! selection.IsSelected(2u) && ! selection.IsSelected(9u), "PreserveOrdered that changes nothing selects no other id");
    Require(selection.GetAnchor() == std::optional<uint64_t>(3u), "PreserveOrdered that changes nothing keeps the anchor");

    // The same ids in another order, as after a sort: the selection takes the list's order and every id stays selected.
    selection.PreserveOrdered(std::vector<uint64_t>{6u, 5u, 4u, 3u});
    Require(std::ranges::equal(selection.GetOrderedSelection(), std::vector<uint64_t>{6u, 5u, 4u, 3u}),
            "PreserveOrdered gives a selection the order of its list when it keeps every id");
    Require(selection.IsSelected(3u) && selection.IsSelected(4u) && selection.IsSelected(5u) && selection.IsSelected(6u),
            "PreserveOrdered keeps every id selected through a new order");
    Require(selection.GetAnchor() == std::optional<uint64_t>(3u), "PreserveOrdered keeps the anchor through a new order");

    selection.PreserveOrdered(std::vector<uint64_t>{6u, 2u, 4u, 3u, 9u});
    Require(std::ranges::equal(selection.GetOrderedSelection(), std::vector<uint64_t>{6u, 4u, 3u}),
            "PreserveOrdered keeps the selected ids in its list's order");
    Require(! selection.IsSelected(5u), "PreserveOrdered drops a selected id its list leaves out");
    Require(! selection.IsSelected(2u) && ! selection.IsSelected(9u), "PreserveOrdered never selects an id its list adds");
    Require(selection.IsSelected(6u) && selection.IsSelected(4u) && selection.IsSelected(3u), "PreserveOrdered keeps the selected ids it lists");
    Require(selection.GetAnchor() == std::optional<uint64_t>(3u), "PreserveOrdered keeps an anchor that survives");

    selection.PreserveOrdered(std::vector<uint64_t>{6u, 4u});
    Require(std::ranges::equal(selection.GetOrderedSelection(), std::vector<uint64_t>{6u, 4u}) && ! selection.IsSelected(3u),
            "PreserveOrdered drops the anchor's id with the others");
    Require(selection.GetAnchor() == std::optional<uint64_t>(6u), "PreserveOrdered moves a dropped anchor to the first id kept");

    selection.PreserveOrdered(std::vector<uint64_t>{1u, 2u});
    Require(selection.GetCount() == 0u && ! selection.GetAnchor().has_value(), "PreserveOrdered that keeps nothing leaves no selection and no anchor");
    Require(! selection.IsSelected(6u) && ! selection.IsSelected(4u), "PreserveOrdered that keeps nothing leaves nothing selected");

    selection.PreserveOrdered(std::vector<uint64_t>{6u, 4u});
    Require(selection.GetCount() == 0u && ! selection.IsSelected(6u), "PreserveOrdered on an empty selection selects nothing");
}

void TestSelectionModelMembershipFollowsEveryMutator()
{
    using DxUi::GridSelectionModel;

    std::vector<uint64_t> row;
    for (uint64_t id = 10u; id < 20u; ++id)
    {
        row.push_back(id);
    }
    const auto selectedIds = [&](const GridSelectionModel& model)
    {
        std::vector<uint64_t> held;
        for (const uint64_t id : row)
        {
            if (model.IsSelected(id))
            {
                held.push_back(id);
            }
        }
        return held;
    };

    GridSelectionModel selection;
    selection.SetRange(row, 10u, 19u);
    Require(selectedIds(selection) == row, "a range over a row selects each of its ids");

    selection.SetSingle(15u);
    Require(selectedIds(selection) == std::vector<uint64_t>{15u}, "SetSingle replaces the membership of an earlier selection");

    selection.Toggle(12u);
    selection.Toggle(18u);
    Require(selectedIds(selection) == (std::vector<uint64_t>{12u, 15u, 18u}), "Toggle adds ids to the membership");
    selection.Toggle(15u);
    Require(selectedIds(selection) == (std::vector<uint64_t>{12u, 18u}), "Toggle removes an id from the membership");

    selection.SetRange(row, 13u, 14u);
    Require(selectedIds(selection) == (std::vector<uint64_t>{13u, 14u}), "a range replaces the membership of an earlier selection");

    selection.PreserveOrdered(std::vector<uint64_t>{19u, 14u, 11u});
    Require(selectedIds(selection) == std::vector<uint64_t>{14u}, "PreserveOrdered leaves only the selected ids it lists as members");

    selection.Toggle(11u);
    selection.Clear();
    Require(selectedIds(selection).empty() && selection.GetCount() == 0u, "Clear leaves no id a member");

    selection.Toggle(19u);
    Require(selectedIds(selection) == std::vector<uint64_t>{19u}, "a Toggle after Clear adds to an empty membership");
}

void TestSelectionModelKeepsSelectionOrderForAScatteredLargeSelection()
{
    using DxUi::GridSelectionModel;

    // 20,000 scattered ids in the order of the rows that Ctrl+A selects. The selection keeps the rows' order, which is not the
    // ascending order that membership is answered from.
    std::vector<uint64_t> rows;
    for (uint32_t row = 0u; row < 20'000u; ++row)
    {
        rows.push_back(static_cast<uint32_t>(row * 2654435761u));
    }
    Require(! std::ranges::is_sorted(rows), "the large selection's ids are scattered");

    GridSelectionModel selection;
    selection.SetRange(rows, rows.front(), rows.back());
    Require(selection.GetCount() == rows.size(), "Ctrl+A over scattered ids selects every row");
    Require(std::ranges::equal(selection.GetOrderedSelection(), rows), "Ctrl+A over scattered ids keeps the rows' order, not the ascending order");
    Require(selection.GetAnchor() == std::optional<uint64_t>(rows.front()), "Ctrl+A anchors at its first row");

    const std::unordered_set<uint64_t> held(rows.begin(), rows.end());
    size_t wrongAnswers = 0u;
    for (const uint64_t id : rows)
    {
        wrongAnswers += selection.IsSelected(id) ? 0u : 1u;
        wrongAnswers += (held.contains(id + 1u) || ! selection.IsSelected(id + 1u)) ? 0u : 1u;
        wrongAnswers += (held.contains(id - 1u) || ! selection.IsSelected(id - 1u)) ? 0u : 1u;
    }
    Require(wrongAnswers == 0u, "membership of a large scattered selection answers true for its ids and false for their neighbors");

    // A data change that removes every other row keeps the rest in their order.
    std::vector<uint64_t> remaining;
    for (size_t index = 1u; index < rows.size(); index += 2u)
    {
        remaining.push_back(rows[index]);
    }
    selection.PreserveOrdered(remaining);
    Require(std::ranges::equal(selection.GetOrderedSelection(), remaining), "PreserveOrdered over a large scattered selection keeps the surviving rows' order");
    size_t staleAnswers = 0u;
    for (size_t index = 0u; index < rows.size(); ++index)
    {
        staleAnswers += selection.IsSelected(rows[index]) == (index % 2u == 1u) ? 0u : 1u;
    }
    Require(staleAnswers == 0u, "membership of a large scattered selection follows PreserveOrdered");
    Require(selection.GetAnchor() == std::optional<uint64_t>(remaining.front()), "PreserveOrdered moves the anchor of a row it dropped to the first row kept");
}

void TestSelectionModelCopiesAnswerMembershipIndependently()
{
    using DxUi::GridSelectionModel;

    GridSelectionModel original;
    original.SetRange(std::vector<uint64_t>{1u, 2u, 3u}, 1u, 3u);
    GridSelectionModel copy = original;
    copy.Toggle(2u);
    copy.Toggle(9u);
    Require(original.IsSelected(2u) && ! original.IsSelected(9u) && original.GetCount() == 3u,
            "changing a copy of a selection leaves the original's membership");
    Require(! copy.IsSelected(2u) && copy.IsSelected(9u) && copy.GetCount() == 3u, "a copy of a selection answers membership for its own changes");

    original = copy;
    Require(! original.IsSelected(2u) && original.IsSelected(9u) && original.IsSelected(1u) && original.GetCount() == 3u,
            "assigning a selection copies its membership");
}

// The ids of rows whose stable ids follow no order: row r has r * 2654435761 (mod 2^32), so the order in which rows are selected is not
// the ascending order that a binary search needs.
[[nodiscard]] std::vector<uint64_t> ScatteredRowIds(size_t count)
{
    std::vector<uint64_t> rows;
    rows.reserve(count);
    for (size_t row = 0u; row < count; ++row)
    {
        rows.push_back(static_cast<uint32_t>(static_cast<uint32_t>(row) * 2654435761u));
    }
    return rows;
}

// A selection that grows by one id at a time past the size at which the model stops scanning and starts to search, and shrinks back
// across it, answers membership correctly at every size: for every id around the limit, and for ids at the two ends of the selection
// order (the first and the newest) and a spread of others at every other size.
void TestSelectionModelAnswersMembershipAtEverySizeAroundItsScanLimit()
{
    using DxUi::GridSelectionModel;

    const size_t universe            = kScanLimit + 80u;
    const std::vector<uint64_t> rows = ScatteredRowIds(universe);
    const auto foreign               = [](size_t index) { return uint64_t{0x1'0000'0000u} + index; };
    std::vector<char> selected(universe, 0);
    GridSelectionModel model;

    const auto check = [&](const char* phase, size_t first, size_t last)
    {
        // The ids selected now are rows[first, last).
        const size_t size       = last - first;
        const bool nearTheLimit = size + 12u >= kScanLimit && size <= kScanLimit + 12u;
        size_t wrong            = model.GetCount() == size ? 0u : 1u;
        for (size_t row = 0u; row < universe; row += (nearTheLimit || size < 3u) ? 1u : 7u)
        {
            wrong += model.IsSelected(rows[row]) == (selected[row] != 0) ? 0u : 1u;
        }
        if (size > 0u)
        {
            wrong += model.IsSelected(rows[first]) ? 0u : 1u;
            wrong += model.IsSelected(rows[last - 1u]) ? 0u : 1u;
        }
        wrong += model.IsSelected(foreign(size)) ? 1u : 0u;
        wrong += model.IsSelected((std::numeric_limits<uint64_t>::max)()) ? 1u : 0u;
        const std::string message = std::format("membership of a selection of {} ids ({}) answers as the selection says", size, phase);
        Require(wrong == 0u, message.c_str());
    };

    check("empty", 0u, 0u);
    for (size_t row = 0u; row < universe; ++row)
    {
        model.Toggle(rows[row]);
        selected[row] = 1;
        check("growing", 0u, row + 1u);
    }
    Require(model.GetCount() > kScanLimit && std::ranges::equal(model.GetOrderedSelection(), rows),
            "the selection grew past the scan limit in the order of its rows");
    for (size_t row = 0u; row < universe; ++row)
    {
        model.Toggle(rows[row]);
        selected[row] = 0;
        check("shrinking", row + 1u, universe);
    }
    Require(model.GetCount() == 0u && ! model.GetAnchor().has_value(), "the selection shrank to nothing again");
}

// The room of a model's buffers. A selection that took room for more than kReleaseLimit ids gives it back when the selection that
// replaces it needs at most half of it (Clear, SetSingle, SetRange and a PreserveOrdered that drops ids), and keeps it otherwise.
void TestSelectionModelGivesBackTheRoomOfALargeSelection()
{
    using DxUi::GridSelectionBufferDebugState;
    using DxUi::GridSelectionModel;

    const auto selectAll = [](GridSelectionModel& model, const std::vector<uint64_t>& rows) { model.SetRange(rows, rows.front(), rows.back()); };

    // A selection of exactly kReleaseLimit ids keeps its room through Clear: only a selection of more ids gives it back.
    {
        const std::vector<uint64_t> rows = ScatteredRowIds(kReleaseLimit);
        GridSelectionModel model;
        selectAll(model, rows);
        const GridSelectionBufferDebugState held = model.DebugGetBuffers();
        Require(held.orderedIds >= rows.size() && held.sortedIds >= rows.size(),
                "a selection of the release limit's size holds room for its ids in both copies");
        model.Clear();
        Require(model.DebugGetBuffers() == held, "Clear keeps the room of a selection of exactly the release limit's size");
        selectAll(model, rows);
        Require(model.DebugGetBuffers() == held, "Ctrl+A again reuses the room that Clear kept");
    }

    // One id more, and Clear gives both copies' room back.
    {
        const std::vector<uint64_t> rows = ScatteredRowIds(kReleaseLimit + 1u);
        GridSelectionModel model;
        selectAll(model, rows);
        const GridSelectionBufferDebugState held = model.DebugGetBuffers();
        Require(held.orderedIds > kReleaseLimit && held.sortedIds > kReleaseLimit,
                "a selection of more than the release limit's size holds room for more than that");
        model.Clear();
        Require(model.DebugGetBuffers() == GridSelectionBufferDebugState{},
                "Clear gives back the room of both copies of a selection larger than the release limit");
        Require(model.GetCount() == 0u && ! model.GetAnchor().has_value() && ! model.IsSelected(rows.front()),
                "Clear still empties a selection whose room it gave back");

        // The model is as good as a new one: it selects, toggles, preserves and clears again.
        selectAll(model, rows);
        Require(model.GetCount() == rows.size() && model.IsSelected(rows[1234]) && model.IsSelected(rows.back()),
                "a model whose room was given back selects again");
        model.Toggle(rows[1234]);
        Require(! model.IsSelected(rows[1234]) && model.IsSelected(rows[1235]) && model.GetCount() == rows.size() - 1u,
                "a model whose room was given back toggles");
        model.Clear();
        model.Clear();
        Require(model.DebugGetBuffers() == GridSelectionBufferDebugState{}, "Clear of an empty model with no room leaves it so");
    }

    // The click that follows Ctrl+A: SetSingle gives back the room of both copies, and the one id it selects is found.
    {
        const std::vector<uint64_t> rows = ScatteredRowIds(10'000u);
        GridSelectionModel model;
        selectAll(model, rows);
        model.SetSingle(rows[777]);
        const GridSelectionBufferDebugState after = model.DebugGetBuffers();
        Require(after.orderedIds >= 1u && after.orderedIds <= kReleaseLimit, "SetSingle gives back the room of the ordered ids of a large selection");
        Require(after.sortedIds >= 1u && after.sortedIds <= kReleaseLimit, "SetSingle gives back the room of the ascending copy of a large selection");
        Require(model.GetCount() == 1u && model.IsSelected(rows[777]) && ! model.IsSelected(rows[778]), "SetSingle after a large selection selects its one id");
        Require(model.GetAnchor() == std::optional<uint64_t>(rows[777]), "SetSingle after a large selection anchors at its id");
    }

    // A SetRange whose selection needs half of the room or less (the Shift+click after Ctrl+A) gives the room back, and one that needs
    // more than half reuses it, so that stepping a large range by a row does not allocate every time.
    {
        const std::vector<uint64_t> rows = ScatteredRowIds(10'000u);
        GridSelectionModel model;
        selectAll(model, rows);
        const GridSelectionBufferDebugState held = model.DebugGetBuffers();
        const size_t half                        = held.orderedIds / 2u;
        Require(held.orderedIds == held.sortedIds && half > kReleaseLimit / 2u, "the large selection holds equal room in both copies");

        model.SetRange(rows, rows.front(), rows[half]);
        Require(model.GetCount() == half + 1u, "the range after Ctrl+A selects the rows from the anchor to the clicked row");
        Require(model.DebugGetBuffers() == held, "a SetRange over more than half of the room keeps that room");
        selectAll(model, rows);
        model.SetRange(rows, rows.front(), rows[half - 1u]);
        Require(model.GetCount() == half, "the range of exactly half of the room selects the rows from the anchor to the clicked row");
        const GridSelectionBufferDebugState smaller = model.DebugGetBuffers();
        Require(smaller.orderedIds < held.orderedIds && smaller.sortedIds < held.sortedIds,
                "a SetRange over exactly half of the room gives back the room of both copies");
        Require(smaller.orderedIds >= half && smaller.sortedIds >= half, "a SetRange that gave room back holds room for its ids");
        Require(model.IsSelected(rows.front()) && model.IsSelected(rows[half - 1u]) && ! model.IsSelected(rows[half]) &&
                    model.GetAnchor() == std::optional<uint64_t>(rows.front()),
                "a SetRange that gave room back selects its range and anchors at its first id");
        Require(std::ranges::equal(model.GetOrderedSelection(), std::span<const uint64_t>(rows).first(half)),
                "a SetRange that gave room back keeps the rows' order");

        selectAll(model, rows);
        model.SetRange(rows, rows[500], rows[100]);
        const GridSelectionBufferDebugState shortRange = model.DebugGetBuffers();
        Require(shortRange.orderedIds >= 401u && shortRange.orderedIds <= kReleaseLimit && shortRange.sortedIds >= 401u &&
                    shortRange.sortedIds <= kReleaseLimit,
                "a short SetRange after a large selection leaves room for a short selection");
        Require(model.IsSelected(rows[300]) && ! model.IsSelected(rows[99]) && ! model.IsSelected(rows[501]) &&
                    model.GetAnchor() == std::optional<uint64_t>(rows[500]),
                "a short range run backwards after a large selection selects its rows and anchors at its first id");
    }

    // A data change that drops almost every row (PreserveOrdered) leaves room for the rows that remain; one that drops nothing keeps
    // the buffers it has.
    {
        const std::vector<uint64_t> rows = ScatteredRowIds(10'000u);
        GridSelectionModel model;
        selectAll(model, rows);
        const GridSelectionBufferDebugState held = model.DebugGetBuffers();
        model.PreserveOrdered(rows);
        Require(model.DebugGetBuffers() == held, "a PreserveOrdered that drops nothing keeps its buffers");

        // Ten of the selected rows among 8,000 rows that were never selected.
        const std::vector<uint64_t> others = ScatteredRowIds(20'000u);
        std::vector<uint64_t> list(rows.begin() + 9000, rows.begin() + 9010);
        list.insert(list.end(), others.begin() + 12'000, others.end());
        model.PreserveOrdered(list);
        Require(model.GetCount() == 10u && model.IsSelected(rows[9005]) && ! model.IsSelected(rows[8999]),
                "PreserveOrdered keeps the rows of the list that were selected");
        const GridSelectionBufferDebugState kept = model.DebugGetBuffers();
        Require(kept.orderedIds >= 10u && kept.orderedIds <= kReleaseLimit && kept.sortedIds >= 10u && kept.sortedIds <= kReleaseLimit,
                "PreserveOrdered leaves room for the few rows that remain of a large selection, however long the list is");
    }

    // Toggle adds or removes one id and leaves the room as it is, whatever is selected after it.
    {
        const std::vector<uint64_t> rows = ScatteredRowIds(10'000u);
        GridSelectionModel model;
        selectAll(model, rows);
        const GridSelectionBufferDebugState held = model.DebugGetBuffers();
        for (size_t row = 0u; row < 9'990u; ++row)
        {
            model.Toggle(rows[row]);
        }
        Require(model.GetCount() == 10u && model.DebugGetBuffers() == held, "Toggle leaves the room of a selection that shrinks to a few ids");
    }
}

// What a user does to a Grid: Ctrl+A over a list longer than the release limit, then a click on one row, then Ctrl+A and a
// Shift+click on a row near the first. The model keeps no room for the selection it replaced.
void TestGridGivesBackTheRoomOfALargeSelectionWhenAClickReplacesIt()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    grid.SetSelectionMode(GridSelectionMode::Extended);
    const std::vector<uint64_t> rowIds = ScatteredRowIds(6000u);
    MutableRowGridModel model;
    model.SetRowIds(rowIds);
    grid.SetModel(&model);

    Require(grid.OnSelectAll(host), "select all handles a list longer than the release limit");
    const GridSelectionBufferDebugState everyRow = grid.GetSelectionModel().DebugGetBuffers();
    Require(grid.GetSelectionModel().GetCount() == 6000u && everyRow.orderedIds >= 6000u && everyRow.sortedIds >= 6000u,
            "select all holds room for every row of the list");

    Require(grid.RequestSelectRow(100u, 0u), "a click handles a row of a selection larger than the release limit");
    const GridSelectionBufferDebugState clicked = grid.GetSelectionModel().DebugGetBuffers();
    Require(clicked.orderedIds <= kReleaseLimit && clicked.sortedIds <= kReleaseLimit,
            "a click gives back the room of the selection larger than the release limit that it replaces");
    Require(grid.GetSelectionModel().GetCount() == 1u && grid.IsRowSelected(100u) && ! grid.IsRowSelected(101u) && ! grid.IsRowSelected(5999u),
            "a click on a row after Ctrl+A selects that row alone");

    Require(grid.OnSelectAll(host), "select all handles the list again");
    Require(grid.GetSelectionModel().GetCount() == 6000u && grid.IsRowSelected(0u) && grid.IsRowSelected(5999u), "select all selects every row again");
    Require(grid.RequestSelectRow(40u, MK_SHIFT), "Shift+click handles a row of a selection larger than the release limit");
    const GridSelectionBufferDebugState ranged = grid.GetSelectionModel().DebugGetBuffers();
    Require(ranged.orderedIds <= kReleaseLimit && ranged.sortedIds <= kReleaseLimit,
            "Shift+click gives back the room of the selection larger than the release limit that it replaces");
    Require(grid.GetSelectionModel().GetCount() == 41u && grid.IsRowSelected(0u) && grid.IsRowSelected(40u) && ! grid.IsRowSelected(41u),
            "Shift+click after Ctrl+A selects the rows from the anchor to the clicked row");
}

void TestGridSelectionOfALargeListFollowsGesturesAndDataChanges()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    grid.SetSelectionMode(GridSelectionMode::Extended);

    // 4,000 rows whose stable ids are scattered, so the selection's order and its ascending copy differ.
    std::vector<uint64_t> rowIds;
    for (uint32_t row = 0u; row < 4000u; ++row)
    {
        rowIds.push_back(static_cast<uint32_t>(row * 2654435761u));
    }
    MutableRowGridModel model;
    model.SetRowIds(rowIds);
    grid.SetModel(&model);

    const auto selectedRows = [&]
    {
        std::vector<size_t> rows;
        for (size_t row = 0u; row < model.GetRowCount(); ++row)
        {
            if (grid.IsRowSelected(row))
            {
                rows.push_back(row);
            }
        }
        return rows;
    };
    const auto rowsInRange = [](size_t first, size_t last)
    {
        std::vector<size_t> rows;
        for (size_t row = first; row <= last; ++row)
        {
            rows.push_back(row);
        }
        return rows;
    };

    Require(grid.OnSelectAll(host), "select all handles a large list");
    Require(selectedRows() == rowsInRange(0u, 3999u) && grid.GetSelectionModel().GetCount() == 4000u, "select all selects every row of a large list");
    Require(grid.GetSelectionModel().GetAnchor() == std::optional<uint64_t>(rowIds.front()), "select all anchors at the first row");

    Require(grid.RequestSelectRow(1234u, MK_CONTROL), "Ctrl+click handles a row of a large selection");
    std::vector<size_t> allButOne = rowsInRange(0u, 1233u);
    for (size_t row = 1235u; row < 4000u; ++row)
    {
        allButOne.push_back(row);
    }
    Require(selectedRows() == allButOne, "Ctrl+click removes one row from a large selection");

    Require(grid.RequestSelectRow(100u, 0u), "click handles a row of a large list");
    Require(selectedRows() == std::vector<size_t>{100u}, "a click replaces a large selection with its row");

    Require(grid.RequestSelectRow(300u, MK_SHIFT), "Shift+click handles a row of a large list");
    Require(selectedRows() == rowsInRange(100u, 300u), "Shift+click selects the rows from the anchor to the clicked row");
    Require(grid.RequestSelectRow(150u, MK_CONTROL), "Ctrl+click handles a row inside a range");
    std::vector<size_t> rangeWithoutOne = rowsInRange(100u, 149u);
    for (size_t row = 151u; row <= 300u; ++row)
    {
        rangeWithoutOne.push_back(row);
    }
    Require(selectedRows() == rangeWithoutOne, "Ctrl+click removes one row from a range");
    Require(grid.GetSelectionModel().GetAnchor() == std::optional<uint64_t>(rowIds[100]), "Ctrl+click on a row other than the anchor leaves the anchor");

    // Rows 200 to 2,999 leave the list: the selected rows that remain keep their order, and every other row stops being selected.
    std::vector<uint64_t> remainingIds(rowIds.begin(), rowIds.begin() + 200);
    remainingIds.insert(remainingIds.end(), rowIds.begin() + 3000, rowIds.end());
    model.SetRowIds(remainingIds);
    grid.NotifyDataChanged();
    std::vector<size_t> survivors = rowsInRange(100u, 149u);
    for (size_t row = 151u; row <= 199u; ++row)
    {
        survivors.push_back(row);
    }
    Require(selectedRows() == survivors, "a data change leaves selected only the selected rows that remain");
    std::vector<uint64_t> survivingIds;
    for (const size_t row : survivors)
    {
        survivingIds.push_back(remainingIds[row]);
    }
    Require(std::ranges::equal(grid.GetSelectionModel().GetOrderedSelection(), survivingIds), "a data change keeps the surviving selection in row order");
    Require(grid.GetSelectionModel().GetAnchor() == std::optional<uint64_t>(rowIds[100]), "a data change keeps an anchor that remains");
}

void TestGridVisibleWorkMetricsStayBoundedForLargeDatasets()
{
    using namespace DxUi;

    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    grid.SetRowHeightDip(24.0f);

    LargeGridModel model(1'000'000u, 64u, 96.0f);
    grid.SetModel(&model);

    const GridVisibleWorkMetrics metrics = grid.GetVisibleWorkMetrics();
    Require(metrics.visibleRowCount == 5u, "large grid visible-work metrics include the bottom partially visible body row");
    Require(metrics.visibleColumnCount == 4u, "large grid visible-work metrics clamp visible columns");
    Require(metrics.visibleCellCount == 20u, "large grid visible-work metrics include cell work for partially visible body rows");
    const std::optional<uint64_t> cellDataReadCount = TryGetVisibleCellDataReadCount(metrics);
    Require(cellDataReadCount.has_value(), "large grid visible-work metrics expose bounded cell-data read count");
    Require(cellDataReadCount.value_or(UINT64_MAX) == metrics.visibleCellCount, "large grid reads cell data once per visible cell for visible-work metrics");
    Require(metrics.hasVerticalScrollbar, "large grid visible-work metrics detect vertical scrollbar");
    Require(metrics.hasHorizontalScrollbar, "large grid visible-work metrics detect horizontal scrollbar");
}

void TestGroupedGridVisibleWorkMetricsIncludeHeaders()
{
    using namespace DxUi;

    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));
    grid.SetRowHeightDip(24.0f);
    grid.SetHeaderHeightDip(32.0f);

    GroupedGridModel model(6u);
    model.SetGroups({
        GroupedGridModel::Group{.stableId = 10u, .title = L"Favorites", .startRowIndex = 0u, .rowCount = 2u},
        GroupedGridModel::Group{.stableId = 20u, .title = L"Folders", .startRowIndex = 3u, .rowCount = 2u},
    });
    grid.SetModel(&model);

    const GridVisibleWorkMetrics metrics = grid.GetVisibleWorkMetrics();
    Require(metrics.visibleRowCount == 4u, "grouped grid visible-work metrics include the bottom partially visible row");
    Require(metrics.visibleGroupHeaderCount == 2u, "grouped grid visible-work metrics count visible headers");
    Require(metrics.visibleColumnCount == 1u, "grouped grid visible-work metrics keep visible column count");
    Require(metrics.visibleCellCount == 4u, "grouped grid visible-work metrics include cell work for partially visible rows");
    Require(metrics.hasVerticalScrollbar, "grouped grid visible-work metrics detect vertical scrollbar");
    Require(! metrics.hasHorizontalScrollbar, "grouped grid visible-work metrics avoid horizontal scrollbar when not needed");
}

void TestGridPartiallyVisibleBottomRowIsPaintedAndHitTestable()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 100.0f));
    grid->SetHeaderHeightDip(20.0f);
    grid->SetRowHeightDip(30.0f);

    LargeGridModel model(10u, 1u, 120.0f);
    grid->SetModel(&model);
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 100.0f));

    Require(grid->GetVisibleRowCount() == 3u, "grid counts the bottom partially visible body row");
    Require(grid->GetVisibleRowAt(2u).value_or(static_cast<size_t>(-1)) == 2u, "grid exposes the bottom partially visible row by visible index");

    const std::optional<D2D1_RECT_F> rowRect = grid->GetVisibleRowRect(2u);
    Require(rowRect.has_value(), "grid exposes a rect for the bottom partially visible row");
    Require(rowRect->top >= 79.5f && rowRect->bottom <= 100.5f && rowRect->bottom > rowRect->top,
            "grid clips the bottom partially visible row to the viewport");

    const std::optional<D2D1_RECT_F> cellRect = grid->GetVisibleCellRect(2u, 0u);
    Require(cellRect.has_value(), "grid exposes a cell rect for the bottom partially visible row");
    Require(cellRect->bottom > cellRect->top, "grid bottom partially visible cell rect has area");

    const D2D1_POINT_2F bottomRowPoint = D2D1::Point2F(48.0f, 90.0f);
    Require(grid->FindRowAtPoint(MakePointDip(bottomRowPoint)).value_or(static_cast<size_t>(-1)) == 2u, "grid hit-tests the bottom partially visible row");
    Require(grid->OnMouseMove(host, bottomRowPoint, 0u), "grid hover handles the bottom partially visible row");
    Require(grid->OnMouseDown(host, bottomRowPoint, false, 0u), "grid click handles the bottom partially visible row");
    Require(grid->GetPrimarySelectedRow().value_or(static_cast<size_t>(-1)) == 2u, "grid click selects the bottom partially visible row");
}

void TestGroupedGridProgrammaticSelectionAllowsOffscreenExpandedRows()
{
    using namespace DxUi;

    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));
    grid.SetRowHeightDip(24.0f);
    grid.SetHeaderHeightDip(32.0f);

    GroupedGridModel model(40u);
    model.SetGroups({
        GroupedGridModel::Group{.stableId = 10u, .title = L"Commands", .startRowIndex = 0u, .rowCount = 40u},
    });
    grid.SetModel(&model);

    constexpr size_t offscreenRowIndex = 25u;
    Require(! grid.GetVisibleRowRect(offscreenRowIndex).has_value(), "grouped grid test row starts outside the current viewport");
    Require(grid.RequestSelectRow(offscreenRowIndex, 0u), "grouped grid programmatic selection accepts offscreen expanded rows");
    Require(grid.GetPrimarySelectedRow().value_or(static_cast<size_t>(-1)) == offscreenRowIndex,
            "grouped grid programmatic selection records the offscreen expanded row");
}

void TestGridMouseWheelReportsUnhandledAtScrollEdges()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    grid->SetRowHeightDip(24.0f);
    grid->SetHeaderHeightDip(32.0f);

    LargeGridModel shortModel(2u, 1u, 120.0f);
    grid->SetModel(&shortModel);
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));

    Require(! grid->OnMouseWheel(host, D2D1::Point2F(20.0f, 80.0f), -static_cast<float>(WHEEL_DELTA), 0u),
            "grid wheel returns unhandled when there is no vertical extent");

    LargeGridModel longModel(80u, 1u, 120.0f);
    grid->SetModel(&longModel);
    grid->DebugSetScrollOffsets(0.0f, 0.0f);
    Require(! grid->OnMouseWheel(host, D2D1::Point2F(20.0f, 80.0f), static_cast<float>(WHEEL_DELTA), 0u),
            "grid wheel returns unhandled at the top edge for upward wheel input");
    Require(grid->OnMouseWheel(host, D2D1::Point2F(20.0f, 80.0f), -static_cast<float>(WHEEL_DELTA), 0u),
            "grid wheel returns handled when the wheel changes the vertical offset");

    grid->DebugSetScrollOffsets(1'000'000.0f, 0.0f);
    Require(! grid->OnMouseWheel(host, D2D1::Point2F(20.0f, 80.0f), -static_cast<float>(WHEEL_DELTA), 0u),
            "grid wheel returns unhandled at the bottom edge for downward wheel input");
}

void TestGroupedGridLongRunScrollKeepsVisibleRowRects()
{
    using namespace DxUi;

    class WideGroupedGridModel final : public IGridModel
    {
    public:
        [[nodiscard]] size_t GetRowCount() const noexcept override
        {
            return 80u;
        }

        [[nodiscard]] size_t GetColumnCount() const noexcept override
        {
            return 2u;
        }

        [[nodiscard]] GridColumnDesc GetColumn(size_t columnIndex) const override
        {
            return GridColumnDesc{
                .id          = columnIndex == 0u ? L"command" : L"key",
                .title       = columnIndex == 0u ? L"Command" : L"Key",
                .widthDip    = columnIndex == 0u ? 460.0f : 220.0f,
                .minWidthDip = columnIndex == 0u ? 260.0f : 140.0f,
            };
        }

        void GetCellData(size_t rowIndex, size_t columnIndex, GridCellData& outCell) const override
        {
            outCell.kind = GridCellKind::Text;
            outCell.text = std::format(L"R{}C{}", rowIndex, columnIndex);
        }

        [[nodiscard]] size_t GetGroupCount() const noexcept override
        {
            return 2u;
        }

        [[nodiscard]] GridGroupDesc GetGroup(size_t groupIndex) const override
        {
            return groupIndex == 0u ? GridGroupDesc{.stableId = 10u, .title = L"Function Bar", .startRowIndex = 0u, .rowCount = 12u}
                                    : GridGroupDesc{.stableId = 20u, .title = L"Folder View", .startRowIndex = 12u, .rowCount = 68u};
        }

        [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
        {
            if (rowId >= GetRowCount())
            {
                return std::nullopt;
            }

            return static_cast<size_t>(rowId);
        }
    };

    WideGroupedGridModel model;
    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(16.0f, 96.0f, 544.0f, 604.0f));
    grid->SetHeaderHeightDip(30.0f);
    grid->SetRowHeightDip(48.0f);
    grid->SetModel(&model);
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 560.0f, 620.0f));

    ThemePalette compactTheme = MakeDefaultThemePalette(true);
    compactTheme.density      = Density::Compact;
    host.SetTheme(compactTheme);

    const GridVisibleWorkMetrics initialMetrics = grid->GetVisibleWorkMetrics();
    Require(initialMetrics.visibleRowCount > 0u, "wide grouped grid starts with visible rows");
    Require(initialMetrics.visibleColumnCount == 2u, "wide grouped grid starts with both columns partly visible");
    Require(initialMetrics.hasVerticalScrollbar, "wide grouped grid starts with a vertical scrollbar");
    Require(initialMetrics.hasHorizontalScrollbar, "wide grouped grid starts with a horizontal scrollbar");

    grid->DebugSetScrollOffsets(2761.76f, 0.0f);
    const std::optional<size_t> boundaryFirstVisibleRow = grid->GetVisibleRowAt(0u);
    Require(boundaryFirstVisibleRow.has_value(), "wide grouped grid exposes a first visible row at a density-scaled row boundary");
    const std::optional<D2D1_RECT_F> boundaryRowRect = boundaryFirstVisibleRow ? grid->GetVisibleRowRect(boundaryFirstVisibleRow.value()) : std::nullopt;
    Require(boundaryRowRect.has_value(), "wide grouped grid skips zero-height rows at a density-scaled row boundary");
    if (boundaryRowRect.has_value())
    {
        RequireRectHasArea(boundaryRowRect.value(), "wide grouped grid boundary first visible row rect has area");
    }
    const std::optional<D2D1_RECT_F> boundaryKeyCellRect =
        boundaryFirstVisibleRow ? grid->GetVisibleCellRect(boundaryFirstVisibleRow.value(), 1u) : std::nullopt;
    Require(boundaryKeyCellRect.has_value(), "wide grouped grid keeps boundary first visible key-cell rect visible");
    if (boundaryKeyCellRect.has_value())
    {
        RequireRectHasArea(boundaryKeyCellRect.value(), "wide grouped grid boundary first visible key-cell rect has area");
    }
    grid->DebugSetScrollOffsets(0.0f, 0.0f);

    Require(grid->OnMouseWheel(host, D2D1::Point2F(0.0f, 0.0f), -static_cast<float>(WHEEL_DELTA), 0u), "wide grouped grid handles overlap probe wheel scroll");
    bool reachedScrollEdge = false;
    for (size_t chunk = 0u; chunk < 8u; ++chunk)
    {
        for (size_t detent = 0u; detent < 12u; ++detent)
        {
            if (! reachedScrollEdge)
            {
                reachedScrollEdge = ! grid->OnMouseWheel(host, D2D1::Point2F(0.0f, 0.0f), -static_cast<float>(WHEEL_DELTA), 0u);
            }
        }

        const GridVisibleWorkMetrics metrics = grid->GetVisibleWorkMetrics();
        Require(metrics.visibleRowCount > 0u, "wide grouped grid keeps bounded visible rows after wheel chunks");
        Require(metrics.visibleRowCount <= initialMetrics.visibleRowCount + 1u, "wide grouped grid keeps visible row work bounded after wheel chunks");
        Require(metrics.visibleColumnCount == initialMetrics.visibleColumnCount, "wide grouped grid keeps visible column work stable after wheel chunks");

        const std::optional<size_t> firstVisibleRow = grid->GetVisibleRowAt(0u);
        Require(firstVisibleRow.has_value(), "wide grouped grid exposes a first visible row after wheel chunks");
        const std::optional<D2D1_RECT_F> rowRect = firstVisibleRow ? grid->GetVisibleRowRect(firstVisibleRow.value()) : std::nullopt;
        Require(rowRect.has_value(), "wide grouped grid exposes a non-empty first visible row rect after wheel chunks");
        if (rowRect.has_value())
        {
            RequireRectHasArea(rowRect.value(), "wide grouped grid first visible row rect has area after wheel chunks");
            const std::optional<D2D1_RECT_F> headerRect = grid->GetVisibleColumnHeaderRect(0u);
            Require(headerRect.has_value(), "wide grouped grid exposes a visible header rect after wheel chunks");
            if (headerRect.has_value())
            {
                Require(rowRect->top >= headerRect->bottom - 0.5f, "wide grouped grid first visible row stays below the sticky header after wheel chunks");
            }
        }

        const std::optional<D2D1_RECT_F> keyCellRect = firstVisibleRow ? grid->GetVisibleCellRect(firstVisibleRow.value(), 1u) : std::nullopt;
        Require(keyCellRect.has_value(), "wide grouped grid exposes a non-empty first visible key-cell rect after wheel chunks");
        if (keyCellRect.has_value())
        {
            RequireRectHasArea(keyCellRect.value(), "wide grouped grid first visible key-cell rect has area after wheel chunks");
        }
    }
}

void TestScrollPanelForwardsCapturedChildGridScrollbarDrag()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* scroll = root->AddChild<ScrollPanel>();
    scroll->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));
    scroll->SetContentHeight(160.0f);
    auto* grid = scroll->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));
    grid->SetHeaderHeightDip(24.0f);
    grid->SetRowHeightDip(24.0f);

    LargeGridModel model(200u, 1u, 180.0f);
    grid->SetModel(&model);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));

    const ThemePalette theme       = MakeDefaultThemePalette(true);
    GridScrollbarVisualState state = grid->DebugGetScrollbarVisualState(theme);
    Require(state.hasVerticalScrollbar, "scroll-panel child grid exposes a vertical scrollbar");
    RequireRectHasArea(state.verticalThumbRect, "scroll-panel child grid exposes a visible scrollbar thumb");

    const LONG thumbX = static_cast<LONG>(std::lround((state.verticalThumbRect.left + state.verticalThumbRect.right) * 0.5f));
    const LONG thumbY = static_cast<LONG>(std::lround((state.verticalThumbRect.top + state.verticalThumbRect.bottom) * 0.5f));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(thumbX, thumbY), handled));
    Require(handled, "scroll-panel child grid handles scrollbar thumb mouse-down");

    static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(thumbX, 220), handled));
    Require(handled, "scroll-panel forwards captured child scrollbar mouse-move outside the viewport");
    Require(grid->GetVisibleWorkMetrics().verticalScrollDip > 0.5f, "scroll-panel child grid scrollbar thumb drag moves the vertical scroll offset");

    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(thumbX, 220), handled));
    Require(handled, "scroll-panel child grid handles scrollbar thumb mouse-up");
    state = grid->DebugGetScrollbarVisualState(theme);
    Require(! state.verticalThumbDragging, "scroll-panel child grid clears scrollbar drag state on mouse-up");
}

void TestGridScrollbarThumbGutterDragThroughWindowHost()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));
    grid->SetHeaderHeightDip(24.0f);
    grid->SetRowHeightDip(24.0f);

    LargeGridModel model(200u, 1u, 180.0f);
    grid->SetModel(&model);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));

    const ThemePalette theme             = MakeDefaultThemePalette(true);
    const GridScrollbarVisualState state = grid->DebugGetScrollbarVisualState(theme);
    Require(state.hasVerticalScrollbar, "grid exposes a vertical scrollbar for thumb gutter drag");
    RequireRectHasArea(state.verticalThumbRect, "grid exposes a visible vertical scrollbar thumb for gutter drag");

    const LONG gutterX = static_cast<LONG>(std::floor(state.verticalTrackRect.left + 1.0f));
    const LONG thumbY  = static_cast<LONG>(std::lround((state.verticalThumbRect.top + state.verticalThumbRect.bottom) * 0.5f));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(gutterX, thumbY), handled));
    Require(handled, "grid handles scrollbar thumb gutter mouse-down as a drag");

    static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(gutterX, thumbY + 48), handled));
    Require(handled, "grid handles captured scrollbar thumb gutter mouse-move");
    Require(grid->GetVisibleWorkMetrics().verticalScrollDip > 0.5f, "grid thumb gutter drag moves the vertical scroll offset");

    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(gutterX, thumbY + 48), handled));
    Require(handled, "grid handles captured scrollbar thumb gutter mouse-up");
}

void TestGroupedGridLayoutOffsetsRowsBelowHeaders()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));
    grid.SetRowHeightDip(24.0f);
    grid.SetHeaderHeightDip(32.0f);

    GroupedGridModel model(6u);
    model.SetGroups({
        GroupedGridModel::Group{.stableId = 10u, .title = L"Favorites", .startRowIndex = 0u, .rowCount = 2u},
        GroupedGridModel::Group{.stableId = 20u, .title = L"Folders", .startRowIndex = 3u, .rowCount = 2u},
    });
    grid.SetModel(&model);

    const GridCellLayoutMetrics row0Metrics = grid.GetCellLayoutMetrics(host, 0u, 0u);
    const GridCellLayoutMetrics row2Metrics = grid.GetCellLayoutMetrics(host, 2u, 0u);
    const GridCellLayoutMetrics row3Metrics = grid.GetCellLayoutMetrics(host, 3u, 0u);

    RequireFloatNear(row0Metrics.cellRect.top, 60.0f, 0.5f, "grouped grid first row begins below the first group header");
    RequireFloatNear(row2Metrics.cellRect.top, 108.0f, 0.5f, "grouped grid ungrouped row keeps prior group header offset");
    RequireFloatNear(row3Metrics.cellRect.top - row2Metrics.cellRect.top, 52.0f, 0.5f, "grouped grid inserts a header gap before the next group");
}

void TestHeaderlessGridStartsFirstRowAtTopEdge()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));
    grid.SetRowHeightDip(24.0f);
    grid.SetHeaderHeightDip(0.0f);

    MultiRowGridModel model(3u);
    grid.SetModel(&model);

    const GridCellLayoutMetrics row0Metrics = grid.GetCellLayoutMetrics(host, 0u, 0u);
    const GridCellLayoutMetrics row1Metrics = grid.GetCellLayoutMetrics(host, 1u, 0u);
    RequireFloatNear(row0Metrics.cellRect.top, 0.0f, 0.5f, "headerless grid first row starts at the top edge");
    RequireFloatNear(row1Metrics.cellRect.top - row0Metrics.cellRect.top, 24.0f, 0.5f, "headerless grid still spaces rows by row height");
}

void TestGroupedGridHeaderClickDoesNotSelectRows()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));
    grid->SetRowHeightDip(24.0f);
    grid->SetHeaderHeightDip(32.0f);
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    GroupedGridModel model(6u);
    model.SetGroups({
        GroupedGridModel::Group{.stableId = 10u, .title = L"Favorites", .startRowIndex = 0u, .rowCount = 2u},
        GroupedGridModel::Group{.stableId = 20u, .title = L"Folders", .startRowIndex = 3u, .rowCount = 2u},
    });

    RecordingGridDelegate delegate;
    grid->SetModel(&model);
    grid->SetDelegate(&delegate);

    Require(grid->OnMouseDown(host, D2D1::Point2F(40.0f, 46.0f), false, 0), "grouped grid group-header click is handled");
    Require(grid->GetSelectionModel().GetCount() == 0u, "grouped grid group-header click does not select any row");
    Require(delegate.selectionChangedCount == 0u, "grouped grid group-header click does not notify row selection changes");
}

void TestGroupedGridHeaderRightClickDoesNotDispatchRowContextMenu()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));
    grid->SetRowHeightDip(24.0f);
    grid->SetHeaderHeightDip(32.0f);
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    GroupedGridModel model(6u);
    model.SetGroups({
        GroupedGridModel::Group{.stableId = 10u, .title = L"Favorites", .startRowIndex = 0u, .rowCount = 2u},
        GroupedGridModel::Group{.stableId = 20u, .title = L"Folders", .startRowIndex = 3u, .rowCount = 2u},
    });

    RecordingGridDelegate delegate;
    grid->SetModel(&model);
    grid->SetDelegate(&delegate);

    Require(grid->OnMouseDown(host, D2D1::Point2F(40.0f, 46.0f), true, 0), "grouped grid group-header right-click is handled");
    Require(delegate.contextMenuCount == 0u, "grouped grid group-header right-click does not dispatch a row context menu");
    Require(grid->GetSelectionModel().GetCount() == 0u, "grouped grid group-header right-click does not synthesize row selection");
}

void TestGroupedGridCollapsedGroupsHideRowsFromVisibleWork()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 220.0f));
    grid->SetRowHeightDip(24.0f);
    grid->SetHeaderHeightDip(32.0f);
    static_cast<Panel*>(root.get())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 220.0f));

    GroupedGridModel model(6u);
    model.SetGroups({
        GroupedGridModel::Group{.stableId = 10u, .title = L"Favorites", .startRowIndex = 0u, .rowCount = 2u, .collapsed = true},
        GroupedGridModel::Group{.stableId = 20u, .title = L"Folders", .startRowIndex = 3u, .rowCount = 2u},
    });
    grid->SetModel(&model);
    host.SetRoot(std::move(root));

    const GridVisibleWorkMetrics metrics = grid->GetVisibleWorkMetrics();
    Require(metrics.visibleRowCount == 4u, "collapsed grouped grid hides collapsed rows from visible-work row count");
    Require(metrics.visibleGroupHeaderCount == 2u, "collapsed grouped grid keeps group headers visible");
    Require(metrics.visibleCellCount == 4u, "collapsed grouped grid only counts fully visible row cells");
    Require(grid->OnMouseDown(host, D2D1::Point2F(40.0f, 68.0f), false, 0), "collapsed grouped grid keeps the first visible body row hit-testable");
    Require(grid->GetSelectionModel().GetOrderedSelection().front() == model.GetStableRowId(2u),
            "collapsed grouped grid keeps the ungrouped row after the collapsed section visible");

    host.SetFocusControl(grid);
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_END, 0, handled));
    Require(handled, "collapsed grouped grid handles keyboard navigation to the last visible row");
    Require(grid->GetSelectionModel().GetOrderedSelection().front() == model.GetStableRowId(5u),
            "collapsed grouped grid keeps trailing ungrouped rows visible after grouped sections");
}

void TestGroupedGridNotifyDataChangedRehomesSelectionWhenGroupCollapsesExternally()
{
    using namespace DxUi;

    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));
    grid.SetRowHeightDip(24.0f);
    grid.SetHeaderHeightDip(32.0f);

    GroupedGridModel model(6u);
    model.SetGroups({
        GroupedGridModel::Group{.stableId = 10u, .title = L"Favorites", .startRowIndex = 0u, .rowCount = 2u},
        GroupedGridModel::Group{.stableId = 20u, .title = L"Folders", .startRowIndex = 3u, .rowCount = 2u},
    });

    RecordingGridDelegate delegate;
    grid.SetModel(&model);
    grid.SetDelegate(&delegate);
    grid.GetSelectionModel().SetSingle(model.GetStableRowId(1u));

    Require(model.SetGroupCollapsed(10u, true), "grouped grid test model collapses the requested group externally");
    grid.NotifyDataChanged();

    Require(delegate.selectionChangedCount == 1u, "grouped grid data-change collapse notifies when selection moves");
    Require(grid.GetSelectionModel().GetCount() == 1u, "grouped grid data-change collapse keeps one visible row selected");
    Require(grid.GetSelectionModel().GetOrderedSelection().front() == model.GetStableRowId(2u),
            "grouped grid data-change collapse rehomes selection to the nearest visible row");
}

void TestGroupedGridCaptureGroupLayoutReportsStableCollapsedState()
{
    using namespace DxUi;

    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    GroupedGridModel model(6u);
    model.SetGroups({
        GroupedGridModel::Group{.stableId = 10u, .title = L"Favorites", .startRowIndex = 0u, .rowCount = 2u},
        GroupedGridModel::Group{.stableId = 20u, .title = L"Folders", .startRowIndex = 3u, .rowCount = 2u, .collapsed = true},
    });

    grid.SetModel(&model);
    const auto layout = grid.CaptureGroupLayout();
    Require(layout.size() == 2u, "grouped grid capture returns one entry per visible group");
    Require(layout[0].groupStableId == 10u && ! layout[0].collapsed, "grouped grid capture preserves first group stable id and collapsed state");
    Require(layout[1].groupStableId == 20u && layout[1].collapsed, "grouped grid capture preserves second group stable id and collapsed state");
}

void TestGroupedGridApplyGroupLayoutRestoresCollapsedStateByStableId()
{
    using namespace DxUi;

    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));
    grid.SetRowHeightDip(24.0f);
    grid.SetHeaderHeightDip(32.0f);

    GroupedGridModel model(6u);
    model.SetGroups({
        GroupedGridModel::Group{.stableId = 10u, .title = L"Favorites", .startRowIndex = 0u, .rowCount = 2u, .collapsed = true},
        GroupedGridModel::Group{.stableId = 20u, .title = L"Folders", .startRowIndex = 3u, .rowCount = 2u},
    });

    CollapsibleGroupedGridDelegate delegate(model);
    grid.SetModel(&model);
    grid.SetDelegate(&delegate);
    grid.GetSelectionModel().SetSingle(model.GetStableRowId(3u));

    const std::array layoutToApply{
        GridGroupLayoutEntry{.groupStableId = 20u, .collapsed = true},
        GridGroupLayoutEntry{.groupStableId = 999u, .collapsed = true},
        GridGroupLayoutEntry{.groupStableId = 10u, .collapsed = false},
    };
    grid.ApplyGroupLayout(layoutToApply);

    Require(delegate.groupToggleCount == 2u, "grouped grid apply layout toggles only the matching changed groups");
    Require(! model.IsGroupCollapsed(10u), "grouped grid apply layout expands the matched first group by stable id");
    Require(model.IsGroupCollapsed(20u), "grouped grid apply layout collapses the matched second group by stable id");
    Require(delegate.selectionChangedCount == 1u, "grouped grid apply layout notifies when collapse hides the selected row");
    Require(grid.GetSelectionModel().GetCount() == 1u, "grouped grid apply layout keeps one visible row selected");
    Require(grid.GetSelectionModel().GetOrderedSelection().front() == model.GetStableRowId(5u),
            "grouped grid apply layout rehomes selection to the first visible row after the collapsed group");

    const auto capturedLayout = grid.CaptureGroupLayout();
    Require(capturedLayout.size() == 2u, "grouped grid capture after apply still returns one entry per group");
    Require(capturedLayout[0].groupStableId == 10u && ! capturedLayout[0].collapsed,
            "grouped grid capture after apply reports the restored first-group collapse state");
    Require(capturedLayout[1].groupStableId == 20u && capturedLayout[1].collapsed,
            "grouped grid capture after apply reports the restored second-group collapse state");
}

void TestGroupedGridKeyboardCollapsePreservesOnlyRowsOutsideCollapsedGroup()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));
    grid.SetRowHeightDip(24.0f);
    grid.SetHeaderHeightDip(32.0f);

    GroupedGridModel model(6u);
    model.SetGroups({
        GroupedGridModel::Group{.stableId = 10u, .title = L"Favorites", .startRowIndex = 0u, .rowCount = 2u},
        GroupedGridModel::Group{.stableId = 20u, .title = L"Folders", .startRowIndex = 3u, .rowCount = 2u},
    });

    CollapsibleGroupedGridDelegate delegate(model);
    grid.SetModel(&model);
    grid.SetDelegate(&delegate);
    grid.GetSelectionModel().SetSingle(model.GetStableRowId(0u));
    grid.GetSelectionModel().Toggle(model.GetStableRowId(2u));
    grid.GetSelectionModel().Toggle(model.GetStableRowId(3u));
    grid.GetSelectionModel().Toggle(model.GetStableRowId(1u));

    Require(grid.OnKeyDown(host, VK_LEFT, 0), "grouped grid keyboard collapse handles left-arrow on an expanded group");
    Require(delegate.groupToggleCount == 1u, "grouped grid keyboard collapse toggles exactly one group");
    Require(delegate.lastGroupStableId == 10u && delegate.lastGroupCollapsed, "grouped grid keyboard collapse targets the owning expanded group");
    Require(model.IsGroupCollapsed(10u), "grouped grid keyboard collapse updates the model collapse state");
    Require(delegate.selectionChangedCount == 1u, "grouped grid keyboard collapse notifies when hidden rows are dropped from the selection");

    const auto selection = grid.GetSelectionModel().GetOrderedSelection();
    Require(selection.size() == 2u, "grouped grid keyboard collapse preserves only rows outside the collapsed group");
    Require(selection[0] == model.GetStableRowId(2u) && selection[1] == model.GetStableRowId(3u),
            "grouped grid keyboard collapse preserves surviving selection order without row-id rescans");
}

void TestGroupedGridCopySkipsRowsHiddenByCollapsedGroups()
{
    using namespace DxUi;

    ClipboardHostWindow window;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));
    grid.SetSelectionMode(GridSelectionMode::Extended);

    GroupedGridModel model(5u);
    model.SetGroups({
        GroupedGridModel::Group{.stableId = 10u, .title = L"Favorites", .startRowIndex = 0u, .rowCount = 2u},
        GroupedGridModel::Group{.stableId = 20u, .title = L"Folders", .startRowIndex = 3u, .rowCount = 1u},
    });

    grid.SetModel(&model);
    grid.GetSelectionModel().SetSingle(model.GetStableRowId(0u));
    grid.GetSelectionModel().Toggle(model.GetStableRowId(2u));

    Require(model.SetGroupCollapsed(10u, true), "grouped grid copy test collapses the selected group externally");
    Require(SetClipboardUnicodeTextForTest(window.Hwnd(), L"sentinel"), "clipboard initialized before grouped grid copy");
    Require(grid.OnCopy(window.Host()), "grouped grid copy handles visible selected rows after collapse");

    const std::optional<std::wstring> copiedText = ReadClipboardUnicodeTextForTest(window.Hwnd());
    Require(copiedText.has_value(), "grouped grid copy writes text after collapse");
    Require(copiedText.value() == L"Row 02", "grouped grid copy omits selected rows hidden by collapsed groups");
}

void TestGroupedGridVisibleRowOrdinalFollowsCollapsedLayout()
{
    using namespace DxUi;

    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    GroupedGridModel model(9u);
    model.SetGroups({
        GroupedGridModel::Group{.stableId = 10u, .title = L"Favorites", .startRowIndex = 0u, .rowCount = 3u, .collapsed = true},
        GroupedGridModel::Group{.stableId = 20u, .title = L"Folders", .startRowIndex = 4u, .rowCount = 2u},
    });
    grid.SetModel(&model);

    Require(! grid.FindVisibleRowOrdinal(0u).has_value(), "grouped grid visible ordinal omits first collapsed group row");
    Require(! grid.FindVisibleRowOrdinal(2u).has_value(), "grouped grid visible ordinal omits last collapsed group row");
    Require(grid.FindVisibleRowOrdinal(3u).value_or(static_cast<size_t>(-1)) == 0u,
            "grouped grid visible ordinal counts the first ungrouped row after a collapsed group");
    Require(grid.FindVisibleRowOrdinal(4u).value_or(static_cast<size_t>(-1)) == 1u,
            "grouped grid visible ordinal counts the first expanded grouped row after preceding visible rows");
    Require(grid.FindVisibleRowOrdinal(5u).value_or(static_cast<size_t>(-1)) == 2u, "grouped grid visible ordinal counts expanded grouped rows in order");
    Require(grid.FindVisibleRowOrdinal(8u).value_or(static_cast<size_t>(-1)) == 5u,
            "grouped grid visible ordinal counts trailing ungrouped rows after grouped sections");
}

void TestGroupedGridSelectAllSkipsRowsHiddenByCollapsedGroups()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));
    grid.SetSelectionMode(GridSelectionMode::Extended);

    GroupedGridModel model(7u);
    model.SetGroups({
        GroupedGridModel::Group{.stableId = 10u, .title = L"Favorites", .startRowIndex = 0u, .rowCount = 2u, .collapsed = true},
        GroupedGridModel::Group{.stableId = 20u, .title = L"Folders", .startRowIndex = 3u, .rowCount = 2u},
    });
    grid.SetModel(&model);

    Require(grid.OnSelectAll(host), "grouped grid select-all handles visible rows when a group is collapsed");

    const auto selection = grid.GetSelectionModel().GetOrderedSelection();
    Require(selection.size() == 5u, "grouped grid select-all excludes rows hidden by collapsed groups");
    Require(selection[0] == model.GetStableRowId(2u) && selection[1] == model.GetStableRowId(3u) && selection[2] == model.GetStableRowId(4u) &&
                selection[3] == model.GetStableRowId(5u) && selection[4] == model.GetStableRowId(6u),
            "grouped grid select-all preserves visible row-id order without hidden collapsed-group rows");
}

void TestGridApplyColumnLayoutCapturesDisplayOrderAndWidths()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 140.0f));

    ColumnLayoutGridModel model;
    grid.SetModel(&model);
    const std::array layoutToApply{
        GridColumnLayoutEntry{.columnId = L"path", .displayIndex = 0u, .widthDip = 240.0f},
        GridColumnLayoutEntry{.columnId = L"modified", .displayIndex = 1u, .widthDip = 180.0f},
        GridColumnLayoutEntry{.columnId = L"name", .displayIndex = 2u, .widthDip = 140.0f},
    };
    grid.ApplyColumnLayout(layoutToApply);

    const auto layout = grid.CaptureColumnLayout();
    Require(layout.size() == 3u, "grid capture returns one entry per visible model column");
    Require(layout[0].columnId == L"path", "grid capture reports restored first column id");
    Require(layout[1].columnId == L"modified", "grid capture reports restored second column id");
    Require(layout[2].columnId == L"name", "grid capture reports restored third column id");
    Require(layout[0].displayIndex == 0u && layout[1].displayIndex == 1u && layout[2].displayIndex == 2u,
            "grid capture normalizes display indexes after layout restore");
    RequireFloatNear(layout[0].widthDip, 240.0f, 0.1f, "grid capture keeps restored first-column width");
    RequireFloatNear(layout[1].widthDip, 180.0f, 0.1f, "grid capture keeps restored second-column width");
    RequireFloatNear(layout[2].widthDip, 140.0f, 0.1f, "grid capture keeps restored third-column width");

    const GridCellLayoutMetrics pathMetrics     = grid.GetCellLayoutMetrics(host, 0u, 1u);
    const GridCellLayoutMetrics modifiedMetrics = grid.GetCellLayoutMetrics(host, 0u, 2u);
    const GridCellLayoutMetrics nameMetrics     = grid.GetCellLayoutMetrics(host, 0u, 0u);
    RequireFloatNear(pathMetrics.cellRect.left, 0.0f, 0.1f, "grid restored first column starts at the left edge");
    RequireFloatNear(modifiedMetrics.cellRect.left, 240.0f, 0.1f, "grid restored second column starts after the restored first width");
    RequireFloatNear(nameMetrics.cellRect.left, 420.0f, 0.1f, "grid restored third column starts after the restored leading widths");

    const std::optional<D2D1_RECT_F> firstDisplayHeader  = grid.GetVisibleDisplayColumnHeaderRect(0u);
    const std::optional<D2D1_RECT_F> secondDisplayHeader = grid.GetVisibleDisplayColumnHeaderRect(1u);
    const std::optional<D2D1_RECT_F> modelNameHeader     = grid.GetVisibleColumnHeaderRect(0u);
    Require(firstDisplayHeader.has_value(), "grid exposes a restored first display-column header rect");
    Require(secondDisplayHeader.has_value(), "grid exposes a restored second display-column header rect");
    Require(modelNameHeader.has_value(), "grid exposes the restored model-column header rect");
    if (firstDisplayHeader.has_value() && secondDisplayHeader.has_value() && modelNameHeader.has_value())
    {
        RequireFloatNear(firstDisplayHeader->left, 0.0f, 0.1f, "grid first display header rect follows display order");
        RequireFloatNear(
            firstDisplayHeader->right - firstDisplayHeader->left, 240.0f, 0.1f, "grid first display header rect uses restored first display width");
        RequireFloatNear(secondDisplayHeader->left, 240.0f, 0.1f, "grid second display header rect follows display order");
        RequireFloatNear(modelNameHeader->left, 420.0f, 0.1f, "grid model-column header rect still resolves by model index");
    }
}

void TestGridApplyColumnLayoutAppendsMissingColumnsInModelOrder()
{
    using namespace DxUi;

    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 140.0f));

    ColumnLayoutGridModel model;
    grid.SetModel(&model);
    const std::array layoutToApply{
        GridColumnLayoutEntry{.columnId = L"path", .displayIndex = 0u, .widthDip = 240.0f},
        GridColumnLayoutEntry{.columnId = L"unknown", .displayIndex = 1u, .widthDip = 300.0f},
    };
    grid.ApplyColumnLayout(layoutToApply);

    const auto layout = grid.CaptureColumnLayout();
    Require(layout.size() == 3u, "grid capture still returns all model columns after partial restore");
    Require(layout[0].columnId == L"path", "grid restore keeps explicitly ordered column first");
    Require(layout[1].columnId == L"name", "grid restore appends first missing column in original model order");
    Require(layout[2].columnId == L"modified", "grid restore appends remaining missing columns in original model order");
    RequireFloatNear(layout[0].widthDip, 240.0f, 0.1f, "grid restore applies width to explicitly restored column");
    RequireFloatNear(layout[1].widthDip, 160.0f, 0.1f, "grid restore keeps default width for first missing column");
    RequireFloatNear(layout[2].widthDip, 180.0f, 0.1f, "grid restore keeps default width for second missing column");
}

void TestGridCopyUsesRestoredDisplayOrder()
{
    using namespace DxUi;

    AttachedHostWindow window;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 140.0f));

    ColumnLayoutGridModel model;
    grid.SetModel(&model);
    grid.GetSelectionModel().SetSingle(1u);
    const std::array layoutToApply{
        GridColumnLayoutEntry{.columnId = L"path", .displayIndex = 0u, .widthDip = 240.0f},
        GridColumnLayoutEntry{.columnId = L"modified", .displayIndex = 1u, .widthDip = 180.0f},
        GridColumnLayoutEntry{.columnId = L"name", .displayIndex = 2u, .widthDip = 140.0f},
    };
    grid.ApplyColumnLayout(layoutToApply);

    Require(SetClipboardUnicodeTextForTest(window.Hwnd(), L"sentinel"), "clipboard initialized before grid copy");
    bool copied = false;
    std::optional<std::wstring> copiedText;
    for (int attempt = 0; attempt < 10; ++attempt)
    {
        if (grid.OnCopy(window.Host()))
        {
            copiedText = ReadClipboardUnicodeTextForTest(window.Hwnd());
            if (copiedText && copiedText.value() == L"C:\\Data\t2026-03-15\talpha.txt")
            {
                copied = true;
                break;
            }
        }

        Sleep(10);
    }

    Require(copied, "grid copy handles restored selection");
    Require(copiedText.has_value(), "grid copy writes clipboard text");
    Require(copiedText.value() == L"C:\\Data\t2026-03-15\talpha.txt", "grid copy follows restored display order");
}

void TestGridHeaderDragReordersColumnsWithoutTriggeringSort()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 140.0f));

    ColumnLayoutGridModel model;
    RecordingGridDelegate delegate;
    grid.SetModel(&model);
    grid.SetDelegate(&delegate);

    Require(grid.OnMouseDown(host, D2D1::Point2F(80.0f, 12.0f), false, 0), "grid header drag handles initial mouse-down");
    Require(grid.OnMouseMove(host, D2D1::Point2F(300.0f, 12.0f), 0), "grid header drag handles mouse-move");
    Require(grid.OnMouseUp(host, D2D1::Point2F(300.0f, 12.0f), false, 0), "grid header drag handles mouse-up");

    const auto layout = grid.CaptureColumnLayout();
    Require(layout.size() == 3u, "grid header drag keeps all columns");
    Require(layout[0].columnId == L"path", "grid header drag moves path column to the first display slot");
    Require(layout[1].columnId == L"name", "grid header drag moves name column after the path column");
    Require(layout[2].columnId == L"modified", "grid header drag leaves the last column in place");
    Require(delegate.sortRequestedCount == 0u, "grid header drag does not trigger sort");
}

void TestGridHeaderClickStillRequestsSortWithoutReordering()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 140.0f));

    ColumnLayoutGridModel model;
    RecordingGridDelegate delegate;
    grid.SetModel(&model);
    grid.SetDelegate(&delegate);

    Require(grid.OnMouseDown(host, D2D1::Point2F(80.0f, 12.0f), false, 0), "grid header click handles mouse-down");
    Require(grid.OnMouseUp(host, D2D1::Point2F(80.0f, 12.0f), false, 0), "grid header click handles mouse-up");

    const auto layout = grid.CaptureColumnLayout();
    Require(layout.size() == 3u, "grid header click keeps all columns");
    Require(layout[0].columnId == L"name" && layout[1].columnId == L"path" && layout[2].columnId == L"modified",
            "grid header click keeps the original column order");
    Require(delegate.sortRequestedCount == 1u, "grid header click requests one sort");
    Require(delegate.lastSortSpec.columnIndex == 0u, "grid header click sorts the clicked model column");
    Require(delegate.lastSortSpec.direction == SortDirection::Ascending, "grid header click starts sort at ascending");
}

void TestGridHeaderClickMovesSortGlyphToClickedColumn()
{
    using namespace DxUi;

    WindowHost host;
    ThemePalette theme  = host.GetTheme();
    theme.reducedMotion = true;
    host.SetTheme(theme);

    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 140.0f));

    ColumnLayoutGridModel model;
    class ApplyingSortDelegate final : public RecordingGridDelegate
    {
    public:
        explicit ApplyingSortDelegate(Grid& targetGrid) noexcept : grid(&targetGrid)
        {
        }

        void OnGridSortRequested(const GridSortSpec& sortSpec) override
        {
            RecordingGridDelegate::OnGridSortRequested(sortSpec);
            grid->SetSortSpec(sortSpec);
        }

    private:
        Grid* grid = nullptr;
    } delegate(grid);

    grid.SetModel(&model);
    grid.SetDelegate(&delegate);
    grid.SetSortSpec({1u, SortDirection::Ascending});

    const GridSortGlyphVisualState pathBefore = grid.DebugGetSortGlyphVisualState(theme, 1u, GetTickCount64() + 200u);
    Require(pathBefore.currentDirection == SortDirection::Ascending, "grid sort glyph starts on the Path column before header click");

    Require(grid.OnMouseDown(host, D2D1::Point2F(80.0f, 12.0f), false, 0), "grid sort glyph click handles mouse-down");
    Require(grid.OnMouseUp(host, D2D1::Point2F(80.0f, 12.0f), false, 0), "grid sort glyph click handles mouse-up");

    const uint64_t settledTick               = GetTickCount64() + 200u;
    const GridSortGlyphVisualState nameAfter = grid.DebugGetSortGlyphVisualState(theme, 0u, settledTick);
    const GridSortGlyphVisualState pathAfter = grid.DebugGetSortGlyphVisualState(theme, 1u, settledTick);
    Require(delegate.sortRequestedCount == 1u, "grid sort glyph click requests one delegated sort");
    Require(delegate.lastSortSpec.columnIndex == 0u, "grid sort glyph click requests the clicked Name column");
    Require(nameAfter.currentDirection == SortDirection::Ascending, "grid sort glyph moves to the clicked Name column");
    Require(pathAfter.currentDirection == SortDirection::None, "grid sort glyph leaves the previous Path column after delegated sort");
}

void TestGridHeaderDragReorderRoundTripsThroughCapturedLayout()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 140.0f));

    ColumnLayoutGridModel model;
    grid.SetModel(&model);

    Require(grid.OnMouseDown(host, D2D1::Point2F(80.0f, 12.0f), false, 0), "grid reorder roundtrip handles initial mouse-down");
    Require(grid.OnMouseMove(host, D2D1::Point2F(300.0f, 12.0f), 0), "grid reorder roundtrip handles drag move");
    Require(grid.OnMouseUp(host, D2D1::Point2F(300.0f, 12.0f), false, 0), "grid reorder roundtrip handles drag mouse-up");

    const auto capturedLayout = grid.CaptureColumnLayout();
    Require(capturedLayout.size() == 3u, "grid reorder roundtrip captures all columns");

    Grid restoredGrid;
    restoredGrid.SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 140.0f));
    restoredGrid.SetModel(&model);
    restoredGrid.ApplyColumnLayout(capturedLayout);

    const auto restoredLayout = restoredGrid.CaptureColumnLayout();
    Require(restoredLayout.size() == capturedLayout.size(), "grid reorder roundtrip restores the captured layout size");
    Require(restoredLayout[0].columnId == L"path", "grid reorder roundtrip restores the reordered first column");
    Require(restoredLayout[1].columnId == L"name", "grid reorder roundtrip restores the reordered second column");
    Require(restoredLayout[2].columnId == L"modified", "grid reorder roundtrip restores the reordered third column");
}

void TestGridHeaderDragReordersColumnToEarlierDisplaySlot()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 140.0f));

    ColumnLayoutGridModel model;
    RecordingGridDelegate delegate;
    grid.SetModel(&model);
    grid.SetDelegate(&delegate);

    Require(grid.OnMouseDown(host, D2D1::Point2F(460.0f, 12.0f), false, 0), "grid reverse header drag handles initial mouse-down");
    Require(grid.OnMouseMove(host, D2D1::Point2F(40.0f, 12.0f), 0), "grid reverse header drag handles mouse-move");
    Require(grid.OnMouseUp(host, D2D1::Point2F(40.0f, 12.0f), false, 0), "grid reverse header drag handles mouse-up");

    const auto layout = grid.CaptureColumnLayout();
    Require(layout.size() == 3u, "grid reverse header drag keeps all columns");
    Require(layout[0].columnId == L"modified", "grid reverse header drag moves the trailing column into the first display slot");
    Require(layout[1].columnId == L"name", "grid reverse header drag shifts the former first column after the moved column");
    Require(layout[2].columnId == L"path", "grid reverse header drag shifts the former middle column after the moved column");
    Require(delegate.sortRequestedCount == 0u, "grid reverse header drag does not trigger sort");
}

void TestGridRightClickInvokesContextMenuForHitRow()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));

    MultiRowGridModel model(3u);
    RecordingGridDelegate delegate;
    grid.SetModel(&model);
    grid.SetDelegate(&delegate);

    const D2D1_POINT_2F rowPoint = D2D1::Point2F(48.0f, 74.0f);
    Require(grid.OnMouseDown(host, rowPoint, true, 0), "grid right-click is handled");
    Require(delegate.contextMenuCount == 1u, "grid right-click invokes one context menu");
    Require(delegate.lastContextMenuRow == 1u, "grid right-click targets the hit row");
    RequirePointNear(delegate.lastContextMenuPoint, POINT{48, 74}, "grid right-click uses the hit point as its screen anchor");
}

void TestGridRightClickPreservesExtendedSelectionForSelectedHitRow()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    grid.SetSelectionMode(GridSelectionMode::Extended);

    MultiRowGridModel model(4u);
    RecordingGridDelegate delegate;
    grid.SetModel(&model);
    grid.SetDelegate(&delegate);
    grid.GetSelectionModel().SetSingle(model.GetStableRowId(0u));
    grid.GetSelectionModel().Toggle(model.GetStableRowId(2u));

    const D2D1_POINT_2F rowPoint = D2D1::Point2F(48.0f, 102.0f);
    Require(grid.OnMouseDown(host, rowPoint, true, 0), "grid right-click on selected extended row is handled");
    Require(delegate.contextMenuCount == 1u, "grid preserved-selection right-click invokes one context menu");
    Require(delegate.lastContextMenuRow == 2u, "grid preserved-selection right-click targets the hit row");
    Require(grid.GetSelectionModel().GetCount() == 2u, "grid right-click should preserve multi-selection on selected hit row");
    Require(grid.GetSelectionModel().IsSelected(model.GetStableRowId(0u)), "grid right-click should keep the first selected row");
    Require(grid.GetSelectionModel().IsSelected(model.GetStableRowId(2u)), "grid right-click should keep the hit selected row");
}

void TestGridSelectionChangeNotifiesDelegateOnUserSelection()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));

    MultiRowGridModel model(3u);
    RecordingGridDelegate delegate;
    grid.SetModel(&model);
    grid.SetDelegate(&delegate);

    Require(grid.OnMouseDown(host, D2D1::Point2F(48.0f, 74.0f), false, 0), "grid left-click is handled");
    Require(delegate.selectionChangedCount == 1u, "grid selection change notifies delegate on pointer selection");
}

void TestGridPointerSelectionSurvivesDelegateModelSwap()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));

    MultiRowGridModel model(3u);
    ModelSwappingGridDelegate delegate;
    grid.SetModel(&model);
    grid.SetDelegate(&delegate);

    Require(grid.OnMouseDown(host, D2D1::Point2F(48.0f, 74.0f), false, 0), "grid click remains handled when the selection delegate swaps the model");
    Require(delegate.selectionChangedCount == 2u, "grid model-swap delegate observes both the initial selection and the model-clear reconciliation");
    Require(grid.GetSelectionModel().GetCount() == 0u, "grid clears selection when the delegate swaps the model away");
}

void TestGridSelectionChangeReportsSenderGrid()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));

    MultiRowGridModel model(4u);
    RecordingGridDelegate delegate;
    grid->SetModel(&model);
    grid->SetDelegate(&delegate);

    Require(grid->OnMouseDown(host, D2D1::Point2F(40.0f, 44.0f), false, 0), "grid sender-selection test handles pointer selection");
    Require(delegate.selectionChangedCount == 1u, "grid sender-selection test notifies exactly once");
    Require(delegate.lastSelectionSender == grid, "grid sender-selection test reports the originating grid");
}

void TestGridSelectionChangeNotifiesDelegateOnDataChange()
{
    using namespace DxUi;

    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));

    MutableRowGridModel model;
    model.SetRowIds({10u, 20u, 30u});

    RecordingGridDelegate delegate;
    grid.SetModel(&model);
    grid.SetDelegate(&delegate);
    grid.GetSelectionModel().SetSingle(20u);

    model.SetRowIds({10u, 30u});
    grid.NotifyDataChanged();

    Require(delegate.selectionChangedCount == 1u, "grid data change notifies delegate when selection is removed");
    Require(grid.GetSelectionModel().GetCount() == 0u, "grid selection clears when the selected row disappears");
}

void TestGridCellLayoutMetricsReserveSpaceForCheckboxAndBadge()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 120.0f));

    GridCellData cellData;
    cellData.kind      = GridCellKind::Checkbox;
    cellData.text      = L"Enabled";
    cellData.checked   = true;
    cellData.badgeText = L"Live";
    cellData.badgeTone = AdornmentTone::Info;

    SingleCellGridModel model(cellData);
    grid.SetModel(&model);

    const GridCellLayoutMetrics metrics = grid.GetCellLayoutMetrics(host, 0u, 0u);
    Require(metrics.hasCheckbox, "grid checkbox layout reports checkbox presence");
    Require(! metrics.hasIcon, "grid checkbox layout does not fabricate icon presence");
    Require(metrics.hasBadge, "grid checkbox layout reports badge presence");
    RequireRectHasArea(metrics.checkboxRect, "grid checkbox layout checkbox rect has area");
    RequireRectHasArea(metrics.badgeRect, "grid checkbox layout badge rect has area");
    RequireRectHasArea(metrics.textRect, "grid checkbox layout text rect has area");
    Require(metrics.textRect.left >= metrics.checkboxRect.right, "grid checkbox text starts after the checkbox indicator");
    Require(metrics.textRect.right <= metrics.badgeRect.left, "grid checkbox text stops before the badge rect");
}

void TestGridCellLayoutMetricsReserveSpaceForIconAndBadge()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 120.0f));

    GridCellData cellData;
    cellData.kind      = GridCellKind::IconText;
    cellData.iconText  = L"*";
    cellData.text      = L"Plugin";
    cellData.badgeText = L"Beta";
    cellData.badgeTone = AdornmentTone::Warning;

    SingleCellGridModel model(cellData);
    grid.SetModel(&model);

    const GridCellLayoutMetrics metrics = grid.GetCellLayoutMetrics(host, 0u, 0u);
    Require(! metrics.hasCheckbox, "grid icon layout does not fabricate checkbox presence");
    Require(metrics.hasIcon, "grid icon layout reports icon presence");
    Require(metrics.hasBadge, "grid icon layout reports badge presence");
    RequireRectHasArea(metrics.iconRect, "grid icon layout icon rect has area");
    RequireRectHasArea(metrics.badgeRect, "grid icon layout badge rect has area");
    RequireRectHasArea(metrics.textRect, "grid icon layout text rect has area");
    Require(metrics.textRect.left >= metrics.iconRect.right, "grid icon text starts after the icon rect");
    Require(metrics.textRect.right <= metrics.badgeRect.left, "grid icon text stops before the badge rect");
}

void TestGridIconTextUsesIconFontForFluentGlyphs()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 120.0f));

    GridCellData cellData;
    cellData.kind     = GridCellKind::IconText;
    cellData.iconText = std::wstring(1u, static_cast<wchar_t>(0xE8A5));
    cellData.text     = L"alpha.txt";
    SingleCellGridModel model(cellData);
    grid.SetModel(&model);

    GridDebugCellVisualState state{};
    Require(grid.DebugGetCellVisualState(host.GetTheme(), 0u, 0u, state), "grid fluent icon font test resolves cell visuals");
    Require(state.hasIcon, "grid fluent icon font test reports icon text");
    Require(state.iconUsesIconFont, "grid fluent private-use glyphs use the icon font instead of the body text font");

    cellData.iconText = L"*";
    SingleCellGridModel plainModel(cellData);
    grid.SetModel(&plainModel);
    Require(grid.DebugGetCellVisualState(host.GetTheme(), 0u, 0u, state), "grid plain icon font test resolves cell visuals");
    Require(state.hasIcon, "grid plain icon font test reports icon text");
    Require(! state.iconUsesIconFont, "grid non-Fluent icon text keeps the small text font");
}

void TestGridIconIndexReservesIconSpaceWithoutTextGlyph()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 120.0f));

    GridCellData cellData;
    cellData.kind      = GridCellKind::IconText;
    cellData.iconIndex = 42;
    cellData.text      = L"alpha.txt";
    SingleCellGridModel model(cellData);
    grid.SetModel(&model);

    const GridCellLayoutMetrics metrics = grid.GetCellLayoutMetrics(host, 0u, 0u);
    Require(metrics.hasIcon, "grid icon-index layout reports icon presence without icon text");
    RequireRectHasArea(metrics.iconRect, "grid icon-index layout reserves icon rect");

    const GridVisibleWorkMetrics workMetrics = grid.GetVisibleWorkMetrics();
    Require(workMetrics.visibleIconCellCount == 1u, "grid visible-work metrics count icon-index cells as icon work");

    GridDebugCellVisualState state{};
    Require(grid.DebugGetCellVisualState(host.GetTheme(), 0u, 0u, state), "grid icon-index visual test resolves cell visuals");
    Require(state.hasIcon, "grid icon-index visual test reports icon presence");
    Require(! state.iconUsesIconFont, "grid icon-index visual test does not require icon-font glyph fallback");
}

void TestGridCellLayoutMetricsCenterDedicatedColorSwatch()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 120.0f));

    GridCellData cellData;
    cellData.kind           = GridCellKind::ColorSwatch;
    cellData.hasSwatchValue = true;
    cellData.swatchArgb     = 0xFF33AA55u;

    SingleCellGridModel model(cellData);
    grid.SetModel(&model);

    const GridCellLayoutMetrics metrics = grid.GetCellLayoutMetrics(host, 0u, 0u);
    Require(! metrics.hasCheckbox, "grid swatch layout does not fabricate checkbox presence");
    Require(! metrics.hasIcon, "grid swatch layout does not fabricate icon presence");
    Require(metrics.hasSwatch, "grid swatch layout reports swatch presence");
    RequireRectHasArea(metrics.swatchRect, "grid swatch layout swatch rect has area");
    Require(metrics.textRect.right <= metrics.textRect.left + 0.5f, "grid swatch layout collapses the text rect for a dedicated swatch cell");

    const float swatchCenterX = (metrics.swatchRect.left + metrics.swatchRect.right) * 0.5f;
    const float cellCenterX   = (metrics.cellRect.left + metrics.cellRect.right) * 0.5f;
    RequireFloatNear(swatchCenterX, cellCenterX, 1.0f, "grid swatch layout centers the swatch inside the cell");
}

void TestGridDoubleClickActivatesHitRow()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));

    MultiRowGridModel model(3u);
    RecordingGridDelegate delegate;
    grid->SetModel(&model);
    grid->SetDelegate(&delegate);

    const std::optional<D2D1_RECT_F> cellRect = grid->GetVisibleCellRect(1u, 0u);
    Require(cellRect.has_value(), "grid exposes the visible cell rect for double-click activation");
    const LONG x = static_cast<LONG>(std::lround((cellRect->left + cellRect->right) * 0.5f));
    const LONG y = static_cast<LONG>(std::lround((cellRect->top + cellRect->bottom) * 0.5f));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDBLCLK, 0, MAKELPARAM(x, y), handled));
    Require(handled, "grid double-click is handled");
    Require(delegate.rowActivatedCount == 1u, "grid double-click activates one row");
    Require(delegate.lastActivatedSender == grid, "grid double-click reports the sender grid");
    Require(delegate.lastActivatedRow == 1u, "grid double-click activates the hit row");
}

void TestDedicatedStateImageColumnCentersIconAndCollapsesText()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 120.0f));

    StateImageColumnGridModel model;
    grid.SetModel(&model);

    const GridCellLayoutMetrics metrics = grid.GetCellLayoutMetrics(host, 0u, 0u);
    Require(! metrics.hasCheckbox, "dedicated state-image column does not fabricate checkbox presence");
    Require(metrics.hasIcon, "dedicated state-image column reports icon presence");
    Require(! metrics.hasBadge, "dedicated state-image column does not fabricate badge presence");
    RequireRectHasArea(metrics.iconRect, "dedicated state-image column icon rect has area");
    Require(metrics.textRect.right <= metrics.textRect.left + 0.5f, "dedicated state-image column collapses the text rect");

    const float iconCenterX = (metrics.iconRect.left + metrics.iconRect.right) * 0.5f;
    const float cellCenterX = (metrics.cellRect.left + metrics.cellRect.right) * 0.5f;
    RequireFloatNear(iconCenterX, cellCenterX, 1.0f, "dedicated state-image icon is centered within the column");
}

void TestGridExplicitTooltipUsesCellTooltipText()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 120.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 120.0f));

    GridCellData cellData;
    cellData.kind        = GridCellKind::Text;
    cellData.text        = L"Open";
    cellData.tooltipText = L"Open runs the selected shortcut immediately.";
    SingleCellGridModel model(std::move(cellData));
    grid->SetModel(&model);

    const GridCellLayoutMetrics metrics = grid->GetCellLayoutMetrics(host, 0u, 0u);
    const D2D1_POINT_2F hoverPoint =
        D2D1::Point2F((metrics.cellRect.left + metrics.cellRect.right) * 0.5f, (metrics.cellRect.top + metrics.cellRect.bottom) * 0.5f);

    Require(grid->OnMouseMove(host, hoverPoint, 0), "grid cell hover is handled for explicit tooltip");
    Require(host.HasTooltip(), "grid explicit tooltip is shown for short cell text");
    Require(host.GetTooltipText() == L"Open runs the selected shortcut immediately.", "grid explicit tooltip uses the cell tooltip text");

    Require(grid->OnMouseLeave(host), "grid mouse leave clears explicit tooltip");
    Require(! host.HasTooltip(), "grid explicit tooltip clears on mouse leave");
}

void TestGridIgnoresExplicitTooltipThatRepeatsCellText()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 120.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 120.0f));

    GridCellData cellData;
    cellData.kind        = GridCellKind::Text;
    cellData.text        = L"Alternate View";
    cellData.tooltipText = cellData.text;
    SingleCellGridModel model(std::move(cellData));
    grid->SetModel(&model);

    const GridCellLayoutMetrics metrics = grid->GetCellLayoutMetrics(host, 0u, 0u);
    const D2D1_POINT_2F hoverPoint =
        D2D1::Point2F((metrics.cellRect.left + metrics.cellRect.right) * 0.5f, (metrics.cellRect.top + metrics.cellRect.bottom) * 0.5f);

    Require(grid->OnMouseMove(host, hoverPoint, 0), "grid cell hover is handled for repeated explicit tooltip");
    Require(! host.HasTooltip(), "grid suppresses an explicit tooltip that repeats the visible cell text");
}

void TestGridIgnoresExplicitTooltipThatRepeatsIconBadgeCellText()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 120.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 120.0f));

    GridCellData cellData;
    cellData.kind        = GridCellKind::IconText;
    cellData.iconText    = L"*";
    cellData.text        = L"Plugin";
    cellData.badgeText   = L"Beta";
    cellData.tooltipText = L"Plugin [Beta]";
    SingleCellGridModel model(std::move(cellData));
    grid->SetModel(&model);

    const GridCellLayoutMetrics metrics = grid->GetCellLayoutMetrics(host, 0u, 0u);
    const D2D1_POINT_2F hoverPoint =
        D2D1::Point2F((metrics.cellRect.left + metrics.cellRect.right) * 0.5f, (metrics.cellRect.top + metrics.cellRect.bottom) * 0.5f);

    Require(grid->OnMouseMove(host, hoverPoint, 0), "grid icon-badge cell hover is handled for repeated explicit tooltip");
    Require(! host.HasTooltip(), "grid suppresses an explicit tooltip that repeats the full icon-badge cell text");
}

void TestGridExplicitTooltipOverridesLongTextFallback()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 120.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 120.0f));

    GridCellData cellData;
    cellData.kind        = GridCellKind::Text;
    cellData.text        = L"This is a deliberately long fallback text that would normally trigger the default grid tooltip heuristic.";
    cellData.tooltipText = L"Conflict with Assign Shortcut.";
    SingleCellGridModel model(std::move(cellData));
    grid->SetModel(&model);

    const GridCellLayoutMetrics metrics = grid->GetCellLayoutMetrics(host, 0u, 0u);
    const D2D1_POINT_2F hoverPoint =
        D2D1::Point2F((metrics.cellRect.left + metrics.cellRect.right) * 0.5f, (metrics.cellRect.top + metrics.cellRect.bottom) * 0.5f);

    Require(grid->OnMouseMove(host, hoverPoint, 0), "grid cell hover is handled for tooltip precedence");
    Require(host.HasTooltip(), "grid explicit tooltip is shown when long text fallback is also available");
    Require(host.GetTooltipText() == L"Conflict with Assign Shortcut.", "grid explicit tooltip overrides the long-text fallback tooltip");
}

void TestGridLongTextFallbackTooltipRequiresClippedText()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 120.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 120.0f));

    GridCellData cellData;
    cellData.kind = GridCellKind::Text;
    cellData.text = L"This long result name is still fully visible when the column is wide enough.";
    SingleCellGridModel model(std::move(cellData));
    grid->SetModel(&model);

    const std::array<GridColumnLayoutEntry, 1u> wideLayout{GridColumnLayoutEntry{.columnId = L"status", .displayIndex = 0u, .widthDip = 560.0f}};
    grid->ApplyColumnLayout(wideLayout);
    GridCellLayoutMetrics metrics = grid->GetCellLayoutMetrics(host, 0u, 0u);
    D2D1_POINT_2F hoverPoint = D2D1::Point2F((metrics.cellRect.left + metrics.cellRect.right) * 0.5f, (metrics.cellRect.top + metrics.cellRect.bottom) * 0.5f);

    Require(grid->OnMouseMove(host, hoverPoint, 0), "grid cell hover is handled for visible long text");
    Require(! host.HasTooltip(), "grid does not show a long-text fallback tooltip when the cell text is fully visible");

    host.ClearTooltip();
    const std::array<GridColumnLayoutEntry, 1u> narrowLayout{GridColumnLayoutEntry{.columnId = L"status", .displayIndex = 0u, .widthDip = 96.0f}};
    grid->ApplyColumnLayout(narrowLayout);
    metrics    = grid->GetCellLayoutMetrics(host, 0u, 0u);
    hoverPoint = D2D1::Point2F((metrics.cellRect.left + metrics.cellRect.right) * 0.5f, (metrics.cellRect.top + metrics.cellRect.bottom) * 0.5f);

    Require(grid->OnMouseMove(host, hoverPoint, 0), "grid cell hover is handled for clipped long text");
    Require(host.HasTooltip(), "grid shows the long-text fallback tooltip when the visible text is clipped");
}

void TestGridRepeatedExplicitTooltipShowsWhenCellTextIsClipped()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 140.0f, 120.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 140.0f, 120.0f));

    GridCellData cellData;
    cellData.kind        = GridCellKind::Text;
    cellData.text        = L"Clipped repeated tooltip text";
    cellData.tooltipText = cellData.text;
    SingleCellGridModel model(std::move(cellData));
    grid->SetModel(&model);

    const std::array<GridColumnLayoutEntry, 1u> narrowLayout{GridColumnLayoutEntry{.columnId = L"status", .displayIndex = 0u, .widthDip = 80.0f}};
    grid->ApplyColumnLayout(narrowLayout);
    const GridCellLayoutMetrics metrics = grid->GetCellLayoutMetrics(host, 0u, 0u);
    const D2D1_POINT_2F hoverPoint =
        D2D1::Point2F((metrics.cellRect.left + metrics.cellRect.right) * 0.5f, (metrics.cellRect.top + metrics.cellRect.bottom) * 0.5f);

    Require(grid->OnMouseMove(host, hoverPoint, 0), "grid cell hover is handled for clipped repeated explicit tooltip");
    Require(host.HasTooltip(), "grid shows a repeated explicit tooltip when the visible text is clipped");
    Require(host.GetTooltipText() == L"Clipped repeated tooltip text", "grid uses the repeated explicit tooltip for clipped visible text");
}

void TestGridScrolledSingleLineCaptionOffersTooltip()
{
    using namespace DxUi;

    // Paint lays single-line captions out against the full cell, so a horizontal scroll that hides the start of a
    // caption clips it: hovering its visible part must offer the complete value, as for a clamped multiline cell.
    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 260.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 260.0f));
    GridCellData cellData;
    cellData.kind = GridCellKind::Text;
    cellData.text = L"Twenty-four chars long!!";
    SingleCellGridModel model(cellData);
    grid->SetModel(&model);
    const std::array<GridColumnLayoutEntry, 1> columns{{{L"status", 0u, 600.0f}}};
    grid->ApplyColumnLayout(columns);
    const auto hoverVisiblePart = [&](const char* context)
    {
        host.ClearTooltip();
        const GridCellLayoutMetrics metrics = grid->GetCellLayoutMetrics(host, 0u, 0u);
        Require(grid->OnMouseMove(host, D2D1::Point2F(150.0f, (metrics.cellRect.top + metrics.cellRect.bottom) * 0.5f), 0), context);
    };
    hoverVisiblePart("an unscrolled single-line caption is hovered");
    Require(! host.HasTooltip(), "a caption the viewport shows completely offers no tooltip");
    grid->DebugSetScrollOffsets(0.0f, 40.0f);
    hoverVisiblePart("a single-line caption scrolled under the left edge is hovered");
    Require(host.HasTooltip() && host.GetTooltipText() == L"Twenty-four chars long!!", "a caption the viewport cuts offers its complete value");
}

void TestGridMultilineTooltipFollowsPaintedLines()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 260.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 260.0f));

    GridCellData cellData;
    cellData.kind      = GridCellKind::Text;
    cellData.text      = L"Ligne 1\nLigne 2";
    cellData.multiline = true;
    SingleCellGridModel model(cellData);
    grid->SetModel(&model);

    const auto hoverCell = [&](std::wstring text, const char* context)
    {
        cellData.text = std::move(text);
        model         = SingleCellGridModel(cellData);
        grid->NotifyDataChanged();
        host.ClearTooltip();
        const GridCellLayoutMetrics metrics = grid->GetCellLayoutMetrics(host, 0u, 0u);
        const D2D1_POINT_2F hoverPoint =
            D2D1::Point2F((metrics.cellRect.left + metrics.cellRect.right) * 0.5f, (metrics.cellRect.top + metrics.cellRect.bottom) * 0.5f);
        Require(grid->OnMouseMove(host, hoverPoint, 0), context);
    };

    // The default 28-DIP row leaves a 22-DIP text area. With the default clamp of
    // two lines only "Ligne 1..." paints, so hovering must offer the full value.
    hoverCell(L"Ligne 1\nLigne 2", "grid multiline cell with an omitted line is hovered");
    Require(host.HasTooltip(), "a multiline cell that paints an omission marker shows a tooltip");
    Require(host.GetTooltipText() == L"Ligne 1\nLigne 2", "the multiline tooltip carries the complete model value");

    // A taller row paints both lines completely; nothing is omitted.
    grid->SetRowHeightDip(64.0f);
    hoverCell(L"Ligne 1\nLigne 2", "grid multiline cell with two complete lines is hovered");
    Require(! host.HasTooltip(), "a multiline cell whose lines all paint shows no tooltip");

    // Wrapping alone omits nothing: a paragraph wider than the column that wraps
    // within the clamp and row paints completely, unlike an unwrapped caption.
    grid->SetRowHeightDip(160.0f);
    grid->SetLineClamp(8u);
    hoverCell(L"Une phrase assez longue pour passer a la ligne dans la colonne", "grid wrapped multiline cell is hovered");
    Require(! host.HasTooltip(), "a wrapped multiline cell that paints completely shows no tooltip");

    // Paint lays text out against the full cell. When a horizontally scrolled
    // viewport hides part of the painted text, the full value is offered as for
    // single-line cells; short text that stays in view is not.
    const std::array<GridColumnLayoutEntry, 1u> wideLayout{GridColumnLayoutEntry{.columnId = L"status", .displayIndex = 0u, .widthDip = 560.0f}};
    grid->ApplyColumnLayout(wideLayout);
    hoverCell(L"Court\nTexte", "grid short multiline cell in a wide column is hovered");
    Require(! host.HasTooltip(), "short multiline text inside the viewport shows no tooltip");
    hoverCell(L"Une seule longue phrase qui continue bien au-dela de la partie visible de cette colonne tres large",
              "grid multiline cell extending past the viewport is hovered");
    Require(host.HasTooltip(), "multiline text that the viewport hides horizontally shows a tooltip");
}

void TestGridFolderViewVisualModeUsesFolderLikeRowHighlights()
{
    using namespace DxUi;

    class StyledGridModel final : public IGridModel
    {
    public:
        [[nodiscard]] size_t GetRowCount() const noexcept override
        {
            return 1u;
        }

        [[nodiscard]] size_t GetColumnCount() const noexcept override
        {
            return 1u;
        }

        [[nodiscard]] GridColumnDesc GetColumn(size_t /*columnIndex*/) const override
        {
            return GridColumnDesc{.id = L"name", .title = L"Name", .widthDip = 180.0f};
        }

        void GetCellData(size_t /*rowIndex*/, size_t /*columnIndex*/, GridCellData& outCell) const override
        {
            outCell.kind = GridCellKind::Text;
            outCell.text = L"alpha.txt";
        }

        [[nodiscard]] GridRowStyle GetRowStyle(size_t /*rowIndex*/) const override
        {
            GridRowStyle style{};
            style.rainbowSeed             = L"alpha.txt";
            style.folderViewRainbowHash32 = 123u;
            return style;
        }

        [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
        {
            return rowId == 0u ? std::optional<size_t>(0u) : std::nullopt;
        }
    };

    ThemePalette theme = MakeDefaultThemePalette(true);
    theme.rainbowMode  = true;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 120.0f));
    StyledGridModel model;
    grid->SetModel(&model);
    grid->SetVisualMode(GridVisualMode::FolderView);
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 120.0f));
    host.SetTheme(theme);
    host.SetFocusControl(grid);

    Require(grid->GetVisualMode() == GridVisualMode::FolderView, "grid records folder-view visual mode");
    Require(grid->RequestSelectRow(0u, 0u), "grid folder-view visual mode test selects the row");

    GridDebugRowVisualState selectedState{};
    Require(grid->DebugGetRowVisualState(theme, 0u, selectedState), "grid exposes selected folder-view row visual state");
    Require(selectedState.selected, "grid folder-view selected row reports selected state");
    Require(selectedState.usesRainbow, "grid folder-view selected row uses the rainbow selection tint in rainbow mode");
    Require(selectedState.fillArgb == PackColor(RainbowFolderViewSelectionTint(123u, theme.dark)),
            "grid folder-view selected row uses the folder-view rainbow highlight formula when a stable hash is supplied");

    Require(grid->RequestRemoveRowSelection(0u), "grid folder-view visual mode test clears selection");
    GridDebugRowVisualState idleState{};
    Require(grid->DebugGetRowVisualState(theme, 0u, idleState), "grid exposes idle folder-view row visual state");
    Require(! idleState.selected, "grid folder-view idle row reports unselected state");
    Require(! idleState.usesRainbow, "grid folder-view idle row does not tint every row in rainbow mode");
    Require(idleState.fillArgb == 0u, "grid folder-view idle row paints no full-row background");

    Require(grid->OnMouseMove(host, D2D1::Point2F(24.0f, 48.0f), 0u), "grid folder-view hover is handled");
    GridDebugRowVisualState hoverState{};
    Require(grid->DebugGetRowVisualState(theme, 0u, hoverState), "grid exposes hovered folder-view row visual state");
    Require(! hoverState.usesRainbow, "grid folder-view hover uses the normal hover fill instead of rainbow row tint");
    Require(hoverState.fillArgb == PackColor(theme.hoverFill), "grid folder-view hover uses the theme hover fill");
}

void TestGridEmptyModelDoesNotHitTestBodyRows()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));

    EmptyGridModel model;
    grid.SetModel(&model);

    const bool handled = grid.OnMouseMove(host, D2D1::Point2F(40.0f, 68.0f), 0);
    Require(! handled, "empty grid body hover is ignored");
    Require(model.cellAccessCount == 0u, "empty grid does not request out-of-range cell data");
}

void TestGridSetModelNullCancelsActiveColumnResize()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));

    MultiRowGridModel model(3u);
    grid.SetModel(&model);

    Require(grid.OnMouseDown(host, D2D1::Point2F(178.0f, 12.0f), false, 0), "grid header resize drag starts");
    grid.SetModel(nullptr);
    Require(! grid.OnMouseMove(host, D2D1::Point2F(208.0f, 12.0f), 0), "grid ignores resize mouse-move after model reset");
    Require(! grid.OnMouseUp(host, D2D1::Point2F(208.0f, 12.0f), false, 0), "grid ignores resize mouse-up after model reset");
}

void TestGridHeaderResizeZoneRequestsHorizontalResizeCursor()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));

    MultiRowGridModel model(3u);
    grid->SetModel(&model);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));

    Require(host.DebugResolveCursorKindForPoint(D2D1::Point2F(40.0f, 12.0f)) == WindowHostCursorKind::Default,
            "grid normal header body uses the default cursor");
    Require(host.DebugResolveCursorKindForPoint(D2D1::Point2F(178.0f, 12.0f)) == WindowHostCursorKind::HorizontalResize,
            "grid header resize hit zone requests the horizontal resize cursor");

    Require(grid->OnMouseDown(host, D2D1::Point2F(178.0f, 12.0f), false, 0), "grid starts resize from the resize cursor zone");
    host.CaptureMouse(grid);
    Require(host.DebugResolveCursorKindForPoint(D2D1::Point2F(80.0f, 90.0f)) == WindowHostCursorKind::HorizontalResize,
            "active grid resize capture keeps the horizontal resize cursor");
}

void TestGridHeaderBusyColumnAloneDoesNotAnimate()
{
    using namespace DxUi;

    WindowHost host;
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    GridCellData cellData;
    cellData.kind = GridCellKind::Text;
    cellData.text = L"Ready";
    SingleCellGridModel model(std::move(cellData));
    grid.SetModel(&model);
    grid.SetHeaderBusyColumn(0u);

    Require(! grid.Tick(host, 0u), "header busy column alone does not request animation ticks");
}

void TestGridCompactDensityShrinksHeaderAndRowMetrics()
{
    using namespace DxUi;

    struct Metrics final
    {
        float headerHeightDip = 0.0f;
        float rowHeightDip    = 0.0f;
    };

    const auto captureMetrics = [](Density density) noexcept
    {
        WindowHost host;
        auto root  = std::make_unique<Panel>();
        auto* grid = root->AddChild<Grid>();
        grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 360.0f, 220.0f));
        grid->SetRowHeightDip(46.0f);
        grid->SetHeaderHeightDip(30.0f);

        MultiRowGridModel model(4u);
        grid->SetModel(&model);

        host.SetRoot(std::move(root));
        static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 360.0f, 220.0f));

        ThemePalette theme = MakeDefaultThemePalette(true);
        theme.density      = density;
        host.SetTheme(theme);

        const GridCellLayoutMetrics row0 = grid->GetCellLayoutMetrics(host, 0u, 0u);
        const GridCellLayoutMetrics row1 = grid->GetCellLayoutMetrics(host, 1u, 0u);
        return Metrics{
            .headerHeightDip = row0.cellRect.top - grid->GetBounds().top,
            .rowHeightDip    = row1.cellRect.top - row0.cellRect.top,
        };
    };

    const Metrics standard = captureMetrics(Density::Standard);
    const Metrics compact  = captureMetrics(Density::Compact);

    Require(standard.headerHeightDip > 0.0f, "standard grid exposes a measurable header height");
    Require(standard.rowHeightDip > 0.0f, "standard grid exposes a measurable row height");
    Require(compact.headerHeightDip > 0.0f, "compact grid exposes a measurable header height");
    Require(compact.rowHeightDip > 0.0f, "compact grid exposes a measurable row height");
    Require(compact.headerHeightDip < standard.headerHeightDip, "compact grid density reduces the effective header height");
    Require(compact.rowHeightDip < standard.rowHeightDip, "compact grid density reduces the effective row height");
}

void TestGridEffectiveRowHeightBypassesDensityScaling()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    grid->SetHeaderHeightDip(0.0f);
    grid->SetRowHeightDip(46.0f);
    grid->SetEffectiveRowHeightDip(24.0f);

    MultiRowGridModel model(2u);
    grid->SetModel(&model);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));

    ThemePalette compactTheme = MakeDefaultThemePalette(true);
    compactTheme.density      = Density::Compact;
    host.SetTheme(compactTheme);

    GridCellLayoutMetrics row0 = grid->GetCellLayoutMetrics(host, 0u, 0u);
    GridCellLayoutMetrics row1 = grid->GetCellLayoutMetrics(host, 1u, 0u);
    RequireFloatNear(row1.cellRect.top - row0.cellRect.top, 24.0f, 0.5f, "effective row height is not compact-density scaled");

    grid->SetRowHeightDip(46.0f);
    row0 = grid->GetCellLayoutMetrics(host, 0u, 0u);
    row1 = grid->GetCellLayoutMetrics(host, 1u, 0u);
    Require(row1.cellRect.top - row0.cellRect.top < 46.0f, "SetRowHeightDip returns to density-scaled row metrics");
}

void TestGridRowMetricsClampToSegoeVariableBodyLineHeight()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 260.0f, 120.0f));
    grid->SetRowHeightDip(12.0f);
    grid->SetHeaderHeightDip(0.0f);

    MultiRowGridModel model(2u);
    grid->SetModel(&model);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 260.0f, 120.0f));

    ThemePalette compactTheme = MakeDefaultThemePalette(true);
    compactTheme.density      = Density::Compact;
    host.SetTheme(compactTheme);

    const GridCellLayoutMetrics row0 = grid->GetCellLayoutMetrics(host, 0u, 0u);
    const GridCellLayoutMetrics row1 = grid->GetCellLayoutMetrics(host, 1u, 0u);
    RequireFloatNear(row1.cellRect.top - row0.cellRect.top,
                     kMinimumInteractiveTextRowHeightDip,
                     0.5f,
                     "grid compact rows clamp to the shared Segoe UI Variable body line-height minimum");
}

} // namespace

void RunGridTests()
{
    DXUI_RUN_TEST(TestSortCycle);
    DXUI_RUN_TEST(TestVisibleSpan);
    DXUI_RUN_TEST(TestSelectionModel);
    DXUI_RUN_TEST(TestSelectionModelMatchesTheLinearReferenceOverRandomOperations);
    DXUI_RUN_TEST(TestSelectionModelMatchesTheLinearReferenceOnBothSidesOfItsScanAndReleaseLimits);
    DXUI_RUN_TEST(TestSelectionModelKeepsEveryOccurrenceOfAnIdThatARangeRepeats);
    DXUI_RUN_TEST(TestSelectionModelRangeRunsBothWaysAndFallsBackToTheCurrentId);
    DXUI_RUN_TEST(TestSelectionModelAnchorLeavesWithItsToggleAndMovesToTheFirstRemainingId);
    DXUI_RUN_TEST(TestSelectionModelPreserveOrderedKeepsSelectedIdsInTheGivenOrder);
    DXUI_RUN_TEST(TestSelectionModelMembershipFollowsEveryMutator);
    DXUI_RUN_TEST(TestSelectionModelKeepsSelectionOrderForAScatteredLargeSelection);
    DXUI_RUN_TEST(TestSelectionModelCopiesAnswerMembershipIndependently);
    DXUI_RUN_TEST(TestSelectionModelAnswersMembershipAtEverySizeAroundItsScanLimit);
    DXUI_RUN_TEST(TestSelectionModelGivesBackTheRoomOfALargeSelection);
    DXUI_RUN_TEST(TestGridSelectionOfALargeListFollowsGesturesAndDataChanges);
    DXUI_RUN_TEST(TestGridGivesBackTheRoomOfALargeSelectionWhenAClickReplacesIt);
    DXUI_RUN_TEST(TestGridVisibleWorkMetricsStayBoundedForLargeDatasets);
    DXUI_RUN_TEST(TestGroupedGridVisibleWorkMetricsIncludeHeaders);
    DXUI_RUN_TEST(TestGridPartiallyVisibleBottomRowIsPaintedAndHitTestable);
    DXUI_RUN_TEST(TestGroupedGridProgrammaticSelectionAllowsOffscreenExpandedRows);
    DXUI_RUN_TEST(TestGridMouseWheelReportsUnhandledAtScrollEdges);
    DXUI_RUN_TEST(TestGroupedGridLongRunScrollKeepsVisibleRowRects);
    DXUI_RUN_TEST(TestScrollPanelForwardsCapturedChildGridScrollbarDrag);
    DXUI_RUN_TEST(TestGridScrollbarThumbGutterDragThroughWindowHost);
    DXUI_RUN_TEST(TestGroupedGridLayoutOffsetsRowsBelowHeaders);
    DXUI_RUN_TEST(TestHeaderlessGridStartsFirstRowAtTopEdge);
    DXUI_RUN_TEST(TestGroupedGridHeaderClickDoesNotSelectRows);
    DXUI_RUN_TEST(TestGroupedGridHeaderRightClickDoesNotDispatchRowContextMenu);
    DXUI_RUN_TEST(TestGroupedGridCollapsedGroupsHideRowsFromVisibleWork);
    DXUI_RUN_TEST(TestGroupedGridNotifyDataChangedRehomesSelectionWhenGroupCollapsesExternally);
    DXUI_RUN_TEST(TestGroupedGridCaptureGroupLayoutReportsStableCollapsedState);
    DXUI_RUN_TEST(TestGroupedGridApplyGroupLayoutRestoresCollapsedStateByStableId);
    DXUI_RUN_TEST(TestGroupedGridKeyboardCollapsePreservesOnlyRowsOutsideCollapsedGroup);
    DXUI_RUN_TEST(TestGroupedGridCopySkipsRowsHiddenByCollapsedGroups);
    DXUI_RUN_TEST(TestGroupedGridVisibleRowOrdinalFollowsCollapsedLayout);
    DXUI_RUN_TEST(TestGroupedGridSelectAllSkipsRowsHiddenByCollapsedGroups);
    DXUI_RUN_TEST(TestGridApplyColumnLayoutCapturesDisplayOrderAndWidths);
    DXUI_RUN_TEST(TestGridApplyColumnLayoutAppendsMissingColumnsInModelOrder);
    DXUI_RUN_TEST(TestGridCopyUsesRestoredDisplayOrder);
    DXUI_RUN_TEST(TestGridHeaderDragReordersColumnsWithoutTriggeringSort);
    DXUI_RUN_TEST(TestGridHeaderClickStillRequestsSortWithoutReordering);
    DXUI_RUN_TEST(TestGridHeaderClickMovesSortGlyphToClickedColumn);
    DXUI_RUN_TEST(TestGridHeaderDragReorderRoundTripsThroughCapturedLayout);
    DXUI_RUN_TEST(TestGridHeaderDragReordersColumnToEarlierDisplaySlot);
    DXUI_RUN_TEST(TestGridRightClickInvokesContextMenuForHitRow);
    DXUI_RUN_TEST(TestGridRightClickPreservesExtendedSelectionForSelectedHitRow);
    DXUI_RUN_TEST(TestGridSelectionChangeNotifiesDelegateOnUserSelection);
    DXUI_RUN_TEST(TestGridPointerSelectionSurvivesDelegateModelSwap);
    DXUI_RUN_TEST(TestGridSelectionChangeReportsSenderGrid);
    DXUI_RUN_TEST(TestGridSelectionChangeNotifiesDelegateOnDataChange);
    DXUI_RUN_TEST(TestGridCellLayoutMetricsReserveSpaceForCheckboxAndBadge);
    DXUI_RUN_TEST(TestGridCellLayoutMetricsReserveSpaceForIconAndBadge);
    DXUI_RUN_TEST(TestGridIconTextUsesIconFontForFluentGlyphs);
    DXUI_RUN_TEST(TestGridIconIndexReservesIconSpaceWithoutTextGlyph);
    DXUI_RUN_TEST(TestGridCellLayoutMetricsCenterDedicatedColorSwatch);
    DXUI_RUN_TEST(TestGridDoubleClickActivatesHitRow);
    DXUI_RUN_TEST(TestDedicatedStateImageColumnCentersIconAndCollapsesText);
    DXUI_RUN_TEST(TestGridExplicitTooltipUsesCellTooltipText);
    DXUI_RUN_TEST(TestGridIgnoresExplicitTooltipThatRepeatsCellText);
    DXUI_RUN_TEST(TestGridIgnoresExplicitTooltipThatRepeatsIconBadgeCellText);
    DXUI_RUN_TEST(TestGridExplicitTooltipOverridesLongTextFallback);
    DXUI_RUN_TEST(TestGridLongTextFallbackTooltipRequiresClippedText);
    DXUI_RUN_TEST(TestGridRepeatedExplicitTooltipShowsWhenCellTextIsClipped);
    DXUI_RUN_TEST(TestGridMultilineTooltipFollowsPaintedLines);
    DXUI_RUN_TEST(TestGridScrolledSingleLineCaptionOffersTooltip);
    DXUI_RUN_TEST(TestGridFolderViewVisualModeUsesFolderLikeRowHighlights);
    DXUI_RUN_TEST(TestGridEmptyModelDoesNotHitTestBodyRows);
    DXUI_RUN_TEST(TestGridSetModelNullCancelsActiveColumnResize);
    DXUI_RUN_TEST(TestGridHeaderResizeZoneRequestsHorizontalResizeCursor);
    DXUI_RUN_TEST(TestGridHeaderBusyColumnAloneDoesNotAnimate);
    DXUI_RUN_TEST(TestGridCompactDensityShrinksHeaderAndRowMetrics);
    DXUI_RUN_TEST(TestGridEffectiveRowHeightBypassesDensityScaling);
    DXUI_RUN_TEST(TestGridRowMetricsClampToSegoeVariableBodyLineHeight);
}
