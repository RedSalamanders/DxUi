#include "../../src/Support/AnimationDispatcher.h"
#include "../Support/Support.Tests.PerformanceCapture.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "DxUi/FrameRuntime.h"

#include <array>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>

namespace
{
[[nodiscard]] std::filesystem::path GetAnimationPerfJsonlPathFromEnvironment()
{
    const DWORD required = GetEnvironmentVariableW(L"DXUI_PERF_JSONL_PATH", nullptr, 0u);
    if (required == 0u)
    {
        return {};
    }

    std::wstring value(required, L'\0');
    const DWORD copied = GetEnvironmentVariableW(L"DXUI_PERF_JSONL_PATH", value.data(), required);
    if (copied == 0u)
    {
        return {};
    }

    value.resize(copied);
    return std::filesystem::path(value);
}

[[nodiscard]] uintmax_t GetAnimationFileSizeOrZero(const std::filesystem::path& path) noexcept
{
    std::error_code ec;
    const uintmax_t size = std::filesystem::file_size(path, ec);
    return ec ? 0u : size;
}

[[nodiscard]] std::string ReadAnimationPerfJsonlFromOffset(const std::filesystem::path& path, uintmax_t offset)
{
    std::ifstream input(path, std::ios::binary);
    if (! input)
    {
        return {};
    }

    input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

[[nodiscard]] size_t CountAnimationMetricRows(std::string_view jsonl, std::string_view metric) noexcept
{
    size_t count     = 0u;
    size_t searchPos = 0u;
    while (searchPos < jsonl.size())
    {
        const size_t position = jsonl.find(metric, searchPos);
        if (position == std::string_view::npos)
        {
            break;
        }
        ++count;
        searchPos = position + metric.size();
    }
    return count;
}

class ScopedAnimationPerfJsonl final
{
public:
    ScopedAnimationPerfJsonl()
    {
        _path = GetAnimationPerfJsonlPathFromEnvironment();
        if (! _path.empty())
        {
            return;
        }

        _path = GetDxUiTestArtifactPath(L"dxui_animation_scheduler_testlocal.jsonl");
        std::error_code ec;
        std::filesystem::remove(_path, ec);
        TestPerformanceCapture::Start(_path, L"DxUiTests", L"Debug");
        _ownsConfiguration = true;
    }

    ~ScopedAnimationPerfJsonl()
    {
        if (_ownsConfiguration)
        {
            TestPerformanceCapture::Stop();
        }
    }

    ScopedAnimationPerfJsonl(const ScopedAnimationPerfJsonl&)            = delete;
    ScopedAnimationPerfJsonl& operator=(const ScopedAnimationPerfJsonl&) = delete;
    ScopedAnimationPerfJsonl(ScopedAnimationPerfJsonl&&)                 = delete;
    ScopedAnimationPerfJsonl& operator=(ScopedAnimationPerfJsonl&&)      = delete;

    [[nodiscard]] const std::filesystem::path& Path() const noexcept
    {
        return _path;
    }

private:
    std::filesystem::path _path;
    bool _ownsConfiguration = false;
};

void PumpAnimationMessagesForMs(uint64_t durationMs)
{
    const uint64_t deadline = GetTickCount64() + durationMs;
    MSG msg{};
    do
    {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(1);
    } while (GetTickCount64() < deadline);
}

struct AnimationTickCapture final
{
    std::vector<uint64_t> ticks;
    size_t keepAliveTicks = 0;
};

bool CaptureAnimationTick(void* context, uint64_t nowTickMs) noexcept
{
    auto* capture = static_cast<AnimationTickCapture*>(context);
    if (! capture)
    {
        return false;
    }

    capture->ticks.push_back(nowTickMs);
    return capture->ticks.size() < capture->keepAliveTicks;
}

void TestFrameRuntimeClockIsMonotonic()
{
    using namespace DxUi;

    FrameClock clock;
    const FrameTimestamp first  = clock.Now();
    const FrameTimestamp second = clock.Now();
    const uint64_t elapsedUs    = clock.ElapsedUs(first, second);

    Require(second.qpc >= first.qpc, "frame runtime QPC timestamps are monotonic");
    Require(elapsedUs < 10'000'000u, "frame runtime elapsed time stays bounded for adjacent monotonic timestamps");
    Require(clock.ElapsedUs(second, first) == 0u, "frame runtime elapsed time clamps reversed timestamps to zero");
}

void TestFrameRuntimeClampsLargeDelta()
{
    using namespace DxUi;

    FrameClock clock;
    FrameBudget budget;
    budget.hitchClampUs = 50'000u;

    Require(clock.SmoothDeltaUs(500'000u, budget) == budget.hitchClampUs, "frame runtime clamps large animation deltas to the hitch budget");
    budget.hitchClampUs = 0u;
    Require(clock.SmoothDeltaUs(500'000u, budget) == 500'000u, "frame runtime leaves large animation deltas unclamped when the budget clamp is disabled");
}

void TestAnimationDispatcherSchedulerPolicyUses120HzTargetAndClampsHitches()
{
    using DxUi::Ui::AnimationDispatcher;

    AnimationDispatcher::GetInstance().Shutdown();
    auto& dispatcher = AnimationDispatcher::GetInstance();

    Require(dispatcher.DebugGetTargetFrameUsForTest() == 8'333u,
            "animation dispatcher scheduler uses an 8,333 us target for synthetic 120 Hz animation pacing");
    const auto hitchTiming = dispatcher.DebugComputeTimingForTest(250'000u);
    Require(hitchTiming.callbackDeltaUs == dispatcher.DebugGetHitchClampUsForTest(), "animation dispatcher scheduler clamps hitch deltas before interpolation");
    Require(hitchTiming.legacyGapMs == 250u, "animation dispatcher legacy tick gap reports the raw elapsed timer delta");
    Require(hitchTiming.legacyOverrun, "animation dispatcher legacy overrun uses the raw elapsed timer delta");
}

void TestAnimationDispatcherActiveSubscribersReceiveMonotonicHighResolutionTicks()
{
    using DxUi::Ui::AnimationDispatcher;

    AnimationDispatcher::GetInstance().Shutdown();
    ScopedAnimationPerfJsonl perfJsonl;
    const std::filesystem::path& perfPath = perfJsonl.Path();
    const uintmax_t startOffset           = GetAnimationFileSizeOrZero(perfPath);

    AnimationTickCapture capture;
    capture.keepAliveTicks        = 3u;
    const uint64_t subscriptionId = AnimationDispatcher::GetInstance().Subscribe(&CaptureAnimationTick, &capture);
    Require(subscriptionId != 0u, "animation dispatcher accepts an active test subscriber");
    PumpAnimationMessagesForMs(250u);

    Require(capture.ticks.size() == 3u, "animation dispatcher active subscriber receives repeated ticks while it remains active");
    for (size_t index = 1; index < capture.ticks.size(); ++index)
    {
        Require(capture.ticks[index] >= capture.ticks[index - 1u], "animation dispatcher tick timestamps are monotonic");
    }

    const std::string appendedMetrics = ReadAnimationPerfJsonlFromOffset(perfPath, startOffset);
    Require(appendedMetrics.find("\"metric\":\"dxui.animation.tick_delta_us\"") != std::string::npos,
            "animation dispatcher emits high-resolution tick delta metrics for active subscribers");
    Require(appendedMetrics.find("\"metric\":\"dxui.animation.jitter_us\"") != std::string::npos,
            "animation dispatcher emits high-resolution jitter metrics for active subscribers");
    Require(appendedMetrics.find("\"metric\":\"dxui.animation.active_count\"") != std::string::npos,
            "animation dispatcher emits active subscriber count metrics");
}

void TestAnimationDispatcherInactiveSubscriberStopsContinuousTicks()
{
    using DxUi::Ui::AnimationDispatcher;

    AnimationDispatcher::GetInstance().Shutdown();

    AnimationTickCapture capture;
    capture.keepAliveTicks        = 1u;
    const uint64_t subscriptionId = AnimationDispatcher::GetInstance().Subscribe(&CaptureAnimationTick, &capture);
    Require(subscriptionId != 0u, "animation dispatcher accepts a one-shot test subscriber");
    PumpAnimationMessagesForMs(180u);

    Require(capture.ticks.size() == 1u, "animation dispatcher stops continuous ticks after the subscriber returns inactive");
}

void TestFrameRuntimeElapsedUsHandlesLargeQpcDelta()
{
    using namespace DxUi;

    LARGE_INTEGER frequency{};
    Require(QueryPerformanceFrequency(&frequency) != 0 && frequency.QuadPart > 0, "frame runtime large-delta test reads a positive QPC frequency");

    constexpr uint64_t kMicrosecondsPerSecond  = 1'000'000u;
    constexpr uint64_t kMaxFrameTimestampQpc   = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    constexpr uint64_t kOverflowThresholdTicks = (std::numeric_limits<uint64_t>::max() / kMicrosecondsPerSecond) + 1u;

    const uint64_t qpcFrequency      = static_cast<uint64_t>(frequency.QuadPart);
    const uint64_t largeDeltaSeconds = (kOverflowThresholdTicks / qpcFrequency) + 1u;
    Require(largeDeltaSeconds <= kMaxFrameTimestampQpc / qpcFrequency, "frame runtime large-delta QPC value fits in FrameTimestamp");

    const uint64_t largeDeltaQpc = qpcFrequency * largeDeltaSeconds;
    Require(largeDeltaQpc >= kOverflowThresholdTicks, "frame runtime large-delta test reaches the multiply-first overflow threshold");
    Require(largeDeltaQpc > std::numeric_limits<uint64_t>::max() / kMicrosecondsPerSecond,
            "frame runtime large-delta test exercises the multiply-first overflow path");

    const uint64_t wholeSeconds = largeDeltaQpc / qpcFrequency;
    const uint64_t remainderQpc = largeDeltaQpc % qpcFrequency;
    uint64_t expectedUs         = std::numeric_limits<uint64_t>::max();
    if (wholeSeconds <= std::numeric_limits<uint64_t>::max() / kMicrosecondsPerSecond)
    {
        const uint64_t quotientUs = wholeSeconds * kMicrosecondsPerSecond;
        Require(remainderQpc <= std::numeric_limits<uint64_t>::max() / kMicrosecondsPerSecond,
                "frame runtime large-delta remainder microseconds fit in uint64_t");

        const uint64_t remainderUs = (remainderQpc * kMicrosecondsPerSecond) / qpcFrequency;
        Require(remainderUs <= std::numeric_limits<uint64_t>::max() - quotientUs, "frame runtime large-delta expected microseconds fit in uint64_t");
        expectedUs = quotientUs + remainderUs;
    }

    FrameClock clock;
    Require(clock.ElapsedUs(FrameTimestamp{0}, FrameTimestamp{static_cast<int64_t>(largeDeltaQpc)}) == expectedUs,
            "frame runtime elapsed microseconds handles large QPC deltas without overflow");
}

void TestFrameRuntimeElapsedUsHandlesWideCrossSignDelta()
{
    using namespace DxUi;

    LARGE_INTEGER frequency{};
    Require(QueryPerformanceFrequency(&frequency) != 0 && frequency.QuadPart > 0, "frame runtime cross-sign delta test reads a positive QPC frequency");

    constexpr uint64_t kMicrosecondsPerSecond = 1'000'000u;
    constexpr FrameTimestamp kStart{std::numeric_limits<int64_t>::min()};
    constexpr FrameTimestamp kEnd{std::numeric_limits<int64_t>::max()};

    const uint64_t deltaQpc     = static_cast<uint64_t>(kEnd.qpc) - static_cast<uint64_t>(kStart.qpc);
    const uint64_t qpcFrequency = static_cast<uint64_t>(frequency.QuadPart);
    const uint64_t wholeSeconds = deltaQpc / qpcFrequency;
    const uint64_t remainderQpc = deltaQpc % qpcFrequency;

    uint64_t expectedUs = std::numeric_limits<uint64_t>::max();
    if (wholeSeconds <= std::numeric_limits<uint64_t>::max() / kMicrosecondsPerSecond)
    {
        const uint64_t wholeUs = wholeSeconds * kMicrosecondsPerSecond;
        Require(remainderQpc <= std::numeric_limits<uint64_t>::max() / kMicrosecondsPerSecond,
                "frame runtime cross-sign delta remainder microseconds fit in uint64_t");
        const uint64_t remainderUs = (remainderQpc * kMicrosecondsPerSecond) / qpcFrequency;
        Require(remainderUs <= std::numeric_limits<uint64_t>::max() - wholeUs, "frame runtime cross-sign delta expected microseconds fit in uint64_t");
        expectedUs = wholeUs + remainderUs;
    }

    FrameClock clock;
    Require(clock.ElapsedUs(kStart, kEnd) == expectedUs, "frame runtime elapsed microseconds handles wide cross-sign QPC deltas without signed overflow");
}

void TestFrameRuntimeReducedMotionPolicy()
{
    using namespace DxUi;

    MotionPolicy animatedPolicy;
    Require(animatedPolicy.ShouldAnimate(), "frame runtime animates when reduced motion is disabled");
    RequireFloatNear(animatedPolicy.ResolveProgress(0.25f, 1.0f), 0.25f, 0.0001f, "frame runtime preserves animated progress when reduced motion is disabled");

    MotionPolicy reducedPolicy;
    reducedPolicy.reducedMotion = true;
    Require(! reducedPolicy.ShouldAnimate(), "frame runtime suppresses animation when reduced motion is enabled");
    RequireFloatNear(reducedPolicy.ResolveProgress(0.25f, 1.0f), 1.0f, 0.0001f, "frame runtime snaps to target progress when reduced motion is enabled");
}

void TestButtonHoverAnimationRequestsTicksUntilSettled()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    ExposedButton button(L"Apply");

    const uint64_t hoverStartTickMs = ::GetTickCount64();
    button.OnHoverChanged(host, true);
    RequireFloatNear(button.DebugGetHoverAnimationProgress(), 0.0f, 0.0001f, "button hover animation starts from the idle progress");
    Require(button.Tick(host, hoverStartTickMs + 70u), "button hover animation advances on the first tick");
    Require(button.DebugGetHoverAnimationProgress() > 0.0f && button.DebugGetHoverAnimationProgress() < 1.0f,
            "button hover animation reaches an in-flight progress value");
    Require(button.Tick(host, hoverStartTickMs + 160u), "button hover animation requests one final repaint when the transition settles");
    RequireFloatNear(button.DebugGetHoverAnimationProgress(), 1.0f, 0.0001f, "button hover animation settles at fully hovered progress");
    Require(! button.Tick(host, hoverStartTickMs + 220u), "settled button hover animation stops requesting ticks");

    const uint64_t fadeStartTickMs = ::GetTickCount64();
    button.OnHoverChanged(host, false);
    Require(button.Tick(host, fadeStartTickMs + 70u), "button hover fade-out advances on the first tick");
    Require(button.DebugGetHoverAnimationProgress() > 0.0f && button.DebugGetHoverAnimationProgress() < 1.0f,
            "button hover fade-out exposes an in-flight progress value");
    Require(button.Tick(host, fadeStartTickMs + 160u), "button hover fade-out requests one final repaint when it reaches idle");
    RequireFloatNear(button.DebugGetHoverAnimationProgress(), 0.0f, 0.0001f, "button hover fade-out settles back to idle progress");
    Require(! button.Tick(host, fadeStartTickMs + 220u), "idle button hover animation stops requesting ticks after settling");
}

void TestButtonReducedMotionSnapsInteractionAnimation()
{
    using namespace DxUi;

    ThemePalette theme  = MakeAnimatedTestThemePalette(true);
    theme.reducedMotion = true;

    WindowHost host;

    EnableMotionForTest(host);
    host.SetTheme(theme);

    ExposedButton button(L"Apply");
    button.OnHoverChanged(host, true);
    RequireFloatNear(button.DebugGetHoverAnimationProgress(), 1.0f, 0.0001f, "reduced-motion button hover snaps directly to the target progress");
    Require(! button.Tick(host, 0u), "reduced-motion button hover does not request animation ticks");

    button.OnFocusChanged(host, true);
    RequireFloatNear(button.DebugGetFocusAnimationProgress(), 1.0f, 0.0001f, "reduced-motion button focus snaps directly to the target progress");
    Require(! button.Tick(host, 80u), "reduced-motion button focus does not request animation ticks");
}

void TestSliderHoverAndPressAnimationRequestsTicksUntilSettled()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    auto root    = std::make_unique<Panel>();
    auto* slider = root->AddChild<Slider>();
    slider->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 48.0f));
    host.SetRoot(std::move(root));

    const float restThumbWidth = slider->DebugGetThumbRect().right - slider->DebugGetThumbRect().left;
    const float restHaloWidth  = slider->DebugGetHaloRect().right - slider->DebugGetHaloRect().left;
    RequireFloatNear(restThumbWidth, 14.0f, 0.01f, "slider accent thumb is visible at rest");
    RequireFloatNear(restHaloWidth, 24.0f, 0.01f, "slider chrome disc is 24 DIP at rest");
    RequireFloatNear(slider->GetHitBounds().bottom - slider->GetHitBounds().top, 48.0f, 0.01f, "slider hit band stays 48 DIP at rest");

    const uint64_t hoverStartTickMs = ::GetTickCount64();
    slider->OnHoverChanged(host, true);
    RequireFloatNear(slider->DebugGetHoverAnimationProgress(), 0.0f, 0.0001f, "slider hover animation starts from the idle progress");
    Require(slider->Tick(host, hoverStartTickMs + 40u), "slider hover animation advances on the first tick");
    Require(slider->DebugGetHoverAnimationProgress() > 0.0f && slider->DebugGetHoverAnimationProgress() < 1.0f,
            "slider hover animation reaches an in-flight progress value");
    Require(slider->DebugGetThumbRect().right - slider->DebugGetThumbRect().left > restThumbWidth,
            "slider inner thumb grows while the hover animation is in flight");
    RequireFloatNear(slider->DebugGetHaloRect().right - slider->DebugGetHaloRect().left,
                     restHaloWidth,
                     0.01f,
                     "slider chrome disc does not scale while the inner thumb grows");
    Require(slider->Tick(host, hoverStartTickMs + 140u), "slider hover animation requests one final repaint when the transition settles");
    RequireFloatNear(slider->DebugGetHoverAnimationProgress(), 1.0f, 0.0001f, "slider hover animation settles at fully hovered progress");
    RequireFloatNear(slider->DebugGetThumbRect().right - slider->DebugGetThumbRect().left, 20.0f, 0.01f, "slider inner thumb settles at 20 DIP when hovered");
    RequireFloatNear(slider->DebugGetHaloRect().right - slider->DebugGetHaloRect().left, 24.0f, 0.01f, "slider chrome disc stays 24 DIP when hovered");
    RequireFloatNear(slider->GetHitBounds().bottom - slider->GetHitBounds().top, 48.0f, 0.01f, "slider hit band does not grow with the painted thumb");
    Require(! slider->Tick(host, hoverStartTickMs + 200u), "settled slider hover animation stops requesting ticks");

    const float hoverThumbWidth     = slider->DebugGetThumbRect().right - slider->DebugGetThumbRect().left;
    const uint64_t pressStartTickMs = ::GetTickCount64();
    Require(slider->OnMouseDown(host, D2D1::Point2F(110.0f, 24.0f), false, 0), "slider press starts a pointer gesture");
    Require(slider->Tick(host, pressStartTickMs + 40u), "slider press animation advances on the first tick");
    Require(slider->DebugGetPressAnimationProgress() > 0.0f && slider->DebugGetPressAnimationProgress() < 1.0f,
            "slider press animation reaches an in-flight progress value");
    Require(slider->DebugGetThumbRect().right - slider->DebugGetThumbRect().left < hoverThumbWidth,
            "slider inner thumb shrinks while the press animation is in flight");
    RequireFloatNear(slider->DebugGetHaloRect().right - slider->DebugGetHaloRect().left,
                     24.0f,
                     0.01f,
                     "slider chrome disc stays 24 DIP while pressed so it does not read as the 48 DIP hit band");
    Require(slider->OnMouseUp(host, D2D1::Point2F(110.0f, 24.0f), false, 0), "slider press ends the pointer gesture");
}

// A touch contact that drags the slider grows a translucent halo from the chrome disc to the 48 DIP grab area, around the
// finger that covers the thumb, and the halo shrinks away when the drag ends. A mouse drag never shows it, a cancelled drag
// removes it at once, and neither the chrome disc nor the hit band changes with it.
void TestSliderTouchDragShowsTouchHaloUntilItEnds()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    auto root    = std::make_unique<Panel>();
    auto* slider = root->AddChild<Slider>();
    slider->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 48.0f));
    slider->SetValue(50.0);
    host.SetRoot(std::move(root));
    const auto width           = [](const D2D1_RECT_F& rect) { return rect.right - rect.left; };
    const D2D1_RECT_F thumb    = slider->DebugGetThumbRect();
    const D2D1_POINT_2F center = D2D1::Point2F((thumb.left + thumb.right) * 0.5f, (thumb.top + thumb.bottom) * 0.5f);

    host.DebugSetPointerDevice(PointerDevice::Mouse);
    uint64_t startTickMs = ::GetTickCount64();
    Require(slider->OnMouseDown(host, center, false, 0), "a mouse press starts a drag");
    static_cast<void>(slider->Tick(host, startTickMs + 200u));
    RequireFloatNear(slider->DebugGetTouchHaloProgress(), 0.0f, 0.0001f, "a mouse drag shows no touch halo");
    RequireFloatNear(width(slider->DebugGetTouchHaloRect()), width(slider->DebugGetHaloRect()), 0.01f, "an idle touch halo is no larger than the chrome disc");
    Require(slider->OnMouseUp(host, center, false, 0), "the mouse drag ends");
    static_cast<void>(slider->Tick(host, startTickMs + 400u));

    host.DebugSetPointerDevice(PointerDevice::Touch);
    startTickMs = ::GetTickCount64();
    Require(slider->OnMouseDown(host, center, false, 0), "a touch press starts a drag");
    Require(slider->Tick(host, startTickMs + 40u), "the touch halo animates on the first tick");
    Require(slider->DebugGetTouchHaloProgress() > 0.0f && slider->DebugGetTouchHaloProgress() < 1.0f, "the touch halo grows in flight");
    Require(width(slider->DebugGetTouchHaloRect()) > width(slider->DebugGetHaloRect()) && width(slider->DebugGetTouchHaloRect()) < 48.0f,
            "between the chrome disc and the grab area");
    static_cast<void>(slider->Tick(host, startTickMs + 200u));
    RequireFloatNear(slider->DebugGetTouchHaloProgress(), 1.0f, 0.0001f, "the touch halo settles while the contact drags");
    RequireFloatNear(width(slider->DebugGetTouchHaloRect()), 48.0f, 0.01f, "at the 48 DIP grab area");
    RequireFloatNear(width(slider->DebugGetHaloRect()), 24.0f, 0.01f, "the chrome disc does not grow with it");
    RequireFloatNear(slider->GetHitBounds().bottom - slider->GetHitBounds().top, 48.0f, 0.01f, "and neither does the hit band");
    const D2D1_POINT_2F dragged = D2D1::Point2F(center.x + 30.0f, center.y);
    Require(slider->OnMouseMove(host, dragged, 0), "the contact drags the thumb");
    const D2D1_RECT_F followed = slider->DebugGetTouchHaloRect();
    RequireFloatNear((followed.left + followed.right) * 0.5f,
                     (slider->DebugGetThumbRect().left + slider->DebugGetThumbRect().right) * 0.5f,
                     0.01f,
                     "the halo follows the thumb");

    startTickMs = ::GetTickCount64();
    Require(slider->OnMouseUp(host, dragged, false, 0), "the touch drag ends");
    Require(slider->Tick(host, startTickMs + 40u), "the touch halo animates out");
    Require(slider->DebugGetTouchHaloProgress() < 1.0f, "the touch halo shrinks once the contact lifts");
    static_cast<void>(slider->Tick(host, startTickMs + 200u));
    RequireFloatNear(slider->DebugGetTouchHaloProgress(), 0.0f, 0.0001f, "and is gone once the drag ended");
    Require(! slider->Tick(host, startTickMs + 400u), "a settled touch halo requests no ticks");

    startTickMs = ::GetTickCount64();
    Require(slider->OnMouseDown(host, center, false, 0), "another touch press starts a drag");
    static_cast<void>(slider->Tick(host, startTickMs + 200u));
    RequireFloatNear(slider->DebugGetTouchHaloProgress(), 1.0f, 0.0001f, "with its touch halo");
    slider->OnCaptureLost(host);
    RequireFloatNear(slider->DebugGetTouchHaloProgress(), 0.0f, 0.0001f, "a cancelled touch drag removes the halo at once");

    // A touch on the track away from the thumb seeks: the thumb moves under the finger and shows the halo there.
    startTickMs = ::GetTickCount64();
    Require(slider->OnMouseDown(host, D2D1::Point2F(20.0f, 24.0f), false, 0), "a touch on the track seeks");
    static_cast<void>(slider->Tick(host, startTickMs + 200u));
    RequireFloatNear(slider->DebugGetTouchHaloProgress(), 1.0f, 0.0001f, "and shows the halo around the thumb it moved");
    Require(slider->OnMouseUp(host, D2D1::Point2F(20.0f, 24.0f), false, 0), "the seek drag ends");
    host.DebugSetPointerDevice(PointerDevice::Mouse);
}

void TestSliderTouchHaloStaysInsideItsPaintBoundsAndBehindOverlays()
{
    using namespace DxUi;

    class PaintProbe final : public Control
    {
    public:
        explicit PaintProbe(D2D1_COLOR_F color) noexcept : _color(color)
        {
        }

        void Paint(ControlHost& host) const override
        {
            if (ID2D1DeviceContext* const dc = host.GetDeviceContext(); dc)
            {
                if (ID2D1SolidColorBrush* const brush = host.GetSolidBrush(_color); brush)
                {
                    dc->FillRectangle(GetBounds(), brush);
                }
            }
        }

        bool OnMouseDown(ControlHost&, D2D1_POINT_2F, bool, UINT) override
        {
            ++pressCount;
            return true;
        }

        size_t pressCount = 0u;

    private:
        D2D1_COLOR_F _color;
    };

    const auto capturePixel = [](const WindowHostBitmapCapture& capture, WindowHost& host, float xDip, float yDip)
    {
        const LONG x = static_cast<LONG>(std::lround(host.DipsToPixels(xDip)));
        const LONG y = static_cast<LONG>(std::lround(host.DipsToPixels(yDip)));
        Require(x >= 0 && y >= 0 && static_cast<UINT>(x) < capture.widthPx && static_cast<UINT>(y) < capture.heightPx,
                "the slider feedback pixel is inside the captured surface");
        const size_t offset = (static_cast<size_t>(y) * capture.widthPx + static_cast<size_t>(x)) * 4u;
        return std::array<uint8_t, 4u>{
            capture.bgraPixels[offset + 0u], capture.bgraPixels[offset + 1u], capture.bgraPixels[offset + 2u], capture.bgraPixels[offset + 3u]};
    };

    AttachedHostWindow window;
    auto root                     = std::make_unique<Panel>();
    auto* adjacent                = root->AddChild<PaintProbe>(D2D1::ColorF(0.0f, 0.65f, 0.2f, 1.0f));
    auto* smallParent             = root->AddChild<Panel>();
    auto* slider                  = smallParent->AddChild<Slider>();
    auto* popup                   = root->AddChild<PopupLayer>();
    const D2D1_COLOR_F popupColor = D2D1::ColorF(0.1f, 0.25f, 0.9f, 1.0f);
    auto* popupPaint              = popup->AddChild<PaintProbe>(popupColor);
    adjacent->SetBounds(D2D1::RectF(72.0f, 34.0f, 152.0f, 50.0f));
    smallParent->SetBounds(D2D1::RectF(72.0f, 50.0f, 152.0f, 82.0f));
    slider->SetBounds(smallParent->GetBounds());
    slider->SetValue(50.0);
    popup->SetBounds(D2D1::RectF(104.0f, 58.0f, 120.0f, 74.0f));
    popupPaint->SetBounds(D2D1::RectF(104.0f, 58.0f, 120.0f, 74.0f));
    window.Host().SetRoot(std::move(root));
    const auto prepareCapture = [](AttachedHostWindow& attachedWindow)
    {
        ShowWindow(attachedWindow.Hwnd(), SW_SHOWNOACTIVATE);
        attachedWindow.PumpMessages();
        RedrawWindow(attachedWindow.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        attachedWindow.PumpMessages();
    };
    prepareCapture(window);

    const D2D1_POINT_2F adjacentPoint = D2D1::Point2F(112.0f, 48.0f);
    Require(window.Host().DebugHitTestControl(adjacentPoint) == adjacent,
            "a point covered only by the clipped touch halo still hit-tests the adjacent control");
    Require(adjacent->OnMouseDown(window.Host(), adjacentPoint, false, 0u) && adjacent->pressCount == 1u,
            "the adjacent control can receive its own pointer press at the halo edge");

    WindowHostBitmapCapture before;
    Require(window.Host().DebugCaptureBitmap(before), "the idle slider and adjacent control capture succeeds");
    window.Host().DebugSetPointerDevice(PointerDevice::Touch);
    const D2D1_RECT_F thumb    = slider->DebugGetThumbRect();
    const D2D1_POINT_2F center = D2D1::Point2F((thumb.left + thumb.right) * 0.5f, (thumb.top + thumb.bottom) * 0.5f);
    const uint64_t startTickMs = ::GetTickCount64();
    Require(slider->OnMouseDown(window.Host(), center, false, 0u), "a touch press starts the slider drag");
    static_cast<void>(slider->Tick(window.Host(), startTickMs + 200u));
    RequireFloatNear(slider->DebugGetTouchHaloProgress(), 1.0f, 0.0001f, "the deterministic touch halo reaches its full 48 DIP size");

    prepareCapture(window);
    WindowHostBitmapCapture during;
    Require(window.Host().DebugCaptureBitmap(during), "the active touch feedback capture succeeds");
    Require(capturePixel(before, window.Host(), adjacentPoint.x, adjacentPoint.y) == capturePixel(during, window.Host(), adjacentPoint.x, adjacentPoint.y),
            "touch feedback does not paint over the neighboring control above the slider's small parent");
    Require(capturePixel(before, window.Host(), 132.0f, 66.0f) != capturePixel(during, window.Host(), 132.0f, 66.0f),
            "the clipped 48 DIP halo still paints over the slider's own track");
    const auto overlayPixel = capturePixel(during, window.Host(), 112.0f, 66.0f);
    const auto channelNear  = [](uint8_t actual, float expected)
    { return std::abs(static_cast<int>(actual) - static_cast<int>(std::lround(expected * 255.0f))) <= 2; };
    Require(channelNear(overlayPixel[0], popupColor.b) && channelNear(overlayPixel[1], popupColor.g) && channelNear(overlayPixel[2], popupColor.r),
            "a popup overlay paints above the active slider halo");

    static_cast<void>(slider->OnMouseUp(window.Host(), center, false, 0u));
    window.Host().DebugSetPointerDevice(PointerDevice::Mouse);

    AttachedHostWindow scrolledWindow;
    auto scrolledRoot    = std::make_unique<Panel>();
    auto* viewport       = scrolledRoot->AddChild<ScrollPanel>();
    auto* scrolledSlider = viewport->AddChild<Slider>();
    viewport->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 80.0f));
    viewport->SetContentHeight(180.0f);
    scrolledSlider->SetBounds(D2D1::RectF(20.0f, 80.0f, 200.0f, 112.0f));
    scrolledSlider->SetValue(50.0);
    viewport->SetScrollOffset(30.0f);
    scrolledWindow.Host().SetRoot(std::move(scrolledRoot));
    prepareCapture(scrolledWindow);
    WindowHostBitmapCapture scrolledBefore;
    Require(scrolledWindow.Host().DebugCaptureBitmap(scrolledBefore), "the scrolled viewport capture succeeds");
    scrolledWindow.Host().DebugSetPointerDevice(PointerDevice::Touch);
    const D2D1_RECT_F scrolledThumb    = scrolledSlider->DebugGetThumbRect();
    const D2D1_POINT_2F scrolledCenter = D2D1::Point2F((scrolledThumb.left + scrolledThumb.right) * 0.5f, (scrolledThumb.top + scrolledThumb.bottom) * 0.5f);
    const uint64_t scrolledStartTickMs = ::GetTickCount64();
    Require(scrolledSlider->OnMouseDown(scrolledWindow.Host(), scrolledCenter, false, 0u), "the scrolled slider accepts a touch press");
    static_cast<void>(scrolledSlider->Tick(scrolledWindow.Host(), scrolledStartTickMs + 200u));
    prepareCapture(scrolledWindow);
    WindowHostBitmapCapture scrolledDuring;
    Require(scrolledWindow.Host().DebugCaptureBitmap(scrolledDuring), "the scrolled touch feedback capture succeeds");
    Require(capturePixel(scrolledBefore, scrolledWindow.Host(), 110.0f, 85.0f) == capturePixel(scrolledDuring, scrolledWindow.Host(), 110.0f, 85.0f),
            "the scrolled viewport clip contains halo paint outside its lower edge");
    Require(capturePixel(scrolledBefore, scrolledWindow.Host(), 124.0f, 78.0f) != capturePixel(scrolledDuring, scrolledWindow.Host(), 124.0f, 78.0f),
            "the same transformed clip keeps visible halo paint aligned inside the scrolled viewport");
    Require(scrolledWindow.Host().DebugHitTestControl(D2D1::Point2F(110.0f, 85.0f)) != scrolledSlider,
            "a touch halo beyond the scrolled viewport does not expand the slider's hit target");
    static_cast<void>(scrolledSlider->OnMouseUp(scrolledWindow.Host(), scrolledCenter, false, 0u));
    scrolledWindow.Host().DebugSetPointerDevice(PointerDevice::Mouse);
}

void TestSliderReducedMotionSnapsInteractionAndValueAnimation()
{
    using namespace DxUi;

    ThemePalette theme  = MakeAnimatedTestThemePalette(true);
    theme.reducedMotion = true;

    WindowHost host;

    EnableMotionForTest(host);
    host.SetTheme(theme);

    auto root    = std::make_unique<Panel>();
    auto* slider = root->AddChild<Slider>();
    slider->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 48.0f));
    slider->SetMinimum(0.0);
    slider->SetMaximum(10.0);
    slider->SetValue(4.0);
    host.SetRoot(std::move(root));
    host.SetFocusControl(slider);

    slider->OnHoverChanged(host, true);
    RequireFloatNear(slider->DebugGetHoverAnimationProgress(), 1.0f, 0.0001f, "reduced-motion slider hover snaps directly to the target progress");
    Require(! slider->Tick(host, 0u), "reduced-motion slider hover does not request animation ticks");

    Require(slider->OnKeyDown(host, VK_RIGHT, 0), "reduced-motion slider still accepts keyboard steps");
    RequireFloatNear(static_cast<float>(slider->GetValue()), 5.0f, 0.0001f, "reduced-motion slider keyboard steps update the model immediately");
    RequireFloatNear(static_cast<float>(slider->DebugGetDisplayedValue()), 5.0f, 0.0001f, "reduced-motion slider keyboard steps snap the painted thumb");
    Require(! slider->Tick(host, 80u), "reduced-motion slider keyboard steps do not request animation ticks");

    const D2D1_RECT_F thumb    = slider->DebugGetThumbRect();
    const D2D1_POINT_2F center = D2D1::Point2F((thumb.left + thumb.right) * 0.5f, (thumb.top + thumb.bottom) * 0.5f);
    host.DebugSetPointerDevice(PointerDevice::Touch);
    Require(slider->OnMouseDown(host, center, false, 0), "reduced-motion slider still accepts a touch drag");
    RequireFloatNear(slider->DebugGetTouchHaloProgress(), 1.0f, 0.0001f, "reduced-motion touch halo snaps in");
    Require(slider->OnMouseUp(host, center, false, 0), "reduced-motion touch drag ends");
    RequireFloatNear(slider->DebugGetTouchHaloProgress(), 0.0f, 0.0001f, "reduced-motion touch halo snaps out");
    Require(! slider->Tick(host, 160u), "reduced-motion touch halo does not request animation ticks");
    host.DebugSetPointerDevice(PointerDevice::Mouse);
}

void TestSliderKeyboardStepsAnimateDisplayedThumb()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    auto root    = std::make_unique<Panel>();
    auto* slider = root->AddChild<Slider>();
    slider->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 48.0f));
    slider->SetMinimum(0.0);
    slider->SetMaximum(10.0);
    slider->SetValue(2.0);
    host.SetRoot(std::move(root));
    host.SetFocusControl(slider);

    const uint64_t stepStartTickMs = ::GetTickCount64();
    Require(slider->OnKeyDown(host, VK_RIGHT, 0), "slider keyboard step starts a committed value change");
    RequireFloatNear(static_cast<float>(slider->GetValue()), 3.0f, 0.0001f, "slider keyboard step updates the model immediately");
    RequireFloatNear(static_cast<float>(slider->DebugGetDisplayedValue()),
                     2.0f,
                     0.0001f,
                     "slider keyboard step keeps the painted thumb at the previous value until the first tick");
    Require(slider->Tick(host, stepStartTickMs + 70u), "slider value animation advances on the first tick");
    Require(slider->DebugGetDisplayedValue() > 2.0 && slider->DebugGetDisplayedValue() < 3.0, "slider value animation exposes an in-flight painted value");
    Require(slider->Tick(host, stepStartTickMs + 200u), "slider value animation requests one final repaint when it settles");
    RequireFloatNear(static_cast<float>(slider->DebugGetDisplayedValue()), 3.0f, 0.0001f, "slider value animation settles on the committed value");
    Require(! slider->Tick(host, stepStartTickMs + 260u), "settled slider value animation stops requesting ticks");
}

void TestTextFieldTickTracksFocusedCaretAnimation()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    ThemePalette theme  = host.GetTheme();
    theme.reducedMotion = false;
    host.SetTheme(theme);
    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"alpha");
    host.SetRoot(std::move(root));
    host.SetFocusControl(field);

    Require(field->Tick(host, 0u), "focused text field requests caret animation ticks");
    host.SetFocusControl(nullptr);
    Require(! field->Tick(host, 1200u), "unfocused text field stops requesting caret animation ticks");
}

void TestEditableComboBoxTickTracksFocusedCaretAnimation()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    ThemePalette theme  = host.GetTheme();
    theme.reducedMotion = false;
    host.SetTheme(theme);
    auto root   = std::make_unique<Panel>();
    auto* combo = root->AddChild<ComboBox>();
    host.SetRoot(std::move(root));
    combo->SetEditable(true);
    combo->SetText(L"alpha");
    host.SetFocusControl(combo);

    Require(combo->Tick(host, 0u), "focused editable combo requests caret animation ticks");
    host.SetFocusControl(nullptr);
    Require(! combo->Tick(host, 1200u), "unfocused editable combo stops requesting caret animation ticks");
}

void TestPanelOverlayPaintsComboPopupAfterLaterSiblingContent()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    Panel root;
    root.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f));

    std::vector<std::string> events;
    auto* combo   = root.AddChild<PaintTraceComboBox>(events);
    auto* sibling = root.AddChild<PaintTraceControl>(events, "sibling");
    combo->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 28.0f));
    sibling->SetBounds(D2D1::RectF(0.0f, 36.0f, 180.0f, 64.0f));
    combo->SetItems({ComboBox::Item{L"one", L"One"}, ComboBox::Item{L"two", L"Two"}});

    Require(combo->OnMouseDown(host, D2D1::Point2F(172.0f, 12.0f), false, 0), "trace combo opens popup for overlay pass");
    Require(combo->GetHitBounds().bottom > combo->GetBounds().bottom, "trace combo popup expands hit bounds");

    root.Paint(host);
    root.PaintOverlay(host);

    Require(events.size() == 3u, "overlay pass records combo, sibling, and popup paint events");
    Require(events[0] == "combo-base", "combo body paints in normal pass");
    Require(events[1] == "sibling", "later sibling paints before overlay content");
    Require(events[2] == "combo-popup", "combo popup paints in overlay pass after later sibling content");
}

void TestPopupLayerPaintsOnlyInOverlayPass()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    Panel root;
    root.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f));

    std::vector<std::string> events;
    auto* background = root.AddChild<PaintTraceControl>(events, "background");
    auto* popupLayer = root.AddChild<PopupLayer>();
    auto* popupChild = popupLayer->AddChild<PaintTraceControl>(events, "popup-child");
    background->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 28.0f));
    popupLayer->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f));
    popupChild->SetBounds(D2D1::RectF(0.0f, 36.0f, 180.0f, 64.0f));

    root.Paint(host);
    root.PaintOverlay(host);

    Require(events.size() == 2u, "popup layer paints only background and overlay child");
    Require(events[0] == "background", "normal sibling paints in normal pass");
    Require(events[1] == "popup-child", "popup layer child paints only during overlay pass");
}

void TestPanelTickPropagatesAnimatedGrid()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    ThemePalette theme  = host.GetTheme();
    theme.reducedMotion = false;
    host.SetTheme(theme);
    Panel root;
    root.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    auto* grid = root.AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    GridCellData cellData;
    cellData.kind = GridCellKind::Spinner;
    cellData.text = L"Loading";
    SingleCellGridModel model(std::move(cellData));
    grid->SetModel(&model);

    Require(root.Tick(host, 0u), "panel tick propagates animated grid state");
}

void TestGridTickReturnsFalseForTextOnlyCells()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    GridCellData cellData;
    cellData.kind = GridCellKind::Text;
    cellData.text = L"Done";
    SingleCellGridModel model(std::move(cellData));
    grid.SetModel(&model);

    Require(! grid.Tick(host, 0u), "text-only grid does not request animation ticks");
}

void TestGridTickReturnsTrueForHeaderBusyIndicator()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    ThemePalette theme  = host.GetTheme();
    theme.reducedMotion = false;
    host.SetTheme(theme);
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    GridCellData cellData;
    cellData.kind = GridCellKind::Text;
    cellData.text = L"Ready";
    SingleCellGridModel model(std::move(cellData));
    grid.SetModel(&model);
    grid.SetHeaderBusy(true);

    Require(grid.Tick(host, 0u), "header busy indicator requests animation ticks");
}

void TestGridTickReturnsTrueForSortGlyphTransition()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    ThemePalette theme  = host.GetTheme();
    theme.reducedMotion = false;
    host.SetTheme(theme);
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    GridCellData cellData;
    cellData.kind = GridCellKind::Text;
    cellData.text = L"Ready";
    SingleCellGridModel model(std::move(cellData));
    grid.SetModel(&model);

    const uint64_t initialTick = GetTickCount64();
    grid.SetSortSpec({0u, SortDirection::Ascending});

    const GridSortGlyphVisualState startState = grid.DebugGetSortGlyphVisualState(theme, 0u, initialTick);
    Require(startState.animating, "sort glyph transition starts animating after none->ascending");
    Require(startState.currentDirection == SortDirection::Ascending, "sort glyph transition targets ascending after first sort");
    Require(startState.currentAlpha <= 0.15f, "sort glyph transition starts faded in from a near-zero alpha");
    Require(startState.previousAlpha <= 0.0001f, "sort glyph transition has no previous glyph when sorting from none");
    Require(grid.Tick(host, initialTick), "sort glyph transition requests animation ticks");

    const GridSortGlyphVisualState midState = grid.DebugGetSortGlyphVisualState(theme, 0u, initialTick + 70u);
    Require(midState.animating, "sort glyph transition remains active mid-animation");
    Require(midState.currentAlpha > 0.0f && midState.currentAlpha < 1.0f, "sort glyph transition fades in the current glyph");

    const uint64_t flipTick = GetTickCount64();
    grid.SetSortSpec({0u, SortDirection::Descending});
    const GridSortGlyphVisualState flipState = grid.DebugGetSortGlyphVisualState(theme, 0u, flipTick);
    Require(flipState.animating, "sort glyph direction flip starts a new animation");
    Require(flipState.currentDirection == SortDirection::Descending, "sort glyph direction flip targets descending");
    Require(flipState.previousDirection == SortDirection::Ascending, "sort glyph direction flip preserves the previous ascending glyph");
    Require(flipState.currentAlpha <= 0.15f, "sort glyph direction flip starts the new glyph near zero alpha");
    Require(flipState.previousAlpha >= 0.85f, "sort glyph direction flip starts the old glyph nearly fully visible");

    const GridSortGlyphVisualState flipMidState = grid.DebugGetSortGlyphVisualState(theme, 0u, flipTick + 70u);
    Require(flipMidState.currentAlpha > 0.0f && flipMidState.currentAlpha < 1.0f, "sort glyph direction flip fades in the new glyph");
    Require(flipMidState.previousAlpha > 0.0f && flipMidState.previousAlpha < 1.0f, "sort glyph direction flip fades out the previous glyph");
    Require(grid.Tick(host, flipTick + 70u), "sort glyph direction flip continues requesting animation ticks mid-transition");

    const GridSortGlyphVisualState settledState = grid.DebugGetSortGlyphVisualState(theme, 0u, flipTick + 200u);
    Require(! settledState.animating, "completed sort glyph transition stops animating");
    Require(settledState.currentDirection == SortDirection::Descending, "completed sort glyph transition preserves the final descending glyph");
    RequireFloatNear(settledState.currentAlpha, 1.0f, 0.0001f, "completed sort glyph transition leaves the final glyph fully visible");
    RequireFloatNear(settledState.previousAlpha, 0.0f, 0.0001f, "completed sort glyph transition clears the previous glyph");
    Require(! grid.Tick(host, flipTick + 200u), "completed sort glyph transition stops requesting animation ticks");
}

void TestReducedMotionSuppressesSortGlyphTransition()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    ThemePalette theme  = host.GetTheme();
    theme.reducedMotion = true;
    host.SetTheme(theme);
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    GridCellData cellData;
    cellData.kind = GridCellKind::Text;
    cellData.text = L"Ready";
    SingleCellGridModel model(std::move(cellData));
    grid.SetModel(&model);
    grid.SetSortSpec({0u, SortDirection::Ascending});

    const GridSortGlyphVisualState state = grid.DebugGetSortGlyphVisualState(theme, 0u, 1000u);
    Require(! state.animating, "reduced motion suppresses sort glyph animation state");
    Require(state.currentDirection == SortDirection::Ascending, "reduced motion still resolves the final sort glyph direction");
    RequireFloatNear(state.currentAlpha, 1.0f, 0.0001f, "reduced motion resolves sort glyph alpha immediately");
    RequireFloatNear(state.previousAlpha, 0.0f, 0.0001f, "reduced motion does not preserve a previous sort glyph");
    Require(! grid.Tick(host, 1000u), "reduced motion suppresses sort glyph animation ticks");
}

void TestGridScrollbarFeedbackFollowsHoverAndDragState()
{
    using namespace DxUi;

    const auto thumbCenter = [](const D2D1_RECT_F& thumbRect) noexcept
    { return D2D1::Point2F((thumbRect.left + thumbRect.right) * 0.5f, (thumbRect.top + thumbRect.bottom) * 0.5f); };
    const auto trackPointOutsideThumb = [](const D2D1_RECT_F& trackRect, const D2D1_RECT_F& thumbRect, bool vertical) noexcept
    {
        if (vertical)
        {
            const float x = (trackRect.left + trackRect.right) * 0.5f;
            const float y = (thumbRect.top - trackRect.top > 2.0f) ? ((trackRect.top + thumbRect.top) * 0.5f) : ((thumbRect.bottom + trackRect.bottom) * 0.5f);
            return D2D1::Point2F(x, y);
        }

        const float y = (trackRect.top + trackRect.bottom) * 0.5f;
        const float x = (thumbRect.left - trackRect.left > 2.0f) ? ((trackRect.left + thumbRect.left) * 0.5f) : ((thumbRect.right + trackRect.right) * 0.5f);
        return D2D1::Point2F(x, y);
    };

    WindowHost host;

    EnableMotionForTest(host);
    const ThemePalette theme = MakeAnimatedTestThemePalette(true);
    host.SetTheme(theme);
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    grid.SetRowHeightDip(24.0f);

    LargeGridModel model(10'000u, 64u, 96.0f);
    grid.SetModel(&model);

    GridScrollbarVisualState state = grid.DebugGetScrollbarVisualState(theme);
    Require(state.hasVerticalScrollbar, "grid scrollbar feedback test exposes a vertical scrollbar");
    Require(state.hasHorizontalScrollbar, "grid scrollbar feedback test exposes a horizontal scrollbar");
    Require(state.verticalTrackArgb == PackColorForTest(theme.scrollbarTrack), "grid scrollbar feedback starts with the idle vertical track color");
    Require(state.verticalThumbArgb == PackColorForTest(theme.scrollbarThumb), "grid scrollbar feedback starts with the idle vertical thumb color");
    Require(state.horizontalTrackArgb == PackColorForTest(theme.scrollbarTrack), "grid scrollbar feedback starts with the idle horizontal track color");
    Require(state.horizontalThumbArgb == PackColorForTest(theme.scrollbarThumb), "grid scrollbar feedback starts with the idle horizontal thumb color");
    RequireFloatNear(state.verticalTrackHotProgress, 0.0f, 0.0001f, "grid scrollbar feedback starts with no vertical track hover progress");
    RequireFloatNear(state.verticalThumbHotProgress, 0.0f, 0.0001f, "grid scrollbar feedback starts with no vertical thumb hover progress");
    RequireFloatNear(state.horizontalTrackHotProgress, 0.0f, 0.0001f, "grid scrollbar feedback starts with no horizontal track hover progress");
    RequireFloatNear(state.horizontalThumbHotProgress, 0.0f, 0.0001f, "grid scrollbar feedback starts with no horizontal thumb hover progress");

    const D2D1_POINT_2F verticalTrackPoint = trackPointOutsideThumb(state.verticalTrackRect, state.verticalThumbRect, true);
    Require(grid.OnMouseMove(host, verticalTrackPoint, 0), "grid scrollbar feedback handles vertical track hover");
    state = grid.DebugGetScrollbarVisualState(theme);
    Require(state.verticalTrackHovered, "grid scrollbar feedback marks the vertical track hovered");
    Require(! state.verticalThumbHovered, "grid scrollbar feedback keeps the vertical thumb distinct from track hover");
    RequireFloatNear(state.verticalTrackHotProgress, 0.0f, 0.0001f, "grid vertical track hover begins from the current track progress");
    RequireFloatNear(state.verticalThumbHotProgress, 0.0f, 0.0001f, "grid vertical track hover begins from the current thumb progress");
    Require(grid.Tick(host, 0u), "grid vertical track hover animation anchors on the first tick");
    Require(grid.Tick(host, 35u), "grid vertical track hover animation continues mid-transition");
    state = grid.DebugGetScrollbarVisualState(theme);
    Require(state.verticalTrackHotProgress > 0.0f && state.verticalTrackHotProgress < 1.0f,
            "grid vertical track hover exposes an in-flight track progress value");
    Require(state.verticalThumbHotProgress > 0.0f && state.verticalThumbHotProgress < 0.45f,
            "grid vertical track hover exposes an in-flight thumb warmup value");
    Require(state.verticalTrackArgb != PackColorForTest(theme.scrollbarTrack), "grid scrollbar feedback tints the vertical track mid-transition");
    Require(state.verticalThumbArgb != PackColorForTest(theme.scrollbarThumb), "grid scrollbar feedback warms the vertical thumb mid-transition");
    Require(grid.Tick(host, 160u), "grid vertical track hover requests a final repaint when settling");
    state = grid.DebugGetScrollbarVisualState(theme);
    RequireFloatNear(state.verticalTrackHotProgress, 1.0f, 0.0001f, "grid vertical track hover settles to a fully warm track");
    RequireFloatNear(state.verticalThumbHotProgress, 0.45f, 0.0001f, "grid vertical track hover settles to the shared thumb warmup strength");

    const D2D1_POINT_2F verticalThumbPoint = thumbCenter(state.verticalThumbRect);
    Require(grid.OnMouseMove(host, verticalThumbPoint, 0), "grid scrollbar feedback handles vertical thumb hover");
    state = grid.DebugGetScrollbarVisualState(theme);
    Require(state.verticalThumbHovered, "grid scrollbar feedback marks the vertical thumb hovered");
    Require(! state.verticalTrackHovered, "grid scrollbar feedback clears vertical track hover when the thumb is hovered");
    RequireFloatNear(state.verticalThumbHotProgress, 0.45f, 0.0001f, "grid vertical thumb hover continues from the track-hover warmup level");
    Require(grid.Tick(host, 160u), "grid vertical thumb-hover animation anchors on the current tick");
    Require(grid.Tick(host, 230u), "grid vertical thumb-hover animation continues mid-transition");
    state = grid.DebugGetScrollbarVisualState(theme);
    Require(state.verticalThumbHotProgress > 0.45f && state.verticalThumbHotProgress < 1.0f,
            "grid vertical thumb hover exposes an in-flight hot progress value");
    Require(grid.Tick(host, 320u), "grid vertical thumb-hover animation requests a final repaint when settling");
    state = grid.DebugGetScrollbarVisualState(theme);
    RequireFloatNear(state.verticalThumbHotProgress, 1.0f, 0.0001f, "grid vertical thumb hover settles at the hot thumb strength");
    Require(state.verticalThumbArgb == PackColorForTest(theme.scrollbarThumbHot), "grid scrollbar feedback uses the hot vertical thumb color after settling");

    Require(grid.OnMouseDown(host, verticalThumbPoint, false, 0), "grid scrollbar feedback handles vertical thumb drag start");
    state = grid.DebugGetScrollbarVisualState(theme);
    Require(state.verticalThumbDragging, "grid scrollbar feedback marks the vertical thumb as dragging");
    RequireFloatNear(state.verticalThumbHotProgress, 1.0f, 0.0001f, "grid vertical drag keeps the thumb fully hot");
    Require(state.verticalThumbArgb == PackColorForTest(theme.scrollbarThumbHot), "grid scrollbar feedback keeps the vertical thumb hot while dragging");

    const D2D1_POINT_2F dragPoint = D2D1::Point2F(verticalThumbPoint.x, std::min(state.verticalTrackRect.bottom - 6.0f, verticalThumbPoint.y + 24.0f));
    Require(grid.OnMouseMove(host, dragPoint, 0), "grid scrollbar feedback handles vertical thumb drag movement");
    state = grid.DebugGetScrollbarVisualState(theme);
    Require(state.verticalThumbDragging, "grid scrollbar feedback preserves drag state while the vertical thumb moves");

    const D2D1_POINT_2F dragThumbPoint = thumbCenter(state.verticalThumbRect);
    Require(grid.OnMouseUp(host, dragThumbPoint, false, 0), "grid scrollbar feedback handles vertical thumb drag release");
    state = grid.DebugGetScrollbarVisualState(theme);
    Require(! state.verticalThumbDragging, "grid scrollbar feedback clears vertical thumb drag state on release");
    Require(state.verticalThumbHovered, "grid scrollbar feedback keeps the vertical thumb hot when the pointer releases over the thumb");

    const D2D1_POINT_2F horizontalTrackPoint = trackPointOutsideThumb(state.horizontalTrackRect, state.horizontalThumbRect, false);
    Require(grid.OnMouseMove(host, horizontalTrackPoint, 0), "grid scrollbar feedback handles horizontal track hover");
    state = grid.DebugGetScrollbarVisualState(theme);
    Require(state.horizontalTrackHovered, "grid scrollbar feedback marks the horizontal track hovered");
    Require(! state.horizontalThumbHovered, "grid scrollbar feedback keeps the horizontal thumb distinct from track hover");
    RequireFloatNear(state.horizontalTrackHotProgress, 0.0f, 0.0001f, "grid horizontal track hover begins from the current horizontal track progress");
    RequireFloatNear(state.horizontalThumbHotProgress, 0.0f, 0.0001f, "grid horizontal track hover begins from the current horizontal thumb progress");
    Require(grid.Tick(host, 320u), "grid horizontal track hover animation anchors on the current tick");
    Require(grid.Tick(host, 355u), "grid horizontal track hover animation continues mid-transition");
    state = grid.DebugGetScrollbarVisualState(theme);
    Require(state.horizontalTrackHotProgress > 0.0f && state.horizontalTrackHotProgress < 1.0f,
            "grid horizontal track hover exposes an in-flight track progress value");
    Require(state.horizontalThumbHotProgress > 0.0f && state.horizontalThumbHotProgress < 0.45f,
            "grid horizontal track hover exposes an in-flight thumb warmup value");
    Require(state.horizontalTrackArgb != PackColorForTest(theme.scrollbarTrack), "grid scrollbar feedback tints the horizontal track mid-transition");
    Require(state.horizontalThumbArgb != PackColorForTest(theme.scrollbarThumb), "grid scrollbar feedback warms the horizontal thumb mid-transition");
    Require(grid.Tick(host, 480u), "grid horizontal track hover requests a final repaint when settling");
    state = grid.DebugGetScrollbarVisualState(theme);
    RequireFloatNear(state.horizontalTrackHotProgress, 1.0f, 0.0001f, "grid horizontal track hover settles to a fully warm track");
    RequireFloatNear(state.horizontalThumbHotProgress, 0.45f, 0.0001f, "grid horizontal track hover settles to the shared thumb warmup strength");

    Require(grid.OnMouseLeave(host), "grid scrollbar feedback handles mouse leave");
    state = grid.DebugGetScrollbarVisualState(theme);
    Require(! state.verticalTrackHovered && ! state.verticalThumbHovered, "grid scrollbar feedback clears vertical hover state on leave");
    Require(! state.horizontalTrackHovered && ! state.horizontalThumbHovered, "grid scrollbar feedback clears horizontal hover state on leave");
    Require(grid.Tick(host, 480u), "grid scrollbar leave animation anchors on the first fade-out tick");
    Require(grid.Tick(host, 515u), "grid scrollbar leave animation continues mid-transition");
    state = grid.DebugGetScrollbarVisualState(theme);
    Require(state.verticalTrackHotProgress >= 0.0f && state.verticalTrackHotProgress < 1.0f, "grid scrollbar leave fades the vertical track back toward idle");
    Require(state.horizontalTrackHotProgress > 0.0f && state.horizontalTrackHotProgress < 1.0f,
            "grid scrollbar leave fades the horizontal track back toward idle");
    Require(state.verticalThumbHotProgress >= 0.0f && state.verticalThumbHotProgress < 1.0f, "grid scrollbar leave fades the vertical thumb back toward idle");
    Require(state.horizontalThumbHotProgress > 0.0f && state.horizontalThumbHotProgress < 0.45f,
            "grid scrollbar leave fades the horizontal thumb back toward idle");
    Require(grid.Tick(host, 640u), "grid scrollbar leave animation requests a final repaint when settling");
    state = grid.DebugGetScrollbarVisualState(theme);
    RequireFloatNear(state.verticalTrackHotProgress, 0.0f, 0.0001f, "grid scrollbar leave settles the vertical track back to idle");
    RequireFloatNear(state.verticalThumbHotProgress, 0.0f, 0.0001f, "grid scrollbar leave settles the vertical thumb back to idle");
    RequireFloatNear(state.horizontalTrackHotProgress, 0.0f, 0.0001f, "grid scrollbar leave settles the horizontal track back to idle");
    RequireFloatNear(state.horizontalThumbHotProgress, 0.0f, 0.0001f, "grid scrollbar leave settles the horizontal thumb back to idle");
    Require(state.verticalTrackArgb == PackColorForTest(theme.scrollbarTrack), "grid scrollbar feedback restores the idle vertical track color on leave");
    Require(state.verticalThumbArgb == PackColorForTest(theme.scrollbarThumb), "grid scrollbar feedback restores the idle vertical thumb color on leave");
    Require(state.horizontalTrackArgb == PackColorForTest(theme.scrollbarTrack), "grid scrollbar feedback restores the idle horizontal track color on leave");
    Require(state.horizontalThumbArgb == PackColorForTest(theme.scrollbarThumb), "grid scrollbar feedback restores the idle horizontal thumb color on leave");
    Require(! grid.Tick(host, 700u), "settled grid scrollbar animation stops requesting ticks");
}

void TestGridScrollbarReducedMotionSnapsFeedback()
{
    using namespace DxUi;

    const auto trackPointOutsideThumb = [](const D2D1_RECT_F& trackRect, const D2D1_RECT_F& thumbRect, bool vertical) noexcept
    {
        if (vertical)
        {
            const float x = (trackRect.left + trackRect.right) * 0.5f;
            const float y = (thumbRect.top - trackRect.top > 2.0f) ? ((trackRect.top + thumbRect.top) * 0.5f) : ((thumbRect.bottom + trackRect.bottom) * 0.5f);
            return D2D1::Point2F(x, y);
        }

        const float y = (trackRect.top + trackRect.bottom) * 0.5f;
        const float x = (thumbRect.left - trackRect.left > 2.0f) ? ((trackRect.left + thumbRect.left) * 0.5f) : ((thumbRect.right + trackRect.right) * 0.5f);
        return D2D1::Point2F(x, y);
    };

    ThemePalette theme  = MakeAnimatedTestThemePalette(true);
    theme.reducedMotion = true;

    WindowHost host;

    EnableMotionForTest(host);
    host.SetTheme(theme);
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    grid.SetRowHeightDip(24.0f);

    LargeGridModel model(10'000u, 64u, 96.0f);
    grid.SetModel(&model);

    GridScrollbarVisualState state         = grid.DebugGetScrollbarVisualState(theme);
    const D2D1_POINT_2F verticalTrackPoint = trackPointOutsideThumb(state.verticalTrackRect, state.verticalThumbRect, true);
    Require(grid.OnMouseMove(host, verticalTrackPoint, 0), "reduced-motion grid scrollbar handles vertical track hover");
    state = grid.DebugGetScrollbarVisualState(theme);
    RequireFloatNear(state.verticalTrackHotProgress, 1.0f, 0.0001f, "reduced-motion grid scrollbar snaps the vertical track to fully warm");
    RequireFloatNear(state.verticalThumbHotProgress, 0.45f, 0.0001f, "reduced-motion grid scrollbar snaps the vertical thumb to the shared warmup value");
    Require(! grid.Tick(host, 0u), "reduced-motion grid scrollbar does not request animation ticks");

    const D2D1_POINT_2F horizontalTrackPoint = trackPointOutsideThumb(state.horizontalTrackRect, state.horizontalThumbRect, false);
    Require(grid.OnMouseMove(host, horizontalTrackPoint, 0), "reduced-motion grid scrollbar handles horizontal track hover");
    state = grid.DebugGetScrollbarVisualState(theme);
    RequireFloatNear(state.horizontalTrackHotProgress, 1.0f, 0.0001f, "reduced-motion grid scrollbar snaps the horizontal track to fully warm");
    RequireFloatNear(state.horizontalThumbHotProgress, 0.45f, 0.0001f, "reduced-motion grid scrollbar snaps the horizontal thumb to the shared warmup value");

    Require(grid.OnMouseLeave(host), "reduced-motion grid scrollbar handles mouse leave");
    state = grid.DebugGetScrollbarVisualState(theme);
    RequireFloatNear(state.verticalTrackHotProgress, 0.0f, 0.0001f, "reduced-motion grid scrollbar snaps the vertical track back to idle");
    RequireFloatNear(state.verticalThumbHotProgress, 0.0f, 0.0001f, "reduced-motion grid scrollbar snaps the vertical thumb back to idle");
    RequireFloatNear(state.horizontalTrackHotProgress, 0.0f, 0.0001f, "reduced-motion grid scrollbar snaps the horizontal track back to idle");
    RequireFloatNear(state.horizontalThumbHotProgress, 0.0f, 0.0001f, "reduced-motion grid scrollbar snaps the horizontal thumb back to idle");
}

void TestGridTickReturnsTrueForIndeterminateMarquee()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    ThemePalette theme  = host.GetTheme();
    theme.reducedMotion = false;
    host.SetTheme(theme);
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    GridCellData cellData;
    cellData.kind     = GridCellKind::Marquee;
    cellData.text     = L"Waiting";
    cellData.progress = 0.0f;
    SingleCellGridModel model(std::move(cellData));
    grid.SetModel(&model);

    Require(grid.Tick(host, 0u), "indeterminate marquee grid requests animation ticks");
}

void TestGridTickReturnsFalseForDeterminateProgress()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    ThemePalette theme  = host.GetTheme();
    theme.reducedMotion = false;
    host.SetTheme(theme);
    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    GridCellData cellData;
    cellData.kind     = GridCellKind::Marquee;
    cellData.text     = L"Halfway";
    cellData.progress = 0.5f;
    SingleCellGridModel model(std::move(cellData));
    grid.SetModel(&model);

    Require(! grid.Tick(host, 0u), "determinate progress cell does not request animation ticks");
}

void TestGridTickReturnsFalseForSpinnerWhenReducedMotionEnabled()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    ThemePalette theme  = host.GetTheme();
    theme.reducedMotion = true;
    host.SetTheme(theme);

    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    GridCellData cellData;
    cellData.kind = GridCellKind::Spinner;
    cellData.text = L"Loading";
    SingleCellGridModel model(std::move(cellData));
    grid.SetModel(&model);

    Require(! grid.Tick(host, 0u), "reduced-motion theme suppresses spinner animation ticks");
}

void TestGridTickReturnsFalseForHeaderBusyIndicatorWhenReducedMotionEnabled()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    ThemePalette theme  = host.GetTheme();
    theme.reducedMotion = true;
    host.SetTheme(theme);

    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    GridCellData cellData;
    cellData.kind = GridCellKind::Text;
    cellData.text = L"Ready";
    SingleCellGridModel model(std::move(cellData));
    grid.SetModel(&model);
    grid.SetHeaderBusy(true);

    Require(! grid.Tick(host, 0u), "reduced-motion theme suppresses header busy animation ticks");
}

void TestGridTickReturnsFalseForIndeterminateMarqueeWhenReducedMotionEnabled()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    ThemePalette theme  = host.GetTheme();
    theme.reducedMotion = true;
    host.SetTheme(theme);

    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    GridCellData cellData;
    cellData.kind     = GridCellKind::Marquee;
    cellData.text     = L"Waiting";
    cellData.progress = 0.0f;
    SingleCellGridModel model(std::move(cellData));
    grid.SetModel(&model);

    Require(! grid.Tick(host, 0u), "reduced-motion theme suppresses marquee animation ticks");
}

void TestTextFieldCaretTickIgnoresReducedMotion()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    ThemePalette theme  = host.GetTheme();
    theme.reducedMotion = true;
    host.SetTheme(theme);

    TextField field(L"alpha");
    host.SetFocusControl(&field);

    Require(field.Tick(host, 0u), "reduced motion does not suppress native text caret animation ticks");
}

void TestEditableComboBoxCaretTickIgnoresReducedMotion()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    ThemePalette theme  = host.GetTheme();
    theme.reducedMotion = true;
    host.SetTheme(theme);

    ComboBox combo;
    combo.SetEditable(true);
    combo.SetText(L"alpha");
    host.SetFocusControl(&combo);

    Require(combo.Tick(host, 0u), "reduced motion does not suppress editable combo caret animation ticks");
}

std::unique_ptr<DxUi::Panel> MakeAnimatedPage(std::wstring title, const D2D1_RECT_F& heroBounds, std::wstring_view heroKey)
{
    using namespace DxUi;

    auto page    = std::make_unique<Panel>();
    auto* label  = page->AddChild<Label>(std::move(title));
    auto* button = page->AddChild<Button>(L"Open");
    label->SetBounds(D2D1::RectF(12.0f, 12.0f, 240.0f, 40.0f));
    button->SetBounds(heroBounds);
    button->SetPrimary(true);
    button->SetConnectedAnimationKey(std::wstring(heroKey));
    return page;
}

void TestEasingCurvesStayBoundedAndMonotonic()
{
    using namespace DxUi;

    const std::array<float, 5u> samplePoints{0.0f, 0.2f, 0.5f, 0.8f, 1.0f};

    float previousLinear = -1.0f;
    float previousFast   = -1.0f;
    float previousPoint  = -1.0f;
    for (const float sample : samplePoints)
    {
        const float linear = EvaluateEasing(EasingCurve::Linear, sample);
        const float fast   = EvaluateEasing(EasingCurve::FastDecelerate, sample);
        const float point  = EvaluateEasing(EasingCurve::PointToPoint, sample);
        Require(linear >= 0.0f && linear <= 1.0f, "linear easing remains in [0, 1]");
        Require(fast >= 0.0f && fast <= 1.0f, "fast-decelerate easing remains in [0, 1]");
        Require(point >= 0.0f && point <= 1.0f, "point-to-point easing remains in [0, 1]");
        Require(linear >= previousLinear, "linear easing stays monotonic");
        Require(fast >= previousFast, "fast-decelerate easing stays monotonic");
        Require(point >= previousPoint, "point-to-point easing stays monotonic");
        previousLinear = linear;
        previousFast   = fast;
        previousPoint  = point;
    }

    RequireFloatNear(EvaluateEasing(EasingCurve::Linear, 0.5f), 0.5f, 0.0001f, "linear easing keeps the midpoint unchanged");
    Require(EvaluateEasing(EasingCurve::FastDecelerate, 0.5f) > 0.80f, "fast-decelerate easing front-loads progress at the midpoint");
    Require(EvaluateEasing(EasingCurve::PointToPoint, 0.5f) > EvaluateEasing(EasingCurve::Linear, 0.5f),
            "point-to-point easing still advances faster than linear at the midpoint");
    Require(std::fabs(EvaluateEasing(EasingCurve::PointToPoint, 0.5f) - EvaluateEasing(EasingCurve::FastDecelerate, 0.5f)) > 0.01f,
            "point-to-point easing remains meaningfully distinct from the direct-entrance curve");
}

void TestPageHostPageTransitionAnimatesAndSettles()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    ThemePalette theme  = host.GetTheme();
    theme.reducedMotion = false;
    host.SetTheme(theme);

    auto root      = std::make_unique<Panel>();
    auto* pageHost = root->AddChild<PageHost>();
    pageHost->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f));
    host.SetRoot(std::move(root));

    pageHost->SetPage(MakeAnimatedPage(L"First", D2D1::RectF(20.0f, 52.0f, 116.0f, 88.0f), L"hero"));
    pageHost->SetPage(MakeAnimatedPage(L"Second", D2D1::RectF(184.0f, 120.0f, 296.0f, 156.0f), L"hero"), L"hero");

    Require(pageHost->HasActiveTransition(), "page host starts a transition when replacing a page on a live host");
    const PageHostDebugState startState = pageHost->DebugGetTransitionState(0u);
    Require(startState.active, "page host debug state reports an active transition at the start");
    RequireFloatNear(startState.linearProgress, 0.0f, 0.0001f, "page host transition starts at zero progress");

    const uint64_t midTick = startState.startTickMs + 125u;
    Require(pageHost->Tick(host, midTick), "page host mid-transition tick keeps the host subscribed");
    const PageHostDebugState midState = pageHost->DebugGetTransitionState(midTick);
    Require(midState.active, "page host transition remains active mid-flight");
    Require(midState.linearProgress > 0.0f && midState.linearProgress < 1.0f, "page host transition exposes in-flight progress");
    Require(midState.incomingOpacity > 0.0f && midState.incomingOpacity < 1.0f, "page host transition fades the incoming page in");
    Require(midState.outgoingOpacity > 0.0f && midState.outgoingOpacity < 1.0f, "page host transition fades the outgoing page out");
    Require(midState.incomingOffsetXDip > 0.0f, "page host transition keeps an in-flight entrance offset");
    Require(midState.outgoingOffsetXDip < 0.0f, "page host transition moves the outgoing page toward the trailing edge");

    Require(pageHost->Tick(host, startState.startTickMs + 320u), "page host completion tick requests a final repaint");
    Require(! pageHost->HasActiveTransition(), "page host transition settles once the duration elapses");
    const PageHostDebugState settledState = pageHost->DebugGetTransitionState(startState.startTickMs + 320u);
    Require(! settledState.active, "page host debug state clears the active flag after settling");
    RequireFloatNear(settledState.incomingOpacity, 1.0f, 0.0001f, "page host leaves the incoming page fully visible after settling");
    RequireFloatNear(settledState.outgoingOpacity, 0.0f, 0.0001f, "page host removes the outgoing page after settling");
}

void TestPageHostConnectedAnimationInterpolatesSharedElementRect()
{
    using namespace DxUi;

    const D2D1_RECT_F sourceHeroBounds = D2D1::RectF(20.0f, 52.0f, 116.0f, 88.0f);
    const D2D1_RECT_F targetHeroBounds = D2D1::RectF(184.0f, 120.0f, 296.0f, 156.0f);

    WindowHost host;

    EnableMotionForTest(host);
    auto root      = std::make_unique<Panel>();
    auto* pageHost = root->AddChild<PageHost>();
    pageHost->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f));
    host.SetRoot(std::move(root));

    pageHost->SetPage(MakeAnimatedPage(L"First", sourceHeroBounds, L"hero"));
    pageHost->SetPage(MakeAnimatedPage(L"Second", targetHeroBounds, L"hero"), L"hero");

    const PageHostDebugState startState = pageHost->DebugGetTransitionState(0u);
    const uint64_t midTick              = startState.startTickMs + 125u;
    pageHost->Tick(host, midTick);
    const PageHostDebugState midState = pageHost->DebugGetTransitionState(midTick);

    Require(midState.hasConnectedAnimation, "page host shared-element transition records a connected animation");
    Require(midState.currentConnectedRectDip.left > sourceHeroBounds.left && midState.currentConnectedRectDip.left < targetHeroBounds.left,
            "page host connected animation interpolates the shared element's left edge");
    Require(midState.currentConnectedRectDip.top > sourceHeroBounds.top && midState.currentConnectedRectDip.top < targetHeroBounds.top,
            "page host connected animation interpolates the shared element's top edge");
    Require(midState.currentConnectedRectDip.right > sourceHeroBounds.right && midState.currentConnectedRectDip.right < targetHeroBounds.right,
            "page host connected animation interpolates the shared element's right edge");
    Require(midState.currentConnectedRectDip.bottom > sourceHeroBounds.bottom && midState.currentConnectedRectDip.bottom < targetHeroBounds.bottom,
            "page host connected animation interpolates the shared element's bottom edge");
}

void TestPageHostConnectedOverlayAnimationEmitsCompositionGateMetrics()
{
    using namespace DxUi;

    DxUi::Ui::AnimationDispatcher::GetInstance().Shutdown();

    ScopedAnimationPerfJsonl perfJsonl;
    Require(! perfJsonl.Path().empty(), "connected overlay animation gate has a perf JSONL sink");

    const D2D1_RECT_F sourceHeroBounds = D2D1::RectF(20.0f, 52.0f, 116.0f, 88.0f);
    const D2D1_RECT_F targetHeroBounds = D2D1::RectF(184.0f, 120.0f, 296.0f, 156.0f);

    std::string appendedMetrics;
    {
        AttachedHostWindow window;
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        window.PumpMessages();

        ThemePalette theme  = window.Host().GetTheme();
        theme.reducedMotion = false;
        window.Host().SetTheme(theme);

        auto root      = std::make_unique<Panel>();
        auto* pageHost = root->AddChild<PageHost>();
        pageHost->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f));
        window.Host().SetRoot(std::move(root));

        pageHost->SetPage(MakeAnimatedPage(L"First", sourceHeroBounds, L"hero"));

        WindowHostBitmapCapture warmCapture;
        Require(window.Host().DebugCaptureBitmap(warmCapture), "connected overlay animation gate warm capture succeeds");
        Require(warmCapture.widthPx > 0u && warmCapture.heightPx > 0u && ! warmCapture.bgraPixels.empty(),
                "connected overlay animation gate warm capture has pixels");

        const uintmax_t metricOffset = GetAnimationFileSizeOrZero(perfJsonl.Path());
        pageHost->SetPage(MakeAnimatedPage(L"Second", targetHeroBounds, L"hero"), L"hero");
        DxUi::Debug::Perf::Emit(L"dxui.animation.allowed_surface", L"lightweight_overlay_transform", 0u, 1u, 0u, S_OK);

        Require(pageHost->HasActiveTransition(), "connected overlay animation gate starts a transition");
        Require(pageHost->DebugGetTransitionState(0u).hasConnectedAnimation, "connected overlay animation gate uses connected overlay surface");

        for (int sample = 0; sample < 10 && pageHost->HasActiveTransition(); ++sample)
        {
            PumpAnimationMessagesForMs(18u);
            window.PumpMessages();

            WindowHostBitmapCapture capture;
            Require(window.Host().DebugCaptureBitmap(capture), "connected overlay animation gate capture succeeds");
            Require(capture.widthPx > 0u && capture.heightPx > 0u && ! capture.bgraPixels.empty(), "connected overlay animation gate capture has pixels");
        }

        appendedMetrics = ReadAnimationPerfJsonlFromOffset(perfJsonl.Path(), metricOffset);
    }

    constexpr std::array<std::string_view, 6> kExpectedMetrics = {{
        "\"metric\":\"dxui.animation.tick_delta_us\"",
        "\"metric\":\"dxui.animation.jitter_us\"",
        "\"metric\":\"dxui.frame.total_us\"",
        "\"metric\":\"dxui.frame.render_us\"",
        "\"metric\":\"dxui.frame.present_us\"",
        "\"metric\":\"dxui.animation.allowed_surface\"",
    }};
    for (const std::string_view metric : kExpectedMetrics)
    {
        Require(appendedMetrics.find(metric) != std::string::npos, "connected overlay animation gate emits required metric");
    }

    Require(CountAnimationMetricRows(appendedMetrics, "\"metric\":\"dxui.animation.connected_overlay.paint\"") > 0u,
            "connected overlay animation gate paints the allowed overlay transform surface");
    Require(CountAnimationMetricRows(appendedMetrics, "\"metric\":\"dxui.animation.jitter_us\"") >= 2u,
            "connected overlay animation gate records multiple animation jitter samples");

    DxUi::Ui::AnimationDispatcher::GetInstance().Shutdown();
}

void TestPageHostReducedMotionSnapsTransition()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    ThemePalette theme  = host.GetTheme();
    theme.reducedMotion = true;
    host.SetTheme(theme);

    auto root      = std::make_unique<Panel>();
    auto* pageHost = root->AddChild<PageHost>();
    pageHost->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f));
    host.SetRoot(std::move(root));

    pageHost->SetPage(MakeAnimatedPage(L"First", D2D1::RectF(20.0f, 52.0f, 116.0f, 88.0f), L"hero"));
    pageHost->SetPage(MakeAnimatedPage(L"Second", D2D1::RectF(184.0f, 120.0f, 296.0f, 156.0f), L"hero"), L"hero");

    Require(! pageHost->HasActiveTransition(), "reduced-motion page host snaps directly to the replacement page");
    const PageHostDebugState state = pageHost->DebugGetTransitionState(0u);
    Require(! state.active, "reduced-motion page host clears transition state immediately");
    RequireFloatNear(state.incomingOpacity, 1.0f, 0.0001f, "reduced-motion page host leaves the incoming page fully visible");
}

} // namespace

void RunAnimationTests()
{
    DXUI_RUN_TEST(TestFrameRuntimeClockIsMonotonic);
    DXUI_RUN_TEST(TestFrameRuntimeClampsLargeDelta);
    DXUI_RUN_TEST(TestAnimationDispatcherSchedulerPolicyUses120HzTargetAndClampsHitches);
    DXUI_RUN_TEST(TestAnimationDispatcherActiveSubscribersReceiveMonotonicHighResolutionTicks);
    DXUI_RUN_TEST(TestAnimationDispatcherInactiveSubscriberStopsContinuousTicks);
    DXUI_RUN_TEST(TestFrameRuntimeElapsedUsHandlesLargeQpcDelta);
    DXUI_RUN_TEST(TestFrameRuntimeElapsedUsHandlesWideCrossSignDelta);
    DXUI_RUN_TEST(TestFrameRuntimeReducedMotionPolicy);
    DXUI_RUN_TEST(TestButtonHoverAnimationRequestsTicksUntilSettled);
    DXUI_RUN_TEST(TestButtonReducedMotionSnapsInteractionAnimation);
    DXUI_RUN_TEST(TestSliderHoverAndPressAnimationRequestsTicksUntilSettled);
    DXUI_RUN_TEST(TestSliderTouchDragShowsTouchHaloUntilItEnds);
    DXUI_RUN_TEST(TestSliderTouchHaloStaysInsideItsPaintBoundsAndBehindOverlays);
    DXUI_RUN_TEST(TestSliderReducedMotionSnapsInteractionAndValueAnimation);
    DXUI_RUN_TEST(TestSliderKeyboardStepsAnimateDisplayedThumb);
    DXUI_RUN_TEST(TestTextFieldTickTracksFocusedCaretAnimation);
    DXUI_RUN_TEST(TestEditableComboBoxTickTracksFocusedCaretAnimation);
    DXUI_RUN_TEST(TestPanelOverlayPaintsComboPopupAfterLaterSiblingContent);
    DXUI_RUN_TEST(TestPopupLayerPaintsOnlyInOverlayPass);
    DXUI_RUN_TEST(TestPanelTickPropagatesAnimatedGrid);
    DXUI_RUN_TEST(TestGridTickReturnsFalseForTextOnlyCells);
    DXUI_RUN_TEST(TestGridTickReturnsTrueForHeaderBusyIndicator);
    DXUI_RUN_TEST(TestGridTickReturnsTrueForSortGlyphTransition);
    DXUI_RUN_TEST(TestReducedMotionSuppressesSortGlyphTransition);
    DXUI_RUN_TEST(TestGridScrollbarFeedbackFollowsHoverAndDragState);
    DXUI_RUN_TEST(TestGridScrollbarReducedMotionSnapsFeedback);
    DXUI_RUN_TEST(TestGridTickReturnsTrueForIndeterminateMarquee);
    DXUI_RUN_TEST(TestGridTickReturnsFalseForDeterminateProgress);
    DXUI_RUN_TEST(TestGridTickReturnsFalseForSpinnerWhenReducedMotionEnabled);
    DXUI_RUN_TEST(TestGridTickReturnsFalseForHeaderBusyIndicatorWhenReducedMotionEnabled);
    DXUI_RUN_TEST(TestGridTickReturnsFalseForIndeterminateMarqueeWhenReducedMotionEnabled);
    DXUI_RUN_TEST(TestTextFieldCaretTickIgnoresReducedMotion);
    DXUI_RUN_TEST(TestEditableComboBoxCaretTickIgnoresReducedMotion);
    DXUI_RUN_TEST(TestEasingCurvesStayBoundedAndMonotonic);
    DXUI_RUN_TEST(TestPageHostPageTransitionAnimatesAndSettles);
    DXUI_RUN_TEST(TestPageHostConnectedAnimationInterpolatesSharedElementRect);
    DXUI_RUN_TEST(TestPageHostConnectedOverlayAnimationEmitsCompositionGateMetrics);
    DXUI_RUN_TEST(TestPageHostReducedMotionSnapsTransition);
}
