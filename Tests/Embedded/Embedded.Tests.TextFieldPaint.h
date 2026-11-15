#pragma once
// Text field paint (plan item W2.7) on the embedded host. Included once from Embedded.Tests.Embedded.cpp after the performance
// fixtures and the allocation instrument; uses Check and Hr from there.
//
// - TestFx1TextFieldPaintGate: the item's gate on FX1. Every focused paint of its 64 Ki-unit multiline field, every quarter-notch
//   wheel step and every PageDown creates no text layout (resources.textLayout.created) while the text and width stay, and every
//   paint of the focused single-line fields (editable and read-only) makes no C++ allocation.
// - TestTextFieldCompositionStateCopies: through the embedded text-input path (EmbeddedHost::ApplyTextInput, the
//   application-side double of an IME, never a real one), a paint copies the native text-input state only while a composition
//   is active, once per paint (text.nativeTextInputStateCopies), and allocates nothing without one.
// - TestTextFieldStaleLayoutIsNeverUsed: after each change that invalidates the multiline layout, its line metrics or the
//   single-line layout (text, typing, a composition preview, width, height, font role, padding, density, DPI, masking, reveal,
//   a device replacement that moves the typography revision), the caret rectangle, hit tests and range bounds the field answers
//   from its retained layouts equal those it answers after every retained cache was dropped.
// - TestMaskedTextFieldWipesItsCopies: the single-line layout cache's copy of the display text is wiped (SecureWipe) whenever a
//   masked field drops or overwrites it, also right after the field is unmasked, and only then: a cache hit neither clears nor
//   copies, and an unmasked field's copy is cleared without the wipe. The multiline layout cache keeps no copy of the text.
// - TestTextFieldParity: the "text-field" incremental-versus-fresh script (Support.Tests.TextFieldPaint.h).
// - --test-text-field-paint: the tests above alone.
#include "Embedded.Tests.IncrementalParity.h"
#include "Support.Tests.TextFieldPaint.h"
#include "Support/Diagnostics.h"
#include "Support/WorkCounters.h"

#include <cstring>
#include <string>
#include <vector>

namespace TextFieldPaintTests
{
namespace Detail
{
// One frame of `instance` with the counters and the allocation count of the frame.
struct FrameMeasure
{
    DxUi::Detail::WorkCounters work{};
    size_t allocations       = 0u;
    size_t actionAllocations = 0u;
};
// The allocations of the frame alone (preparation and composition), and those of the action before it.
template <typename Action> [[nodiscard]] FrameMeasure MeasureFrame(PerformanceFixtures::EmbeddedInstance& instance, GraphicsFixture& gpu, const Action& action)
{
    const DxUi::Detail::WorkCounterScope scope;
    ::allocations    = 0u;
    countAllocations = true;
    action();
    const size_t actionAllocations = ::allocations;
    ::allocations                  = 0u;
    const HRESULT framed           = instance.Frame(gpu);
    countAllocations               = false;
    Hr(framed, "W2.7 frame");
    return {scope.Delta(), ::allocations, actionAllocations};
}
} // namespace Detail

// FX1 played once; per phase, the step counters the gate reads.
__declspec(noinline) inline void TestFx1TextFieldPaintGate()
{
    using namespace PerformanceFixtures;
    const RunRecord run = RunEmbedded(Fx1::kDefinition, Fx1::kVariants[0]);
    for (size_t phase = 0u; phase < run.phases.size(); ++phase)
    {
        uint64_t layouts = 0u, maximumLayouts = 0u, paintedSteps = 0u, stepAllocations = 0u, maximumAllocations = 0u;
        for (const StepRecord& step : run.phases[phase].steps)
        {
            layouts += step.work.resources.textLayout.created;
            maximumLayouts = (std::max)(maximumLayouts, step.work.resources.textLayout.created);
            if (step.work.frame.paintExecutions != 0u)
            {
                ++paintedSteps;
                stepAllocations += step.cppAllocations;
                maximumAllocations = (std::max)(maximumAllocations, static_cast<uint64_t>(step.cppAllocations));
            }
        }
        std::cout << "  FX1 " << Fx1::kPhases[phase].name << ": " << run.phases[phase].steps.size() << " steps, " << paintedSteps
                  << " painted; text layouts created " << layouts << " (max " << maximumLayouts << " per step); C++ allocations in painted steps "
                  << stepAllocations << " (max " << maximumAllocations << ")\n";
    }

    // The multiline field keeps its text and width from the build on: no step from its focus to the last PageDown lays it out.
    for (const size_t phase : {Fx1::kMultiline, Fx1::kMultilineWheel, Fx1::kMultilinePage})
        for (const StepRecord& step : run.phases[phase].steps)
            Check(step.work.resources.textLayout.created == 0u,
                  "W2.7: a focused paint, a quarter-notch wheel or a PageDown of FX1's 64 Ki-unit multiline field creates no text layout");
    uint64_t wheelPaints = 0u;
    for (const StepRecord& step : run.phases[Fx1::kMultilineWheel].steps)
        wheelPaints += step.work.frame.paintExecutions != 0u ? 1u : 0u;
    Check(wheelPaints >= Fx1::kWheelSteps / 4u, "FX1: the quarter-notch wheel scrolls and repaints at every fourth step");
    for (const StepRecord& step : run.phases[Fx1::kMultilinePage].steps)
        Check(step.work.frame.paintExecutions != 0u, "FX1: every PageDown moves the caret and repaints");

    // The focused single-line fields: every painted frame (a caret flip, the focus change) allocates nothing. The fixture's
    // steps above also count their action (a caret tick); here FX1 is played again with only each frame counted.
    GraphicsFixture gpu;
    Hr(gpu.Create(true), "W2.7 single-line paint device");
    EmbeddedInstance instance;
    instance.Instantiate(Fx1::kDefinition);
    Hr(instance.Create(gpu.device.get(), Fx1::kVariants[0]), "W2.7 single-line paint scene");
    for (const size_t phase : {Fx1::kWarm, Fx1::kEditable, Fx1::kReadOnly})
    {
        uint64_t painted = 0u, stepAllocations = 0u;
        for (size_t index = 0u; index < Fx1::kPhases[phase].steps; ++index)
        {
            const Detail::FrameMeasure frame = Detail::MeasureFrame(instance, gpu, [&] { instance.Step(phase, index); });
            if (frame.work.frame.paintExecutions == 0u || phase == Fx1::kWarm)
                continue;
            ++painted;
            stepAllocations += frame.actionAllocations;
            if (frame.allocations != 0u)
                std::cerr << "  FX1 " << Fx1::kPhases[phase].name << " step " << index << ": the frame made " << frame.allocations << " C++ allocations\n";
            Check(frame.allocations == 0u, "W2.7: a paint of FX1's focused single-line field makes no C++ allocation");
        }
        if (phase != Fx1::kWarm)
        {
            std::cout << "  FX1 " << Fx1::kPhases[phase].name << ": " << painted << " painted frames, 0 C++ allocations in them; " << stepAllocations
                      << " in the steps' own actions (caret ticks)\n";
            Check(painted != 0u, "FX1: the single-line phases paint");
        }
    }
    std::cout << "PASS FX1 text field paint gate: 0 layouts per focused multiline paint, quarter notch and PageDown; 0 allocations per focused "
                 "single-line paint\n";
}

__declspec(noinline) inline void TestTextFieldCompositionStateCopies()
{
    using namespace PerformanceFixtures;
    GraphicsFixture gpu;
    Hr(gpu.Create(true), "W2.7 composition device");
    EmbeddedInstance instance;
    instance.Instantiate(Fx1::kDefinition);
    Hr(instance.Create(gpu.device.get(), Fx1::kVariants[0]), "W2.7 composition scene");
    for (size_t step = 0u; step < Fx1::kPhases[Fx1::kWarm].steps; ++step)
    {
        instance.Step(Fx1::kWarm, step);
        Hr(instance.Frame(gpu), "W2.7 composition warm frame");
    }
    DxUi::EmbeddedHost& view = instance.View();
    auto* const editable     = dynamic_cast<DxUi::TextField*>(view.Controls().GetFocusControl());
    Check(editable && ! editable->IsReadOnly() && ! editable->IsMultiline(), "FX1's warm-up focuses the editable field");
    // The next caret flip, which repaints the field and changes nothing else: the fixture clock moves 100 ms a frame until a
    // frame paints, and that frame is measured.
    const auto flip = [&]
    {
        Detail::FrameMeasure frame;
        for (int step = 0; step < 20 && frame.work.frame.paintExecutions == 0u; ++step)
            frame = Detail::MeasureFrame(instance, gpu, [&] { instance.GetContext().port.AdvanceTime(Fx1::kStepMs); });
        return frame;
    };
    const Detail::FrameMeasure idle = flip();
    Check(idle.work.frame.paintExecutions != 0u, "a caret flip repaints the field");
    Check(idle.work.text.nativeTextInputStateCopies == 0u && idle.allocations == 0u,
          "W2.7: without a composition a paint copies no text-input state and allocates nothing");

    DxUi::EmbeddedTextInputSnapshot snapshot;
    Check(view.ReadTextInput(snapshot) == S_OK, "the focused field exports its text input");
    DxUi::NativeTextInputState state = snapshot.state;
    state.text.insert(state.caretIndex, L"\u6771\u4EAC");
    state.compositionStartIndex       = snapshot.state.caretIndex;
    state.compositionEndIndex         = snapshot.state.caretIndex + 2u;
    state.conversionTargetStartIndex  = snapshot.state.caretIndex;
    state.conversionTargetEndIndex    = snapshot.state.caretIndex + 1u;
    state.compositionCursorIndex      = snapshot.state.caretIndex + 2u;
    state.compositionClauseBoundaries = {snapshot.state.caretIndex, snapshot.state.caretIndex + 1u, snapshot.state.caretIndex + 2u};
    state.caretIndex                  = snapshot.state.caretIndex + 2u;
    state.selectionAnchorIndex.reset();
    Check(view.ApplyTextInput(snapshot.revision, state, DxUi::EmbeddedTextInputAction::Preview) == S_OK, "the composition preview is accepted");
    const Detail::FrameMeasure composing = Detail::MeasureFrame(instance, gpu, [] {});
    const Detail::FrameMeasure composed  = flip();
    Check(composed.work.frame.paintExecutions != 0u, "a caret flip during the composition repaints the field");
    std::cout << "  composition: idle paint copies " << idle.work.text.nativeTextInputStateCopies << " (allocations " << idle.allocations
              << "); first composing frame copies " << composing.work.text.nativeTextInputStateCopies << "; composing paint copies "
              << composed.work.text.nativeTextInputStateCopies << " (allocations " << composed.allocations << ")\n";
    Check(composed.work.text.nativeTextInputStateCopies == 1u, "W2.7: a paint during a composition copies the text-input state once");

    Check(view.ReadTextInput(snapshot) == S_OK, "the composing field exports its text input");
    DxUi::NativeTextInputState committed = snapshot.state;
    committed.compositionStartIndex.reset();
    committed.compositionEndIndex.reset();
    committed.conversionTargetStartIndex.reset();
    committed.conversionTargetEndIndex.reset();
    committed.compositionCursorIndex.reset();
    committed.compositionClauseBoundaries.clear();
    const HRESULT commit = view.ApplyTextInput(snapshot.revision, committed, DxUi::EmbeddedTextInputAction::Commit);
    if (commit != S_OK)
        std::cerr << "  commit: 0x" << std::hex << static_cast<uint32_t>(commit) << std::dec << '\n';
    Check(commit == S_OK, "the composition commits");
    static_cast<void>(Detail::MeasureFrame(instance, gpu, [] {}));
    const Detail::FrameMeasure after = flip();
    Check(after.work.frame.paintExecutions != 0u && after.work.text.nativeTextInputStateCopies == 0u,
          "W2.7: after the commit a paint copies no text-input state");
    std::cout << "PASS text field composition state: copied once per paint only while composing\n";
}

namespace Detail
{
// What a field answers from its layouts: the caret rectangle at a few indices, hit tests at a few points and range bounds.
struct Geometry
{
    std::vector<D2D1_RECT_F> carets;
    std::vector<size_t> hits;
    std::vector<D2D1_RECT_F> ranges;
    [[nodiscard]] bool operator==(const Geometry& other) const noexcept
    {
        return carets.size() == other.carets.size() && hits == other.hits && ranges.size() == other.ranges.size() &&
               std::memcmp(carets.data(), other.carets.data(), carets.size() * sizeof(D2D1_RECT_F)) == 0 &&
               std::memcmp(ranges.data(), other.ranges.data(), ranges.size() * sizeof(D2D1_RECT_F)) == 0;
    }
};

[[nodiscard]] inline Geometry ReadGeometry(DxUi::EmbeddedHost& view, DxUi::TextField& field)
{
    Geometry geometry;
    const size_t length = field.GetText().size();
    for (const size_t index : {size_t{0u}, length / 3u, length / 2u, length})
    {
        D2D1_RECT_F rect{};
        static_cast<void>(field.DebugGetCaretRect(view.Controls(), index, rect));
        geometry.carets.push_back(rect);
    }
    DxUi::EmbeddedTextInputSnapshot snapshot;
    if (view.ReadTextInput(snapshot) != S_OK)
        return geometry;
    const D2D1_RECT_F bounds = field.GetBounds();
    for (const float fraction : {0.1f, 0.37f, 0.5f, 0.83f})
    {
        size_t index      = 0u;
        const HRESULT hit = view.HitTestTextInput(
            snapshot.revision,
            D2D1::Point2F(bounds.left + (bounds.right - bounds.left) * fraction, bounds.top + (bounds.bottom - bounds.top) * (1.0f - fraction)),
            index);
        geometry.hits.push_back(SUCCEEDED(hit) ? index : static_cast<size_t>(-1));
    }
    if (length > 2u)
    {
        D2D1_RECT_F rect{};
        bool clipped = false;
        static_cast<void>(view.GetTextInputRangeBounds(snapshot.revision, length / 4u, length / 4u + 2u, rect, clipped));
        geometry.ranges.push_back(rect);
        static_cast<void>(view.GetTextInputRangeBounds(snapshot.revision, 0u, length, rect, clipped));
        geometry.ranges.push_back(rect);
    }
    return geometry;
}
} // namespace Detail

__declspec(noinline) inline void TestTextFieldStaleLayoutIsNeverUsed(GraphicsFixture& gpu)
{
    DxUi::EmbeddedHost view;
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "W2.7 stale-layout graphics");
    Hr(view.Attach(graphics), "W2.7 stale-layout view");
    view.Controls().SetTheme(IncrementalParity::ParityTheme(false));
    auto rootOwner   = std::make_unique<DxUi::Panel>();
    auto* const root = rootOwner.get();
    auto* multiline  = root->AddChild<DxUi::TextField>();
    multiline->SetMultiline(true);
    multiline->SetText(ParityMultilineText());
    multiline->SetBounds(D2D1::RectF(16.0f, 16.0f, 360.0f, 220.0f));
    auto* single = root->AddChild<DxUi::TextField>(L"secret pass phrase");
    single->SetBounds(D2D1::RectF(16.0f, 240.0f, 300.0f, 272.0f));
    view.Controls().SetRoot(std::move(rootOwner));
    float dpi          = 96.0f;
    const auto prepare = [&] { Hr(view.Prepare(static_cast<UINT>(480.0f * dpi / 96.0f), static_cast<UINT>(320.0f * dpi / 96.0f), dpi), "W2.7 prepare"); };

    size_t compared     = 0u;
    const auto validate = [&](DxUi::TextField& field, std::string_view change)
    {
        view.Controls().SetFocusControl(&field, false);
        prepare(); // paints with whatever the field retained
        const Detail::Geometry retained = Detail::ReadGeometry(view, field);
        view.DebugDropRetainedStateForTest();
        prepare();
        const Detail::Geometry fresh = Detail::ReadGeometry(view, field);
        if (! (retained == fresh))
            std::cerr << "  retained and fresh text field geometry differ after: " << change << '\n';
        Check(retained == fresh, "W2.7: no stale retained text layout is drawn, hit-tested or measured");
        ++compared;
    };
    const auto change = [&](DxUi::TextField& field, std::string_view name, const auto& mutate)
    {
        view.Controls().SetFocusControl(&field, false);
        prepare();
        static_cast<void>(Detail::ReadGeometry(view, field)); // the retained layouts are warm
        mutate();
        validate(field, name);
    };

    for (DxUi::TextField* field : {multiline, single})
    {
        change(*field, "SetText", [&] { field->SetText(field->IsMultiline() ? std::wstring(L"short\nmultiline text") : std::wstring(L"shorter")); });
        change(*field, "typing", [&] { static_cast<void>(view.DispatchCharacter(L'w', 0u)); });
        change(*field, "width", [&] { field->SetBounds(D2D1::RectF(16.0f, field->GetBounds().top, 250.0f, field->GetBounds().bottom)); });
        change(*field, "height", [&] { field->SetBounds(D2D1::RectF(16.0f, field->GetBounds().top, 250.0f, field->GetBounds().bottom - 6.0f)); });
        change(*field, "font role", [&] { field->SetFontRole(DxUi::FontRole::Title); });
        change(*field, "padding", [&] { field->SetHorizontalTextPadding(14.0f, 3.0f); });
        change(*field, "density", [&] { root->SetDensity(root->IsCompactDensity() ? DxUi::Density::Standard : DxUi::Density::Compact); });
        change(*field, "dpi", [&] { dpi = dpi == 96.0f ? 144.0f : 96.0f; });
        change(*field, "flow direction", [&] {
            root->SetFlowDirection(root->IsRightToLeft() ? DxUi::FlowDirection::LeftToRight : DxUi::FlowDirection::RightToLeft);
        });
        change(*field, "mask", [&] { field->SetMasked(true); });
        change(*field, "reveal", [&] { field->SetPasswordRevealState(DxUi::PasswordRevealState::Visible); });
        change(*field, "hide", [&] { field->SetPasswordRevealState(DxUi::PasswordRevealState::Hidden); });
        change(*field, "unmask", [&] { field->SetMasked(false); });
        change(*field,
               "composition preview",
               [&]
        {
            DxUi::EmbeddedTextInputSnapshot snapshot;
            Check(view.ReadTextInput(snapshot) == S_OK, "the focused field exports its text input");
            DxUi::NativeTextInputState state = snapshot.state;
            state.text.insert(state.caretIndex, L"\u6771\u4EAC");
            state.compositionStartIndex = snapshot.state.caretIndex;
            state.compositionEndIndex   = snapshot.state.caretIndex + 2u;
            state.caretIndex            = snapshot.state.caretIndex + 2u;
            state.selectionAnchorIndex.reset();
            Check(view.ApplyTextInput(snapshot.revision, state, DxUi::EmbeddedTextInputAction::Preview) == S_OK, "a composition preview is accepted");
        });
        change(*field, "composition cancel", [&] { view.CancelTextInput(); });
        change(*field,
               "typography revision",
               [&]
        {
            std::shared_ptr<DxUi::GraphicsDevice> replacement;
            Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), replacement), "W2.7 replacement graphics");
            Hr(view.ReplaceDevice(replacement), "W2.7 device replacement");
        });
        change(*field, "long text", [&] { field->SetText(field->IsMultiline() ? ParityMultilineText() : std::wstring(L"Single line field again")); });
    }
    view.Detach();
    std::cout << "PASS text field retained layouts: " << compared << " changes, retained geometry equals fresh geometry after each\n";
}

namespace Detail
{
inline thread_local std::vector<std::wstring>* g_wiped = nullptr;
inline void ObserveWipe(std::wstring_view wiped) noexcept
{
    if (g_wiped)
        g_wiped->emplace_back(wiped);
}
[[nodiscard]] inline bool AllDots(std::wstring_view text) noexcept
{
    return std::ranges::all_of(text, [](wchar_t unit) { return unit == L'\u2022'; });
}
} // namespace Detail

__declspec(noinline) inline void TestMaskedTextFieldWipesItsCopies(GraphicsFixture& gpu)
{
    std::vector<std::wstring> wiped;
    Detail::g_wiped                          = &wiped;
    DxUi::Detail::secureClearObserverForTest = &Detail::ObserveWipe;
    {
        DxUi::EmbeddedHost view;
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "W2.7 wipe graphics");
        Hr(view.Attach(graphics), "W2.7 wipe view");
        auto rootOwner        = std::make_unique<DxUi::Panel>();
        constexpr auto secret = L"correct horse battery";
        auto* field           = rootOwner->AddChild<DxUi::TextField>(secret);
        field->SetMasked(true);
        field->SetBounds(D2D1::RectF(16.0f, 16.0f, 320.0f, 48.0f));
        auto* plain = rootOwner->AddChild<DxUi::TextField>(L"plain words in a field");
        plain->SetBounds(D2D1::RectF(16.0f, 64.0f, 320.0f, 96.0f));
        view.Controls().SetRoot(std::move(rootOwner));
        view.Controls().SetFocusControl(field, false);
        const auto prepare      = [&] { Hr(view.Prepare(400u, 160u), "W2.7 wipe prepare"); };
        const auto anyWipeHolds = [&](std::wstring_view text)
        { return std::ranges::any_of(wiped, [&](const std::wstring& entry) { return entry.find(text) != std::wstring::npos; }); };

        prepare();
        Check(std::ranges::all_of(wiped, [](const std::wstring& entry) { return Detail::AllDots(entry); }),
              "a concealed masked field never holds its plaintext in a layout cache");
        field->SetPasswordRevealState(DxUi::PasswordRevealState::Visible);
        prepare(); // the revealed plaintext is now the single-line layout cache's copy
        wiped.clear();
        for (int frame = 0; frame < 3; ++frame)
        {
            // Another field's change repaints the view; the focused masked field paints from its cache.
            plain->SetText(frame % 2 ? L"plain words in a field" : L"other plain words");
            prepare();
        }
        Check(wiped.empty(), "W2.7: a cache hit neither clears nor copies, and an unmasked field's relayout skips the wipe");

        static_cast<void>(view.DispatchCharacter(L'!', 0u));
        Check(anyWipeHolds(secret), "a masked field's edit wipes the revealed plaintext its layout cache held");
        prepare();
        wiped.clear();
        field->SetMasked(false);
        Check(anyWipeHolds(secret), "unmasking wipes the plaintext the cache took while the field was masked");
        prepare();
        wiped.clear();
        static_cast<void>(view.DispatchCharacter(L'?', 0u));
        prepare();
        Check(wiped.empty(), "W2.7: an unmasked field's edits and relayouts clear its cache without the secure wipe");
        field->SetMasked(true);
        field->SetPasswordRevealState(DxUi::PasswordRevealState::Visible);
        prepare();
        wiped.clear();
        field->SetPasswordRevealState(DxUi::PasswordRevealState::Hidden);
        Check(anyWipeHolds(secret), "hiding a revealed password wipes the plaintext copy");
        prepare();
        field->SetPasswordRevealState(DxUi::PasswordRevealState::Visible);
        prepare();
        wiped.clear();
        view.Controls().SetRoot(nullptr); // destroys both fields
        Check(anyWipeHolds(secret), "destroying a masked field wipes its layout cache's copy");
        view.Detach();
    }
    DxUi::Detail::secureClearObserverForTest = nullptr;
    Detail::g_wiped                          = nullptr;
    std::cout << "PASS masked text field: every layout-cache copy of its plaintext is wiped; unmasked fields and cache hits skip the wipe\n";
}

__declspec(noinline) inline void TestTextFieldParity(GraphicsFixture& gpu)
{
    const auto make = [&] { return std::make_unique<EmbeddedParity::EmbeddedParityScene>(gpu.device.get()); };
    IncrementalParity::RunParityScript(TextFieldScript<EmbeddedParity::EmbeddedParityScene>(), make, "embedded", EmbeddedParity::kReplay);
}

inline void Run(GraphicsFixture& gpu)
{
    TestFx1TextFieldPaintGate();
    TestTextFieldCompositionStateCopies();
    TestTextFieldStaleLayoutIsNeverUsed(gpu);
    TestMaskedTextFieldWipesItsCopies(gpu);
    TestTextFieldParity(gpu);
}
} // namespace TextFieldPaintTests
