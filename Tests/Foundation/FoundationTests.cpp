#include <DxUi/FrameRuntime.h>
#include <cstdio>
#include <limits>
#include <string_view>
#include <windows.h>

namespace
{
int failures = 0;
void Check(bool condition, const char *message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}
struct Metrics
{
    unsigned calls = 0;
    uint64_t last = 0;
};
void Record(void *context, std::wstring_view, uint64_t value) noexcept
{
    auto &metrics = *static_cast<Metrics *>(context);
    ++metrics.calls;
    metrics.last = value;
}
} // namespace
int main()
{
    DxUi::FrameClock clock;
    LARGE_INTEGER frequency{};
    Check(QueryPerformanceFrequency(&frequency) != 0, "QPC frequency available");
    Check(clock.ElapsedUs({0}, {frequency.QuadPart}) == 1'000'000, "one second converts exactly");
    Check(clock.ElapsedUs({7}, {6}) == 0, "reversed timestamps fail safely");
    Check(clock.ElapsedUs({7}, {7}) == 0, "equal timestamps have zero duration");
    Check(clock.ElapsedUs({0}, {frequency.QuadPart / 2}) <= 500'000, "fractional conversion floors");
    Check(clock.SmoothDeltaUs(90'000, {16'667, 50'000}) == 50'000, "hitch clamp");
    Check(clock.SmoothDeltaUs(90'000, {16'667, 0}) == 90'000, "zero disables clamp");
    auto stage = DxUi::FrameStage::Idle;
    {
        DxUi::FrameStageScope outer(stage, DxUi::FrameStage::Render);
        Check(stage == DxUi::FrameStage::Render, "scope enters render");
#if defined(_DEBUG)
        Check(DxUi::IsDxUiRenderStageActiveForDebug(), "render guard observes render");
#else
        Check(!DxUi::IsDxUiRenderStageActiveForDebug(), "release omits debug guard");
#endif
        {
            DxUi::FrameStageScope inner(stage, DxUi::FrameStage::Layout);
            Check(stage == DxUi::FrameStage::Layout, "nested stage");
            Check(!DxUi::IsDxUiRenderStageActiveForDebug(), "nested guard");
        }
        Check(stage == DxUi::FrameStage::Render, "nested restore");
    }
    Check(stage == DxUi::FrameStage::Idle, "outer restore");
    Check(!DxUi::IsDxUiRenderStageActiveForDebug(), "guard restored");
    DxUi::MotionPolicy motion;
    Check(motion.ShouldAnimate() && motion.ResolveProgress(0.25f, 1.0f) == 0.25f, "normal motion");
    motion.reducedMotion = true;
    Check(!motion.ShouldAnimate() && motion.ResolveProgress(0.25f, 1.0f) == 1.0f, "reduced motion");
    Metrics metrics;
    DxUi::SetFrameMetricSink(&Record, &metrics);
    DxUi::EmitFrameMetric(L"fixture", 42);
    Check(metrics.calls == 1 && metrics.last == 42, "explicit diagnostics sink");
    DxUi::EmitDxUiRenderMutationBlockedForDebug();
    Check(metrics.calls == 2 && metrics.last == 1, "render mutation metric");
    DxUi::SetFrameMetricSink(nullptr, nullptr);
    DxUi::EmitFrameMetric(L"fixture", 99);
    Check(metrics.calls == 2, "reset removes borrowed sink");
    std::printf("Foundation tests: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
