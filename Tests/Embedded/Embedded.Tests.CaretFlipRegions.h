#pragma once
// A caret blink flip repaints only the caret's ink (plan item W4.7) on the embedded host. Included once from
// Embedded.Tests.Embedded.cpp after the paint-region tests; uses Check and Hr from there, and
// `DxUi.EmbeddedTests.exe --test-caret-flip-regions` runs it alone.
//
// - TestFx1CaretFlipPaintsOnlyTheCaret: FX1's editable and multiline phases (the fixture's own definition): a frame that
//   follows a blink flip paints one region rectangle of at most the caret's ink bounds + 2 px. The ink is measured, not
//   computed by the control: the bounding box of the pixels that differ between the frame before the flip and the frame after.
// - TestCaretFlipsInEveryEditorAndState: a TextField (single line, multiline, password), an editable ComboBox and a
//   RichTextEditor, each with the caret at the end, at the start, in the middle, on an empty line, at a line end, scrolled,
//   with a selection, in right-to-left text and flow, and with an IME composition, at 96, 144 and 192 DPI and at 150.5 DPI:
//   four flips each, every one a region of the caret's ink. The frame verifier of the test program repaints every one of these
//   frames whole and compares exactly, so a region that is too small fails at once with the differing pixels.
// - TestCaretFlipOracleScripts: the incremental-versus-fresh oracle (RunParityScript) over a script of flips between edits.
// - TestEverythingThatMovesTheCaretStillRepaintsTheControl: typing, selection, scroll, focus change and a layout change repaint
//   the control's extent (more than the caret), as before.
// - TestCaretFlipFallsBackWhenTheInkIsUnknown: an unfocused-then-focused editor before its first paint, and an open ComboBox
//   popup, repaint the control's extent.
// Set DXUI_CARET_FLIP_REPORT=1 to print an unmet bound instead of failing (the measurement before the change).
#include "Support.Tests.PaintRegions.h"

#include <cstring>

namespace CaretFlipRegionTests
{
[[nodiscard]] inline bool ReportOnly() noexcept
{
    static const bool report = GetEnvironmentVariableW(L"DXUI_CARET_FLIP_REPORT", nullptr, 0u) != 0u;
    return report;
}
inline uint64_t g_unmet = 0u;
inline void Expect(bool ok, const char* text)
{
    if (ReportOnly())
    {
        if (! ok)
        {
            ++g_unmet;
            std::cout << "    NOT MET: " << text << '\n';
        }
        return;
    }
    Check(ok, text);
}

// The bounding box of the device pixels that differ between two captures.
struct Box final
{
    LONG left   = LONG_MAX;
    LONG top    = LONG_MAX;
    LONG right  = 0; // exclusive
    LONG bottom = 0;
    [[nodiscard]] bool Empty() const noexcept
    {
        return right <= left || bottom <= top;
    }
    [[nodiscard]] uint64_t Width() const noexcept
    {
        return Empty() ? 0u : uint64_t(right - left);
    }
    [[nodiscard]] uint64_t Height() const noexcept
    {
        return Empty() ? 0u : uint64_t(bottom - top);
    }
};
[[nodiscard]] inline Box Difference(const DxUi::WindowHostBitmapCapture& a, const DxUi::WindowHostBitmapCapture& b)
{
    Check(a.widthPx == b.widthPx && a.heightPx == b.heightPx && a.bgraPixels.size() == b.bgraPixels.size() &&
              a.bgraPixels.size() >= size_t(a.widthPx) * a.heightPx * 4u,
          "W4.7: two captures of one surface have one size");
    Box box;
    for (UINT y = 0u; y < a.heightPx; ++y)
    {
        const uint8_t* const rowA = a.bgraPixels.data() + size_t(y) * a.widthPx * 4u;
        const uint8_t* const rowB = b.bgraPixels.data() + size_t(y) * a.widthPx * 4u;
        if (std::memcmp(rowA, rowB, size_t(a.widthPx) * 4u) == 0)
            continue;
        for (UINT x = 0u; x < a.widthPx; ++x)
            if (std::memcmp(rowA + size_t(x) * 4u, rowB + size_t(x) * 4u, 4u) != 0)
            {
                box.left   = (std::min)(box.left, LONG(x));
                box.right  = (std::max)(box.right, LONG(x) + 1);
                box.top    = (std::min)(box.top, LONG(y));
                box.bottom = (std::max)(box.bottom, LONG(y) + 1);
            }
    }
    return box;
}

// What a frame painted and the ink it changed.
struct FlipFrame final
{
    uint64_t pixels     = 0u; // frame.pixelsPainted
    uint64_t rects      = 0u; // frame.paintRegionRects
    uint64_t whole      = 0u; // frame.paintRegionFull
    uint64_t executions = 0u;
    Box ink;
};
// The bound of the gate: the ink's bounding box grown by 2 px in each dimension.
[[nodiscard]] inline uint64_t Allowed(const Box& ink) noexcept
{
    return (ink.Width() + 2u) * (ink.Height() + 2u);
}
[[nodiscard]] inline FlipFrame RenderAndCompare(PerformanceFixtures::EmbeddedInstance& instance, GraphicsFixture& gpu, DxUi::WindowHostBitmapCapture& surface)
{
    FlipFrame result;
    DxUi::WindowHostBitmapCapture before = std::move(surface);
    const DxUi::Detail::WorkCounterScope scope;
    Hr(instance.Frame(gpu), "W4.7 flip frame");
    const DxUi::Detail::WorkCounters work = scope.Delta();
    surface                               = {};
    Hr(instance.View().DebugReadSurfaceForTest(surface), "W4.7 surface read");
    result.pixels     = work.frame.pixelsPainted;
    result.rects      = work.frame.paintRegionRects;
    result.whole      = work.frame.paintRegionFull;
    result.executions = work.frame.paintExecutions;
    result.ink        = before.bgraPixels.empty() ? Box{} : Difference(before, surface);
    return result;
}

// ------------------------------------------------------------------------------------------------------------------
// FX1
// ------------------------------------------------------------------------------------------------------------------
__declspec(noinline) inline void TestFx1CaretFlipPaintsOnlyTheCaret()
{
    using namespace PerformanceFixtures;
    const Definition& definition = Fx1::kDefinition;
    const Variant& variant       = Fx1::kVariants[0];
    GraphicsFixture gpu;
    gpu.width  = variant.width;
    gpu.height = variant.height;
    Hr(gpu.Create(true), "W4.7 FX1 device");
    EmbeddedInstance instance;
    instance.Instantiate(definition);
    Hr(instance.Create(gpu.device.get(), variant), "W4.7 FX1 build");
    DxUi::WindowHostBitmapCapture surface;
    uint64_t flips = 0u, wholeFrames = 0u, totalPixels = 0u, maxPixels = 0u, maxAllowed = 0u, totalAllowed = 0u;
    const auto controls = [&]() -> DxUi::ControlHost& { return instance.View().Controls(); };
    // Phases 0 (warm), 1 (editable single line), 2 (read-only: steady caret) and 3 (64 Ki-unit multiline).
    for (size_t phase = 0u; phase <= Fx1::kMultiline; ++phase)
    {
        uint64_t phaseFlips = 0u, phasePixels = 0u, phaseAllowed = 0u, phaseMax = 0u;
        for (size_t step = 0u; step < definition.phases[phase].steps; ++step)
        {
            const bool shown = controls().DebugIsCaretClockVisibleForTest();
            instance.Step(phase, step);
            const bool flipped    = shown != controls().DebugIsCaretClockVisibleForTest() && step != 0u;
            const FlipFrame frame = RenderAndCompare(instance, gpu, surface);
            if (phase == Fx1::kWarm || ! flipped)
                continue;
            ++phaseFlips;
            phasePixels += frame.pixels;
            phaseMax = (std::max)(phaseMax, frame.pixels);
            phaseAllowed += Allowed(frame.ink);
            std::cout << std::format(
                "    FX1 {} step {}: caret {} -> {} paint region rectangle(s) {}, {} pixels painted, ink {}x{} px, bound {} px, Paint executions {}\n",
                definition.phases[phase].name,
                step,
                shown ? "shown" : "hidden",
                shown ? "hidden" : "shown",
                frame.rects,
                frame.pixels,
                frame.ink.Width(),
                frame.ink.Height(),
                Allowed(frame.ink),
                frame.executions);
            wholeFrames += frame.whole;
            Expect(frame.whole == 0u && frame.rects == 1u, "W4.7: an FX1 caret flip repaints one region rectangle, not the whole view");
            Expect(! frame.ink.Empty(), "W4.7: an FX1 caret flip changes pixels (the measurement sees the caret)");
            Expect(frame.pixels <= Allowed(frame.ink), "W4.7 (gate): pixels written per FX1 caret flip <= the caret's ink bounds + 2 px");
        }
        flips += phaseFlips;
        totalPixels += phasePixels;
        totalAllowed += phaseAllowed;
        maxPixels  = (std::max)(maxPixels, phaseMax);
        maxAllowed = (std::max)(maxAllowed, phaseAllowed);
        if (phase != Fx1::kWarm)
            std::cout << std::format("  FX1 {}: {} caret flips, {} pixels painted in all ({} on average, {} at most), the bound sums to {}\n",
                                     definition.phases[phase].name,
                                     phaseFlips,
                                     phasePixels,
                                     phaseFlips ? phasePixels / phaseFlips : 0u,
                                     phaseMax,
                                     phaseAllowed);
    }
    Expect(flips >= 6u, "W4.7: FX1 flips its carets");
    std::cout << std::format("PASS W4.7 FX1: {} caret flips, {} pixels painted in all against a bound of {} ({} whole-view repaints)\n",
                             flips,
                             totalPixels,
                             totalAllowed,
                             wholeFrames);
}

// ------------------------------------------------------------------------------------------------------------------
// Every editor and state
// ------------------------------------------------------------------------------------------------------------------
enum class Kind : uint8_t
{
    SingleLine,
    Password,
    Multiline,
    Combo,
    Rich,
};
struct CaseSpec final
{
    const char* name;
    Kind kind;
    const wchar_t* text;
    const wchar_t* placeholder; // single line and combo
    bool rtlFlow;
    bool markdown; // rich: rendered Markdown; false: plain source
    void (*arrange)(PerformanceFixtures::EmbeddedInstance& instance, DxUi::Control& editor);
};

inline const CaseSpec* g_spec = nullptr;
class CaretScene final : public PerformanceFixtures::Scenario
{
public:
    explicit CaretScene(const CaseSpec& spec) : spec_(spec)
    {
    }
    CaretScene(const CaretScene&)            = delete;
    CaretScene& operator=(const CaretScene&) = delete;
    CaretScene(CaretScene&&)                 = delete;
    CaretScene& operator=(CaretScene&&)      = delete;
    ~CaretScene() override                   = default;
    HRESULT Build(PerformanceFixtures::Context& context) override
    {
        context.controls.DebugSetCaretBlinkSettingsForTest(DxUi::ControlHost::CaretBlinkSettingsForTest{530u, 0u});
        auto root = std::make_unique<DxUi::Panel>();
        switch (spec_.kind)
        {
            case Kind::SingleLine:
            case Kind::Password:
            case Kind::Multiline:
            {
                auto* field = root->AddChild<DxUi::TextField>();
                if (spec_.kind == Kind::Multiline)
                    field->SetMultiline(true);
                if (spec_.kind == Kind::Password)
                    field->SetMasked(true);
                field->SetText(spec_.text);
                if (spec_.placeholder)
                    field->SetPlaceholder(spec_.placeholder);
                field->SetBounds(spec_.kind == Kind::Multiline ? D2D1::RectF(16.0f, 16.0f, 216.0f, 96.0f) : D2D1::RectF(16.0f, 16.0f, 216.0f, 48.0f));
                editor_ = field;
                break;
            }
            case Kind::Combo:
            {
                auto* combo = root->AddChild<DxUi::ComboBox>();
                combo->SetEditable(true);
                combo->SetText(spec_.text);
                if (spec_.placeholder)
                    combo->SetPlaceholder(spec_.placeholder);
                combo->SetBounds(D2D1::RectF(16.0f, 16.0f, 236.0f, 48.0f));
                editor_ = combo;
                break;
            }
            case Kind::Rich:
            {
                auto* rich = root->AddChild<DxUi::RichTextEditor>();
                if (spec_.markdown)
                {
                    rich->SetMarkdownAnalysisProfile(DxUi::RichText::MarkdownProfile::RichMarkdown);
                    rich->SetMarkdownRenderingEnabled(true);
                }
                rich->SetText(spec_.text);
                rich->SetBounds(D2D1::RectF(16.0f, 16.0f, 296.0f, 136.0f));
                editor_ = rich;
                break;
            }
        }
        if (spec_.rtlFlow)
            editor_->SetFlowDirection(DxUi::FlowDirection::RightToLeft);
        context.controls.SetRoot(std::move(root));
        return S_OK;
    }
    // Step 0 focuses the editor: after a first frame, so the host has the area its caret blinks in.
    void Step(PerformanceFixtures::Context& context, size_t, size_t) override
    {
        context.controls.SetFocusControl(editor_, false);
    }
    [[nodiscard]] DxUi::Control& Editor() const noexcept
    {
        return *editor_;
    }

private:
    const CaseSpec& spec_;
    DxUi::Control* editor_ = nullptr;
};

// The fixture framework builds a scene through a plain function pointer: the case it builds is the one last made here.
[[nodiscard]] inline std::unique_ptr<PerformanceFixtures::Scenario> CreateScene()
{
    return std::make_unique<CaretScene>(*g_spec);
}
[[nodiscard]] inline PerformanceFixtures::Definition MakeDefinition(const CaseSpec& spec)
{
    g_spec = &spec;
    PerformanceFixtures::Definition definition{};
    definition.id            = "W4.7";
    definition.title         = "Caret flip";
    definition.reducedMotion = false; // a caret blinks only with motion on
    definition.create        = &CreateScene;
    return definition;
}

// One frame (the host learns its area), then the scene focuses its editor.
inline void FocusEditor(PerformanceFixtures::EmbeddedInstance& instance, GraphicsFixture& gpu)
{
    Hr(instance.Frame(gpu), "W4.7 first frame");
    instance.Step(0u, 0u);
}

inline void Press(PerformanceFixtures::EmbeddedInstance& instance, UINT key, UINT modifiers = 0u)
{
    static_cast<void>(instance.Key(key, true, modifiers));
    static_cast<void>(instance.Key(key, false, modifiers));
}
inline void Type(PerformanceFixtures::EmbeddedInstance& instance, std::wstring_view text)
{
    for (const wchar_t character : text)
        static_cast<void>(instance.Character(character, 0u));
}
// An IME preview: `composition` replaces the text after `prefixLength` units, as a composition string in progress.
inline void Compose(PerformanceFixtures::EmbeddedInstance& instance, std::wstring_view composition)
{
    DxUi::EmbeddedTextInputSnapshot snapshot;
    Hr(instance.View().ReadTextInput(snapshot), "W4.7 read text input");
    DxUi::NativeTextInputState next = snapshot.state;
    const size_t start              = next.caretIndex;
    next.text.insert(start, composition);
    next.caretIndex = start + composition.size();
    next.selectionAnchorIndex.reset();
    next.compositionStartIndex       = start;
    next.compositionEndIndex         = start + composition.size();
    next.conversionTargetStartIndex  = start;
    next.conversionTargetEndIndex    = start + composition.size();
    next.compositionCursorIndex      = start + composition.size();
    next.compositionClauseBoundaries = {start, start + composition.size()};
    Hr(instance.View().ApplyTextInput(snapshot.revision, next, DxUi::EmbeddedTextInputAction::Preview), "W4.7 IME preview");
}

inline void ArrangeEnd(PerformanceFixtures::EmbeddedInstance& instance, DxUi::Control&)
{
    Press(instance, VK_END, MK_CONTROL);
}
inline void ArrangeStart(PerformanceFixtures::EmbeddedInstance& instance, DxUi::Control&)
{
    Press(instance, VK_HOME, MK_CONTROL);
}
inline void ArrangeMiddle(PerformanceFixtures::EmbeddedInstance& instance, DxUi::Control&)
{
    Press(instance, VK_HOME, MK_CONTROL);
    for (int index = 0; index < 5; ++index)
        Press(instance, VK_RIGHT);
}
inline void ArrangeSelection(PerformanceFixtures::EmbeddedInstance& instance, DxUi::Control&)
{
    Press(instance, VK_END, MK_CONTROL);
    for (int index = 0; index < 4; ++index)
        Press(instance, VK_LEFT, MK_SHIFT);
}
inline void ArrangeEndOfFirstLine(PerformanceFixtures::EmbeddedInstance& instance, DxUi::Control&)
{
    Press(instance, VK_HOME, MK_CONTROL);
    Press(instance, VK_END);
}
inline void ArrangeEmptyLine(PerformanceFixtures::EmbeddedInstance& instance, DxUi::Control&)
{
    // "first\n\nthird": the empty second line.
    Press(instance, VK_HOME, MK_CONTROL);
    Press(instance, VK_DOWN);
}
inline void ArrangeTyped(PerformanceFixtures::EmbeddedInstance& instance, DxUi::Control&)
{
    Press(instance, VK_END, MK_CONTROL);
    Type(instance, L"ab");
}
inline void ArrangeComposition(PerformanceFixtures::EmbeddedInstance& instance, DxUi::Control&)
{
    Press(instance, VK_END, MK_CONTROL);
    Compose(instance, L"東京");
}
inline void ArrangeNothing(PerformanceFixtures::EmbeddedInstance&, DxUi::Control&)
{
}

inline const std::array<CaseSpec, 25u> kCases{{
    {"textfield-end", Kind::SingleLine, L"Caret at the end", nullptr, false, false, &ArrangeEnd},
    {"textfield-start", Kind::SingleLine, L"Caret at the start", nullptr, false, false, &ArrangeStart},
    {"textfield-middle", Kind::SingleLine, L"Caret in the middle", nullptr, false, false, &ArrangeMiddle},
    {"textfield-empty", Kind::SingleLine, L"", nullptr, false, false, &ArrangeNothing},
    {"textfield-placeholder", Kind::SingleLine, L"", L"Placeholder", false, false, &ArrangeNothing},
    {"textfield-selection", Kind::SingleLine, L"A selection ends at the caret", nullptr, false, false, &ArrangeSelection},
    {"textfield-scrolled",
     Kind::SingleLine,
     L"A long line of text that scrolls the field far past its right edge and keeps going",
     nullptr,
     false,
     false,
     &ArrangeEnd},
    {"textfield-hebrew-end", Kind::SingleLine, L"שלום עולם", nullptr, false, false, &ArrangeEnd},
    {"textfield-hebrew-start", Kind::SingleLine, L"שלום עולם", nullptr, false, false, &ArrangeStart},
    {"textfield-mixed-middle", Kind::SingleLine, L"abc שלום def", nullptr, false, false, &ArrangeMiddle},
    {"textfield-rtl-flow", Kind::SingleLine, L"Right-to-left flow", nullptr, true, false, &ArrangeEnd},
    {"textfield-composition", Kind::SingleLine, L"Typed ", nullptr, false, false, &ArrangeComposition},
    {"textfield-password", Kind::Password, L"secret", nullptr, false, false, &ArrangeEnd},
    {"multiline-line-end", Kind::Multiline, L"first line\nsecond line\nthird line", nullptr, false, false, &ArrangeEndOfFirstLine},
    {"multiline-empty-line", Kind::Multiline, L"first\n\nthird", nullptr, false, false, &ArrangeEmptyLine},
    {"multiline-scrolled", Kind::Multiline, L"one\ntwo\nthree\nfour\nfive\nsix\nseven\neight\nnine\nten\neleven\ntwelve", nullptr, false, false, &ArrangeEnd},
    {"multiline-rtl-flow", Kind::Multiline, L"first\nsecond", nullptr, true, false, &ArrangeEnd},
    {"combobox-end", Kind::Combo, L"Editable combo box", nullptr, false, false, &ArrangeEnd},
    {"combobox-placeholder", Kind::Combo, L"", L"Placeholder", false, false, &ArrangeNothing},
    {"combobox-hebrew", Kind::Combo, L"שלום", nullptr, false, false, &ArrangeEnd},
    {"rich-source-end", Kind::Rich, L"Plain **source** text\nsecond line", nullptr, false, false, &ArrangeEnd},
    {"rich-source-empty", Kind::Rich, L"", nullptr, false, false, &ArrangeNothing},
    {"rich-source-typed", Kind::Rich, L"Plain source text", nullptr, false, false, &ArrangeTyped},
    {"rich-markdown-start", Kind::Rich, L"# Heading\n\nSome **bold** text.\n\n- item\n", nullptr, false, true, &ArrangeStart},
    {"rich-markdown-hebrew", Kind::Rich, L"שלום עולם\n\nEnglish", nullptr, false, true, &ArrangeStart},
}};

// Four DPIs: 100 %, 150 %, 200 % and a fractional one.
inline constexpr std::array<float, 4u> kDpis{96.0f, 144.0f, 192.0f, 150.5f};

[[nodiscard]] inline Box RunCase(const CaseSpec& spec, float dpi, GraphicsFixture& gpu, uint64_t& flips)
{
    using namespace PerformanceFixtures;
    const UINT width  = static_cast<UINT>(std::ceil(320.0f * dpi / 96.0f));
    const UINT height = static_cast<UINT>(std::ceil(160.0f * dpi / 96.0f));
    const Variant variant{"caret-flip", width, height, dpi, false, false};
    const Definition definition = MakeDefinition(spec);
    EmbeddedInstance instance;
    instance.Instantiate(definition);
    Hr(instance.Create(gpu.device.get(), variant), "W4.7 case build");
    FocusEditor(instance, gpu);
    auto& scene = static_cast<CaretScene&>(instance.GetScenario());
    spec.arrange(instance, scene.Editor());
    // Settle: preparation turns until nothing is pending, then one more frame so the caret's paint is current.
    DxUi::WindowHostBitmapCapture surface;
    for (size_t turn = 0u; turn < 256u && (turn == 0u || instance.View().NeedsPreparation()); ++turn)
        Hr(instance.Frame(gpu), "W4.7 case settle");
    Hr(instance.Frame(gpu), "W4.7 case settled frame");
    Hr(instance.View().DebugReadSurfaceForTest(surface), "W4.7 case surface");
    auto& controls = instance.View().Controls();
    Box widest;
    for (int flip = 0; flip < 4; ++flip)
    {
        const bool shown = controls.DebugIsCaretClockVisibleForTest();
        for (int pass = 0; pass < 40 && controls.DebugIsCaretClockVisibleForTest() == shown; ++pass)
            instance.AdvanceTime(100u); // steps of 100 ms, as FX1 plays
        if (controls.DebugIsCaretClockVisibleForTest() == shown)
        {
            const auto request = instance.View().GetAnimationRequest();
            std::cerr << std::format("  case {} @{} DPI: no flip; request kind {} due {} now {} focus {} visible {}\n",
                                     spec.name,
                                     dpi,
                                     static_cast<int>(request.kind),
                                     request.dueTickMs,
                                     instance.NowMs(),
                                     controls.GetFocusControl() != nullptr,
                                     shown);
        }
        Check(controls.DebugIsCaretClockVisibleForTest() != shown, "W4.7: the caret clock flips within a blink phase of fixture time");
        const FlipFrame frame = RenderAndCompare(instance, gpu, surface);
        ++flips;
        std::cout << std::format("    {} @{} DPI flip {} ({}): {} region rectangle(s), {} pixels painted, ink {}x{} px, bound {} px\n",
                                 spec.name,
                                 dpi,
                                 flip,
                                 shown ? "hide" : "show",
                                 frame.rects,
                                 frame.pixels,
                                 frame.ink.Width(),
                                 frame.ink.Height(),
                                 Allowed(frame.ink));
        Expect(frame.whole == 0u, "W4.7: a caret flip does not repaint the whole view");
        Expect(frame.rects == 1u, "W4.7: a caret flip repaints one region rectangle");
        Expect(! frame.ink.Empty(), "W4.7: a caret flip changes pixels (the caret is on screen in this state)");
        Expect(frame.pixels <= Allowed(frame.ink), "W4.7: a caret flip paints at most the caret's ink bounds + 2 px");
        if (frame.ink.Width() * frame.ink.Height() > widest.Width() * widest.Height())
            widest = frame.ink;
    }
    return widest;
}

__declspec(noinline) inline void TestCaretFlipsInEveryEditorAndState()
{
    uint64_t flips = 0u;
    for (const float dpi : kDpis)
    {
        GraphicsFixture gpu;
        gpu.width  = static_cast<UINT>(std::ceil(320.0f * dpi / 96.0f));
        gpu.height = static_cast<UINT>(std::ceil(160.0f * dpi / 96.0f));
        Hr(gpu.Create(true), "W4.7 case device");
        for (const CaseSpec& spec : kCases)
            static_cast<void>(RunCase(spec, dpi, gpu, flips));
    }
    std::cout << std::format("PASS W4.7 every editor and state: {} caret flips at 4 scales, each a region of the caret's ink\n", flips);
}

// ------------------------------------------------------------------------------------------------------------------
// What still repaints the control
// ------------------------------------------------------------------------------------------------------------------
__declspec(noinline) inline void TestEverythingThatMovesTheCaretStillRepaintsTheControl()
{
    using namespace PerformanceFixtures;
    GraphicsFixture gpu;
    gpu.width  = 320u;
    gpu.height = 160u;
    Hr(gpu.Create(true), "W4.7 moves device");
    const CaseSpec& spec        = kCases[0];
    const Definition definition = MakeDefinition(spec);
    EmbeddedInstance instance;
    instance.Instantiate(definition);
    const Variant variant{"caret-flip", 320u, 160u, 96.0f, false, false};
    Hr(instance.Create(gpu.device.get(), variant), "W4.7 moves build");
    FocusEditor(instance, gpu);
    DxUi::WindowHostBitmapCapture surface;
    Hr(instance.Frame(gpu), "W4.7 moves first frame");
    Hr(instance.Frame(gpu), "W4.7 moves second frame");
    Hr(instance.View().DebugReadSurfaceForTest(surface), "W4.7 moves surface");
    const D2D1_RECT_F bounds = static_cast<CaretScene&>(instance.GetScenario()).Editor().GetBounds();
    // The field's own extent: its bounds plus the library reach, in pixels at 96 DPI.
    const uint64_t extentPixels = uint64_t(bounds.right - bounds.left + 6.0f) * uint64_t(bounds.bottom - bounds.top + 6.0f);
    const auto repaints         = [&](const char* what, auto&& action)
    {
        action();
        const FlipFrame frame = RenderAndCompare(instance, gpu, surface);
        std::cout << std::format(
                         "    {}: {} pixels painted in {} region rectangle(s) (the control's extent is {} px)\n", what, frame.pixels, frame.rects, extentPixels)
                  << std::flush;
        Check(frame.whole == 1u || frame.rects == 1u, "W4.7: a change that moves the caret repaints a region rectangle or the whole view");
        Check(frame.pixels >= uint64_t(bounds.right - bounds.left) * uint64_t(bounds.bottom - bounds.top),
              "W4.7: a change that moves the caret repaints the control's extent, not the caret's ink");
    };
    repaints("typing", [&] { Type(instance, L"x"); });
    repaints("selection", [&] { Press(instance, VK_LEFT, MK_SHIFT); });
    repaints("caret movement", [&] { Press(instance, VK_HOME); });
    repaints("layout (bounds change)", [&] { static_cast<CaretScene&>(instance.GetScenario()).Editor().SetBounds(D2D1::RectF(16.0f, 16.0f, 226.0f, 48.0f)); });
    repaints("focus change", [&] { instance.View().Controls().SetFocusControl(nullptr, false); });
    repaints("focus returns", [&] { instance.View().Controls().SetFocusControl(&static_cast<CaretScene&>(instance.GetScenario()).Editor(), false); });
    std::cout << "PASS W4.7 typing, selection, movement, layout and focus changes still repaint the control's extent\n";
}

// ------------------------------------------------------------------------------------------------------------------
// Fallbacks
// ------------------------------------------------------------------------------------------------------------------
__declspec(noinline) inline void TestCaretFlipFallsBackWhenTheInkIsUnknown()
{
    using namespace PerformanceFixtures;
    GraphicsFixture gpu;
    gpu.width  = 320u;
    gpu.height = 160u;
    Hr(gpu.Create(true), "W4.7 fallback device");
    // An open ComboBox popup: its extent is unbounded, so a flip repaints the whole view.
    {
        const CaseSpec& spec        = kCases[17];
        const Definition definition = MakeDefinition(spec);
        EmbeddedInstance instance;
        instance.Instantiate(definition);
        const Variant variant{"caret-flip", 320u, 160u, 96.0f, false, false};
        Hr(instance.Create(gpu.device.get(), variant), "W4.7 popup build");
        FocusEditor(instance, gpu);
        auto& combo = static_cast<DxUi::ComboBox&>(static_cast<CaretScene&>(instance.GetScenario()).Editor());
        combo.SetItems({{L"1", L"One"}, {L"2", L"Two"}});
        Hr(instance.Frame(gpu), "W4.7 popup frame");
        Press(instance, VK_F4);
        Hr(instance.Frame(gpu), "W4.7 popup open frame");
        Hr(instance.Frame(gpu), "W4.7 popup open second frame");
        DxUi::WindowHostBitmapCapture surface;
        Hr(instance.View().DebugReadSurfaceForTest(surface), "W4.7 popup surface");
        auto& controls   = instance.View().Controls();
        const bool shown = controls.DebugIsCaretClockVisibleForTest();
        for (int pass = 0; pass < 40 && controls.DebugIsCaretClockVisibleForTest() == shown; ++pass)
            instance.AdvanceTime(100u); // steps of 100 ms, as FX1 plays
        const FlipFrame frame = RenderAndCompare(instance, gpu, surface);
        std::cout << std::format("    open popup flip: {} pixels painted, {} whole repaint(s)\n", frame.pixels, frame.whole);
        Check(controls.DebugIsCaretClockVisibleForTest() != shown, "W4.7: the caret of a combo box with an open popup still blinks");
        Check(frame.whole == 1u, "W4.7: a flip of a combo box whose popup is open repaints the whole view (its extent is unbounded)");
    }
    std::cout << "PASS W4.7 a flip falls back to the control's extent where the ink is unknown\n";
}

// W0.5: the incremental scene paints each flip as a region of the caret's ink, the fresh one whole; 0 differences.
__declspec(noinline) inline void TestCaretFlipOracleScripts(GraphicsFixture& gpu)
{
    using EmbeddedParity::EmbeddedParityScene;
    const auto make                   = [&] { return std::make_unique<EmbeddedParityScene>(gpu.device.get()); };
    constexpr std::string_view replay = "DxUi.EmbeddedTests.exe --test-caret-flip-regions";
    const DxUi::Detail::WorkCounterScope scope;
    RunParityScript(PaintRegionScript::CaretFlipScript<EmbeddedParityScene>(), make, "embedded", replay);
    RunParityScript(PaintRegionScript::CaretFlipDpiScript<150u, EmbeddedParityScene>(), make, "embedded 150 DPI", replay);
    RunParityScript(PaintRegionScript::CaretFlipDpiScript<192u, EmbeddedParityScene>(), make, "embedded 192 DPI", replay);
    const DxUi::Detail::WorkCounters work = scope.Delta();
    std::cout << "  caret flips oracle: " << work.frame.paintRegionRects << " region rectangles, " << work.frame.paintRegionFull << " whole repaints\n";
    Check(work.frame.paintRegionRects > 0u, "W4.7: the caret-flip oracle compares region frames");
    std::cout << "PASS W4.7 caret flips oracle: 0 differences after every partial frame\n";
}

inline void Run(GraphicsFixture& gpu)
{
    TestFx1CaretFlipPaintsOnlyTheCaret();
    TestCaretFlipsInEveryEditorAndState();
    TestCaretFlipOracleScripts(gpu);
    TestEverythingThatMovesTheCaretStillRepaintsTheControl();
    TestCaretFlipFallsBackWhenTheInkIsUnknown();
    if (ReportOnly())
        std::cout << "W4.7 report: " << g_unmet << " bound(s) not met\n";
}
} // namespace CaretFlipRegionTests
