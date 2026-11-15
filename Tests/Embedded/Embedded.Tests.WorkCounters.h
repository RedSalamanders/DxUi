#pragma once
// Work counters on the ComplexUi fixture (FX5 of the performance and motion plan): 83 controls at 1280x720, 96 DPI, WARP,
// offscreen, reduced motion, driven exactly as the canonical benchmark drives it. Included by Embedded.Tests.Embedded.cpp
// after Embedded.Tests.BenchmarkInstrument.h, whose replaced allocation functions count the C++ allocations of each frame.
// The canonical benchmark header is left unchanged: paired runs copy it onto older trees that have no work counters.
//
// A run can install the counting COM wrappers of Support.Tests.CountingDirect2D.h before the scene creates its graphics
// device. They then see every creation that reaches Direct2D and DirectWrite and every Direct2D draw call, and the run
// requires, frame by frame, that they saw exactly the creations the counted helpers reported, kind by kind.
#include "ComplexUiScene.h"
#include "Support.Tests.CountingDirect2D.h"
#include "Support/WorkCounters.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace WorkCountersComplexUi
{
inline constexpr UINT kWidth                = 1280u;
inline constexpr UINT kHeight               = 720u;
inline constexpr size_t kWarmFrames         = 20u;
inline constexpr size_t kFramesPerScenario  = 40u;
inline constexpr size_t kComplexUiControls  = 83u;
inline constexpr size_t kTrackedResourceCap = 16384u;
// The C++ allocations of one dirty ComplexUi frame in a Release build (no debug iterators). Definition: after 20 warm-up and
// 40 clean frames, the allocation sink's count from ComplexUiScene::Update(20 + n) through Prepare and Composite on the
// 1280x720 WARP target, the WorkCounterScope constructed before counting starts; every one of the 40 dirty frames must
// equal it. The FX5 fixture's dirty step (Fx5::Scene::Step, then EmbeddedInstance::Frame, which also flushes) counts the
// same calls in the same window, so its per-step value and the FX5 receipt's dirty series equal this per-frame value; the
// trace provider's quiet frame is this frame again. The W0.1 baseline receipt and
// the benchmark sink (Measurements/EmbeddedInteractionLayout/Default-B1.json) recorded 54; W2.5's Grid column-description
// cache removed the 52 "Column N" titles the scene's Grid model formatted for each header and visible cell, and its receipt
// (Measurements/PerformanceAndMotion_2026-11-13/W2.5/WorkCounters) records the remaining 2: the paint's visible-item list
// and the model's cell text. A change that alters it changes this value with its own receipt; it is never rebaselined
// silently. The FX5 fixture gate and the trace-provider gate pin the same constant.
inline constexpr size_t kReleaseDirtyFrameAllocations = 2u;

struct FrameRecord final
{
    DxUi::Detail::WorkCounters work;
    size_t cppAllocations = 0u;
    // Only with the counting wrappers installed: what they saw in this frame.
    DxUi::Detail::ResourceWork seen;
    uint64_t otherCreations    = 0u;
    uint64_t escapedInterfaces = 0u;
    uint64_t unwrappedTargets  = 0u;
    // Only when pixels are hashed: the composited frame's FNV-1a hash, and the time of Prepare plus Composite.
    uint64_t pixelHash = 0u;
    double frameMs     = 0.0;
};

struct RunOptions final
{
    const wchar_t* receiptPath = nullptr;
    bool wrapped               = false; // Install the counting wrappers and cross-check them against the helpers.
    bool hashPixels            = false; // Read back and hash every measured frame, and time Prepare plus Composite.
};

struct RunResult final
{
    std::vector<FrameRecord> clean;
    std::vector<FrameRecord> dirty;
    std::vector<uint8_t> lastDirtyPixels;
    // From scene initialization through the warm-up frames: the helpers' counts and, when wrapped, the wrappers'.
    DxUi::Detail::WorkCounters setup;
    DxUi::Detail::ResourceWork setupSeen;
};

// Every control that a paint of the tree reaches: a visible control and its visible logical descendants.
[[nodiscard]] inline size_t CountVisibleControls(const DxUi::Control* control) noexcept
{
    if (! control || ! control->IsVisible())
        return 0u;
    size_t count = 1u;
    for (size_t index = 0u; index < control->GetLogicalChildCount(); ++index)
        count += CountVisibleControls(control->GetLogicalChild(index));
    return count;
}

[[nodiscard]] inline const char* ConfigurationName() noexcept
{
#if defined(__SANITIZE_ADDRESS__)
    return "ASan Debug";
#elif defined(_DEBUG)
    return "Debug";
#else
    return "Release";
#endif
}

[[nodiscard]] inline uint64_t HashPixels(const std::vector<uint8_t>& pixels) noexcept
{
    uint64_t hash = 14695981039346656037ull;
    for (const uint8_t value : pixels)
    {
        hash ^= value;
        hash *= 1099511628211ull;
    }
    return hash;
}

[[nodiscard]] inline double MedianMs(const std::vector<FrameRecord>& frames)
{
    std::vector<double> values;
    values.reserve(frames.size());
    for (const FrameRecord& frame : frames)
        values.push_back(frame.frameMs);
    std::sort(values.begin(), values.end());
    return values.empty() ? 0.0 : values[values.size() / 2u];
}

// One scenario's counters as {"name":{"min":…,"max":…,"total":…}} grouped by counter group, plus the allocation sink,
// total creations and churn of each frame. A wrapped run adds the wrappers' creations by kind beside the helpers'.
inline void WriteScenario(std::ostream& output, const char* name, const std::vector<FrameRecord>& frames, bool wrapped)
{
    struct Series
    {
        std::string group;
        std::string name;
        uint64_t minimum = (std::numeric_limits<uint64_t>::max)();
        uint64_t maximum = 0u;
        uint64_t total   = 0u;
        void Add(uint64_t value)
        {
            minimum = (std::min)(minimum, value);
            maximum = (std::max)(maximum, value);
            total += value;
        }
    };
    std::vector<Series> series;
    series.push_back({"", "cppAllocations"});
    series.push_back({"", "created"});
    series.push_back({"", "churn"});
    DxUi::Detail::ForEachWorkCounter(frames.front().work, [&](const char* group, const char* counter, uint64_t) { series.push_back({group, counter}); });
    if (wrapped)
    {
        for (size_t index = 0u; index < static_cast<size_t>(DxUi::Detail::ResourceKind::Count); ++index)
            if (const auto kind = static_cast<DxUi::Detail::ResourceKind>(index); NativeWorkTests::WrappersSee(kind))
                series.push_back({"wrappers", std::string(NativeWorkTests::ResourceKindName(kind)) + ".created"});
        series.push_back({"wrappers", "otherCreations"});
        series.push_back({"wrappers", "escapedInterfaces"});
        series.push_back({"wrappers", "unwrappedTargets"});
    }
    for (const FrameRecord& frame : frames)
    {
        size_t index = 0u;
        series[index++].Add(frame.cppAllocations);
        series[index++].Add(DxUi::Detail::TotalCreated(frame.work));
        series[index++].Add(DxUi::Detail::Churn(frame.work));
        DxUi::Detail::ForEachWorkCounter(frame.work, [&](const char*, const char*, uint64_t value) { series[index++].Add(value); });
        if (wrapped)
        {
            for (size_t kindIndex = 0u; kindIndex < static_cast<size_t>(DxUi::Detail::ResourceKind::Count); ++kindIndex)
                if (const auto kind = static_cast<DxUi::Detail::ResourceKind>(kindIndex); NativeWorkTests::WrappersSee(kind))
                    series[index++].Add(DxUi::Detail::Tally(frame.seen, kind).created);
            series[index++].Add(frame.otherCreations);
            series[index++].Add(frame.escapedInterfaces);
            series[index++].Add(frame.unwrappedTargets);
        }
    }
    const auto writeSeries = [&](const Series& value)
    { output << '"' << value.name << "\":{\"min\":" << value.minimum << ",\"max\":" << value.maximum << ",\"total\":" << value.total << '}'; };
    output << "{\"name\":\"" << name << "\",\"frames\":" << frames.size() << ",\"work\":{";
    std::string openGroup;
    bool first = true;
    for (const Series& value : series)
    {
        if (value.group != openGroup)
        {
            if (! openGroup.empty())
                output << '}';
            output << ",\"" << value.group << "\":{";
            openGroup = value.group;
            first     = true;
        }
        else if (! first)
            output << ',';
        writeSeries(value);
        first = false;
    }
    if (! openGroup.empty())
        output << '}';
    output << "}}";
}

// Measures 40 clean and 40 dirty frames after 20 warm-up frames, checks the work-counter gate and, given a path, writes the
// receipt. A ResourceLifetimeTracker observes destructions for the whole run, so churn is measured, not assumed.
inline RunResult Run(const RunOptions& options)
{
    // Declared first, so it outlives every object the scene creates through it.
    std::optional<NativeWorkTests::CountingWrappers> wrappers;
    if (options.wrapped)
        wrappers.emplace();
    RunResult result;
    auto& clean              = result.clean;
    auto& dirty              = result.dirty;
    size_t visibleControls   = 0u;
    size_t trackedHeld       = 0u;
    uint64_t trackerOverflow = 0u;
    {
        GraphicsFixture gpu;
        gpu.width  = kWidth;
        gpu.height = kHeight;
        Hr(gpu.Create(), "work counters WARP device");
        // Setup (attachment, first layout and the warm-up frames) is cross-checked as a whole as well.
        const DxUi::Detail::WorkCounterScope setupScope;
        ComplexUiScene scene;
        Hr(scene.Initialize(gpu.device.get()), "work counters ComplexUi scene");
        auto& view = scene.view;

        std::vector<DxUi::Detail::TrackedResource> storage(kTrackedResourceCap);
        DxUi::Detail::ResourceLifetimeTracker tracker(storage);
        for (size_t frame = 0u; frame < kWarmFrames; ++frame)
        {
            scene.Update(frame);
            Hr(view.Prepare(kWidth, kHeight), "work counters warm preparation");
            gpu.Bind();
            Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "work counters warm composition");
        }
        static_cast<void>(tracker.Sweep());
        result.setup = setupScope.Delta();
        if (wrappers)
            result.setupSeen = wrappers->Seen();

        visibleControls = CountVisibleControls(view.Controls().GetRoot());
        Check(visibleControls == kComplexUiControls, "the ComplexUi fixture paints 83 visible controls");

        clean.reserve(kFramesPerScenario);
        dirty.reserve(kFramesPerScenario);
        std::vector<uint8_t> pixels;
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        const auto measure = [&](std::vector<FrameRecord>& records, bool isDirty, size_t frame)
        {
            const DxUi::Detail::ResourceWork seenBefore = wrappers ? wrappers->Seen() : DxUi::Detail::ResourceWork{};
            const uint64_t otherBefore                  = wrappers ? wrappers->OtherCreations() : 0u;
            const uint64_t escapedBefore                = wrappers ? wrappers->EscapedInterfaces() : 0u;
            const uint64_t targetsBefore                = wrappers ? wrappers->direct2D.unwrappedTargets : 0u;
            LARGE_INTEGER start{};
            LARGE_INTEGER end{};
            const DxUi::Detail::WorkCounterScope scope;
            allocations      = 0u;
            countAllocations = true;
            QueryPerformanceCounter(&start);
            if (isDirty)
                scene.Update(frame);
            Hr(view.Prepare(kWidth, kHeight), "work counters preparation");
            gpu.Bind();
            Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "work counters composition");
            QueryPerformanceCounter(&end);
            countAllocations          = false;
            const size_t cppAllocated = allocations;
            static_cast<void>(tracker.Sweep());
            FrameRecord record{scope.Delta(), cppAllocated};
            if (wrappers)
            {
                record.seen              = NativeWorkTests::CreatedSince(wrappers->Seen(), seenBefore);
                record.otherCreations    = wrappers->OtherCreations() - otherBefore;
                record.escapedInterfaces = wrappers->EscapedInterfaces() - escapedBefore;
                record.unwrappedTargets  = wrappers->direct2D.unwrappedTargets - targetsBefore;
            }
            if (options.hashPixels)
            {
                record.frameMs = double(end.QuadPart - start.QuadPart) * 1000.0 / double(frequency.QuadPart);
                Hr(gpu.Read(pixels), "work counters frame readback");
                record.pixelHash = HashPixels(pixels);
            }
            records.push_back(record);
        };
        for (size_t frame = 0u; frame < kFramesPerScenario; ++frame)
            measure(clean, false, 0u);
        for (size_t frame = 0u; frame < kFramesPerScenario; ++frame)
            measure(dirty, true, kWarmFrames + frame);
        if (options.hashPixels)
            result.lastDirtyPixels = std::move(pixels);
        trackedHeld     = tracker.Held();
        trackerOverflow = tracker.Overflow();
    }
    Check(trackerOverflow == 0u, "the tracker observed every resource the frames created");

    for (const FrameRecord& frame : clean)
    {
        Check(frame.cppAllocations == 0u, "a clean ComplexUi frame makes no C++ allocation");
        Check(DxUi::Detail::TotalCreated(frame.work) == 0u, "a clean ComplexUi frame creates no counted resource");
        Check(DxUi::Detail::Churn(frame.work) == 0u, "a clean ComplexUi frame has no resource churn");
        Check(frame.work.frame.preparations == 0u && frame.work.frame.paintExecutions == 0u && frame.work.frame.treeVisitsPaint == 0u,
              "a clean ComplexUi frame neither prepares nor paints");
        Check(frame.work.frame.composites == 1u, "a clean ComplexUi frame composites once");
        Check(frame.work.frame.drawCalls == 0u, "a clean ComplexUi frame makes no Direct2D draw call");
    }
    for (const FrameRecord& frame : dirty)
    {
        Check(frame.work.frame.preparations == 1u && frame.work.frame.composites == 1u, "a dirty ComplexUi frame prepares and composites once");
        Check(frame.work.frame.paintExecutions == visibleControls, "a dirty ComplexUi frame paints every visible control once");
        Check(frame.work.frame.treeVisitsPaint == visibleControls, "the dirty paint walk examines every control once");
        Check(frame.work.frame.pixelsPainted == uint64_t(kWidth) * kHeight, "a dirty ComplexUi frame repaints the whole surface");
#if _ITERATOR_DEBUG_LEVEL == 0 && ! defined(__SANITIZE_ADDRESS__)
        Check(frame.cppAllocations == kReleaseDirtyFrameAllocations, "a Release dirty ComplexUi frame makes the receipt's 2 C++ allocations");
#endif
        Check(options.wrapped ? frame.work.frame.drawCalls != 0u : frame.work.frame.drawCalls == 0u,
              "Direct2D draw calls are counted exactly where the counting wrappers are installed");
    }
    if (wrappers)
    {
        // The cross-check: on every measured frame the wrappers saw exactly the creations the helpers counted, kind by kind
        // (0 and 0 on clean frames), nothing of a kind the helpers do not count, and no interface escaped them.
        const auto crossCheck = [](const FrameRecord& frame, const char* scenario)
        {
            std::array<NativeWorkTests::CreationMismatch, 4> mismatches{};
            const size_t count = NativeWorkTests::CompareCreations(frame.work.resources, frame.seen, mismatches);
            for (size_t index = 0u; index < (std::min)(count, mismatches.size()); ++index)
                std::cout << "Counting wrappers mismatch (" << scenario << " frame): " << NativeWorkTests::ResourceKindName(mismatches[index].kind)
                          << " helpers " << mismatches[index].helpers << ", wrappers " << mismatches[index].wrappers << '\n';
            Check(count == 0u, "the counting wrappers saw exactly the creations the helpers counted, kind by kind");
            Check(frame.otherCreations == 0u && frame.unwrappedTargets == 0u, "no frame creates an object of a kind the helpers do not count");
            Check(frame.escapedInterfaces == 0u, "no interface escapes the counting wrappers during a frame");
        };
        for (const FrameRecord& frame : clean)
        {
            crossCheck(frame, "clean");
            DxUi::Detail::WorkCounters seenOnly{};
            seenOnly.resources = frame.seen;
            Check(DxUi::Detail::TotalCreated(seenOnly) == 0u && DxUi::Detail::TotalCreated(frame.work) == 0u,
                  "a clean frame creates nothing, by the helpers' count and the wrappers'");
        }
        for (const FrameRecord& frame : dirty)
            crossCheck(frame, "dirty");
        {
            std::array<NativeWorkTests::CreationMismatch, 8> mismatches{};
            const size_t count = NativeWorkTests::CompareCreations(result.setup.resources, result.setupSeen, mismatches);
            for (size_t index = 0u; index < (std::min)(count, mismatches.size()); ++index)
                std::cout << "Counting wrappers mismatch (setup): " << NativeWorkTests::ResourceKindName(mismatches[index].kind) << " helpers "
                          << mismatches[index].helpers << ", wrappers " << mismatches[index].wrappers << '\n';
            Check(count == 0u, "over setup too, the counting wrappers saw exactly the creations the helpers counted");
        }
        // Lifetime is unchanged: releasing the scene released every wrapper, so it released every real object too.
        Check(wrappers->direct2D.liveWrappers == 0u, "the scene's teardown released every counting wrapper");
        Check(wrappers->direct2D.wrappedFactories == 1u && wrappers->direct2D.wrappedDevices == 1u && wrappers->writeFactoryCount == 1u &&
                  wrappers->unrecordedWriteFactories == 0u,
              "the ComplexUi graphics device was created through one wrapped factory pair and device");
    }

    std::cout << "Work counters ComplexUi" << (options.wrapped ? " (counting wrappers)" : "") << ": dirty frame cppAllocations " << dirty.front().cppAllocations
              << ", paintExecutions " << dirty.front().work.frame.paintExecutions << ", created " << DxUi::Detail::TotalCreated(dirty.front().work)
              << ", churn " << DxUi::Detail::Churn(dirty.front().work) << ", drawCalls " << dirty.front().work.frame.drawCalls
              << "; clean frame cppAllocations " << clean.front().cppAllocations << '\n';
    if (! options.receiptPath)
        return result;
    std::ofstream output{std::filesystem::path(options.receiptPath)};
    Check(bool(output), "work counters receipt file");
    output << "{\"fixture\":\"dxui-complex-ui-v2\",\"plan\":\"FX5\",\"renderer\":\"WARP\",\"configuration\":\"" << ConfigurationName()
           << "\",\"iteratorDebugLevel\":" << _ITERATOR_DEBUG_LEVEL << ",\"compiler\":" << _MSC_FULL_VER << ",\"width\":" << kWidth << ",\"height\":" << kHeight
           << ",\"dpi\":96,\"controls\":" << kComplexUiControls << ",\"visibleControls\":" << visibleControls << ",\"warmFrames\":" << kWarmFrames
           << ",\"trackedResources\":" << trackedHeld << ",\"trackerOverflow\":" << trackerOverflow;
    if (wrappers)
        output << ",\"countingWrappers\":{\"factories\":" << wrappers->direct2D.wrappedFactories << ",\"devices\":" << wrappers->direct2D.wrappedDevices
               << ",\"deviceContexts\":" << wrappers->direct2D.wrappedContexts << ",\"writeFactories\":" << wrappers->writeFactoryCount
               << ",\"otherCreations\":" << wrappers->OtherCreations() << ",\"escapedInterfaces\":" << wrappers->EscapedInterfaces()
               << ",\"unwrappedTargets\":" << wrappers->direct2D.unwrappedTargets << ",\"liveAfterTeardown\":" << wrappers->direct2D.liveWrappers;
    if (wrappers)
    {
        // Creations from scene initialization through the warm-up frames, kind by kind: helpers and wrappers.
        output << ",\"setupCreated\":{";
        bool firstKind = true;
        for (size_t index = 0u; index < static_cast<size_t>(DxUi::Detail::ResourceKind::Count); ++index)
        {
            const auto kind = static_cast<DxUi::Detail::ResourceKind>(index);
            if (! NativeWorkTests::WrappersSee(kind))
                continue;
            output << (firstKind ? "" : ",") << '"' << NativeWorkTests::ResourceKindName(kind)
                   << "\":{\"helpers\":" << DxUi::Detail::Tally(result.setup.resources, kind).created
                   << ",\"wrappers\":" << DxUi::Detail::Tally(result.setupSeen, kind).created << '}';
            firstKind = false;
        }
        output << "}}";
    }
    output << ",\"scenarios\":[";
    WriteScenario(output, "clean", clean, options.wrapped);
    output << ',';
    WriteScenario(output, "dirty", dirty, options.wrapped);
    output << "]}\n";
    output.close();
    Check(bool(output), "work counters receipt written");
    return result;
}

inline RunResult Run(const wchar_t* receiptPath)
{
    return Run(RunOptions{receiptPath});
}

// Pixel identity and cost of the counting wrappers on FX5: runs the fixture without and with them in the order
// unwrapped, wrapped (and, for a receipt, wrapped, unwrapped again, so that order does not favour either), requires every
// measured frame's composited pixels to be identical, and reports the median Prepare-plus-Composite time of each.
inline void RunWrapperComparison(const wchar_t* wrappedReceiptPath, const wchar_t* comparisonPath)
{
    const bool receipt = comparisonPath != nullptr;
    std::vector<RunResult> unwrapped;
    std::vector<RunResult> wrapped;
    unwrapped.push_back(Run(RunOptions{nullptr, false, true}));
    wrapped.push_back(Run(RunOptions{wrappedReceiptPath, true, true}));
    if (receipt)
    {
        wrapped.push_back(Run(RunOptions{nullptr, true, true}));
        unwrapped.push_back(Run(RunOptions{nullptr, false, true}));
    }
    const RunResult& reference = unwrapped.front();
    const auto samePixels      = [&](const RunResult& candidate)
    {
        const auto sameHashes = [](const std::vector<FrameRecord>& left, const std::vector<FrameRecord>& right)
        {
            return left.size() == right.size() &&
                   std::equal(left.begin(), left.end(), right.begin(), [](const FrameRecord& a, const FrameRecord& b) { return a.pixelHash == b.pixelHash; });
        };
        return sameHashes(reference.clean, candidate.clean) && sameHashes(reference.dirty, candidate.dirty) &&
               reference.lastDirtyPixels == candidate.lastDirtyPixels;
    };
    for (const RunResult& run : wrapped)
        Check(samePixels(run), "the counting wrappers change no pixel of any measured ComplexUi frame");
    for (const RunResult& run : unwrapped)
        Check(samePixels(run), "repeated unwrapped ComplexUi runs produce identical pixels");
    uint64_t dirtyDrawCalls = 0u;
    for (const FrameRecord& frame : wrapped.front().dirty)
        dirtyDrawCalls += frame.work.frame.drawCalls;
    for (const RunResult& run : wrapped)
    {
        uint64_t total = 0u;
        for (const FrameRecord& frame : run.dirty)
            total += frame.work.frame.drawCalls;
        Check(total == dirtyDrawCalls, "consecutive wrapped runs count the same Direct2D draw calls");
    }
    const auto medians = [](const std::vector<RunResult>& runs, bool isDirty)
    {
        std::vector<double> values;
        for (const RunResult& run : runs)
            values.push_back(MedianMs(isDirty ? run.dirty : run.clean));
        return values;
    };
    const auto mean = [](const std::vector<double>& values)
    {
        double total = 0.0;
        for (const double value : values)
            total += value;
        return values.empty() ? 0.0 : total / double(values.size());
    };
    const double unwrappedDirty = mean(medians(unwrapped, true));
    const double wrappedDirty   = mean(medians(wrapped, true));
    const double unwrappedClean = mean(medians(unwrapped, false));
    const double wrappedClean   = mean(medians(wrapped, false));
    std::cout << "Counting wrappers ComplexUi: pixels identical over " << (unwrapped.size() + wrapped.size()) << " runs; dirty draw calls per 40 frames "
              << dirtyDrawCalls << "; median dirty frame " << unwrappedDirty << " ms unwrapped, " << wrappedDirty << " ms wrapped; median clean frame "
              << unwrappedClean << " ms unwrapped, " << wrappedClean << " ms wrapped\n";
    if (! receipt)
        return;
    std::ofstream output{std::filesystem::path(comparisonPath)};
    Check(bool(output), "counting wrappers comparison file");
    const auto writeList = [&](const std::vector<double>& values)
    {
        output << '[';
        for (size_t index = 0u; index < values.size(); ++index)
            output << (index ? "," : "") << values[index];
        output << ']';
    };
    output << "{\"fixture\":\"dxui-complex-ui-v2\",\"plan\":\"FX5\",\"renderer\":\"WARP\",\"configuration\":\"" << ConfigurationName()
           << "\",\"compiler\":" << _MSC_FULL_VER << ",\"order\":[\"unwrapped\",\"wrapped\",\"wrapped\",\"unwrapped\"],\"pixelsIdentical\":true"
           << ",\"lastDirtyFrameHash\":" << HashPixels(reference.lastDirtyPixels) << ",\"dirtyDrawCallsPer40Frames\":" << dirtyDrawCalls
           << ",\"medianDirtyFrameMs\":{\"unwrapped\":";
    writeList(medians(unwrapped, true));
    output << ",\"wrapped\":";
    writeList(medians(wrapped, true));
    output << "},\"medianCleanFrameMs\":{\"unwrapped\":";
    writeList(medians(unwrapped, false));
    output << ",\"wrapped\":";
    writeList(medians(wrapped, false));
    output << "}}\n";
    output.close();
    Check(bool(output), "counting wrappers comparison written");
}

// A deliberately uncounted creation: this control creates its brush directly on the host's device context, bypassing
// CountedCreateSolidColorBrush. The helpers cannot count it; the wrappers see it.
class UncountedBrushProbe final : public DxUi::Control
{
public:
    UncountedBrushProbe()                                      = default;
    UncountedBrushProbe(const UncountedBrushProbe&)            = delete;
    UncountedBrushProbe& operator=(const UncountedBrushProbe&) = delete;
    UncountedBrushProbe(UncountedBrushProbe&&)                 = delete;
    UncountedBrushProbe& operator=(UncountedBrushProbe&&)      = delete;
    ~UncountedBrushProbe() override                            = default;
    void Paint(DxUi::ControlHost& host) const override
    {
        ID2D1DeviceContext* const context = host.GetDeviceContext();
        wil::com_ptr_nothrow<ID2D1SolidColorBrush> brush;
        if (context && SUCCEEDED(context->CreateSolidColorBrush(D2D1::ColorF(0.25f, 0.5f, 0.75f), brush.put())))
            context->FillRectangle(D2D1::RectF(0.f, 0.f, 16.f, 16.f), brush.get());
    }
};

inline void TestCountingWrappersReportUncountedCreation()
{
    NativeWorkTests::CountingWrappers wrappers;
    {
        GraphicsFixture gpu;
        gpu.width  = 64u;
        gpu.height = 64u;
        Hr(gpu.Create(), "seeded creation WARP device");
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "seeded creation graphics device");
        DxUi::EmbeddedHost view;
        Hr(view.Attach(std::move(graphics), {}), "seeded creation attach");
        view.Controls().SetRoot(std::make_unique<UncountedBrushProbe>());
        Hr(view.Prepare(64u, 64u), "seeded creation first preparation");
        view.Controls().Invalidate();

        const DxUi::Detail::ResourceWork seenBefore = wrappers.Seen();
        const DxUi::Detail::WorkCounterScope scope;
        Hr(view.Prepare(64u, 64u), "seeded creation dirty preparation");
        const DxUi::Detail::WorkCounters delta = scope.Delta();
        const DxUi::Detail::ResourceWork seen  = NativeWorkTests::CreatedSince(wrappers.Seen(), seenBefore);

        std::array<NativeWorkTests::CreationMismatch, 4> mismatches{};
        const size_t count = NativeWorkTests::CompareCreations(delta.resources, seen, mismatches);
        Check(delta.frame.preparations == 1u, "the seeded frame prepared");
        Check(count == 1u && mismatches[0].kind == DxUi::Detail::ResourceKind::brush && mismatches[0].helpers == 0u && mismatches[0].wrappers == 1u,
              "the wrappers report the uncounted brush as the one mismatch: helpers 0, wrappers 1");
        Check(delta.frame.drawCalls == 2u, "the seeded frame makes two Direct2D draw calls: the host's Clear and the probe's FillRectangle");
    }
    Check(wrappers.direct2D.liveWrappers == 0u, "releasing the seeded host released every counting wrapper");
}
} // namespace WorkCountersComplexUi
