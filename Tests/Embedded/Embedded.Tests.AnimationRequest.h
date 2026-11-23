#pragma once
// W3.5, the embedded animation request (API revision 6; Specs/Rendering/Rendering_EmbeddedD3D11.md, "Animation request"):
// GetAnimationRequest and AdvanceAnimation(nowTickMs) report None, Continuous or a Deadline on the application's own clock;
// the clock rules (baseline after a pause, no going back, saturation) are unchanged; a caret phase and a tooltip deadline
// are neither lost nor fired early or all at once across a pause, a hidden period and a clock that jumps by hours; the
// view wakes the application when it needs it sooner than the request it last reported; one RefreshUserSettings call
// re-reads every user setting the host keeps. Included once, from Embedded.Tests.Embedded.cpp, after Check, Hr,
// AnimationRequested and GraphicsFixture. The fixture gates (application wakes on FX1, FX13 and FX14) are in
// Embedded.Tests.PerformanceFixtures.h.
#include "Support/WorkCounters.h"

#include <functional>
#include <iostream>

namespace EmbeddedAnimationRequestTests
{
using Kind = DxUi::EmbeddedAnimationRequest::Kind;

// A control whose Tick counts and runs a test's function.
class RequestProbe final : public DxUi::Control
{
public:
    RequestProbe()                               = default;
    RequestProbe(const RequestProbe&)            = delete;
    RequestProbe& operator=(const RequestProbe&) = delete;
    RequestProbe(RequestProbe&&)                 = delete;
    RequestProbe& operator=(RequestProbe&&)      = delete;
    ~RequestProbe() override                     = default;
    void Paint(DxUi::ControlHost&) const override
    {
    }
    bool Tick(DxUi::ControlHost& host, uint64_t nowTickMs) override
    {
        ++ticks;
        return onTick ? onTick(host, nowTickMs) : false;
    }
    size_t ticks = 0u;
    std::function<bool(DxUi::ControlHost&, uint64_t)> onTick;
};

struct Wakes
{
    size_t calls = 0u;
    static void Count(void* context) noexcept
    {
        ++static_cast<Wakes*>(context)->calls;
    }
};

[[nodiscard]] inline bool IsDeadlineAt(const DxUi::EmbeddedAnimationRequest& request, uint64_t dueTickMs) noexcept
{
    return request.kind == Kind::Deadline && request.dueTickMs == dueTickMs;
}

// A view on WARP with motion on, a root panel and the given callbacks; the discovery tick of the new root is taken.
struct View
{
    GraphicsFixture gpu;
    Wakes wakes;
    RunningTestClock clock;
    DxUi::EmbeddedHost view;
    DxUi::Panel* root            = nullptr;
    View(const View&)            = delete;
    View& operator=(const View&) = delete;
    View(View&&)                 = delete;
    View& operator=(View&&)      = delete;
    ~View()                      = default;
    explicit View(uint64_t discoveryTickMs)
    {
        Hr(gpu.Create(true), "animation request WARP device");
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "animation request graphics");
        // The application's clock: 0 until a test sets it, which keeps every stamp at the last advance; a test about stamps
        // between advances moves clock.nowMs as its application's time passes.
        clock.requestPreparation = &Wakes::Count;
        clock.requestContext     = &wakes;
        Hr(view.Attach(std::move(graphics), clock.Callbacks()), "attach the animation request view");
        auto theme          = DxUi::MakeDefaultThemePalette(true);
        theme.reducedMotion = false;
        view.Controls().SetTheme(theme);
        auto panel = std::make_unique<DxUi::Panel>();
        root       = panel.get();
        view.Controls().SetRoot(std::move(panel));
        Hr(view.Prepare(320u, 200u), "prepare the animation request view");
        Check(view.GetAnimationRequest().kind == Kind::Continuous, "a new root asks for one frame: its discovery tick");
        Check(! AnimationRequested(view.AdvanceAnimation(discoveryTickMs)), "the discovery tick leaves nothing requested");
    }
};

// The request is on the application's clock: the first advance after a pause only takes the baseline (the request reports
// it due at once, 0), a deadline is that baseline plus what the view's timeline still lacks, an advance before it ticks and
// walks nothing and returns the same deadline, an advance at it ticks only the due control, a tick that goes back adds
// nothing, and the clock saturates instead of wrapping. Hidden and zero-sized views request nothing.
inline void TestRequestIsOnTheApplicationClock()
{
    View scene(1'000u);
    DxUi::EmbeddedHost& view = scene.view;
    DxUi::ControlHost& host  = view.Controls();
    auto* probe              = scene.root->AddChild<RequestProbe>();
    auto* idle               = scene.root->AddChild<RequestProbe>();
    Hr(view.Prepare(320u, 200u), "prepare the probes");
    static_cast<void>(view.AdvanceAnimation(1'001u)); // a changed root's discovery tick, if any
    Check(! AnimationRequested(view.GetAnimationRequest()), "nothing is requested");
    probe->ticks = idle->ticks = 0u;

    const size_t wakesBefore = scene.wakes.calls;
    host.RequestAnimationAt(*probe, host.GetAnimationTickMs() + 300u);
    Check(scene.wakes.calls == wakesBefore + 1u, "a deadline after nothing wakes the application once");
    Check(IsDeadlineAt(view.GetAnimationRequest(), 0u), "after a pause the request is due at once: the next advance takes the baseline");
    constexpr uint64_t kEpoch = 7'000'000'000u;
    Check(IsDeadlineAt(view.AdvanceAnimation(kEpoch), kEpoch + 300u) && probe->ticks == 0u,
          "the baseline advance ticks nothing and reports the deadline on the application's clock");
    Check(IsDeadlineAt(view.GetAnimationRequest(), kEpoch + 300u), "reading the request again reports the same deadline");
    {
        const DxUi::Detail::WorkCounterScope early;
        Check(IsDeadlineAt(view.AdvanceAnimation(kEpoch + 299u), kEpoch + 300u), "an early advance returns the same deadline");
        const auto delta = early.Delta();
        Check(probe->ticks == 0u && delta.frame.tickCalls == 0u && delta.frame.treeVisitsTick == 0u, "an advance before the deadline ticks and walks nothing");
    }
    {
        const DxUi::Detail::WorkCounterScope due;
        Check(! AnimationRequested(view.AdvanceAnimation(kEpoch + 300u)), "the deadline is consumed");
        const auto delta = due.Delta();
        Check(probe->ticks == 1u && idle->ticks == 0u && delta.frame.tickCalls == 1u && delta.frame.tickRequesters == 1u && delta.frame.treeVisitsTick == 0u,
              "the advance at the deadline ticks the due control alone, with no tree walk");
    }

    // A tick that goes back adds nothing: the deadline is counted from it, never fired by the jump back. A far deadline of
    // the idle probe keeps the clock running, so the deadline below is armed without a pause.
    host.RequestAnimationAt(*idle, host.GetAnimationTickMs() + 1'000'000u);
    Check(IsDeadlineAt(view.GetAnimationRequest(), 0u), "a new request after nothing is due at once, for the baseline");
    static_cast<void>(view.AdvanceAnimation(kEpoch + 1'000u));
    host.RequestAnimationAt(*probe, host.GetAnimationTickMs() + 300u);
    Check(IsDeadlineAt(view.GetAnimationRequest(), kEpoch + 1'300u), "a deadline armed while the clock runs is on the running clock");
    probe->ticks = 0u;
    Check(IsDeadlineAt(view.AdvanceAnimation(kEpoch + 500u), kEpoch + 800u) && probe->ticks == 0u,
          "a tick that goes back adds nothing and moves the deadline with it");
    Check(IsDeadlineAt(view.AdvanceAnimation(kEpoch + 799u), kEpoch + 800u) && probe->ticks == 0u, "and nothing fires early");
    static_cast<void>(view.AdvanceAnimation(kEpoch + 800u));
    Check(probe->ticks == 1u && idle->ticks == 0u, "the deadline fires 300 ms of forward time after it was armed");

    // Saturation: an application clock near the top of its range saturates instead of wrapping, so nothing fires early.
    constexpr uint64_t kTop = UINT64_MAX - 100u;
    static_cast<void>(view.AdvanceAnimation(kTop)); // a jump that makes the idle probe's far deadline due
    Check(idle->ticks == 1u && view.GetAnimationRequest().kind == Kind::None, "the jump fires the far deadline once");
    host.RequestAnimationAt(*idle, host.GetAnimationTickMs() + 1'000'000u);
    static_cast<void>(view.AdvanceAnimation(kTop)); // the baseline, at the top of the application's clock
    host.RequestAnimationAt(*probe, host.GetAnimationTickMs() + 300u);
    probe->ticks = 0u;
    // The application's clock can reach only 100 ms more: the far deadline was already unreachable (reported None, a pause),
    // so the new one first asks for its baseline advance; after it, the deadline 300 ms ahead is unreachable and is None.
    static_cast<void>(view.AdvanceAnimation(kTop));
    Check(view.GetAnimationRequest().kind == Kind::None, "a deadline past the end of the application's clock is not reported");
    Check(! AnimationRequested(view.AdvanceAnimation(UINT64_MAX)) && probe->ticks == 0u, "the saturated clock never wraps into firing early");
    host.CancelAnimation(*probe);
    host.CancelAnimation(*idle);

    // Hidden and zero-sized views request nothing, and an advance then does nothing.
    host.RequestAnimation(*probe);
    view.SetVisible(false);
    Check(view.GetAnimationRequest().kind == Kind::None, "a hidden view requests nothing");
    {
        const DxUi::Detail::WorkCounterScope hidden;
        Check(! AnimationRequested(view.AdvanceAnimation(10u)) && hidden.Delta().wakes.embeddedAdvance == 0u, "an advance of a hidden view does nothing");
    }
    view.SetVisible(true);
    Check(view.GetAnimationRequest().kind == Kind::Continuous, "showing it requests again");
    Check(view.Prepare(0u, 0u) == S_FALSE && view.GetAnimationRequest().kind == Kind::None, "a zero-sized view requests nothing");
    host.CancelAnimation(*probe);
    std::cout << "PASS embedded animation request on the application clock\n";
}

// The view wakes the application (its requestPreparation callback) when the schedule needs it sooner than the request it
// last reported: something after nothing, every frame after a deadline, an earlier deadline. A later or equal need does
// not wake it, and a request raised while a wake runs is returned by that advance instead.
inline void TestEarlierNeedWakesTheApplication()
{
    View scene(1'000u);
    DxUi::EmbeddedHost& view = scene.view;
    DxUi::ControlHost& host  = view.Controls();
    auto* first              = scene.root->AddChild<RequestProbe>();
    auto* second             = scene.root->AddChild<RequestProbe>();
    Hr(view.Prepare(320u, 200u), "prepare the probes");
    static_cast<void>(view.AdvanceAnimation(1'001u));
    host.RequestAnimationAt(*first, host.GetAnimationTickMs() + 500u);
    static_cast<void>(view.AdvanceAnimation(2'000u)); // the baseline
    Check(IsDeadlineAt(view.GetAnimationRequest(), 2'500u), "the application sleeps until 2,500");
    // A wake is one requestPreparation call, coalesced until the next Prepare: prepare before each count.
    Hr(view.Prepare(320u, 200u), "prepare before counting wakes");
    size_t wakes = scene.wakes.calls;
    host.RequestAnimationAt(*second, host.GetAnimationTickMs() + 800u);
    Check(scene.wakes.calls == wakes && IsDeadlineAt(view.GetAnimationRequest(), 2'500u), "a later deadline does not wake the application");
    Hr(view.Prepare(320u, 200u), "prepare after the later deadline");
    host.RequestAnimationAt(*second, host.GetAnimationTickMs() + 200u);
    Check(scene.wakes.calls == wakes + 1u && IsDeadlineAt(view.GetAnimationRequest(), 2'200u), "an earlier deadline wakes it once");
    Hr(view.Prepare(320u, 200u), "prepare after the earlier deadline");
    wakes = scene.wakes.calls;
    host.RequestAnimation(*first);
    Check(scene.wakes.calls == wakes + 1u && view.GetAnimationRequest().kind == Kind::Continuous, "every frame after a deadline wakes it once");
    Hr(view.Prepare(320u, 200u), "prepare after the frame request");
    wakes = scene.wakes.calls;
    host.RequestAnimation(*second);
    Check(scene.wakes.calls == wakes, "a second every-frame request does not wake it again");
    // Raised inside a wake: the advance returns it, and no callback is needed.
    first->onTick = [second](DxUi::ControlHost& owner, uint64_t nowTickMs)
    {
        owner.RequestAnimationAt(*second, nowTickMs + 50u);
        return false;
    };
    second->onTick = nullptr;
    host.CancelAnimation(*second);
    wakes                                         = scene.wakes.calls;
    const DxUi::EmbeddedAnimationRequest returned = view.AdvanceAnimation(2'016u);
    Check(scene.wakes.calls == wakes && IsDeadlineAt(returned, 2'066u), "a request a Tick raises comes back from that advance");
    std::cout << "PASS embedded animation request wakes the application only when needed sooner\n";
}

// One text column whose rows each carry their own tooltip.
class TooltipRowsModel final : public DxUi::IGridModel
{
public:
    std::vector<std::wstring> tooltips;
    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return tooltips.size();
    }
    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 1u;
    }
    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t /*columnIndex*/) const override
    {
        DxUi::GridColumnDesc column;
        column.id       = L"name";
        column.title    = L"Name";
        column.widthDip = 240.0f;
        return column;
    }
    void GetCellData(size_t rowIndex, size_t /*columnIndex*/, DxUi::GridCellData& outCell) const override
    {
        outCell.kind        = DxUi::GridCellKind::Text;
        outCell.text        = L"Row";
        outCell.tooltipText = tooltips.at(rowIndex);
    }
    [[nodiscard]] uint64_t GetStableRowId(size_t rowIndex) const noexcept override
    {
        return std::hash<std::wstring>{}(tooltips.at(rowIndex));
    }
    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        for (size_t row = 0u; row < tooltips.size(); ++row)
            if (GetStableRowId(row) == rowId)
                return row;
        return std::nullopt;
    }
};

// W3.3's tooltip mark seen through the request (W3.5): a Grid row changed under the resting pointer marks the shown tooltip
// stale, which makes the schedule due at once. A sleeping application learns it through requestPreparation, the request
// becomes a deadline already reached, and one advance shows the tooltip of the row now under the pointer.
inline void TestTooltipMarkWakesTheApplication()
{
    View scene(1'000u);
    DxUi::EmbeddedHost& view = scene.view;
    DxUi::ControlHost& host  = view.Controls();
    auto* grid               = scene.root->AddChild<DxUi::Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 140.0f));
    TooltipRowsModel model;
    model.tooltips = {L"First row help", L"Second row help"};
    grid->SetModel(&model);
    Hr(view.Prepare(320u, 200u), "prepare the tooltip grid");
    uint64_t now = 5'000u;
    for (size_t turn = 0u; turn < 8u && AnimationRequested(view.GetAnimationRequest()); ++turn)
        static_cast<void>(view.AdvanceAnimation(now += 16u));
    Hr(view.Prepare(320u, 200u), "settle the tooltip grid");
    const DxUi::GridCellLayoutMetrics metrics = grid->GetCellLayoutMetrics(host, 0u, 0u);
    static_cast<void>(view.DispatchPointer(
        {DxUi::PointerAction::Move, (metrics.cellRect.left + metrics.cellRect.right) * 0.5f, (metrics.cellRect.top + metrics.cellRect.bottom) * 0.5f}));
    Check(host.HasTooltip() && host.GetTooltipText() == L"First row help", "the hovered Grid cell shows its tooltip");
    Hr(view.Prepare(320u, 200u), "prepare after the hover");
    for (size_t turn = 0u; turn < 8u && AnimationRequested(view.GetAnimationRequest()); ++turn)
        static_cast<void>(view.AdvanceAnimation(now += 16u));
    Hr(view.Prepare(320u, 200u), "prepare before the row change");
    const DxUi::EmbeddedAnimationRequest before = view.GetAnimationRequest();
    const size_t wakes                          = scene.wakes.calls;

    model.tooltips = {L"Second row help"};
    grid->NotifyDataChanged();
    const DxUi::EmbeddedAnimationRequest marked = view.GetAnimationRequest();
    Check(scene.wakes.calls > wakes, "the marked tooltip wakes a sleeping application through requestPreparation");
    Check(marked.kind == Kind::Deadline && marked.dueTickMs <= now, "the request turns into a deadline already reached");
    Check(before.kind == Kind::None || (before.kind == Kind::Deadline && before.dueTickMs > marked.dueTickMs),
          "the marked request is sooner than the one reported before the change");
    {
        const DxUi::Detail::WorkCounterScope resolve;
        static_cast<void>(view.AdvanceAnimation(now += 16u));
        Check(resolve.Delta().frame.tooltipTargetValidations == 1u && host.GetTooltipText() == L"Second row help",
              "one advance resolves the row now under the pointer");
    }
    grid->SetModel(nullptr);
    std::cout << "PASS a Grid row change under a shown tooltip wakes a sleeping application\n";
}

// The mark alone wakes the application: SetTooltipText changes nothing a preparation draws, so without a tooltip it calls
// no requestPreparation (the control case), and with the target's tooltip pending the mark is the only reason one comes.
inline void TestTooltipMarkAloneWakesTheApplication()
{
    View scene(1'000u);
    DxUi::EmbeddedHost& view = scene.view;
    DxUi::ControlHost& host  = view.Controls();
    host.DebugSetTooltipHoverTimeForTest(400u);
    auto* label = scene.root->AddChild<DxUi::Label>(L"Help");
    label->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 100.0f));
    label->SetTooltipText(L"First help");
    Hr(view.Prepare(320u, 200u), "prepare the tooltip label");
    uint64_t now = 5'000u;
    for (size_t turn = 0u; turn < 8u && AnimationRequested(view.GetAnimationRequest()); ++turn)
        static_cast<void>(view.AdvanceAnimation(now += 16u));
    Hr(view.Prepare(320u, 200u), "settle the tooltip label");

    // Control case: no tooltip pending or shown, so the change marks nothing and wakes nothing.
    size_t wakes = scene.wakes.calls;
    label->SetTooltipText(L"Second help");
    Check(scene.wakes.calls == wakes && ! view.NeedsPreparation() && view.GetAnimationRequest().kind == Kind::None,
          "with no tooltip, SetTooltipText neither wakes the application nor dirties the view");

    static_cast<void>(view.DispatchPointer({DxUi::PointerAction::Move, 50.0f, 50.0f}));
    Check(host.DebugGetPendingTooltipText() == L"Second help", "the resting pointer schedules the label's tooltip");
    static_cast<void>(view.AdvanceAnimation(now += 16u)); // the baseline after the pause
    const DxUi::EmbeddedAnimationRequest pending = view.GetAnimationRequest();
    Check(pending.kind == Kind::Deadline && pending.dueTickMs == now + 400u, "the application sleeps until the show");
    Hr(view.Prepare(320u, 200u), "prepare before the mark");
    wakes = scene.wakes.calls;
    label->SetTooltipText(L"Third help");
    const DxUi::EmbeddedAnimationRequest marked = view.GetAnimationRequest();
    Check(scene.wakes.calls == wakes + 1u, "the mark alone wakes the sleeping application once");
    Check(marked.kind == Kind::Deadline && marked.dueTickMs <= now, "the request turns into a deadline already reached");
    {
        const DxUi::Detail::WorkCounterScope resolve;
        static_cast<void>(view.AdvanceAnimation(now += 16u));
        Check(resolve.Delta().frame.tooltipTargetValidations == 1u && host.DebugGetPendingTooltipText() == L"Third help" && ! host.HasTooltip(),
              "one advance resolves the target: the new text waits for its full delay");
    }
    host.DebugSetTooltipHoverTimeForTest(std::nullopt);
    std::cout << "PASS a tooltip mark alone wakes a sleeping application\n";
}

// A tooltip waiting to show and a caret phase across a pause (zero size), a hidden period and a clock that jumps forward by
// hours: neither is lost, fired early, or fired more than once. A hidden view drops a pending show and restarts a focused
// caret on show, as the native host does; a held hide keeps its remaining time on the view's timeline.
inline void TestDeadlinesSurvivePausesHidingAndClockJumps()
{
    using DxUi::Detail::WorkCounterScope;
    constexpr uint64_t kHourMs = 3'600'000u;
    View scene(1'000u);
    DxUi::EmbeddedHost& view = scene.view;
    DxUi::ControlHost& host  = view.Controls();
    host.DebugSetTooltipHoverTimeForTest(400u);
    auto* field = scene.root->AddChild<DxUi::TextField>(L"caret");
    field->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
    Hr(view.Prepare(320u, 200u), "prepare the caret field");
    static_cast<void>(view.AdvanceAnimation(1'001u));

    // A tooltip waiting to show across a pause (zero size for an hour of application time).
    Check(host.SetTooltipDelayed(L"Pause", D2D1::Point2F(20.0f, 20.0f)), "a tooltip waits to show");
    uint64_t now = 10'000u;
    Check(IsDeadlineAt(view.AdvanceAnimation(now), now + 400u), "the baseline advance reports the show 400 ms later");
    now += 100u;
    Check(IsDeadlineAt(view.AdvanceAnimation(now), now + 300u) && ! host.HasTooltip(), "100 ms in, 300 ms remain");
    Check(view.Prepare(0u, 0u) == S_FALSE && view.GetAnimationRequest().kind == Kind::None, "a zero-sized view pauses");
    now += kHourMs;
    Hr(view.Prepare(320u, 200u), "size the view again");
    Check(IsDeadlineAt(view.GetAnimationRequest(), 0u), "after the pause the next advance takes the baseline");
    {
        const WorkCounterScope pause;
        Check(IsDeadlineAt(view.AdvanceAnimation(now), now + 300u) && ! host.HasTooltip(), "the paused hour is not counted: 300 ms still remain");
        Check(IsDeadlineAt(view.AdvanceAnimation(now + 299u), now + 300u) && ! host.HasTooltip(), "the show is not early");
        static_cast<void>(view.AdvanceAnimation(now += 300u));
        Check(host.HasTooltip() && pause.Delta().frame.tooltipDeadlineTicks == 1u, "the show fires once, on time");
    }
    Check(IsDeadlineAt(view.GetAnimationRequest(), now + 5'000u), "the hide is 5 s after the show");

    // The shown tooltip's hide across a hidden period: held, with its remaining time on the view's timeline.
    static_cast<void>(view.AdvanceAnimation(now += 2'000u));
    view.SetVisible(false);
    Check(view.GetAnimationRequest().kind == Kind::None, "a hidden view requests nothing");
    now += kHourMs;
    view.SetVisible(true);
    Hr(view.Prepare(320u, 200u), "prepare the shown view");
    Check(IsDeadlineAt(view.GetAnimationRequest(), 0u) && host.HasTooltip(), "the held hide waits for the baseline");
    {
        const WorkCounterScope hidden;
        Check(IsDeadlineAt(view.AdvanceAnimation(now), now + 3'000u) && host.HasTooltip(), "the hidden hour is not counted: 3 s of the 5 s remain");
        static_cast<void>(view.AdvanceAnimation(now + 2'999u));
        Check(host.HasTooltip(), "the hide is not early");
        static_cast<void>(view.AdvanceAnimation(now += 3'000u));
        Check(! host.HasTooltip() && hidden.Delta().frame.tooltipDeadlineTicks == 1u, "the hide fires once, on time");
    }
    Check(view.GetAnimationRequest().kind == Kind::None, "nothing is left");

    // A tooltip waiting to show across a hidden period is dropped, as on the native host: its pointer is not known on show.
    Check(host.SetTooltipDelayed(L"Hidden", D2D1::Point2F(20.0f, 20.0f)), "another tooltip waits to show");
    {
        const WorkCounterScope dropped;
        view.SetVisible(false);
        view.SetVisible(true);
        Check(view.GetAnimationRequest().kind == Kind::None && dropped.Delta().frame.tooltipShowsDropped == 1u, "hiding drops the pending show");
    }
    Hr(view.Prepare(320u, 200u), "prepare after the dropped show");

    // A tooltip waiting to show across a clock that jumps forward by hours: it shows once, late, and the hide waits 5 s more.
    Check(host.SetTooltipDelayed(L"Jump", D2D1::Point2F(20.0f, 20.0f)), "a third tooltip waits to show");
    static_cast<void>(view.AdvanceAnimation(now += 10u)); // baseline
    {
        const WorkCounterScope jump;
        now += 3u * kHourMs;
        Check(IsDeadlineAt(view.AdvanceAnimation(now), now + 5'000u), "after a three-hour jump the hide is still 5 s away");
        Check(host.HasTooltip() && jump.Delta().frame.tooltipDeadlineTicks == 1u, "the show fires once, not lost, and the hide not with it");
    }
    host.DebugSetTooltipHoverTimeForTest(std::nullopt);
    static_cast<void>(host.ClearTooltip());
    Hr(view.Prepare(320u, 200u), "prepare after the tooltips");

    // A caret phase that never times out (timeout 0) across a hidden period and a clock jump.
    constexpr UINT kBlinkMs = 530u;
    host.DebugSetCaretBlinkSettingsForTest(DxUi::ControlHost::CaretBlinkSettingsForTest{kBlinkMs, 0u});
    host.SetFocusControl(field, false);
    Check(IsDeadlineAt(view.GetAnimationRequest(), 0u) || AnimationRequested(view.GetAnimationRequest()), "the focused caret requests its first flip");
    now += 10u;
    const DxUi::EmbeddedAnimationRequest first = view.AdvanceAnimation(now);
    Check(IsDeadlineAt(first, now + kBlinkMs) && field->DebugIsCaretVisible(), "the caret's first flip is one period after its start");
    static_cast<void>(view.AdvanceAnimation(now += kBlinkMs));
    Check(! field->DebugIsCaretVisible() && IsDeadlineAt(view.GetAnimationRequest(), now + kBlinkMs), "it blinks off and arms the next flip");
    static_cast<void>(view.AdvanceAnimation(now += 200u));
    view.SetVisible(false);
    now += kHourMs;
    view.SetVisible(true);
    Hr(view.Prepare(320u, 200u), "prepare the shown caret");
    Check(field->DebugIsCaretVisible() && IsDeadlineAt(view.GetAnimationRequest(), 0u), "showing the view shows the caret at once; its phase restarts");
    Check(IsDeadlineAt(view.AdvanceAnimation(now), now + kBlinkMs), "the restarted phase flips one period after the baseline, not early");
    {
        const WorkCounterScope jump;
        now += 3u * kHourMs + 17u;
        const DxUi::EmbeddedAnimationRequest next = view.AdvanceAnimation(now);
        const auto delta                          = jump.Delta();
        Check(delta.frame.tickCalls == 1u, "a three-hour jump ticks the caret once, not once per missed flip");
        Check(next.kind == Kind::Deadline && next.dueTickMs > now && next.dueTickMs <= now + kBlinkMs, "the next flip is within one period, in phase");
    }
    // With the 5 s caret timeout, a jump past it leaves the caret shown and steady: one tick, then nothing armed.
    host.DebugSetCaretBlinkSettingsForTest(DxUi::ControlHost::CaretBlinkSettingsForTest{kBlinkMs, 5'000u});
    host.RefreshUserSettings();
    {
        const WorkCounterScope timeout;
        static_cast<void>(view.AdvanceAnimation(now += 2u * kHourMs));
        Check(timeout.Delta().frame.tickCalls == 1u && field->DebugIsCaretVisible() && view.GetAnimationRequest().kind == Kind::None,
              "past the caret timeout one tick leaves the caret shown and nothing requested");
    }
    host.DebugSetCaretBlinkSettingsForTest(std::nullopt);
    std::cout << "PASS embedded deadlines across a pause, a hidden period and a clock jump\n";
}

// One RefreshUserSettings call re-reads both settings an embedded view keeps (the caret blink time and timeout, the mouse
// hover time), so an application that forwards WM_SETTINGCHANGE cannot refresh one and leave the other stale.
inline void TestOneCallRefreshesEveryUserSetting()
{
    View scene(1'000u);
    DxUi::EmbeddedHost& view = scene.view;
    DxUi::ControlHost& host  = view.Controls();
    auto* field              = scene.root->AddChild<DxUi::TextField>(L"settings");
    field->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
    Hr(view.Prepare(320u, 200u), "prepare the settings field");
    static_cast<void>(view.AdvanceAnimation(1'001u));
    host.DebugSetCaretBlinkSettingsForTest(DxUi::ControlHost::CaretBlinkSettingsForTest{530u, 0u});
    host.DebugSetTooltipHoverTimeForTest(300u);
    host.SetFocusControl(field, false);
    static_cast<void>(view.AdvanceAnimation(2'000u)); // baseline
    Check(IsDeadlineAt(view.GetAnimationRequest(), 2'530u) && host.DebugGetTooltipShowDelayMsForTest() == 300u, "the settings are read once");
    host.DebugSetCaretBlinkSettingsForTest(DxUi::ControlHost::CaretBlinkSettingsForTest{200u, 0u});
    host.DebugSetTooltipHoverTimeForTest(900u);
    Check(IsDeadlineAt(view.GetAnimationRequest(), 2'530u) && host.DebugGetTooltipShowDelayMsForTest() == 300u, "and kept until a settings change is relayed");
    host.RefreshUserSettings();
    Check(IsDeadlineAt(view.GetAnimationRequest(), 2'200u) && host.DebugGetTooltipShowDelayMsForTest() == 900u,
          "one call applies the new blink time and the new hover time");
    host.DebugSetCaretBlinkSettingsForTest(std::nullopt);
    host.DebugSetTooltipHoverTimeForTest(std::nullopt);
    std::cout << "PASS one RefreshUserSettings call refreshes every user setting of an embedded view\n";
}

// ---- W3.5 review fixes -------------------------------------------------------------------------------------------------

// The application's clock reaches the view between advances (review F1): a stamp taken from input is taken at the
// application's current time, not at the last advance. The three stamps the review named, in three situations: while a
// deadline is pending and the application sleeps until it, after a hidden period, and after a pause with nothing requested.
enum class StampSituation : uint8_t
{
    Sleeping,
    AfterHidden,
    AfterPause,
};
[[nodiscard]] inline const char* SituationName(StampSituation situation) noexcept
{
    switch (situation)
    {
        case StampSituation::Sleeping: return "while sleeping until a deadline";
        case StampSituation::AfterHidden: return "after a hidden period";
        case StampSituation::AfterPause: return "after a pause with nothing requested";
    }
    return "?";
}

// A scene whose application clock the test moves: a TextField (caret 530 ms, never timing out), a Label with a tooltip
// (400 ms) and a Button (hover transition). `Arrive` brings it to the situation at application time `atMs`, with the caret
// unfocused when the situation is a pause.
struct ClockScene
{
    View scene{1'000u};
    DxUi::TextField* field = nullptr;
    DxUi::Label* label     = nullptr;
    DxUi::Button* button   = nullptr;
    ClockScene()
    {
        DxUi::ControlHost& host = scene.view.Controls();
        host.DebugSetTooltipHoverTimeForTest(400u);
        host.DebugSetCaretBlinkSettingsForTest(DxUi::ControlHost::CaretBlinkSettingsForTest{530u, 0u});
        field = scene.root->AddChild<DxUi::TextField>(L"caret");
        field->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
        label = scene.root->AddChild<DxUi::Label>(L"Help");
        label->SetBounds(D2D1::RectF(8.0f, 60.0f, 150.0f, 120.0f));
        label->SetTooltipText(L"Help text");
        button = scene.root->AddChild<DxUi::Button>(L"Go");
        button->SetBounds(D2D1::RectF(160.0f, 60.0f, 300.0f, 120.0f));
        Hr(scene.view.Prepare(320u, 200u), "prepare the clock scene");
        for (uint64_t at = 1'016u; at < 1'500u && AnimationRequested(scene.view.GetAnimationRequest()); at += 16u)
            static_cast<void>(scene.view.AdvanceAnimation(at));
    }
    ~ClockScene()
    {
        scene.view.Controls().DebugSetTooltipHoverTimeForTest(std::nullopt);
        scene.view.Controls().DebugSetCaretBlinkSettingsForTest(std::nullopt);
    }
    ClockScene(const ClockScene&)            = delete;
    ClockScene& operator=(const ClockScene&) = delete;
    ClockScene(ClockScene&&)                 = delete;
    ClockScene& operator=(ClockScene&&)      = delete;
    // The application sets its clock and advances once, as its loop does at `atMs`.
    DxUi::EmbeddedAnimationRequest AdvanceAt(uint64_t atMs)
    {
        scene.clock.nowMs = atMs;
        return scene.view.AdvanceAnimation(atMs);
    }
    // Leaves the scene at application time `atMs` in the situation: the caret blinking and the application asleep until its
    // next flip; or shown again after an hour hidden, the application having advanced once at once; or idle for an hour with
    // nothing requested.
    void Arrive(StampSituation situation, uint64_t atMs)
    {
        DxUi::EmbeddedHost& view = scene.view;
        scene.clock.nowMs        = 10'000u;
        view.Controls().SetFocusControl(field, false);
        DxUi::EmbeddedAnimationRequest request = AdvanceAt(10'000u); // the baseline after the focus
        Check(IsDeadlineAt(request, 10'530u), "the caret's first flip is 530 ms after the focus");
        request = AdvanceAt(10'530u);
        Check(IsDeadlineAt(request, 11'060u) && ! field->DebugIsCaretVisible(), "the caret is off until 11,060");
        switch (situation)
        {
            case StampSituation::Sleeping: Check(atMs < 11'060u, "the stamp falls while the application sleeps until the caret's next flip"); break;
            case StampSituation::AfterHidden:
                view.SetVisible(false);
                scene.clock.nowMs = atMs - 500u;
                view.SetVisible(true);
                Hr(view.Prepare(320u, 200u), "prepare the shown view");
                request = view.GetAnimationRequest();
                Check(IsDeadlineAt(request, 0u), "after the hidden hour the application advances once at once");
                request = AdvanceAt(atMs - 500u);
                Check(IsDeadlineAt(request, atMs - 500u + 530u), "the shown caret restarts its phase at the baseline");
                break;
            case StampSituation::AfterPause:
                view.Controls().SetFocusControl(nullptr, false);
                Check(view.GetAnimationRequest().kind == DxUi::EmbeddedAnimationRequest::Kind::None, "with no focus nothing is requested");
                break;
        }
        Hr(view.Prepare(320u, 200u), "prepare before the stamp");
        scene.clock.nowMs = atMs;
    }
};

inline void TestStampsTakeTheApplicationsTime()
{
    using Kind = DxUi::EmbeddedAnimationRequest::Kind;
    for (const StampSituation situation : {StampSituation::Sleeping, StampSituation::AfterHidden, StampSituation::AfterPause})
    {
        const uint64_t at = situation == StampSituation::Sleeping ? 11'000u : 3'600'000u;
        // S1: the pointer comes to rest on the label: the tooltip shows 400 ms later on the application's clock.
        {
            ClockScene clock;
            clock.Arrive(situation, at);
            DxUi::EmbeddedHost& view = clock.scene.view;
            static_cast<void>(view.DispatchPointer({DxUi::PointerAction::Move, 50.0f, 90.0f}));
            DxUi::EmbeddedAnimationRequest request = view.GetAnimationRequest();
            for (size_t turn = 0u; turn < 8u && request.kind == Kind::Deadline && request.dueTickMs <= at; ++turn)
                request = clock.AdvanceAt(at); // a baseline after a pause is advanced at once, at the stamp's time
            uint64_t now = at;
            while (request.kind == Kind::Deadline && request.dueTickMs < at + 400u && ! view.Controls().HasTooltip())
            {
                now     = request.dueTickMs; // the caret's flips on the way
                request = clock.AdvanceAt(now);
            }
            Check(! view.Controls().HasTooltip() && IsDeadlineAt(request, at + 400u), "S1: the tooltip is due 400 ms after the hover, not before");
            static_cast<void>(clock.AdvanceAt(at + 399u));
            Check(! view.Controls().HasTooltip(), "S1: the tooltip is not early");
            static_cast<void>(clock.AdvanceAt(at + 400u));
            Check(view.Controls().HasTooltip(), "S1: the tooltip shows on time");
        }
        // S2: a character typed restarts the caret: it stays shown a whole blink period after the keystroke.
        {
            ClockScene clock;
            clock.Arrive(situation, at);
            DxUi::EmbeddedHost& view = clock.scene.view;
            if (situation == StampSituation::AfterPause)
                view.Controls().SetFocusControl(clock.field, false); // the focus is the input that ends the pause
            else
                Check(view.DispatchCharacter(L'a'), "S2: the character reaches the field");
            DxUi::EmbeddedAnimationRequest request = view.GetAnimationRequest();
            if (request.kind == Kind::Deadline && request.dueTickMs <= at)
                request = clock.AdvanceAt(at); // a baseline after a pause is advanced at once
            Check(clock.field->DebugIsCaretVisible() && IsDeadlineAt(request, at + 530u), "S2: the caret's next flip is a whole period after the input");
        }
        // A transition started from input runs from the input's time: the hover's first frame is one frame in.
        {
            ClockScene clock;
            clock.Arrive(situation, at);
            DxUi::EmbeddedHost& view = clock.scene.view;
            static_cast<void>(view.DispatchPointer({DxUi::PointerAction::Move, 230.0f, 90.0f}));
            DxUi::EmbeddedAnimationRequest request = view.GetAnimationRequest();
            if (request.kind == Kind::Deadline && request.dueTickMs <= at)
                request = clock.AdvanceAt(at); // a baseline after a pause is advanced at once
            Check(request.kind == Kind::Continuous, "the hover asks for every frame");
            // After a pause the first frame only takes the baseline (progress 0); otherwise it is 16 ms in. Either way two
            // frames are at most 32 ms of the 140 ms transition: the long sleep before the hover is not counted.
            static_cast<void>(clock.AdvanceAt(at + 16u));
            static_cast<void>(clock.AdvanceAt(at + 32u));
            const float progress = clock.button->DebugGetHoverAnimationProgress();
            Check(progress > 0.0f && progress < 0.5f, "the hover transition runs from the input's time, not finished by the long sleep");
        }
        std::cout << "PASS stamps take the application's time " << SituationName(situation) << '\n';
    }
}

// The view remembers the request the application last read (review F2): a cancellation that leaves a later deadline, a
// Deadline after Continuous and a request whose control was destroyed are all re-read, and a need sooner than what the
// application now sleeps on wakes it.
inline void TestTheRecordFollowsWhatTheApplicationRead()
{
    using Kind = DxUi::EmbeddedAnimationRequest::Kind;
    // S3: deadlines at 2,500 and 2,800; 2,500 is cancelled and the application reads 2,800; a new one at 2,600 wakes it.
    {
        View scene(1'000u);
        DxUi::ControlHost& host = scene.view.Controls();
        auto* first             = scene.root->AddChild<RequestProbe>();
        auto* second            = scene.root->AddChild<RequestProbe>();
        Hr(scene.view.Prepare(320u, 200u), "prepare the S3 probes");
        static_cast<void>(scene.view.AdvanceAnimation(1'001u));
        host.RequestAnimationAt(*first, host.GetAnimationTickMs() + 500u);
        static_cast<void>(scene.view.AdvanceAnimation(2'000u));
        host.RequestAnimationAt(*second, host.GetAnimationTickMs() + 800u);
        host.CancelAnimation(*first);
        Check(IsDeadlineAt(scene.view.GetAnimationRequest(), 2'800u), "S3: after the cancel the application sleeps until 2,800");
        Hr(scene.view.Prepare(320u, 200u), "prepare before the new deadline");
        const size_t wakes = scene.wakes.calls;
        host.RequestAnimationAt(*first, host.GetAnimationTickMs() + 600u);
        Check(scene.wakes.calls == wakes + 1u && IsDeadlineAt(scene.view.GetAnimationRequest(), 2'600u),
              "S3: a deadline sooner than the one the application sleeps on wakes it");
    }
    // A Deadline after Continuous: the application reads the deadline, and a sooner one wakes it.
    {
        View scene(1'000u);
        DxUi::ControlHost& host = scene.view.Controls();
        auto* frames            = scene.root->AddChild<RequestProbe>();
        auto* deadline          = scene.root->AddChild<RequestProbe>();
        Hr(scene.view.Prepare(320u, 200u), "prepare the Continuous probes");
        static_cast<void>(scene.view.AdvanceAnimation(1'001u));
        host.RequestAnimationAt(*deadline, host.GetAnimationTickMs() + 5'000u);
        Check(IsDeadlineAt(scene.view.AdvanceAnimation(2'000u), 7'000u), "the application sleeps until 7,000");
        host.RequestAnimation(*frames);
        Check(scene.view.GetAnimationRequest().kind == Kind::Continuous, "the application is told every frame");
        host.CancelAnimation(*frames);
        Check(IsDeadlineAt(scene.view.GetAnimationRequest(), 7'000u), "after the cancel it reads the deadline again");
        Hr(scene.view.Prepare(320u, 200u), "prepare before the sooner deadline");
        const size_t wakes = scene.wakes.calls;
        host.RequestAnimationAt(*frames, host.GetAnimationTickMs() + 300u);
        Check(scene.wakes.calls == wakes + 1u, "a deadline sooner than the one read after Continuous wakes the application");
    }
    // A request whose control was destroyed: the application reads None, and the next request wakes it.
    {
        View scene(1'000u);
        DxUi::ControlHost& host = scene.view.Controls();
        auto* gone              = scene.root->AddChild<RequestProbe>();
        auto* next              = scene.root->AddChild<RequestProbe>();
        Hr(scene.view.Prepare(320u, 200u), "prepare the expiring probe");
        static_cast<void>(scene.view.AdvanceAnimation(1'001u));
        host.RequestAnimationAt(*gone, host.GetAnimationTickMs() + 500u);
        static_cast<void>(scene.view.AdvanceAnimation(2'000u));
        static_cast<void>(scene.root->TakeChild(0u)); // destroys the probe that held the deadline
        Check(scene.view.GetAnimationRequest().kind == Kind::None, "with its control gone the application reads None");
        Hr(scene.view.Prepare(320u, 200u), "prepare before the next request");
        const size_t wakes = scene.wakes.calls;
        host.RequestAnimationAt(*next, host.GetAnimationTickMs() + 900u);
        Check(scene.wakes.calls == wakes + 1u, "a request after None wakes the application, however late it is");
    }
    std::cout << "PASS the view's record follows the request the application last read\n";
}

// S4: at the end of the application's clock a deadline that cannot be reached is not reported, so the documented loop
// sleeps instead of advancing with a zero sleep forever (review F3).
inline void TestASaturatedClockNeverSpins()
{
    using Kind = DxUi::EmbeddedAnimationRequest::Kind;
    View scene(1'000u);
    DxUi::ControlHost& host = scene.view.Controls();
    auto* probe             = scene.root->AddChild<RequestProbe>();
    Hr(scene.view.Prepare(320u, 200u), "prepare the saturation probe");
    static_cast<void>(scene.view.AdvanceAnimation(1'001u));
    constexpr uint64_t kTop = UINT64_MAX - 100u;
    host.RequestAnimationAt(*probe, host.GetAnimationTickMs() + 1'000'000u);
    static_cast<void>(scene.view.AdvanceAnimation(kTop));
    host.RequestAnimationAt(*probe, host.GetAnimationTickMs() + 300u);
    size_t zeroSleeps = 0u;
    for (int iteration = 0; iteration < 1'000; ++iteration)
    {
        constexpr uint64_t now                 = UINT64_MAX;
        DxUi::EmbeddedAnimationRequest request = scene.view.GetAnimationRequest();
        if (request.kind == Kind::Continuous || (request.kind == Kind::Deadline && request.dueTickMs <= now))
            request = scene.view.AdvanceAnimation(now);
        if (request.kind == Kind::Continuous || (request.kind == Kind::Deadline && request.dueTickMs <= now))
            ++zeroSleeps;
    }
    Check(zeroSleeps == 0u && probe->ticks == 0u && scene.view.GetAnimationRequest().kind == Kind::None,
          "S4: an unreachable deadline is None: the loop sleeps, and nothing fires early");
    std::cout << "PASS a saturated application clock never makes the loop spin\n";
}

inline void Run()
{
    TestRequestIsOnTheApplicationClock();
    TestEarlierNeedWakesTheApplication();
    TestTooltipMarkWakesTheApplication();
    TestTooltipMarkAloneWakesTheApplication();
    TestDeadlinesSurvivePausesHidingAndClockJumps();
    TestOneCallRefreshesEveryUserSetting();
    TestStampsTakeTheApplicationsTime();
    TestTheRecordFollowsWhatTheApplicationRead();
    TestASaturatedClockNeverSpins();
}
} // namespace EmbeddedAnimationRequestTests
