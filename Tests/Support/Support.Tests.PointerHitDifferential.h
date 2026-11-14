#pragma once
// The pointer hit-result differential (plan item W2.9; Specs/UI/UI_InputAndAccessibility.md, "Pointer hit result"), shared
// by the native suite (Tests/Controls/Controls.Tests.PointerHit.cpp) and the embedded suite (Tests/Embedded/Embedded.Tests.PointerHit.h).
//
// Two identical scenes replay the same seeded pointer script: one host answers its cursor query, hover and dispatch from
// the hit result of the pointer event (the default), the other walks the tree for every hit test
// (ControlHost::DebugSetPointerHitResultEnabled(false), the uncached hit test kept callable in a test build). Every
// control the scene builds records the pointer callbacks it receives, in order; after every step both logs must be
// equal, and so must the hovered, captured and focused control and the cursor each query answered. The script mixes
// moves (each announced by a cursor query at the same point, as Windows sends WM_SETCURSOR before the mouse message),
// clicks, wheel scrolls, tree edits (remove, re-add, hide, disable, move, reorder by re-adding), popups, page switches and
// edits made between a cursor query and the move it announces, and from inside the controls' own hover callbacks.
//
// A Scene adapter (one per host) provides:
//   DxUi::ControlHost& Host();             void SetRoot(std::unique_ptr<DxUi::Control>);
//   void Frame();                          // prepares (embedded) or paints (native) so arranged geometry is current
//   DxUi::WindowHostCursorKind CursorQuery(D2D1_POINT_2F dip);   // the cursor query the host answers before a move
//   void Move(D2D1_POINT_2F dip);          void Down(D2D1_POINT_2F dip);     void Up(D2D1_POINT_2F dip);
//   void Wheel(D2D1_POINT_2F dip, float delta);                  void Leave();
//   void MiddleDown(D2D1_POINT_2F dip);    void MiddleUp(D2D1_POINT_2F dip);
//   static constexpr bool kNativePopups;   // true: a ComboBox popup is a native window, which the script never opens
//   void Key(UINT virtualKey);             // to the focused control
//   D2D1_POINT_2F Snap(D2D1_POINT_2F dip); // the point the host computes for a pointer message aimed at `dip`
#include <DxUi/DxUi.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace PointerHitDifferential
{
// splitmix64, as the incremental-versus-fresh oracle uses: the same seed gives the same script on every machine.
struct Rng final
{
    uint64_t state = 0u;
    [[nodiscard]] uint64_t Next() noexcept
    {
        uint64_t z = (state += 0x9E3779B97F4A7C15ull);
        z          = (z ^ (z >> 30u)) * 0xBF58476D1CE4E5B9ull;
        z          = (z ^ (z >> 27u)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31u);
    }
    [[nodiscard]] uint32_t Below(uint32_t bound) noexcept
    {
        return bound == 0u ? 0u : static_cast<uint32_t>(Next() % bound);
    }
    [[nodiscard]] float Unit() noexcept
    {
        return static_cast<float>(Next() >> 40u) / static_cast<float>(1ull << 24u);
    }
};

// The ASCII names the scenes give their recorders, for the log.
[[nodiscard]] inline std::string Narrow(std::wstring_view text)
{
    std::string narrow;
    for (const wchar_t c : text)
        narrow.push_back(c < 0x80 ? static_cast<char>(c) : '?');
    return narrow;
}

// The log both scenes write: one line per pointer callback, in the order the host delivered them.
struct Log final
{
    std::vector<std::string> lines;
    void Add(std::string_view name, std::string_view event)
    {
        lines.push_back(std::format("{} {}", name, event));
    }
};

// Instances are owned by their panels and never copied. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
// A Button that records its pointer callbacks. It overrides no hit-testing function, so it declares that its hit test
// depends only on what the host sees; without the declaration the host would treat it as opaque and never reuse a result.
// An optional hover action edits the tree from inside the hover callback.
class Recorder : public DxUi::Button
{
public:
    Recorder(std::wstring name, Log& log, bool declareReported = true) : DxUi::Button(name), _name(Narrow(name)), _log(&log)
    {
        if (declareReported)
            DeclareHitTestReported();
    }
    std::function<void(Recorder&, bool hovered)> onHover;

    bool OnMouseMove(DxUi::ControlHost& host, D2D1_POINT_2F point, UINT modifiers) override
    {
        _log->Add(_name, "move");
        return DxUi::Button::OnMouseMove(host, point, modifiers);
    }
    bool OnMouseLeave(DxUi::ControlHost& host) override
    {
        _log->Add(_name, "leave");
        return DxUi::Button::OnMouseLeave(host);
    }
    bool OnMouseDown(DxUi::ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT modifiers) override
    {
        _log->Add(_name, "down");
        return DxUi::Button::OnMouseDown(host, point, rightButton, modifiers);
    }
    bool OnMouseUp(DxUi::ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT modifiers) override
    {
        _log->Add(_name, "up");
        return DxUi::Button::OnMouseUp(host, point, rightButton, modifiers);
    }
    bool OnMouseWheel(DxUi::ControlHost& host, D2D1_POINT_2F point, float delta, UINT modifiers) override
    {
        _log->Add(_name, "wheel");
        return DxUi::Button::OnMouseWheel(host, point, delta, modifiers);
    }
    bool OnMouseMiddleDown(DxUi::ControlHost& host, D2D1_POINT_2F point, UINT modifiers) override
    {
        _log->Add(_name, "middle-down");
        return DxUi::Button::OnMouseMiddleDown(host, point, modifiers);
    }
    bool OnMouseMiddleUp(DxUi::ControlHost& host, D2D1_POINT_2F point, UINT modifiers) override
    {
        _log->Add(_name, "middle-up");
        return DxUi::Button::OnMouseMiddleUp(host, point, modifiers);
    }
    void OnHoverChanged(DxUi::ControlHost& host, bool hovered) override
    {
        _log->Add(_name, hovered ? "enter" : "exit");
        DxUi::Button::OnHoverChanged(host, hovered);
        if (onHover)
        {
            const auto action = onHover;
            action(*this, hovered);
        }
    }
    [[nodiscard]] const std::string& Name() const noexcept
    {
        return _name;
    }

private:
    std::string _name;
    Log* _log;
};

// A recorder whose hit test has a hole that it opens and closes from its own move callback: state the host cannot see.
// With `reported` it declares the hit test reported and moves the hit revision (InvalidateHitTest) whenever the hole
// changes; without it the host never reuses a hit result while it is in the tree.
class HoleRecorder final : public Recorder
{
public:
    HoleRecorder(std::wstring name, Log& log, bool reported) : Recorder(std::move(name), log, reported), _reported(reported)
    {
    }
    bool OnMouseMove(DxUi::ControlHost& host, D2D1_POINT_2F point, UINT modifiers) override
    {
        const bool result = Recorder::OnMouseMove(host, point, modifiers);
        _hole             = ! _hole;
        if (_reported)
            InvalidateHitTest();
        return result;
    }
    [[nodiscard]] bool Reported() const noexcept
    {
        return _reported;
    }

protected:
    [[nodiscard]] DxUi::Control* HitTest(D2D1_POINT_2F point) override
    {
        const D2D1_RECT_F bounds = GetBounds();
        const float middle       = (bounds.left + bounds.right) * 0.5f;
        if (_hole && point.x >= middle)
            return nullptr;
        return DxUi::Button::HitTest(point);
    }
    [[nodiscard]] const DxUi::Control* HitTest(D2D1_POINT_2F point) const override
    {
        return const_cast<HoleRecorder*>(this)->HitTest(point);
    }

private:
    bool _reported = false;
    bool _hole     = false;
};
#pragma warning(pop)

// The scene: a root Panel with recorders (two overlap, so z-order decides), a hole recorder (reported or not), an editable
// ComboBox whose popup covers recorders when open, a ScrollPanel of recorders and a TabControl of two recorder pages.
struct SceneSlots final
{
    DxUi::Panel* root = nullptr;
    std::array<Recorder*, 4> top{};
    HoleRecorder* hole        = nullptr;
    DxUi::ComboBox* combo     = nullptr;
    DxUi::ScrollPanel* scroll = nullptr;
    DxUi::TabControl* tabs    = nullptr;
    std::vector<Recorder*> rows;
    uint32_t serial = 0u;
};

template <typename Scene> void BuildScene(Scene& scene, SceneSlots& slots, Log& log, bool reportedHole)
{
    auto root        = std::make_unique<DxUi::Panel>();
    slots.root       = root.get();
    const auto place = [](DxUi::Control* control, float l, float t, float r, float b) { control->SetBounds(D2D1::RectF(l, t, r, b)); };
    for (size_t index = 0u; index < slots.top.size(); ++index)
    {
        slots.top[index] = root->AddChild<Recorder>(std::format(L"top{}", index), log);
        place(slots.top[index], 10.0f + 70.0f * static_cast<float>(index), 10.0f, 90.0f + 70.0f * static_cast<float>(index), 50.0f); // 0/1, 1/2 ... overlap
    }
    slots.hole = root->AddChild<HoleRecorder>(L"hole", log, reportedHole);
    place(slots.hole, 10.0f, 60.0f, 170.0f, 100.0f);
    slots.combo = root->AddChild<DxUi::ComboBox>();
    slots.combo->SetItems({{L"a", L"Alpha"}, {L"b", L"Beta"}, {L"c", L"Gamma"}, {L"d", L"Delta"}, {L"e", L"Epsilon"}});
    place(slots.combo, 190.0f, 60.0f, 330.0f, 92.0f);
    // Below the ComboBox: its open popup covers these recorders.
    for (size_t index = 0u; index < 2u; ++index)
    {
        auto* under = root->AddChild<Recorder>(std::format(L"under{}", index), log);
        place(under, 190.0f, 100.0f + 40.0f * static_cast<float>(index), 330.0f, 136.0f + 40.0f * static_cast<float>(index));
    }
    slots.scroll = root->AddChild<DxUi::ScrollPanel>();
    for (size_t index = 0u; index < 6u; ++index)
    {
        auto* row = slots.scroll->AddChild<Recorder>(std::format(L"row{}", index), log);
        place(row, 10.0f, 190.0f + 36.0f * static_cast<float>(index), 150.0f, 222.0f + 36.0f * static_cast<float>(index));
        slots.rows.push_back(row);
    }
    slots.scroll->SetContentHeight(240.0f);
    place(slots.scroll, 10.0f, 190.0f, 170.0f, 290.0f);
    slots.tabs = root->AddChild<DxUi::TabControl>();
    slots.tabs->AddTab<Recorder>(L"One", L"page0", log);
    slots.tabs->AddTab<Recorder>(L"Two", L"page1", log);
    place(slots.tabs, 190.0f, 190.0f, 400.0f, 300.0f);
    scene.SetRoot(std::move(root));
    scene.Frame();
}

// Interesting points: the centres and edges of every control, both halves of the hole, the popup rows' area, and random.
template <typename Scene> [[nodiscard]] D2D1_POINT_2F PickPoint(Scene& scene, SceneSlots& slots, Rng& rng)
{
    std::vector<D2D1_RECT_F> rects;
    for (auto* control : slots.top)
        rects.push_back(control->GetBounds());
    rects.push_back(slots.hole->GetBounds());
    rects.push_back(slots.combo->GetBounds());
    rects.push_back(D2D1::RectF(190.0f, 92.0f, 330.0f, 220.0f)); // where the open popup and the recorders under it are
    rects.push_back(slots.scroll->GetBounds());
    rects.push_back(slots.tabs->GetBounds());
    rects.push_back(D2D1::RectF(0.0f, 0.0f, 420.0f, 320.0f));
    const D2D1_RECT_F rect = rects[rng.Below(static_cast<uint32_t>(rects.size()))];
    return scene.Snap(
        D2D1::Point2F(rect.left + 1.0f + (rect.right - rect.left - 2.0f) * rng.Unit(), rect.top + 1.0f + (rect.bottom - rect.top - 2.0f) * rng.Unit()));
}

// One tree, bounds, visibility, popup, scroll or page edit. Every edit uses public API only.
template <typename Scene> void Edit(Scene& scene, SceneSlots& slots, Log& log, Rng& rng)
{
    switch (rng.Below(9u))
    {
        case 0u:
        {
            DxUi::Control* control = slots.top[rng.Below(4u)];
            control->SetVisible(! control->IsVisible());
            break;
        }
        case 1u:
        {
            DxUi::Control* control = slots.top[rng.Below(4u)];
            control->SetEnabled(! control->IsEnabled());
            break;
        }
        case 2u:
        {
            DxUi::Control* control = slots.top[rng.Below(4u)];
            D2D1_RECT_F bounds     = control->GetBounds();
            const float dx         = (rng.Unit() - 0.5f) * 40.0f;
            bounds.left += dx;
            bounds.right += dx;
            control->SetBounds(bounds);
            break;
        }
        case 3u:
        {
            // Remove one top recorder and add a new one in its place, last: tree membership and z-order change together.
            const uint32_t index = rng.Below(4u);
            Recorder* old        = slots.top[index];
            const D2D1_RECT_F at = old->GetBounds();
            for (size_t child = 0u; child < slots.root->GetLogicalChildCount(); ++child)
                if (slots.root->GetLogicalChild(child) == old)
                {
                    std::unique_ptr<DxUi::Control> taken = slots.root->TakeChild(child);
                    static_cast<void>(slots.root->CompactChildren());
                    if (! taken)
                        return; // a callback kept it; leave the scene unchanged
                    break;
                }
            slots.top[index] = slots.root->AddChild<Recorder>(std::format(L"top{}x{}", index, ++slots.serial), log);
            slots.top[index]->SetBounds(at);
            break;
        }
        case 4u:
            // A native popup is a window of its own, which this script never opens.
            if constexpr (! Scene::kNativePopups)
            {
                scene.Host().SetFocusControl(slots.combo, false);
                scene.Key(slots.combo->IsPopupOpen() ? VK_ESCAPE : VK_F4);
            }
            break;
        case 5u: slots.scroll->SetScrollOffset(rng.Unit() * 160.0f); break;
        case 6u: slots.tabs->SetSelectedIndex(slots.tabs->GetSelectedIndex().value_or(0u) == 0u ? 1u : 0u); break;
        case 7u: slots.hole->SetVisible(! slots.hole->IsVisible()); break;
        default:
        {
            // A hover callback edits the tree: hide a neighbour, or move this recorder, while the pointer enters it.
            Recorder* host     = slots.top[rng.Below(4u)];
            const uint32_t how = rng.Below(3u);
            Recorder* other    = slots.top[rng.Below(4u)];
            host->onHover      = [how, other](Recorder& self, bool hovered)
            {
                if (! hovered)
                    return;
                if (how == 0u && other != &self)
                    other->SetVisible(! other->IsVisible());
                else if (how == 1u)
                {
                    D2D1_RECT_F bounds = self.GetBounds();
                    bounds.top += 4.0f;
                    bounds.bottom += 4.0f;
                    self.SetBounds(bounds);
                }
                self.onHover = {};
            };
            break;
        }
    }
}

// What one step left behind, beyond the log: the controls the host holds and the cursor answers.
struct StepRecord final
{
    std::vector<std::string> lines;
    std::string state;
    bool operator==(const StepRecord&) const = default;
};

[[nodiscard]] inline std::string Describe(const DxUi::Control* control)
{
    if (! control)
        return "none";
    if (const auto* recorder = dynamic_cast<const Recorder*>(control))
        return recorder->Name();
    return typeid(*control).name();
}

// One step on one scene: a pointer gesture, sometimes after an edit, sometimes with an edit between the cursor query and
// the move it announces.
template <typename Scene> StepRecord Step(Scene& scene, SceneSlots& slots, Log& log, Rng& rng)
{
    log.lines.clear();
    std::string cursors;
    const auto query    = [&](D2D1_POINT_2F point) { cursors += std::format("{} ", static_cast<int>(scene.CursorQuery(point))); };
    const uint32_t kind = rng.Below(10u);
    if (kind == 0u)
    {
        Edit(scene, slots, log, rng);
        scene.Frame();
    }
    const D2D1_POINT_2F point = PickPoint(scene, slots, rng);
    switch (kind)
    {
        case 1u:
        case 2u:
            query(point);
            scene.Move(point);
            break;
        case 3u:
            // The tree changes between the cursor query and the move it announces.
            query(point);
            Edit(scene, slots, log, rng);
            scene.Move(point);
            break;
        case 4u:
            if constexpr (Scene::kNativePopups)
            {
                const D2D1_RECT_F combo = slots.combo->GetBounds();
                if (point.x >= combo.left && point.x < combo.right && point.y >= combo.top && point.y < combo.bottom)
                {
                    scene.Leave();
                    break;
                }
            }
            query(point);
            scene.Move(point);
            query(point);
            scene.Down(point);
            query(point);
            scene.Up(point);
            break;
        case 5u:
            query(point);
            scene.Move(point);
            scene.Wheel(point, rng.Below(2u) ? -120.0f : 120.0f);
            break;
        case 6u: scene.Leave(); break;
        case 7u:
            // A move without a cursor query (captured pointers, synthesized moves), then one with.
            scene.Move(point);
            query(point);
            scene.Move(point);
            break;
        case 8u:
            // The middle button (W1.1): its press and release reach the control under the pointer.
            query(point);
            scene.Move(point);
            query(point);
            scene.MiddleDown(point);
            query(point);
            scene.MiddleUp(point);
            break;
        default:
            query(point);
            scene.Move(point);
            break;
    }
    scene.Frame();
    StepRecord record;
    record.lines = log.lines;
    record.state = std::format("hover={} capture={} focus={} cursors={}",
                               Describe(scene.Host().DebugGetHoveredControl()),
                               Describe(scene.Host().GetCapturedControl()),
                               Describe(scene.Host().GetFocusControl()),
                               cursors);
    return record;
}
} // namespace PointerHitDifferential
