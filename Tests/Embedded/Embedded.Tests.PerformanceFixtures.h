#pragma once
// Performance fixtures on the embedded host: the entry point of DxUi.EmbeddedTests.exe (plan W0.4; Tests/README.md,
// "Performance fixtures"). Fixtures are defined in Embedded.Tests.PerformanceFixtureFx<N>.h and listed in
// Embedded.Tests.PerformanceFixtureCatalog.h; this header runs them. It is included once, from Embedded.Tests.Embedded.cpp,
// after the allocation instrument (counting operator new), GraphicsFixture, FrameTiming and MemoryMetrics, and uses Check
// and Hr from there. It is not a paired-benchmark input: older trees never compile it.
//
// What it provides, for every catalog fixture whose hosts include the embedded host:
// - RunEmbedded: one run, every W0.1 counter per step and phase, the C++ allocation sink, the W0.3 memory fields;
// - EmbeddedInstance: a live fixture for item gate tests and the pixel oracle (Step, Frame, the HostPort);
// - the receipt command (--fixture-receipt), the catalog command (--fixture-catalog) and the determinism gate;
// - FrameTiming::MakeFixture, so performance-timing.ps1 -Fixture FX<N>[@variant] times the same definition.
// Nothing here is edited to add a fixture.
#include "Embedded.Tests.PerformanceFixtureCatalog.h"
#include "Embedded.Tests.WorkCounters.h"
#include "Support.Tests.CountingDirect2D.h"
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027) // scenarios are never copied
#include "Embedded.Tests.PerformanceFixtureSelfTest.h"
#pragma warning(pop)

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
namespace PerformanceFixtures
{
// How the simulated application drives an embedded view's animation clock when a step advances the fixture clock.
enum class ApplicationLoop : uint8_t
{
    // At every AdvanceTime, one AdvanceAnimation at the new time whenever the view's request is not None: the application
    // that polls each frame, the only kind API revision 5 allowed (NeedsAnimation). Receipts and their wakes.embeddedAdvance
    // counter are this application's.
    PollEveryStep,
    // W3.5: the application sleeps on the fixture clock until the view's request is due and advances exactly then: at a
    // Deadline's due time, once per step while Continuous (one frame), never while None. A request already due when it is
    // read (after input, or the baseline Deadline of 0 after a pause) is advanced at once in the same awake turn.
    SleepUntilRequest,
};
// One AdvanceAnimation the simulated application made, with what it cost.
struct ApplicationAdvance
{
    size_t phase  = 0u; // the fixture step that advanced the clock
    size_t step   = 0u;
    uint64_t atMs = 0u;
    // True when the application slept until this advance (a timer it set from the request fired, or a Continuous frame);
    // false for an advance made at once, in a turn the application was already awake for.
    bool woke                                    = false;
    DxUi::EmbeddedAnimationRequest::Kind request = DxUi::EmbeddedAnimationRequest::Kind::None;
    uint64_t tickCalls                           = 0u;
    uint64_t tickRequesters                      = 0u;
    uint64_t treeVisitsTick                      = 0u;
    uint64_t tooltipDeadlineTicks                = 0u;
    uint64_t tooltipTargetValidations            = 0u;
};

// A fixture live on an EmbeddedHost. Instantiate, then Create (attach and build), then any sequence of Step and Frame.
class EmbeddedInstance final : public HostPort
{
public:
    // The application loop and, when `log` is given, a record of every advance it makes (reserve its capacity first: the
    // log is never grown during a counted step's allocations).
    void SetApplicationLoop(ApplicationLoop loop, std::vector<ApplicationAdvance>* log = nullptr) noexcept
    {
        loop_ = loop;
        log_  = log;
    }
    // Advances logged from now on carry this phase (a gate's own steps after the fixture's phases).
    void SetLoggedPhase(size_t phase) noexcept
    {
        phase_ = phase;
        step_  = 0u;
    }
    // Creates the scenario object (and any model it owns) without attaching anything.
    void Instantiate(const Definition& definition)
    {
        definition_ = &definition;
        scenario_   = definition.create();
    }
    // Attaches the host on `device` and builds the scene at `variant`. The scenario's own host is used when it has one.
    HRESULT Create(ID3D11Device* device, const Variant& variant)
    {
        variant_       = &variant;
        width_         = variant.width;
        height_        = variant.height;
        dpi_           = variant.dpi;
        HRESULT result = S_OK;
        view_          = scenario_->AttachOwnEmbeddedHost(device, result);
        RETURN_IF_FAILED(result);
        if (! view_)
        {
            std::shared_ptr<DxUi::GraphicsDevice> graphics;
            RETURN_IF_FAILED(DxUi::GraphicsDevice::Create(device, graphics));
            ownedView_ = std::make_unique<DxUi::EmbeddedHost>();
            // The application's clock is the fixture clock, and its preparation callback is how the view wakes the simulated
            // application that sleeps until its request (W3.5 review F4).
            RETURN_IF_FAILED(ownedView_->Attach(std::move(graphics), DxUi::EmbeddedCallbacks{this, &WakeThunk, &NowThunk}));
            auto theme          = DxUi::MakeDefaultThemePalette(true);
            theme.reducedMotion = definition_->reducedMotion;
            ownedView_->Controls().SetTheme(theme);
            view_ = ownedView_.get();
        }
        context_.emplace(Context{Host::Embedded, variant, view_->Controls(), *this});
        return scenario_->Build(*context_);
    }
    [[nodiscard]] DxUi::EmbeddedHost& View() noexcept
    {
        return *view_;
    }
    [[nodiscard]] Context& GetContext() noexcept
    {
        return *context_;
    }
    // The live scene, for a gate that reads what its fixture recorded (W9.3's text-history edits).
    [[nodiscard]] Scenario& GetScenario() noexcept
    {
        return *scenario_;
    }
    void Step(size_t phase, size_t step)
    {
        phase_ = phase;
        step_  = step;
        scenario_->Step(*context_, phase, step);
    }
    HRESULT Prepare() noexcept
    {
        return view_->Prepare(width_, height_, dpi_);
    }
    // One frame: preparation, then composition onto the fixture's target, resized to the host size when a step resized it.
    HRESULT Frame(GraphicsFixture& gpu)
    {
        if (gpu.width != width_ || gpu.height != height_)
            RETURN_IF_FAILED(gpu.Resize(width_, height_));
        const HRESULT prepared = Prepare();
        RETURN_IF_FAILED(prepared);
        gpu.Bind();
        RETURN_IF_FAILED(view_->Composite(gpu.context.get(), gpu.Viewport()));
        // The frame ends as an application's does, which presents or flushes it: WARP keeps unflushed immediate-context
        // work in process heap memory, so a frame loop that never flushes grows private bytes by the queued work of every
        // frame (about 80-90 KB per composited frame at 640x480 and 1080p), which a hardware driver flushes on its own.
        gpu.context->Flush();
        return prepared;
    }

    bool Pointer(const DxUi::PointerEvent& event) override
    {
        return view_->DispatchPointer(event);
    }
    bool Key(UINT virtualKey, bool down, UINT modifiers) override
    {
        return view_->DispatchKey(virtualKey, down, modifiers);
    }
    bool Character(wchar_t character, UINT modifiers) override
    {
        return view_->DispatchCharacter(character, modifiers);
    }
    void AdvanceTime(uint64_t milliseconds) override
    {
        using Kind         = DxUi::EmbeddedAnimationRequest::Kind;
        const uint64_t end = now_ + milliseconds;
        if (loop_ == ApplicationLoop::PollEveryStep)
        {
            now_ = end;
            if (const auto request = view_->GetAnimationRequest(); request.kind != Kind::None)
                Advance(request.kind, false);
            return;
        }
        // The application knows only the request it last read: from its own last advance, or read again when the view's
        // callback woke it (input, a setter, a model change in the steps before). It never polls. Each pass either stops (None,
        // or a deadline after this step) or advances once; an advance re-arms at most the requests it ticked, so a step of
        // fixture time needs a bounded number of passes.
        for (size_t pass = 0u;; ++pass)
        {
            Check(pass < 100'000u, "the simulated application settles within one fixture step");
            if (std::exchange(woken_, false))
                sleepingOn_ = view_->GetAnimationRequest();
            const DxUi::EmbeddedAnimationRequest request = sleepingOn_;
            if (request.kind == Kind::None)
                break;
            if (request.kind == Kind::Continuous)
            {
                // Every frame: one frame at the end of the step, as the fixture renders one frame per step.
                now_        = end;
                sleepingOn_ = Advance(request.kind, true);
                break;
            }
            if (request.dueTickMs > end)
                break;
            const bool slept = request.dueTickMs > now_;
            now_             = slept ? request.dueTickMs : now_;
            sleepingOn_      = Advance(request.kind, slept);
        }
        now_ = end;
    }
    // The simulated application made a call that can only make its request later or None (hiding the view): it reads the
    // request again, as the hosting guide advises, instead of waking once more for nothing at the request it slept on.
    void ApplicationChangedView() noexcept
    {
        woken_ = true;
    }
    // Calls of the view's requestPreparation callback since the simulation started.
    [[nodiscard]] size_t ApplicationCallbacks() const noexcept
    {
        return callbacks_;
    }
    void Resize(UINT width, UINT height, float dpi) override
    {
        width_  = width;
        height_ = height;
        dpi_    = dpi;
    }
    [[nodiscard]] uint64_t NowMs() const noexcept override
    {
        return now_;
    }

private:
    static void WakeThunk(void* context) noexcept
    {
        auto* const self = static_cast<EmbeddedInstance*>(context);
        self->woken_     = true;
        ++self->callbacks_;
    }
    static uint64_t NowThunk(void* context) noexcept
    {
        return static_cast<EmbeddedInstance*>(context)->now_;
    }
    DxUi::EmbeddedAnimationRequest Advance(DxUi::EmbeddedAnimationRequest::Kind request, bool woke)
    {
        const DxUi::Detail::WorkCounterScope scope;
        const DxUi::EmbeddedAnimationRequest next = view_->AdvanceAnimation(now_);
        if (! log_)
            return next;
        Check(log_->size() < log_->capacity(), "the advance log was reserved for the run");
        const DxUi::Detail::WorkCounters work = scope.Delta();
        log_->push_back(ApplicationAdvance{phase_,
                                           step_,
                                           now_,
                                           woke,
                                           request,
                                           work.frame.tickCalls,
                                           work.frame.tickRequesters,
                                           work.frame.treeVisitsTick,
                                           work.frame.tooltipDeadlineTicks,
                                           work.frame.tooltipTargetValidations});
        return next;
    }

    // The simulated application's view of the schedule: the last request it read, and whether the callback has woken it.
    DxUi::EmbeddedAnimationRequest sleepingOn_{};
    bool woken_                           = true; // an application reads the request once when it starts
    size_t callbacks_                     = 0u;
    ApplicationLoop loop_                 = ApplicationLoop::PollEveryStep;
    std::vector<ApplicationAdvance>* log_ = nullptr;
    size_t phase_                         = 0u;
    size_t step_                          = 0u;
    const Definition* definition_         = nullptr;
    const Variant* variant_               = nullptr;
    // Declared before the host it may own models for, so the host is destroyed first.
    std::unique_ptr<Scenario> scenario_;
    std::unique_ptr<DxUi::EmbeddedHost> ownedView_;
    DxUi::EmbeddedHost* view_ = nullptr;
    std::optional<Context> context_;
    UINT width_   = 0u;
    UINT height_  = 0u;
    float dpi_    = 96.0f;
    uint64_t now_ = kClockOriginMs;
};

inline AdapterRecord DescribeAdapter(ID3D11Device* device)
{
    const FrameTiming::AdapterIdentity identity = FrameTiming::DescribeAdapter(device);
    return AdapterRecord{identity.description, identity.vendorId, identity.deviceId, identity.software, identity.driver, identity.videoBytes};
}

// Owner turns one settling step may take (Definition::settlePreparation) before the run fails.
inline constexpr size_t kMaximumSettleTurns = 1u << 20u;

struct RunOptions
{
    bool hardware = false;
    // Installs the counting COM wrappers (Support.Tests.CountingDirect2D.h) before the host's graphics device exists, so
    // frame.drawCalls counts every Direct2D draw call; without them that counter stays 0.
    bool countDrawCalls            = false;
    size_t trackedResourceCapacity = 262144u; // FX2 creates over 16,384 tracked text layouts in one step
    // Called after the last phase, before the view is hidden: a gate test inspects the live scene or measures its own
    // action here. It runs outside every counted step.
    std::function<void(EmbeddedInstance&, GraphicsFixture&)> afterPhases;
    // The G13 Grid cell probe (W9.1), after afterPhases: hide every visible Grid, record the cell layouts released and the
    // live heap bytes that fell, show them again, render one frame and record the live heap bytes the rebuild added. Receipts and the G13 gate run it; it is
    // off by default so that other gates see the scene exactly as their phases left it.
    bool probeGridCells = false;
    // The simulated application's loop (W3.5) and, optionally, the record of each advance it makes (reserved by the caller).
    ApplicationLoop applicationLoop                 = ApplicationLoop::PollEveryStep;
    std::vector<ApplicationAdvance>* applicationLog = nullptr;
};

// The controls of a tree, and those that keep a text history (TextField, ComboBox).
inline void CountControls(const DxUi::Control* control, uint64_t& controls, uint64_t& editors, uint64_t& richEditors)
{
    if (! control)
        return;
    ++controls;
    if (dynamic_cast<const DxUi::TextField*>(control) || dynamic_cast<const DxUi::ComboBox*>(control))
        ++editors;
    else if (dynamic_cast<const DxUi::RichTextEditor*>(control))
        ++richEditors;
    for (size_t index = 0u; index < control->GetLogicalChildCount(); ++index)
        CountControls(control->GetLogicalChild(index), controls, editors, richEditors);
}

// The G13 Grid cell probe; see RunOptions::probeGridCells.
inline GridCellRelease ProbeGridCellRelease(EmbeddedInstance& instance, GraphicsFixture& gpu, bool settlePreparation)
{
    std::array<DxUi::Grid*, 64> grids{};
    size_t count      = 0u;
    const auto gather = [&](const auto& self, DxUi::Control* control) -> void
    {
        if (! control || ! control->IsVisible())
            return;
        if (auto* grid = dynamic_cast<DxUi::Grid*>(control); grid && count < grids.size())
            grids[count++] = grid;
        for (size_t index = 0u; index < control->GetLogicalChildCount(); ++index)
            self(self, control->GetLogicalChild(index));
    };
    gather(gather, instance.View().Controls().GetRoot());
    if (count == 0u)
        return {}; // nothing to probe: the scene is left exactly as its last phase left it
    gpu.context->Flush();
    const uint64_t layoutsBefore = instance.View().GetStatistics().retainedTextLayouts;
    const uint64_t heapBefore    = MemoryMetricsTests::LiveHeapBytes();
    for (size_t index = 0u; index < count; ++index)
        grids[index]->SetVisible(false);
    const uint64_t heapAfter    = MemoryMetricsTests::LiveHeapBytes();
    const uint64_t layoutsAfter = instance.View().GetStatistics().retainedTextLayouts;
    for (size_t index = 0u; index < count; ++index)
        grids[index]->SetVisible(true);
    HRESULT framed = instance.Frame(gpu);
    for (size_t turn = 1u; settlePreparation && SUCCEEDED(framed) && instance.View().NeedsPreparation(); ++turn)
    {
        Check(turn < kMaximumSettleTurns, "the Grid cell probe's frame finishes its preparation");
        framed = instance.Frame(gpu);
    }
    Hr(framed, "performance fixture frame after the Grid cell probe");
    gpu.context->Flush();
    const uint64_t heapRebuilt = MemoryMetricsTests::LiveHeapBytes();
    return GridCellRelease{layoutsBefore - layoutsAfter,
                           static_cast<int64_t>(heapBefore) - static_cast<int64_t>(heapAfter),
                           static_cast<int64_t>(heapRebuilt) - static_cast<int64_t>(heapAfter)};
}

// One run of `definition` at `variant`: per step, the WorkCounterScope delta of the step and its frame (taken after a
// ResourceLifetimeTracker sweep, so destructions and churn are measured) and the C++ allocations of the same interval;
// memory at the device, after the build, after each phase and hidden. Record storage is reserved before live bytes are
// tracked, so the tracked bytes are the scene's own.
inline RunRecord RunEmbedded(const Definition& definition, const Variant& variant, const RunOptions& options = {})
{
    Check(definition.Built() && Supports(definition.hosts, Host::Embedded), "the fixture is built for the embedded host");
    RunRecord run;
    run.definition            = &definition;
    run.variant               = &variant;
    run.host                  = Host::Embedded;
    run.hardware              = options.hardware;
    run.cppAllocationsCounted = true;
    run.entryPrivateBytes     = ComplexUiBenchmark::Memory().PrivateUsage;
    GraphicsFixture gpu;
    gpu.width  = variant.width;
    gpu.height = variant.height;
    Hr(gpu.Create(! options.hardware), "performance fixture device");
    run.adapter = DescribeAdapter(gpu.device.get());
    Check(options.hardware != run.adapter.software, "a hardware run has a hardware adapter and a WARP run the software adapter");
    wil::com_ptr_nothrow<IDXGIDevice> dxgi;
    wil::com_ptr_nothrow<IDXGIAdapter> adapter;
    wil::com_ptr_nothrow<IDXGIAdapter3> adapter3;
    Hr(gpu.device->QueryInterface(IID_PPV_ARGS(dxgi.put())), "performance fixture DXGI device");
    Hr(dxgi->GetAdapter(adapter.put()), "performance fixture adapter");
    Hr(adapter->QueryInterface(IID_PPV_ARGS(adapter3.put())), "performance fixture adapter 3");

    run.phases.resize(definition.phases.size());
    for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
    {
        run.phases[phase].index = phase;
        run.phases[phase].steps.reserve(definition.phases[phase].steps);
    }
    run.memory.reserve(definition.phases.size() + 3u);
    std::vector<DxUi::Detail::TrackedResource> storage(options.trackedResourceCapacity);
    EmbeddedInstance instance;
    const auto sample = [&](std::string_view at, std::string_view phase, bool tracked)
    {
        gpu.context->Flush(); // a released texture leaves video memory once its destruction is submitted
        const auto video   = MemoryMetricsTests::QueryVideoMemory(adapter3.get());
        const auto process = ComplexUiBenchmark::Memory();
        MemorySample value{};
        value.at              = at;
        value.phase           = phase;
        value.privateBytes    = process.PrivateUsage;
        value.workingSetBytes = process.WorkingSetSize;
        if (tracked)
        {
            value.trackedLiveBytes = liveBytes;
            value.retained         = instance.View().GetStatistics();
        }
        value.video = video.applicable;
        if (video.applicable)
        {
            value.localVideoUsageBytes     = video.local.CurrentUsage;
            value.localVideoBudgetBytes    = video.local.Budget;
            value.nonLocalVideoUsageBytes  = video.nonLocal.CurrentUsage;
            value.nonLocalVideoBudgetBytes = video.nonLocal.Budget;
        }
        run.memory.push_back(value);
    };
    sample("device", {}, false);
    std::optional<NativeWorkTests::CountingWrappers> wrappers;
    if (options.countDrawCalls)
        wrappers.emplace();
    run.drawCallsCounted = options.countDrawCalls;
    instance.Instantiate(definition);
    instance.SetApplicationLoop(options.applicationLoop, options.applicationLog);
    liveBytes      = 0;
    countLiveBytes = true;
    Hr(instance.Create(gpu.device.get(), variant), "performance fixture build");
    sample("built", {}, true);
    uint64_t controls = 0u, editors = 0u, richEditors = 0u;
    CountControls(instance.View().Controls().GetRoot(), controls, editors, richEditors);
    run.controls    = controls;
    run.editors     = editors;
    run.richEditors = richEditors;
    {
        DxUi::Detail::ResourceLifetimeTracker tracker(storage);
        for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
        {
            auto& steps = run.phases[phase].steps;
            for (size_t step = 0u; step < definition.phases[phase].steps; ++step)
            {
                const DxUi::Detail::WorkCounterScope scope;
                allocations                 = 0u;
                largeAllocations            = 0u;
                largeAllocationMinimumBytes = definition.largeAllocationBytes != 0u ? definition.largeAllocationBytes : std::numeric_limits<size_t>::max();
                countAllocations            = true;
                instance.Step(phase, step);
                HRESULT framed = instance.Frame(gpu);
                for (size_t turn = 1u; definition.settlePreparation && SUCCEEDED(framed) && instance.View().NeedsPreparation(); ++turn)
                {
                    Check(turn < kMaximumSettleTurns, "a settling performance fixture step finishes its preparation");
                    framed = instance.Frame(gpu);
                }
                countAllocations            = false;
                largeAllocationMinimumBytes = std::numeric_limits<size_t>::max();
                const size_t counted        = allocations;
                const size_t large          = largeAllocations;
                Hr(framed, "performance fixture frame");
                static_cast<void>(tracker.Sweep());
                steps.push_back(StepRecord{scope.Delta(), counted, large});
            }
            sample("phase", definition.phases[phase].name, true);
        }
        run.trackedResources = tracker.Held();
        run.trackerOverflow  = tracker.Overflow();
        {
            // The registry's per-cache breakdown, collected outside the tracked bytes: the records are the receipt's.
            countLiveBytes = false;
            std::vector<DxUi::RetainedCacheCount> counts;
            DxUi::RetainedCacheAccess::Count(instance.View(), counts);
            run.caches.reserve(counts.size());
            for (const DxUi::RetainedCacheCount& count : counts)
                run.caches.push_back(CacheRecord{std::string(count.name), count.entries, count.bytesCounted, count.residentBytes});
            counts         = {};
            countLiveBytes = true;
        }
        if (options.afterPhases)
            options.afterPhases(instance, gpu);
        if (options.probeGridCells)
            run.gridCells = ProbeGridCellRelease(instance, gpu, definition.settlePreparation);
    }
    instance.View().SetVisible(false);
    sample("hidden", {}, true);
    countLiveBytes = false;
    return run;
}

inline void WriteReceiptFile(const RunRecord& run, const std::filesystem::path& path)
{
    if (const std::filesystem::path parent = path.parent_path(); ! parent.empty())
        std::filesystem::create_directories(parent);
    std::ofstream output{path, std::ios::binary};
    Check(bool(output), "performance fixture receipt file");
    WriteReceiptJson(output, run);
    output.close();
    Check(bool(output), "performance fixture receipt written");
}

// Writes the receipt of one fixture, variant and adapter, for --fixture-receipt and the FX5 aliases --memory-receipt and
// --memory-receipt-hardware.
inline void WriteReceipt(const Definition& definition, const Variant& variant, bool hardware, const std::filesystem::path& path, bool countDrawCalls = false)
{
    Check(! variant.hardwareOnly || hardware, "a hardware-only variant is measured on a hardware adapter");
    RunOptions options;
    options.hardware       = hardware;
    options.countDrawCalls = countDrawCalls;
    options.probeGridCells = true;
    const RunRecord run    = RunEmbedded(definition, variant, options);
    Check(run.trackerOverflow == 0u, "the tracker observed every resource the fixture created");
    WriteReceiptFile(run, path);
    std::cout << "Performance fixture receipt " << definition.id << '@' << variant.name << " (" << (hardware ? "hardware" : "WARP") << "): " << path.string()
              << '\n';
}

// DxUi.EmbeddedTests.exe --fixture-receipt <FX<N>[@variant]> Warp|Hardware <output.json> [DrawCalls]
inline int RunReceiptCommand(int argc, wchar_t** argv)
{
    const bool countDrawCalls = argc == 6 && std::wstring_view(argv[5]) == L"DrawCalls";
    if ((argc != 5 && ! countDrawCalls) || (std::wstring_view(argv[3]) != L"Warp" && std::wstring_view(argv[3]) != L"Hardware"))
    {
        std::fputs("--fixture-receipt requires FX<N>[@variant], Warp|Hardware and an output path.\n", stderr);
        return 2;
    }
    const FixtureSelection selection = ParseSelection(argv[2]);
    const Definition* definition     = FindFixture(selection.id);
    if (definition && definition->interactiveOnly)
    {
        // Only performance-fixtures.ps1 -Interactive runs it, on DxUi.ControlTests.exe under the desktop lease; no receipt here.
        std::cout << InteractiveNotRun(*definition) << '\n';
        return 0;
    }
    if (! definition || ! definition->Built() || ! Supports(definition->hosts, Host::Embedded))
    {
        std::fputs("--fixture-receipt names no fixture built for the embedded host.\n", stderr);
        return 2;
    }
    const Variant* variant = FindVariant(*definition, selection.variant);
    if (! variant)
    {
        std::fputs("--fixture-receipt names no variant of that fixture.\n", stderr);
        return 2;
    }
    WriteReceipt(*definition, *variant, std::wstring_view(argv[3]) == L"Hardware", argv[4], countDrawCalls);
    return 0;
}

// DxUi.EmbeddedTests.exe --fixture-catalog <output.json>
inline int RunCatalogCommand(int argc, wchar_t** argv)
{
    if (argc != 3)
    {
        std::fputs("--fixture-catalog requires an output path.\n", stderr);
        return 2;
    }
    std::ofstream output{std::filesystem::path(argv[2]), std::ios::binary};
    Check(bool(output), "performance fixture catalog file");
    WriteCatalogJson(output, kCatalog);
    output.close();
    Check(bool(output), "performance fixture catalog written");
    return 0;
}

// Two runs, one after the other in this process, must record equal counters in every measured phase.
inline void CheckDeterministic(const RunRecord& first, const RunRecord& second)
{
    if (const std::optional<Difference> difference = FirstDifference(first, second))
    {
        std::cerr << "Nondeterministic performance fixture: " << Describe(first, *difference) << '\n';
        Check(false, "two consecutive WARP runs of a performance fixture record equal counters");
    }
}

// The framework on its own fixture: per-step records, input through the host port, the fixture clock, and a comparator
// that names the counter and step of a difference and ignores warm-up phases.
__declspec(noinline) inline void TestFrameworkSelfTest()
{
    const Definition& definition = SelfTest::kDefinition;
    const Variant& variant       = SelfTest::kVariants[0];
    const RunRecord first        = RunEmbedded(definition, variant);
    const RunRecord second       = RunEmbedded(definition, variant);
    CheckDeterministic(first, second);
    Check(first.phases.size() == SelfTest::kPhases.size() && first.Phase("hover").steps.size() == 4u, "a run records every declared step");
    for (const StepRecord& step : first.Phase("clean").steps)
        Check(step.work.frame.preparations == 0u && step.work.frame.paintExecutions == 0u && step.cppAllocations == 0u &&
                  DxUi::Detail::TotalCreated(step.work) == 0u && step.work.frame.composites == 1u,
              "a clean self-test step prepares nothing, paints nothing, allocates nothing and composites once");
    for (const StepRecord& step : first.Phase("text").steps)
        Check(step.work.frame.preparations == 1u && step.work.frame.paintExecutions == SelfTest::kVisibleControls,
              "a text change prepares once and paints every visible control");
    Check(Summarize(first.Phase("hover"), "frame.treeVisitsHitTest").minimum > 0u, "every pointer step reaches the host's hit test");
    Check(Summarize(first.Phase("text"), "frame.paintExecutions").total == 3u * SelfTest::kVisibleControls, "a summary totals a counter over the phase");

    RunOptions wrapped;
    wrapped.countDrawCalls = true;
    const RunRecord drawn  = RunEmbedded(definition, variant, wrapped);
    Check(drawn.drawCallsCounted && Summarize(drawn.Phase("text"), "frame.drawCalls").minimum > 0u &&
              Summarize(drawn.Phase("clean"), "frame.drawCalls").maximum == 0u && Summarize(first.Phase("text"), "frame.drawCalls").maximum == 0u,
          "draw calls are counted per step exactly when the counting wrappers are installed");

    RunRecord changed = first;
    ++changed.phases[SelfTest::kText].steps[1].work.frame.paintExecutions;
    const std::optional<Difference> difference = FirstDifference(first, changed);
    Check(difference && difference->phase == "text" && difference->step == 1u && difference->counter == "frame.paintExecutions" &&
              difference->first + 1u == difference->second,
          "the comparator names the phase, step and counter that differ");
    Check(Describe(first, *difference).find("phase text step 1: frame.paintExecutions") != std::string::npos, "the difference is described for the failure");
    RunRecord warm = first;
    warm.phases[SelfTest::kWarm].steps[0].cppAllocations += 7u;
    Check(! FirstDifference(first, warm), "warm-up phases are recorded but not compared");

    std::ostringstream receipt;
    WriteReceiptJson(receipt, first);
    const std::string text = receipt.str();
    Check(text.find("\"workloadOwner\":\"DxUi\"") != std::string::npos && text.find("\"frame.paintExecutions\":[") != std::string::npos &&
              text.find("\"timing\":{\"status\":\"not measured\"") != std::string::npos && text.find("\"at\":\"hidden\"") != std::string::npos &&
              text.back() == '\n',
          "the receipt carries every counter series, the memory samples and the timing status");
    std::cout << "PASS performance fixture framework self-test\n";
}

// The determinism gate: each built catalog fixture on the embedded host, every WARP variant, run twice. Given a directory,
// the first run of each is written there as a receipt.
__declspec(noinline) inline void TestCatalogIsDeterministic(const wchar_t* artifactDirectory)
{
    size_t compared = 0u;
    for (const Definition* definition : kCatalog)
    {
        if (! definition->Built())
        {
            std::cout << "Performance fixture " << definition->id << ": declared, not built\n";
            continue;
        }
        if (definition->interactiveOnly)
        {
            std::cout << "Performance fixture " << InteractiveNotRun(*definition) << '\n';
            continue;
        }
        if (! Supports(definition->hosts, Host::Embedded))
            continue;
        for (const Variant& variant : definition->variants)
        {
            if (variant.hardwareOnly || variant.receiptOnly)
                continue;
            const auto started     = std::chrono::steady_clock::now();
            const RunRecord first  = RunEmbedded(*definition, variant);
            const RunRecord second = RunEmbedded(*definition, variant);
            Check(first.trackerOverflow == 0u && second.trackerOverflow == 0u, "the tracker observed every resource the fixture created");
            CheckDeterministic(first, second);
            ++compared;
            if (artifactDirectory)
                WriteReceiptFile(
                    first, std::filesystem::path(artifactDirectory) / (std::string(definition->id) + '@' + std::string(variant.name) + "-embedded-warp.json"));
            std::cout << "Performance fixture " << definition->id << '@' << variant.name << " (embedded, WARP): two runs record equal counters ("
                      << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count() << " ms)\n";
        }
    }
    Check(compared > 0u, "the determinism gate compared at least one fixture");
}

// The worked example of an item gate on a fixture: FX5 through the framework must report what the work-counter receipt
// reports (the W0.1 baseline, then each item's receipt that changes it) and carry the W0.3 memory fields. Its dirty step
// counts the same window as the work-counter frame (Update(20 + n), prepare, composite; see
// WorkCountersComplexUi::kReleaseDirtyFrameAllocations), so the two gates share that constant rather than pin two numbers.
// A mismatch is a finding about the framework or the library, never a reason to change these values.
__declspec(noinline) inline void TestFx5MatchesWorkCounterReceipt()
{
    const auto countVisible = [](const auto& self, const DxUi::Control* control) -> uint64_t
    {
        if (! control || ! control->IsVisible())
            return 0u;
        uint64_t count = 1u;
        for (size_t index = 0u; index < control->GetLogicalChildCount(); ++index)
            count += self(self, control->GetLogicalChild(index));
        return count;
    };
    uint64_t visibleControls = 0u;
    RunOptions options;
    options.afterPhases = [&](EmbeddedInstance& instance, GraphicsFixture&)
    { visibleControls = countVisible(countVisible, instance.View().Controls().GetRoot()); };
    const Variant& variant = Fx5::kVariants[0];
    const RunRecord run    = RunEmbedded(Fx5::kDefinition, variant, options);
    Check(run.trackerOverflow == 0u, "the tracker observed every resource FX5 created");
    Check(visibleControls == 83u, "FX5 paints 83 visible controls");
    const PhaseRecord& clean = run.Phase("clean");
    const PhaseRecord& dirty = run.Phase("dirty");
    Check(clean.steps.size() == Fx5::kCleanSteps && dirty.steps.size() == Fx5::kDirtySteps, "FX5 records 40 clean and 40 dirty frames");
    for (const StepRecord& frame : clean.steps)
        Check(frame.cppAllocations == 0u && DxUi::Detail::TotalCreated(frame.work) == 0u && DxUi::Detail::Churn(frame.work) == 0u &&
                  frame.work.frame.preparations == 0u && frame.work.frame.paintExecutions == 0u && frame.work.frame.composites == 1u,
              "an FX5 clean frame allocates, creates, churns, prepares and paints nothing and composites once");
    for (const StepRecord& frame : dirty.steps)
    {
        Check(frame.work.frame.preparations == 1u && frame.work.frame.composites == 1u, "an FX5 dirty frame prepares and composites once");
        Check(frame.work.frame.paintExecutions == visibleControls && frame.work.frame.treeVisitsPaint == visibleControls,
              "an FX5 dirty frame paints and visits every visible control once");
        Check(frame.work.frame.pixelsPainted == uint64_t(variant.width) * variant.height, "an FX5 dirty frame repaints the whole surface");
#if _ITERATOR_DEBUG_LEVEL == 0 && ! defined(__SANITIZE_ADDRESS__)
        Check(frame.cppAllocations == WorkCountersComplexUi::kReleaseDirtyFrameAllocations,
              "a Release FX5 dirty frame makes the work-counter receipt's C++ allocations");
#endif
    }
    const MemorySample& built  = run.memory[1];
    const MemorySample& warm   = run.memory[2];
    const MemorySample& hidden = run.memory.back();
    Check(built.at == "built" && warm.phase == "warm" && hidden.at == "hidden", "FX5 memory is sampled after the build, each phase and hidden");
    Check(built.trackedLiveBytes.value_or(0) > 0 && warm.trackedLiveBytes.value_or(0) > 0, "FX5 tracks the scene's live C++ bytes");
    Check(warm.retained && warm.retained->surfaceBytes == uint64_t(variant.width) * variant.height * 4u && hidden.retained &&
              hidden.retained->surfaceBytes == 0u,
          "FX5 holds its surface while shown and none hidden");
    Check(! warm.video && run.adapter.software, "a WARP run reports no video memory");
    const StepRecord& first = dirty.steps.front();
    std::cout << "FX5 on the fixture framework: dirty frame cppAllocations " << first.cppAllocations << ", paintExecutions " << first.work.frame.paintExecutions
              << ", created " << DxUi::Detail::TotalCreated(first.work) << ", churn " << DxUi::Detail::Churn(first.work) << "; clean frame cppAllocations "
              << clean.steps.front().cppAllocations << "; tracked live bytes built/warm " << *built.trackedLiveBytes << '/' << *warm.trackedLiveBytes << '\n';
}

// The W9.1 gate on FX5: every G13 line is reported as a counter on the fixture and compared with its budget; a line over
// budget is a recorded exception naming its closing item; the registry's per-cache breakdown adds up to the statistics.
__declspec(noinline) inline void TestFx5G13Budgets()
{
    RunOptions options;
    options.probeGridCells = true;
    DxUi::EmbeddedStatistics live{};
    options.afterPhases    = [&](EmbeddedInstance& instance, GraphicsFixture&) { live = instance.View().GetStatistics(); };
    const Variant& variant = Fx5::kVariants[0];
    const RunRecord run    = RunEmbedded(Fx5::kDefinition, variant, options);
    Check(std::ranges::find(Fx5::kBudgets, std::string_view("G13")) != Fx5::kBudgets.end(), "FX5 reports budget G13");
    Check(run.controls.value_or(0u) >= 83u && run.editors == 0u, "FX5 counts its controls and has no editor");

    // Per-cache entries and resident bytes come from the registry and add up to the statistics' totals.
    uint64_t entries = 0u, bytes = 0u;
    const CacheRecord* gridCache = nullptr;
    for (const CacheRecord& cache : run.caches)
    {
        if (cache.name == "EmbeddedHost.State.texture")
        {
            Check(cache.entries == 1u && cache.bytes == uint64_t(variant.width) * variant.height * 4u, "the surface row reports the surface bytes");
            continue;
        }
        entries += cache.entries;
        bytes += cache.bytes;
        if (cache.name == "Grid._cellTextLayouts")
            gridCache = &cache;
    }
    Check(live.retainedCacheEntries == entries && live.retainedCacheBytes == bytes, "EmbeddedStatistics totals equal the registry's per-cache rows");
    Check(gridCache && gridCache->entries > 0u && gridCache->bytesCounted && gridCache->bytes >= gridCache->entries * sizeof(void*),
          "the Grid's cell table reports its entries and resident bytes");
    Check(std::ranges::any_of(run.caches, [](const CacheRecord& cache) { return cache.name == "RichTextEditor.layout" && ! cache.bytesCounted; }),
          "a row without a byte count is reported as such");

    // The Grid cell probe released every retained cell layout (FX5's only retained layouts) and the heap bytes behind them.
    Check(run.gridCells && run.gridCells->layouts == live.retainedTextLayouts && run.gridCells->layouts > 0u,
          "the Grid cell probe releases every retained cell layout");
    Check(run.memory.back().at == "hidden" && run.memory.back().retained->retainedTextLayouts == 0u, "the probe leaves the scene to hide as before");

    const auto results = EvaluateG13(run);
    const auto find    = [&](std::string_view id) -> const BudgetResult&
    { return *std::ranges::find_if(results, [&](const BudgetResult& result) { return result.line->id == id; }); };
    const BudgetResult& idle    = find("idle-private");
    const BudgetResult& video   = find("minimized-video");
    const BudgetResult& control = find("per-control");
    const BudgetResult& cell    = find("per-grid-cell");
    const BudgetResult& history = find("text-history");
    std::cout << "FX5 G13: per control " << control.value.value_or(0u) << " B over " << *run.controls << " controls, per Grid cell " << cell.value.value_or(0u)
              << " B (rebuilding adds " << run.gridCells->rebuiltHeapBytes << " heap bytes, hiding returns " << run.gridCells->releasedHeapBytes << ", over "
              << run.gridCells->layouts << " cells), private above entry " << idle.value.value_or(0u) << " B (advisory), cache entries " << entries
              << " / resident bytes " << bytes << '\n';
    Check(idle.verdict == BudgetVerdict::Advisory && idle.value, "private bytes above entry are reported as advisory");
    Check(video.verdict == BudgetVerdict::NotMeasured && ! video.reason.empty(), "WARP reports no video memory, with the reason");
    Check(control.value == static_cast<uint64_t>(*run.memory[1].trackedLiveBytes) / *run.controls, "per-control bytes are the built tracked bytes per control");
    Check(history.verdict == BudgetVerdict::NotMeasured && history.reason == "the fixture has no TextField or ComboBox", "FX5 has no text history to judge");
    // Today's findings, each a recorded exception with the item that closes it (Measurements/.../W9.1/README.md).
    Check(control.verdict == BudgetVerdict::Exceeded && control.exception && control.exception->closingItem == "W9.6", "FX5 per-control bytes: exception W9.6");
    Check(cell.exception && cell.exception->closingItem == "W9.4", "FX5 per-cell bytes: exception W9.4");
    Check(cell.verdict == BudgetVerdict::NotMeasured && cell.reason.starts_with("software adapter"), "WARP leaves the cell line to hardware receipts");
    // The cell line as a hardware run evaluates it: today's 21 KB per cell is the recorded exception.
    RunRecord onHardware                   = run;
    onHardware.adapter.software            = false;
    onHardware.gridCells->rebuiltHeapBytes = static_cast<int64_t>(onHardware.gridCells->layouts * 20'965u);
    const auto hardwareCell                = EvaluateG13(onHardware)[3];
    Check(hardwareCell.verdict == BudgetVerdict::Exceeded && hardwareCell.value == 20'965u && hardwareCell.exception,
          "a hardware cell figure is judged against 1 KB");
    const std::vector<std::string> failures = G13Failures(run);
    for (const std::string& failure : failures)
        std::cerr << failure << '\n';
    Check(failures.empty(), "FX5 meets every G13 line or records the exception that closes it");

    // The gate itself: an exceeded line without an exception fails, and so does an exception on a line within budget.
    RunRecord unrecorded  = run;
    unrecorded.definition = &SelfTest::kDefinition;
    Check(std::ranges::any_of(G13Failures(unrecorded), [](const std::string& failure) { return failure.find("per-control") != std::string::npos; }),
          "an exceeded line without a recorded exception fails the gate");
    RunRecord fixed                   = onHardware;
    fixed.gridCells->rebuiltHeapBytes = static_cast<int64_t>(fixed.gridCells->layouts * 100u);
    Check(std::ranges::any_of(G13Failures(fixed),
                              [](const std::string& failure) { return failure.find("delete the recorded exception of W9.4") != std::string::npos; }),
          "a recorded exception on a line within budget fails the gate");

    std::ostringstream receipt;
    WriteReceiptJson(receipt, run);
    const std::string text = receipt.str();
    Check(text.find("\"budgetLines\":{\"G13\":[") != std::string::npos && text.find("\"status\":\"recorded exception\"") != std::string::npos &&
              text.find("\"retainedCaches\":[") != std::string::npos && text.find("\"gridCellRelease\":") != std::string::npos,
          "the receipt carries the G13 lines, the per-cache rows and the cell probe");
}

// The W9.1 gate on FX13, the whole application: its G13 lines are counted, and any over budget is a recorded exception.
__declspec(noinline) inline void TestFx13G13Budgets()
{
    RunOptions options;
    options.probeGridCells = true;
    const RunRecord run    = RunEmbedded(Fx13::kDefinition, Fx13::kVariants[0], options);
    const auto results     = EvaluateG13(run);
    for (const BudgetResult& result : results)
        std::cout << "FX13 G13 " << result.line->id << ": " << (result.value ? std::to_string(*result.value) : std::string("not measured")) << '\n';
    Check(run.editors.value_or(0u) > 0u && results[4].verdict == BudgetVerdict::Met, "FX13's search field holds a text history within budget");
    Check(results[2].value.has_value() && results[0].verdict == BudgetVerdict::Advisory, "FX13 counts bytes per control and reports private bytes as advisory");
    const std::vector<std::string> failures = G13Failures(run);
    for (const std::string& failure : failures)
        std::cerr << failure << '\n';
    Check(failures.empty(), "FX13 meets every G13 line or records the exception that closes it");
}
// W3.1's frame-schedule gate on the fixtures (Specs/Rendering/Rendering_Win32Host.md, "Frame schedule"). Every catalog
// fixture: each step ticks exactly the due requesters, and the library's own controls never take the legacy whole-tree path.
// FX1 editable (G1): the caret is ticked at most 20 times in 10 s, one deadline per blink, with no tree walk. The fixture's
// application still polls its request every step (PollEveryStep: it is not None while a deadline is pending), so wakes.embeddedAdvance is
// the polling application's, not the schedule's; the schedule's wakes are the steps whose advance ticked a due request.
__declspec(noinline) inline void TestFrameScheduleGatesOnFixtures()
{
    for (const Definition* definition : kCatalog)
    {
        // An interactive-only fixture takes the foreground: its own gate runs in the interactive Menu suite.
        if (definition->interactiveOnly || ! definition->Built() || ! Supports(definition->hosts, Host::Embedded))
            continue;
        const Variant& variant = definition->variants.front();
        if (variant.hardwareOnly)
            continue;
        const RunRecord run = RunEmbedded(*definition, variant);
        for (const PhaseRecord& phase : run.phases)
            for (const StepRecord& step : phase.steps)
            {
                Check(step.work.frame.tickCalls <= step.work.frame.tickRequesters, "a wake ticks no control without a due request");
                Check(step.work.frame.tickLegacyTreeWalks == 0u, "the library's own controls never take the legacy whole-tree tick");
                Check(step.work.frame.treeVisitsTick == 0u || step.work.frame.tickDiscoveryWalks != 0u,
                      "no step walks the tree to tick, except the discovery tick of a newly installed root");
            }
        std::cout << "Frame schedule on " << definition->id << " (embedded): ticks only due requesters, no legacy walk\n";
    }
    const RunRecord fx1     = RunEmbedded(Fx1::kDefinition, Fx1::kVariants[0]);
    uint64_t tickCalls      = 0u;
    uint64_t wakesWithTicks = 0u;
    for (const StepRecord& step : fx1.Phase("editable").steps)
    {
        Check(step.work.frame.tickCalls == step.work.frame.tickRequesters, "FX1: Tick calls per wake equal the requesting controls");
        tickCalls += step.work.frame.tickCalls;
        wakesWithTicks += step.work.frame.tickCalls != 0u ? 1u : 0u;
    }
    std::cout << "FX1 editable (embedded, fixture clock): " << tickCalls << " caret ticks at " << wakesWithTicks << " wakes in 10 s\n";
    // W3.2: the caret blinks for the 5 s caret timeout FX1 pins (10 flips at 530 ms), then nothing is armed.
    Check(wakesWithTicks <= 20u && tickCalls <= 20u && tickCalls >= 9u, "G1: FX1 editable needs at most 20 wakes in 10 s, one per caret blink");
    const RunRecord fx5 = RunEmbedded(Fx5::kDefinition, Fx5::kVariants[0]);
    for (const PhaseRecord& phase : fx5.phases)
        for (const StepRecord& step : phase.steps)
            Check(step.work.frame.tickCalls == step.work.frame.tickRequesters && step.work.frame.treeVisitsTick == 0u &&
                      step.work.frame.tickLegacyTreeWalks == 0u,
                  "FX5: Tick calls per wake equal the requesting controls, with no tree walk");
}
// W3.2's caret-clock gate on FX1 (Specs/Rendering/Rendering_Win32Host.md, "Caret clock"), embedded, on the fixture clock:
// motion on and the caret settings pinned to Windows' defaults (530 ms, 5 s timeout). A wake is a step whose advance ticked
// the caret. Editable: at most 20 wakes in 10 s, one per flip, and 0 wakes (no advance at all) after the timeout; read-only:
// 0 wakes and the caret present at every frame, with nothing armed. Also prints the paints per flip (one whole-surface
// preparation each: the host cannot invalidate only the caret yet, W3.7 and W4).
__declspec(noinline) inline void TestCaretClockGatesOnFx1()
{
    const RunRecord fx1 = RunEmbedded(Fx1::kDefinition, Fx1::kVariants[0]);
    // The last flip is at 5,300 ms of the view's timeline (the 5 s timeout); the timeline starts at the first advance after
    // the focus, so it lags the fixture clock by one step. Everything after the step of the last flip is "after the timeout".
    size_t wakes = 0u, preparations = 0u, paints = 0u, wakesAfterTimeout = 0u, advancesAfterTimeout = 0u, lastFlipStep = 0u;
    const PhaseRecord& editable = fx1.Phase("editable");
    for (size_t index = 0u; index < editable.steps.size(); ++index)
    {
        const StepRecord& step = editable.steps[index];
        Check(step.work.frame.tickCalls <= 1u, "FX1 editable: a step ticks the caret at most once");
        if (step.work.frame.tickCalls != 0u)
        {
            ++wakes;
            preparations += step.work.frame.preparations;
            paints += step.work.frame.paintExecutions;
            lastFlipStep = index;
        }
    }
    Check(lastFlipStep * Fx1::kStepMs <= 5'500u, "FX1 editable: the last flip comes at the 5 s caret timeout");
    for (size_t index = lastFlipStep + 1u; index < editable.steps.size(); ++index)
    {
        wakesAfterTimeout += editable.steps[index].work.frame.tickCalls;
        advancesAfterTimeout += editable.steps[index].work.wakes.embeddedAdvance;
    }
    std::cout << "FX1 editable (embedded, W3.2): " << wakes << " caret wakes in 10 s, the last at step " << lastFlipStep << ", " << wakesAfterTimeout
              << " after it; per flip " << (wakes ? preparations / wakes : 0u) << " preparation(s) and " << (wakes ? paints / wakes : 0u)
              << " paint executions\n";
    Check(wakes <= 20u && wakes == 10u, "W3.2: FX1 editable at the 530 ms default wakes once per flip, at most 20 times in 10 s");
    Check(wakesAfterTimeout == 0u && advancesAfterTimeout == 0u, "W3.2: 0 wakes after the caret timeout, not even a polling advance");
    for (const StepRecord& step : fx1.Phase("readonly").steps)
        Check(step.work.frame.tickCalls == 0u && step.work.wakes.embeddedAdvance == 0u, "W3.2: FX1 read-only wakes nothing");

    // The read-only caret at every frame, on FX1's own scene stepped by hand.
    GraphicsFixture gpu;
    Hr(gpu.Create(), "FX1 caret gate device");
    EmbeddedInstance instance;
    instance.Instantiate(Fx1::kDefinition);
    Hr(instance.Create(gpu.device.get(), Fx1::kVariants[0]), "FX1 caret gate scene");
    auto* const root     = instance.View().Controls().GetRoot();
    auto* const readOnly = root ? dynamic_cast<DxUi::TextField*>(root->GetLogicalChild(1u)) : nullptr;
    Check(readOnly && readOnly->IsReadOnly(), "FX1's second field is the read-only one");
    for (size_t step = 0u; step < Fx1::kPhases[Fx1::kWarm].steps; ++step)
    {
        instance.Step(Fx1::kWarm, step);
        Hr(instance.Frame(gpu), "FX1 caret gate warm frame");
    }
    for (size_t step = 0u; step < Fx1::kIdleSteps; ++step)
    {
        instance.Step(Fx1::kReadOnly, step);
        Hr(instance.Frame(gpu), "FX1 caret gate read-only frame");
        Check(readOnly->HasFocus() && readOnly->DebugIsCaretVisible(), "W3.2: the read-only caret is present in every frame");
        Check(instance.View().Controls().DebugGetFrameScheduleWakeForTest().kind == 0u && ! AnimationRequested(instance.View().GetAnimationRequest()),
              "W3.2: a focused read-only field schedules nothing");
    }
    std::cout << "FX1 read-only (embedded, W3.2): caret shown in all " << Fx1::kIdleSteps << " frames, nothing scheduled\n";
}

// W3.3's gate on FX14 (Specs/Rendering/Rendering_Win32Host.md, "Tooltip deadlines"): the tooltip is ticked at its show
// deadline and at its hide deadline, nothing ticks between them, and it never asks for every frame. The show step's first
// advance (+1 ms) only sets the view's timeline baseline, as every first advance after a pause does: it ticks nothing.
__declspec(noinline) inline void TestTooltipDeadlineGatesOnFx14()
{
    const RunRecord run   = RunEmbedded(Fx14::kDefinition, Fx14::kVariants[0]);
    uint64_t tooltipTicks = 0u;
    uint64_t tooltipWakes = 0u;
    for (const Phase& phase : Fx14::kPhases)
    {
        if (phase.kind != PhaseKind::Measured)
            continue;
        const uint64_t ticks    = Summarize(run.Phase(phase.name), "frame.tooltipDeadlineTicks").total;
        const uint64_t controls = Summarize(run.Phase(phase.name), "frame.tickCalls").total;
        const uint64_t advances = Summarize(run.Phase(phase.name), "wakes.embeddedAdvance").total;
        std::cout << "FX14 " << phase.name << " (embedded): " << ticks << " tooltip deadline ticks, " << controls << " control Tick calls, " << advances
                  << " advances\n";
        const bool atDeadline = phase.name == "show" || phase.name == "hide";
        Check(ticks == (atDeadline ? 1u : 0u), "FX14: the tooltip is ticked once at its show and once at its hide deadline, never between them");
        Check(controls == 0u, "FX14: no Tick call between the deadlines");
        tooltipTicks += ticks;
        for (const StepRecord& step : run.Phase(phase.name).steps)
            tooltipWakes += step.work.frame.tooltipDeadlineTicks != 0u ? 1u : 0u;
    }
    Check(tooltipTicks == 2u && tooltipWakes <= 2u, "G1: FX14 wakes from the tooltip <= 2 per show, one show and one hide");
}

// W3.5's gate (Specs/Rendering/Rendering_EmbeddedD3D11.md, "Animation request"): the same fixtures driven by an application
// that sleeps on the fixture clock until the view's request is due (ApplicationLoop::SleepUntilRequest), against the
// application that polls every step, the only one revision 5 allowed (PollEveryStep, the receipts' application). A wake is
// an advance the sleeping application slept until; never a real timer.
struct ApplicationRun
{
    RunRecord run;
    std::vector<ApplicationAdvance> advances;
};
inline ApplicationRun RunApplication(const Definition& definition,
                                     ApplicationLoop loop,
                                     std::function<void(EmbeddedInstance&, GraphicsFixture&)> afterPhases = {})
{
    ApplicationRun result;
    result.advances.reserve(1u << 16u);
    RunOptions options;
    options.applicationLoop = loop;
    options.applicationLog  = &result.advances;
    options.afterPhases     = std::move(afterPhases);
    result.run              = RunEmbedded(definition, definition.variants.front(), options);
    return result;
}
struct PhaseAdvances
{
    uint64_t advances = 0u, wakes = 0u, tickCalls = 0u, tickRequesters = 0u, treeVisitsTick = 0u, tooltipTicks = 0u, targetWalks = 0u;
};
inline PhaseAdvances SummarizeAdvances(const std::vector<ApplicationAdvance>& advances, size_t phase, size_t firstStep = 0u)
{
    PhaseAdvances sum;
    for (const ApplicationAdvance& advance : advances)
    {
        if (advance.phase != phase || advance.step < firstStep)
            continue;
        ++sum.advances;
        sum.wakes += advance.woke ? 1u : 0u;
        sum.tickCalls += advance.tickCalls;
        sum.tickRequesters += advance.tickRequesters;
        sum.treeVisitsTick += advance.treeVisitsTick;
        sum.tooltipTicks += advance.tooltipDeadlineTicks;
        sum.targetWalks += advance.tooltipTargetValidations;
    }
    return sum;
}
inline void PrintAdvances(std::string_view fixture, std::string_view phase, const PhaseAdvances& poll, const PhaseAdvances& sleep)
{
    std::cout << "W3.5 " << fixture << ' ' << phase << ": polling application " << poll.advances << " advances (" << poll.tickCalls << " Tick calls, "
              << poll.tooltipTicks << " tooltip ticks, " << poll.treeVisitsTick << " tick tree visits, " << poll.targetWalks
              << " tooltip target walks); sleeping application " << sleep.wakes << " wakes, " << sleep.advances << " advances (" << sleep.tickCalls
              << " Tick calls, " << sleep.tooltipTicks << " tooltip ticks, " << sleep.treeVisitsTick << " tick tree visits, " << sleep.targetWalks
              << " tooltip target walks)\n";
}
// Every advance of the sleeping application ticks exactly the due requests, and one it slept until for a deadline ticks
// something: it never wakes for nothing.
inline void CheckEveryAdvanceTicksOnlyTheDue(std::string_view fixture, const std::vector<ApplicationAdvance>& advances)
{
    for (const ApplicationAdvance& advance : advances)
    {
        Check(advance.tickCalls == advance.tickRequesters, "W3.5: an advance ticks exactly the controls whose requests are due");
        if (advance.woke && advance.request == DxUi::EmbeddedAnimationRequest::Kind::Deadline && advance.tickCalls + advance.tooltipDeadlineTicks == 0u)
        {
            std::cerr << "W3.5 " << fixture << ": empty deadline wake at phase " << advance.phase << " step " << advance.step << " time " << advance.atMs
                      << '\n';
            Check(false, "W3.5: a wake at a deadline ticks what is due, never nothing");
        }
    }
    std::cout << "W3.5 " << fixture << ": every advance ticks only the due requests; no deadline wake ticks nothing\n";
}
// The polling application's advances before a deadline (they tick nothing) run no wake at all: no tree visit and no
// supplemental-tooltip target walk. Returns how many there were.
inline size_t CheckEarlyAdvancesDoNoWork(const std::vector<ApplicationAdvance>& advances)
{
    size_t early = 0u;
    for (const ApplicationAdvance& advance : advances)
    {
        if (advance.request != DxUi::EmbeddedAnimationRequest::Kind::Deadline || advance.tickCalls + advance.tooltipDeadlineTicks != 0u)
            continue;
        ++early;
        Check(advance.treeVisitsTick == 0u && advance.tooltipTargetValidations == 0u, "W3.5: an advance before the deadline walks nothing");
    }
    return early;
}

__declspec(noinline) inline void TestApplicationWakeGatesOnFixtures()
{
    using Kind = DxUi::EmbeddedAnimationRequest::Kind;
    // FX1: the editable caret blinks for its 5 s timeout. The sleeping application wakes once per flip, at most twice per
    // blink period (two phases of 530 ms), and never after the timeout; the read-only caret wakes nothing. After the phases
    // the editable field is focused again (blinking) and the view hidden for 10 s: 0 wakes, and the request is None.
    size_t hiddenAdvances = 0u, shownAdvances = 0u;
    Kind hiddenRequest           = Kind::Continuous;
    const auto hideWhileBlinking = [&](EmbeddedInstance& instance, GraphicsFixture&)
    {
        auto* const editable = dynamic_cast<DxUi::TextField*>(instance.View().Controls().GetRoot()->GetLogicalChild(0u));
        Check(editable && ! editable->IsReadOnly(), "FX1's first field is the editable one");
        instance.SetLoggedPhase(Fx1::kPhases.size());
        instance.View().Controls().SetFocusControl(editable, false);
        const DxUi::Detail::WorkCounterScope shown;
        instance.AdvanceTime(2u * Fx1::kCaretSettings.blinkTimeMs);
        shownAdvances = static_cast<size_t>(shown.Delta().wakes.embeddedAdvance);
        instance.View().SetVisible(false);
        instance.ApplicationChangedView();
        hiddenRequest = instance.View().GetAnimationRequest().kind;
        const DxUi::Detail::WorkCounterScope hidden;
        instance.AdvanceTime(10'000u);
        hiddenAdvances = static_cast<size_t>(hidden.Delta().wakes.embeddedAdvance);
    };
    const ApplicationRun fx1Poll  = RunApplication(Fx1::kDefinition, ApplicationLoop::PollEveryStep, hideWhileBlinking);
    const size_t pollHidden       = hiddenAdvances;
    const ApplicationRun fx1Sleep = RunApplication(Fx1::kDefinition, ApplicationLoop::SleepUntilRequest, hideWhileBlinking);
    Check(shownAdvances >= 2u, "FX1: the refocused caret wakes the sleeping application before the view is hidden");
    Check(hiddenRequest == Kind::None && hiddenAdvances == 0u && pollHidden == 0u, "W3.5: FX1 hidden requests nothing and wakes 0 times, while blinking");
    CheckEveryAdvanceTicksOnlyTheDue("FX1", fx1Sleep.advances);
    std::cout << "W3.5 FX1: " << CheckEarlyAdvancesDoNoWork(fx1Poll.advances) << " polling advances before a deadline, each walking nothing\n";
    const uint64_t blinkPeriodMs = 2u * Fx1::kCaretSettings.blinkTimeMs;
    for (const size_t phase : {Fx1::kEditable, Fx1::kMultiline})
    {
        std::vector<uint64_t> wakeTimes;
        uint64_t flips = 0u, lastFlipMs = 0u;
        for (const ApplicationAdvance& advance : fx1Sleep.advances)
        {
            if (advance.phase != phase)
                continue;
            if (advance.woke)
                wakeTimes.push_back(advance.atMs);
            if (advance.tickCalls != 0u && advance.treeVisitsTick == 0u)
            {
                ++flips;
                lastFlipMs = advance.atMs;
            }
        }
        size_t mostInOnePeriod = 0u;
        for (size_t first = 0u; first < wakeTimes.size(); ++first)
        {
            size_t inPeriod = 0u;
            for (size_t next = first; next < wakeTimes.size() && wakeTimes[next] < wakeTimes[first] + blinkPeriodMs; ++next)
                ++inPeriod;
            mostInOnePeriod = (std::max)(mostInOnePeriod, inPeriod);
        }
        size_t afterLastFlip = 0u;
        for (const ApplicationAdvance& advance : fx1Sleep.advances)
            afterLastFlip += advance.phase == phase && advance.atMs > lastFlipMs ? 1u : 0u;
        const PhaseAdvances poll  = SummarizeAdvances(fx1Poll.advances, phase);
        const PhaseAdvances sleep = SummarizeAdvances(fx1Sleep.advances, phase);
        PrintAdvances("FX1", Fx1::kPhases[phase].name, poll, sleep);
        std::cout << "W3.5 FX1 " << Fx1::kPhases[phase].name << ": " << flips << " caret flips, at most " << mostInOnePeriod << " wakes in one "
                  << blinkPeriodMs << " ms blink period, " << afterLastFlip << " advances after the last flip\n";
        // Ten flips in the 5 s caret timeout. The polling application ticks the same flips on FX1's 100 ms steps; its 500 ms
        // multiline steps tick each flip late and can merge two into one tick.
        Check(flips == 10u, "W3.5: the sleeping application ticks every caret flip of the 5 s caret timeout, once");
        if (phase == Fx1::kEditable)
            Check(sleep.tickCalls == poll.tickCalls, "W3.5: the sleeping and the polling application tick the caret equally often");
        Check(mostInOnePeriod <= 2u, "W3.5 gate: FX1 at most 2 application wakes per blink period");
        Check(afterLastFlip == 0u, "W3.5: 0 wakes after the caret timeout");
        // The polling application advances every step while anything waits: on 100 ms steps far more often; on the 500 ms
        // multiline steps about as often as one flip per wake.
        Check(sleep.wakes <= poll.advances && (phase != Fx1::kEditable || sleep.advances < poll.advances),
              "W3.5: the sleeping application never wakes more often than the polling one advances");
    }
    const PhaseAdvances readOnly = SummarizeAdvances(fx1Sleep.advances, Fx1::kReadOnly);
    PrintAdvances("FX1", "readonly", SummarizeAdvances(fx1Poll.advances, Fx1::kReadOnly), readOnly);
    Check(readOnly.advances == 0u, "W3.5: FX1 read-only: 0 application wakes");

    // FX13: 0 wakes with nothing armed (after the new root's one discovery frame at the first step), 0 with its steady caret
    // (reduced motion), and 0 while hidden for 60 s after the phases.
    size_t fx13Hidden = 0u;
    const auto hide60 = [&](EmbeddedInstance& instance, GraphicsFixture&)
    {
        instance.View().SetVisible(false);
        instance.ApplicationChangedView();
        const DxUi::Detail::WorkCounterScope hidden;
        instance.AdvanceTime(60'000u);
        fx13Hidden = static_cast<size_t>(hidden.Delta().wakes.embeddedAdvance);
    };
    const ApplicationRun fx13Poll  = RunApplication(Fx13::kDefinition, ApplicationLoop::PollEveryStep);
    const ApplicationRun fx13Sleep = RunApplication(Fx13::kDefinition, ApplicationLoop::SleepUntilRequest, hide60);
    CheckEveryAdvanceTicksOnlyTheDue("FX13", fx13Sleep.advances);
    std::cout << "W3.5 FX13: " << CheckEarlyAdvancesDoNoWork(fx13Poll.advances) << " polling advances before a deadline, each walking nothing\n";
    for (const size_t phase : {Fx13::kNothingArmed, Fx13::kCaret, Fx13::kCaretPeriod})
    {
        const PhaseAdvances sleep = SummarizeAdvances(fx13Sleep.advances, phase);
        PrintAdvances("FX13", Fx13::kPhases[phase].name, SummarizeAdvances(fx13Poll.advances, phase), sleep);
        const PhaseAdvances idle = SummarizeAdvances(fx13Sleep.advances, phase, phase == Fx13::kNothingArmed ? 1u : 0u);
        Check(idle.advances == 0u && idle.wakes == 0u, "W3.5 gate: FX13 idle: 0 application wakes");
    }
    Check(SummarizeAdvances(fx13Sleep.advances, Fx13::kNothingArmed).treeVisitsTick > 0u,
          "FX13: the one advance at the first idle step is the new root's discovery");
    std::cout << "W3.5 FX13 hidden for 60 s: " << fx13Hidden << " wakes\n";
    Check(fx13Hidden == 0u, "W3.5 gate: FX13 hidden: 0 application wakes");

    // FX14: the sleeping application wakes for the tooltip twice, once to show it and once to hide it.
    const ApplicationRun fx14Poll  = RunApplication(Fx14::kDefinition, ApplicationLoop::PollEveryStep);
    const ApplicationRun fx14Sleep = RunApplication(Fx14::kDefinition, ApplicationLoop::SleepUntilRequest);
    CheckEveryAdvanceTicksOnlyTheDue("FX14", fx14Sleep.advances);
    std::cout << "W3.5 FX14: " << CheckEarlyAdvancesDoNoWork(fx14Poll.advances) << " polling advances before a deadline, each walking nothing\n";
    uint64_t tooltipWakes = 0u, tooltipTicks = 0u;
    for (const Phase& phase : Fx14::kPhases)
    {
        const size_t index        = static_cast<size_t>(&phase - Fx14::kPhases.data());
        const PhaseAdvances sleep = SummarizeAdvances(fx14Sleep.advances, index);
        PrintAdvances("FX14", phase.name, SummarizeAdvances(fx14Poll.advances, index), sleep);
        tooltipTicks += sleep.tooltipTicks;
    }
    // The sleeping application wakes exactly at the show (400 ms after the hover) and the hide (5 s later), so the hide
    // comes inside the show step, and the 50 moves of the shown phase then schedule a second show, which the hide step's
    // 5 s reach. The polling application, on the fixture's coarse steps, sees one show and one hide. Either way every
    // tooltip deadline is one wake, and a show's cycle is two: the show, then the hide 5 s later with nothing between.
    std::vector<uint64_t> wakeTimes;
    std::vector<uint64_t> tooltipWakeTimes;
    for (const ApplicationAdvance& advance : fx14Sleep.advances)
    {
        if (! advance.woke)
            continue;
        wakeTimes.push_back(advance.atMs);
        if (advance.tooltipDeadlineTicks != 0u)
        {
            ++tooltipWakes;
            tooltipWakeTimes.push_back(advance.atMs);
        }
    }
    const uint64_t shows = (tooltipTicks + 1u) / 2u;
    std::cout << "W3.5 FX14: " << tooltipWakes << " tooltip wakes for " << tooltipTicks << " tooltip deadlines (" << shows << " shows)\n";
    Check(tooltipWakes == tooltipTicks && tooltipWakes <= 2u * shows, "W3.5 gate: FX14 at most 2 application wakes per tooltip show, one show and one hide");
    Check(tooltipWakeTimes.size() >= 2u && tooltipWakeTimes[1] == tooltipWakeTimes[0] + Fx14::kDisplayLifetimeMs,
          "FX14: the hide is one wake, 5 s after the show");
    for (const uint64_t at : wakeTimes)
        Check(at <= tooltipWakeTimes[0] || at >= tooltipWakeTimes[1], "FX14: nothing wakes the sleeping application between the show and the hide");
}

// The timing harness's view of a catalog fixture: performance-timing.ps1 -Phase Dirty plays the timed phase, one step per
// frame; -Phase Clean renders unchanged frames. Phases before the timed phase are played (prepared, not composited) first.
class CatalogTimingFixture final : public FrameTiming::Fixture
{
public:
    CatalogTimingFixture(const Definition& definition, const Variant& variant)
        : definition_(definition),
          variant_(variant),
          name_(variant.name == definition.variants.front().name ? std::string(definition.id) : std::string(definition.id) + '@' + std::string(variant.name)),
          timedPhase_(definition.FindPhase(definition.timedPhase).value_or(0u))
    {
    }
    std::string_view Name() const noexcept override
    {
        return name_;
    }
    std::string_view Description() const noexcept override
    {
        return definition_.title;
    }
    UINT Width() const noexcept override
    {
        return variant_.width;
    }
    UINT Height() const noexcept override
    {
        return variant_.height;
    }
    float Dpi() const noexcept override
    {
        return variant_.dpi;
    }
    // Builds the scene and plays the phases before the timed phase (prepared, not composited). BeforeFrame runs it again.
    HRESULT Initialize(ID3D11Device* device) override
    {
        device_ = device;
        instance_.reset(); // first: a scene's teardown can undo a process-wide test setting that the next Build sets
        instance_.emplace();
        instance_->Instantiate(definition_);
        RETURN_IF_FAILED(instance_->Create(device, variant_));
        for (size_t phase = 0u; phase < timedPhase_; ++phase)
            for (size_t step = 0u; step < definition_.phases[phase].steps; ++step)
            {
                instance_->Step(phase, step);
                RETURN_IF_FAILED(instance_->Prepare());
            }
        return S_OK;
    }
    // Untimed: when the next dirty frame would start a finite timed phase again, the scene is rebuilt (Definition::timedPhaseRepeats).
    void BeforeFrame(size_t frame, FrameTiming::Phase phase) override
    {
        const size_t steps = TimedSteps();
        if (phase == FrameTiming::Phase::Dirty && steps != 0u && frame != 0u && frame % steps == 0u)
        {
            Hr(Initialize(device_), "frame timing rebuilds the fixture before its timed phase repeats");
            ++rebuilds_;
        }
    }
    void Update(size_t frame, FrameTiming::Phase phase) override
    {
        const size_t steps = TimedSteps();
        if (phase == FrameTiming::Phase::Dirty)
            instance_->Step(timedPhase_, steps == 0u ? frame : frame % steps);
    }
    HRESULT Prepare() override
    {
        return instance_->Prepare();
    }
    HRESULT Composite(ID3D11DeviceContext* context, const D3D11_VIEWPORT& viewport) override
    {
        return instance_->View().Composite(context, viewport);
    }
    size_t TimedSteps() const noexcept override
    {
        return definition_.timedPhaseRepeats ? 0u : definition_.phases[timedPhase_].steps;
    }
    size_t Rebuilds() const noexcept override
    {
        return rebuilds_;
    }

private:
    const Definition& definition_;
    const Variant& variant_;
    std::string name_;
    size_t timedPhase_    = 0u;
    ID3D11Device* device_ = nullptr; // the timing harness's device, alive for every frame of the run
    size_t rebuilds_      = 0u;
    std::optional<EmbeddedInstance> instance_;
};
} // namespace PerformanceFixtures

// Declared in Embedded.Tests.FrameTiming.h: every built embedded catalog fixture, as FX<N> or FX<N>@<variant>.
inline std::unique_ptr<FrameTiming::Fixture> FrameTiming::MakeFixture(std::wstring_view name)
{
    using namespace PerformanceFixtures;
    const FixtureSelection selection = ParseSelection(name);
    const Definition* definition     = FindFixture(selection.id);
    if (! definition || definition->interactiveOnly || ! definition->Built() || ! Supports(definition->hosts, Host::Embedded) ||
        ! definition->FindPhase(definition->timedPhase))
        return nullptr;
    const Variant* variant = FindVariant(*definition, selection.variant);
    if (! variant)
        return nullptr;
    return std::make_unique<CatalogTimingFixture>(*definition, *variant);
}

// Declared in Embedded.Tests.FrameTiming.h: an interactive-only catalog fixture is never timed; says so and returns true.
inline bool FrameTiming::SkipsFixture(std::wstring_view name)
{
    using namespace PerformanceFixtures;
    const Definition* definition = FindFixture(ParseSelection(name).id);
    if (! definition || ! definition->interactiveOnly)
        return false;
    std::cout << InteractiveNotRun(*definition) << '\n';
    return true;
}
#pragma warning(pop)
