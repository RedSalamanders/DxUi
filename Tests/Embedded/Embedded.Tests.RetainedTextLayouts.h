#pragma once
// Retained text layouts (plan item W2.1) on the embedded host. Included once from Embedded.Tests.Embedded.cpp after the
// performance fixtures and the allocation instrument; uses Check and Hr from there.
//
// - TestFx5RetainedTextLayoutGate: the item's gate on FX5. Every step of every phase: the host's table holds no more
//   entries than the strings its last paint drew (the registry row ControlHost._textLayoutCache); every hover frame creates
//   0 text layouts and lays out nothing inside DrawText (G11); a first frame whose strings are all new creates its layouts
//   in preparation while composition creates nothing and allocates nothing (A3); and each of FX5's labels measures with 0
//   C++ allocations.
// - TestLabelMeasureAllocatesNothing: a single-line, a wrapped multi-line and a typography-override Label measure with 0
//   C++ allocations; a label with more lines than the stack buffer reads its line metrics on the heap, counted.
#include "Controls/DxUi.Internal.h"
#include "Controls/DxUi.RetainedCaches.h"

namespace RetainedTextLayoutTests
{
[[nodiscard]] inline uint64_t TextLayoutCacheEntries(DxUi::EmbeddedHost& view)
{
    std::vector<DxUi::RetainedCacheCount> counts;
    DxUi::RetainedCacheAccess::Count(view, counts);
    for (const DxUi::RetainedCacheCount& count : counts)
        if (count.name == "ControlHost._textLayoutCache")
            return count.entries;
    Check(false, "the registry has a row for the host's text layout table");
    return 0u;
}

template <typename Visit> void VisitLabels(DxUi::Control* control, const Visit& visit)
{
    if (! control)
        return;
    if (auto* label = dynamic_cast<DxUi::Label*>(control))
        visit(*label);
    for (size_t index = 0u; index < control->GetLogicalChildCount(); ++index)
        VisitLabels(control->GetLogicalChild(index), visit);
}

// C++ allocations of measuring `label` once at `constraints`.
[[nodiscard]] inline size_t MeasureAllocations(DxUi::ControlHost& host, const DxUi::Label& label, const DxUi::LayoutConstraints& constraints, HRESULT& result)
{
    DxUi::DesiredSize desired{};
    const DxUi::MeasureContext context{host};
    allocations      = 0u;
    countAllocations = true;
    result           = label.Measure(context, constraints, desired);
    countAllocations = false;
    return allocations;
}

// A Label whose measurement can be invalidated from the test, so that every measure runs Label::MeasureOverride rather
// than answering from Control::Measure's cache (W6.2).
class MeasureProbeLabel final : public DxUi::Label
{
public:
    explicit MeasureProbeLabel(std::wstring text) : DxUi::Label(std::move(text))
    {
    }
    MeasureProbeLabel(const MeasureProbeLabel&)            = delete;
    MeasureProbeLabel& operator=(const MeasureProbeLabel&) = delete;
    MeasureProbeLabel(MeasureProbeLabel&&)                 = delete;
    MeasureProbeLabel& operator=(MeasureProbeLabel&&)      = delete;
    ~MeasureProbeLabel() override                          = default;
    void Invalidate() noexcept
    {
        InvalidateMeasure();
    }
};

__declspec(noinline) inline void TestFx5RetainedTextLayoutGate()
{
    using namespace PerformanceFixtures;
    const Definition& definition = Fx5::kDefinition;
    const Variant& variant       = Fx5::kVariants[0];
    GraphicsFixture gpu;
    gpu.width  = variant.width;
    gpu.height = variant.height;
    Hr(gpu.Create(true), "FX5 retained text layout gate device");
    EmbeddedInstance instance;
    instance.Instantiate(definition);
    Hr(instance.Create(gpu.device.get(), variant), "FX5 retained text layout gate build");

    // The first frame: every string is new. Preparation creates the layouts; composition only composites.
    {
        const DxUi::Detail::WorkCounterScope prepare;
        Hr(instance.Prepare(), "FX5 first preparation");
        const DxUi::Detail::WorkCounters prepared = prepare.Delta();
        gpu.Bind();
        const DxUi::Detail::WorkCounterScope compose;
        allocations      = 0u;
        countAllocations = true;
        Hr(instance.View().Composite(gpu.context.get(), gpu.Viewport()), "FX5 first composition");
        countAllocations                          = false;
        const size_t compositionAllocations       = allocations;
        const DxUi::Detail::WorkCounters composed = compose.Delta();
        std::cout << "  FX5 first frame: preparation created " << prepared.text.textLayoutCacheMisses << " retained layouts ("
                  << prepared.resources.textLayout.created << " text layouts in all); composition created " << DxUi::Detail::TotalCreated(composed)
                  << " resources and made " << compositionAllocations << " C++ allocations\n";
        Check(prepared.text.textLayoutCacheMisses > 0u && prepared.resources.implicitTextLayout.created == 0u,
              "FX5's first preparation creates its strings' layouts and lays out nothing inside DrawText");
        Check(DxUi::Detail::TotalCreated(composed) == 0u && composed.text.textLayoutCacheMisses == 0u && compositionAllocations == 0u,
              "FX5's first composition creates nothing and allocates nothing");
    }

    // Every step: the table holds at most the strings the last paint drew (and at least one when it drew any).
    uint64_t hoverFrames = 0u, maximumEntries = 0u;
    for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
        for (size_t step = 0u; step < definition.phases[phase].steps; ++step)
        {
            const DxUi::Detail::WorkCounterScope scope;
            instance.Step(phase, step);
            Hr(instance.Frame(gpu), "FX5 retained text layout gate frame");
            const DxUi::Detail::WorkCounters work = scope.Delta();
            const uint64_t drawn                  = work.text.textLayoutCacheHits + work.text.textLayoutCacheMisses;
            const uint64_t entries                = TextLayoutCacheEntries(instance.View());
            maximumEntries                        = (std::max)(maximumEntries, entries);
            if (work.frame.preparations != 0u)
                Check(entries <= drawn && entries > 0u, "FX5: resident entries never exceed the strings the last paint drew");
            Check(work.text.textLayoutCacheOverflows == 0u, "FX5 never reaches the table's bound");
            if (definition.phases[phase].name == "hover")
            {
                ++hoverFrames;
                Check(work.frame.preparations == 1u && work.frame.paintExecutions > 0u, "an FX5 hover step repaints");
                Check(work.resources.textLayout.created == 0u && work.resources.implicitTextLayout.created == 0u,
                      "G11: an FX5 hover frame creates 0 text layouts, explicit or inside DrawText");
            }
            if (definition.phases[phase].name == "dirty")
                Check(work.resources.implicitTextLayout.created == 0u, "an FX5 dirty frame lays out nothing inside DrawText");
        }
    Check(hoverFrames == Fx5::kHoverSteps, "FX5 records its hover frames");

    // Every label of FX5 measures without a C++ allocation. Its first measurement also creates the control's measure cache
    // (W6.2), one block per control for its lifetime; every later measure is a cache hit or a Label::MeasureOverride run,
    // and neither allocates (TestLabelMeasureAllocatesNothing forces the runs).
    size_t labels = 0u;
    VisitLabels(instance.View().Controls().GetRoot(),
                [&](DxUi::Label& label)
    {
        HRESULT measured = E_FAIL;
        static_cast<void>(MeasureAllocations(instance.View().Controls(), label, {}, measured));
        Hr(measured, "an FX5 label measures");
        DxUi::LayoutConstraints narrower{};
        narrower.maximumWidthDip = 400.0f;
        for (const DxUi::LayoutConstraints& constraints : {DxUi::LayoutConstraints{}, narrower})
        {
            const size_t counted = MeasureAllocations(instance.View().Controls(), label, constraints, measured);
            Hr(measured, "an FX5 label measures again");
            Check(counted == 0u, "an FX5 Label measure makes 0 C++ allocations");
        }
        ++labels;
    });
    Check(labels == 16u, "FX5 measures its 16 labels");
    std::cout << "PASS FX5 retained text layouts: " << hoverFrames << " hover frames create 0 layouts; at most " << maximumEntries << " resident entries; "
              << labels << " label measures allocate nothing\n";
}

__declspec(noinline) inline void TestLabelMeasureAllocatesNothing(GraphicsFixture& gpu)
{
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "label measure graphics device");
    DxUi::EmbeddedHost view;
    Hr(view.Attach(std::move(graphics)), "label measure view");
    auto root     = std::make_unique<DxUi::Panel>();
    auto* single  = root->AddChild<MeasureProbeLabel>(L"Metric 7");
    auto* wrapped = root->AddChild<MeasureProbeLabel>(L"A label long enough to wrap onto a few lines at a narrow width");
    auto* tall    = root->AddChild<MeasureProbeLabel>(L"1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20");
    auto* typed   = root->AddChild<MeasureProbeLabel>(L"Typed label");
    wrapped->SetMultiline(true);
    tall->SetMultiline(true);
    Hr(typed->SetTypographyOverride(DxUi::LabelTypography{15.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, 0.5f}), "label typography override");
    view.Controls().SetRoot(std::move(root));
    Hr(view.Prepare(320u, 240u), "label measure preparation");
    DxUi::LayoutConstraints narrow{};
    narrow.maximumWidthDip = 80.0f;
    for (const auto& [label, constraints, what] : {std::tuple{single, DxUi::LayoutConstraints{}, "single-line"},
                                                   std::tuple{wrapped, narrow, "wrapped"},
                                                   std::tuple{typed, DxUi::LayoutConstraints{}, "typography override"}})
    {
        HRESULT measured = E_FAIL;
        static_cast<void>(MeasureAllocations(view.Controls(), *label, constraints, measured)); // formats and the measure cache
        label->Invalidate();                                                                   // the next measure runs the override
        const DxUi::Detail::WorkCounterScope run;
        const size_t counted = MeasureAllocations(view.Controls(), *label, constraints, measured);
        Hr(measured, "a label measures");
        Check(run.Delta().layout.measureOverrideExecutions == 1u, "the measure ran Label::MeasureOverride, not a cache hit");
        std::cout << "  Label measure (" << what << "): " << counted << " C++ allocations\n";
        Check(counted == 0u, "a Label measure makes 0 C++ allocations");
    }
    // Twenty lines exceed the stack buffer of line metrics: that one measure reads them into a counted heap buffer.
    tall->Invalidate();
    const DxUi::Detail::WorkCounterScope scope;
    HRESULT measured             = E_FAIL;
    const size_t tallAllocations = MeasureAllocations(view.Controls(), *tall, {}, measured);
    Hr(measured, "a twenty-line label measures");
    Check(scope.Delta().text.lineMetricsHeapFallbacks == 1u && tallAllocations > 0u,
          "a label with more lines than the stack buffer reads its line metrics into a counted heap buffer");
    view.Detach();
    std::cout << "PASS Label measure allocates nothing below 17 lines\n";
}

// The bound's balance measurement (W2.1 receipt): DxUi.EmbeddedTests.exe --measure-text-layout-bound FX<N>[@variant]
// <bound> <frames>. Bound 0 draws every string through DrawText (the table off); any other bound lowers the table's
// ceiling to it (a power of two). The fixture plays every phase once, then `frames` unchanged frames (ControlHost::
// Invalidate, a full repaint of the same strings) after two settling ones. Per frame: the UI thread's cycles in Prepare
// (QueryThreadCycleTime), the strings drawn and the layouts created. One JSON line on stdout; one process per run, so the
// private bytes of different bounds are compared between processes.
inline int RunBoundMeasurement(int argc, wchar_t** argv)
{
    using namespace PerformanceFixtures;
    if (argc != 5)
    {
        std::fputs("--measure-text-layout-bound requires FX<N>[@variant], a bound and a frame count.\n", stderr);
        return 2;
    }
    const FixtureSelection selection = ParseSelection(argv[2]);
    const Definition* definition     = FindFixture(selection.id);
    const Variant* variant           = definition ? FindVariant(*definition, selection.variant) : nullptr;
    const size_t bound               = static_cast<size_t>(std::wcstoull(argv[3], nullptr, 10));
    const size_t frames              = static_cast<size_t>(std::wcstoull(argv[4], nullptr, 10));
    if (! variant || ! definition->Built() || ! Supports(definition->hosts, Host::Embedded) || frames == 0u)
    {
        std::fputs("--measure-text-layout-bound names no embedded fixture variant.\n", stderr);
        return 2;
    }
    GraphicsFixture gpu;
    gpu.width  = variant->width;
    gpu.height = variant->height;
    Hr(gpu.Create(true), "bound measurement device");
    EmbeddedInstance instance;
    instance.Instantiate(*definition);
    Hr(instance.Create(gpu.device.get(), *variant), "bound measurement build");
    DxUi::ControlHost& host = instance.View().Controls();
    host.DebugSetTextLayoutCacheEnabledForTest(bound != 0u);
    host.DebugSetTextLayoutCacheLimitForTest(bound);
    for (size_t phase = 0u; phase < definition->phases.size(); ++phase)
        for (size_t step = 0u; step < definition->phases[phase].steps; ++step)
        {
            instance.Step(phase, step);
            Hr(instance.Frame(gpu), "bound measurement phase frame");
        }
    std::vector<uint64_t> cycles;
    cycles.reserve(frames);
    uint64_t drawn = 0u, created = 0u, overflow = 0u, maximumCreated = 0u;
    for (size_t frame = 0u; frame < frames + 2u; ++frame)
    {
        host.Invalidate();
        const DxUi::Detail::WorkCounterScope scope;
        ULONG64 before = 0u, after = 0u;
        QueryThreadCycleTime(GetCurrentThread(), &before);
        Hr(instance.Prepare(), "bound measurement preparation");
        QueryThreadCycleTime(GetCurrentThread(), &after);
        gpu.Bind();
        Hr(instance.View().Composite(gpu.context.get(), gpu.Viewport()), "bound measurement composition");
        gpu.context->Flush();
        if (frame < 2u)
            continue;
        const DxUi::Detail::WorkCounters work = scope.Delta();
        const uint64_t frameCreated           = work.resources.textLayout.created + work.resources.implicitTextLayout.created;
        drawn = work.text.textLayoutCacheHits + work.text.textLayoutCacheMisses + work.text.textLayoutCacheBypasses + work.text.textLayoutCacheOverflows;
        created += frameCreated;
        maximumCreated = (std::max)(maximumCreated, frameCreated);
        overflow += work.text.textLayoutCacheOverflows;
        cycles.push_back(after - before);
    }
    std::vector<uint64_t> sorted = cycles;
    std::ranges::sort(sorted);
    const auto percentile                   = [&](size_t p) { return sorted[(sorted.size() - 1u) * p / 100u]; };
    const PROCESS_MEMORY_COUNTERS_EX memory = ComplexUiBenchmark::Memory();
    // DirectWrite's bytes per retained layout, in this process: the live heap bytes that releasing the table returns, and
    // those that one more unchanged frame adds when it creates every layout again.
    const size_t entries  = host.DebugGetTextLayoutCacheEntries();
    const size_t eligible = host.DebugCountGlyphRunEligibleTextLayouts();
    const size_t capacity = host.DebugGetTextLayoutCacheCapacity();
    gpu.context->Flush();
    const uint64_t heapRetained = MemoryMetricsTests::LiveHeapBytes();
    DxUi::TextLayoutCacheAccess::Release(host);
    const uint64_t heapReleased = MemoryMetricsTests::LiveHeapBytes();
    host.Invalidate();
    Hr(instance.Frame(gpu), "bound measurement rebuild frame");
    gpu.context->Flush();
    const uint64_t heapRebuilt = MemoryMetricsTests::LiveHeapBytes();
    std::cout << "{\"fixture\":\"" << definition->id << '@' << variant->name << "\",\"bound\":" << bound << ",\"frames\":" << frames
              << ",\"stringsPerFrame\":" << drawn << ",\"entries\":" << entries
              << ",\"releasedHeapBytes\":" << static_cast<int64_t>(heapRetained) - static_cast<int64_t>(heapReleased)
              << ",\"rebuiltHeapBytes\":" << static_cast<int64_t>(heapRebuilt) - static_cast<int64_t>(heapReleased) << ",\"glyphRunEligible\":" << eligible
              << ",\"tableEntries\":" << capacity << ",\"layoutsCreatedPerFrame\":" << static_cast<double>(created) / static_cast<double>(frames)
              << ",\"maxLayoutsCreatedInAFrame\":" << maximumCreated << ",\"overflowPerFrame\":" << static_cast<double>(overflow) / static_cast<double>(frames)
              << ",\"prepareCyclesMedian\":" << percentile(50u) << ",\"prepareCyclesP10\":" << percentile(10u) << ",\"prepareCyclesP90\":" << percentile(90u)
              << ",\"privateBytes\":" << memory.PrivateUsage << "}\n";
    return 0;
}

inline void Run(GraphicsFixture& gpu)
{
    TestFx5RetainedTextLayoutGate();
    TestLabelMeasureAllocatesNothing(gpu);
}
} // namespace RetainedTextLayoutTests
