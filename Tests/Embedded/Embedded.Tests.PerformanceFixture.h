#pragma once
// Performance fixtures: the host-neutral model (Specs/Testing/Testing_Validation.md, "Performance fixtures"; plan item
// W0.4). A fixture is defined once, in its own header (Embedded.Tests.PerformanceFixtureFx<N>.h), as a Definition and a
// Scenario. Every consumer drives that one definition: the counter and memory receipt, the hardware timing harness
// (FrameTiming), the determinism gate, the incremental-versus-fresh pixel oracle and item gate tests. The two runners live
// in the executables' entry points: Embedded.Tests.PerformanceFixtures.h (EmbeddedHost, with the C++ allocation sink) and
// Controls.Tests.PerformanceFixtures.cpp (WindowHost). This header compiles in both test executables, so a fixture header
// uses only DxUi, the standard library and what is declared here: never Check, Require, GraphicsFixture or a host window.
#include <DxUi/DxUi.h>
#include <DxUi/Embedded.h>

#include "Support/WorkCounters.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Scenarios and records are owned in place and never copied. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4623 4625 4626 5026 5027)
namespace PerformanceFixtures
{
// The hosts a fixture runs on. A runner skips a fixture that does not name its host.
enum class Host : uint8_t
{
    Embedded = 1u,
    Window   = 2u,
};
using HostMask                       = uint8_t;
inline constexpr HostMask kEmbedded  = 1u;
inline constexpr HostMask kWindow    = 2u;
inline constexpr HostMask kBothHosts = kEmbedded | kWindow;

[[nodiscard]] constexpr bool Supports(HostMask hosts, Host host) noexcept
{
    return (hosts & static_cast<HostMask>(host)) != 0u;
}
[[nodiscard]] constexpr std::string_view HostName(Host host) noexcept
{
    return host == Host::Embedded ? "embedded" : "window";
}

// One size of the scene: the device-pixel surface or client area and its DPI. variants[0] is the default. A hardware-only
// variant is declared for hardware timing and receipts; WARP runs, the determinism gate among them, leave it out. A
// receipt-only variant is a full-size scene too heavy for the ordinary suites: receipts and hardware timing run it on any
// adapter, and the suites' determinism gate leaves it out (its own runs are compared with performance-fixtures.ps1).
struct Variant
{
    std::string_view name; // stable lowercase id used on command lines and in receipt names: "default", "uhd-192dpi"
    UINT width        = 0u;
    UINT height       = 0u;
    float dpi         = 96.0f;
    bool hardwareOnly = false;
    bool receiptOnly  = false;
};

// A warm-up phase settles first-use state (device resources, caches, process-wide DirectWrite state): its counters are
// recorded but two runs in one process may differ there, so the determinism gate compares measured phases only.
enum class PhaseKind : uint8_t
{
    Warmup,
    Measured,
};
struct Phase
{
    std::string_view name; // lowercase id: "warm", "clean", "dirty", "hover"
    size_t steps   = 0u;   // steps the receipt and the determinism gate record; a step is one action and one frame
    PhaseKind kind = PhaseKind::Measured;
    std::string_view description;
};

// What a step may do to its host besides changing the controls it built: input in device pixels of the variant, the
// frozen fixture clock and the host size. Both runners implement it; nothing here waits, sleeps or reads the real clock.
class HostPort
{
public:
    HostPort()                           = default;
    HostPort(const HostPort&)            = delete;
    HostPort& operator=(const HostPort&) = delete;
    HostPort(HostPort&&)                 = delete;
    HostPort& operator=(HostPort&&)      = delete;
    virtual ~HostPort()                  = default;
    // Pointer input; a WindowHost receives the matching mouse message. Returns whether the host handled it.
    virtual bool Pointer(const DxUi::PointerEvent& event) = 0;
    // Keyboard input. On a WindowHost, modifier state comes from the real keyboard, so a window step sends no modifiers.
    virtual bool Key(UINT virtualKey, bool down, UINT modifiers = 0u) = 0;
    virtual bool Character(wchar_t character, UINT modifiers = 0u)    = 0;
    // Advances the fixture clock, which starts at kClockOriginMs for every run, and delivers one animation tick at the new
    // time when the host has animation pending. The only source of time a step sees.
    virtual void AdvanceTime(uint64_t milliseconds) = 0;
    // Changes the host's pixel size and DPI; the next frame renders at the new size.
    virtual void Resize(UINT width, UINT height, float dpi) = 0;
    [[nodiscard]] virtual uint64_t NowMs() const noexcept   = 0;
};
inline constexpr uint64_t kClockOriginMs = 1'000'000u;

struct Context
{
    Host host;
    const Variant& variant;
    DxUi::ControlHost& controls; // the host's control tree: Build sets its root
    HostPort& port;
};

// One live instance of a fixture on one host. The runner owns it and its host and destroys the host first, so a scenario
// may own the models its controls borrow. Build and Step never render: the runner renders exactly one frame after every
// step (none after Build, so a fixture whose steps need prepared layout, such as hit testing, starts with a warm-up
// phase). The state after a step depends only on the steps applied before it and the frames rendered after each, never on
// the real clock or randomness, which lets the pixel oracle replay any prefix on a new instance and compare.
class Scenario
{
public:
    Scenario()                           = default;
    Scenario(const Scenario&)            = delete;
    Scenario& operator=(const Scenario&) = delete;
    Scenario(Scenario&&)                 = delete;
    Scenario& operator=(Scenario&&)      = delete;
    virtual ~Scenario()                  = default;
    // Only for a scene that owns its EmbeddedHost and is shared verbatim with a benchmark (FX5's ComplexUiScene): attach and
    // build it on `device` and return its host. The runner then uses that host and calls Build with it. Default: none, and
    // the runner supplies an attached host with the fixture's theme.
    virtual DxUi::EmbeddedHost* AttachOwnEmbeddedHost(ID3D11Device* /*device*/, HRESULT& result)
    {
        result = S_OK;
        return nullptr;
    }
    virtual HRESULT Build(Context& context) = 0;
    // Applies step `step` of phase `phase` (an index into Definition::phases). Every step index must be accepted, also beyond
    // the phase's declared count: the timing harness plays the timed phase for as many frames as it measures.
    virtual void Step(Context& context, size_t phase, size_t step) = 0;
};

// The fixture as the plan declares it. Fixture headers define one constexpr Definition each; see the catalog.
struct Definition
{
    std::string_view id;    // "FX5"
    std::string_view title; // one line
    std::string_view scenario;
    std::span<const std::string_view> budgets; // plan budgets this fixture proves: "G2", "G3"
    HostMask hosts = 0u;
    std::span<const Variant> variants;
    std::span<const Phase> phases;
    std::string_view timedPhase;               // the phase performance-timing.ps1 -Phase Dirty plays; -Phase Clean renders unchanged frames
    std::span<const std::string_view> sources; // repository paths of the scene; the receipt hashes them with the framework's own
    bool reducedMotion                    = true;
    std::unique_ptr<Scenario> (*create)() = nullptr; // nullptr: declared by the plan, not built yet
    // Nonzero: the embedded runner also counts, per step, the C++ allocations of at least this many bytes
    // ("cppLargeAllocations"), such as copies of a document of that size; 0 leaves that counter out of the receipt.
    size_t largeAllocationBytes = 0u;
    // Each step's frame repeats, within the same step, until the host needs no further preparation: one step is then one
    // action with every owner turn of cooperative preparation it causes (FX8's rich document). Honored by the embedded
    // runner; a fixture that sets it runs on the embedded host only.
    bool settlePreparation = false;
    // Takes the foreground: only the interactive run executes it (Tests/README.md, "Performance fixtures"). Its own runner in
    // DxUi.ControlTests.exe runs it, under test.ps1 -Interactive or performance-fixtures.ps1 -Interactive, never through a
    // Scenario, so `create` stays nullptr. Every other runner, gate, receipt command and the timing harness skips it, says
    // "<id>: interactive, not run" and never fails on it.
    bool interactiveOnly = false;
    // The timed phase's Step(phase, n) does the same kind of work for every n past its declared step count (FX5's
    // ComplexUiScene::Update is periodic). Otherwise performance-timing.ps1 times only the declared steps: after the last
    // one, the scene is rebuilt and the phases before the timed phase replayed, outside the timed frames, and the next
    // frame plays step 0 again. A wheel phase that scrolled on past its declared steps would reach the end of its list
    // and time thousands of frames that change nothing.
    bool timedPhaseRepeats = false;

    [[nodiscard]] constexpr bool Built() const noexcept
    {
        return (create != nullptr || interactiveOnly) && ! phases.empty() && ! variants.empty() && hosts != 0u;
    }
    [[nodiscard]] constexpr const Variant* FindVariant(std::string_view name) const noexcept
    {
        for (const Variant& variant : variants)
            if (variant.name == name)
                return &variant;
        return nullptr;
    }
    [[nodiscard]] constexpr std::optional<size_t> FindPhase(std::string_view name) const noexcept
    {
        for (size_t index = 0u; index < phases.size(); ++index)
            if (phases[index].name == name)
                return index;
        return std::nullopt;
    }
};

// ---- Replay ----------------------------------------------------------------------------------------------------------
// A fixture's steps as one sequence, phase after phase: what the incremental-versus-fresh oracle (W0.5,
// Support.Tests.IncrementalParity.h) replays. Its ParityScript build function builds the scenario on the scene's host,
// and its step function maps oracle step k (1-based) to StepAt(definition, k - 1) and applies it through a HostPort over
// the oracle scene. A fixture that owns its host (FX5) is replayed by its own runner instead.
struct PhaseStep
{
    size_t phase = 0u;
    size_t step  = 0u;
};
[[nodiscard]] constexpr size_t TotalSteps(const Definition& definition) noexcept
{
    size_t total = 0u;
    for (const Phase& phase : definition.phases)
        total += phase.steps;
    return total;
}
// The phase and step at `index` (0-based) of the whole sequence; nullopt past its end.
[[nodiscard]] constexpr std::optional<PhaseStep> StepAt(const Definition& definition, size_t index) noexcept
{
    for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
    {
        if (index < definition.phases[phase].steps)
            return PhaseStep{phase, index};
        index -= definition.phases[phase].steps;
    }
    return std::nullopt;
}

// ---- Records ---------------------------------------------------------------------------------------------------------

// The work of one step: the action and its frame. cppAllocations is the C++ allocation sink, counted only where the
// executable replaces the allocation functions (the Embedded suite).
struct StepRecord
{
    DxUi::Detail::WorkCounters work{};
    uint64_t cppAllocations      = 0u;
    uint64_t cppLargeAllocations = 0u; // of at least Definition::largeAllocationBytes; 0 where not counted
};

// Process and host memory at one point of a run (the W0.3 fields). Absent parts are not measurable on that host or adapter.
struct MemorySample
{
    std::string_view at;    // "device", "built", "phase" (after the named phase's last step), "hidden"
    std::string_view phase; // for "phase" samples
    uint64_t privateBytes    = 0u;
    uint64_t workingSetBytes = 0u;
    std::optional<int64_t> trackedLiveBytes; // live C++ bytes since the scene build began (Embedded suite only)
    std::optional<DxUi::EmbeddedStatistics> retained;
    bool video                        = false; // a hardware adapter reported video memory
    uint64_t localVideoUsageBytes     = 0u;
    uint64_t localVideoBudgetBytes    = 0u;
    uint64_t nonLocalVideoUsageBytes  = 0u;
    uint64_t nonLocalVideoBudgetBytes = 0u;
};

struct PhaseRecord
{
    size_t index = 0u;
    std::vector<StepRecord> steps;
};

struct AdapterRecord
{
    std::string description;
    UINT vendorId       = 0u;
    UINT deviceId       = 0u;
    bool software       = true;
    uint64_t driver     = 0u; // UMD version from CheckInterfaceSupport
    uint64_t videoBytes = 0u;
};

// One registered retained cache after the last phase (W9.1): its registry name, live entries and resident bytes
// (bytesCounted false: the registry row has no byte count).
struct CacheRecord
{
    std::string name;
    uint64_t entries  = 0u;
    bool bytesCounted = false;
    uint64_t bytes    = 0u;
};

// The Grid cell probe after the last phase (W9.1): every visible Grid is hidden, which releases its cell layouts, then
// shown and rendered again, which rebuilds them. Recorded: the layouts released, the live heap bytes that fell on hiding
// and those that rebuilding added. The rebuild is what the cells hold (DirectWrite's layout storage with DxUi's entries
// and strings); hiding returns far less, because the released storage stays busy in the process heaps. Live heap bytes are
// the busy blocks of every process heap: exact at the moment, but they include any concurrent allocation of another thread.
struct GridCellRelease
{
    uint64_t layouts          = 0u;
    int64_t releasedHeapBytes = 0;
    int64_t rebuiltHeapBytes  = 0;
};

struct RunRecord
{
    const Definition* definition = nullptr;
    const Variant* variant       = nullptr;
    Host host                    = Host::Embedded;
    bool hardware                = false;
    bool cppAllocationsCounted   = false;
    bool drawCallsCounted        = false; // counting COM wrappers installed: frame.drawCalls is meaningful
    AdapterRecord adapter;
    std::vector<PhaseRecord> phases;
    std::vector<MemorySample> memory;
    uint64_t entryPrivateBytes = 0u;
    uint64_t trackedResources  = 0u;
    uint64_t trackerOverflow   = 0u;
    // G13 inputs (W9.1), set by a runner that observes them: the controls and the editors (TextField, ComboBox) of the
    // built tree, the registered caches after the last phase and the Grid cell release probe.
    std::optional<uint64_t> controls;
    std::optional<uint64_t> editors;
    std::optional<uint64_t> richEditors; // RichTextEditor controls: their history is not in largestTextHistoryBytes
    std::vector<CacheRecord> caches;
    std::optional<GridCellRelease> gridCells;

    [[nodiscard]] const PhaseRecord& Phase(std::string_view name) const
    {
        const std::optional<size_t> index = definition->FindPhase(name);
        return phases.at(index.value());
    }
};

// Every counter a record carries, by qualified name: "cppAllocations", "created" (resources over every kind), "churn",
// then each W0.1 counter as "<group>.<name>" (resource kinds as "resources.<kind>.created" and ".destroyed").
template <typename Visitor> void ForEachCounter(const StepRecord& step, Visitor&& visit)
{
    visit(std::string_view("cppAllocations"), step.cppAllocations);
    visit(std::string_view("cppLargeAllocations"), step.cppLargeAllocations);
    visit(std::string_view("created"), DxUi::Detail::TotalCreated(step.work));
    visit(std::string_view("churn"), DxUi::Detail::Churn(step.work));
    DxUi::Detail::ForEachWorkCounter(step.work,
                                     [&](const char* group, const char* name, uint64_t value)
    {
        std::array<char, 96> qualified{};
        size_t length = 0u;
        for (const char* part : {group, ".", name})
            for (; *part && length + 1u < qualified.size(); ++part)
                qualified[length++] = *part;
        visit(std::string_view(qualified.data(), length), value);
    });
}

[[nodiscard]] inline std::optional<uint64_t> CounterValue(const StepRecord& step, std::string_view name)
{
    std::optional<uint64_t> found;
    ForEachCounter(step,
                   [&](std::string_view counter, uint64_t value)
    {
        if (counter == name)
            found = value;
    });
    return found;
}

struct CounterSummary
{
    uint64_t minimum = 0u;
    uint64_t maximum = 0u;
    uint64_t total   = 0u;
};

// Minimum, maximum and total of one counter over a phase's steps. Throws std::out_of_range for an unknown counter, which
// ends a test run as an unexpected exception: a gate must name counters that exist.
[[nodiscard]] inline CounterSummary Summarize(const PhaseRecord& phase, std::string_view name)
{
    CounterSummary summary{};
    bool first = true;
    for (const StepRecord& step : phase.steps)
    {
        const std::optional<uint64_t> value = CounterValue(step, name);
        if (! value)
            throw std::out_of_range("unknown performance fixture counter");
        summary.minimum = first ? *value : (std::min)(summary.minimum, *value);
        summary.maximum = (std::max)(summary.maximum, *value);
        summary.total += *value;
        first = false;
    }
    return summary;
}

// The first counter that differs between two runs of one fixture, variant and host, in measured phases.
struct Difference
{
    std::string phase;
    size_t step = 0u;
    std::string counter;
    uint64_t first  = 0u;
    uint64_t second = 0u;
};

[[nodiscard]] inline std::optional<Difference> FirstDifference(const RunRecord& first, const RunRecord& second)
{
    if (first.definition != second.definition || first.variant != second.variant || first.host != second.host || first.phases.size() != second.phases.size())
        return Difference{"<run>", 0u, "<fixture, variant or host>", 0u, 0u};
    for (size_t phase = 0u; phase < first.phases.size(); ++phase)
    {
        const auto& declared = first.definition->phases[phase];
        if (declared.kind != PhaseKind::Measured)
            continue;
        const auto& a = first.phases[phase].steps;
        const auto& b = second.phases[phase].steps;
        if (a.size() != b.size())
            return Difference{std::string(declared.name), (std::min)(a.size(), b.size()), "<step count>", a.size(), b.size()};
        for (size_t step = 0u; step < a.size(); ++step)
        {
            std::vector<std::pair<std::string, uint64_t>> values;
            ForEachCounter(a[step], [&](std::string_view name, uint64_t value) { values.emplace_back(std::string(name), value); });
            size_t index = 0u;
            std::optional<Difference> difference;
            ForEachCounter(b[step],
                           [&](std::string_view, uint64_t value)
            {
                if (! difference && values[index].second != value)
                    difference = Difference{std::string(declared.name), step, values[index].first, values[index].second, value};
                ++index;
            });
            if (difference)
                return difference;
        }
    }
    return std::nullopt;
}

[[nodiscard]] inline std::string Describe(const RunRecord& run, const Difference& difference)
{
    return std::string(run.definition->id) + ' ' + std::string(run.variant->name) + ' ' + std::string(HostName(run.host)) + " phase " + difference.phase +
           " step " + std::to_string(difference.step) + ": " + difference.counter + " was " + std::to_string(difference.first) + " in the first run and " +
           std::to_string(difference.second) + " in the second";
}

// ---- G13 memory budget lines (W9.1) ------------------------------------------------------------------------------------
// Every receipt reports each line of budget G13 (plan section 1) as a counter, compared with its budget. A line a fixture
// cannot observe is "not measured" with the reason; the private-bytes line is advisory, a band compared between paired
// runs (same-binary drift is about 0.3 MB), and never judged. A counted line over its budget must be a recorded exception
// naming the item that closes it; when that item lands it deletes its row below, and the gate then enforces the budget.
// A row whose line is within budget is stale and fails the gate too, so a fixed line cannot keep its exception.
struct BudgetLine
{
    std::string_view id;
    std::string_view statement; // the plan's words
    std::string_view counter;   // what the receipt reports
    uint64_t limit = 0u;        // bytes
    bool advisory  = false;
};
inline constexpr std::array kG13Lines{
    BudgetLine{"idle-private",
               "idle native window <= 8 MB private above process entry at 1080p",
               "private bytes after the last phase, shown, minus private bytes at process entry",
               8'000'000u,
               true},
    BudgetLine{"minimized-video",
               "<= 8 MiB video memory when minimized",
               "local video memory usage of the process, hidden (a hidden embedded view is its minimized state)",
               8u * 1024u * 1024u},
    BudgetLine{"per-control",
               "<= 256 B per simple control all-in",
               "tracked live C++ bytes after the build divided by the controls of the built tree (host attachment included)",
               256u},
    BudgetLine{"per-grid-cell",
               "<= 1 KB per visible single-line Grid cell",
               "live heap bytes that rebuilding every Grid's cell layouts adds after hiding released them, per cell layout",
               1'000u},
    BudgetLine{"text-history",
               "<= 4 MiB text history per editor",
               "EmbeddedStatistics::largestTextHistoryBytes after the last phase (TextField and ComboBox; RichTextEditor history is not counted)",
               4u * 1024u * 1024u},
};

struct BudgetException
{
    std::string_view fixture;
    std::string_view line;
    std::string_view closingItem;
    std::string_view finding;
};
inline constexpr std::array kG13Exceptions{
    BudgetException{"FX5", "per-control", "W9.6", "Label alone is 448 B in Release; FX5 holds about 640 tracked bytes per control"},
    BudgetException{"FX5", "per-grid-cell", "W9.4", "a single-line cell keeps a whole DirectWrite layout between paints"},
    BudgetException{"FX5", "minimized-video", "W9.2", "hiding releases the surface, yet the process's local video usage does not fall"},
    BudgetException{"FX13", "per-control", "W9.6", "35 controls share the host attachment; the controls alone exceed 256 B each"},
    BudgetException{"FX13", "minimized-video", "W9.2", "the hidden 1080p view keeps the process's local video usage"},
};

enum class BudgetVerdict : uint8_t
{
    Met,
    Exceeded,
    Advisory,
    NotMeasured,
};
struct BudgetResult
{
    const BudgetLine* line = nullptr;
    std::optional<uint64_t> value;
    std::string_view reason; // why a line is not measured
    BudgetVerdict verdict            = BudgetVerdict::NotMeasured;
    const BudgetException* exception = nullptr;
};

[[nodiscard]] constexpr const BudgetException* FindBudgetException(std::string_view fixture, std::string_view line) noexcept
{
    for (const BudgetException& exception : kG13Exceptions)
        if (exception.fixture == fixture && exception.line == line)
            return &exception;
    return nullptr;
}

[[nodiscard]] inline std::array<BudgetResult, kG13Lines.size()> EvaluateG13(const RunRecord& run)
{
    const MemorySample* built  = nullptr;
    const MemorySample* shown  = nullptr; // after the last phase
    const MemorySample* hidden = nullptr;
    for (const MemorySample& sample : run.memory)
    {
        if (sample.at == "built")
            built = &sample;
        else if (sample.at == "phase")
            shown = &sample;
        else if (sample.at == "hidden")
            hidden = &sample;
    }
    std::array<BudgetResult, kG13Lines.size()> results{};
    for (size_t index = 0u; index < kG13Lines.size(); ++index)
    {
        BudgetResult& result      = results[index];
        result.line               = &kG13Lines[index];
        const std::string_view id = result.line->id;
        if (id == "idle-private")
        {
            if (shown)
                result.value = shown->privateBytes > run.entryPrivateBytes ? shown->privateBytes - run.entryPrivateBytes : 0u;
            else
                result.reason = "no sample after the last phase";
        }
        else if (id == "minimized-video")
        {
            if (hidden && hidden->video)
                result.value = hidden->localVideoUsageBytes;
            else
                result.reason = "software adapter: rendering uses system memory, which private bytes count";
        }
        else if (id == "per-control")
        {
            if (built && built->trackedLiveBytes && run.controls && *run.controls)
                result.value = static_cast<uint64_t>((std::max)(*built->trackedLiveBytes, int64_t{0})) / *run.controls;
            else
                result.reason = "this executable does not replace the allocation functions";
        }
        else if (id == "per-grid-cell")
        {
            if (run.gridCells && run.gridCells->layouts && run.adapter.software)
                result.reason = "software adapter: WARP's rasterizer threads allocate in the same heaps while the cells are rebuilt (hardware receipts "
                                "measure this line)";
            else if (run.gridCells && run.gridCells->layouts && run.gridCells->rebuiltHeapBytes > 0)
                result.value = static_cast<uint64_t>(run.gridCells->rebuiltHeapBytes) / run.gridCells->layouts;
            else if (run.gridCells && run.gridCells->layouts)
                result.reason = "the process heaps did not grow when the cells were rebuilt (AddressSanitizer allocates outside them)";
            else
                result.reason = run.gridCells ? "the fixture keeps no Grid cell layout" : "the run did not probe Grid cell release";
        }
        else if (id == "text-history")
        {
            if (run.editors && *run.editors && shown && shown->retained)
                result.value = shown->retained->largestTextHistoryBytes;
            else if (run.editors && ! *run.editors && run.richEditors.value_or(0u))
                result.reason = "the fixture's editors are RichTextEditors, whose piece-shared history has no byte count yet (W9.3)";
            else
                result.reason = run.editors && ! *run.editors ? "the fixture has no TextField or ComboBox" : "this host does not report EmbeddedStatistics";
        }
        if (result.line->advisory && result.value)
            result.verdict = BudgetVerdict::Advisory;
        else if (result.value)
            result.verdict = *result.value <= result.line->limit ? BudgetVerdict::Met : BudgetVerdict::Exceeded;
        result.exception = FindBudgetException(run.definition->id, id);
    }
    return results;
}

// The gate on a run: a counted line over its budget without a recorded exception, or a recorded exception on a line
// within its budget. Empty when the run passes.
[[nodiscard]] inline std::vector<std::string> G13Failures(const RunRecord& run)
{
    std::vector<std::string> failures;
    for (const BudgetResult& result : EvaluateG13(run))
    {
        const std::string line = std::string(run.definition->id) + " G13 " + std::string(result.line->id) + ": ";
        if (result.verdict == BudgetVerdict::Exceeded && ! result.exception)
            failures.push_back(line + std::to_string(*result.value) + " bytes exceed the budget of " + std::to_string(result.line->limit));
        else if (result.verdict == BudgetVerdict::Met && result.exception)
            failures.push_back(line + std::to_string(*result.value) + " bytes meet the budget: delete the recorded exception of " +
                               std::string(result.exception->closingItem));
    }
    return failures;
}

// ---- Receipt JSON ----------------------------------------------------------------------------------------------------
// The native part of a receipt; performance-fixtures.ps1 adds provenance, input hashes and the README.

inline void WriteJsonString(std::ostream& output, std::string_view text)
{
    output << '"';
    for (const char c : text)
    {
        if (c == '"' || c == '\\')
            output << '\\' << c;
        else if (static_cast<unsigned char>(c) < 0x20u || static_cast<unsigned char>(c) >= 0x7Fu)
            output << '?';
        else
            output << c;
    }
    output << '"';
}

inline void WriteRetainedJson(std::ostream& output, const DxUi::EmbeddedStatistics& stats)
{
    output << "{\"surfaceBytes\":" << stats.surfaceBytes << ",\"cachedBrushes\":" << stats.cachedBrushes << ",\"cachedTextFormats\":" << stats.cachedTextFormats
           << ",\"retainedTextLayouts\":" << stats.retainedTextLayouts << ",\"retainedTextLayoutUnits\":" << stats.retainedTextLayoutUnits
           << ",\"retainedBitmaps\":" << stats.retainedBitmaps << ",\"retainedBitmapBytes\":" << stats.retainedBitmapBytes
           << ",\"retainedCacheEntries\":" << stats.retainedCacheEntries << ",\"retainedCacheBytes\":" << stats.retainedCacheBytes
           << ",\"largestTextHistoryBytes\":" << stats.largestTextHistoryBytes << '}';
}

// The G13 lines of a run and the registered caches behind retainedCacheEntries and retainedCacheBytes (W9.1).
inline void WriteBudgetLinesJson(std::ostream& output, const RunRecord& run)
{
    output << "{\"G13\":[";
    bool first = true;
    for (const BudgetResult& result : EvaluateG13(run))
    {
        output << (first ? "" : ",") << "{\"line\":";
        WriteJsonString(output, result.line->id);
        output << ",\"budget\":";
        WriteJsonString(output, result.line->statement);
        output << ",\"counter\":";
        WriteJsonString(output, result.line->counter);
        output << ",\"limitBytes\":" << result.line->limit << ",\"measuredBytes\":";
        if (result.value)
            output << *result.value;
        else
            output << "null";
        output << ",\"status\":\"";
        switch (result.verdict)
        {
            case BudgetVerdict::Met: output << "met"; break;
            case BudgetVerdict::Exceeded: output << (result.exception ? "recorded exception" : "exceeded"); break;
            case BudgetVerdict::Advisory: output << "advisory"; break;
            case BudgetVerdict::NotMeasured: output << "not measured"; break;
        }
        output << '"';
        if (! result.reason.empty())
        {
            output << ",\"reason\":";
            WriteJsonString(output, result.reason);
        }
        if (result.exception)
        {
            output << ",\"exception\":{\"closingItem\":";
            WriteJsonString(output, result.exception->closingItem);
            output << ",\"finding\":";
            WriteJsonString(output, result.exception->finding);
            output << '}';
        }
        output << '}';
        first = false;
    }
    output << "]}";
}

inline void WriteCachesJson(std::ostream& output, const RunRecord& run)
{
    output << '[';
    for (size_t index = 0u; index < run.caches.size(); ++index)
    {
        const CacheRecord& cache = run.caches[index];
        output << (index ? "," : "") << "{\"name\":";
        WriteJsonString(output, cache.name);
        output << ",\"entries\":" << cache.entries << ",\"residentBytes\":";
        if (cache.bytesCounted)
            output << cache.bytes;
        else
            output << "null";
        output << '}';
    }
    output << ']';
}

inline void WriteMemoryJson(std::ostream& output, const RunRecord& run)
{
    const auto delta = [](uint64_t after, uint64_t before) { return static_cast<int64_t>(after) - static_cast<int64_t>(before); };
    output << "{\"entryPrivateBytes\":" << run.entryPrivateBytes << ",\"videoMemory\":{\"applicable\":" << (run.adapter.software ? "false" : "true");
    if (run.adapter.software)
        output << ",\"reason\":\"software adapter: rendering uses system memory, which privateBytes counts\"";
    output << "},\"trackedLiveBytes\":";
    if (run.cppAllocationsCounted)
        output << "\"measured from the scene build onwards\"";
    else
        output << "\"not measured: this executable does not replace the allocation functions\"";
    output << ",\"samples\":[";
    for (size_t index = 0u; index < run.memory.size(); ++index)
    {
        const MemorySample& sample = run.memory[index];
        output << (index ? "," : "") << "{\"at\":";
        WriteJsonString(output, sample.at);
        if (! sample.phase.empty())
        {
            output << ",\"phase\":";
            WriteJsonString(output, sample.phase);
        }
        output << ",\"privateBytes\":" << sample.privateBytes << ",\"workingSetBytes\":" << sample.workingSetBytes;
        if (sample.trackedLiveBytes)
            output << ",\"trackedLiveBytes\":" << *sample.trackedLiveBytes;
        if (sample.video)
            output << ",\"localVideoUsageBytes\":" << sample.localVideoUsageBytes << ",\"localVideoBudgetBytes\":" << sample.localVideoBudgetBytes
                   << ",\"nonLocalVideoUsageBytes\":" << sample.nonLocalVideoUsageBytes << ",\"nonLocalVideoBudgetBytes\":" << sample.nonLocalVideoBudgetBytes;
        if (sample.retained)
        {
            output << ",\"retained\":";
            WriteRetainedJson(output, *sample.retained);
        }
        output << '}';
    }
    output << ']';
    // Hiding is how an embedded view is minimized: the last sample is "hidden" and the one before it is the shown state.
    if (run.memory.size() >= 2u && run.memory.back().at == "hidden")
    {
        const MemorySample& shown  = run.memory[run.memory.size() - 2u];
        const MemorySample& hidden = run.memory.back();
        output << ",\"hiddenResidencyDelta\":{\"privateBytes\":" << delta(hidden.privateBytes, shown.privateBytes)
               << ",\"workingSetBytes\":" << delta(hidden.workingSetBytes, shown.workingSetBytes);
        if (shown.retained && hidden.retained)
            output << ",\"surfaceBytes\":" << delta(hidden.retained->surfaceBytes, shown.retained->surfaceBytes)
                   << ",\"retainedTextLayouts\":" << delta(hidden.retained->retainedTextLayouts, shown.retained->retainedTextLayouts)
                   << ",\"retainedBitmapBytes\":" << delta(hidden.retained->retainedBitmapBytes, shown.retained->retainedBitmapBytes);
        if (shown.video && hidden.video)
            output << ",\"localVideoBytes\":" << delta(hidden.localVideoUsageBytes, shown.localVideoUsageBytes)
                   << ",\"nonLocalVideoBytes\":" << delta(hidden.nonLocalVideoUsageBytes, shown.nonLocalVideoUsageBytes);
        output << "},\"minimizedResidency\":\"hidden\"";
    }
    output << ",\"controlBytes\":{\"Control\":" << sizeof(DxUi::Control) << ",\"Label\":" << sizeof(DxUi::Label) << ",\"Button\":" << sizeof(DxUi::Button)
           << ",\"Toggle\":" << sizeof(DxUi::Toggle) << ",\"Checkbox\":" << sizeof(DxUi::Checkbox) << ",\"RadioButton\":" << sizeof(DxUi::RadioButton)
           << ",\"ThroughputGraphSample\":" << sizeof(DxUi::ThroughputGraphSample) << '}';
    if (run.controls)
        output << ",\"controls\":" << *run.controls;
    if (run.gridCells)
        output << ",\"gridCellRelease\":{\"layouts\":" << run.gridCells->layouts << ",\"releasedHeapBytes\":" << run.gridCells->releasedHeapBytes
               << ",\"rebuiltHeapBytes\":" << run.gridCells->rebuiltHeapBytes << '}';
    output << ",\"retainedCaches\":";
    WriteCachesJson(output, run);
    output << '}';
}

// Per phase: every counter as one value per step ("series") and its minimum, maximum and total ("summary").
inline void WritePhasesJson(std::ostream& output, const RunRecord& run)
{
    output << '[';
    for (size_t phase = 0u; phase < run.phases.size(); ++phase)
    {
        const auto& declared = run.definition->phases[phase];
        const auto& steps    = run.phases[phase].steps;
        output << (phase ? "," : "") << "{\"name\":";
        WriteJsonString(output, declared.name);
        output << ",\"kind\":\"" << (declared.kind == PhaseKind::Warmup ? "warmup" : "measured") << "\",\"description\":";
        WriteJsonString(output, declared.description);
        output << ",\"steps\":" << steps.size() << ",\"series\":{";
        std::vector<std::string> names;
        if (! steps.empty())
            ForEachCounter(steps.front(),
                           [&](std::string_view name, uint64_t)
            {
                const bool counted = name == "cppAllocations"        ? run.cppAllocationsCounted
                                     : name == "cppLargeAllocations" ? run.cppAllocationsCounted && run.definition->largeAllocationBytes != 0u
                                                                     : true;
                if (counted)
                    names.emplace_back(name);
            });
        bool first = true;
        for (const std::string& name : names)
        {
            output << (first ? "" : ",") << '"' << name << "\":[";
            for (size_t step = 0u; step < steps.size(); ++step)
                output << (step ? "," : "") << CounterValue(steps[step], name).value();
            output << ']';
            first = false;
        }
        output << "},\"summary\":{";
        first = true;
        for (const std::string& name : names)
        {
            const CounterSummary summary = Summarize(run.phases[phase], name);
            output << (first ? "" : ",") << '"' << name << "\":{\"min\":" << summary.minimum << ",\"max\":" << summary.maximum << ",\"total\":" << summary.total
                   << '}';
            first = false;
        }
        output << "}}";
    }
    output << ']';
}

inline void WriteDriverVersion(std::ostream& output, uint64_t driver)
{
    output << '"' << (driver >> 48u) << '.' << ((driver >> 32u) & 0xFFFFu) << '.' << ((driver >> 16u) & 0xFFFFu) << '.' << (driver & 0xFFFFu) << '"';
}

[[nodiscard]] constexpr const char* ConfigurationName() noexcept
{
#if defined(__SANITIZE_ADDRESS__)
    return "ASan Debug";
#elif defined(_DEBUG)
    return "Debug";
#else
    return "Release";
#endif
}

// The executable-written receipt of one run. Timing is never measured here: performance-timing.ps1 owns it.
inline void WriteReceiptJson(std::ostream& output, const RunRecord& run)
{
    const Definition& definition = *run.definition;
    const Variant& variant       = *run.variant;
    output << "{\"schema\":1,\"workloadOwner\":\"DxUi\",\"fixture\":\"dxui-performance-fixture-v1\",\"fixtureId\":";
    WriteJsonString(output, definition.id);
    output << ",\"title\":";
    WriteJsonString(output, definition.title);
    output << ",\"scenario\":";
    WriteJsonString(output, definition.scenario);
    output << ",\"budgets\":[";
    for (size_t index = 0u; index < definition.budgets.size(); ++index)
    {
        output << (index ? "," : "");
        WriteJsonString(output, definition.budgets[index]);
    }
    output << "],\"host\":\"" << HostName(run.host) << "\",\"variant\":{\"name\":";
    WriteJsonString(output, variant.name);
    output << ",\"width\":" << variant.width << ",\"height\":" << variant.height << ",\"dpi\":" << variant.dpi
           << ",\"hardwareOnly\":" << (variant.hardwareOnly ? "true" : "false") << "},\"reducedMotion\":" << (definition.reducedMotion ? "true" : "false")
           << ",\"renderer\":\"" << (run.hardware ? "Hardware" : "WARP") << "\",\"configuration\":\"" << ConfigurationName()
           << "\",\"iteratorDebugLevel\":" << _ITERATOR_DEBUG_LEVEL << ",\"compiler\":" << _MSC_FULL_VER << ",\"adapter\":{\"description\":";
    WriteJsonString(output, run.adapter.description);
    output << ",\"vendorId\":" << run.adapter.vendorId << ",\"deviceId\":" << run.adapter.deviceId
           << ",\"software\":" << (run.adapter.software ? "true" : "false") << ",\"driverVersion\":";
    WriteDriverVersion(output, run.adapter.driver);
    output << ",\"dedicatedVideoBytes\":" << run.adapter.videoBytes << "},\"cppAllocations\":"
           << (run.cppAllocationsCounted ? "\"counted per step\"" : "\"not counted: this executable does not replace the allocation functions\"");
    if (run.cppAllocationsCounted && definition.largeAllocationBytes != 0u)
        output << ",\"cppLargeAllocationMinimumBytes\":" << definition.largeAllocationBytes;
    output << ",\"drawCalls\":"
           << (run.drawCallsCounted ? "\"counted per step by the counting COM wrappers\""
                                    : "\"not counted: frame.drawCalls is 0 without the counting wrappers\"")
           << ",\"trackedResources\":" << run.trackedResources << ",\"trackerOverflow\":" << run.trackerOverflow << ",\"phases\":";
    WritePhasesJson(output, run);
    output << ",\"memory\":";
    WriteMemoryJson(output, run);
    output << ",\"budgetLines\":";
    WriteBudgetLinesJson(output, run);
    output << ",\"timing\":{\"status\":\"not measured\",\"reason\":\"timing comes only from performance-timing.ps1 on a hardware adapter in a quiet "
              "window (W0.2); counts and memory are recorded here\"}}\n";
}

// The catalog's metadata, for performance-fixtures.ps1: which fixtures exist, are built, run where, and what they hash.
template <size_t Count> void WriteCatalogJson(std::ostream& output, const std::array<const Definition*, Count>& catalog)
{
    output << "{\"schema\":1,\"fixtures\":[";
    for (size_t index = 0u; index < catalog.size(); ++index)
    {
        const Definition& definition = *catalog[index];
        output << (index ? "," : "") << "{\"id\":";
        WriteJsonString(output, definition.id);
        output << ",\"title\":";
        WriteJsonString(output, definition.title);
        output << ",\"scenario\":";
        WriteJsonString(output, definition.scenario);
        output << ",\"built\":" << (definition.Built() ? "true" : "false") << ",\"interactiveOnly\":" << (definition.interactiveOnly ? "true" : "false")
               << ",\"hosts\":[";
        bool first = true;
        for (const Host host : {Host::Embedded, Host::Window})
            if (Supports(definition.hosts, host))
            {
                output << (first ? "" : ",") << '"' << HostName(host) << '"';
                first = false;
            }
        output << "],\"budgets\":[";
        for (size_t budget = 0u; budget < definition.budgets.size(); ++budget)
        {
            output << (budget ? "," : "");
            WriteJsonString(output, definition.budgets[budget]);
        }
        output << "],\"variants\":[";
        for (size_t variant = 0u; variant < definition.variants.size(); ++variant)
        {
            const Variant& value = definition.variants[variant];
            output << (variant ? "," : "") << "{\"name\":";
            WriteJsonString(output, value.name);
            output << ",\"width\":" << value.width << ",\"height\":" << value.height << ",\"dpi\":" << value.dpi
                   << ",\"hardwareOnly\":" << (value.hardwareOnly ? "true" : "false") << '}';
        }
        output << "],\"phases\":[";
        for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
        {
            const Phase& value = definition.phases[phase];
            output << (phase ? "," : "") << "{\"name\":";
            WriteJsonString(output, value.name);
            output << ",\"steps\":" << value.steps << ",\"kind\":\"" << (value.kind == PhaseKind::Warmup ? "warmup" : "measured") << "\",\"description\":";
            WriteJsonString(output, value.description);
            output << '}';
        }
        output << "],\"timedPhase\":";
        WriteJsonString(output, definition.timedPhase);
        output << ",\"sources\":[";
        for (size_t source = 0u; source < definition.sources.size(); ++source)
        {
            output << (source ? "," : "");
            WriteJsonString(output, definition.sources[source]);
        }
        output << "]}";
    }
    output << "]}\n";
}

// What every ordinary runner, gate, receipt command and the timing harness says of an interactive-only fixture it skips.
[[nodiscard]] inline std::string InteractiveNotRun(const Definition& definition)
{
    return std::string(definition.id) + ": interactive, not run";
}

// Parses "FX5" or "FX5@uhd-192dpi" into the fixture id and variant name (empty: the default variant).
struct FixtureSelection
{
    std::string id;
    std::string variant;
};
[[nodiscard]] inline FixtureSelection ParseSelection(std::wstring_view text)
{
    FixtureSelection selection;
    const size_t at = text.find(L'@');
    for (const wchar_t c : text.substr(0u, at))
        selection.id.push_back(c < 0x80 ? static_cast<char>(c) : '?');
    if (at != std::wstring_view::npos)
        for (const wchar_t c : text.substr(at + 1u))
            selection.variant.push_back(c < 0x80 ? static_cast<char>(c) : '?');
    return selection;
}
} // namespace PerformanceFixtures
#pragma warning(pop)
