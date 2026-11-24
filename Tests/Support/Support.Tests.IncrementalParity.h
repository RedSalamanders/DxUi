#pragma once
// The incremental-versus-fresh oracle (Testing_Validation.md, "Incremental-versus-fresh oracle"; plan item W0.5), shared by
// the native suite (Tests/Controls/Controls.Tests.IncrementalParity.cpp) and the embedded suite
// (Tests/Embedded/Embedded.Tests.IncrementalParity.h).
//
// Two identical scenes run the same seeded mutation script. After every step the incremental scene draws with whatever it
// retained, the fresh scene first drops every registered retained cache (DebugDropRetainedStateForTest), requires each of
// them to report 0 entries, and draws from scratch. The two frames must agree exactly: premultiplied BGRA pixels (WARP),
// the bounds and state of every control, and the UI Automation tree. The first difference names the script, seed, step,
// pixel and the deepest control under it, with the command that replays that seed alone.
//
// Adding a script: write one `template <typename Scene> void Build...(Scene&)` and one
// `template <typename Scene> void Step...(Scene&, Rng&, size_t step)` (here or in the suite file) and a `ParityScript` entry
// beside them, then register one test per suite that calls RunParityScript with it. Each script is its own entry: no shared
// list grows at one line. A later item that needs its own scene (a retained command list, a partial region) does the same.
//
// A Scene adapter (one per host) provides:
//   DxUi::ControlHost& Host();                       std::array<DxUi::Control*, kParitySlots> slots;
//   void SetRoot(std::unique_ptr<DxUi::Control>);    D2D1_SIZE_F SizeDip() const;
//   void Pointer(ParityPointer, D2D1_POINT_2F dip, float wheelDelta = 0);
//   void Key(UINT virtualKey);                       void Character(wchar_t);
//   void SetDpi(UINT dpi);                           void Advance(uint64_t milliseconds);   // the frozen clock
//   ParityFrame Capture(bool fresh);
#include "Controls/DxUi.Internal.h"
#include "Controls/DxUi.RetainedCaches.h"
#include "Support/RetainedCaches.Tests.Hooks.h"
#include <DxUi/DxUi.h>
#include <DxUi/RichImage.h>

#include <UIAutomation.h>
#include <wil/com.h>
#include <wil/resource.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

namespace IncrementalParity
{
inline constexpr size_t kParitySlots  = 32u;
inline constexpr size_t kUiaNodeLimit = 4096u;

[[noreturn]] inline void Fail(const std::string& message)
{
    std::cerr << "FAILED: " << message << '\n' << std::flush;
    std::exit(1);
}

// splitmix64: the same seed gives the same steps in both scenes, in every build and on every machine.
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

// What the latest step did, for the failure message (a step names its operation with NoteStep).
inline thread_local std::string_view g_lastStep = {};
inline void NoteStep(std::string_view operation) noexcept
{
    g_lastStep = operation;
}

enum class ParityPointer : uint8_t
{
    Move,
    Down,
    Up,
    Wheel,
    Leave,
};

struct ControlRecord final
{
    std::string path;
    std::string kind;
    D2D1_RECT_F bounds{};
    bool visible = false;
    bool enabled = false;
    bool focused = false;
    bool hovered = false;
    [[nodiscard]] bool operator==(const ControlRecord& other) const noexcept
    {
        return path == other.path && kind == other.kind && std::memcmp(&bounds, &other.bounds, sizeof(bounds)) == 0 && visible == other.visible &&
               enabled == other.enabled && focused == other.focused && hovered == other.hovered;
    }
};

struct ParityFrame final
{
    UINT widthPx  = 0u;
    UINT heightPx = 0u;
    float dpi     = 96.0f;
    std::vector<uint8_t> bgra;
    std::vector<ControlRecord> controls;
    std::vector<std::wstring> uia;
};

[[nodiscard]] inline std::string Narrow(std::wstring_view text)
{
    std::string out;
    for (const wchar_t unit : text)
        out.push_back(unit < 0x80 ? static_cast<char>(unit) : '?');
    return out;
}

inline void CollectControls(const DxUi::Control& control, const std::string& path, std::vector<ControlRecord>& out)
{
    out.push_back({path, typeid(control).name(), control.GetBounds(), control.IsVisible(), control.IsEnabled(), control.HasFocus(), control.IsHovered()});
    for (size_t index = 0u; index < control.GetLogicalChildCount(); ++index)
        if (const DxUi::Control* child = control.GetLogicalChild(index))
            CollectControls(*child, std::format("{}/{}", path, index), out);
}

[[nodiscard]] inline std::vector<ControlRecord> CollectControls(const DxUi::ControlHost& host)
{
    std::vector<ControlRecord> out;
    if (const DxUi::Control* root = host.GetRoot())
        CollectControls(*root, "root", out);
    return out;
}

[[nodiscard]] inline std::wstring VariantText(const VARIANT& value)
{
    switch (value.vt)
    {
        case VT_EMPTY: return L"-";
        case VT_BSTR: return value.bstrVal ? std::wstring(value.bstrVal, SysStringLen(value.bstrVal)) : std::wstring{};
        case VT_I4: return std::to_wstring(value.lVal);
        case VT_BOOL: return value.boolVal == VARIANT_TRUE ? L"T" : L"F";
        case VT_R8: return std::format(L"{:.4f}", value.dblVal);
        case VT_UNKNOWN: return L"unk";
        default: return std::format(L"vt{}", static_cast<int>(value.vt));
    }
}

// One line per element in depth-first order: what a UI Automation client reads from the published snapshot.
inline void DescribeUia(IRawElementProviderFragment& fragment, size_t depth, std::vector<std::wstring>& out)
{
    if (out.size() >= kUiaNodeLimit)
        return;
    std::wstring line = std::format(L"{}", depth);
    if (wil::com_ptr_nothrow<IRawElementProviderSimple> simple; SUCCEEDED(fragment.QueryInterface(IID_PPV_ARGS(simple.put()))) && simple)
    {
        constexpr std::array<PROPERTYID, 13u> properties{UIA_ControlTypePropertyId,
                                                         UIA_NamePropertyId,
                                                         UIA_AutomationIdPropertyId,
                                                         UIA_IsEnabledPropertyId,
                                                         UIA_HasKeyboardFocusPropertyId,
                                                         UIA_IsKeyboardFocusablePropertyId,
                                                         UIA_IsOffscreenPropertyId,
                                                         UIA_ValueValuePropertyId,
                                                         UIA_ToggleToggleStatePropertyId,
                                                         UIA_ExpandCollapseExpandCollapseStatePropertyId,
                                                         UIA_SelectionItemIsSelectedPropertyId,
                                                         UIA_RangeValueValuePropertyId,
                                                         UIA_HelpTextPropertyId};
        for (const PROPERTYID property : properties)
        {
            VARIANT value{};
            VariantInit(&value);
            const HRESULT hr = simple->GetPropertyValue(property, &value);
            line += L'|';
            line += SUCCEEDED(hr) ? VariantText(value) : std::format(L"hr{:08X}", static_cast<uint32_t>(hr));
            VariantClear(&value);
        }
    }
    UiaRect rect{};
    if (SUCCEEDED(fragment.get_BoundingRectangle(&rect)))
        line += std::format(L"|{:.2f},{:.2f},{:.2f},{:.2f}", rect.left, rect.top, rect.width, rect.height);
    out.push_back(std::move(line));
    wil::com_ptr_nothrow<IRawElementProviderFragment> child;
    if (FAILED(fragment.Navigate(NavigateDirection_FirstChild, child.put())))
        return;
    while (child && out.size() < kUiaNodeLimit)
    {
        DescribeUia(*child, depth + 1u, out);
        wil::com_ptr_nothrow<IRawElementProviderFragment> next;
        if (FAILED(child->Navigate(NavigateDirection_NextSibling, next.put())))
            break;
        child = std::move(next);
    }
}

[[nodiscard]] inline std::vector<std::wstring> DescribeUia(IRawElementProviderFragmentRoot* root)
{
    std::vector<std::wstring> out;
    if (! root)
        return out;
    wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
    if (SUCCEEDED(root->QueryInterface(IID_PPV_ARGS(fragment.put()))) && fragment)
        DescribeUia(*fragment, 0u, out);
    return out;
}

// Every registered cache must read 0 right after the drop: a row that keeps entries is not dropping what it counts.
inline void RequireRetainedCachesEmpty(std::span<const DxUi::RetainedCacheCount> counts, std::string_view where)
{
    for (const auto& count : counts)
        if (count.entries != 0u)
            Fail(std::format("{}: retained cache {} still holds {} entries after DebugDropRetainedStateForTest", where, count.name, count.entries));
    if (counts.size() != DxUi::RetainedCacheAccess::Names().size() && counts.size() + 1u != DxUi::RetainedCacheAccess::Names().size())
        Fail(std::format("{}: {} cache counts for {} registered caches", where, counts.size(), DxUi::RetainedCacheAccess::Names().size()));
}

// The deepest visible control whose bounds contain the pixel, in the fresh scene's tree.
[[nodiscard]] inline std::string ControlAtPixel(const ParityFrame& frame, UINT x, UINT y)
{
    const float scale = frame.dpi / 96.0f;
    const float dipX = (static_cast<float>(x) + 0.5f) / scale, dipY = (static_cast<float>(y) + 0.5f) / scale;
    const ControlRecord* best = nullptr;
    for (const ControlRecord& record : frame.controls)
    {
        if (! record.visible || dipX < record.bounds.left || dipX >= record.bounds.right || dipY < record.bounds.top || dipY >= record.bounds.bottom)
            continue;
        if (! best || record.path.size() >= best->path.size())
            best = &record;
    }
    return best ? std::format("{} ({})", best->path, best->kind) : std::string("no control (window background or overlay)");
}

// Differences a host is known to produce today, each exempted by name with its reason and reported, never hidden. A Scene
// declares the ones it takes in `static constexpr uint32_t kKnownDifferences`.
enum ParityKnownDifference : uint32_t
{
    kNoKnownDifference = 0u,
    // None today. The native window host's stale bounding rectangles after Control::SetBounds, found by this oracle (W0.5),
    // were exempted here until W8.2 published geometry incrementally.
};

// Empty when the frames agree; otherwise the first difference, pixels first. channelTolerance stays 0 except for an item
// whose gate declares one (an effect rasterized differently on purpose). An exempted known difference is appended to
// `known` instead of failing.
[[nodiscard]] inline std::string FirstDifference(
    const ParityFrame& incremental, const ParityFrame& fresh, uint8_t channelTolerance = 0u, uint32_t knownDifferences = 0u, std::string* known = nullptr)
{
    // No difference is exempted today (ParityKnownDifference); the parameters stay for the next one a scene declares.
    static_cast<void>(knownDifferences);
    static_cast<void>(known);
    if (incremental.widthPx != fresh.widthPx || incremental.heightPx != fresh.heightPx || incremental.bgra.size() != fresh.bgra.size())
        return std::format("frame size {}x{} incremental, {}x{} fresh", incremental.widthPx, incremental.heightPx, fresh.widthPx, fresh.heightPx);
    if (fresh.bgra.empty())
        return "no pixels were read";
    size_t differing = 0u;
    std::optional<size_t> first;
    UINT left = fresh.widthPx, top = fresh.heightPx, right = 0u, bottom = 0u;
    for (size_t offset = 0u; offset < fresh.bgra.size(); offset += 4u)
    {
        bool same = true;
        for (size_t channel = 0u; channel < 4u; ++channel)
            same = same && std::abs(int{incremental.bgra[offset + channel]} - int{fresh.bgra[offset + channel]}) <= channelTolerance;
        if (same)
            continue;
        ++differing;
        first            = first.value_or(offset);
        const auto pixel = offset / 4u;
        const UINT x     = static_cast<UINT>(pixel % fresh.widthPx);
        const UINT y     = static_cast<UINT>(pixel / fresh.widthPx);
        left             = (std::min)(left, x);
        top              = (std::min)(top, y);
        right            = (std::max)(right, x);
        bottom           = (std::max)(bottom, y);
    }
    if (first)
    {
        const size_t offset = *first;
        const UINT x = static_cast<UINT>((offset / 4u) % fresh.widthPx), y = static_cast<UINT>((offset / 4u) / fresh.widthPx);
        const auto bgra = [&](const ParityFrame& frame)
        { return std::format("{:02X}{:02X}{:02X}{:02X}", frame.bgra[offset], frame.bgra[offset + 1u], frame.bgra[offset + 2u], frame.bgra[offset + 3u]); };
        return std::format("{} BGRA pixels differ inside ({},{})-({},{}); first at ({},{}): incremental {} fresh {}; control {}",
                           differing,
                           left,
                           top,
                           right,
                           bottom,
                           x,
                           y,
                           bgra(incremental),
                           bgra(fresh),
                           ControlAtPixel(fresh, x, y));
    }
    const size_t controls = (std::min)(incremental.controls.size(), fresh.controls.size());
    for (size_t index = 0u; index < controls; ++index)
    {
        const ControlRecord& a = incremental.controls[index];
        const ControlRecord& b = fresh.controls[index];
        if (! (a == b))
            return std::format(
                "control {} ({}): incremental bounds ({:.3f},{:.3f},{:.3f},{:.3f}) v{}e{}f{}h{}, fresh ({:.3f},{:.3f},{:.3f},{:.3f}) v{}e{}f{}h{}",
                b.path,
                b.kind,
                a.bounds.left,
                a.bounds.top,
                a.bounds.right,
                a.bounds.bottom,
                int{a.visible},
                int{a.enabled},
                int{a.focused},
                int{a.hovered},
                b.bounds.left,
                b.bounds.top,
                b.bounds.right,
                b.bounds.bottom,
                int{b.visible},
                int{b.enabled},
                int{b.focused},
                int{b.hovered});
    }
    if (incremental.controls.size() != fresh.controls.size())
        return std::format("{} controls incremental, {} fresh", incremental.controls.size(), fresh.controls.size());
    const size_t nodes = (std::min)(incremental.uia.size(), fresh.uia.size());
    for (size_t index = 0u; index < nodes; ++index)
        if (incremental.uia[index] != fresh.uia[index])
            return std::format("UI Automation element {}: incremental [{}] fresh [{}]", index, Narrow(incremental.uia[index]), Narrow(fresh.uia[index]));
    if (incremental.uia.size() != fresh.uia.size())
        return std::format("{} UI Automation elements incremental, {} fresh", incremental.uia.size(), fresh.uia.size());
    return {};
}

template <typename Scene> struct ParityScript final
{
    const char* name;
    std::span<const uint64_t> seeds; // Checked in; a failure prints the seed to replay alone.
    size_t steps;
    void (*build)(Scene& scene);
    void (*step)(Scene& scene, Rng& rng, size_t step);
};

// DXUI_PARITY_SEED=<seed> (decimal or 0x hex) runs only that seed of every script, to replay a failure alone.
[[nodiscard]] inline std::vector<uint64_t> SelectSeeds(std::span<const uint64_t> seeds)
{
    char* value   = nullptr;
    size_t length = 0u;
    if (_dupenv_s(&value, &length, "DXUI_PARITY_SEED") == 0 && value)
    {
        const std::unique_ptr<char, decltype(&std::free)> owned(value, &std::free);
        return {std::strtoull(value, nullptr, 0)};
    }
    return {seeds.begin(), seeds.end()};
}

// Runs the script on an incremental and a fresh scene made by `make` and returns the first difference, or an empty string.
// `replay` names how to run this test alone. The first frame is compared before any step.
template <typename Scene, typename Make>
[[nodiscard]] std::string RunParityScriptOnce(
    const ParityScript<Scene>& script, uint64_t seed, const Make& make, uint8_t channelTolerance = 0u, std::string* known = nullptr)
{
    std::unique_ptr<Scene> incremental = make();
    std::unique_ptr<Scene> fresh       = make();
    // The fresh scene also prepares without reusing a previous publication (W7.1: the semantic builder's unchanged
    // blocks), which the cache drop alone cannot reach.
    const auto freshly = [](auto&& operation)
    {
        DxUi::Detail::SetFreshPreparationForTest(true);
        operation();
        DxUi::Detail::SetFreshPreparationForTest(false);
    };
    // Every incremental UI Automation publication of either scene is also checked against a whole build of the same tree (W8.2).
    const bool wasVerifying = DxUi::DebugGetAccessibilityIncrementalVerificationForTest();
    DxUi::DebugResetAccessibilityIncrementalVerificationForTest();
    DxUi::DebugSetAccessibilityIncrementalVerificationForTest(true);
    const auto stopVerifying = wil::scope_exit([wasVerifying] { DxUi::DebugSetAccessibilityIncrementalVerificationForTest(wasVerifying); });
    script.build(*incremental);
    freshly([&] { script.build(*fresh); });
    Rng incrementalRng{seed};
    Rng freshRng{seed};
    for (size_t step = 0u; step <= script.steps; ++step)
    {
        if (step != 0u)
        {
            g_lastStep = "unnamed";
            script.step(*incremental, incrementalRng, step);
            freshly([&] { script.step(*fresh, freshRng, step); });
        }
        const ParityFrame a = incremental->Capture(false);
        ParityFrame b;
        freshly([&] { b = fresh->Capture(true); });
        std::string exempted;
        if (std::string difference = FirstDifference(a, b, channelTolerance, Scene::kKnownDifferences, &exempted); ! difference.empty())
            return std::format("step {} ({}): {}", step, step == 0u ? std::string_view("initial frame") : g_lastStep, difference);
        if (DxUi::DebugGetAccessibilityIncrementalMismatchCountForTest() != 0u)
            return std::format("step {} ({}): an incremental UI Automation publication differs from a whole build: {}",
                               step,
                               g_lastStep,
                               Narrow(DxUi::DebugGetAccessibilityFirstIncrementalMismatchForTest()));
        if (known && known->empty() && ! exempted.empty())
            *known = std::format("step {} ({}): {}", step, g_lastStep, exempted);
    }
    return {};
}

template <typename Scene, typename Make>
void RunParityScript(const ParityScript<Scene>& script, const Make& make, std::string_view host, std::string_view replay, uint8_t channelTolerance = 0u)
{
    for (const uint64_t seed : SelectSeeds(script.seeds))
    {
        std::string known;
        if (std::string difference = RunParityScriptOnce(script, seed, make, channelTolerance, &known); ! difference.empty())
            Fail(std::format("incremental-versus-fresh oracle, {} host, script {}, seed 0x{:016X}, {}. Replay alone: set DXUI_PARITY_SEED=0x{:016X} and run {}",
                             host,
                             script.name,
                             seed,
                             difference,
                             seed,
                             replay));
        std::cout << "  parity " << host << ' ' << script.name << std::format(" seed 0x{:016X}", seed) << ": " << script.steps << " steps, 0 differences\n";
        if (! known.empty())
            std::cout << "    KNOWN DIFFERENCE (exempted by name), first at " << known << '\n';
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Shared fixtures and scripts. Slots hold borrowed pointers into the scene's own tree; both scenes fill them identically.

class ParityGridModel final : public DxUi::IGridModel
{
public:
    size_t rows     = 40u;
    uint32_t serial = 0u;
    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return rows;
    }
    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 3u;
    }
    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t column) const override
    {
        constexpr std::array<std::wstring_view, 3u> titles{L"Name", L"Notes", L"Size"};
        DxUi::GridColumnDesc desc;
        desc.id       = std::wstring(titles[column]);
        desc.title    = std::wstring(titles[column]);
        desc.widthDip = column == 1u ? 150.0f : 90.0f;
        return desc;
    }
    void GetCellData(size_t row, size_t column, DxUi::GridCellData& cell) const override
    {
        cell.multiline = column == 1u;
        switch (column)
        {
            case 0u: cell.text = std::format(L"Item {} é", row + serial); break;
            case 1u: cell.text = row % 3u == 0u ? std::format(L"Line one {}\nLine two שלום", serial) : std::format(L"Short {}", row); break;
            default: cell.text = std::format(L"{} KB", (row * 37u + serial) % 1000u); break;
        }
    }
    [[nodiscard]] uint64_t GetStableRowId(size_t row) const noexcept override
    {
        return row + 1u;
    }
    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t id) const noexcept override
    {
        return id >= 1u && id <= rows ? std::optional<size_t>(id - 1u) : std::nullopt;
    }
};

class ParityTreeModel final : public DxUi::ITreeModel
{
public:
    uint32_t serial = 0u;
    [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
    {
        return 24u;
    }
    void GetVisibleItem(size_t index, DxUi::TreeItemData& item) const override
    {
        item             = {};
        item.id          = index + 1u;
        item.depth       = static_cast<uint32_t>(index % 3u);
        item.hasChildren = index % 3u != 2u;
        item.expanded    = item.hasChildren;
        item.text        = std::format(L"Node {} 東京", index + serial);
        item.badgeText   = index % 4u == 0u ? std::format(L"{}", index + serial) : std::wstring{};
        item.tooltipText = std::format(L"Tooltip {}", index);
    }
};

[[nodiscard]] inline DxUi::ThemePalette ParityTheme(bool dark)
{
    DxUi::ThemePalette theme = DxUi::MakeDefaultThemePalette(dark);
    theme.reducedMotion      = true;
    // A blurred popup backdrop is a screen capture: input from outside DxUi that the two scenes cannot share.
    theme.overlayMaterial = DxUi::OverlayMaterial::Solid;
    return theme;
}

// Slot layout of the shared "controls" scene.
enum ControlsSlot : size_t
{
    kRoot,
    kTitle,
    kMnemonicLabel,
    kButton,
    kIconButton,
    kCheckbox,
    kToggle,
    kField,
    kMultilineField,
    kCombo,
    kSlider,
    kProgress,
    kTabs,
    kGrid,
    kTree,
    kScroll,
    kMenuBar,
    kScrollFirst,
    kLink,
    kFlex,
    kSlotCount,
};
static_assert(kSlotCount <= kParitySlots);

// Models outlive the controls that borrow them: they are owned by the scene through `models`.
template <typename Scene> void BuildControlsScene(Scene& scene)
{
    auto grid = std::make_shared<ParityGridModel>();
    auto tree = std::make_shared<ParityTreeModel>();
    scene.models.push_back(grid);
    scene.models.push_back(tree);
    scene.Host().SetTheme(ParityTheme(false));
    auto root        = std::make_unique<DxUi::Panel>();
    auto& slots      = scene.slots;
    slots[kRoot]     = root.get();
    const auto place = [](DxUi::Control* control, float l, float t, float r, float b) { control->SetBounds(D2D1::RectF(l, t, r, b)); };
    place(slots[kTitle] = root->AddChild<DxUi::Label>(L"Incremental parity é中文"), 12, 8, 300, 32);
    auto* mnemonic = root->AddChild<DxUi::Label>(L"Name with mnemonic");
    mnemonic->SetMnemonic(L'N');
    place(slots[kMnemonicLabel] = mnemonic, 12, 36, 200, 60);
    auto* button = root->AddChild<DxUi::Button>(L"Apply");
    button->SetShortcutText(L"Ctrl+S");
    place(slots[kButton] = button, 12, 64, 180, 96);
    auto* icon = root->AddChild<DxUi::Button>(L"");
    icon->SetVariant(DxUi::ButtonVariant::IconOnly);
    icon->SetAccessibleName(L"Add");
    auto* link = root->AddChild<DxUi::Button>(L"A hyperlink");
    link->SetVariant(DxUi::ButtonVariant::Hyperlink);
    place(slots[kLink] = link, 232, 64, 330, 96);
    place(slots[kIconButton] = icon, 190, 64, 226, 96);
    place(slots[kCheckbox] = root->AddChild<DxUi::Checkbox>(L"Check me"), 12, 100, 200, 128);
    place(slots[kToggle] = root->AddChild<DxUi::Toggle>(L"Toggle"), 210, 100, 380, 128);
    place(slots[kField] = root->AddChild<DxUi::TextField>(L"Single line مرحبا"), 12, 134, 300, 166);
    auto* multi = root->AddChild<DxUi::TextField>(L"First line\nSecond line\nThird line with more words");
    multi->SetMultiline(true);
    place(slots[kMultilineField] = multi, 12, 172, 300, 248);
    auto* combo = root->AddChild<DxUi::ComboBox>();
    combo->SetItems({{L"a", L"Alpha"}, {L"b", L"Beta"}, {L"c", L"Gamma ß"}, {L"d", L"Delta"}});
    combo->SetEditable(true);
    combo->SetSelectedIndex(1u);
    place(slots[kCombo] = combo, 310, 134, 470, 166);
    auto* slider = root->AddChild<DxUi::Slider>();
    slider->SetMinimum(0.0);
    slider->SetMaximum(100.0);
    slider->SetValue(40.0);
    place(slots[kSlider] = slider, 310, 172, 470, 204);
    auto* progress = root->AddChild<DxUi::ProgressBar>();
    progress->SetValue(0.35);
    place(slots[kProgress] = progress, 310, 212, 470, 228);
    auto* tabs = root->AddChild<DxUi::TabControl>();
    tabs->AddTab<DxUi::Label>(L"First tab", L"Page one");
    tabs->AddTab<DxUi::Label>(L"Second ü", L"Page two");
    place(slots[kTabs] = tabs, 12, 256, 300, 340);
    auto* gridControl = root->AddChild<DxUi::Grid>();
    gridControl->SetModel(grid.get());
    place(slots[kGrid] = gridControl, 310, 236, 640, 420);
    auto* treeControl = root->AddChild<DxUi::Tree>();
    treeControl->SetModel(tree.get());
    // Two lookups: the first learns that the model keeps ITreeModel's default lookup, the second builds the Tree's id index
    // (W2.6), so the drop and the oracle cover it.
    treeControl->SetFocusedItemId(5u);
    treeControl->SetFocusedItemId(5u);
    place(slots[kTree] = treeControl, 12, 346, 300, 470);
    auto* scroll = root->AddChild<DxUi::ScrollPanel>();
    for (int index = 0; index < 8; ++index)
    {
        auto* item = scroll->AddChild<DxUi::Label>(std::format(L"Scrolled row {}", index));
        item->SetBounds(D2D1::RectF(480, 8.0f + 28.0f * static_cast<float>(index), 620, 32.0f + 28.0f * static_cast<float>(index)));
        if (index == 0)
            slots[kScrollFirst] = item;
    }
    scroll->SetContentHeight(232.0f);
    place(slots[kScroll] = scroll, 480, 8, 640, 120);
    auto* bar = root->AddChild<DxUi::MenuBar>();
    bar->SetItems({{L"File", L'F'}, {L"Edit", L'E'}, {L"View", L'V'}});
    place(slots[kMenuBar] = bar, 310, 424, 640, 452);
    auto* flex = root->AddChild<DxUi::FlexPanel>();
    static_cast<void>(flex->SetOrientation(DxUi::StackOrientation::Horizontal));
    static_cast<void>(flex->SetGap(6.0f));
    flex->AddChild<DxUi::Button>(L"Flex one");
    flex->AddChild<DxUi::Label>(L"Flex label é");
    place(slots[kFlex] = flex, 310, 454, 640, 478);
    scene.SetRoot(std::move(root));
}

[[nodiscard]] inline D2D1_POINT_2F CenterOf(const DxUi::Control& control) noexcept
{
    const D2D1_RECT_F bounds = control.GetBounds();
    return D2D1::Point2F((bounds.left + bounds.right) * 0.5f, (bounds.top + bounds.bottom) * 0.5f);
}

[[nodiscard]] inline D2D1_POINT_2F RandomPointIn(const DxUi::Control& control, Rng& rng) noexcept
{
    const D2D1_RECT_F bounds = control.GetBounds();
    return D2D1::Point2F(bounds.left + 1.0f + (bounds.right - bounds.left - 2.0f) * rng.Unit(),
                         bounds.top + 1.0f + (bounds.bottom - bounds.top - 2.0f) * rng.Unit());
}

// Script "layout": text, bounds, visibility, theme, DPI 96/144/192, LTR/RTL and density changes.
template <typename Scene> void StepLayout(Scene& scene, Rng& rng, size_t step)
{
    auto& slots = scene.slots;
    switch (rng.Below(9u))
    {
        case 0u:
            NoteStep("title text");
            static_cast<DxUi::Label*>(slots[kTitle])->SetText(std::format(L"Title {} é", rng.Below(1000u)));
            break;
        case 1u:
            NoteStep("mnemonic label text");
            static_cast<DxUi::Label*>(slots[kMnemonicLabel])->SetText(rng.Below(2u) ? L"Name with mnemonic" : std::format(L"New name {}", step));
            break;
        case 2u:
            NoteStep("button text");
            static_cast<DxUi::Button*>(slots[kButton])->SetText(std::format(L"Apply {}", rng.Below(100u)));
            break;
        case 3u:
            NoteStep("field text");
            static_cast<DxUi::TextField*>(slots[kField])->SetText(std::format(L"Field {} שלום", rng.Below(1000u)));
            break;
        case 4u:
        {
            NoteStep("bounds");
            // Move or resize one of the leaf controls by a fraction of a DIP or several DIPs.
            constexpr std::array<size_t, 6u> movable{kTitle, kMnemonicLabel, kButton, kCheckbox, kField, kCombo};
            DxUi::Control* control = slots[movable[rng.Below(static_cast<uint32_t>(movable.size()))]];
            D2D1_RECT_F bounds     = control->GetBounds();
            const float dx = (rng.Unit() - 0.5f) * 12.0f, dw = (rng.Unit() - 0.5f) * 20.0f;
            bounds.left += dx;
            bounds.right = (std::max)(bounds.left + 24.0f, bounds.right + dx + dw);
            control->SetBounds(bounds);
            break;
        }
        case 5u:
        {
            NoteStep("visibility");
            constexpr std::array<size_t, 5u> hideable{kIconButton, kToggle, kProgress, kTree, kMultilineField};
            DxUi::Control* control = slots[hideable[rng.Below(static_cast<uint32_t>(hideable.size()))]];
            control->SetVisible(! control->IsVisible());
            break;
        }
        case 6u:
            NoteStep("theme");
            scene.Host().SetTheme(ParityTheme(! scene.Host().GetTheme().dark));
            break;
        case 7u:
        {
            NoteStep("dpi");
            constexpr std::array<UINT, 3u> dpis{96u, 144u, 192u};
            scene.SetDpi(dpis[rng.Below(3u)]);
            break;
        }
        default:
        {
            NoteStep("flow direction or density");
            DxUi::Control* root = slots[kRoot];
            if (rng.Below(2u))
                root->SetFlowDirection(root->IsRightToLeft() ? DxUi::FlowDirection::LeftToRight : DxUi::FlowDirection::RightToLeft);
            else
                root->SetDensity(root->IsCompactDensity() ? DxUi::Density::Standard : DxUi::Density::Compact);
            break;
        }
    }
}

// Script "interaction": hover, focus, press, capture (a slider drag), typing, keys, wheel scrolling, the ComboBox popup,
// time on the frozen clock (caret, tooltip delay), and detaching a hovered control.
template <typename Scene> void StepInteraction(Scene& scene, Rng& rng, size_t /*step*/)
{
    auto& slots = scene.slots;
    constexpr std::array<size_t, 12u> targets{
        kButton, kIconButton, kCheckbox, kToggle, kField, kMultilineField, kCombo, kSlider, kTabs, kGrid, kTree, kMenuBar};
    const auto pick = [&]() -> DxUi::Control*
    {
        DxUi::Control* control = slots[targets[rng.Below(static_cast<uint32_t>(targets.size()))]];
        return control && control->IsVisible() ? control : slots[kButton];
    };
    switch (rng.Below(12u))
    {
        case 0u:
        case 1u:
            NoteStep("hover move");
            scene.Pointer(ParityPointer::Move, RandomPointIn(*pick(), rng));
            break;
        case 2u:
        {
            NoteStep("click");
            // A click: press and release at one point (no menu bar: it opens a native menu).
            DxUi::Control* control = pick();
            if (control == slots[kMenuBar])
                control = slots[kCheckbox];
            const D2D1_POINT_2F point = RandomPointIn(*control, rng);
            scene.Pointer(ParityPointer::Move, point);
            scene.Pointer(ParityPointer::Down, point);
            scene.Pointer(ParityPointer::Up, point);
            break;
        }
        case 3u:
        {
            NoteStep("captured slider drag");
            // Captured drag across steps: press on the slider now, move while captured, release on a later step.
            auto* slider = slots[kSlider];
            if (! scene.dragging)
            {
                const D2D1_POINT_2F point = CenterOf(*slider);
                scene.Pointer(ParityPointer::Move, point);
                scene.Pointer(ParityPointer::Down, point);
                scene.dragging = true;
            }
            const D2D1_RECT_F bounds = slider->GetBounds();
            scene.Pointer(ParityPointer::Move,
                          D2D1::Point2F(bounds.left - 20.0f + (bounds.right - bounds.left + 40.0f) * rng.Unit(), CenterOf(*slider).y + 30.0f * rng.Unit()));
            if constexpr (! Scene::kCrossStepCapture)
            {
                // Win32 mouse capture belongs to one window of the thread: a drag held across steps in one native scene
                // would be cancelled by the other scene's press, so native drags complete within their step.
                scene.Pointer(ParityPointer::Up, CenterOf(*slider));
                scene.dragging = false;
            }
            break;
        }
        case 4u:
            NoteStep("release or leave");
            if (scene.dragging)
            {
                scene.Pointer(ParityPointer::Up, CenterOf(*slots[kSlider]));
                scene.dragging = false;
            }
            else
                scene.Pointer(ParityPointer::Leave, {});
            break;
        case 5u:
            NoteStep("focus");
            scene.Host().SetFocusControl(pick(), false);
            break;
        case 6u:
        {
            NoteStep("character");
            constexpr std::wstring_view typed = L"ab é中";
            scene.Character(typed[rng.Below(static_cast<uint32_t>(typed.size()))]);
            break;
        }
        case 7u:
        {
            NoteStep("key");
            constexpr std::array<UINT, 8u> keys{VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_HOME, VK_END, VK_BACK, VK_SPACE};
            scene.Key(keys[rng.Below(static_cast<uint32_t>(keys.size()))]);
            break;
        }
        case 8u:
        {
            NoteStep("wheel");
            constexpr std::array<size_t, 4u> scrollers{kGrid, kTree, kScroll, kMultilineField};
            DxUi::Control* control = slots[scrollers[rng.Below(static_cast<uint32_t>(scrollers.size()))]];
            if (control->IsVisible())
                scene.Pointer(ParityPointer::Wheel, RandomPointIn(*control, rng), rng.Below(2u) ? -120.0f : 120.0f);
            break;
        }
        case 9u:
        {
            NoteStep("combo popup");
            // The ComboBox popup: open with F4 while focused, close with Escape.
            auto* combo = static_cast<DxUi::ComboBox*>(slots[kCombo]);
            scene.Host().SetFocusControl(combo, false);
            scene.Key(combo->IsPopupOpen() ? VK_ESCAPE : VK_F4);
            break;
        }
        case 10u:
            NoteStep("clock");
            scene.Advance(80u + rng.Below(700u));
            break;
        default:
        {
            NoteStep("detach hovered");
            // Detach during hover: hover a scrolled row, then remove the panel's first child while it is hovered.
            auto* scroll = static_cast<DxUi::ScrollPanel*>(slots[kScroll]);
            if (scroll->GetLogicalChildCount() > 2u)
            {
                if (DxUi::Control* first = scroll->GetLogicalChild(0u))
                    scene.Pointer(ParityPointer::Move, CenterOf(*first));
                std::unique_ptr<DxUi::Control> taken = scroll->TakeChild(0u);
                static_cast<void>(scroll->CompactChildren());
                taken.reset();
            }
            break;
        }
    }
}

template <typename Scene> void BuildRichScene(Scene& scene)
{
    scene.Host().SetTheme(ParityTheme(false));
    auto root    = std::make_unique<DxUi::Panel>();
    auto& slots  = scene.slots;
    slots[kRoot] = root.get();
    auto* editor = root->AddChild<DxUi::RichTextEditor>();
    editor->SetBounds(D2D1::RectF(12, 12, 620, 440));
    editor->SetMarkdownAnalysisProfile(DxUi::RichText::MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    editor->SetText(L"# Parity heading\r\n\r\nSome **bold** and *italic* text with `code` and 東京.\r\n\r\n"
                    L"- First item\r\n- [x] Done item\r\n\r\n| A | B |\r\n| --- | --- |\r\n| one | two |\r\n\r\n"
                    L"> Quoted שלום text\r\n\r\n```cpp\r\nint value = 1;\r\n```\r\n\r\nLast paragraph.\r\n");
    slots[1] = editor;
    scene.SetRoot(std::move(root));
    scene.Host().SetFocusControl(editor, false);
}

// Script "rich": typing, line breaks, deletions, caret movement, a Markdown command and width changes in a rich editor.
template <typename Scene> void StepRich(Scene& scene, Rng& rng, size_t /*step*/)
{
    auto* editor = static_cast<DxUi::RichTextEditor*>(scene.slots[1]);
    switch (rng.Below(7u))
    {
        case 0u:
        case 1u:
        {
            NoteStep("rich character");
            constexpr std::wstring_view typed = L"xy zé*";
            scene.Character(typed[rng.Below(static_cast<uint32_t>(typed.size()))]);
            break;
        }
        case 2u:
            NoteStep("rich enter");
            scene.Key(VK_RETURN);
            break;
        case 3u:
            NoteStep("rich backspace");
            scene.Key(VK_BACK);
            break;
        case 4u:
        {
            NoteStep("rich caret key");
            constexpr std::array<UINT, 6u> keys{VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_HOME, VK_END};
            scene.Key(keys[rng.Below(static_cast<uint32_t>(keys.size()))]);
            break;
        }
        case 5u:
            NoteStep("rich bold command");
            static_cast<void>(editor->ExecuteMarkdownCommand({.command = DxUi::RichText::MarkdownCommand::ToggleStyle}));
            break;
        default:
        {
            NoteStep("rich width");
            D2D1_RECT_F bounds = editor->GetBounds();
            bounds.right       = 360.0f + 260.0f * rng.Unit();
            editor->SetBounds(bounds);
            break;
        }
    }
}

// Script "richBlocks" (W7.1): the "rich" steps on a document with every block kind the semantic builder reuses (math,
// a diagram, a callout, native HTML, nested lists, a table and fences), so typing and Enter edit one block while the
// fresh scene rebuilds the whole semantic document and the incremental one reuses the rest.
template <typename Scene> void BuildRichBlocksScene(Scene& scene)
{
    BuildRichScene(scene);
    auto* editor = static_cast<DxUi::RichTextEditor*>(scene.slots[1]);
    editor->SetText(L"# Blocks\r\n\r\nInline $a^2$ math and **bold** text.\r\n\r\n$$\r\nx + y\r\n$$\r\n\r\n"
                    L"> [!note] Callout\r\n> body text\r\n\r\n- one\r\n  - nested\r\n- [ ] task\r\n\r\n"
                    L"<p align=\"center\">html <b>bold</b></p>\r\n\r\n```mermaid\r\nflowchart TD\r\nA-->B\r\n```\r\n\r\n"
                    L"| A | B |\r\n| --- | --- |\r\n| 1 | 2 |\r\n\r\n```cpp\r\nint value = 1;\r\n```\r\n\r\nLast paragraph.\r\n");
}

// Script "richObjects" (W2.3): every prepared object kind (display and inline formulas, two equal formulas, a diagram, a
// raster image, an SVG) edited before, inside and after each object, objects deleted and re-inserted (undo), a duplicate
// image added and the width changed, so the incremental scene carries prepared objects across edits while the fresh one
// prepares every object again. Both scenes complete the editor's image requests with the same decoded pictures before each
// step, as an application would after each preparation. The steps are fixed; the seed only names the run.
struct ParityPictures final
{
    std::shared_ptr<const DxUi::RichText::RichImage> raster, vector;
};
[[nodiscard]] inline const ParityPictures& RichObjectPictures()
{
    static const ParityPictures pictures = []
    {
        const HRESULT apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const auto uninitialize = wil::scope_exit([&]() noexcept
        {
            if (SUCCEEDED(apartment))
                CoUninitialize();
        });
        ParityPictures result;
        std::array<std::byte, 8u * 6u * 4u> pixels{};
        for (size_t index = 0u; index < pixels.size(); index += 4u)
        {
            pixels[index]      = std::byte{static_cast<unsigned char>(index * 7u % 256u)};
            pixels[index + 1u] = std::byte{0x90};
            pixels[index + 2u] = std::byte{static_cast<unsigned char>(255u - index * 3u % 256u)};
            pixels[index + 3u] = std::byte{0xff};
        }
        if (FAILED(DxUi::RichText::RichImage::CreatePbgra(8u, 6u, pixels, result.raster)))
            Fail("the richObjects raster picture is created");
        constexpr std::string_view svg = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 40 30'><rect width='40' height='30' fill='#2060c0'/>"
                                         "<circle cx='20' cy='15' r='9' fill='#f0c020'/></svg>";
        if (FAILED(DxUi::RichText::RichImage::CreateSvg(40u, 30u, std::as_bytes(std::span(svg)), result.vector)))
            Fail("the richObjects SVG picture is created");
        return result;
    }();
    return pictures;
}
template <typename Scene> void ServiceRichObjectImages(Scene& scene)
{
    auto* editor = static_cast<DxUi::RichTextEditor*>(scene.slots[1]);
    std::vector<DxUi::RichText::RichImageRequest> requests;
    editor->TakePendingImageRequests(requests);
    for (const auto& request : requests)
        static_cast<void>(
            editor->CompleteImageRequest(request, request.Source().ends_with(L".svg") ? RichObjectPictures().vector : RichObjectPictures().raster));
}
template <typename Scene> void BuildRichObjectsScene(Scene& scene)
{
    BuildRichScene(scene);
    auto* editor = static_cast<DxUi::RichTextEditor*>(scene.slots[1]);
    editor->SetDocumentFilePath(L"C:\\parity\\objects.md"); // resolves the relative image sources; never opened
    editor->SetSvgImagesEnabled(true);
    editor->SetText(
        L"Intro paragraph.\n\n$$\nx + y\n$$\n\n$$\nx + y\n$$\n\n```mermaid\nflowchart TD\nA[Start]-->B[Stop]\nstyle A fill:#204080,color:#ffffff\n```\n\n"
        L"Inline $a^2$ formula.\n\n![raster](picture.png)\n\n![vector](shape.svg)\n\nLast paragraph.\n");
    editor->SetSelection({0u, 0u});
}
template <typename Scene> void StepRichObjects(Scene& scene, Rng& /*rng*/, size_t step)
{
    ServiceRichObjectImages(scene);
    auto* editor       = static_cast<DxUi::RichTextEditor*>(scene.slots[1]);
    const auto text    = editor->GetText();
    const auto caretAt = [&](std::wstring_view marker, size_t offset)
    {
        const size_t at = text.find(marker);
        if (at == std::wstring::npos)
            Fail(std::format("richObjects step {}: marker not found", step));
        editor->SetSelection({at + offset, at + offset});
    };
    const auto remove = [&](std::wstring_view block)
    {
        const size_t at = text.find(block);
        if (at == std::wstring::npos)
            Fail(std::format("richObjects step {}: block not found", step));
        editor->SetSelection({at, at + block.size()});
        static_cast<void>(editor->ReplaceSelection(L""));
    };
    switch (step)
    {
        case 1u: NoteStep("objects: images completed"); break;
        case 2u:
            NoteStep("objects: type before every object");
            caretAt(L"Intro paragraph", 15u);
            scene.Character(L'x');
            break;
        case 3u:
            NoteStep("objects: Enter before every object");
            scene.Key(VK_RETURN);
            scene.Character(L'n');
            break;
        case 4u:
            NoteStep("objects: type inside the first formula");
            caretAt(L"x + y", 5u);
            scene.Character(L'z');
            break;
        case 5u:
            NoteStep("objects: type after every object");
            caretAt(L"Last paragraph", 14u);
            scene.Character(L'q');
            break;
        case 6u:
            NoteStep("objects: delete the second formula");
            remove(L"$$\nx + y\n$$\n\n");
            break;
        case 7u:
            NoteStep("objects: undo re-inserts the formula");
            static_cast<void>(editor->Undo());
            break;
        case 8u:
            NoteStep("objects: add a duplicate formula and a duplicate image");
            caretAt(L"Last paragraph", 0u);
            static_cast<void>(editor->ReplaceSelection(L"$$\nx + y\n$$\n\n![again](picture.png)\n\n"));
            break;
        case 9u:
        {
            NoteStep("objects: width narrower");
            D2D1_RECT_F bounds = editor->GetBounds();
            bounds.right       = bounds.left + 300.0f;
            editor->SetBounds(bounds);
            break;
        }
        case 10u:
            NoteStep("objects: type inside the diagram");
            caretAt(L"B[Stop]", 6u);
            scene.Character(L's');
            break;
        case 11u:
            NoteStep("objects: delete the diagram");
            remove(L"```mermaid\nflowchart TD\nA[Start]-->B[Stops]\nstyle A fill:#204080,color:#ffffff\n```\n\n");
            break;
        case 12u:
            NoteStep("objects: undo re-inserts the diagram");
            static_cast<void>(editor->Undo());
            break;
        case 13u:
            NoteStep("objects: delete the first raster image");
            remove(L"![raster](picture.png)\n\n");
            break;
        case 14u:
            NoteStep("objects: delete the SVG");
            remove(L"![vector](shape.svg)\n\n");
            break;
        case 15u:
            NoteStep("objects: undo twice re-inserts both images");
            static_cast<void>(editor->Undo());
            static_cast<void>(editor->Undo());
            break;
        case 16u:
            NoteStep("objects: type inside the inline formula");
            caretAt(L"$a^2$", 4u);
            scene.Character(L'3');
            break;
        default:
        {
            NoteStep("objects: width restored and prose typed");
            D2D1_RECT_F bounds = editor->GetBounds();
            bounds.right       = 620.0f;
            editor->SetBounds(bounds);
            caretAt(L"Intro paragraph", 0u);
            scene.Character(L'w');
            break;
        }
    }
}

inline constexpr std::array<uint64_t, 3u> kLayoutSeeds{0x5EED00A1u, 0x5EED00A2u, 0x5EED00A3u};
inline constexpr std::array<uint64_t, 3u> kInteractionSeeds{0x5EED00B1u, 0x5EED00B2u, 0x5EED00B3u};
inline constexpr std::array<uint64_t, 2u> kRichSeeds{0x5EED00C1u, 0x5EED00C2u};

template <typename Scene> [[nodiscard]] ParityScript<Scene> LayoutScript() noexcept
{
    return {"layout", kLayoutSeeds, 24u, &BuildControlsScene<Scene>, &StepLayout<Scene>};
}
template <typename Scene> [[nodiscard]] ParityScript<Scene> InteractionScript() noexcept
{
    return {"interaction", kInteractionSeeds, 32u, &BuildControlsScene<Scene>, &StepInteraction<Scene>};
}
template <typename Scene> [[nodiscard]] ParityScript<Scene> RichScript() noexcept
{
    return {"rich", kRichSeeds, 16u, &BuildRichScene<Scene>, &StepRich<Scene>};
}
inline constexpr std::array<uint64_t, 2u> kRichBlocksSeeds{0x5EED00D1u, 0x5EED00D2u};
template <typename Scene> [[nodiscard]] ParityScript<Scene> RichBlocksScript() noexcept
{
    return {"richBlocks", kRichBlocksSeeds, 24u, &BuildRichBlocksScene<Scene>, &StepRich<Scene>};
}
inline constexpr std::array<uint64_t, 1u> kRichObjectsSeeds{0x5EED00E1u};
template <typename Scene> [[nodiscard]] ParityScript<Scene> RichObjectsScript() noexcept
{
    return {"richObjects", kRichObjectsSeeds, 18u, &BuildRichObjectsScene<Scene>, &StepRichObjects<Scene>};
}

// Scripts "richChunks" and "richChunksMultiplier" (W7.3): the v0 document layout's position-independent chunks in a
// rich editor, Source mode and Rich mode without Markdown rendering at line height 1.5, with a fractional font size and a
// document of many chunks: typing, Enter and Backspace above later chunks (which move without being laid again), width
// changes (viewport-first preparation with previous chunks shown meanwhile) and scrolling far down (large origins). The
// fresh scene prepares with the incremental scene's chunk starts (Detail::testRichRecordChunkPlan, set by the test that
// runs the script), so the comparison is "a fresh preparation with the same chunking".
template <typename Scene> void BuildRichChunksSceneWith(Scene& scene, bool multiplier)
{
    scene.Host().SetTheme(ParityTheme(false));
    auto root    = std::make_unique<DxUi::Panel>();
    auto& slots  = scene.slots;
    slots[kRoot] = root.get();
    auto* editor = root->AddChild<DxUi::RichTextEditor>();
    editor->SetBounds(D2D1::RectF(12, 12, 560, 420));
    std::wstring text;
    for (size_t line = 0u; text.size() < 9000u; ++line)
        text += std::format(L"Line {} carries ordinary words, **bold** marks and \u05d0\u05d1\u05d2 so that it wraps at narrow widths.\r\n", line);
    editor->SetText(text);
    auto appearance            = editor->GetAppearance();
    appearance.bodyFontSizeDip = 15.25f;
    appearance.codeFontSizeDip = 14.75f;
    if (multiplier)
    {
        appearance.lineHeightMultiplier = 1.5f;
        editor->SetMarkdownRenderingEnabled(false);
    }
    else
        editor->SetViewMode(DxUi::RichText::ViewMode::Source);
    editor->SetAppearance(appearance);
    editor->SetSelection({200u, 200u});
    slots[1] = editor;
    scene.SetRoot(std::move(root));
    scene.Host().SetFocusControl(editor, false);
}
template <typename Scene> void BuildRichChunksScene(Scene& scene)
{
    BuildRichChunksSceneWith(scene, false);
}
template <typename Scene> void BuildRichChunksMultiplierScene(Scene& scene)
{
    BuildRichChunksSceneWith(scene, true);
}
template <typename Scene> void StepRichChunks(Scene& scene, Rng& rng, size_t /*step*/)
{
    auto* editor = static_cast<DxUi::RichTextEditor*>(scene.slots[1]);
    switch (rng.Below(6u))
    {
        case 0u:
        case 1u:
            NoteStep("chunk character");
            scene.Character(L"ab\u05d3 "[rng.Below(4u)]);
            break;
        case 2u:
            NoteStep("chunk enter");
            scene.Key(VK_RETURN);
            break;
        case 3u:
            NoteStep("chunk backspace");
            scene.Key(VK_BACK);
            break;
        case 4u:
        {
            NoteStep("chunk width");
            D2D1_RECT_F bounds = editor->GetBounds();
            bounds.right       = 300.0f + 260.0f * rng.Unit();
            editor->SetBounds(bounds);
            break;
        }
        default:
            NoteStep("chunk scroll");
            static_cast<void>(editor->SetScrollOffsetDip(rng.Unit() * 40000.0f));
            break;
    }
}
inline constexpr std::array<uint64_t, 2u> kRichChunksSeeds{0x5EED00F1u, 0x5EED00F2u};
template <typename Scene> [[nodiscard]] ParityScript<Scene> RichChunksScript() noexcept
{
    return {"richChunks", kRichChunksSeeds, 20u, &BuildRichChunksScene<Scene>, &StepRichChunks<Scene>};
}
template <typename Scene> [[nodiscard]] ParityScript<Scene> RichChunksMultiplierScript() noexcept
{
    return {"richChunksMultiplier", kRichChunksSeeds, 20u, &BuildRichChunksMultiplierScene<Scene>, &StepRichChunks<Scene>};
}

// The registry's rows read populated before the drop on the controls and rich scenes together, except those the scenes
// cannot populate (listed with the reason). A row that reads 0 on a scene that exercises it is not counting what it drops.
inline constexpr std::array<std::string_view, 7u> kRowsNotPopulatedByScenes{
    "ComboBox._popupBackdropBitmap",    // Solid overlay material: no backdrop is captured.
    "ComboBox._popupItemWidthCache",    // Measured only for an open popup; the combo-popup and interaction scripts open it.
    "ComboBox._popupLayoutCache",       // A bounds change drops the placement and a closed popup is not placed again (same scripts).
    "ColorPicker._saturationBrush",     // No ColorPicker in the scenes (its own suite covers its brushes).
    "TooltipLayer._layoutCache",        // No tooltip is shown before the drop.
    "ControlHost._richTitleHoverCache", // No pointer hovers a rich editor before the drop (W7.9 tests cover it).
    "RichText.mathFontData", // Thread state: the drop of any scene empties it and a carried formula reads no font again (W7.7 tests fill and drop it).
};
} // namespace IncrementalParity
