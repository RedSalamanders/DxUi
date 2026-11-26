#include "Controls.Tests.LayoutScopes.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.PerformanceFixtures.h"
// Scenarios are never copied. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
#include "Embedded.Tests.PerformanceFixtureFx11.h"
#include "Embedded.Tests.PerformanceFixtureFx12.h"
#pragma warning(pop)
#include "GraphicsFixture.h"
#include "Support/LayoutScopes.Tests.Hooks.h"
#include "Support/WorkCounters.h"

#include <DxUi/Embedded.h>
#include <wil/resource.h>

#include <array>
#include <format>
#include <iostream>
#include <string>
#include <string_view>

// The W6.3 gate on its fixtures, window host, WARP (Tests/README.md, "A gate assertion"):
// - FX12 form-layout "value": one Label's text changes, its size unchanged. A preparation pass enters at most the Label's
//   ancestor chain (the Label, its row, its section, the column, the ScrollPanel and the root: 6 controls) and arranges no
//   intrinsic owner (the row, section and column FlexPanels).
// - FX12 typography-120 "move": 500 typed Labels move 1 DIP and back, 1,000 position-only moves: 0 text layouts created.
// - FX11 "tree-expand-*": a folder of 6 rows opens: its first frame creates text layouts for those 6 newly shown rows at most.
//   The animation frames that follow are an open clause (see the test).
namespace
{
using namespace PerformanceFixtures;

// The value Label of FX12's form-layout variants and its ancestors (Fx12::Scene::BuildFormLayout).
constexpr uint64_t kValueLabelChain = 6u;

void PrintLine(const std::string& text)
{
    std::cerr << text;
}

[[nodiscard]] CounterSummary PhaseSummary(const RunRecord& run, std::string_view phase, std::string_view counter)
{
    return Summarize(run.Phase(phase), counter);
}

void TestFx12SameSizeLabelTextStaysInItsAncestorChain()
{
#if defined(_DEBUG)
    constexpr std::array kVariants{"form-layout"};
#else
    constexpr std::array kVariants{"form-layout", "form-layout-10k"};
#endif
    for (const char* name : kVariants)
    {
        const Variant* variant = Fx12::kDefinition.FindVariant(name);
        Require(variant != nullptr, "FX12 declares its form-layout variants");
        // Debug builds compare every pruned pass of the run with a whole layout (invisible to the counters read below).
        const bool previous  = DxUi::Detail::LayoutScopeVerificationEnabled();
        const bool verifying = DxUi::Detail::SetLayoutScopeVerificationForTest(true);
        const RunRecord run  = RunWindowFixture(Fx12::kDefinition, *variant);
        static_cast<void>(DxUi::Detail::SetLayoutScopeVerificationForTest(previous));
        uint64_t mismatches = 0u;
        for (const auto& phase : Fx12::kPhases)
            mismatches += PhaseSummary(run, phase.name, "layout.scopeVerifyMismatches").total;
        PrintLine(std::format("    [W6.3] FX12 {} layout verification {}: {} mismatches\n", name, verifying ? "on" : "not in this build", mismatches));
        Require(mismatches == 0u, "W6.3 FX12: every pruned pass equals a whole layout");
        const auto visits   = PhaseSummary(run, "value", "frame.treeVisitsPreparation");
        const auto arranged = PhaseSummary(run, "value", "layout.intrinsicArrangements");
        const auto children = PhaseSummary(run, "value", "layout.arrangedChildren");
        const auto measures = PhaseSummary(run, "value", "layout.measureOverrideExecutions");
        PrintLine(std::format("    [W6.3] FX12 {} value (window), per step: preparation visits {}-{}, intrinsic arrangements {}-{}, arranged children "
                              "{}-{}, measure executions {}-{}\n",
                              name,
                              visits.minimum,
                              visits.maximum,
                              arranged.minimum,
                              arranged.maximum,
                              children.minimum,
                              children.maximum,
                              measures.minimum,
                              measures.maximum));
        // The same run with layout scopes off (the whole-tree walk and the eager invalidation of the base), for the record: the
        // level-by-level check must not measure more than the eager pass when a size does change ("size", "width").
        DxUi::Detail::SetLayoutScopePruningForTest(false);
        const RunRecord eager = RunWindowFixture(Fx12::kDefinition, *variant);
        DxUi::Detail::SetLayoutScopePruningForTest(true);
        for (const char* phase : {"value", "size", "width"})
        {
            const auto scoped = PhaseSummary(run, phase, "layout.measureOverrideExecutions").total;
            const auto whole  = PhaseSummary(eager, phase, "layout.measureOverrideExecutions").total;
            PrintLine(std::format("    [W6.3] FX12 {} {}: measure executions {} (scopes off {}), intrinsic arrangements {} ({}), arranged children {} ({}), "
                                  "preparation visits {} ({})\n",
                                  name,
                                  phase,
                                  scoped,
                                  whole,
                                  PhaseSummary(run, phase, "layout.intrinsicArrangements").total,
                                  PhaseSummary(eager, phase, "layout.intrinsicArrangements").total,
                                  PhaseSummary(run, phase, "layout.arrangedChildren").total,
                                  PhaseSummary(eager, phase, "layout.arrangedChildren").total,
                                  PhaseSummary(run, phase, "frame.treeVisitsPreparation").total,
                                  PhaseSummary(eager, phase, "frame.treeVisitsPreparation").total));
            Require(scoped <= whole, "W6.3 FX12: layout scopes measure no more than the whole-tree pass");
        }
        Require(visits.maximum <= kValueLabelChain, "W6.3 FX12: a same-size Label text change visits at most its ancestor chain");
        Require(arranged.total == 0u && children.total == 0u, "W6.3 FX12: a same-size Label text change arranges no ancestor");
        // The size phase (the Label grows and shrinks) must still lay its row out again.
        Require(PhaseSummary(run, "size", "layout.intrinsicArrangements").minimum > 0u, "W6.3 FX12: a Label whose size changes lays its ancestors out");
    }
}

void TestFx12PositionOnlyMovesCreateNoTextLayout()
{
    for (const char* name : {"default", "typography-120"})
    {
        const Variant* variant = Fx12::kDefinition.FindVariant(name);
        Require(variant != nullptr, "FX12 declares its dense-screen variants");
        const RunRecord run = RunWindowFixture(Fx12::kDefinition, *variant);
        const auto created  = PhaseSummary(run, "move", "resources.textLayout.created");
        const auto implicit = PhaseSummary(run, "move", "resources.implicitTextLayout.created");
        const auto paints   = PhaseSummary(run, "move", "frame.paintExecutions");
        const auto prepared = PhaseSummary(run, "move", "frame.treeVisitsPreparation");
        PrintLine(std::format("    [W6.3] FX12 {} move (window), {} steps: text layouts created {} (per step {}-{}), implicit {}, paints {}, preparation "
                              "visits {}\n",
                              name,
                              run.Phase("move").steps.size(),
                              created.total,
                              created.minimum,
                              created.maximum,
                              implicit.total,
                              paints.total,
                              prepared.total));
        // typography-120's typed Trees draw their rows through DrawText on every frame (W2.8), moved or not: a move step may add
        // no implicit layout beyond what an unchanged full frame draws.
        const auto still = PhaseSummary(run, "full", "resources.implicitTextLayout.created");
        Require(paints.minimum > 0u, "W6.3 FX12: every move step repaints the moved Labels");
        Require(created.total == 0u && implicit.maximum <= still.maximum, "W6.3 FX12: position-only moves create no text layout");
    }
}

void TestFx11TreeExpandCreatesLayoutsOnlyForNewRows()
{
    constexpr uint64_t kNewRows = Fx11::MotionTreeModel::kChildren;
    const RunRecord run         = RunWindowFixture(Fx11::kDefinition, Fx11::kVariants[0]);
    for (size_t rate = 0u; rate < Fx11::kRates.size(); ++rate)
    {
        const size_t phase  = Fx11::PhaseIndex(rate, Fx11::Transition::TreeExpand);
        const auto& record  = run.phases[phase];
        const auto created  = Summarize(record, "resources.textLayout.created");
        const auto implicit = Summarize(record, "resources.implicitTextLayout.created");
        PrintLine(std::format("    [W6.3] FX11 {} (window): text layouts created {} (first step {}), implicit {}\n",
                              Fx11::kPhases[phase].name,
                              created.total,
                              record.steps.empty() ? 0u : CounterValue(record.steps.front(), "resources.textLayout.created").value_or(0u),
                              implicit.total));
        // OPEN (reported, not met): the animation frames after the change create more layouts in the host's layout table. Its key
        // is the drawn box's width and height, computed by the Tree as bottom - top from fractional row positions while the
        // rows slide, so the same row height rounds to a neighbouring float on some frames (W2.1's table and the Tree's row
        // geometry, outside W6.3). Asserted here: the change itself lays out exactly the newly shown rows.
        const uint64_t first = record.steps.empty() ? 0u : CounterValue(record.steps.front(), "resources.textLayout.created").value_or(0u);
        Require(first <= kNewRows && implicit.total == 0u, "W6.3 FX11: the Tree expand itself creates text layouts only for its newly shown rows");
    }
}
} // namespace

namespace
{
using namespace DxUi;

// outer (vertical FlexPanel) > middle (vertical FlexPanel) > row (horizontal FlexPanel) > the Label and a Button. The Label
// keeps a 160 DIP minimum width unless `options` says otherwise. Owns its tree: moved, never copied.
// Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626)
struct Chain
{
    std::unique_ptr<FlexPanel> owned;
    FlexPanel* outer  = nullptr;
    FlexPanel* middle = nullptr;
    FlexPanel* row    = nullptr;
    Label* label      = nullptr;
};
#pragma warning(pop)
[[nodiscard]] Chain BuildChain(std::wstring text, const FlexChildOptions& options)
{
    Chain chain;
    chain.owned  = std::make_unique<FlexPanel>();
    chain.outer  = chain.owned.get();
    chain.middle = chain.outer->AddChild<FlexPanel>();
    chain.row    = chain.middle->AddChild<FlexPanel>();
    static_cast<void>(chain.row->SetOrientation(StackOrientation::Horizontal));
    static_cast<void>(chain.row->SetGap(6.0f));
    chain.label = chain.row->AddChild<Label>(std::move(text));
    chain.label->SetMultiline(true);
    RequireSucceeded(chain.row->SetChildOptions(chain.label, options), "the Label's options");
    chain.row->AddChild<Button>(L"After");
    return chain;
}
[[nodiscard]] DesiredSize MeasureAt(const Control& control, ControlHost& host, float width)
{
    DesiredSize size;
    RequireSucceeded(control.Measure(MeasureContext{host}, LayoutConstraints::AtMost(width, std::nullopt), size), "measure");
    return size;
}
[[nodiscard]] bool Same(const DesiredSize& a, const DesiredSize& b) noexcept
{
    return a.sizeDip.width == b.sizeDip.width && a.sizeDip.height == b.sizeDip.height && a.overflow == b.overflow && a.baselineDip == b.baselineDip;
}

// Addition A of W6.3: an assignment's effect reaches its ancestors before any measurement reads them. The parent and the
// control two levels up, measured in the same call stack as the assignment, answer what a fresh tree with the new text
// answers: for a size that changed, one that did not, a Label arranged at a declared size (a layout root: the row never reads
// its measure), a Label whose width alone is fixed by its constraints (not a root: its height comes from its measure), and a
// tree that is not attached to a host (eager, as before).
void TestAssignmentReachesAncestorsBeforeAnyMeasureReadsThem()
{
    constexpr float kWidth = 420.0f;
    const FlexChildOptions minimum{.constraints = {.minimumDip = D2D1::SizeF(160.0f, 0.0f)}};
    const FlexChildOptions declared{.explicitSizeDip = D2D1::SizeF(110.0f, 24.0f)};
    const FlexChildOptions fixedWidth{.constraints = {.minimumDip = D2D1::SizeF(110.0f, 0.0f), .maximumWidthDip = 110.0f}};
    struct Case
    {
        const char* name;
        FlexChildOptions options;
        const wchar_t* before;
        const wchar_t* after;
        bool attached;
        bool cutoff; // the queued invalidation stops at the Label
    };
    const std::array cases{
        Case{"same size", minimum, L"Value 1", L"Value 2", true, true},
        Case{"different size", minimum, L"Value", L"A caption long enough to need more than the minimum width of the label", true, false},
        Case{"declared size", declared, L"Short", L"A caption long enough to wrap inside a declared width, twice over at least", true, true},
        Case{"fixed width only", fixedWidth, L"Short", L"A caption long enough to wrap inside a fixed width, twice over at least", true, false},
        Case{"detached", minimum, L"Value", L"A caption long enough to need more than the minimum width of the label", false, false},
    };
    for (const Case& test : cases)
    {
        ControlHost host;
        Chain chain            = BuildChain(test.before, test.options);
        FlexPanel* const outer = chain.outer;
        if (test.attached)
            host.SetRoot(std::move(chain.owned));
        const DesiredSize outerBefore  = MeasureAt(*outer, host, kWidth);
        const DesiredSize middleBefore = MeasureAt(*chain.middle, host, kWidth);
        const Detail::WorkCounterScope scope;
        chain.label->SetText(test.after);
        const DesiredSize middleAfter = MeasureAt(*chain.middle, host, kWidth);
        const DesiredSize outerAfter  = MeasureAt(*outer, host, kWidth);
        const auto cutoffs            = scope.Delta().layout.measureCutoffs;
        ControlHost freshHost;
        Chain fresh = BuildChain(test.after, test.options);
        freshHost.SetRoot(std::move(fresh.owned));
        const DesiredSize middleFresh = MeasureAt(*fresh.middle, freshHost, kWidth);
        const DesiredSize outerFresh  = MeasureAt(*fresh.outer, freshHost, kWidth);
        PrintLine(std::format("    [W6.3] {}: outer {}x{} -> {}x{} (fresh {}x{}), {} cutoffs\n",
                              test.name,
                              outerBefore.sizeDip.width,
                              outerBefore.sizeDip.height,
                              outerAfter.sizeDip.width,
                              outerAfter.sizeDip.height,
                              outerFresh.sizeDip.width,
                              outerFresh.sizeDip.height,
                              cutoffs));
        Require(Same(middleAfter, middleFresh) && Same(outerAfter, outerFresh), "W6.3: a measurement right after an assignment sees the new size");
        if (test.cutoff)
            Require(cutoffs >= 1u && Same(outerBefore, outerAfter) && Same(middleBefore, middleAfter),
                    "W6.3: a size that held stops the invalidation at the Label and leaves the ancestors' sizes");
        else
            Require(! Same(outerBefore, outerAfter), "W6.3: the case changes the ancestors' size");
        if (! test.attached)
            Require(cutoffs == 0u, "W6.3: a control without a host invalidates its ancestors at once");
    }
}

// The verification seam (Debug and ASan Debug): a pruned pass's bounds equal a whole layout's, and its negative control, a
// cutoff taken without its check, is caught as a mismatch.
void TestLayoutScopeVerificationCatchesAWrongCutoff()
{
    const bool previous = Detail::LayoutScopeVerificationEnabled();
    if (! Detail::SetLayoutScopeVerificationForTest(true))
    {
        PrintLine("    [W6.3] layout scope verification: not in this build\n");
        return;
    }
    const auto restore = wil::scope_exit([previous]
    {
        static_cast<void>(Detail::SetLayoutScopeVerificationForTest(previous));
        Detail::SetLayoutCutoffUncheckedForTest(false);
    });
    GraphicsFixture graphics;
    RequireSucceeded(graphics.Create(), "W6.3 WARP fixture");
    std::shared_ptr<GraphicsDevice> device;
    RequireSucceeded(GraphicsDevice::Create(graphics.device.get(), device), "W6.3 graphics pool");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(device, StoppedTestClock()), "W6.3 embedded attach");
    Chain chain        = BuildChain(L"Value 1", FlexChildOptions{.constraints = {.minimumDip = D2D1::SizeF(160.0f, 0.0f)}});
    FlexPanel* outer   = chain.outer;
    Label* const label = chain.label;
    view.Controls().SetRoot(std::move(chain.owned));
    outer->SetBounds(D2D1::RectF(0.0f, 0.0f, 420.0f, 300.0f));
    const auto prepare = [&] { RequireSucceeded(view.Prepare(420u, 300u, 96.0f), "W6.3 preparation"); };
    prepare();
    const Detail::WorkCounterScope scope;
    for (const wchar_t* text : {L"Value 2", L"A caption long enough to need more than the minimum width of the label", L"Value 3"})
    {
        label->SetText(text);
        prepare();
    }
    const auto clean = scope.Delta().layout.scopeVerifyMismatches;
    Detail::SetLayoutCutoffUncheckedForTest(true);
    label->SetText(L"Another caption long enough to need more than the minimum width, and longer still than the first");
    prepare();
    const auto wrong = scope.Delta().layout.scopeVerifyMismatches - clean;
    PrintLine(std::format("    [W6.3] layout scope verification: {} mismatches on the script, {} with the unchecked cutoff\n", clean, wrong));
    Require(clean == 0u, "W6.3: every pruned pass equals a whole layout");
    Require(wrong > 0u, "W6.3: the verification sees a cutoff taken without its check");
    view.Controls().SetRoot(nullptr);
}
} // namespace

void RunLayoutScopeTests()
{
    DXUI_RUN_TEST(TestAssignmentReachesAncestorsBeforeAnyMeasureReadsThem);
    DXUI_RUN_TEST(TestLayoutScopeVerificationCatchesAWrongCutoff);
    DXUI_RUN_TEST(TestFx12SameSizeLabelTextStaysInItsAncestorChain);
    DXUI_RUN_TEST(TestFx12PositionOnlyMovesCreateNoTextLayout);
    DXUI_RUN_TEST(TestFx11TreeExpandCreatesLayoutsOnlyForNewRows);
}
