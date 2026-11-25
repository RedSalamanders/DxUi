#pragma once
// Paint regions and culling (plan item W4.10): the "regions" script of the incremental-versus-fresh oracle, shared by the
// embedded suite (Tests/Embedded/Embedded.Tests.PaintRegions.h), where every incremental frame after the first repaints only
// its paint region, and the native suite (Tests/Controls/Controls.Tests.IncrementalParity.cpp), where frames stay whole and
// children are culled. Include after Support.Tests.IncrementalParity.h.
//
// The scene holds what an attributed invalidation must get right: cards of library controls (hover, press, focus rings that
// appear and leave), a Label whose text escapes its bounds and stops escaping, a consumer Button that paints a halo 10 DIP
// outside its bounds without declaring it, the same halo declared through GetPaintOverhang, a consumer panel that translates
// its children without declaring it, and a ScrollPanel of rows that scrolls under the pointer. Steps move controls by whole
// and fractional DIPs, change text and visibility, open and close the ComboBox popup and switch the theme.
#include <DxUi/DxUi.h>

#include <array>
#include <format>
#include <memory>

namespace PaintRegionScript
{
using namespace IncrementalParity;

// A Button that strokes a halo 10 DIP outside its bounds while its halo is on, and asks for a repaint only through its own
// invalidation, as an application control might. It does not declare its painted extent: its reach is never trusted.
class HaloButton : public DxUi::Button
{
public:
    explicit HaloButton(std::wstring text) : DxUi::Button(std::move(text))
    {
    }
    HaloButton(const HaloButton&)            = delete;
    HaloButton& operator=(const HaloButton&) = delete;
    HaloButton(HaloButton&&)                 = delete;
    HaloButton& operator=(HaloButton&&)      = delete;
    ~HaloButton() override                   = default;
    static constexpr float kHaloDip          = 10.0f;
    void SetHalo(bool halo)
    {
        if (_halo == halo)
            return;
        _halo = halo;
        RequestInvalidate();
    }
    void Paint(DxUi::ControlHost& host) const override
    {
        DxUi::Button::Paint(host);
        if (! _halo)
            return;
        auto* dc    = host.GetDeviceContext();
        auto* brush = host.GetSolidBrush(D2D1::ColorF(0.85f, 0.2f, 0.1f, 0.8f));
        if (! dc || ! brush)
            return;
        const D2D1_RECT_F bounds = GetBounds();
        dc->DrawRectangle(
            D2D1::RectF(bounds.left - kHaloDip + 1.0f, bounds.top - kHaloDip + 1.0f, bounds.right + kHaloDip - 1.0f, bounds.bottom + kHaloDip - 1.0f),
            brush,
            2.0f);
    }

private:
    bool _halo = false;
};

// The same halo, declared: its own constructor declares and its reach covers the halo.
class DeclaredHaloButton final : public HaloButton
{
public:
    explicit DeclaredHaloButton(std::wstring text) : HaloButton(std::move(text))
    {
        DeclarePaintOverhang();
    }
    DeclaredHaloButton(const DeclaredHaloButton&)            = delete;
    DeclaredHaloButton& operator=(const DeclaredHaloButton&) = delete;
    DeclaredHaloButton(DeclaredHaloButton&&)                 = delete;
    DeclaredHaloButton& operator=(DeclaredHaloButton&&)      = delete;
    ~DeclaredHaloButton() override                           = default;

protected:
    [[nodiscard]] DxUi::PaintOverhang GetPaintOverhang(const DxUi::ControlHost&) const noexcept override
    {
        return {kHaloDip, kHaloDip, kHaloDip, kHaloDip};
    }
};
// A declared control whose ink fills its whole extent: its bounds plus a 2 DIP reach, to the extent's last pixel.
class EdgeInk final : public DxUi::Control
{
public:
    static constexpr float kReachDip = 2.0f;
    EdgeInk()
    {
        DeclarePaintOverhang();
    }
    EdgeInk(const EdgeInk&)            = delete;
    EdgeInk& operator=(const EdgeInk&) = delete;
    EdgeInk(EdgeInk&&)                 = delete;
    EdgeInk& operator=(EdgeInk&&)      = delete;
    ~EdgeInk() override                = default;
    void Paint(DxUi::ControlHost& host) const override
    {
        auto* dc    = host.GetDeviceContext();
        auto* brush = host.GetSolidBrush(D2D1::ColorF(0.1f, 0.4f, 0.8f, 1.0f));
        if (! dc || ! brush)
            return;
        const D2D1_RECT_F b = GetBounds();
        dc->FillRectangle(D2D1::RectF(b.left - kReachDip, b.top - kReachDip, b.right + kReachDip, b.bottom + kReachDip), brush);
    }

protected:
    [[nodiscard]] DxUi::PaintOverhang GetPaintOverhang(const DxUi::ControlHost&) const noexcept override
    {
        return {kReachDip, kReachDip, kReachDip, kReachDip};
    }
};

// A declared control with no reach that fills its bounds in one of two colours and invalidates itself when it flips.
class Patch final : public DxUi::Control
{
public:
    Patch()
    {
        DeclarePaintOverhang();
    }
    Patch(const Patch&)            = delete;
    Patch& operator=(const Patch&) = delete;
    Patch(Patch&&)                 = delete;
    Patch& operator=(Patch&&)      = delete;
    ~Patch() override              = default;
    void Flip()
    {
        _on = ! _on;
        RequestInvalidate();
    }
    void Paint(DxUi::ControlHost& host) const override
    {
        auto* dc    = host.GetDeviceContext();
        auto* brush = host.GetSolidBrush(_on ? D2D1::ColorF(0.9f, 0.3f, 0.1f, 0.6f) : D2D1::ColorF(0.2f, 0.7f, 0.2f, 0.6f));
        if (dc && brush)
            dc->FillRectangle(GetBounds(), brush);
    }

protected:
    [[nodiscard]] DxUi::PaintOverhang GetPaintOverhang(const DxUi::ControlHost&) const noexcept override
    {
        return {};
    }

private:
    bool _on = false;
};

// A panel that draws its children 6 DIP to the right of where they are, without declaring it.
class ShiftingPanel final : public DxUi::Panel
{
public:
    ShiftingPanel()                                = default;
    ShiftingPanel(const ShiftingPanel&)            = delete;
    ShiftingPanel& operator=(const ShiftingPanel&) = delete;
    ShiftingPanel(ShiftingPanel&&)                 = delete;
    ShiftingPanel& operator=(ShiftingPanel&&)      = delete;
    ~ShiftingPanel() override                      = default;
    void Paint(DxUi::ControlHost& host) const override
    {
        auto* dc = host.GetDeviceContext();
        if (! dc)
            return;
        D2D1_MATRIX_3X2_F previous{};
        dc->GetTransform(&previous);
        dc->SetTransform(D2D1::Matrix3x2F::Translation(6.0f, 0.0f) * previous);
        DxUi::Panel::Paint(host);
        dc->SetTransform(previous);
    }
};

enum RegionSlot : size_t
{
    kRegionRoot,
    kCardA,
    kCardB,
    kToggleA,
    kToggleB,
    kButtonA,
    kButtonB,
    kEscapingLabel,
    kUndeclaredHalo,
    kDeclaredHalo,
    kShifted,
    kShiftedButton,
    kRows,
    kRowFirst,
    kRegionCombo,
    kRegionCheckbox,
    kRegionSlotCount,
};
static_assert(kRegionSlotCount <= kParitySlots);

template <typename Scene> void BuildRegionScene(Scene& scene)
{
    scene.Host().SetTheme(ParityTheme(false));
    auto root          = std::make_unique<DxUi::Panel>();
    auto& slots        = scene.slots;
    slots[kRegionRoot] = root.get();
    const auto place   = [](DxUi::Control* control, float l, float t, float r, float b) { control->SetBounds(D2D1::RectF(l, t, r, b)); };
    for (size_t card = 0u; card < 2u; ++card)
    {
        const float left = 12.0f + 210.0f * static_cast<float>(card);
        auto* panel      = root->AddChild<DxUi::CardPanel>();
        place(panel, left, 12, left + 200, 150);
        auto* label = panel->AddChild<DxUi::Label>(std::format(L"Card {}", card));
        place(label, left + 10, 20, left + 190, 44);
        auto* toggle = panel->AddChild<DxUi::Toggle>(L"Sync");
        place(toggle, left + 10, 52, left + 190, 80);
        auto* button = panel->AddChild<DxUi::Button>(L"Apply");
        place(button, left + 10, 90, left + 120, 122);
        slots[card == 0u ? kCardA : kCardB]     = panel;
        slots[card == 0u ? kToggleA : kToggleB] = toggle;
        slots[card == 0u ? kButtonA : kButtonB] = button;
    }
    // Wrapped in a short box: "Short" fits, the long caption wraps into lines that overflow the box above and below.
    auto* escaping = root->AddChild<DxUi::Label>(L"Short");
    escaping->SetMultiline(true);
    place(slots[kEscapingLabel] = escaping, 440, 20, 520, 44);
    place(slots[kUndeclaredHalo] = root->AddChild<HaloButton>(L"Undeclared"), 440, 60, 560, 90);
    place(slots[kDeclaredHalo] = root->AddChild<DeclaredHaloButton>(L"Declared"), 440, 120, 560, 150);
    auto* shifted = root->AddChild<ShiftingPanel>();
    place(slots[kShifted] = shifted, 12, 170, 300, 220);
    place(slots[kShiftedButton] = shifted->AddChild<DxUi::Button>(L"Shifted"), 20, 178, 140, 210);
    auto* rows = root->AddChild<DxUi::ScrollPanel>();
    for (int index = 0; index < 24; ++index)
    {
        auto* row = rows->AddChild<DxUi::Button>(std::format(L"Row {}", index));
        row->SetBounds(D2D1::RectF(330, 176.0f + 30.0f * static_cast<float>(index), 520, 202.0f + 30.0f * static_cast<float>(index)));
        if (index == 0)
            slots[kRowFirst] = row;
    }
    rows->SetContentHeight(30.0f * 24.0f + 8.0f);
    place(slots[kRows] = rows, 320, 170, 540, 330);
    auto* combo = root->AddChild<DxUi::ComboBox>();
    combo->SetItems({{L"a", L"Alpha"}, {L"b", L"Beta"}, {L"c", L"Gamma"}, {L"d", L"Delta"}});
    combo->SetSelectedIndex(0u);
    place(slots[kRegionCombo] = combo, 12, 240, 200, 272);
    place(slots[kRegionCheckbox] = root->AddChild<DxUi::Checkbox>(L"Check"), 12, 290, 200, 318);
    scene.SetRoot(std::move(root));
}

template <typename Scene> void StepRegions(Scene& scene, Rng& rng, size_t step)
{
    auto& slots = scene.slots;
    constexpr std::array<size_t, 9u> hoverable{
        kToggleA, kToggleB, kButtonA, kButtonB, kUndeclaredHalo, kDeclaredHalo, kShiftedButton, kRegionCheckbox, kRegionCombo};
    switch (rng.Below(11u))
    {
        case 0u:
        case 1u:
        {
            NoteStep("hover");
            DxUi::Control* control = slots[hoverable[rng.Below(static_cast<uint32_t>(hoverable.size()))]];
            scene.Pointer(ParityPointer::Move, RandomPointIn(*control, rng));
            break;
        }
        case 2u:
        {
            NoteStep("hover a scrolled row");
            const D2D1_RECT_F viewport = slots[kRows]->GetBounds();
            scene.Pointer(ParityPointer::Move, D2D1::Point2F(400.0f, viewport.top + 4.0f + (viewport.bottom - viewport.top - 8.0f) * rng.Unit()));
            break;
        }
        case 3u:
        {
            NoteStep("click");
            constexpr std::array<size_t, 4u> clickable{kToggleA, kButtonB, kRegionCheckbox, kRegionCombo};
            const D2D1_POINT_2F point = CenterOf(*slots[clickable[rng.Below(static_cast<uint32_t>(clickable.size()))]]);
            scene.Pointer(ParityPointer::Move, point);
            scene.Pointer(ParityPointer::Down, point);
            scene.Pointer(ParityPointer::Up, point);
            break;
        }
        case 4u:
            NoteStep("focus moves (Tab), the focus ring leaves one control for another");
            scene.Key(VK_TAB);
            break;
        case 5u:
            NoteStep("escaping label text");
            static_cast<DxUi::Label*>(slots[kEscapingLabel])
                ->SetText(rng.Below(2u) ? std::wstring(L"Short") : std::wstring(L"A caption far wider than its narrow box, escaping it"));
            break;
        case 6u:
        {
            NoteStep("halo");
            auto* halo = static_cast<HaloButton*>(slots[rng.Below(2u) ? kUndeclaredHalo : kDeclaredHalo]);
            halo->SetHalo(rng.Below(2u) != 0u);
            break;
        }
        case 7u:
        {
            NoteStep("move by a whole or fractional DIP");
            constexpr std::array<size_t, 6u> movable{kToggleA, kButtonB, kEscapingLabel, kDeclaredHalo, kShiftedButton, kRowFirst};
            DxUi::Control* control = slots[movable[rng.Below(static_cast<uint32_t>(movable.size()))]];
            D2D1_RECT_F bounds     = control->GetBounds();
            const float dx         = rng.Below(2u) ? static_cast<float>(static_cast<int>(rng.Below(9u)) - 4) : (rng.Unit() - 0.5f) * 7.0f;
            const float dy         = rng.Below(2u) ? static_cast<float>(static_cast<int>(rng.Below(5u)) - 2) : (rng.Unit() - 0.5f) * 3.0f;
            bounds                 = D2D1::RectF(bounds.left + dx, bounds.top + dy, bounds.right + dx, bounds.bottom + dy);
            control->SetBounds(bounds);
            break;
        }
        case 8u:
        {
            NoteStep("scroll the rows");
            const D2D1_POINT_2F point = CenterOf(*slots[kRows]);
            scene.Pointer(ParityPointer::Move, point);
            scene.Pointer(ParityPointer::Wheel, point, rng.Below(2u) ? -120.0f : 120.0f);
            break;
        }
        case 9u:
        {
            NoteStep("visibility");
            constexpr std::array<size_t, 3u> hideable{kButtonA, kRegionCheckbox, kShiftedButton};
            DxUi::Control* control = slots[hideable[rng.Below(static_cast<uint32_t>(hideable.size()))]];
            control->SetVisible(! control->IsVisible());
            break;
        }
        default:
            if (step % 5u == 0u)
            {
                NoteStep("theme");
                scene.Host().SetTheme(ParityTheme(! scene.Host().GetTheme().dark));
            }
            else
            {
                NoteStep("Escape (closes an open popup)");
                scene.Key(VK_ESCAPE);
            }
            break;
    }
}

inline constexpr std::array<uint64_t, 3u> kRegionSeeds{0x5EED0410A1u, 0x5EED0410B2u, 0x5EED0410C3u};

template <typename Scene> [[nodiscard]] ParityScript<Scene> RegionScript()
{
    return {"regions", kRegionSeeds, 48u, &BuildRegionScene<Scene>, &StepRegions<Scene>};
}

template <typename Scene> void StepRegionsAtDpi(Scene& scene, Rng& rng, size_t step)
{
    if (step == 1u)
    {
        // Whole and fractional scales: 120 and 168 DPI put DIP edges between pixels.
        constexpr std::array<UINT, 5u> dpis{96u, 120u, 144u, 168u, 192u};
        scene.SetDpi(dpis[rng.Below(5u)]);
        NoteStep("dpi");
        return;
    }
    StepRegions(scene, rng, step);
}

inline constexpr std::array<uint64_t, 6u> kRegionDpiSeeds{0x5EED0410D4u, 0x5EED0410E5u, 0x5EED0410F6u, 0x0410'2001u, 0x0410'2002u, 0x0410'2003u};

// The same script after a first step that sets 96, 120, 144, 168 or 192 DPI: region rectangles snap to device pixels at each
// scale.
template <typename Scene> [[nodiscard]] ParityScript<Scene> RegionDpiScript()
{
    return {"regions-dpi", kRegionDpiSeeds, 32u, &BuildRegionScene<Scene>, &StepRegionsAtDpi<Scene>};
}

// Script "region-edges", deterministic (every step is fixed): what a one-pixel error in an extent, a reach left out of the
// region or a moved control's old place left out of it would leave behind.
// - An EdgeInk's ink fills its extent to the extent's last pixel column, and a Patch begins inside that same pixel column:
//   when only the Patch changes, the region's left edge is that column, and the EdgeInk must not be culled.
// - A declared halo (reach 10 DIP) is drawn and taken away: the region must include the reach.
// - A Button moves 40 DIP and back by a fractional amount: its old place must be repainted.
enum EdgeSlot : size_t
{
    kEdgeRoot,
    kEdgeInk,
    kEdgePatch,
    kEdgeHalo,
    kEdgeMover,
    kEdgeSlotCount,
};
template <typename Scene> void BuildEdgeScene(Scene& scene)
{
    scene.Host().SetTheme(ParityTheme(false));
    auto root        = std::make_unique<DxUi::Panel>();
    auto& slots      = scene.slots;
    slots[kEdgeRoot] = root.get();
    // The EdgeInk's extent ends at 142.5 DIP; the Patch begins at 142.6 DIP: the same pixel column at 96 (142), 120 (178) and
    // 168 DPI (249).
    (slots[kEdgeInk] = root->AddChild<EdgeInk>())->SetBounds(D2D1::RectF(100.0f, 100.0f, 140.5f, 140.0f));
    (slots[kEdgePatch] = root->AddChild<Patch>())->SetBounds(D2D1::RectF(142.6f, 100.0f, 170.0f, 140.0f));
    (slots[kEdgeHalo] = root->AddChild<DeclaredHaloButton>(L"Halo"))->SetBounds(D2D1::RectF(240.0f, 100.0f, 340.0f, 130.0f));
    (slots[kEdgeMover] = root->AddChild<DxUi::Button>(L"Mover"))->SetBounds(D2D1::RectF(100.0f, 200.0f, 200.0f, 232.0f));
    scene.SetRoot(std::move(root));
}
template <UINT Dpi, typename Scene> void StepEdges(Scene& scene, Rng&, size_t step)
{
    auto& slots     = scene.slots;
    const auto move = [&](float dx)
    {
        const D2D1_RECT_F b = slots[kEdgeMover]->GetBounds();
        slots[kEdgeMover]->SetBounds(D2D1::RectF(b.left + dx, b.top, b.right + dx, b.bottom));
    };
    switch (step)
    {
        case 1u:
            NoteStep("dpi");
            scene.SetDpi(Dpi);
            break;
        case 2u:
        case 3u:
        case 8u:
            NoteStep("the Patch beside the EdgeInk's last pixel column changes");
            static_cast<Patch*>(slots[kEdgePatch])->Flip();
            break;
        case 4u:
            NoteStep("the declared halo appears");
            static_cast<HaloButton*>(slots[kEdgeHalo])->SetHalo(true);
            break;
        case 5u:
            NoteStep("the declared halo is taken away (its reach must be repainted)");
            static_cast<HaloButton*>(slots[kEdgeHalo])->SetHalo(false);
            break;
        case 6u:
            NoteStep("a Button moves 40 DIP (its old place must be repainted)");
            move(40.0f);
            break;
        default:
            NoteStep("a Button moves back 37.5 DIP");
            move(-37.5f);
            break;
    }
}
inline constexpr std::array<uint64_t, 1u> kEdgeSeed{0x5EED0410EDu};
template <UINT Dpi, typename Scene> [[nodiscard]] ParityScript<Scene> EdgeScript()
{
    return {"region-edges", kEdgeSeed, 8u, &BuildEdgeScene<Scene>, &StepEdges<Dpi, Scene>};
}

// Scripts of the review probes, deterministic: a PageIndicator with more pages than its box holds (its row passes both
// ends), and a 12 DIP Slider whose hovered thumb (20 DIP) passes its top and bottom.
template <typename Scene> void BuildOverfullPageIndicator(Scene& scene)
{
    scene.Host().SetTheme(ParityTheme(false));
    auto root  = std::make_unique<DxUi::Panel>();
    auto* dots = root->AddChild<DxUi::PageIndicator>();
    dots->SetPageCount(16u);
    dots->SetSelectedIndex(0u);
    dots->SetBounds(D2D1::RectF(250, 100, 310, 120));
    scene.slots[0] = root.get();
    scene.slots[1] = dots;
    scene.SetRoot(std::move(root));
}
template <typename Scene> void StepOverfullPageIndicator(Scene& scene, Rng&, size_t step)
{
    auto* dots = static_cast<DxUi::PageIndicator*>(scene.slots[1]);
    if (step == 5u)
    {
        NoteStep("PageIndicator::SetPageCount shrinks the row");
        dots->SetPageCount(3u);
        return;
    }
    NoteStep("PageIndicator::SetSelectedIndex");
    dots->SetSelectedIndex(static_cast<uint32_t>((step * 5u) % 16u));
}
template <typename Scene> void BuildThinSlider(Scene& scene)
{
    scene.Host().SetTheme(ParityTheme(false));
    auto root    = std::make_unique<DxUi::Panel>();
    auto* slider = root->AddChild<DxUi::Slider>();
    slider->SetValue(50.0);
    slider->SetBounds(D2D1::RectF(100, 100, 300, 112));
    scene.slots[0] = root.get();
    scene.slots[1] = slider;
    scene.SetRoot(std::move(root));
}
template <typename Scene> void StepThinSlider(Scene& scene, Rng&, size_t step)
{
    if (step % 2u == 1u)
    {
        NoteStep("hover the 12 DIP slider");
        scene.Pointer(ParityPointer::Move, CenterOf(*scene.slots[1]));
    }
    else
    {
        NoteStep("leave the 12 DIP slider");
        scene.Pointer(ParityPointer::Move, D2D1::Point2F(20.0f, 20.0f));
    }
}
inline constexpr std::array<uint64_t, 1u> kProbeSeed{0x04100001u};
template <typename Scene> [[nodiscard]] ParityScript<Scene> OverfullPageIndicatorScript()
{
    return {"overfull-page-indicator", kProbeSeed, 6u, &BuildOverfullPageIndicator<Scene>, &StepOverfullPageIndicator<Scene>};
}
template <typename Scene> [[nodiscard]] ParityScript<Scene> ThinSliderScript()
{
    return {"thin-slider", kProbeSeed, 6u, &BuildThinSlider<Scene>, &StepThinSlider<Scene>};
}

// Script "caret-flips" (W4.7): a blink flip repaints only the caret's ink. A TextField, a multiline TextField, an editable ComboBox
// and a RichTextEditor, motion on, the caret blinking at 530 ms without a timeout. Six steps per editor in turn: focus it, two
// flips, a typed character (the whole control repaints, the caret moves), two flips; then the next editor. Every flip frame of
// the incremental scene is compared with a whole fresh one.
enum CaretSlot : size_t
{
    kCaretRoot,
    kCaretField,
    kCaretMultiline,
    kCaretCombo,
    kCaretRich,
};
template <typename Scene> void BuildCaretScene(Scene& scene)
{
    DxUi::ThemePalette theme = ParityTheme(false);
    theme.reducedMotion      = false; // a caret blinks only with motion on
    scene.Host().SetTheme(theme);
    scene.Host().DebugSetCaretBlinkSettingsForTest(DxUi::ControlHost::CaretBlinkSettingsForTest{530u, 0u});
    auto root         = std::make_unique<DxUi::Panel>();
    auto& slots       = scene.slots;
    slots[kCaretRoot] = root.get();
    auto* field       = root->AddChild<DxUi::TextField>(L"Single line שלום");
    field->SetBounds(D2D1::RectF(12, 12, 300, 44));
    slots[kCaretField] = field;
    auto* multiline    = root->AddChild<DxUi::TextField>();
    multiline->SetMultiline(true);
    multiline->SetText(L"first line\nsecond\n\nfourth line of the text");
    multiline->SetBounds(D2D1::RectF(12, 56, 300, 136));
    slots[kCaretMultiline] = multiline;
    auto* combo            = root->AddChild<DxUi::ComboBox>();
    combo->SetEditable(true);
    combo->SetText(L"Editable combo box");
    combo->SetBounds(D2D1::RectF(12, 148, 300, 180));
    slots[kCaretCombo] = combo;
    auto* rich         = root->AddChild<DxUi::RichTextEditor>();
    rich->SetText(L"Rich editor text\nsecond line");
    rich->SetBounds(D2D1::RectF(12, 192, 300, 300));
    slots[kCaretRich] = rich;
    scene.SetRoot(std::move(root));
}
template <typename Scene> void StepCaretFlips(Scene& scene, Rng& rng, size_t step)
{
    constexpr std::array<size_t, 4u> editors{kCaretField, kCaretMultiline, kCaretCombo, kCaretRich};
    const size_t phase    = (step - 1u) % 6u;
    DxUi::Control* editor = scene.slots[editors[((step - 1u) / 6u) % editors.size()]];
    switch (phase)
    {
        case 0u:
            NoteStep("focus the next editor");
            scene.Host().SetFocusControl(editor, false);
            break;
        case 3u:
            NoteStep("type (the caret moves)");
            scene.Character(L"xyzא"[rng.Below(4u)]);
            break;
        default:
            NoteStep("blink flip");
            scene.Advance(530u);
            break;
    }
}
template <typename Scene> [[nodiscard]] ParityScript<Scene> CaretFlipScript()
{
    return {"caret-flips", kProbeSeed, 24u, &BuildCaretScene<Scene>, &StepCaretFlips<Scene>};
}
template <UINT Dpi, typename Scene> void StepCaretFlipsAtDpi(Scene& scene, Rng& rng, size_t step)
{
    if (step == 1u)
    {
        NoteStep("dpi");
        scene.SetDpi(Dpi);
        return;
    }
    StepCaretFlips(scene, rng, step - 1u);
}
template <UINT Dpi, typename Scene> [[nodiscard]] ParityScript<Scene> CaretFlipDpiScript()
{
    return {"caret-flips-dpi", kProbeSeed, 25u, &BuildCaretScene<Scene>, &StepCaretFlipsAtDpi<Dpi, Scene>};
}

// Script "page-host-culling" (W4.10 round 3, X1): what a PageHost paints follows the culling rules of a panel's children.
// - An undeclared consumer page that paints its children into a command list and draws it 200 DIP lower: its Button above
//   the view (outside every paint clip) is drawn inside it and must not be culled; nothing below an undeclared page is.
// - A second PageHost with a library Panel page: an EdgeInk outside the view whose 2 DIP reach reaches 1 DIP into it must not
//   be culled, and one wholly outside is.
// - A page change with motion on, its transition frozen half way (the translated, faded pages), and a change beside it.
class CommandListPage final : public DxUi::Panel
{
public:
    CommandListPage()                                  = default;
    CommandListPage(const CommandListPage&)            = delete;
    CommandListPage& operator=(const CommandListPage&) = delete;
    CommandListPage(CommandListPage&&)                 = delete;
    CommandListPage& operator=(CommandListPage&&)      = delete;
    ~CommandListPage() override                        = default;
    void Paint(DxUi::ControlHost& host) const override
    {
        auto* dc = host.GetDeviceContext();
        if (! dc)
            return;
        wil::com_ptr<ID2D1CommandList> list;
        if (FAILED(dc->CreateCommandList(list.put())))
            return;
        wil::com_ptr<ID2D1Image> previous;
        dc->GetTarget(previous.put());
        dc->SetTarget(list.get());
        DxUi::Panel::Paint(host);
        dc->SetTarget(previous.get());
        if (FAILED(list->Close()))
            return;
        dc->DrawImage(list.get(), D2D1::Point2F(0.0f, 200.0f));
    }
};
[[nodiscard]] inline std::unique_ptr<DxUi::Control> MakeCommandListPage(const wchar_t* text)
{
    auto page = std::make_unique<CommandListPage>();
    page->AddChild<DxUi::Button>(text)->SetBounds(D2D1::RectF(20, -150, 160, -118));
    page->AddChild<DxUi::Label>(L"Drawn where it is, 200 DIP lower")->SetBounds(D2D1::RectF(20, 10, 220, 34));
    return page;
}
template <typename Scene> void BuildPageHostCulling(Scene& scene)
{
    scene.Host().SetTheme(ParityTheme(false));
    auto root   = std::make_unique<DxUi::Panel>();
    auto* pages = root->AddChild<DxUi::PageHost>();
    pages->SetBounds(D2D1::RectF(0, 0, 300, 300));
    pages->SetPage(MakeCommandListPage(L"Above the view"));
    auto* declared = root->AddChild<DxUi::PageHost>();
    declared->SetBounds(D2D1::RectF(-100, 100, 400, 200));
    auto page     = std::make_unique<DxUi::Panel>();
    auto* reaches = page->AddChild<EdgeInk>();
    reaches->SetBounds(D2D1::RectF(-60, 120, -1, 150));                    // its reach ends 1 DIP inside the view
    page->AddChild<EdgeInk>()->SetBounds(D2D1::RectF(-60, 160, -10, 190)); // wholly outside it
    declared->SetPage(std::move(page));
    auto* patch = root->AddChild<Patch>();
    patch->SetBounds(D2D1::RectF(420, 10, 460, 40));
    scene.slots[0] = root.get();
    scene.slots[1] = pages;
    scene.slots[2] = patch;
    scene.slots[3] = declared;
    scene.SetRoot(std::move(root));
}
template <typename Scene> void StepPageHostCulling(Scene& scene, Rng&, size_t step)
{
    if (step == 3u)
    {
        NoteStep("motion on, a new page, its transition frozen half way");
        DxUi::ThemePalette theme = ParityTheme(false);
        theme.reducedMotion      = false;
        scene.Host().SetTheme(theme);
        auto* pages = static_cast<DxUi::PageHost*>(scene.slots[1]);
        pages->SetPage(MakeCommandListPage(L"The next page"));
        pages->DebugFreezeTransitionProgress(0.5f);
        return;
    }
    NoteStep("a Patch beside the pages flips");
    static_cast<Patch*>(scene.slots[2])->Flip();
}
template <typename Scene> [[nodiscard]] ParityScript<Scene> PageHostCullingScript()
{
    return {"page-host-culling", kProbeSeed, 5u, &BuildPageHostCulling<Scene>, &StepPageHostCulling<Scene>};
}
} // namespace PaintRegionScript
