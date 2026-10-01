#pragma once
#include "../../Samples/ComplexUi/ComplexUiScene.h"
#include <DxUi/Embedded.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <psapi.h>
#include <realtimeapiset.h>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#pragma comment(lib, "psapi.lib")

// Opt-in measurement of how Grid selection membership scales (Specs/Core/Core_PerformanceAndResources.md):
//   DxUi.EmbeddedTests.exe --benchmark-grid-selection <report.json> [parts]
// It is synthetic and library-owned: no application model, settings or service. One report holds six parts, all of them unless
// `parts` names some of them, comma separated (for example `membership,retention`).
//   paint:      a Grid of 1,000 to 1,000,000 rows with rows selected, painted offscreen on WARP (time and UI-thread cycles of Prepare,
//               time of the whole frame). The Grid asks IsSelected once per visible row each paint.
//   selectionCost: the cost of that question inside a paint, isolated: one Grid painted in alternating blocks with a small and a
//               full selection that draw alike, so the difference of the two is what the longer selection costs.
//   membership: IsSelected on its own, per call, over selections of 0 to 1,000,000 ids, for an id that is selected and one that is not.
//   retention:  the C++ heap bytes a selection model holds after Ctrl+A over a list and after each way back from it (Clear, a click
//               on a row, a Shift+click on a near row, a data change that drops half the rows), counted exactly by the allocation
//               hook of EmbeddedTests.cpp, with the process's private bytes beside them.
//   mutators:   what each GridSelectionModel mutator costs in time and C++ heap bytes, so a faster test is not paid for there.
//   complexUiScene: how many rows the default complex-UI scene's Grid holds selected (none), which bounds what that scene can cost.
// It uses only the public GridSelectionModel and Grid interfaces, so the identical source measures any revision of the library.
// Fixture-only code, never used by the library. Check, Hr and the allocation counters come from EmbeddedTests.cpp.
namespace GridSelectionBenchmark
{
using Clock = std::chrono::steady_clock;

inline constexpr size_t kWarmupFrames   = 20;
inline constexpr size_t kFramesPerRound = 60;
inline constexpr size_t kRoundCount     = 5;
inline constexpr size_t kCostRoundCount = 10;

[[nodiscard]] inline double ElapsedMs(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

[[nodiscard]] inline double ElapsedNs(Clock::time_point start)
{
    return std::chrono::duration<double, std::nano>(Clock::now() - start).count();
}

[[nodiscard]] inline double Median(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

// The nearest-rank 95th percentile, as the complex-UI benchmark reads it (element 37 of 40 sorted frames).
[[nodiscard]] inline double Percentile95(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    return values[((values.size() * 95u) - 1u) / 100u];
}

[[nodiscard]] inline size_t PrivateBytes()
{
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    Check(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) != FALSE,
          "grid selection process memory");
    return memory.PrivateUsage;
}

// The multiplicative inverse of an odd number modulo 2^32, by Newton's iteration (each step doubles the correct low bits).
[[nodiscard]] constexpr uint32_t InverseOfOdd(uint32_t odd) noexcept
{
    uint32_t inverse = odd;
    for (int step = 0; step < 5; ++step)
        inverse *= 2u - odd * inverse;
    return inverse;
}

inline constexpr uint32_t kIdMultiplier = 2654435761u;
inline constexpr uint32_t kIdInverse    = InverseOfOdd(kIdMultiplier);
static_assert(static_cast<uint32_t>(kIdMultiplier* kIdInverse) == 1u, "the id scramble is invertible");

// A synthetic file list. Row i has the stable id Id(i), a fixed scramble of i (an odd multiplier is one-to-one on 32 bits), so a
// selection made in row order holds ids that are not ascending, as those of a sorted directory listing are not.
struct ScrambledRowsModel final : DxUi::IGridModel
{
    [[nodiscard]] static uint64_t Id(size_t row) noexcept
    {
        return static_cast<uint32_t>(static_cast<uint32_t>(row) * kIdMultiplier);
    }

    size_t rows = 0;

    size_t GetRowCount() const noexcept override
    {
        return rows;
    }
    size_t GetColumnCount() const noexcept override
    {
        return 4;
    }
    DxUi::GridColumnDesc GetColumn(size_t column) const override
    {
        return {std::to_wstring(column), L"Column " + std::to_wstring(column), 170};
    }
    void GetCellData(size_t row, size_t column, DxUi::GridCellData& cell) const override
    {
        cell.text = L"File " + std::to_wstring(row) + L"." + std::to_wstring(column);
    }
    uint64_t GetStableRowId(size_t row) const noexcept override
    {
        return Id(row);
    }
    std::optional<size_t> FindRowByStableId(uint64_t id) const noexcept override
    {
        if (id > UINT32_MAX)
            return std::nullopt;
        const size_t row = static_cast<uint32_t>(static_cast<uint32_t>(id) * kIdInverse);
        return row < rows ? std::optional<size_t>(row) : std::nullopt;
    }
};

// The ids of `rows` rows in row order: scrambled like ScrambledRowsModel's, or ascending.
[[nodiscard]] inline std::vector<uint64_t> RowIds(size_t rows, bool scrambled)
{
    std::vector<uint64_t> ids(rows);
    for (size_t row = 0; row < rows; ++row)
        ids[row] = scrambled ? ScrambledRowsModel::Id(row) : static_cast<uint64_t>(row);
    return ids;
}

enum class Shape
{
    None,
    Few,
    All
};
enum class Position
{
    Top,
    Middle,
    End
};

struct PaintScenario
{
    const char* name;
    size_t rows;
    Shape shape;
    Position position;
};

// A Grid in an embedded view on the supplied WARP device, painted with MarkDirty and Prepare (what a repaint costs the library)
// and then composited and completed (the whole frame).
class PaintFixture final
{
public:
    PaintFixture(GraphicsFixture& gpu, const std::shared_ptr<DxUi::GraphicsDevice>& graphics, size_t rows) : _gpu(gpu)
    {
        _model.rows = rows;
        Hr(_view.Attach(graphics), "grid selection benchmark view");
        auto theme          = DxUi::MakeDefaultThemePalette(true);
        theme.reducedMotion = true;
        _view.Controls().SetTheme(theme);
        auto root = std::make_unique<DxUi::Panel>();
        _grid     = root->AddChild<DxUi::Grid>();
        _grid->SetBounds(D2D1::RectF(8, 8, 1272, 712));
        _grid->SetModel(&_model);
        _view.Controls().SetRoot(std::move(root));

        D3D11_TEXTURE2D_DESC readDesc{};
        readDesc.Width = readDesc.Height = readDesc.MipLevels = readDesc.ArraySize = readDesc.SampleDesc.Count = 1;
        readDesc.Format                                                                                        = DXGI_FORMAT_B8G8R8A8_UNORM;
        readDesc.Usage                                                                                         = D3D11_USAGE_STAGING;
        readDesc.CPUAccessFlags                                                                                = D3D11_CPU_ACCESS_READ;
        Hr(gpu.device->CreateTexture2D(&readDesc, nullptr, _completion.put()), "grid selection benchmark completion texture");
    }

    [[nodiscard]] DxUi::Grid& Grid() noexcept
    {
        return *_grid;
    }

    // Returns the time of Prepare and of the whole frame, in milliseconds, and the cycles the UI thread spent in Prepare, in
    // millions. Cycles count only this thread's own work, so other processes taking the processor do not add to them.
    void Paint(double& prepareMs, double& frameMs, double& prepareMegacycles)
    {
        _view.MarkDirty();
        ULONG64 cyclesBefore = 0;
        ULONG64 cyclesAfter  = 0;
        Check(QueryThreadCycleTime(GetCurrentThread(), &cyclesBefore) != FALSE, "grid selection benchmark thread cycles");
        const auto start = Clock::now();
        Hr(_view.Prepare(1280, 720), "grid selection benchmark preparation");
        prepareMs = ElapsedMs(start);
        Check(QueryThreadCycleTime(GetCurrentThread(), &cyclesAfter) != FALSE, "grid selection benchmark thread cycles");
        prepareMegacycles = static_cast<double>(cyclesAfter - cyclesBefore) / 1.0e6;
        _gpu.Bind();
        Hr(_view.Composite(_gpu.context.get(), _gpu.Viewport()), "grid selection benchmark composition");
        const D3D11_BOX pixel{0, 0, 0, 1, 1, 1};
        _gpu.context->CopySubresourceRegion(_completion.get(), 0, 0, 0, 0, _gpu.target.get(), 0, &pixel);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Hr(_gpu.context->Map(_completion.get(), 0, D3D11_MAP_READ, 0, &mapped), "grid selection benchmark GPU completion");
        _gpu.context->Unmap(_completion.get(), 0);
        frameMs = ElapsedMs(start);
    }

private:
    GraphicsFixture& _gpu;
    // The view is destroyed before its borrowed model.
    ScrambledRowsModel _model;
    DxUi::EmbeddedHost _view;
    DxUi::Grid* _grid = nullptr;
    wil::com_ptr_nothrow<ID3D11Texture2D> _completion;
};

// One scenario: a Grid selected as Ctrl+A or a few clicks would select it, scrolled, and painted in five rounds of 60 dirty frames.
inline void MeasurePaint(GraphicsFixture& gpu, const std::shared_ptr<DxUi::GraphicsDevice>& graphics, const PaintScenario& scenario, std::ostream& output)
{
    PaintFixture fixture(gpu, graphics, scenario.rows);
    DxUi::Grid& grid                = fixture.Grid();
    const std::vector<uint64_t> ids = RowIds(scenario.rows, true);
    auto& selection                 = grid.GetSelectionModel();
    size_t selected                 = 0;
    if (scenario.shape == Shape::All)
    {
        selection.SetRange(ids, ids.front(), ids.back());
        selected = scenario.rows;
    }
    else if (scenario.shape == Shape::Few)
    {
        selection.SetSingle(ids[0]);
        selection.Toggle(ids[1]);
        selection.Toggle(ids[2]);
        selected = 3;
    }
    Check(selection.GetCount() == selected, "grid selection benchmark selects the rows it names");
    size_t shownRow = 0;
    if (scenario.position == Position::Middle)
        shownRow = scenario.rows / 2;
    else if (scenario.position == Position::End)
        shownRow = scenario.rows - 1;
    grid.EnsureRowVisible(shownRow);
    Check(grid.IsRowSelected(shownRow) == (scenario.shape == Shape::All || (scenario.shape == Shape::Few && shownRow < 3)),
          "grid selection benchmark shows a row in the state the scenario names");

    double prepareMs        = 0;
    double frameMs          = 0;
    double prepareMegacycle = 0;
    for (size_t frame = 0; frame < kWarmupFrames; ++frame)
        fixture.Paint(prepareMs, frameMs, prepareMegacycle);
    const size_t visibleRows = grid.GetVisibleRowCount();

    output << "{\"name\":\"" << scenario.name << "\",\"rows\":" << scenario.rows << ",\"selected\":" << selected << ",\"visibleRows\":" << visibleRows
           << ",\"rounds\":[";
    for (size_t round = 0; round < kRoundCount; ++round)
    {
        std::vector<double> prepare(kFramesPerRound);
        std::vector<double> frames(kFramesPerRound);
        std::vector<double> megacycles(kFramesPerRound);
        size_t cppAllocations = 0;
        size_t cppBytes       = 0;
        for (size_t frame = 0; frame < kFramesPerRound; ++frame)
        {
            allocations      = 0;
            allocationBytes  = 0;
            countAllocations = true;
            fixture.Paint(prepare[frame], frames[frame], megacycles[frame]);
            countAllocations = false;
            cppAllocations += allocations;
            cppBytes += allocationBytes;
        }
        if (round)
            output << ',';
        output << "{\"prepareP50Ms\":" << Median(prepare) << ",\"prepareP95Ms\":" << Percentile95(prepare) << ",\"prepareMcycP50\":" << Median(megacycles)
               << ",\"prepareMcycP95\":" << Percentile95(megacycles) << ",\"frameP50Ms\":" << Median(frames) << ",\"frameP95Ms\":" << Percentile95(frames)
               << ",\"cppAllocations\":" << cppAllocations << ",\"cppBytes\":" << cppBytes << ",\"privateBytes\":" << PrivateBytes() << '}';
    }
    output << "]}";
    std::cout << "Grid selection paint " << scenario.name << ": " << visibleRows << " visible rows\n";
}

// What IsSelected costs inside a paint, isolated: one Grid and view, scrolled to the middle of `rows` rows, painted in blocks of
// 30 frames that alternate between two selections that draw alike (every visible row is selected in both) and differ only in size,
// the 81 rows around the view and every row. Whatever the machine does to the process while it runs happens to both blocks, so the
// difference of the two medians is the cost of the longer selection; the first five frames after each change are not counted.
inline void MeasureSelectionCost(GraphicsFixture& gpu, const std::shared_ptr<DxUi::GraphicsDevice>& graphics, size_t rows, std::ostream& output)
{
    PaintFixture fixture(gpu, graphics, rows);
    DxUi::Grid& grid                = fixture.Grid();
    auto& selection                 = grid.GetSelectionModel();
    const std::vector<uint64_t> ids = RowIds(rows, true);
    const size_t shownRow           = rows / 2;
    grid.EnsureRowVisible(shownRow);
    const size_t nearFirst = shownRow >= 40 ? shownRow - 40 : 0;
    const size_t nearLast  = (std::min)(rows - 1, shownRow + 40);
    const std::vector<uint64_t> nearIds(ids.begin() + static_cast<std::ptrdiff_t>(nearFirst), ids.begin() + static_cast<std::ptrdiff_t>(nearLast) + 1);
    const auto select = [&](bool every)
    {
        if (every)
            selection.SetRange(ids, ids.front(), ids.back());
        else
            selection.SetRange(nearIds, nearIds.front(), nearIds.back());
        for (size_t row = shownRow >= 23 ? shownRow - 23 : 0; row <= shownRow; ++row)
            Check(grid.IsRowSelected(row), "grid selection benchmark selects every visible row in both selections");
    };

    double prepareMs        = 0;
    double frameMs          = 0;
    double prepareMegacycle = 0;
    select(false);
    for (size_t frame = 0; frame < kWarmupFrames; ++frame)
        fixture.Paint(prepareMs, frameMs, prepareMegacycle);

    constexpr size_t kBlocksPerRound = 4;
    constexpr size_t kBlockFrames    = 30;
    constexpr size_t kSettleFrames   = 5;
    output << "{\"rows\":" << rows << ",\"nearRows\":" << nearIds.size() << ",\"blockFrames\":" << kBlockFrames << ",\"settleFrames\":" << kSettleFrames
           << ",\"rounds\":[";
    for (size_t round = 0; round < kCostRoundCount; ++round)
    {
        std::vector<double> smallCycles;
        std::vector<double> everyCycles;
        std::vector<double> smallMs;
        std::vector<double> everyMs;
        for (size_t block = 0; block < kBlocksPerRound; ++block)
        {
            // Rounds start with alternate selections, so neither selection is always the first in its round.
            const bool every = ((block + round) % 2u) == 1u;
            select(every);
            for (size_t frame = 0; frame < kBlockFrames; ++frame)
            {
                fixture.Paint(prepareMs, frameMs, prepareMegacycle);
                if (frame < kSettleFrames)
                    continue;
                (every ? everyCycles : smallCycles).push_back(prepareMegacycle);
                (every ? everyMs : smallMs).push_back(prepareMs);
            }
        }
        if (round)
            output << ',';
        output << "{\"smallMcycP50\":" << Median(smallCycles) << ",\"everyMcycP50\":" << Median(everyCycles) << ",\"smallP50Ms\":" << Median(smallMs)
               << ",\"everyP50Ms\":" << Median(everyMs) << ",\"costMcyc\":" << Median(everyCycles) - Median(smallCycles)
               << ",\"costMs\":" << Median(everyMs) - Median(smallMs) << '}';
    }
    output << "]}";
    std::cout << "Grid selection cost of IsSelected in a paint over " << rows << " rows\n";
}

// Nanoseconds per IsSelected call over `queries`, each expected to answer `expectedHits` times in a pass: the median of seven
// timed passes of a repetition count that makes one pass last about 20 ms.
[[nodiscard]] inline double NanosecondsPerCall(const DxUi::GridSelectionModel& model, std::span<const uint64_t> queries, size_t expectedHits)
{
    static volatile size_t sink = 0;
    const auto pass             = [&](size_t repetitions)
    {
        size_t hits      = 0;
        const auto start = Clock::now();
        for (size_t repetition = 0; repetition < repetitions; ++repetition)
            for (const uint64_t id : queries)
                hits += model.IsSelected(id) ? 1u : 0u;
        const double ns = ElapsedNs(start);
        sink            = sink + hits;
        Check(hits == repetitions * expectedHits, "grid selection membership answers as the selection says");
        return ns / static_cast<double>(repetitions * queries.size());
    };
    const double calibration = pass(1);
    const size_t repetitions =
        static_cast<size_t>(std::clamp(20'000'000.0 / (std::max)(calibration * static_cast<double>(queries.size()), 1.0), 1.0, 1'000'000.0));
    std::vector<double> samples;
    for (size_t sample = 0; sample < 7; ++sample)
        samples.push_back(pass(repetitions));
    return Median(samples);
}

// IsSelected against a selection of `selected` ascending ids made as Ctrl+A makes it: a batch of 32 consecutive ids from the
// middle of it (as many rows as a screen shows) that are all selected, and 32 ids above it that are not.
inline void MeasureMembership(size_t selected, std::ostream& output)
{
    const std::vector<uint64_t> ids = RowIds(selected, false);
    DxUi::GridSelectionModel model;
    if (selected)
        model.SetRange(ids, ids.front(), ids.back());
    Check(model.GetCount() == selected, "grid selection membership fixture");
    std::vector<uint64_t> hitQueries;
    const size_t hitCount = (std::min)(selected, size_t{32});
    for (size_t index = 0; index < hitCount; ++index)
        hitQueries.push_back(ids[(selected / 2) - (hitCount / 2) + index]);
    std::vector<uint64_t> missQueries;
    for (size_t index = 0; index < 32; ++index)
        missQueries.push_back(selected + 1000u + index);
    output << "{\"selected\":" << selected << ",\"hitNs\":";
    if (hitQueries.empty())
        output << "null";
    else
        output << NanosecondsPerCall(model, hitQueries, hitQueries.size());
    output << ",\"missNs\":" << NanosecondsPerCall(model, missQueries, 0) << '}';
}

struct OperationCost
{
    double ms         = 0;
    size_t allocation = 0;
    size_t bytes      = 0;
};

// Runs `operation` once, counting its C++ heap calls and the bytes they ask for (only the operation is inside the count).
template <typename Operation> [[nodiscard]] OperationCost CostOf(Operation&& operation)
{
    allocations      = 0;
    allocationBytes  = 0;
    countAllocations = true;
    const auto start = Clock::now();
    operation();
    const double ms  = ElapsedMs(start);
    countAllocations = false;
    return {ms, allocations, allocationBytes};
}

[[nodiscard]] inline OperationCost MedianCost(std::vector<OperationCost> costs)
{
    std::sort(costs.begin(), costs.end(), [](const OperationCost& left, const OperationCost& right) { return left.ms < right.ms; });
    return costs[costs.size() / 2];
}

inline void WriteCost(std::ostream& output, const char* name, size_t rows, bool scrambled, const OperationCost& cost, size_t privateGrowth)
{
    output << "{\"name\":\"" << name << "\",\"rows\":" << rows << ",\"scrambledIds\":" << (scrambled ? "true" : "false") << ",\"ms\":" << cost.ms
           << ",\"cppAllocations\":" << cost.allocation << ",\"cppBytes\":" << cost.bytes << ",\"privateGrowthBytes\":" << privateGrowth << '}';
}

// What the mutators cost for a selection of `rows` ids, in time and heap bytes. A cold call is the first one on a new model,
// which allocates; warm calls reuse what the model already holds (the median of nine). Ctrl+A is SetRange over every row, a
// data change is PreserveOrdered over every row (nothing dropped) or over half of them, and a Ctrl+click is a Toggle.
inline void MeasureMutators(size_t rows, bool scrambled, std::ostream& output)
{
    const std::vector<uint64_t> ids = RowIds(rows, scrambled);
    std::vector<uint64_t> half;
    for (size_t index = 0; index < rows; index += 2)
        half.push_back(ids[index]);
    const auto selectAll     = [&](DxUi::GridSelectionModel& model) { model.SetRange(ids, ids.front(), ids.back()); };
    constexpr size_t repeats = 9;

    DxUi::GridSelectionModel coldModel;
    const size_t privateBefore = PrivateBytes();
    const OperationCost cold   = CostOf([&] { selectAll(coldModel); });
    const size_t privateAfter  = PrivateBytes();
    Check(coldModel.GetCount() == rows, "grid selection benchmark select all");
    WriteCost(output, "SetRange all, cold", rows, scrambled, cold, privateAfter > privateBefore ? privateAfter - privateBefore : 0);
    output << ',';

    std::vector<OperationCost> costs;
    for (size_t repeat = 0; repeat < repeats; ++repeat)
    {
        coldModel.Clear();
        costs.push_back(CostOf([&] { selectAll(coldModel); }));
    }
    WriteCost(output, "SetRange all, warm", rows, scrambled, MedianCost(costs), 0);
    output << ',';

    // Ctrl+A again over a selection that already is every row: its buffers are the right size, so nothing has to grow.
    costs.clear();
    for (size_t repeat = 0; repeat < repeats; ++repeat)
        costs.push_back(CostOf([&] { selectAll(coldModel); }));
    Check(coldModel.GetCount() == rows, "grid selection benchmark select all again");
    WriteCost(output, "SetRange all, over the same selection", rows, scrambled, MedianCost(costs), 0);
    output << ',';

    costs.clear();
    for (size_t repeat = 0; repeat < repeats; ++repeat)
        costs.push_back(CostOf([&] { coldModel.PreserveOrdered(ids); }));
    Check(coldModel.GetCount() == rows, "grid selection benchmark preserve everything");
    WriteCost(output, "PreserveOrdered all kept", rows, scrambled, MedianCost(costs), 0);
    output << ',';

    costs.clear();
    for (size_t repeat = 0; repeat < repeats; ++repeat)
    {
        selectAll(coldModel);
        costs.push_back(CostOf([&] { coldModel.PreserveOrdered(half); }));
    }
    Check(coldModel.GetCount() == half.size(), "grid selection benchmark preserve half");
    WriteCost(output, "PreserveOrdered half kept", rows, scrambled, MedianCost(costs), 0);
    output << ',';

    // The ways back from Ctrl+A, each timed on a model that has just selected every row: Escape clears the selection, and a click on
    // a row makes that row the only one selected. What either leaves in the heap is the retention part's question.
    costs.clear();
    for (size_t repeat = 0; repeat < repeats; ++repeat)
    {
        selectAll(coldModel);
        costs.push_back(CostOf([&] { coldModel.Clear(); }));
    }
    Check(coldModel.GetCount() == 0, "grid selection benchmark clear after select all");
    WriteCost(output, "Clear after select all", rows, scrambled, MedianCost(costs), 0);
    output << ',';

    costs.clear();
    for (size_t repeat = 0; repeat < repeats; ++repeat)
    {
        selectAll(coldModel);
        costs.push_back(CostOf([&] { coldModel.SetSingle(ids[rows / 2]); }));
    }
    Check(coldModel.GetCount() == 1, "grid selection benchmark click after select all");
    WriteCost(output, "SetSingle after select all", rows, scrambled, MedianCost(costs), 0);
    output << ',';

    // Ctrl+click on one row in the middle: it is added to a selection of every other row, then removed again.
    std::vector<uint64_t> others(ids);
    const uint64_t clicked = others[rows / 2];
    others.erase(others.begin() + static_cast<std::ptrdiff_t>(rows / 2));
    coldModel.Clear();
    coldModel.SetRange(others, others.front(), others.back());
    const size_t pairs = (std::max)(size_t{10}, size_t{2'000'000} / rows);
    std::vector<double> perPair;
    for (size_t repeat = 0; repeat < repeats; ++repeat)
    {
        const auto start = Clock::now();
        for (size_t pair = 0; pair < pairs; ++pair)
        {
            coldModel.Toggle(clicked);
            coldModel.Toggle(clicked);
        }
        perPair.push_back(ElapsedMs(start) / static_cast<double>(pairs));
    }
    Check(coldModel.GetCount() == rows - 1 && ! coldModel.IsSelected(clicked), "grid selection benchmark toggle pairs restore the selection");
    WriteCost(output, "Toggle one row on and off", rows, scrambled, OperationCost{Median(perPair), 0, 0}, 0);
}

// The C++ heap bytes one selection model holds at two moments: after Ctrl+A over a list, and after a way back from it.
struct HeldBytes
{
    ptrdiff_t afterSelectAll = 0;
    ptrdiff_t afterWayBack   = 0;
};

// Builds a model inside the allocation hook's live count, makes Ctrl+A over `ids` and takes `wayBack`. The count reads the sizes of
// the heap blocks the model's buffers occupy (no more than the model owns), so the second figure is what the model still holds.
template <typename WayBack> [[nodiscard]] HeldBytes HeldAfterSelectAll(const std::vector<uint64_t>& ids, WayBack&& wayBack)
{
    DxUi::GridSelectionModel model;
    liveBytes      = 0;
    countLiveBytes = true;
    model.SetRange(ids, ids.front(), ids.back());
    const ptrdiff_t selected = liveBytes;
    wayBack(model);
    const ptrdiff_t kept = liveBytes;
    countLiveBytes       = false;
    return {selected, kept};
}

// What a selection of `count` ids leaves in the heap once the user has moved on from it, in bytes of C++ heap held by the model:
// after Ctrl+A, then after Escape (Clear), a click on a row (SetSingle), a Shift+click on a row 99 below the anchor (SetRange over a
// short range) and a data change that drops every other row (PreserveOrdered). Every way back starts from a new model. The ids are
// not ascending, as a scrambled model's are. The process's private bytes (the operating system's view of the same events, which the
// heap's own policy about returning memory colours) are recorded beside them for the Clear case.
inline void MeasureRetention(size_t count, std::ostream& output)
{
    const std::vector<uint64_t> ids = RowIds(count, true);
    std::vector<uint64_t> half;
    for (size_t index = 0; index < count; index += 2)
        half.push_back(ids[index]);
    const size_t clickedRow = count / 2;
    const size_t shortLast  = (std::min)(count - 1, clickedRow + 99u);

    const HeldBytes cleared    = HeldAfterSelectAll(ids, [](DxUi::GridSelectionModel& model) { model.Clear(); });
    const HeldBytes clicked    = HeldAfterSelectAll(ids, [&](DxUi::GridSelectionModel& model) { model.SetSingle(ids[clickedRow]); });
    const HeldBytes shortRange = HeldAfterSelectAll(ids, [&](DxUi::GridSelectionModel& model) { model.SetRange(ids, ids[clickedRow], ids[shortLast]); });
    const HeldBytes preserved  = HeldAfterSelectAll(ids, [&](DxUi::GridSelectionModel& model) { model.PreserveOrdered(half); });
    Check(clicked.afterSelectAll == cleared.afterSelectAll && shortRange.afterSelectAll == cleared.afterSelectAll &&
              preserved.afterSelectAll == cleared.afterSelectAll,
          "grid selection retention starts every way back from the same selection");

    DxUi::GridSelectionModel model;
    const size_t privateBefore = PrivateBytes();
    model.SetRange(ids, ids.front(), ids.back());
    const size_t privateSelected = PrivateBytes();
    model.Clear();
    const size_t privateCleared = PrivateBytes();
    Check(model.GetCount() == 0, "grid selection retention clears the model");

    output << "{\"ids\":" << count << ",\"shortRangeIds\":" << (shortLast - clickedRow + 1u) << ",\"keptHalfIds\":" << half.size()
           << ",\"selectAllBytes\":" << cleared.afterSelectAll << ",\"afterClearBytes\":" << cleared.afterWayBack
           << ",\"afterSetSingleBytes\":" << clicked.afterWayBack << ",\"afterShortRangeBytes\":" << shortRange.afterWayBack
           << ",\"afterPreserveHalfBytes\":" << preserved.afterWayBack
           << ",\"privateAfterSelectAllBytes\":" << static_cast<long long>(privateSelected) - static_cast<long long>(privateBefore)
           << ",\"privateAfterClearBytes\":" << static_cast<long long>(privateCleared) - static_cast<long long>(privateBefore) << '}';
    std::cout << "Grid selection retention measured for " << count << " ids\n";
}

// Whether the comma separated `parts` names `part`; an empty `parts` names every part.
[[nodiscard]] inline bool Wants(std::wstring_view parts, std::wstring_view part)
{
    if (parts.empty())
        return true;
    size_t start = 0;
    while (start <= parts.size())
    {
        const size_t end = parts.find(L',', start);
        if (parts.substr(start, end == std::wstring_view::npos ? std::wstring_view::npos : end - start) == part)
            return true;
        if (end == std::wstring_view::npos)
            break;
        start = end + 1u;
    }
    return false;
}

// What the Grid of the default complex-UI scene (Samples/ComplexUi, the scene performance.ps1 measures) holds selected after the
// benchmark's warm-up and a round of dirty frames. The scene selects nothing, so its paints ask IsSelected of an empty selection.
inline void MeasureComplexUiSelection(GraphicsFixture& gpu, std::ostream& output)
{
    ComplexUiScene scene;
    Hr(scene.Initialize(gpu.device.get()), "grid selection benchmark complex-UI scene");
    for (size_t frame = 0; frame < kWarmupFrames + kFramesPerRound; ++frame)
    {
        scene.Update(frame);
        Hr(scene.view.Prepare(1280, 720), "grid selection benchmark complex-UI preparation");
    }
    output << "{\"gridRows\":" << scene.model.names.size() << ",\"visibleRows\":" << scene.grid->GetVisibleRowCount()
           << ",\"selectedRows\":" << scene.grid->GetSelectionModel().GetCount() << '}';
}

inline void Run(const wchar_t* outputPath, std::wstring_view parts = {})
{
    GraphicsFixture gpu;
    gpu.width  = 1280;
    gpu.height = 720;
    Hr(gpu.Create(), "grid selection benchmark WARP device");
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "grid selection benchmark graphics pool");

    constexpr std::array<std::wstring_view, 6> partNames{L"paint", L"selectionCost", L"membership", L"retention", L"mutators", L"complexUiScene"};
    // A part named wrongly would measure nothing, and the report would show it only by the part's absence.
    for (size_t start = 0; ! parts.empty();)
    {
        const size_t end                  = parts.find(L',', start);
        const std::wstring_view requested = parts.substr(start, end == std::wstring_view::npos ? std::wstring_view::npos : end - start);
        Check(std::ranges::find(partNames, requested) != partNames.end(), "grid selection benchmark names only parts it has");
        if (end == std::wstring_view::npos)
            break;
        start = end + 1u;
    }
    std::string measured;
    for (const std::wstring_view name : partNames)
    {
        if (! Wants(parts, name))
            continue;
        measured += measured.empty() ? "\"" : ",\"";
        for (const wchar_t letter : name)
            measured += static_cast<char>(letter);
        measured += '"';
    }

    std::ofstream output{std::filesystem::path(outputPath)};
    Check(bool(output), "grid selection benchmark output file");
    output << std::setprecision(10) << "{\"compiler\":" << _MSC_FULL_VER << ",\"fixture\":\"dxui-grid-selection-v2\",\"renderer\":\"WARP\",\"width\":1280,"
           << "\"height\":720,\"dpi\":96,\"warmupFrames\":" << kWarmupFrames << ",\"framesPerRound\":" << kFramesPerRound << ",\"roundCount\":" << kRoundCount
           << ",\"selectionModelBytes\":" << sizeof(DxUi::GridSelectionModel) << ",\"parts\":[" << measured << ']';

    if (Wants(parts, L"paint"))
    {
        output << ",\"paint\":[";
        // The first scenario also warms the process (fonts, the graphics pool), so a throwaway pass runs first.
        {
            std::ostringstream discarded;
            MeasurePaint(gpu, graphics, PaintScenario{"warm-up", 1'000, Shape::All, Position::Middle}, discarded);
        }
        constexpr std::array scenarios{
            PaintScenario{"all-1k-middle", 1'000, Shape::All, Position::Middle},
            PaintScenario{"all-20k-top", 20'000, Shape::All, Position::Top},
            PaintScenario{"all-20k-middle", 20'000, Shape::All, Position::Middle},
            PaintScenario{"all-20k-end", 20'000, Shape::All, Position::End},
            PaintScenario{"all-200k-middle", 200'000, Shape::All, Position::Middle},
            PaintScenario{"all-1m-middle", 1'000'000, Shape::All, Position::Middle},
            PaintScenario{"none-20k-middle", 20'000, Shape::None, Position::Middle},
            PaintScenario{"few-1k-top", 1'000, Shape::Few, Position::Top},
        };
        for (size_t index = 0; index < scenarios.size(); ++index)
        {
            if (index)
                output << ',';
            MeasurePaint(gpu, graphics, scenarios[index], output);
        }
        output << ']';
    }

    if (Wants(parts, L"selectionCost"))
    {
        output << ",\"selectionCost\":[";
        constexpr std::array costRows{size_t{20'000}, size_t{200'000}, size_t{1'000'000}};
        for (size_t index = 0; index < costRows.size(); ++index)
        {
            if (index)
                output << ',';
            MeasureSelectionCost(gpu, graphics, costRows[index], output);
        }
        output << ']';
    }

    if (Wants(parts, L"membership"))
    {
        output << ",\"membership\":[";
        constexpr std::array selectionSizes{size_t{0},     size_t{1},      size_t{2},       size_t{3},        size_t{4},   size_t{6},
                                            size_t{8},     size_t{12},     size_t{16},      size_t{24},       size_t{32},  size_t{48},
                                            size_t{64},    size_t{96},     size_t{128},     size_t{192},      size_t{256}, size_t{512},
                                            size_t{1'000}, size_t{20'000}, size_t{200'000}, size_t{1'000'000}};
        for (size_t index = 0; index < selectionSizes.size(); ++index)
        {
            if (index)
                output << ',';
            MeasureMembership(selectionSizes[index], output);
        }
        std::cout << "Grid selection membership measured for " << selectionSizes.size() << " selection sizes\n";
        output << ']';
    }

    if (Wants(parts, L"retention"))
    {
        output << ",\"retention\":[";
        constexpr std::array retentionIds{size_t{1'000}, size_t{4'096}, size_t{4'097}, size_t{20'000}, size_t{200'000}};
        for (size_t index = 0; index < retentionIds.size(); ++index)
        {
            if (index)
                output << ',';
            MeasureRetention(retentionIds[index], output);
        }
        output << ']';
    }

    if (Wants(parts, L"mutators"))
    {
        output << ",\"mutators\":[";
        bool first = true;
        for (const size_t rows : {size_t{1'000}, size_t{20'000}, size_t{200'000}})
        {
            for (const bool scrambled : {false, true})
            {
                if (! first)
                    output << ',';
                first = false;
                MeasureMutators(rows, scrambled, output);
            }
        }
        output << ']';
    }

    if (Wants(parts, L"complexUiScene"))
    {
        output << ",\"complexUiScene\":";
        MeasureComplexUiSelection(gpu, output);
    }
    output << "}\n";
    output.close();
    Check(bool(output), "grid selection benchmark report written");
    std::cout << "Grid selection report written\n";
}
} // namespace GridSelectionBenchmark
