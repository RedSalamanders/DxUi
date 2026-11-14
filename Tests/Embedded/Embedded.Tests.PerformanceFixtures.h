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
#include "Support.Tests.CountingDirect2D.h"
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027) // scenarios are never copied
#include "Embedded.Tests.PerformanceFixtureSelfTest.h"
#pragma warning(pop)

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
namespace PerformanceFixtures
{
// A fixture live on an EmbeddedHost. Instantiate, then Create (attach and build), then any sequence of Step and Frame.
class EmbeddedInstance final : public HostPort
{
public:
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
            RETURN_IF_FAILED(ownedView_->Attach(std::move(graphics)));
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
    void Step(size_t phase, size_t step)
    {
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
        now_ += milliseconds;
        if (view_->NeedsAnimation())
            static_cast<void>(view_->AdvanceAnimation(now_));
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
    const Definition* definition_ = nullptr;
    const Variant* variant_       = nullptr;
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

struct RunOptions
{
    bool hardware = false;
    // Installs the counting COM wrappers (Support.Tests.CountingDirect2D.h) before the host's graphics device exists, so
    // frame.drawCalls counts every Direct2D draw call; without them that counter stays 0.
    bool countDrawCalls            = false;
    size_t trackedResourceCapacity = 16384u;
    // Called after the last phase, before the view is hidden: a gate test inspects the live scene or measures its own
    // action here. It runs outside every counted step.
    std::function<void(EmbeddedInstance&, GraphicsFixture&)> afterPhases;
};

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
    liveBytes      = 0;
    countLiveBytes = true;
    Hr(instance.Create(gpu.device.get(), variant), "performance fixture build");
    sample("built", {}, true);
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
                const HRESULT framed        = instance.Frame(gpu);
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
        if (options.afterPhases)
            options.afterPhases(instance, gpu);
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
        if (! Supports(definition->hosts, Host::Embedded))
            continue;
        for (const Variant& variant : definition->variants)
        {
            if (variant.hardwareOnly)
                continue;
            const RunRecord first  = RunEmbedded(*definition, variant);
            const RunRecord second = RunEmbedded(*definition, variant);
            Check(first.trackerOverflow == 0u && second.trackerOverflow == 0u, "the tracker observed every resource the fixture created");
            CheckDeterministic(first, second);
            ++compared;
            if (artifactDirectory)
                WriteReceiptFile(
                    first, std::filesystem::path(artifactDirectory) / (std::string(definition->id) + '@' + std::string(variant.name) + "-embedded-warp.json"));
            std::cout << "Performance fixture " << definition->id << '@' << variant.name << " (embedded, WARP): two runs record equal counters\n";
        }
    }
    Check(compared > 0u, "the determinism gate compared at least one fixture");
}

// The worked example of an item gate on a fixture: FX5 through the framework must report what the W0.1 work-counter
// receipt reports (Measurements/PerformanceAndMotion_2026-11-13/W0.1) and carry the W0.3 memory fields. A mismatch is a
// finding about the framework or the library, never a reason to change these values.
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
        Check(frame.cppAllocations == 54u, "a Release FX5 dirty frame makes the W0.1 receipt's 54 C++ allocations");
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
    HRESULT Initialize(ID3D11Device* device) override
    {
        instance_.Instantiate(definition_);
        RETURN_IF_FAILED(instance_.Create(device, variant_));
        for (size_t phase = 0u; phase < timedPhase_; ++phase)
            for (size_t step = 0u; step < definition_.phases[phase].steps; ++step)
            {
                instance_.Step(phase, step);
                RETURN_IF_FAILED(instance_.Prepare());
            }
        return S_OK;
    }
    void Update(size_t frame, FrameTiming::Phase phase) override
    {
        if (phase == FrameTiming::Phase::Dirty)
            instance_.Step(timedPhase_, frame);
    }
    HRESULT Prepare() override
    {
        return instance_.Prepare();
    }
    HRESULT Composite(ID3D11DeviceContext* context, const D3D11_VIEWPORT& viewport) override
    {
        return instance_.View().Composite(context, viewport);
    }

private:
    const Definition& definition_;
    const Variant& variant_;
    std::string name_;
    size_t timedPhase_ = 0u;
    EmbeddedInstance instance_;
};
} // namespace PerformanceFixtures

// Declared in Embedded.Tests.FrameTiming.h: every built embedded catalog fixture, as FX<N> or FX<N>@<variant>.
inline std::unique_ptr<FrameTiming::Fixture> FrameTiming::MakeFixture(std::wstring_view name)
{
    using namespace PerformanceFixtures;
    const FixtureSelection selection = ParseSelection(name);
    const Definition* definition     = FindFixture(selection.id);
    if (! definition || ! definition->Built() || ! Supports(definition->hosts, Host::Embedded) || ! definition->FindPhase(definition->timedPhase))
        return nullptr;
    const Variant* variant = FindVariant(*definition, selection.variant);
    if (! variant)
        return nullptr;
    return std::make_unique<CatalogTimingFixture>(*definition, *variant);
}
#pragma warning(pop)
