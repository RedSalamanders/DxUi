#pragma once
// Pointer-move cost on the EmbeddedHost (plan W2.9; Specs/UI/UI_InputAndAccessibility.md, "Pointer hit result"): one hit
// test per pointer move on FX14 and FX2, no preparation membership search and no plain label pending preparation on FX10,
// the seeded differential against the uncached hit test, and the hit result's lifetime. Included once, from
// Embedded.Tests.Embedded.cpp, after Embedded.Tests.PerformanceFixtures.h (RunEmbedded, EmbeddedInstance, Check, Hr).
#include "Controls/DxUi.Internal.h"
#include "Support.Tests.PointerHitDifferential.h"

namespace EmbeddedPointerHit
{
using PointerHitDifferential::Log;
using PointerHitDifferential::Recorder;

// Scenes and probes are owned in place and never copied. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4623 4625 4626 5026 5027)
// An EmbeddedHost at 96 DPI on the suite's WARP device. An embedded host has no cursor query: its pointer event is one
// DispatchPointer, whose hover and dispatch share the event's hit result.
class EmbeddedHitScene final
{
public:
    static constexpr bool kNativePopups = false;
    static constexpr UINT kWidth        = 440u;
    static constexpr UINT kHeight       = 340u;

    EmbeddedHitScene(ID3D11Device* device, bool hitResultEnabled)
    {
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(device, graphics), "pointer-hit graphics pool");
        Hr(view.Attach(std::move(graphics)), "pointer-hit view attachment");
        auto theme          = DxUi::MakeDefaultThemePalette(false);
        theme.reducedMotion = true;
        view.Controls().SetTheme(theme);
        view.Controls().DebugSetPointerHitResultEnabled(hitResultEnabled);
    }
    ~EmbeddedHitScene()
    {
        view.Detach();
    }
    [[nodiscard]] DxUi::ControlHost& Host() noexcept
    {
        return view.Controls();
    }
    void SetRoot(std::unique_ptr<DxUi::Control> root)
    {
        view.Controls().SetRoot(std::move(root));
    }
    void Frame()
    {
        for (int turn = 0; turn < 64; ++turn)
        {
            const HRESULT prepared = view.Prepare(kWidth, kHeight, 96.0f);
            Check(SUCCEEDED(prepared) || prepared == E_ABORT, "pointer-hit preparation");
            if (! view.NeedsPreparation())
                break;
        }
    }
    [[nodiscard]] D2D1_POINT_2F Snap(D2D1_POINT_2F dip) const noexcept
    {
        return D2D1::Point2F(std::round(dip.x), std::round(dip.y));
    }
    DxUi::WindowHostCursorKind CursorQuery(D2D1_POINT_2F) noexcept
    {
        return DxUi::WindowHostCursorKind::Default;
    }
    void Move(D2D1_POINT_2F dip)
    {
        Dispatch(DxUi::PointerAction::Move, dip);
    }
    void Down(D2D1_POINT_2F dip)
    {
        Dispatch(DxUi::PointerAction::Down, dip);
    }
    void Up(D2D1_POINT_2F dip)
    {
        Dispatch(DxUi::PointerAction::Up, dip);
    }
    void MiddleDown(D2D1_POINT_2F dip)
    {
        Dispatch(DxUi::PointerAction::MiddleDown, dip);
    }
    void MiddleUp(D2D1_POINT_2F dip)
    {
        Dispatch(DxUi::PointerAction::MiddleUp, dip);
    }
    void Wheel(D2D1_POINT_2F dip, float delta)
    {
        DxUi::PointerEvent event{DxUi::PointerAction::Wheel, dip.x, dip.y};
        event.wheelDelta = delta;
        static_cast<void>(view.DispatchPointer(event));
    }
    void Leave()
    {
        Dispatch(DxUi::PointerAction::Leave, D2D1::Point2F());
    }
    void Key(UINT virtualKey)
    {
        static_cast<void>(view.DispatchKey(virtualKey, true));
        static_cast<void>(view.DispatchKey(virtualKey, false));
    }

    DxUi::EmbeddedHost view;

private:
    void Dispatch(DxUi::PointerAction action, D2D1_POINT_2F dip)
    {
        static_cast<void>(view.DispatchPointer({action, dip.x, dip.y}));
    }
};

// A recorder that removes and destroys itself from inside a hover callback (entering) or its move callback.
class SelfRemover final : public Recorder
{
public:
    SelfRemover(std::wstring name, Log& log, bool onEnter) : Recorder(std::move(name), log, false), _onEnter(onEnter)
    {
        DeclareHitTestReported(); // a declaration covers exactly its class
    }
    DxUi::Panel* owner = nullptr;
    void OnHoverChanged(DxUi::ControlHost& host, bool hovered) override
    {
        Recorder::OnHoverChanged(host, hovered);
        if (hovered && _onEnter)
            RemoveSelf(); // `this` is gone afterwards
    }
    bool OnMouseMove(DxUi::ControlHost& host, D2D1_POINT_2F point, UINT modifiers) override
    {
        static_cast<void>(Recorder::OnMouseMove(host, point, modifiers));
        if (! _onEnter)
            RemoveSelf(); // `this` is gone afterwards
        return true;
    }

private:
    void RemoveSelf() noexcept
    {
        DxUi::Panel* const parent = owner;
        for (size_t index = 0u; parent && index < parent->GetLogicalChildCount(); ++index)
            if (parent->GetLogicalChild(index) == this)
            {
                std::unique_ptr<DxUi::Control> self = parent->TakeChild(index);
                static_cast<void>(parent->CompactChildren());
                self.reset();
                return;
            }
    }
    bool _onEnter = false;
};
#pragma warning(pop)

[[nodiscard]] inline D2D1_POINT_2F CenterOf(const DxUi::Control& control) noexcept
{
    const D2D1_RECT_F bounds = control.GetBounds();
    return D2D1::Point2F(std::round((bounds.left + bounds.right) * 0.5f), std::round((bounds.top + bounds.bottom) * 0.5f));
}

// FX14 and FX2 on the embedded host: one hit test per pointer move; the dispatch reuses the hover's.
__declspec(noinline) inline void TestPointerHitFx14AndFx2HitTestOncePerMove()
{
    using namespace PerformanceFixtures;
    const auto check = [](const RunRecord& run, std::string_view fixture, std::initializer_list<std::string_view> phases)
    {
        for (const std::string_view phase : phases)
        {
            const CounterSummary walks  = Summarize(run.Phase(phase), "frame.hitTests");
            const CounterSummary reused = Summarize(run.Phase(phase), "frame.hitTestReuses");
            std::cout << "    " << fixture << " embedded " << phase << ": hit tests per move " << walks.minimum << '-' << walks.maximum << ", reused "
                      << reused.minimum << '-' << reused.maximum << '\n';
            Check(walks.minimum == 1u && walks.maximum == 1u, "one hit test per pointer move");
            Check(reused.minimum == 1u && reused.maximum == 1u, "the dispatch reuses the hover's hit result");
        }
    };
    check(RunEmbedded(Fx14::kDefinition, Fx14::kVariants[0]), "FX14", {"enter", "pending", "shown", "leave"});
    check(RunEmbedded(Fx2::kDefinition, Fx2::kVariants[0]), "FX2", {"hover", "fit-hover"});
    std::cout << "PASS W2.9 FX14 and FX2: one hit test per pointer move on the embedded host\n";
}

// FX10 on the embedded host: after every step of the live-resize sweep, no IsControlInTree search ran during preparation,
// no plain Label was prepared, and no plain Label is pending preparation.
__declspec(noinline) inline void TestPointerHitFx10PreparesNoPlainLabelAndSearchesNoTree()
{
    using namespace PerformanceFixtures;
    const Definition& definition = Fx10::kDefinition;
    GraphicsFixture gpu; // its own target: the fixture resizes it every step
    gpu.width  = Fx10::kVariants[0].width;
    gpu.height = Fx10::kVariants[0].height;
    Hr(gpu.Create(true), "FX10 WARP device");
    EmbeddedInstance instance;
    instance.Instantiate(definition);
    Hr(instance.Create(gpu.device.get(), Fx10::kVariants[0]), "FX10 build");
    size_t labels           = 0u;
    const auto plainPending = [&labels](const auto& self, const DxUi::Control* control) -> size_t
    {
        if (! control)
            return 0u;
        size_t pending = 0u;
        if (const auto* label = dynamic_cast<const DxUi::Label*>(control))
        {
            ++labels;
            if (! label->GetTypographyOverride() && label->GetMnemonic() == L'\0' && label->DebugVisualPreparationEnabled())
                ++pending;
        }
        for (size_t index = 0u; index < control->GetLogicalChildCount(); ++index)
            pending += self(self, control->GetLogicalChild(index));
        return pending;
    };
    for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
        for (size_t step = 0u; step < definition.phases[phase].steps; ++step)
        {
            const DxUi::Detail::WorkCounterScope scope;
            instance.Step(phase, step);
            Hr(instance.Frame(gpu), "FX10 frame");
            const auto work = scope.Delta();
            if (definition.phases[phase].kind == PhaseKind::Measured)
            {
                Check(work.frame.preparationMembershipSearches == 0u, "FX10: no IsControlInTree search during preparation");
                Check(work.frame.plainLabelPreparations == 0u, "FX10: no plain Label is prepared");
            }
            labels = 0u;
            Check(plainPending(plainPending, instance.View().Controls().GetRoot()) == 0u, "FX10: no plain Label is pending preparation after the step");
        }
    Check(labels >= 3u, "FX10 holds plain Labels (the paragraph and the status label among them)");
    std::cout << "PASS W2.9 FX10: no preparation tree search, no plain label prepared or pending, " << labels << " labels\n";
}

// The seeded differential on the embedded host (see Support.Tests.PointerHitDifferential.h).
__declspec(noinline) inline void TestPointerHitEmbeddedDifferentialMatchesTheUncachedHitTest(GraphicsFixture& gpu)
{
    using namespace PointerHitDifferential;
    constexpr std::array<uint64_t, 4u> kSeeds{0x5EED2901u, 0x5EED2902u, 0x5EED2903u, 0x5EED2904u};
    for (const uint64_t seed : kSeeds)
        for (const bool reportedHole : {true, false})
        {
            // The logs outlive the scenes: detaching a scene still reports hover changes.
            Log cachedLog;
            Log walkedLog;
            EmbeddedHitScene cached(gpu.device.get(), true);
            EmbeddedHitScene walked(gpu.device.get(), false);
            SceneSlots cachedSlots;
            SceneSlots walkedSlots;
            BuildScene(cached, cachedSlots, cachedLog, reportedHole);
            BuildScene(walked, walkedSlots, walkedLog, reportedHole);
            Rng cachedRng{seed};
            Rng walkedRng{seed};
            uint64_t reuses = 0u;
            for (size_t step = 0u; step < 160u; ++step)
            {
                const DxUi::Detail::WorkCounterScope scope;
                const StepRecord a = Step(cached, cachedSlots, cachedLog, cachedRng);
                reuses += scope.Delta().frame.hitTestReuses;
                const StepRecord b = Step(walked, walkedSlots, walkedLog, walkedRng);
                if (a != b)
                {
                    std::cerr << "W2.9 differential (embedded) seed " << seed << " hole reported " << reportedHole << " step " << step
                              << ":\n  cached: " << a.state << '\n';
                    for (const auto& line : a.lines)
                        std::cerr << "    " << line << '\n';
                    std::cerr << "  walked: " << b.state << '\n';
                    for (const auto& line : b.lines)
                        std::cerr << "    " << line << '\n';
                }
                Check(a == b, "a host that reuses hit results delivers what the uncached hit test delivers");
                Check(! cached.Host().DebugHasPointerHitResult(), "no hit result outlives its DispatchPointer");
            }
            std::cout << "  W2.9 differential embedded seed " << seed << (reportedHole ? " (reported hole)" : " (unreported hole)") << ": 160 steps, " << reuses
                      << " reused hit results, 0 differences\n";
            if (reportedHole)
                Check(reuses > 0u, "the cached scene reused hit results");
            else
                Check(reuses == 0u, "an unreported control in the tree turns reuse off");
        }
    std::cout << "PASS W2.9 embedded differential\n";
}

// The hit result never outlives its control: a control that removes and destroys itself from inside its hover callback,
// or its move callback, between the hover's hit test and the dispatch's; then another move at the same point. ASan Debug.
__declspec(noinline) inline void TestPointerHitEmbeddedResultNeverOutlivesItsControl(GraphicsFixture& gpu)
{
    for (const bool onEnter : {true, false})
    {
        Log log; // outlives the scene, whose detach reports hover changes
        EmbeddedHitScene scene(gpu.device.get(), true);
        auto root          = std::make_unique<DxUi::Panel>();
        DxUi::Panel* panel = root.get();
        auto* remover      = root->AddChild<SelfRemover>(L"remover", log, onEnter);
        remover->owner     = panel;
        remover->SetBounds(D2D1::RectF(20.0f, 20.0f, 200.0f, 80.0f));
        auto* behind = root->AddChild<Recorder>(L"other", log);
        behind->SetBounds(D2D1::RectF(20.0f, 100.0f, 200.0f, 160.0f));
        scene.SetRoot(std::move(root));
        scene.Frame();
        const D2D1_POINT_2F point = CenterOf(*remover);
        scene.Move(point); // the remover destroys itself inside this event
        Check(log.lines.size() >= 1u && log.lines.front() == "remover enter", "the remover was entered");
        Check(! scene.Host().DebugHasPointerHitResult(), "the event's hit result ended with it");
        scene.Frame();
        scene.Move(point);
        Check(scene.Host().DebugGetHoveredControl() == panel, "a second move at the same point finds the panel");
        Check(scene.Host().DebugGetHoveredControl() == scene.Host().DebugHitTestControl(point), "as the uncached hit test does");
        // Detach and device replacement leave nothing behind either.
        scene.Move(CenterOf(*behind));
        scene.view.SetVisible(false);
        Check(! scene.Host().DebugHasPointerHitResult(), "hiding leaves no hit result");
    }
    std::cout << "PASS W2.9 embedded hit result lifetime\n";
}

// Mutation coverage of the hit revision (W2.9 review): each scenario makes one hit-relevant change from a hover callback,
// between the hover's hit test and the dispatch's, where only the named site moves the revision. With the site live the
// press reaches the control now under the point; with only that site suppressed it reaches the stale one. Every scenario
// also runs once more with the revision comparison itself suppressed (HitRevisionSite::Key).
struct SiteState
{
    D2D1_POINT_2F point{};
    std::function<void(EmbeddedHitScene&)> open;   // after the first frame: open a popup, then the point is chosen
    std::function<void(EmbeddedHitScene&)> choose; // sets `point` once the popup is open
    std::function<void(DxUi::ControlHost&)> change;
};
struct SiteScenario
{
    const char* name;
    DxUi::HitRevisionSite site;
    bool freshHitsUnder; // the control the change leaves under the point is the `under` recorder
    std::function<void(DxUi::Panel&, Log&, SiteState&)> build;
    bool siteAlone = true; // false: another site also moves the revision for this change, so suppressing this one alone changes nothing
};

// Returns whether the `under` recorder received the press.
[[nodiscard]] inline bool RunSiteScenario(GraphicsFixture& gpu, const SiteScenario& scenario, DxUi::HitRevisionSite suppressed)
{
    Log log;
    EmbeddedHitScene scene(gpu.device.get(), true);
    auto root   = std::make_unique<DxUi::Panel>();
    auto* under = root->AddChild<Recorder>(L"under", log);
    under->SetBounds(D2D1::RectF(0.0f, 0.0f, 360.0f, 340.0f));
    SiteState state;
    scenario.build(*root, log, state);
    auto* away = root->AddChild<Recorder>(L"away", log);
    away->SetBounds(D2D1::RectF(380.0f, 300.0f, 436.0f, 336.0f));
    scene.SetRoot(std::move(root));
    scene.Frame();
    if (state.open)
        state.open(scene);
    scene.Frame();
    if (state.choose)
        state.choose(scene);
    scene.Move(CenterOf(*away));
    Check(scene.Host().DebugGetHoveredControl() == away, "the scenario starts hovering the away recorder");
    away->onHover = [&state, &scene](Recorder&, bool hovered)
    {
        if (! hovered && state.change)
            state.change(scene.Host());
    };
    log.lines.clear();
    DxUi::g_suppressedHitRevisionSiteForTest = suppressed;
    scene.Down(state.point);
    DxUi::g_suppressedHitRevisionSiteForTest = DxUi::HitRevisionSite::None;
    const bool underPressed                  = std::ranges::find(log.lines, std::string("under down")) != log.lines.end();
    scene.Up(state.point);
    return underPressed;
}

[[nodiscard]] inline std::vector<SiteScenario> SiteScenarios()
{
    using DxUi::HitRevisionSite;
    const auto openCombo = [](DxUi::ComboBox* combo)
    {
        return [combo](EmbeddedHitScene& scene)
        {
            scene.Host().SetFocusControl(combo, false);
            scene.Key(VK_F4);
        };
    };
    const auto addCombo = [](DxUi::Panel& parent, bool editable)
    {
        auto* combo = parent.AddChild<DxUi::ComboBox>();
        std::vector<DxUi::ComboBox::Item> items;
        for (int index = 0; index < 8; ++index)
            items.push_back({std::format(L"v{}", index), std::format(L"Item {}", index)});
        combo->SetItems(std::move(items));
        combo->SetEditable(editable);
        combo->SetAutoOpenOnTextInput(false);
        combo->SetBounds(D2D1::RectF(20.0f, 20.0f, 180.0f, 52.0f));
        return combo;
    };
    // A point inside the lower part of the open popup.
    const auto lowRow = [](DxUi::ComboBox* combo, SiteState& state)
    {
        return [combo, &state](EmbeddedHitScene&)
        {
            const D2D1_RECT_F popup = combo->DebugGetPopupBounds();
            state.point             = D2D1::Point2F(std::round((popup.left + popup.right) * 0.5f), std::round(popup.bottom - 10.0f));
        };
    };
    std::vector<SiteScenario> scenarios;
    // SetMaxVisibleItems and SetMinimumPopupItemHeight on an open popup were measured too: the open popup keeps its rows
    // until it reopens, so neither changes which control a point hits (they still move the revision, through
    // ResetPopupLayout). SetNoMatchesText does change it, below.
    scenarios.push_back({"ComboBox SetNoMatchesText widens the open popup",
                         HitRevisionSite::ComboBoxLayout,
                         false,
                         [=](DxUi::Panel& root, Log&, SiteState& state)
    {
        auto* combo = addCombo(root, true);
        combo->SetText(L"zzz");
        combo->SetNoMatchesText(L"None");
        state.open   = openCombo(combo);
        state.choose = [combo, &state](EmbeddedHitScene&)
        {
            const D2D1_RECT_F popup = combo->DebugGetPopupBounds();
            state.point             = D2D1::Point2F(std::round(popup.right + 30.0f), std::round((popup.top + popup.bottom) * 0.5f));
        };
        state.change = [combo](DxUi::ControlHost&)
        { combo->SetNoMatchesText(L"No item starts with what you typed, and this sentence is long enough to widen the popup"); };
    }});
    scenarios.push_back({"ComboBox loses focus and closes its popup",
                         HitRevisionSite::ComboBoxClose,
                         true,
                         [=](DxUi::Panel& root, Log&, SiteState& state)
    {
        auto* combo  = addCombo(root, false);
        state.open   = openCombo(combo);
        state.choose = lowRow(combo, state);
        state.change = [](DxUi::ControlHost& host) { host.SetFocusControl(nullptr); };
    },
                         false}); // ClosePopup also resets the popup layout (ComboBoxLayout): two sites cover it
    scenarios.push_back({"ComboBox refilters its open popup",
                         HitRevisionSite::ComboBoxRebuild,
                         true,
                         [=](DxUi::Panel& root, Log&, SiteState& state)
    {
        auto* combo = addCombo(root, true);
        combo->SetText(L"Item"); // every row matches
        state.open   = openCombo(combo);
        state.choose = lowRow(combo, state);
        state.change = [combo](DxUi::ControlHost&) { combo->SetText(L"Item 1"); };
    },
                         false}); // the recomputed placement differs too (ComboBoxLayout in UpdatePopupLayout): two sites cover it
    // A ScrollPanel offset change was measured too: the ScrollPanel is the host-level result anywhere inside it (its children,
    // their popups included, are its own sub-hit) and a child's popup stays within it, so scrolling does not change which
    // control the host hits today. NotifyScrollChanged still moves the revision (ScrollOffset), as defence in depth.
    scenarios.push_back({"Splitter SetPosition moves its separator away",
                         HitRevisionSite::SplitterPosition,
                         true,
                         [](DxUi::Panel& root, Log&, SiteState& state)
    {
        auto* splitter = root.AddChild<DxUi::Splitter>();
        splitter->SetBounds(D2D1::RectF(20.0f, 20.0f, 320.0f, 200.0f));
        splitter->SetPosition(100.0f);
        state.choose = [splitter, &state](EmbeddedHitScene&)
        {
            const D2D1_RECT_F separator = splitter->GetSeparatorBounds();
            state.point                 = D2D1::Point2F(std::round((separator.left + separator.right) * 0.5f), 60.0f);
        };
        state.change = [splitter](DxUi::ControlHost&) { splitter->SetPosition(220.0f); };
    }});
    scenarios.push_back({"Splitter SetOrientation turns its separator",
                         HitRevisionSite::SplitterGeometry,
                         true,
                         [](DxUi::Panel& root, Log&, SiteState& state)
    {
        auto* splitter = root.AddChild<DxUi::Splitter>();
        splitter->SetBounds(D2D1::RectF(20.0f, 20.0f, 320.0f, 200.0f));
        splitter->SetPosition(100.0f);
        state.choose = [splitter, &state](EmbeddedHitScene&)
        {
            const D2D1_RECT_F separator = splitter->GetSeparatorBounds();
            state.point                 = D2D1::Point2F(std::round((separator.left + separator.right) * 0.5f), 60.0f);
        };
        state.change = [splitter](DxUi::ControlHost&) { splitter->SetOrientation(DxUi::SplitterOrientation::Horizontal); };
    }});
    scenarios.push_back({"Slider SetOrientation turns its hit band",
                         HitRevisionSite::SliderOrientation,
                         true,
                         [](DxUi::Panel& root, Log&, SiteState& state)
    {
        auto* slider = root.AddChild<DxUi::Slider>();
        slider->SetBounds(D2D1::RectF(20.0f, 20.0f, 320.0f, 120.0f));
        state.point  = D2D1::Point2F(40.0f, 70.0f);
        state.change = [slider](DxUi::ControlHost&) { slider->SetOrientation(DxUi::SliderOrientation::Vertical); };
    }});
    scenarios.push_back({"PageIndicator SetPageCount leaves no hit bounds",
                         HitRevisionSite::PageIndicatorCount,
                         true,
                         [](DxUi::Panel& root, Log&, SiteState& state)
    {
        auto* indicator = root.AddChild<DxUi::PageIndicator>();
        indicator->SetPageCount(5u);
        indicator->SetBounds(D2D1::RectF(20.0f, 20.0f, 320.0f, 60.0f));
        state.point  = D2D1::Point2F(170.0f, 40.0f);
        state.change = [indicator](DxUi::ControlHost&) { indicator->SetPageCount(1u); };
    }});
    return scenarios;
}

__declspec(noinline) inline void TestPointerHitEmbeddedEverySiteMovesTheRevision(GraphicsFixture& gpu)
{
    std::string failures;
    for (const SiteScenario& scenario : SiteScenarios())
    {
        const bool live       = RunSiteScenario(gpu, scenario, DxUi::HitRevisionSite::None);
        const bool suppressed = RunSiteScenario(gpu, scenario, scenario.site);
        const bool keyless    = RunSiteScenario(gpu, scenario, DxUi::HitRevisionSite::Key);
        std::cout << "  W2.9 site scenario: " << scenario.name << ": under pressed live " << live << ", site suppressed " << suppressed << ", key suppressed "
                  << keyless << '\n';
        if (live != scenario.freshHitsUnder || (scenario.siteAlone && suppressed == scenario.freshHitsUnder) || keyless == scenario.freshHitsUnder)
            failures += std::string(scenario.name) + "; ";
    }
    if (! failures.empty())
        std::cerr << "W2.9 site scenarios that failed: " << failures << '\n';
    Check(failures.empty(),
          "with every site live the press reaches the control now under the point, and with that site (or the key's revision) suppressed the stale one");
    std::cout << "PASS W2.9 every hit-revision site is load-bearing\n";
}
} // namespace EmbeddedPointerHit
