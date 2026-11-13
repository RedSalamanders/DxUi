#pragma once
// Work counters on the ComplexUi fixture (FX5 of the performance and motion plan): 83 controls at 1280x720, 96 DPI, WARP,
// offscreen, reduced motion, driven exactly as the canonical benchmark drives it. Included by Embedded.Tests.Embedded.cpp
// after Embedded.Tests.BenchmarkInstrument.h, whose replaced allocation functions count the C++ allocations of each frame.
// The canonical benchmark header is left unchanged: paired runs copy it onto older trees that have no work counters.
#include "ComplexUiScene.h"
#include "Support/WorkCounters.h"

#include <algorithm>
#include <fstream>
#include <limits>
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
// The C++ allocations of one dirty ComplexUi frame in a Release build (no debug iterators): the retained WARP receipt's
// 2,160 allocations per 40 dirty frames (Measurements/EmbeddedInteractionLayout/Default-B1.json). A change that alters it
// changes this value with its own receipt; it is never rebaselined silently.
inline constexpr size_t kReleaseDirtyFrameAllocations = 54u;

struct FrameRecord final
{
    DxUi::Detail::WorkCounters work;
    size_t cppAllocations = 0u;
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

// One scenario's counters as {"name":{"min":…,"max":…,"total":…}} grouped by counter group, plus the allocation sink,
// total creations and churn of each frame.
inline void WriteScenario(std::ostream& output, const char* name, const std::vector<FrameRecord>& frames)
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
    for (const FrameRecord& frame : frames)
    {
        size_t index = 0u;
        series[index++].Add(frame.cppAllocations);
        series[index++].Add(DxUi::Detail::TotalCreated(frame.work));
        series[index++].Add(DxUi::Detail::Churn(frame.work));
        DxUi::Detail::ForEachWorkCounter(frame.work, [&](const char*, const char*, uint64_t value) { series[index++].Add(value); });
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
inline void Run(const wchar_t* receiptPath)
{
    GraphicsFixture gpu;
    gpu.width  = kWidth;
    gpu.height = kHeight;
    Hr(gpu.Create(), "work counters WARP device");
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

    const size_t visibleControls = CountVisibleControls(view.Controls().GetRoot());
    Check(visibleControls == kComplexUiControls, "the ComplexUi fixture paints 83 visible controls");

    std::vector<FrameRecord> clean;
    std::vector<FrameRecord> dirty;
    clean.reserve(kFramesPerScenario);
    dirty.reserve(kFramesPerScenario);
    const auto measure = [&](std::vector<FrameRecord>& records, bool isDirty, size_t frame)
    {
        const DxUi::Detail::WorkCounterScope scope;
        allocations      = 0u;
        countAllocations = true;
        if (isDirty)
            scene.Update(frame);
        Hr(view.Prepare(kWidth, kHeight), "work counters preparation");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "work counters composition");
        countAllocations          = false;
        const size_t cppAllocated = allocations;
        static_cast<void>(tracker.Sweep());
        records.push_back({scope.Delta(), cppAllocated});
    };
    for (size_t frame = 0u; frame < kFramesPerScenario; ++frame)
        measure(clean, false, 0u);
    for (size_t frame = 0u; frame < kFramesPerScenario; ++frame)
        measure(dirty, true, kWarmFrames + frame);
    Check(tracker.Overflow() == 0u, "the tracker observed every resource the frames created");

    for (const FrameRecord& frame : clean)
    {
        Check(frame.cppAllocations == 0u, "a clean ComplexUi frame makes no C++ allocation");
        Check(DxUi::Detail::TotalCreated(frame.work) == 0u, "a clean ComplexUi frame creates no counted resource");
        Check(DxUi::Detail::Churn(frame.work) == 0u, "a clean ComplexUi frame has no resource churn");
        Check(frame.work.frame.preparations == 0u && frame.work.frame.paintExecutions == 0u && frame.work.frame.treeVisitsPaint == 0u,
              "a clean ComplexUi frame neither prepares nor paints");
        Check(frame.work.frame.composites == 1u, "a clean ComplexUi frame composites once");
    }
    for (const FrameRecord& frame : dirty)
    {
        Check(frame.work.frame.preparations == 1u && frame.work.frame.composites == 1u, "a dirty ComplexUi frame prepares and composites once");
        Check(frame.work.frame.paintExecutions == visibleControls, "a dirty ComplexUi frame paints every visible control once");
        Check(frame.work.frame.treeVisitsPaint == visibleControls, "the dirty paint walk examines every control once");
        Check(frame.work.frame.pixelsPainted == uint64_t(kWidth) * kHeight, "a dirty ComplexUi frame repaints the whole surface");
#if _ITERATOR_DEBUG_LEVEL == 0 && ! defined(__SANITIZE_ADDRESS__)
        Check(frame.cppAllocations == kReleaseDirtyFrameAllocations, "a Release dirty ComplexUi frame makes today's 54 C++ allocations");
#endif
    }

    std::cout << "Work counters ComplexUi: dirty frame cppAllocations " << dirty.front().cppAllocations << ", paintExecutions "
              << dirty.front().work.frame.paintExecutions << ", created " << DxUi::Detail::TotalCreated(dirty.front().work) << ", churn "
              << DxUi::Detail::Churn(dirty.front().work) << "; clean frame cppAllocations " << clean.front().cppAllocations << '\n';
    if (! receiptPath)
        return;
    std::ofstream output{std::filesystem::path(receiptPath)};
    Check(bool(output), "work counters receipt file");
    output << "{\"fixture\":\"dxui-complex-ui-v2\",\"plan\":\"FX5\",\"renderer\":\"WARP\",\"configuration\":\"" << ConfigurationName()
           << "\",\"iteratorDebugLevel\":" << _ITERATOR_DEBUG_LEVEL << ",\"compiler\":" << _MSC_FULL_VER << ",\"width\":" << kWidth << ",\"height\":" << kHeight
           << ",\"dpi\":96,\"controls\":" << kComplexUiControls << ",\"visibleControls\":" << visibleControls << ",\"warmFrames\":" << kWarmFrames
           << ",\"trackedResources\":" << tracker.Held() << ",\"trackerOverflow\":" << tracker.Overflow() << ",\"scenarios\":[";
    WriteScenario(output, "clean", clean);
    output << ',';
    WriteScenario(output, "dirty", dirty);
    output << "]}\n";
    output.close();
    Check(bool(output), "work counters receipt written");
}
} // namespace WorkCountersComplexUi
