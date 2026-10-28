#include "Controls.Tests.AnimationClock.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "GraphicsFixture.h"
#include "Support/AnimationDispatcher.h"

#include <algorithm>
#include <array>
#include <functional>
#include <limits>

namespace
{
// The application's clock and the view's own animation clock start independently. The application epochs are those that
// once broke a control's transitions: zero, one, either side of 2^32, a large clock, near 2^63 and near the top of the range,
// where an unchecked sum wraps. The view's clock starts far below or far above system uptime, so a transition that stamps
// anything but the view's clock either never completes or completes at its first tick.
struct ClockCase
{
    uint64_t applicationEpochMs = 0u;
    uint64_t viewClockMs        = 0u;
};
constexpr uint64_t kViewClockAboveUptimeMs = 1'000'000'000'000'000u;
constexpr std::array<ClockCase, 7> kClockCases{{
    {0u, kViewClockAboveUptimeMs},
    {1u, 0u},
    {0xFFFF'FF00u, kViewClockAboveUptimeMs},
    {0xFFFF'FFFFu - 20u, 0u},
    {1'000'000'000'000u, 0u},
    {0x7FFF'FFFF'FFFF'F000u, kViewClockAboveUptimeMs},
    {(std::numeric_limits<uint64_t>::max)() - 100'000u, 0u},
}};

std::shared_ptr<DxUi::GraphicsDevice> MakeClockPool(GraphicsFixture& gpu)
{
    RequireSucceeded(gpu.Create(), "create the animation clock WARP device");
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    RequireSucceeded(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "share the animation clock graphics");
    return graphics;
}

// Attaches a view with motion on, starts its clock where asked, installs its root and prepares it. The first tick answers
// the root's discovery request at the epoch, after which nothing in these roots moves.
void StartClockView(DxUi::EmbeddedHost& view,
                    const std::shared_ptr<DxUi::GraphicsDevice>& pool,
                    std::unique_ptr<DxUi::Control> root,
                    uint64_t epoch,
                    std::optional<uint64_t> viewClockMs,
                    UINT width,
                    UINT height)
{
    RequireSucceeded(view.Attach(pool), "attach an application-clock view");
    view.Controls().DebugSetAnimationClockForTest(viewClockMs);
    view.Controls().SetTheme(MakeAnimatedTestThemePalette(false));
    view.Controls().SetRoot(std::move(root));
    RequireSucceeded(view.Prepare(width, height), "prepare the application-clock view");
    static_cast<void>(view.AdvanceAnimation(epoch));
    Require(! view.NeedsAnimation(), "the resting root releases animation after its discovery tick");
}

// After its last tick a view is at rest: nothing moves and, once prepared, nothing waits to be painted.
void RequireIdleView(DxUi::EmbeddedHost& view, UINT width, UINT height, const char* message)
{
    Require(SUCCEEDED(view.Prepare(width, height)) && ! view.NeedsAnimation() && ! view.NeedsPreparation(), message);
}

// The host's show delay (ResolveTooltipShowDelayMs in DxUi.WindowHost.cpp): the system hover time, within 100 to 2,500 ms.
[[nodiscard]] uint64_t ExpectedTooltipShowDelayMs() noexcept
{
    UINT hoverTimeMs = 0u;
    if (SystemParametersInfoW(SPI_GETMOUSEHOVERTIME, 0u, &hoverTimeMs, 0u) != FALSE && hoverTimeMs > 0u)
    {
        return std::clamp<uint64_t>(hoverTimeMs, 100u, 2'500u);
    }
    return 500u;
}

template <class Condition> [[nodiscard]] bool DispatchMessagesUntil(Condition&& reached)
{
    constexpr ULONGLONG kLimitMs = 10'000u;
    const ULONGLONG started      = GetTickCount64();
    for (ULONGLONG elapsedMs = 0u; elapsedMs < kLimitMs; elapsedMs = GetTickCount64() - started)
    {
        MSG msg{};
        if (PeekMessageW(&msg, nullptr, 0u, 0u, PM_REMOVE) == FALSE)
        {
            static_cast<void>(MsgWaitForMultipleObjectsEx(0u, nullptr, static_cast<DWORD>(kLimitMs - elapsedMs), QS_ALLINPUT, MWMO_INPUTAVAILABLE));
            continue;
        }
        static_cast<void>(DispatchQueuedMessageForTest(msg));
        if (reached())
        {
            return true;
        }
    }
    return false;
}

// Keeps the UI thread's dispatcher ticking while it lives, so its clock is the clock of its ticks.
class DispatcherTicks final
{
public:
    DispatcherTicks() noexcept : _subscriptionId(DxUi::Ui::AnimationDispatcher::GetInstance().Subscribe(&CountTick, &_tickCount))
    {
    }

    ~DispatcherTicks()
    {
        DxUi::Ui::AnimationDispatcher::GetInstance().Unsubscribe(_subscriptionId);
    }

    DispatcherTicks(const DispatcherTicks&)            = delete;
    DispatcherTicks& operator=(const DispatcherTicks&) = delete;

    // Whether the dispatcher delivered a tick, to every subscriber, since the call began.
    [[nodiscard]] bool WaitForTick()
    {
        const size_t ticksBefore = _tickCount;
        return _subscriptionId != 0u && DispatchMessagesUntil([this, ticksBefore] { return _tickCount != ticksBefore; });
    }

private:
    static bool CountTick(void* context, uint64_t /*nowTickMs*/) noexcept
    {
        ++*static_cast<size_t*>(context);
        return true;
    }

    size_t _tickCount        = 0u;
    uint64_t _subscriptionId = 0u;
};
} // namespace

// N-14 and Q14 of the 10 October review: a Button's hover stamped system uptime while an embedded view's ticks carried the
// application's clock, so with a clock far above uptime the first tick completed the hover with no frame in between.
void TestEmbeddedButtonHoverTransitionAtAnyEpoch()
{
    using namespace DxUi;
    GraphicsFixture gpu;
    const auto pool = MakeClockPool(gpu);
    for (const ClockCase& clock : kClockCases)
    {
        const uint64_t epoch = clock.applicationEpochMs;
        EmbeddedHost view;
        auto owned   = std::make_unique<ExposedButton>(L"Apply");
        auto* button = owned.get();
        StartClockView(view, pool, std::move(owned), epoch, clock.viewClockMs, 160u, 48u);
        auto& host = view.Controls();

        button->OnHoverChanged(host, true);
        Require(view.NeedsAnimation(), "hovering a button asks for ticks");
        Require(view.AdvanceAnimation(epoch + 1'000u), "the first tick keeps the hover running");
        RequireFloatNear(button->DebugGetHoverAnimationProgress(), 0.0f, 0.0001f, "and shows where it starts, whatever the application's epoch");
        Require(view.AdvanceAnimation(epoch + 1'070u), "the hover runs on application time");
        RequireFloatNear(button->DebugGetHoverAnimationProgress(), 0.5f, 0.0001f, "half its duration later, it is half way");
        static_cast<void>(view.AdvanceAnimation(epoch + 1'300u));
        static_cast<void>(view.AdvanceAnimation(epoch + 1'301u));
        RequireFloatNear(button->DebugGetHoverAnimationProgress(), 1.0f, 0.0001f, "the hover settles");
        RequireIdleView(view, 160u, 48u, "a settled hover leaves the view idle and clean");
    }
}

// Q14 of the 10 October review: a Slider's hover and press stamp the host's clock, not a first-tick anchor of their own.
void TestEmbeddedSliderVisualTransitionsAtAnyEpoch()
{
    using namespace DxUi;
    GraphicsFixture gpu;
    const auto pool = MakeClockPool(gpu);
    for (const ClockCase& clock : kClockCases)
    {
        const uint64_t epoch = clock.applicationEpochMs;
        EmbeddedHost view;
        auto owned   = std::make_unique<Slider>();
        auto* slider = owned.get();
        slider->SetValue(50.0);
        StartClockView(view, pool, std::move(owned), epoch, clock.viewClockMs, 220u, 48u);
        auto& host = view.Controls();

        slider->OnHoverChanged(host, true);
        Require(view.AdvanceAnimation(epoch + 1'000u), "the first tick keeps the slider's hover running");
        RequireFloatNear(slider->DebugGetHoverAnimationProgress(), 0.0f, 0.0001f, "and shows where it starts, whatever the application's epoch");
        Require(view.AdvanceAnimation(epoch + 1'040u), "the hover runs on application time");
        Require(slider->DebugGetHoverAnimationProgress() > 0.0f && slider->DebugGetHoverAnimationProgress() < 1.0f, "the hover is in flight");
        static_cast<void>(view.AdvanceAnimation(epoch + 1'200u));
        RequireFloatNear(slider->DebugGetHoverAnimationProgress(), 1.0f, 0.0001f, "the hover settles");

        const D2D1_RECT_F thumb    = slider->DebugGetThumbRect();
        const D2D1_POINT_2F center = D2D1::Point2F((thumb.left + thumb.right) * 0.5f, (thumb.top + thumb.bottom) * 0.5f);
        Require(slider->OnMouseDown(host, center, false, 0), "a press on the thumb starts a drag");
        Require(view.AdvanceAnimation(epoch + 1'216u), "the press runs from the hover's last tick");
        Require(slider->DebugGetPressAnimationProgress() > 0.0f && slider->DebugGetPressAnimationProgress() < 1.0f, "the press is in flight");
        static_cast<void>(view.AdvanceAnimation(epoch + 1'400u));
        RequireFloatNear(slider->DebugGetPressAnimationProgress(), 1.0f, 0.0001f, "the press settles");
        Require(slider->OnMouseUp(host, center, false, 0), "the drag ends");
        static_cast<void>(view.AdvanceAnimation(epoch + 1'600u));
        static_cast<void>(view.AdvanceAnimation(epoch + 1'601u));
        RequireFloatNear(slider->DebugGetPressAnimationProgress(), 0.0f, 0.0001f, "the release settles");
        RequireIdleView(view, 220u, 48u, "settled slider transitions leave the view idle and clean");
    }
}

// N-14 of the 10 October review: a caret's blink anchor was system uptime while the ticks carried the application's clock, so
// the unsigned difference wrapped and typing could leave the caret hidden.
void TestEmbeddedCaretRestartsVisibleAndBlinksAtAnyEpoch()
{
    using namespace DxUi;
    constexpr uint64_t kCaretBlinkPeriodMs = 530u; // kCaretBlinkPeriodMs in DxUi.TextInput.cpp and DxUi.ComboBox.cpp
    GraphicsFixture gpu;
    const auto pool = MakeClockPool(gpu);
    for (const ClockCase& clock : kClockCases)
    {
        const uint64_t epoch = clock.applicationEpochMs;
        for (const bool combo : {false, true})
        {
            EmbeddedHost view;
            std::unique_ptr<Control> owned;
            Control* editor = nullptr;
            std::function<bool()> caretVisible;
            if (combo)
            {
                auto box = std::make_unique<ComboBox>();
                box->SetEditable(true);
                box->SetText(L"Caret");
                ComboBox* const live = box.get();
                caretVisible         = [live] { return live->DebugIsCaretVisible(); };
                editor               = live;
                owned                = std::move(box);
            }
            else
            {
                auto field            = std::make_unique<TextField>(L"Caret");
                TextField* const live = field.get();
                caretVisible          = [live] { return live->DebugIsCaretVisible(); };
                editor                = live;
                owned                 = std::move(field);
            }
            StartClockView(view, pool, std::move(owned), epoch, clock.viewClockMs, 240u, 40u);

            view.Controls().SetFocusControl(editor);
            Require(view.NeedsAnimation(), "a focused caret asks for ticks");
            Require(view.AdvanceAnimation(epoch + 1'000u) && caretVisible(), "the caret starts visible, whatever the application's epoch");
            Require(view.AdvanceAnimation(epoch + 1'000u + kCaretBlinkPeriodMs + 10u) && ! caretVisible(), "and blinks off after one period");
            Require(view.DispatchCharacter(L'x') && caretVisible(), "typing shows the caret");
            Require(view.AdvanceAnimation(epoch + 1'000u + kCaretBlinkPeriodMs + 26u) && caretVisible(),
                    "and the next tick keeps it shown: typing restarted the blink");
            Require(view.AdvanceAnimation(epoch + 1'000u + 2u * kCaretBlinkPeriodMs + 40u) && ! caretVisible(),
                    "the restarted blink turns the caret off one period after the typing");
        }
    }
}

// N-14 of the 10 October review: the Tree stamped its expansion with system uptime and waited for a tick after the stamp, so
// with an application clock below uptime the rows stayed at their first frame; a UI Automation Expand asked for no ticks.
void TestEmbeddedTreeExpansionAnimatesAndCompletesAtAnyEpoch()
{
    using namespace DxUi;

    // The application publishes the folder's rows when asked to expand it.
    class PublishingDelegate final : public ITreeDelegate
    {
    public:
        PublishingDelegate(MutableTreeModel& model, Tree*& tree) noexcept : _model(&model), _tree(&tree)
        {
        }

        void OnTreeToggleExpanded(uint64_t /*itemId*/, bool expanded) override
        {
            std::vector<TreeItemData> items{TreeItemData{.id = 1u, .text = L"Folder", .hasChildren = true, .expanded = expanded}};
            if (expanded)
            {
                items.push_back(TreeItemData{.id = 2u, .parentId = 1u, .text = L"Child one", .depth = 1u});
                items.push_back(TreeItemData{.id = 3u, .parentId = 1u, .text = L"Child two", .depth = 1u});
            }
            items.push_back(TreeItemData{.id = 4u, .text = L"Other"});
            _model->SetVisibleItems(std::move(items));
            (*_tree)->NotifyDataChanged();
        }

    private:
        MutableTreeModel* _model;
        Tree** _tree;
    };

    GraphicsFixture gpu;
    const auto pool = MakeClockPool(gpu);
    for (const ClockCase& clock : kClockCases)
    {
        const uint64_t epoch = clock.applicationEpochMs;
        MutableTreeModel model;
        model.SetVisibleItems({TreeItemData{.id = 1u, .text = L"Folder", .hasChildren = true, .expanded = false}, TreeItemData{.id = 4u, .text = L"Other"}});
        Tree* tree = nullptr;
        PublishingDelegate delegate(model, tree);
        EmbeddedHost view;
        auto owned = std::make_unique<Tree>();
        tree       = owned.get();
        tree->SetModel(&model);
        tree->SetDelegate(&delegate);
        StartClockView(view, pool, std::move(owned), epoch, clock.viewClockMs, 240u, 180u);
        const auto& host = view.Controls();

        // UI Automation's Expand reaches the tree through no input handler; the expansion still asks for its ticks.
        Require(tree->RequestExpandedState(0u, true), "an Expand the application publishes succeeds");
        Require(view.NeedsAnimation(), "the expansion asks for ticks itself");
        Require(view.AdvanceAnimation(epoch + 1'000u), "the first tick keeps the expansion running");
        Require(tree->DebugGetExpansionAnimationItemId() == std::optional<uint64_t>{1u}, "the folder's rows slide open");
        RequireFloatNear(tree->DebugGetExpansionProgress(host.GetAnimationTickMs()), 0.0f, 0.0001f, "from where they start, whatever the application's epoch");
        Require(view.AdvanceAnimation(epoch + 1'160u), "the expansion runs on application time");
        const float rows    = tree->DebugGetExpansionProgress(host.GetAnimationTickMs());
        const float chevron = tree->DebugGetExpanderProgress(1u, true, host.GetAnimationTickMs());
        Require(rows > 0.0f && rows < 1.0f && chevron > 0.0f && chevron < 1.0f, "half way, the rows and the chevron are in flight");
        static_cast<void>(view.AdvanceAnimation(epoch + 1'400u));
        static_cast<void>(view.AdvanceAnimation(epoch + 1'401u));
        Require(! tree->DebugGetExpansionAnimationItemId().has_value(), "the expansion completes");
        RequireIdleView(view, 240u, 180u, "a completed expansion leaves the view idle and clean");
    }
}

// N-14 of the 10 October review: the Grid stamped its sort glyph with system uptime, so with an application clock below
// uptime the glyph never settled and its view ticked and repainted forever.
void TestEmbeddedGridSortGlyphSettlesAtAnyEpoch()
{
    using namespace DxUi;
    GraphicsFixture gpu;
    const auto pool = MakeClockPool(gpu);
    for (const ClockCase& clock : kClockCases)
    {
        const uint64_t epoch = clock.applicationEpochMs;
        GridCellData cellData;
        cellData.kind = GridCellKind::Text;
        cellData.text = L"Ready";
        SingleCellGridModel model(std::move(cellData));
        EmbeddedHost view;
        auto owned = std::make_unique<Grid>();
        Grid* grid = owned.get();
        grid->SetModel(&model);
        StartClockView(view, pool, std::move(owned), epoch, clock.viewClockMs, 320u, 180u);
        const auto& host = view.Controls();

        grid->SetSortSpec({0u, SortDirection::Ascending});
        view.MarkDirty();
        RequireSucceeded(view.Prepare(320u, 180u), "paint the sort glyph's first frame");
        Require(view.NeedsAnimation(), "the glyph's cross-fade asks for ticks");
        Require(view.AdvanceAnimation(epoch + 1'000u), "the first tick keeps the cross-fade running");
        Require(grid->DebugGetSortGlyphVisualState(host.GetTheme(), 0u, host.GetAnimationTickMs()).animating, "the glyph starts its cross-fade");
        Require(view.AdvanceAnimation(epoch + 1'070u), "the cross-fade runs on application time");
        const GridSortGlyphVisualState midway = grid->DebugGetSortGlyphVisualState(host.GetTheme(), 0u, host.GetAnimationTickMs());
        Require(midway.animating && midway.currentAlpha > 0.0f && midway.currentAlpha < 1.0f, "half way, the glyph fades in");
        static_cast<void>(view.AdvanceAnimation(epoch + 1'300u));
        static_cast<void>(view.AdvanceAnimation(epoch + 1'301u));
        Require(! grid->DebugGetSortGlyphVisualState(host.GetTheme(), 0u, host.GetAnimationTickMs()).animating, "the glyph settles");
        RequireIdleView(view, 320u, 180u, "a settled glyph stops ticking: nothing animates forever");
    }
}

// N-14 of the 10 October review: a view's first tooltip took its deadline from system uptime, so with an application clock
// below uptime it never showed while the view ticked on, and with a clock above uptime it showed at once.
void TestEmbeddedFirstTooltipHonorsItsDelayAtAnyEpoch()
{
    using namespace DxUi;
    const uint64_t delayMs = ExpectedTooltipShowDelayMs();
    GraphicsFixture gpu;
    const auto pool = MakeClockPool(gpu);
    for (const ClockCase& clock : kClockCases)
    {
        const uint64_t epoch = clock.applicationEpochMs;
        EmbeddedHost view;
        StartClockView(view, pool, std::make_unique<Panel>(), epoch, clock.viewClockMs, 320u, 160u);
        auto& host = view.Controls();

        Require(host.SetTooltipDelayed(L"Help", D2D1::Point2F(8.0f, 8.0f)), "the view's first tooltip is scheduled");
        Require(view.AdvanceAnimation(epoch + 1'000u) && ! host.HasTooltip(), "the first tick waits for the delay, whatever the application's epoch");
        Require(view.AdvanceAnimation(epoch + 1'000u + delayMs - 10u) && ! host.HasTooltip(), "the tooltip waits for its whole delay");
        static_cast<void>(view.AdvanceAnimation(epoch + 1'000u + delayMs + 10u));
        Require(host.HasTooltip(), "and shows once the delay passed");
    }
}

// Q14 of the 10 October review: an embedded view's clock counts only the time between the ticks it asked for. An idle,
// hidden or zero-sized minute adds nothing, and a clock that goes back adds nothing either.
void TestEmbeddedAnimationClockSkipsPausesInTicking()
{
    using namespace DxUi;
    constexpr uint64_t kMinuteMs = 60'000u;
    GraphicsFixture gpu;
    const auto pool = MakeClockPool(gpu);
    for (const ClockCase& clock : {ClockCase{0u, kViewClockAboveUptimeMs},
                                   ClockCase{0xFFFF'FF00u, 0u},
                                   ClockCase{1'000'000'000'000u, kViewClockAboveUptimeMs},
                                   ClockCase{0x7FFF'FFFF'FFFF'F000u, 0u}})
    {
        const uint64_t epoch = clock.applicationEpochMs;
        EmbeddedHost view;
        auto owned   = std::make_unique<ExposedButton>(L"Apply");
        auto* button = owned.get();
        StartClockView(view, pool, std::move(owned), epoch, clock.viewClockMs, 160u, 48u);
        const auto progress = [button] { return button->DebugGetHoverAnimationProgress(); };

        button->OnHoverChanged(view.Controls(), true);
        Require(view.AdvanceAnimation(epoch + kMinuteMs), "a hover after an idle minute ticks");
        RequireFloatNear(progress(), 0.0f, 0.0001f, "the idle minute does not count");
        static_cast<void>(view.AdvanceAnimation(epoch + kMinuteMs + 35u));
        RequireFloatNear(progress(), 0.25f, 0.0001f, "a quarter of the hover's duration later, it is a quarter of the way");

        view.SetVisible(false);
        view.SetVisible(true);
        RequireSucceeded(view.Prepare(160u, 48u), "prepare the shown view");
        Require(view.AdvanceAnimation(epoch + 2u * kMinuteMs + 35u), "the shown view ticks");
        RequireFloatNear(progress(), 0.25f, 0.0001f, "a hidden minute does not count");
        static_cast<void>(view.AdvanceAnimation(epoch + 2u * kMinuteMs + 70u));
        RequireFloatNear(progress(), 0.5f, 0.0001f, "the hover goes on from where it was");

        Require(view.Prepare(0u, 0u) == S_FALSE && ! view.NeedsAnimation(), "a zero-sized view is not ticked");
        RequireSucceeded(view.Prepare(160u, 48u), "prepare the sized view");
        Require(view.AdvanceAnimation(epoch + 3u * kMinuteMs + 70u), "the sized view ticks");
        RequireFloatNear(progress(), 0.5f, 0.0001f, "a zero-sized minute does not count");

        static_cast<void>(view.AdvanceAnimation(epoch + 100u));
        RequireFloatNear(progress(), 0.5f, 0.0001f, "a tick that goes back adds nothing");
        static_cast<void>(view.AdvanceAnimation(epoch + 135u));
        RequireFloatNear(progress(), 0.75f, 0.0001f, "and the next tick counts from it");
    }
}

// Q14 of the 10 October review: each attachment brings its own application clock. The view's clock never goes back across
// attachments, even from far above system uptime, and a new application clock far ahead of the old one does not complete a
// transition at its first tick.
void TestEmbeddedAnimationClockContinuesAcrossAttachments()
{
    using namespace DxUi;
    GraphicsFixture gpu;
    const auto pool = MakeClockPool(gpu);
    EmbeddedHost view;
    auto first   = std::make_unique<ExposedButton>(L"First");
    auto* button = first.get();
    StartClockView(view, pool, std::move(first), 5'000u, kViewClockAboveUptimeMs, 160u, 48u);
    button->OnHoverChanged(view.Controls(), true);
    static_cast<void>(view.AdvanceAnimation(6'000u));
    static_cast<void>(view.AdvanceAnimation(6'300u));
    static_cast<void>(view.AdvanceAnimation(6'301u));
    RequireFloatNear(button->DebugGetHoverAnimationProgress(), 1.0f, 0.0001f, "the first attachment's hover settles");
    const uint64_t firstClockMs = view.Controls().GetAnimationTickMs();

    view.Detach();
    constexpr uint64_t kSecondEpoch = 1'000'000'000'000u;
    auto second                     = std::make_unique<ExposedButton>(L"Second");
    button                          = second.get();
    StartClockView(view, pool, std::move(second), kSecondEpoch, std::nullopt, 160u, 48u);
    Require(view.Controls().GetAnimationTickMs() >= firstClockMs, "the animation clock never goes back across attachments");
    button->OnHoverChanged(view.Controls(), true);
    Require(view.AdvanceAnimation(kSecondEpoch + 1'000u), "the second attachment's hover ticks");
    RequireFloatNear(button->DebugGetHoverAnimationProgress(), 0.0f, 0.0001f, "and starts from rest rather than jumping by the clocks' difference");
    static_cast<void>(view.AdvanceAnimation(kSecondEpoch + 1'070u));
    RequireFloatNear(button->DebugGetHoverAnimationProgress(), 0.5f, 0.0001f, "half its duration later, it is half way");
}

// Q14 of the 10 October review: a native host's animation clock is its thread's dispatcher timeline, which an idle host reads
// too, never the last tick that host received.
void TestNativeAnimationClockIsTheDispatcherTimeline()
{
    using namespace DxUi;
    auto& dispatcher = Ui::AnimationDispatcher::GetInstance();
    WindowHost host;
    const uint64_t beforeMs = dispatcher.GetCurrentTickMs();
    static_cast<void>(host.DebugAnimationTickForTest(1u));
    const uint64_t nowMs = host.GetAnimationTickMs();
    Require(nowMs >= beforeMs && nowMs <= dispatcher.GetCurrentTickMs(), "a native host reads the dispatcher's clock, not its own last tick");
    host.DebugSetAnimationClockForTest(42u);
    Require(host.GetAnimationTickMs() == 42u, "a test can hold a native host's clock");
    host.DebugSetAnimationClockForTest(std::nullopt);
    Require(host.GetAnimationTickMs() >= nowMs, "and give it back to the dispatcher");
}

// N-15 of the 10 October review: native transitions stamped system uptime while their ticks followed the dispatcher's
// timeline, which loses every stall beyond its hitch clamp. After a stall a transition stood still for the lost time.
void TestNativeTransitionStartsOnTheDispatcherTimelineAfterAStall()
{
    using namespace DxUi;
    auto& dispatcher = Ui::AnimationDispatcher::GetInstance();
    AttachedHostWindow window;
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    EnableMotionForTest(window.Host());
    auto root    = std::make_unique<Panel>();
    auto* slider = root->AddChild<Slider>();
    slider->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 48.0f));
    slider->SetValue(20.0);
    window.Host().SetRoot(std::move(root));
    window.Host().SetFocusControl(slider, false);
    DispatcherTicks ticks;
    Require(ticks.WaitForTick(), "the dispatcher ticks");

    // A stall longer than the hitch clamp: the dispatcher adds only the clamp, so its clock falls behind uptime.
    const uint64_t clampMs = dispatcher.DebugGetHitchClampUsForTest() / 1'000u;
    Sleep(static_cast<DWORD>(4u * clampMs));
    Require(ticks.WaitForTick(), "the dispatcher ticks after the stall");
    Require(GetTickCount64() > window.Host().GetAnimationTickMs() + clampMs, "the stall left the dispatcher's clock behind uptime");

    const double beforeValue = slider->DebugGetDisplayedValue();
    Require(slider->OnKeyDown(window.Host(), VK_RIGHT, 0) && slider->DebugGetDisplayedValue() == beforeValue, "a keyboard step starts a value animation");
    Require(ticks.WaitForTick(), "the dispatcher ticks once more");
    Require(slider->DebugGetDisplayedValue() > beforeValue, "the next tick moves the thumb: the start and the ticks share one clock");
}
