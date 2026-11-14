#pragma once
// The trace provider on the ComplexUi fixture (FX5 of the performance and motion plan), driven as the work-counter gate
// drives it. Included by Embedded.Tests.Embedded.cpp after Embedded.Tests.BenchmarkInstrument.h (the C++ allocation sink)
// and Embedded.Tests.WorkCounters.h (the fixture constants).
//
// The gate: with nothing listening a dirty frame makes exactly the allocations it made before the provider existed (54 in a
// Release build, the work-counter receipt); with a test sink listening it makes the same number, and each frame writes its
// prepare-begin, prepare-end and composite events, all with one frame number.
#include "ComplexUiScene.h"
#include "Embedded.Tests.WorkCounters.h"
#include "Support/Trace.h"
#include "Support/WorkCounters.h"

#include <array>
#include <vector>

namespace TraceProviderComplexUi
{
namespace Trace = DxUi::Detail::Trace;

struct Capture final
{
    std::array<Trace::Record, 1024u> records{};
    size_t count    = 0u;
    size_t overflow = 0u;
};

inline void CaptureRecord(void* context, const Trace::Record& record) noexcept
{
    auto& capture = *static_cast<Capture*>(context);
    if (capture.count == capture.records.size())
    {
        ++capture.overflow;
        return;
    }
    capture.records[capture.count++] = record;
}

inline void Run()
{
    using namespace WorkCountersComplexUi;
    Check((Trace::g_listeners.load() & Trace::kSinkInstalled) == 0u, "no trace sink is installed before the trace gate");
    GraphicsFixture gpu;
    gpu.width  = kWidth;
    gpu.height = kHeight;
    Hr(gpu.Create(), "trace gate WARP device");
    ComplexUiScene scene;
    Hr(scene.Initialize(gpu.device.get()), "trace gate ComplexUi scene");
    auto& view        = scene.view;
    size_t frameIndex = 0u;
    for (; frameIndex < kWarmFrames; ++frameIndex)
    {
        scene.Update(frameIndex);
        Hr(view.Prepare(kWidth, kHeight), "trace gate warm preparation");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "trace gate warm composition");
    }
    const auto dirtyFrameAllocations = [&]
    {
        allocations      = 0u;
        countAllocations = true;
        scene.Update(frameIndex++);
        Hr(view.Prepare(kWidth, kHeight), "trace gate preparation");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "trace gate composition");
        countAllocations = false;
        return static_cast<size_t>(allocations);
    };

    // Nothing listening: the provider's sites cost a branch and allocate nothing. A developer's own ETW session may be
    // listening while the suite runs; that writes events but allocates nothing either, so the gate holds the same.
    const bool sessionListening = Trace::SessionListening();
    std::vector<size_t> quiet;
    quiet.reserve(kFramesPerScenario);
    for (size_t frame = 0u; frame < kFramesPerScenario; ++frame)
        quiet.push_back(dirtyFrameAllocations());
    for (const size_t value : quiet)
        Check(value == quiet.front(), "every quiet dirty ComplexUi frame makes the same number of C++ allocations");
#if _ITERATOR_DEBUG_LEVEL == 0 && ! defined(__SANITIZE_ADDRESS__)
    Check(quiet.front() == kReleaseDirtyFrameAllocations, "with the trace provider compiled in, a Release dirty frame still makes 54 C++ allocations");
#endif

    // A sink listening: the same allocations, and per frame one prepare begin and end and one composite, in that order.
    Capture capture;
    Trace::InstallSinkForTest(&CaptureRecord, &capture);
    std::vector<size_t> traced;
    traced.reserve(kFramesPerScenario);
    for (size_t frame = 0u; frame < kFramesPerScenario; ++frame)
        traced.push_back(dirtyFrameAllocations());
    Trace::InstallSinkForTest(nullptr, nullptr);
    Check(! Trace::Listening() || sessionListening, "removing the sink leaves nothing listening");
    for (const size_t value : traced)
        Check(value == quiet.front(), "a traced dirty ComplexUi frame makes exactly the allocations of a quiet one");
    Check(capture.overflow == 0u, "the capture holds every traced record");

    size_t prepares = 0u;
    uint64_t hostId = 0u;
    for (size_t index = 0u; index < capture.count; ++index)
    {
        const Trace::Record& record = capture.records[index];
        if (record.event != Trace::Event::PrepareBegin)
            continue;
        Check(index + 2u < capture.count, "a traced preparation is followed by its end and composite");
        const Trace::Record& end       = capture.records[index + 1u];
        const Trace::Record& composite = capture.records[index + 2u];
        Check(end.event == Trace::Event::PrepareEnd && composite.event == Trace::Event::Composite, "prepare begin, prepare end, composite");
        Check(end.frame == record.frame && composite.frame == record.frame, "the preparation's events share one frame number");
        Check(end.result == S_OK && end.pixels == uint64_t(kWidth) * kHeight, "the prepare end records success and the repainted pixels");
        Check(record.hostId != 0u && end.hostId == record.hostId && composite.hostId == record.hostId, "every event names the view's host");
        Check(record.qpcTimestamp <= end.qpcTimestamp && end.qpcTimestamp <= composite.qpcTimestamp, "timestamps follow the work");
        Check(hostId == 0u || hostId == record.hostId, "one view, one host number");
        hostId = record.hostId;
        ++prepares;
    }
    Check(prepares == kFramesPerScenario, "each traced dirty frame writes one preparation");
    std::cout << "Trace provider ComplexUi: dirty frame cppAllocations quiet " << quiet.front() << ", traced " << traced.front() << ", records "
              << capture.count << (sessionListening ? " (an ETW session was listening)" : "") << '\n';
}
} // namespace TraceProviderComplexUi
