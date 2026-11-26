#pragma once

#include "DxUi.TextElements.h"
#include "DxUi.h"
#include "Support/CountedCreation.h"
#include <DxUi/Failure.h>

#include <algorithm>
#include <bit>
#include <exception>
#include <new>
#include <oleauto.h>
#include <span>
#include <utility>

namespace DxUi
{
// The animation clock's milliseconds from a stamp to a tick; zero when the tick is not after the stamp.
[[nodiscard]] constexpr uint64_t ElapsedTickMs(uint64_t startTickMs, uint64_t nowTickMs) noexcept
{
    return nowTickMs > startTickMs ? nowTickMs - startTickMs : 0u;
}

// The animation-clock time of a blinking caret's next phase flip after nowTickMs: phases are counted from the anchor, so
// a late wake never shifts the ones after it. Saturates at the top of the clock.
[[nodiscard]] constexpr uint64_t NextCaretBlinkFlipTickMs(uint64_t anchorTickMs, uint64_t nowTickMs, uint64_t periodMs) noexcept
{
    const uint64_t phases   = ElapsedTickMs(anchorTickMs, nowTickMs) / periodMs + 1u;
    const uint64_t offsetMs = phases > UINT64_MAX / periodMs ? UINT64_MAX : phases * periodMs;
    return offsetMs > UINT64_MAX - anchorTickMs ? UINT64_MAX : anchorTickMs + offsetMs;
}

// The caret owners' view of their host's caret clock (Specs/Rendering/Rendering_Win32Host.md, "Caret clock"). A focused
// TextField, editable ComboBox or RichTextEditor tells it "focus gained or input happened" (Restart), "focus lost" (Stop),
// forwards its scheduled Tick, and asks at paint whether the caret is shown. `editable` false is a steady caret.
struct CaretClockAccess final
{
    static void Restart(ControlHost& host, const Control& owner, bool editable) noexcept
    {
        host.RestartCaret(owner, editable);
    }
    static void Stop(ControlHost& host, const Control& owner) noexcept
    {
        host.StopCaret(owner);
    }
    [[nodiscard]] static bool Tick(ControlHost& host, const Control& owner, uint64_t nowTickMs) noexcept
    {
        return host.TickCaret(owner, nowTickMs);
    }
    [[nodiscard]] static bool Shown(const ControlHost& host, const Control& control) noexcept
    {
        return host.IsCaretShown(control);
    }
};

inline constexpr float kToggleTrackWidthDip           = 34.0f;
inline constexpr float kToggleTrackHeightDip          = 20.0f;
inline constexpr float kToggleKnobDiameterDip         = 16.0f;
inline constexpr float kToggleKnobInsetDip            = 2.0f;
inline constexpr float kToggleRowPaddingXDip          = 6.0f;
inline constexpr float kToggleTextTrackGapDip         = 8.0f;
inline constexpr float kToggleTrackTrailingPaddingDip = 4.0f;

[[nodiscard]] std::weak_ptr<int> GetControlLifetimeToken(const Control& control) noexcept;

// Win32 key-transition policy shared by both editors. Pure Unicode/grapheme
// services stay in TextElements; packet and bare modifier keys do not replace
// an unfinished UTF-16 scalar. Editing/navigation keys still cancel it.
[[nodiscard]] constexpr bool KeyKeepsPendingSurrogate(UINT key) noexcept
{
    switch (key)
    {
        case VK_PACKET:
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
        case VK_LWIN:
        case VK_RWIN: return true;
        default: return false;
    }
}

struct ControlCaptureAccess
{
    [[nodiscard]] static Control* GestureOwner(const ControlHost& host) noexcept
    {
        Control* owner = host.GetCapturedControl();
        while (const auto* panel = dynamic_cast<const ScrollPanel*>(owner))
        {
            Control* const child = panel->LiveInnerCaptured();
            if (! child || child == owner)
                break;
            owner = child;
        }
        return owner;
    }
};

// A cancellation callback can replace the tree or capture another control. Release only the cancelling control's
// unchanged gesture, including nested ScrollPanel forwarding. Weak tokens reject address reuse after destruction.
[[nodiscard]] inline auto ReleaseUnchangedCaptureOnExit(ControlHost& host, Control& cancelling) noexcept
{
    Control* const gesture     = &cancelling;
    Control* const captured    = ControlCaptureAccess::GestureOwner(host) == gesture ? host.GetCapturedControl() : nullptr;
    const auto lifetime        = captured ? GetControlLifetimeToken(*captured) : std::weak_ptr<int>{};
    const auto gestureLifetime = GetControlLifetimeToken(cancelling);
    return wil::scope_exit([&host, captured, gesture, lifetime, gestureLifetime]() noexcept
    {
        if (captured && ! lifetime.expired() && ! gestureLifetime.expired() && host.GetCapturedControl() == captured &&
            ControlCaptureAccess::GestureOwner(host) == gesture)
            host.ReleaseMouseCapture();
    });
}

// Declared again, with its contract, under Incremental publication below.
void NoteAccessibilityGeometryMove(const Control& control, uint64_t interactionBefore, uint64_t hitBefore) noexcept;
struct ControlModelQueryAccess
{
    [[nodiscard]] static std::wstring_view RadioGroupHeader(const RadioButtons& group) noexcept
    {
        return group._header;
    }
    // The library control class `control` is or derives from (W8.3), as its constructor recorded it.
    [[nodiscard]] static Detail::AccessibilityControlKind AccessibilityKind(const Control& control) noexcept
    {
        return static_cast<Detail::AccessibilityControlKind>(control._accessibilityKind);
    }
    // Called by a library control class's constructor, after its bases': the most derived library class wins.
    static void SetAccessibilityKind(Control& control, Detail::AccessibilityControlKind kind) noexcept
    {
        control._accessibilityKind = static_cast<uint8_t>(kind);
    }
    // Control::InvalidateHitTest for a scroll offset that moved (HitRevisionSite::ScrollOffset): the hit revision moves, and UI
    // Automation is told of a scroll (NoteAccessibilityScrollMove) rather than of a geometry change.
    static void InvalidateHitTestForScroll(Control& control) noexcept;
    // The repaint of a model owner's geometry change, declared to UI Automation as the marked geometry move it is (W8.2).
    static void DeclaredGeometryInvalidate(const Control& owner) noexcept;
    template <typename Owner> [[nodiscard]] static auto InvalidateGeometry(Owner& owner) noexcept
    {
        // A model owner's rows or geometry changing under a pending or shown tooltip (a Grid cell's or Tree row's) mark it for
        // re-resolution at the resting pointer; the dispatcher learns it when the change is complete.
        bool tooltipTargetStale = false;
        auto* const host        = owner.GetHost();
        if (host)
        {
            const uint64_t interactionBefore = host->_interactionRevision;
            ++host->_interactionRevision;
            // UI Automation republishes the owner's record (its rows and cells follow this geometry) and every rectangle:
            // a marked move, which does not by itself build the whole window (W8.2).
            NoteAccessibilityGeometryMove(owner, interactionBefore, host->_hitRevision);
            tooltipTargetStale = host->MarkTooltipTargetStale();
            // During an intrinsic arrangement, geometry the arranged branch changes about itself (a Grid clamping its scroll
            // to the bounds it was just given) is accepted like the branch's own bounds; anything outside it is foreign. Without
            // this an owner aborted after such a child and left every later sibling unarranged until an unrelated change.
            if (host->_intrinsicArrangeBranch)
            {
                const Control* ancestor = &owner;
                while (ancestor && ancestor != host->_intrinsicArrangeBranch)
                    ancestor = ancestor->_parent;
                if (ancestor)
                    ++host->_intrinsicArrangeBoundsRevision;
                else
                    ++host->_intrinsicArrangeForeignRevision;
            }
        }
        const auto lifetime = owner.GetLifetimeToken();
        // Every exit, including a failed getter, leaves a surviving model owner scheduled for fresh preparation.
        return wil::scope_exit([&owner, lifetime, host, tooltipTargetStale]() noexcept
        {
            if (lifetime.expired())
                return;
            DeclaredGeometryInvalidate(owner);
            if (tooltipTargetStale && ! lifetime.expired() && owner.GetHost() == host)
                host->OnTooltipScheduleChanged();
        });
    }
    // Control::Measure for a container that counts its children's actual measurements: `executed` reports whether
    // MeasureOverride ran (false on a measure-cache hit or on input refused before it).
    [[nodiscard]] static HRESULT MeasureCounted(
        const Control& control, const MeasureContext& context, const LayoutConstraints& constraints, DesiredSize& result, bool& executed) noexcept
    {
        return control.MeasureCounted(context, constraints, result, &executed);
    }
    // Releases a control's measure cache (a hidden container's children; tests comparing with an uncached measure).
    static void DropMeasureCache(const Control& control) noexcept
    {
        control.DropMeasureCache();
    }
    [[nodiscard]] static size_t MeasureCacheEntries(const Control& control) noexcept
    {
        return control.GetMeasureCacheEntries();
    }
    [[nodiscard]] static size_t MeasureCacheBytes(const Control& control) noexcept
    {
        return control.GetMeasureCacheBytes();
    }
    // The logical parent, a PageHost or TabControl included, which GetParent (Panels only, and protected) is not.
    [[nodiscard]] static const Control* LogicalParent(const Control& control) noexcept
    {
        return control._parent;
    }
    // Every intrinsic layout of a subtree runs again at the next preparation, as after a DPI change: what it measured
    // through a replaced text factory counts no longer (its records key on the typography revision).
    static void RequestIntrinsicLayoutOfTree(Control& control) noexcept
    {
        control.RequestIntrinsicLayout();
        for (size_t index = 0u; index < control.GetLogicalChildCount(); ++index)
        {
            if (Control* const child = control.GetLogicalChild(index))
                RequestIntrinsicLayoutOfTree(*child);
        }
    }
    // A TabControl's tabs for UI Automation: each tab's title and identity, the part of its header the strip shows, and the
    // gesture that selects one (with its callback, as a click does).
    [[nodiscard]] static std::span<const TabControl::TabItem> Tabs(const TabControl& tabs) noexcept
    {
        return tabs._tabs;
    }
    // Clipped to the strip's scrolled viewport between the overflow buttons; empty for a header scrolled out of it.
    [[nodiscard]] static D2D1_RECT_F TabHeaderVisibleRect(const TabControl& tabs, size_t index) noexcept
    {
        D2D1_RECT_F rect = tabs.GetTabRect(index);
        rect.left        = (std::max)(rect.left, tabs.GetHeaderViewportLeft());
        rect.right       = (std::min)(rect.right, tabs.GetHeaderViewportRight());
        return rect.right > rect.left && rect.bottom > rect.top ? rect : D2D1::RectF();
    }
    [[nodiscard]] static bool SelectTab(TabControl& tabs, ControlHost& host, size_t index, bool focusSelf) noexcept
    {
        return tabs.SelectTab(host, index, focusSelf);
    }
    // A page indicator's page chosen as a click or an arrow key chooses it, with its selection callback.
    static void SelectPage(PageIndicator& indicator, ControlHost& host, uint32_t index)
    {
        indicator.SelectIndex(host, index);
    }
    // The model column at each display position, every column included; empty without a model. Reads the model.
    [[nodiscard]] static std::vector<size_t> GridColumnDisplayOrder(const Grid& grid)
    {
        grid.EnsureColumnWidths();
        return grid._columnDisplayOrder;
    }
    // Scrolls the grid until the row, and the column when given, is in its viewport. Runs the model's getters.
    static void RevealGridItem(Grid& grid, size_t rowIndex, std::optional<size_t> columnIndex)
    {
        grid.EnsureRowVisible(rowIndex);
        if (columnIndex)
            static_cast<void>(grid.RevealColumn(*columnIndex));
    }
    [[nodiscard]] static uint64_t Revision(const Tree& owner) noexcept
    {
        return owner._modelRevision;
    }
    [[nodiscard]] static uint64_t Revision(const Grid& owner) noexcept
    {
        return owner._modelRevision;
    }
    [[nodiscard]] static uint64_t BindingRevision(const Tree& owner) noexcept
    {
        return owner._modelBindingRevision;
    }
    [[nodiscard]] static uint64_t BindingRevision(const Grid& owner) noexcept
    {
        return owner._modelBindingRevision;
    }
    [[nodiscard]] static uint64_t GeometryRevision(const Control& owner) noexcept
    {
        const auto* host = owner.GetHost();
        return host ? host->_interactionRevision : 0u;
    }
    [[nodiscard]] static const ControlHost* Host(const Control& owner) noexcept
    {
        return owner.GetHost();
    }
};

// Shared by model reads and delegate dispatch. The revision also rejects replacing a model and restoring its pointer.
template <typename Owner, typename Model> class BorrowedControlModelGuard final
{
public:
    BorrowedControlModelGuard(const Owner& owner, std::weak_ptr<int> lifetime, const Model* model) noexcept
        : _owner(&owner),
          _lifetime(std::move(lifetime)),
          _model(model),
          _revision(ControlModelQueryAccess::Revision(owner)),
          _bindingRevision(ControlModelQueryAccess::BindingRevision(owner)),
          _host(ControlModelQueryAccess::Host(owner)),
          _geometryRevision(ControlModelQueryAccess::GeometryRevision(owner))
    {
    }

    [[nodiscard]] bool IsCurrent() const noexcept
    {
        return IsBindingCurrent() && ControlModelQueryAccess::Revision(*_owner) == _revision && ControlModelQueryAccess::Host(*_owner) == _host &&
               ControlModelQueryAccess::GeometryRevision(*_owner) == _geometryRevision;
    }

    [[nodiscard]] bool IsBindingCurrent() const noexcept
    {
        return ! _lifetime.expired() && _owner->GetModel() == _model && ControlModelQueryAccess::BindingRevision(*_owner) == _bindingRevision;
    }

    // After a delegate callback, which is application code: the operation goes on while the control lives in the same
    // host with the same model and delegate. A data refresh or a layout change the callback made (its own or any other
    // control's) becomes the new baseline. The caller re-resolves rows by stable id and recomputes geometry before
    // using them again. Strict IsCurrent() stays the rule after model getters and for values derived before a callback.
    [[nodiscard]] bool ResumeAfterCallback() noexcept
    {
        if (! IsBindingCurrent() || ControlModelQueryAccess::Host(*_owner) != _host)
            return false;
        _revision         = ControlModelQueryAccess::Revision(*_owner);
        _geometryRevision = ControlModelQueryAccess::GeometryRevision(*_owner);
        return true;
    }

private:
    const Owner* _owner;
    std::weak_ptr<int> _lifetime;
    const Model* _model;
    uint64_t _revision;
    uint64_t _bindingRevision;
    const ControlHost* _host;
    uint64_t _geometryRevision;
};

// Callback refusal is an explicit result owned by the callback contract. Any C++
// exception is unexpected; observe it before unwinding, including unknown types.
// Callers still revalidate borrowed lifetimes after a normally returning callback.
template <typename Callback> [[nodiscard]] bool TryControlCallback(Callback&& callback) noexcept
{
    const Failure::Scope failureScope;
    std::forward<Callback>(callback)();
    return true;
}

// Registration owns the callable; dispatch retains this storage without cloning mutable state or allocating.
template <typename Signature>
void ReplaceControlCallback(std::shared_ptr<const std::function<Signature>>& destination, std::function<Signature> callback) noexcept
{
    const Failure::Scope failureScope;
    auto replacement = callback ? std::make_shared<const std::function<Signature>>(std::move(callback)) : nullptr;
    destination      = std::move(replacement);
}

// Retained text layouts (the Grid's multiline cells and their omitted tails) live in set-associative tables: a mixed
// 64-bit key hash selects a set of kTextLayoutWays entries and the caller confirms a hit by comparing the whole key. A
// table doubles, up to its bound, when the values of the current and the previous paint overflow a set, so no visible
// value evicts another (scrolling back meets the values the last paint drew); the entries a paint did not use release
// their layouts afterwards. Entries have text, keyHash, lastUse and layout members.
inline constexpr size_t kTextLayoutWays = 32u;

// FNV-1a over the UTF-16 units folds in the layout box and the caller's other key values, then MurmurHash3's fmix64
// spreads every bit into the high bits that select the set: FNV-1a's low bits depend only on the low bits of each unit,
// so similar values (the same words with another number) would otherwise crowd a few sets. The hash is the library's
// own and no address enters it, so placement, and with it every allocation count, repeats from run to run and build to
// build.
[[nodiscard]] inline uint64_t HashTextLayoutKey(std::wstring_view text, float width, float height, uint64_t extra) noexcept
{
    uint64_t hash = 0xCBF29CE484222325ull;
    for (const wchar_t unit : text)
    {
        hash ^= static_cast<uint16_t>(unit);
        hash *= 0x100000001B3ull;
    }
    hash ^= (static_cast<uint64_t>(std::bit_cast<uint32_t>(width)) << 32u) | std::bit_cast<uint32_t>(height);
    hash += extra * 0x9E3779B97F4A7C15ull;
    hash ^= hash >> 33u;
    hash *= 0xFF51AFD7ED558CCDull;
    hash ^= hash >> 33u;
    hash *= 0xC4CEB9FE1A85EC53ull;
    hash ^= hash >> 33u;
    return hash;
}

[[nodiscard]] inline size_t FindTextLayoutSetStart(uint64_t keyHash, size_t entryCount) noexcept
{
    const size_t sets = entryCount / kTextLayoutWays; // A power of two.
    return (sets > 1u ? static_cast<size_t>(keyHash >> (64u - static_cast<unsigned>(std::countr_zero(sets)))) : 0u) * kTextLayoutWays;
}

// Grows a reused key or display string to the next power of two, so an entry settles after one allocation and the
// similar values that later reuse it (the same column, another row) copy in place.
inline void ReserveTextStorage(std::wstring& text, size_t size)
{
    if (text.capacity() < size)
        text.reserve(std::bit_ceil(size));
}

// Whether a key string owns heap storage beyond the inline small-string buffer.
[[nodiscard]] inline bool HoldsTextStorage(const std::wstring& text) noexcept
{
    return text.capacity() > std::wstring().capacity();
}

// Moves the entries into a table of entryCount entries: layouts the current paint uses first, so a smaller table only
// drops older ones, then other layouts, then the string storage of released entries, which later keys reuse without
// allocating.
template <typename Entry> void ResizeTextLayoutTable(std::vector<Entry>& table, size_t entryCount, uint64_t generation)
{
    std::vector<Entry> resized(entryCount);
    const auto place = [&resized, entryCount](Entry& entry)
    {
        const auto ways = std::span(resized).subspan(FindTextLayoutSetStart(entry.keyHash, entryCount), kTextLayoutWays);
        const auto free = std::ranges::find_if(ways, [](const Entry& way) { return ! way.layout && ! HoldsTextStorage(way.text); });
        if (free != ways.end())
            *free = std::move(entry);
    };
    for (auto& entry : table)
        if (entry.layout && entry.lastUse == generation)
            place(entry);
    for (auto& entry : table)
        if (entry.layout)
            place(entry);
    for (auto& entry : table)
        if (! entry.layout && HoldsTextStorage(entry.text))
            place(entry);
    table.swap(resized);
}

// The entry holding the key (hit), or the one to rebuild for it: the free way with the most string storage (reusing it
// allocates nothing), else the least recently used layout neither the current nor the previous paint drew, else one
// found after growing the table (at the bound, the least recently used way). Keeping the previous paint's layouts
// matters when a paint meets new values first: scrolling up draws the entering row before the rows still in view, and
// would otherwise evict their layouts just before drawing them.
template <typename Entry, typename Matches>
[[nodiscard]] Entry& FindTextLayoutEntry(
    std::vector<Entry>& table, uint64_t keyHash, uint64_t generation, size_t initialEntries, size_t maxEntries, const Matches& matches, bool& hit)
{
    if (table.empty())
        table.resize(initialEntries);
    for (;;)
    {
        const auto ways = std::span(table).subspan(FindTextLayoutSetStart(keyHash, table.size()), kTextLayoutWays);
        Entry* victim   = nullptr;
        for (auto& entry : ways)
        {
            if (! entry.layout)
            {
                if (! victim || victim->layout || entry.text.capacity() > victim->text.capacity())
                    victim = &entry;
                continue;
            }
            if (entry.keyHash == keyHash && matches(entry))
            {
                hit = true;
                return entry;
            }
            if (entry.lastUse + 1u < generation && (! victim || (victim->layout && entry.lastUse < victim->lastUse)))
                victim = &entry;
        }
        hit = false;
        if (victim)
            return *victim;
        if (table.size() >= maxEntries)
            return *std::ranges::min_element(ways, {}, &Entry::lastUse);
        ResizeTextLayoutTable(table, table.size() * 2u, generation);
    }
}

// After a paint: entries it did not use release their layouts (their string storage serves later keys), and a table
// used below an eighth of its size halves, returning the excess entries and strings.
template <typename Entry, typename Release>
void EndTextLayoutPaint(std::vector<Entry>& table, uint64_t generation, size_t initialEntries, const Release& release) noexcept
{
    size_t used = 0u;
    for (auto& entry : table)
    {
        if (entry.layout && entry.lastUse == generation)
        {
            ++used;
            continue;
        }
        release(entry);
    }
    if (table.size() > initialEntries && used * 8u < table.size())
    {
        const Failure::Scope failureScope;
        ResizeTextLayoutTable(table, table.size() / 2u, generation);
    }
}

// The host's retained layouts of the plain text its controls draw (ControlHost::_textLayoutCache, plan item W2.1), in the
// table above, 32 entries growing to kTextLayoutCacheMaxEntries. A string is keyed by its whole text (compared on a hit,
// which allocates nothing), its format (held) and the exact width and height of its box; a layout of that size drawn at
// the box's origin with the caller's options draws what DrawText draws. A string longer than kMaxRetainedTextLayoutUnits,
// an empty, negative or non-finite box, a draw outside a host paint, and a string meeting a full set at the bound (whose
// ways this paint uses: none is evicted for it) are drawn through DrawText instead, and counted. Each host paint is one
// generation: the entries it did not draw release their layouts when it ends, so the table holds the strings of the last
// paint. A hidden, detached or device-replaced host and a new typography revision keep none.
inline constexpr size_t kTextLayoutCacheInitialEntries = 32u;
// The bound balances memory against layout work (W2.1 receipt, "Bound"). DirectWrite keeps about 18 KB of live heap per
// retained short string (about 27 KB of private bytes with the heap's own retention), which the registry's byte count (the
// table and its key strings) does not see, so 2,048 entries hold about 37 MB of heap (about 55 MB private) at most per
// host, within the developer's envelope of about 50 MB at 2,048; the Grid's ceiling of 16,384 would allow about 290 MB. A table
// takes power-of-two sizes and a set holds 32 ways, so 1,024 entries keep only about 950 of FX12's 1,014 distinct strings
// and lay 67 out again every frame; 2,048 keep all of them, the densest real 1080p and 4K screen measured. Strings beyond
// the bound are drawn through DrawText.
inline constexpr size_t kTextLayoutCacheMaxEntries  = 2048u;
inline constexpr size_t kMaxRetainedTextLayoutUnits = 4096u;
struct TextLayoutCacheAccess final
{
    // DrawHostText: the retained layout of `text` in `format` at `rect`'s size, created on a miss, or DrawText.
    static void Draw(ControlHost& host,
                     ID2D1RenderTarget* target,
                     std::wstring_view text,
                     IDWriteTextFormat* format,
                     const D2D1_RECT_F& rect,
                     ID2D1Brush* brush,
                     D2D1_DRAW_TEXT_OPTIONS options) noexcept;
    // A host paint: nested paints share the outer one's generation. A paint of a region only (W4.10) starts no generation and
    // releases nothing when it ends: the table keeps the strings of the last paint of the whole view.
    static void BeginPaint(ControlHost& host, bool partial = false) noexcept;
    static void EndPaint(ControlHost& host) noexcept;
    // Returns every layout, string and the table itself.
    static void Release(ControlHost& host) noexcept;
    [[nodiscard]] static size_t Entries(const ControlHost& host) noexcept;
    // The table's own storage: its entries and their heap strings (DirectWrite keeps the layouts' storage).
    [[nodiscard]] static uint64_t ResidentBytes(const ControlHost& host) noexcept;
};
// Brackets one host paint of the tree (native render, embedded preparation).
class TextLayoutPaintScope final
{
public:
    explicit TextLayoutPaintScope(ControlHost& host, bool partial = false) noexcept : _host(host)
    {
        TextLayoutCacheAccess::BeginPaint(_host, partial);
    }
    ~TextLayoutPaintScope()
    {
        TextLayoutCacheAccess::EndPaint(_host);
    }
    TextLayoutPaintScope(const TextLayoutPaintScope&)            = delete;
    TextLayoutPaintScope& operator=(const TextLayoutPaintScope&) = delete;
    TextLayoutPaintScope(TextLayoutPaintScope&&)                 = delete;
    TextLayoutPaintScope& operator=(TextLayoutPaintScope&&)      = delete;

private:
    ControlHost& _host;
};

// Layout invalidation scopes (W6.3; UI_ControlsAndLayout.md, "Layout invalidation scopes"). A host preparation pass enters only
// the controls marked kSubtreePending: the control or one below it holds preparation work (intrinsic layout, a PageHost's page
// bounds, content or visual preparation). Every site that sets such work marks the control and its ancestors; the pass
// clears a control's mark when it enters it and sets it again after the visit while work remains below it.
// A property assignment that affects layout (Affects::Layout) defers its ancestors: the control's own revision moves at
// once, and the control waits in its host's queue. Before the next preparation pass and before any measurement is read, the
// queue is resolved: a control whose re-measured result equals, under every constraint its cache recorded since its last
// reset, what its ancestors were given stops there (its ancestors keep their measurements); otherwise its parent is
// invalidated and checked the same way, level by level.
struct LayoutScopeAccess final
{
    static constexpr uint8_t kSubtreePending  = 1u; // Control::_layoutFlags
    static constexpr uint8_t kMeasureDeferred = 2u; // the control waits in its host's queue
    // Queued controls per host; an assignment beyond them invalidates its ancestors at once, as before.
    static constexpr size_t kDeferredMeasureCapacity = 64u;
    // Marks the control and its ancestors up to the first one already marked.
    static void MarkPending(const Control& control) noexcept
    {
        for (const Control* current = &control; current; current = current->_parent)
        {
            if ((current->_layoutFlags & kSubtreePending) != 0u)
                return;
            current->_layoutFlags = static_cast<uint8_t>(current->_layoutFlags | kSubtreePending);
        }
    }
    [[nodiscard]] static bool IsPending(const Control& control) noexcept
    {
        return (control._layoutFlags & kSubtreePending) != 0u;
    }
    static void ClearPending(const Control& control) noexcept
    {
        control._layoutFlags = static_cast<uint8_t>(control._layoutFlags & ~kSubtreePending);
    }
    // The measure invalidation of an assignment: deferred when the control is attached below an intrinsic owner, eager
    // (Control::InvalidateMeasure) otherwise.
    static void InvalidateMeasureOfAssignment(Control& control) noexcept;
    // Leaves the queue: the control moved to another host or none, which invalidates its measure eagerly.
    static void Forget(Control& control) noexcept
    {
        control._layoutFlags = static_cast<uint8_t>(control._layoutFlags & ~kMeasureDeferred);
    }
    // Resolves every queued invalidation of `host` (no-op while resolving or when the queue is empty).
    static void Resolve(ControlHost& host) noexcept;
    [[nodiscard]] static bool HasQueued(const ControlHost& host) noexcept
    {
        return ! host._deferredMeasures.empty() && ! host._resolvingDeferredMeasures;
    }
    // Whether the cache of `control` holds results for its current revision and context with no slot displaced since it was
    // last emptied; if so, marks it as the snapshot a cutoff check compares with (DxUi.Layout.cpp).
    static bool TakeMeasureSnapshot(const Control& control) noexcept;
    // Re-measures every snapshot slot of `control` and returns true when all agree with the snapshot (DxUi.Layout.cpp).
    [[nodiscard]] static bool MeasureUnchanged(Control& control, ControlHost& host) noexcept;
    // Whether the parent never reads the control's measure: a FlexPanel child arranged at FlexChildOptions::explicitSizeDip.
    [[nodiscard]] static bool IsArrangedAtDeclaredSize(const Control& control) noexcept;
    // The measure count a FlexPanel or FormField reports through DebugGetMeasureCount, or nullptr: the Debug verification
    // restores it, so its whole layout stays invisible to the tests that count (DxUi.Layout.cpp).
    [[nodiscard]] static uint64_t* DebugMeasureCount(Control& control) noexcept;
};

// Painted extent, paint region and culling (W4.10; Core_PerformanceAndResources.md, "Paint region"; UI_ControlsAndLayout.md,
// "Painted extent"). A control's painted extent is its bounds plus its reach (Control::GetPaintOverhang); a panel's also covers
// its visible descendants, except a ScrollPanel's, whose Paint clips its content to its own bounds. The extent is trusted only
// for a declared control (a library class exactly, or a class that called DeclarePaintOverhang); anything undeclared, an
// unbounded reach, a page transition, or text whose ink escaped its control makes the extent unbounded, and an invalidation of
// an unbounded extent repaints the whole view.
enum class PaintKind : uint8_t
{
    Unresolved = 0u, // not attached
    Plain,           // derives from no library class: reach 0 by default
    Library,         // any other library control: the focus ring's reach
    Panel,
    ScrollPanel,
    PopupLayer,
    PageHost,
    ComboBox,
};
// The reach of every library control: PaintFocusRing strokes a 2 DIP ring 2 DIP outside the control, 3 DIP in all.
inline constexpr float kLibraryPaintReachDip = 3.0f;
struct PaintExtentAccess final
{
    static constexpr uint8_t kDeclared   = 1u; // Control::_paintFlags: the extent is trusted
    static constexpr uint8_t kInkEscaped = 2u; // the last paint drew text beyond bounds plus reach
    // A library control whose Paint draws all its text under an axis-aligned clip inside its bounds (Tree, Grid, TextField,
    // ComboBox, RichTextEditor, TabControl's headers): its text cannot escape, so its ink is not checked.
    static constexpr uint8_t kClipsText = 4u;
    // A library control whose Paint draws the hover of a control inside it (TagPicker draws its field's frame from its inner
    // ComboBox's hover): a hover change below it adds its own extent too (AddHoverChange).
    static constexpr uint8_t kPaintsInnerHover = 8u;
    // Resolves kind and declaration when a control joins a host (PropagateHost).
    static void Resolve(Control& control) noexcept;
    [[nodiscard]] static bool IsDeclared(const Control& control) noexcept
    {
        return (control._paintFlags & kDeclared) != 0u;
    }
    [[nodiscard]] static PaintKind Kind(const Control& control) noexcept
    {
        return static_cast<PaintKind>(control._paintKind);
    }
    // The control's own extent (bounds plus reach) in its coordinates; false when unbounded or undeclared.
    [[nodiscard]] static bool OwnExtent(const ControlHost& host, const Control& control, D2D1_RECT_F& extent) noexcept;
    // The extent of the control and what it paints below it, in its coordinates (a panel's cached subtree extent); false when
    // unbounded.
    [[nodiscard]] static bool Extent(const ControlHost& host, const Control& control, D2D1_RECT_F& extent) noexcept;
    // The extent mapped through its ancestors to DIPs of the view and clipped to it; false when the whole view must repaint.
    // An empty result (scrolled out, off the view) adds nothing.
    [[nodiscard]] static bool ViewExtent(const ControlHost& host, const Control& control, bool subtree, D2D1_RECT_F& extent) noexcept;
    // Maps `rect` from the coordinates `ancestor`'s children are drawn in to DIPs of the view, through `ancestor` and every
    // control above it (scroll offsets, viewports unless `overlay`), clipped to the view; false when the whole view must repaint.
    [[nodiscard]] static bool MapToView(const ControlHost& host, const Control* ancestor, bool overlay, D2D1_RECT_F& rect) noexcept;
    // The control's painted extent may have changed (bounds, visibility, reach, ink escape): its ancestors' cached extents are
    // stale. Walks up to the first one already stale.
    static void ExtentChanged(const Control& control) noexcept;
    // Adds the control's view extent to its host's region, or marks the region full. Requests no frame.
    static void AddToRegion(ControlHost& host, const Control& control, bool subtree) noexcept;
    // The attributed invalidation: AddToRegion, then a frame request.
    static void Invalidate(ControlHost& host, const Control& control) noexcept;
    // A blink flip of an editor's caret (W4.7): adds only `inkDip` (the caret's ink in `control`'s coordinates) to the region and
    // requests a frame. Falls back to Invalidate where the ink cannot be trusted: a control that is not a library editor whose
    // text cannot escape (`kClipsText`), an unbounded or undeclared extent (an open ComboBox popup), regions switched off.
    static void InvalidateInk(ControlHost& host, const Control& control, const D2D1_RECT_F& inkDip) noexcept;
    // Hover moved onto or off `control` (it and what it paints below it), and every ancestor that paints the hover of a
    // control inside it (kPaintsInnerHover). Requests no frame.
    static void AddHoverChange(ControlHost& host, const Control& control) noexcept;
    // A ScrollPanel's offset moved from `previousOffset`: the overlay content below it (drawn through no viewport) moved with
    // it, so its extent at the old and the new offset joins the region, and the panels above extend again.
    static void ScrollMoved(const ScrollPanel& panel, float previousOffset) noexcept;
    // The scale from DIPs to device pixels of the paint region and the cull: the embedded host's own float DPI (not the integer
    // GetDpi() rounds it to), the window host's DPI otherwise.
    [[nodiscard]] static float PixelScale(const ControlHost& host) noexcept;
    // `rect` (world space under `transform`, which must not rotate or skew) grown to the device pixels it touches, in world
    // space again: an aliased axis-aligned clip to it keeps exactly those pixels.
    [[nodiscard]] static D2D1_RECT_F SnapOutward(const ControlHost& host, const D2D1_MATRIX_3X2_F& transform, const D2D1_RECT_F& rect) noexcept;
    // Adds a rectangle in DIPs of the view.
    static void AddDip(ControlHost& host, const D2D1_RECT_F& rectDip) noexcept;
    static void MarkFull(const ControlHost& host) noexcept;
    // Culling: whether `extent` (in the current world space of `dc`) misses the paint clip.
    [[nodiscard]] static bool MissesClip(const ControlHost& host, const D2D1_MATRIX_3X2_F& transform, const D2D1_RECT_F& extent) noexcept;
    // Whether every device pixel of the clip lies wholly inside `rect` (world space): an inner clip to it changes nothing.
    [[nodiscard]] static bool ClipEnclosedBy(const ControlHost& host, const D2D1_MATRIX_3X2_F& transform, const D2D1_RECT_F& rect) noexcept;
    // Narrows the paint clip to `rect` (world space); the caller restores the previous clip.
    static void IntersectClip(ControlHost& host, const D2D1_MATRIX_3X2_F& transform, const D2D1_RECT_F& rect) noexcept;
    // Ink escape: text drawn while `control` paints, at `inkDip` in its coordinates.
    static void NoteTextInk(ControlHost& host, const D2D1_RECT_F& inkDip) noexcept;
    static void NoteTextInk(ControlHost& host, IDWriteTextLayout* layout, D2D1_POINT_2F origin) noexcept;
    // Text whose ink the host cannot know (DrawText without a layout) counts as escaping.
    static void NoteUnknownTextInk(ControlHost& host) noexcept;
    // The registry row of Panel::_paintExtent (DxUi.RetainedCaches.cpp).
    [[nodiscard]] static size_t CachedExtents(const Control& control) noexcept;
    static void DropCachedExtent(Control& control) noexcept;
    // Restores the paint clip when it ends.
    struct ClipScope final
    {
        explicit ClipScope(ControlHost& host) noexcept;
        ~ClipScope();
        ClipScope(const ClipScope&)            = delete;
        ClipScope& operator=(const ClipScope&) = delete;
        ControlHost& host;
        D2D1_RECT_F clip{};
        bool bounded = false;
    };
    // Sets the paint clip of one frame (a region rectangle, the system's damage or the whole view, in DIPs of the target);
    // ClipScope restores the unbounded clip after it.
    static void SetClip(ControlHost& host, const D2D1_RECT_F& clipDip) noexcept;
    // Brackets one control's own Paint for ink escape; restores the outer control's state.
    // Brackets one control's own Paint for ink escape; restores the outer control's state. An undeclared control's Paint may
    // draw its children anywhere (into a command list, an effect input, another target), so nothing below it is culled:
    // the scope makes the paint clip unbounded for it and restores it after.
    struct PaintingScope final
    {
        PaintingScope(ControlHost& host, const Control& control) noexcept;
        ~PaintingScope();
        PaintingScope(const PaintingScope&)            = delete;
        PaintingScope& operator=(const PaintingScope&) = delete;
        ControlHost& host;
        std::weak_ptr<int> lifetime; // a Paint that retires the control (application code) leaves it untouched
        const Control* outerControl = nullptr;
        D2D1_RECT_F outerLimit{};
        bool outerBounded     = false;
        bool outerEscaped     = false;
        bool clipSuspended    = false; // the control is undeclared: the clip below is unbounded
        bool outerClipBounded = false;
    };
#if DXUI_ENABLE_DIAGNOSTICS
    // The frame verifier's reference paint (test programs only; RetainedCaches.Tests.Hooks.h). While it lives, culling and ink
    // escape are off, and the host's retained caches (brushes, formats, text layouts, transition brushes), its paint region, its
    // new-escape note, the paint observer and the work counters are set aside; its end puts each back exactly as the frame left
    // it, so the reference paint changes nothing a test reads (what it created is released with it).
    class VerificationScope final
    {
    public:
        explicit VerificationScope(ControlHost& host) noexcept;
        ~VerificationScope();
        VerificationScope(const VerificationScope&)            = delete;
        VerificationScope& operator=(const VerificationScope&) = delete;
        VerificationScope(VerificationScope&&)                 = delete;
        VerificationScope& operator=(VerificationScope&&)      = delete;

    private:
        ControlHost& _host;
        Detail::WorkCounters _work;
        void* _observerContext                            = nullptr;
        void (*_observer)(const Control&, void*) noexcept = nullptr;
        ControlHost::PaintRegion _region;
        bool _inkEscapedNew = false;
        // Brushes and formats are shared, not set aside: a brush key draws the exact color it was first asked for, and controls
        // keep layouts made from a format by its identity, so caches of the reference paint's own would paint or lay out
        // differently. Their ages are put back, and the brush keys the reference paint added are removed (nothing holds a
        // keyed brush across frames); a format it added stays, as a whole frame's would.
        std::vector<std::pair<uint32_t, uint64_t>> _brushAges;
        std::vector<std::pair<uint64_t, uint64_t>> _formatAges;
        std::vector<ControlHost::RetainedTextLayout> _layouts;
        std::vector<wil::com_ptr<ID2D1SolidColorBrush>> _transitionBrushes;
        size_t _transitionBrushesUsed = 0u;
    };
#endif
    // Brackets one control's PaintOverlay: the overlay pass follows the culling rule of the paint pass, so below an undeclared
    // control (which may draw its overlay content anywhere) the paint clip is unbounded.
    struct OverlayScope final
    {
        OverlayScope(ControlHost& host, const Control& control) noexcept;
        ~OverlayScope();
        OverlayScope(const OverlayScope&)            = delete;
        OverlayScope& operator=(const OverlayScope&) = delete;
        ControlHost& host;
        bool clipSuspended = false;
    };
};
namespace Detail
{
// Whether paint regions and culling are on (default). Off, every invalidation repaints the whole view and nothing is
// culled: the same-binary comparison of W4.10 (test seam, RetainedCaches.Tests.Hooks.h).
[[nodiscard]] bool PaintRegionsEnabled() noexcept;
// Set only while the frame verifier of a test program (RetainedCaches.Tests.Hooks.h) paints its whole reference frame: that
// paint keeps no ink escape (PaintingScope) and changes no state a frame leaves behind. Only a diagnostics build has the
// verifier (src/Rendering/Embedded.cpp); without diagnostics the name is a compile-time false, so every reader compiles
// unchanged and its branch folds away.
#if DXUI_ENABLE_DIAGNOSTICS
inline thread_local bool g_paintVerificationActive = false;
#else
inline constexpr bool g_paintVerificationActive = false;
#endif
} // namespace Detail
struct safearray_deleter
{
    void operator()(SAFEARRAY* sa) const noexcept
    {
        if (sa)
        {
            SafeArrayDestroy(sa);
        }
    }
};
using unique_safearray = std::unique_ptr<SAFEARRAY, safearray_deleter>;

[[nodiscard]] bool PointInRect(const D2D1_RECT_F& rect, const D2D1_POINT_2F& point) noexcept;
[[nodiscard]] D2D1_RECT_F InflateRect(const D2D1_RECT_F& rect, float amountX, float amountY) noexcept;
[[nodiscard]] float SnapDipToPixel(const ControlHost& host, float dip) noexcept;
[[nodiscard]] D2D1_RECT_F SnapRectToPixel(const ControlHost& host, const D2D1_RECT_F& rect) noexcept;
[[nodiscard]] std::optional<size_t> FindMnemonicTextIndex(std::wstring_view text, wchar_t mnemonic) noexcept;
// Gives `control` the host's focus, first the window's native focus when asked, and says whether the control outlived it.
// The control that loses focus, the focused control's own handler and the host's focus-changed callback run inside, and
// any of them may rebuild the controls: a control that focuses itself touches nothing of its own once this is false.
[[nodiscard]] bool FocusControlAndSurvive(ControlHost& host, Control& control, bool takeNativeFocus = false) noexcept;

[[nodiscard]] D2D1_COLOR_F CompositeOverBackground(const D2D1_COLOR_F& overlay, const D2D1_COLOR_F& background) noexcept;
[[nodiscard]] uint32_t PackColor(const D2D1_COLOR_F& color) noexcept;
[[nodiscard]] D2D1_COLOR_F RainbowTint(std::wstring_view seed, bool dark) noexcept;
[[nodiscard]] D2D1_COLOR_F RainbowMenuSelectionTint(std::wstring_view seed, bool dark) noexcept;
[[nodiscard]] D2D1_COLOR_F RainbowFolderViewSelectionTint(uint32_t stableHash32, bool dark) noexcept;
[[nodiscard]] D2D1_COLOR_F ChooseContrastingTextColor(const D2D1_COLOR_F& background) noexcept;
[[nodiscard]] D2D1_COLOR_F ResolveInactiveSelectionTextColor(const ThemePalette& theme,
                                                             const D2D1_COLOR_F& preferredText,
                                                             const D2D1_COLOR_F& paintedGround) noexcept;
// A list row's focus ring (Tree, Grid). The current row owns it, selected or not: shown while keyboard focus is visible, and
// always in high contrast while the control has focus, in a color that contrasts with the row's painted fill.
struct RowFocusVisuals final
{
    D2D1_COLOR_F color{};
    bool shown = false;
};
[[nodiscard]] RowFocusVisuals ResolveRowFocusVisuals(
    const ThemePalette& theme, const D2D1_COLOR_F& fill, bool selected, bool current, bool focused, bool keyboardFocused) noexcept;
[[nodiscard]] std::wstring_view GetCheckboxCheckGlyph(const ControlHost& host) noexcept;
[[nodiscard]] FontRole GetCheckboxCheckFontRole(const ControlHost& host) noexcept;
[[nodiscard]] DWRITE_READING_DIRECTION ResolveReadingDirection(FlowDirection flowDirection) noexcept;
void ResolveAdornmentColors(const ThemePalette& theme, AdornmentTone tone, D2D1_COLOR_F& fill, D2D1_COLOR_F& text) noexcept;
[[nodiscard]] bool RaiseWindowHostTextInputAutomationEvent(HWND hwnd, const Control* control, TextInputAutomationEventKind kind) noexcept;
void RaiseWindowHostDisclosureChanged(HWND hwnd, const Control* control, bool expanded) noexcept;
// A native menu popup's row focus, raised once its keyboard transition completes (ordinary window hosts announce
// focus from their snapshot changes instead).
void RaiseWindowHostFocusChanged(HWND hwnd, const Control* control) noexcept;
// Immutable shared document text of text input (Specs/UI/UI_InputAndAccessibility.md, "Shared input text"). One buffer is
// held at once by a TextField or editable ComboBox, its undo history, the host's native text-input cache and the TSF store:
// reading, staging, comparing and bounds checks share it, and only an edit makes a new buffer. Invariants relied on wherever
// a buffer is read or written:
// - Every buffer is created by MakeSharedText as a non-const std::wstring. Pointers to const keep sharers from writing;
//   MutableSharedText writes in place only through a buffer MakeSharedText made (std::get_deleter proves it) whose only owner
//   is the caller's pointer, so no other holder can observe the write.
// - A std::wstring_view or raw pointer into a buffer is valid only while a SharedText pinning that buffer is alive. Code that
//   can re-enter a user callback, TSF or SetText while it reads keeps its own SharedText copy for as long as it reads; it never
//   reads through a member that the re-entered code may replace.
// - No weak_ptr to a buffer is ever made, and other threads (UI Automation workers) only receive SharedText copies made on the
//   owner thread. use_count() == 1 therefore means no other thread holds or can gain a reference.
// - Release wipes: when the last owner releases a buffer MakeSharedText made, its deleter zeroes the whole allocation
//   (every unit up to capacity(), not only the current size) before freeing it. An edit never lets std::wstring reallocate a
//   shared buffer in place: MutableSharedText is told the edit's final size and copies to a large enough buffer first, so the
//   old block also leaves through the deleter. Text that shrinks in place keeps stale units beyond its size only until the
//   buffer's release. What is not covered: copies callers make into their own std::wstring (TextInputState, the layout
//   caches, the public snapshots) follow their existing wiping rules.
// - TextField and ComboBox hold a buffer at all times, the shared empty one (EmptySharedText) when they have no text.
// - The shared path (SharedTextInputAccess) reaches a control's text directly only when the control keeps the default
//   ExportTextInputState/ImportTextInputState (KeepsDefaultTextInputState); any override is always called.
// - A RichTextEditor keeps its document in its piece tree, not in a buffer. It holds at most one buffer, a contiguous copy of
//   one source revision for text input (made on demand, or adopted from a TSF import), and only while it has focus: it
//   releases the copy on focus loss and destruction, and a read or import while unfocused (a store restoring a composition
//   after focus left) gets a one-off copy that the editor does not keep. The copy is keyed by that revision's identity, so a
//   stale copy is never mistaken for the current text. Like the shared buffers, the copy is wiped when released; rich-document
//   storage itself (piece-tree buffers) and transient slices read out of it are not.
using SharedText = std::shared_ptr<const std::wstring>;
// May throw std::bad_alloc, like the std::wstring it replaces; callers keep their existing allocation-failure handling.
[[nodiscard]] SharedText MakeSharedText(std::wstring text);
// One process-wide empty buffer, made on first use. It always has another owner, so MutableSharedText never writes to it.
[[nodiscard]] const SharedText& EmptySharedText() noexcept;
// Writable access to `text` for one edit after which the text is at most `requiredUnits` long: the buffer itself when `text`
// is its only owner and its capacity already holds requiredUnits, else a copy with room to grow that `text` then owns alone
// (the old buffer is released through its deleter). The reference is valid until `text` is next reassigned or shared, and an
// edit through it must not grow the text beyond requiredUnits.
[[nodiscard]] std::wstring& MutableSharedText(SharedText& text, size_t requiredUnits);
namespace Detail
{
// Test-only observation of the release wipe: called with the zeroed storage, its capacity in units and the size the text had
// when released, before the storage is freed. Null outside tests.
inline thread_local constinit void (*sharedTextReleaseObserver)(const wchar_t* storage, size_t capacityUnits, size_t sizeUnits) noexcept = nullptr;
} // namespace Detail
[[nodiscard]] inline std::wstring_view SharedTextView(const SharedText& text) noexcept
{
    return text ? std::wstring_view(*text) : std::wstring_view();
}
// Equal content; the same buffer compares without reading it.
[[nodiscard]] inline bool SharedTextEquals(const SharedText& text, std::wstring_view other) noexcept
{
    const std::wstring_view view = SharedTextView(text);
    return (view.data() == other.data() && view.size() == other.size()) || view == other;
}
[[nodiscard]] inline bool SharedTextEquals(const SharedText& left, const SharedText& right) noexcept
{
    return left == right || SharedTextEquals(left, SharedTextView(right));
}

// TextInputState with its document as shared text: what the native host cache and the TSF store pass between them.
//
// A RichTextEditor's state may instead carry its document as `source`, the immutable source revision it shows (W7.4,
// "Rich text-input snapshots" in Specs/UI/UI_InputAndAccessibility.md). Invariants relied on wherever such a state is read:
// - `source`, when set, is authoritative; `text`, when also set, has exactly its content. Code that makes new text for a
//   state (a TSF edit) resets `source` in the same step, so a stale revision never travels with newer text.
// - `text` may be null while `source` is set (not yet materialized). View() is then empty, so code that reads the document
//   contiguously materializes first (SharedTextInputAccess::Materialize, the store's ReadBackingState); comparisons and
//   lengths use SameTextInputContent, TextInputContentEquals and Length, which never copy.
// - A SourceSnapshot is immutable and pins its own piece tree, so a state, the host cache, undo history and the store's
//   observed state each keep their revision readable for as long as they hold it, across re-entrant callbacks, SetText,
//   mode switches, document reset and history expiry. Nothing ever writes to a revision.
struct SharedTextInputState
{
    SharedText text;
    std::optional<size_t> selectionAnchorIndex;
    size_t caretIndex       = 0u;
    size_t firstVisibleLine = 0u;
    bool readOnly           = false;
    bool masked             = false;
    bool multiline          = false;
    std::optional<RichText::SourceSnapshot> source;

    [[nodiscard]] std::wstring_view View() const noexcept
    {
        return SharedTextView(text);
    }
    [[nodiscard]] size_t Length() const noexcept
    {
        return text ? text->size() : source ? source->Length() : 0u;
    }
    [[nodiscard]] bool HasDocument() const noexcept
    {
        return text || source;
    }
};
// Equal documents. The same buffer or the same revision compares without reading; otherwise the text is compared in place
// (two revisions of one document only across their changed range). Never copies.
[[nodiscard]] bool SameTextInputContent(const SharedText& leftText,
                                        const std::optional<RichText::SourceSnapshot>& leftSource,
                                        const SharedText& rightText,
                                        const std::optional<RichText::SourceSnapshot>& rightSource) noexcept;
[[nodiscard]] inline bool SameTextInputContent(const SharedTextInputState& left, const SharedTextInputState& right) noexcept
{
    return SameTextInputContent(left.text, left.source, right.text, right.source);
}
[[nodiscard]] bool TextInputContentEquals(const SharedTextInputState& state, std::wstring_view text) noexcept;
// One contiguous copy of a revision, in one allocation at its exact size. May throw std::bad_alloc.
[[nodiscard]] SharedText MaterializeSharedText(const RichText::SourceSnapshot& source);
// Counts a whole-document copy of a rich editor's text made outside the document itself (a public text-input state).
inline void NoteRichDocumentCopy(size_t units) noexcept
{
    auto& work = Detail::Work().rich;
    work.documentBytesCopied += units * sizeof(wchar_t);
    if (units)
        ++work.documentFlattens;
}

// Moves a control's text-input state in and out without copying the document where the control keeps shared text and its
// default ExportTextInputState/ImportTextInputState (TextField, ComboBox, RichTextEditor: KeepsDefaultTextInputState).
// Every other control, including a derived class that overrides either virtual, goes through the virtuals with one copy, as
// every host path does.
struct SharedTextInputAccess
{
    // Whether Export and Import reach this control's document directly (no virtual call, no copy).
    [[nodiscard]] static bool Shares(const Control& control) noexcept;
    // May throw std::bad_alloc. A RichTextEditor exports its source revision and, when it already holds a contiguous copy of
    // that revision, the copy; otherwise `text` stays null until a reader materializes it.
    [[nodiscard]] static bool Export(const Control& control, SharedTextInputState& state);
    // Fills a null `text` from `source`: through the RichTextEditor's retained copy when `control` is one that still shows that
    // revision (an edit's changed range only, when nothing else holds the copy), else as one new copy. May throw std::bad_alloc.
    static void Materialize(const Control* control, SharedTextInputState& state);
    // Whether the live control shows exactly `text`. Compares in place; a control holding that buffer answers without reading.
    [[nodiscard]] static bool ShowsText(const Control& control, const SharedText& text) noexcept;
    // The control's own buffer, what its GetText reads (an editable ComboBox's), shared without a copy: a UI Automation record
    // borrows it (W8.3). Never null.
    [[nodiscard]] static const SharedText& Text(const TextField& field) noexcept;
    [[nodiscard]] static const SharedText& Text(const ComboBox& comboBox) noexcept;
    // Test observation: the RichTextEditor's retained text-input copy (null when it holds none) and the revision it copies.
    [[nodiscard]] static SharedText RetainedRichTextInputCopy(const Control& control, std::optional<RichText::SourceSnapshot>* source = nullptr) noexcept;
    // May throw what the control's import and its notifications throw.
    [[nodiscard]] static bool Import(Control& control, ControlHost& host, const SharedTextInputState& state, bool notifyChange);
    // A composition ended without restoring its preview (teardown, focus loss): an editable ComboBox takes the text it shows
    // as committed again, re-filtering its popup and selection. No other control keeps preview state; no callbacks run.
    static void SettlePreview(Control& control, ControlHost& host) noexcept;
};

[[nodiscard]] ITextStoreACP* CreateNativeTextInputTextStore(ControlHost& host, Control& control, bool hostDeferredWorkEligible = false) noexcept;
void DetachNativeTextInputTextStore(IUnknown* store) noexcept;
void DisconnectNativeTextInputTextStore(IUnknown* textStore) noexcept;
// Per native HWND, preserves the borrowed application IMM association while a masked view owns logical focus.
void SetNativeWindowImeSuppressed(ControlHost* host, bool suppressed) noexcept;
[[nodiscard]] bool IsRenderStageActiveForDebug() noexcept;
[[nodiscard]] bool IsRenderStageActive() noexcept;
void EmitRenderMutationBlockedForDebug() noexcept;
[[nodiscard]] bool CaptureBackdropScreenRegion(const RECT& screenRect, WindowHostBitmapCapture& outCapture, std::wstring_view componentName) noexcept;

inline constexpr float kMenuItemHeightDip                  = MenuBar::kDefaultHeightDip;
inline constexpr float kMenuCompactItemHeightDip           = 24.0f;
inline constexpr float kMenuHeaderHeightDip                = 24.0f;
inline constexpr float kMenuCompactHeaderHeightDip         = 20.0f;
inline constexpr float kMenuBarHeightDip                   = kMenuItemHeightDip;
inline constexpr float kMenuBarCompactHeightDip            = kMenuCompactItemHeightDip;
inline constexpr float kMenuBarInsetDip                    = 2.0f;
inline constexpr float kMenuBarCompactInsetDip             = 0.0f;
inline constexpr float kMenuBarItemPaddingXDip             = 10.0f;
inline constexpr float kMenuBarCompactItemPaddingXDip      = 6.0f;
inline constexpr float kMenuBarItemGapDip                  = 2.0f;
inline constexpr float kMenuBarCompactItemGapDip           = 0.0f;
inline constexpr float kMenuBarItemMeasureHeightDip        = 24.0f;
inline constexpr float kMenuBarCompactItemMeasureHeightDip = 24.0f;
inline constexpr float kMinimumInteractiveTextRowHeightDip = 20.0f;
// Menu flyouts and ComboBox dropdowns emulate the Windows 11 small-corner popup silhouette
// in-app. This is a shared visual token, not a literal DWM window-corner preference.
inline constexpr float kPopupRoundSmallCornerRadiusDip = 4.0f;
inline constexpr float kOverlayMicaBackdropBlurDip     = 28.0f;
inline constexpr float kOverlayMicaAltBackdropBlurDip  = 34.0f;
inline constexpr float kOverlayAcrylicBackdropBlurDip  = 40.0f;

[[nodiscard]] inline float ResolveMenuItemHeightDip(const ThemePalette& theme) noexcept
{
    return theme.density == Density::Compact ? kMenuCompactItemHeightDip : kMenuItemHeightDip;
}

[[nodiscard]] inline float ResolveMenuHeaderHeightDip(const ThemePalette& theme) noexcept
{
    return theme.density == Density::Compact ? kMenuCompactHeaderHeightDip : kMenuHeaderHeightDip;
}

[[nodiscard]] inline float ResolveMenuBarHeightDip(const ThemePalette& theme) noexcept
{
    return theme.density == Density::Compact ? kMenuBarCompactHeightDip : kMenuBarHeightDip;
}

[[nodiscard]] inline float ResolveMenuBarInsetDip(const ThemePalette& theme) noexcept
{
    return theme.density == Density::Compact ? kMenuBarCompactInsetDip : kMenuBarInsetDip;
}

[[nodiscard]] inline float ResolveMenuBarItemPaddingXDip(const ThemePalette& theme) noexcept
{
    return theme.density == Density::Compact ? kMenuBarCompactItemPaddingXDip : kMenuBarItemPaddingXDip;
}

[[nodiscard]] inline float ResolveMenuBarItemGapDip(const ThemePalette& theme) noexcept
{
    return theme.density == Density::Compact ? kMenuBarCompactItemGapDip : kMenuBarItemGapDip;
}

[[nodiscard]] inline float ResolveMenuBarItemMeasureHeightDip(const ThemePalette& theme) noexcept
{
    return theme.density == Density::Compact ? kMenuBarCompactItemMeasureHeightDip : kMenuBarItemMeasureHeightDip;
}

[[nodiscard]] inline FontRole ResolveMenuBarFontRole(const ThemePalette& theme) noexcept
{
    return theme.density == Density::Compact ? FontRole::Small : FontRole::Body;
}

[[nodiscard]] inline float ResolveOverlayBackdropOpacity(const ThemePalette& theme) noexcept
{
    switch (theme.overlayMaterial)
    {
        case OverlayMaterial::Mica: return theme.dark ? 0.76f : 0.68f;
        case OverlayMaterial::MicaAlt: return theme.dark ? 0.86f : 0.78f;
        case OverlayMaterial::Acrylic: return theme.dark ? 0.98f : 0.94f;
        case OverlayMaterial::Solid:
        default: return 0.0f;
    }
}

[[nodiscard]] inline float ResolveOverlayBackdropBlurDip(const ThemePalette& theme) noexcept
{
    switch (theme.overlayMaterial)
    {
        case OverlayMaterial::Mica: return kOverlayMicaBackdropBlurDip;
        case OverlayMaterial::MicaAlt: return kOverlayMicaAltBackdropBlurDip;
        case OverlayMaterial::Acrylic: return kOverlayAcrylicBackdropBlurDip;
        case OverlayMaterial::Solid:
        default: return 0.0f;
    }
}

void DrawRoundedRect(ControlHost& host, const D2D1_RECT_F& rect, const D2D1_COLOR_F& fill, const D2D1_COLOR_F& stroke, float radiusDip = 4.0f);

// WinUI double-stroke focus ring: outer stroke (2 DIP) + inner stroke (1 DIP) outside control bounds.
void PaintFocusRing(ControlHost& host, const D2D1_RECT_F& controlBounds, float controlCornerRadiusDip) noexcept;

// Popup/menu drop shadow. Uses the D2D shadow effect when available and falls back
// to a coarse rounded-rect approximation only if effect creation fails.
void DrawDropShadow(ControlHost& host,
                    const D2D1_RECT_F& targetRect,
                    float cornerRadiusDip,
                    float yOffsetDip   = 4.0f,
                    float spreadDip    = 4.0f,
                    float outerOpacity = 0.24f,
                    float innerOpacity = 0.12f) noexcept;

inline constexpr auto kTextDrawOptions = static_cast<D2D1_DRAW_TEXT_OPTIONS>(D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);

// W2.8 (Specs/Core/Core_PerformanceAndResources.md, "Format and brush caches"). While an active scope lives, a color the
// host's keyed brush map does not hold is drawn through the host's transition brushes instead of becoming a keyed entry:
// each such request of a frame takes a brush no earlier request of that frame took and sets its color, and the next
// frame reuses the same brushes. Library paint code opens a scope only for a color transition in progress (a Tree row's
// fade, a Button-family hover or focus blend) and never keeps such a brush beyond the call that drew with it.
class TransitionBrushScope final
{
public:
    TransitionBrushScope(const ControlHost& host, bool active) noexcept : _host(active ? &host : nullptr)
    {
        if (_host)
            ++_host->_transitionBrushScopes;
    }
    ~TransitionBrushScope()
    {
        if (_host)
            --_host->_transitionBrushScopes;
    }
    TransitionBrushScope(const TransitionBrushScope&)            = delete;
    TransitionBrushScope& operator=(const TransitionBrushScope&) = delete;

    // A brush set to `color` that no keyed entry owns, for code that sets its own colors before each draw
    // (DrawButtonChrome): a keyed brush must keep its color.
    [[nodiscard]] static ID2D1SolidColorBrush* ScratchBrush(const ControlHost& host, const D2D1_COLOR_F& color)
    {
        return host.GetTransitionBrush(color, true);
    }

private:
    const ControlHost* _host = nullptr;
};

// Whether a hover or focus progress is between its two states: the colors blended from it are a transition's.
[[nodiscard]] constexpr bool IsColorTransitionProgress(float progress) noexcept
{
    return progress > 0.0f && progress < 1.0f;
}

// Draws `text` in `format` into `rect` as DrawText would, with the host's retained layout of it when the host serves one
// (TextLayoutCacheAccess): the layout drawn at the box's origin with the same options. An empty string draws nothing.
void DrawHostText(ControlHost& host,
                  ID2D1RenderTarget* target,
                  std::wstring_view text,
                  IDWriteTextFormat* format,
                  const D2D1_RECT_F& rect,
                  ID2D1Brush* brush,
                  D2D1_DRAW_TEXT_OPTIONS options) noexcept;

void DrawCenteredText(ControlHost& host,
                      std::wstring_view text,
                      const D2D1_RECT_F& rect,
                      FontRole fontRole,
                      const D2D1_COLOR_F& color,
                      DWRITE_TEXT_ALIGNMENT alignment               = DWRITE_TEXT_ALIGNMENT_CENTER,
                      DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                      bool wrap                                     = false,
                      FlowDirection flowDirection                   = FlowDirection::LeftToRight,
                      IDWriteTextFormat* configuredFormat           = nullptr);
#if DXUI_ENABLE_DIAGNOSTICS
extern thread_local uint64_t testMnemonicLayoutCreations;
#endif
void DrawTextWithMnemonic(ControlHost& host,
                          std::wstring_view text,
                          const D2D1_RECT_F& rect,
                          FontRole fontRole,
                          const D2D1_COLOR_F& color,
                          wchar_t mnemonic,
                          DWRITE_TEXT_ALIGNMENT alignment               = DWRITE_TEXT_ALIGNMENT_CENTER,
                          DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                          bool wrap                                     = false,
                          FlowDirection flowDirection                   = FlowDirection::LeftToRight);

[[nodiscard]] ButtonVisualStyle ResolveButtonVisualStyle(
    const ThemePalette& theme, bool enabled, bool hovered, bool pressed, bool focused, bool keyboardFocused, bool primary, bool toggled) noexcept;
[[nodiscard]] ButtonVisualStyle ResolveButtonVisualStyle(const ThemePalette& theme,
                                                         bool enabled,
                                                         bool hovered,
                                                         bool pressed,
                                                         bool focused,
                                                         bool keyboardFocused,
                                                         bool primary,
                                                         float hoverStrength,
                                                         float focusStrength,
                                                         bool toggled) noexcept;
[[nodiscard]] ToggleVisualStyle ResolveToggleVisualStyle(
    const ThemePalette& theme, bool enabled, bool hovered, bool pressed, bool focused, bool keyboardFocused, bool checked) noexcept;
[[nodiscard]] ToggleVisualStyle ResolveToggleVisualStyle(const ThemePalette& theme,
                                                         bool enabled,
                                                         bool hovered,
                                                         bool pressed,
                                                         bool focused,
                                                         bool keyboardFocused,
                                                         bool checked,
                                                         float hoverStrength,
                                                         float focusStrength) noexcept;
[[nodiscard]] CheckboxVisualStyle ResolveCheckboxVisualStyle(
    const ThemePalette& theme, bool enabled, bool hovered, bool pressed, bool focused, bool keyboardFocused, bool checked) noexcept;
[[nodiscard]] CheckboxVisualStyle ResolveCheckboxVisualStyle(const ThemePalette& theme,
                                                             bool enabled,
                                                             bool hovered,
                                                             bool pressed,
                                                             bool focused,
                                                             bool keyboardFocused,
                                                             bool checked,
                                                             float hoverStrength,
                                                             float focusStrength) noexcept;
[[nodiscard]] ComboBoxVisualStyle ResolveComboBoxVisualStyle(
    const ThemePalette& theme, ComboBoxVariant variant, bool enabled, bool hovered, bool popupOpen, bool focused, bool keyboardFocused) noexcept;

// Resolve `target` by walking the live tree under `root`; `target` is never dereferenced unless it is found there,
// so a removed (possibly destroyed) control is a safe argument. Interactive also requires every ancestor to be
// visible and enabled.
[[nodiscard]] bool IsControlInTree(const Control* root, const Control* target) noexcept;
// Nesting depth of ControlHost::PreparePendingPageLayout on this thread: the work counter preparationMembershipSearches
// counts the IsControlInTree searches that start while it is nonzero.
inline thread_local constinit uint32_t g_hostPreparationDepth = 0u;
// Every place the library moves the pointer hit revision (UI_InputAndAccessibility.md, "Pointer hit result"), named so that a
// test can suppress exactly one and show that a scenario then hits a stale control (mutation coverage of the W2.9
// differential). `Key` suppresses the hit-revision comparison of the cached result instead.
enum class HitRevisionSite : uint8_t
{
    None,
    Assign, // Control::Assign and RouteAffects with Affects::Hit
    ComboBoxRebuild,
    ComboBoxOpen,
    ComboBoxClose,
    ComboBoxLayout,
    ScrollOffset,
    PageTransitionStart,
    PageTransitionEnd,
    PageIndicatorCount,
    SliderOrientation,
    SplitterGeometry,
    SplitterPosition,
    OverlayDismissal,
    HostSize,
    HostDpi,
    HostTheme,
    DeviceLoss,
    RootReset,
    SurfaceLoss,
    Key,
};
inline thread_local constinit HitRevisionSite g_suppressedHitRevisionSiteForTest = HitRevisionSite::None;
[[nodiscard]] inline bool HitRevisionSiteLive(HitRevisionSite site) noexcept
{
    return site != g_suppressedHitRevisionSiteForTest;
}
// A library control's hit-testing state changed (Control::InvalidateHitTest), at a named site.

inline void NoteHitTestChange(Control& control, HitRevisionSite site) noexcept

{
    if (! HitRevisionSiteLive(site))
        return;
    // A scroll offset moves content under its viewport and nothing in it (W8.3): UI Automation moves that scroll scope alone.
    if (site == HitRevisionSite::ScrollOffset)
        ControlModelQueryAccess::InvalidateHitTestForScroll(control);
    else
        control.InvalidateHitTest();
} // Control::Assign's equality (AssignEquality::Same) for the public value types the library's setters store that have no
// operator==: every member, so that an unchanged value routes nothing and a changed one is stored. One inline definition
// here, found by argument-dependent lookup wherever the library instantiates Same for these types.
[[nodiscard]] inline bool SameAssignedValue(const ThroughputGraphHueWeight& a, const ThroughputGraphHueWeight& b) noexcept
{
    return AssignEquality::Same(a.hueDegrees, b.hueDegrees) && AssignEquality::Same(a.weight, b.weight) && a.colorSlot == b.colorSlot;
}
[[nodiscard]] inline bool SameAssignedValue(const ThroughputGraphSample& a, const ThroughputGraphSample& b) noexcept
{
    return AssignEquality::Same(a.value, b.value) && AssignEquality::Same(a.hueDegrees, b.hueDegrees) && a.colorSlot == b.colorSlot &&
           a.hueWeightCount == b.hueWeightCount && AssignEquality::Same(a.hueWeights, b.hueWeights);
}
[[nodiscard]] inline bool SameAssignedValue(const MenuBarItem& a, const MenuBarItem& b) noexcept
{
    return a.text == b.text && a.mnemonic == b.mnemonic && a.enabled == b.enabled && a.rightJustified == b.rightJustified && a.sourceIndex == b.sourceIndex;
}
[[nodiscard]] inline bool SameAssignedValue(const StatusStrip::Section& a, const StatusStrip::Section& b) noexcept
{
    return a.text == b.text && AssignEquality::Same(a.widthDip, b.widthDip) && a.alignment == b.alignment && a.leadingEllipsis == b.leadingEllipsis;
}
[[nodiscard]] inline bool SameAssignedValue(const ComboBox::Item& a, const ComboBox::Item& b) noexcept
{
    return a.value == b.value && a.display == b.display;
}
[[nodiscard]] inline bool SameAssignedValue(const GridSortSpec& a, const GridSortSpec& b) noexcept
{
    return a.columnIndex == b.columnIndex && a.direction == b.direction;
}
[[nodiscard]] inline bool SameAssignedValue(const FlexChildOptions& a, const FlexChildOptions& b) noexcept
{
    return AssignEquality::Same(a.constraints.minimumDip, b.constraints.minimumDip) &&
           AssignEquality::Same(a.constraints.maximumWidthDip, b.constraints.maximumWidthDip) &&
           AssignEquality::Same(a.constraints.maximumHeightDip, b.constraints.maximumHeightDip) && AssignEquality::Same(a.explicitSizeDip, b.explicitSizeDip) &&
           AssignEquality::Same(a.grow, b.grow) && a.alignment == b.alignment;
}
[[nodiscard]] inline bool SameAssignedValue(const HsvColor& a, const HsvColor& b) noexcept
{
    return AssignEquality::Same(a.hue, b.hue) && AssignEquality::Same(a.saturation, b.saturation) && AssignEquality::Same(a.value, b.value);
} // Control::Assign for a library setter whose hit-test change keeps its own named site, so that the hit revision's mutation
// coverage can still suppress that site alone: Affects::Hit moves the revision at site instead of HitRevisionSite::Assign.
struct AssignAccess
{
    template <typename Field, typename Value, typename OnStored>
    static bool Assign(Control& control, HitRevisionSite site, Field&& field, Value&& value, Control::Affects affects, OnStored&& onStored)
    {
        if (AssignEquality::Same(field, value))
            return false;
        AssignEquality::Detail::Store(field, std::forward<Value>(value));
        const std::weak_ptr<int> lifetime = control._lifetimeToken;
        std::forward<OnStored>(onStored)();
        if (! lifetime.expired())
            control.RouteAffectsAt(affects, static_cast<uint8_t>(site));
        return true;
    }
};
// Item icon text (Grid cells, Tree rows): private-use glyphs need the icon font; letters, digits and symbols keep the
// small UI font, which the icon font would draw as missing-glyph boxes.
[[nodiscard]] bool IconTextUsesIconFont(std::wstring_view iconText) noexcept;
[[nodiscard]] FontRole ResolveIconTextFontRole(std::wstring_view iconText) noexcept;
[[nodiscard]] bool IsControlEffectivelyInteractive(const Control* root, const Control* target) noexcept;
// After a press handler returned: whether its gesture still owns the pointer. False when the handler's own nested work
// already ended a secondary press (a blocking menu loop consumed the release) or gave the pointer to another window (a
// popup that took capture): capturing then would strand a gesture with no button down, or close the popup. Window-less
// and embedded hosts, whose input the application delivers, keep capturing.
[[nodiscard]] bool PressStillOwnsPointer(const ControlHost& host, bool rightButton) noexcept;

inline constexpr PCWSTR kNativeAccessibilityTargetProperty = L"DxUi.ControlHost";
// Exact attached HWND lookup for library-owned modal loops; never routes child/foreign window input.
[[nodiscard]] bool DispatchAttachedWindowHostKeyboardMessage(const MSG& message) noexcept;
void RegisterWindowHostAccessibilityTarget(HWND hwnd, ControlHost* host) noexcept;
void UnregisterWindowHostAccessibilityTarget(HWND hwnd, ControlHost* host) noexcept;
void NotifyWindowHostAccessibilityDestroyed(HWND hwnd) noexcept;
// Marks a window host's snapshot stale and posts one coalesced flush, which republishes it and raises what changed for
// clients: StructureChanged when semantic controls were added, removed or replaced, and the focus change when another
// element took focus inside a window that holds the foreground's keyboard focus. Without a client it only marks the
// snapshot; with one, a focus change republishes at once. While the window itself gains focus, the system's focus event
// reports it; a move later in the turn of the gain is left to that event only in a window whose fragment-root GetFocus
// no call has ever begun on, because the first such call reports the moved-to element (see ReporterOfFocusMove).
enum class AccessibilityMutationKind : unsigned
{
    General,
    // Only content changes that cannot alter focus, visibility, enabled state or logical structure.
    ContentOnly,
    // The caller marked the records it changed (NoteAccessibilityRecordChange): the next publication need not be whole (W8.2).
    // Marked is content (a name, a value, a geometry); MarkedFocus a focus move, which keeps the focus path below.
    Marked,
    MarkedFocus,
};
void RefreshWindowHostAccessibilitySnapshot(HWND hwnd, ControlHost* host, AccessibilityMutationKind mutation = AccessibilityMutationKind::General) noexcept;
// An event a host raised or queued for UI Automation, as a test compares two runs (W8.2, DebugSetAccessibilityEventRecorderForTest).
enum class AccessibilityRaisedEventKindForTest : uint8_t
{
    Property,
    Automation,
    Structure,
    Focus,
};
struct AccessibilityRaisedEventForTest final
{
    AccessibilityRaisedEventKindForTest kind = AccessibilityRaisedEventKindForTest::Property;
    long id                                  = 0;
    std::wstring sender;   // The sender's control path ("/0/2"), empty for the window's element.
    uint64_t identity = 0; // The sender's control identity, 0 for the window's element.
    std::wstring oldValue, newValue;
};
using AccessibilityEventRecorderForTest = void (*)(const AccessibilityRaisedEventForTest& event, void* context) noexcept;
// Incremental publication (W8.2; UI_InputAndAccessibility.md, "Incremental publication"). What a precise change asks of the
// next publication of its control's record: its name or value (the record alone, and a label's mnemonic target), its keyboard
// focus (the record and the focused element), or its geometry (the records of its subtree and every bounding rectangle). A
// scroll (W8.3) is a control's content moving under its viewport: for a ScrollPanel nothing in its content changed, so the
// publication moves that scroll scope and shares every record and rectangle; for a Grid or Tree it counts as geometry of that
// control. A scroll is published at once, never coalesced with geometry.
enum class AccessibilityRecordAspect : uint8_t
{
    Name     = 1u,
    Value    = 2u,
    Focus    = 4u,
    Geometry = 8u,
    Scroll   = 16u,
};
namespace Detail
{
// A host's marks since its last publication, kept on its owner thread while a snapshot is published (ControlHost owns them).
// The next publication shares every record no mark names when the host can show that nothing else changed: no whole-window
// mutation was raised, every move of the interaction and hit revisions was a marked one, and every invalidation came from a
// path that declared what it changed. Otherwise, or when the marks overflow, it builds the whole window.
struct AccessibilityRecordMark final
{
    const Control* control = nullptr; // Compared and resolved through the published tree, never dereferenced unchecked.
    std::weak_ptr<int> lifetime;
    uint8_t aspects = 0u;
};
struct AccessibilityRecordMarks final
{
    static constexpr size_t kCapacity = 32u;
    std::array<AccessibilityRecordMark, kCapacity> marks{};
    size_t count                 = 0u;
    bool overflow                = false;
    bool wholeOwed               = true;  // A whole-window mutation, or nothing published yet: the next publication builds.
    bool unattested              = false; // An invalidation that declared nothing since the last publication.
    bool attestNextInvalidate    = false; // One-shot: the next ControlHost::Invalidate is a declared one (AttestedInvalidation).
    bool flushRequested          = false; // Native: a flush was posted for these marks.
    bool dormant                 = false; // Native: already dirty while no client was interested; later marks need no refresh.
    uint64_t epoch               = 0u;    // Moves on every note; a build that saw it move publishes and owes a whole build.
    uint64_t baseGeneration      = 0u;    // The published snapshot these marks apply to.
    uint64_t interactionRevision = 0u;    // The host revisions the published snapshot accounts for, advanced by marked moves.
    uint64_t hitRevision         = 0u;
    // Coalescing of geometry-only publications (W8.2): at most one per kGeometryPublicationIntervalMs on the host's clock,
    // plus one when the layout settles: an embedded view's next update with no geometry mark since the deferral, a window
    // host's schedule wake kGeometrySettleMs after the last deferral (its flushes follow every frame of marks).
    static constexpr uint64_t kGeometryPublicationIntervalMs = 100u;
    static constexpr uint64_t kGeometrySettleMs              = 32u;
    uint64_t geometryMarks                                   = 0u;    // Moves on every geometry mark.
    uint64_t geometryMarksAtDeferral                         = 0u;    // geometryMarks when the last geometry-only publication was deferred.
    uint64_t geometryDeferredAtMs                            = 0u;    // The host's clock at that deferral.
    uint64_t lastGeometryPublicationMs                       = 0u;    // The host's clock at the last publication that carried a geometry mark.
    bool geometryPublished                                   = false; // lastGeometryPublicationMs is set.
    bool geometryDeferred                                    = false; // A geometry-only publication waits for the interval or the settle.
    bool geometryDue                                         = false; // The next publication carries the deferred geometry, whatever the interval.
};
} // namespace Detail
// Marks `control`'s record for the next publication of its host (owner thread). A whole-window publication covers any mark.
void NoteAccessibilityRecordChange(const Control& control, AccessibilityRecordAspect aspect) noexcept;
// The host's interaction or hit revision just moved from `before` for `control`'s geometry (SetBounds, a named hit site):
// marked as a geometry change, so the move is accounted for and does not force a whole build.
void NoteAccessibilityGeometryMove(const Control& control, uint64_t interactionBefore, uint64_t hitBefore) noexcept;
// As NoteAccessibilityGeometryMove, for a ScrollPanel whose scroll offset alone moved (AccessibilityRecordAspect::Scroll).
void NoteAccessibilityScrollMove(const Control& control, uint64_t interactionBefore, uint64_t hitBefore) noexcept;
struct AccessibilityRevisions final
{
    uint64_t interaction = 0u;
    uint64_t hit         = 0u;
};
[[nodiscard]] AccessibilityRevisions ReadAccessibilityRevisions(const ControlHost& host) noexcept;
// A change the next publication must build whole for (a mnemonic target, a structure the marks cannot follow).
void NoteAccessibilityWholeChange(const ControlHost& host) noexcept;
// The host's accessibility attachment ends or begins (a window detaches or attaches, an embedded view connects or
// disconnects): its marks are dropped, no flush rides on an old attachment, and the next publication builds whole.
void ResetAccessibilityRecordMarks(const ControlHost& host) noexcept;
// When `host`'s schedule must wake for a deferred geometry-only publication (W8.2 coalescing): a window host at its settle or
// interval, whichever is first; an embedded view at once, which brings the application back for its next update. None when
// nothing is deferred.
[[nodiscard]] std::optional<uint64_t> AccessibilityGeometryWakeDueTickMs(const ControlHost& host) noexcept;
// A window host that presents no frame, for any reason its frame schedule knows (no window, presentation turned off, hidden or
// minimized, hidden through an ancestor, no dispatcher): its schedule is held, so it never holds a geometry deferral. False for
// an embedded view, whose frames are the application's.
[[nodiscard]] bool HostPresentsNoFrame(const ControlHost& host) noexcept;
// A wake of a window host's schedule at `nowTickMs`: a deferred geometry-only publication whose layout settled (no deferral
// for kGeometrySettleMs) or whose interval passed becomes due, and the host posts its flush; with `now`, whatever the time
// (a wake that finds the host hidden, which then holds no wake for it). An embedded view decides at its next update.
void SettleAccessibilityGeometry(ControlHost& host, uint64_t nowTickMs, bool now = false) noexcept;
// The invalidation of a caret blink flip (W4.7), the same on both hosts: `inkDip`, when the editor knows the ink its last Paint drew
// for the caret (in its own coordinates), is all the paint region takes; null repaints the control's extent as Invalidate does.
void InvalidateCaretFlip(ControlHost& host, const Control& control, const D2D1_RECT_F* inkDip) noexcept;
// Exact library types that report every change of their own record through marks and declare their own invalidations: a
// handled input event on one of them needs no whole-window republish, and its invalidations are declared ones.
[[nodiscard]] bool ReportsOwnAccessibilityChanges(const Control& control) noexcept;
// While alive, the next ControlHost::Invalidate on `host` declares what it repaints (a marked change, a geometry move, a
// focus-visual or a self-reporting control's paint): it does not by itself make the next publication whole.
class AttestedInvalidation final
{
public:
    explicit AttestedInvalidation(const ControlHost* host, bool attest = true) noexcept;
    ~AttestedInvalidation();
    AttestedInvalidation(const AttestedInvalidation&)            = delete;
    AttestedInvalidation& operator=(const AttestedInvalidation&) = delete;
    AttestedInvalidation(AttestedInvalidation&&)                 = delete;
    AttestedInvalidation& operator=(AttestedInvalidation&&)      = delete;

    // The host owning `marks` is being destroyed (from code its invalidation ran): every live attestation of this thread
    // that refers to them forgets them, so none writes to them after the host is gone.
    static void ForgetMarks(const Detail::AccessibilityRecordMarks* marks) noexcept;

private:
    Detail::AccessibilityRecordMarks* _marks = nullptr;
    AttestedInvalidation* _outer             = nullptr; // The attestation alive on this thread before this one.
};
// A window host begins to gain focus, before it publishes anything: records how many fragment-root GetFocus calls had
// begun, which says whether UI Automation will ask for what the gain focuses or restores (none had) or answer without
// asking (see EndWindowHostFocusGainTurn), and starts the gain with no announcement made.
void BeginWindowHostFocusGain(HWND hwnd, ControlHost* host) noexcept;
// The turn of a window host's gain ended with the window still focused. UI Automation answers the gain of a window it has
// asked for its focus before from the root element's keyboard-focus property, which reports nothing unless the root stands
// for the focused control, so the host announces the focused element then, unless it announced a move of the turn itself.
void EndWindowHostFocusGainTurn(HWND hwnd, ControlHost* host) noexcept;
void PublishEmptyWindowHostAccessibilitySnapshot(HWND hwnd, ControlHost* host) noexcept;
// True for a native menu popup window. Explicit UIA focus of a row in such a popup tracks
// logical row focus only; the menu session keeps its own Win32 focus target.
[[nodiscard]] bool IsNativeMenuPopupWindow(HWND hwnd) noexcept;
// Closes current-thread async menus before the process-exit window-host sweep.
void DismissAsyncMenusForProcessExit() noexcept;
[[nodiscard]] LRESULT ReturnWindowHostAccessibilityProvider(HWND hwnd, WPARAM wp, LPARAM lp) noexcept;
[[nodiscard]] bool TryHandleWindowHostAccessibilityMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LRESULT& outResult) noexcept;
#if DXUI_ENABLE_DIAGNOSTICS
void DebugFailNextWindowHostAccessibilityRegistrationForTest() noexcept;
void DebugSetAccessibilityUiActionHandlerStallForTest(HANDLE enteredEvent, HANDLE releaseEvent) noexcept;
void DebugSetAccessibilityUiActionHandlerTakenStallForTest(HANDLE enteredEvent, HANDLE releaseEvent) noexcept;
void DebugSetAccessibilityUiActionPostedEventForTest(HANDLE postedEvent) noexcept;
void DebugSetAccessibilityUiActionDispatchTimeoutForTest(DWORD timeoutMs) noexcept;
void DebugResetAccessibilityUiActionExecutionCountForTest() noexcept;
[[nodiscard]] uint32_t DebugGetAccessibilityUiActionExecutionCountForTest() noexcept;
void DebugSetAccessibilityOffscreenSelectedRowMaterializationLimitForTest(size_t limit) noexcept;
// How many calls of `hwnd`'s fragment-root GetFocus have begun: each counts itself before it reads the snapshot. The
// largest uint64_t when the window has no registered host.
[[nodiscard]] uint64_t DebugGetAccessibilityFocusResolutionCountForTest(HWND hwnd) noexcept;
// Window-host and embedded accessibility targets alive in this process.
[[nodiscard]] size_t DebugGetLiveAccessibilityTargetCountForTest() noexcept;
[[nodiscard]] uint64_t DebugGetAccessibilitySnapshotBuildCountForTest() noexcept;
// Every publication of a snapshot, whole build or successor (W8.2): what "one publication" means for a host's flushes.
[[nodiscard]] uint64_t DebugGetAccessibilityPublicationCountForTest() noexcept;
// W8.2, process-wide: `recorder` receives every event a host raises or queues (null clears it), on the raising thread.
void DebugSetAccessibilityEventRecorderForTest(AccessibilityEventRecorderForTest recorder, void* context) noexcept;
// W8.2, process-wide. Whole: every publication builds the whole window, the reference an incremental publication is held to.
// Verification: after every incremental publication the owner also builds the whole window, unpublished and uncounted, and
// compares it field by field; the mismatches are counted and the first is described (on the publishing thread).
void DebugSetAccessibilityWholePublicationForTest(bool whole) noexcept;
// W8.3, process-wide: the record reads before W8.3 (a dynamic_cast per type test, a tree walk per label lookup, a copy of each
// text control's document), counted in the same work counters, so a test shows its gate failing on the old path; and the
// upward path search a successor made per control it resolved (accessibility.pathChildReads). Only those reads: everything else,
// the count pass and its label index among it, is the current code's.
void DebugSetAccessibilityLegacyRecordReadsForTest(bool legacy) noexcept;
// W8.3: the bytes of a record's hot core and of each part a record may hold (text, range, grid, tree, tab control).
struct AccessibilityRecordBytesForTest final
{
    size_t core = 0u, text = 0u, range = 0u, grid = 0u, tree = 0u, tabs = 0u;
};
[[nodiscard]] AccessibilityRecordBytesForTest DebugGetAccessibilityRecordBytesForTest() noexcept;
void DebugSetAccessibilityIncrementalVerificationForTest(bool verify) noexcept;
[[nodiscard]] bool DebugGetAccessibilityIncrementalVerificationForTest() noexcept;
// Since the program started, never reset: verified incremental publications and those that differed from a whole build.
void DebugGetAccessibilityIncrementalVerificationTotalsForTest(uint64_t& verified, uint64_t& mismatches) noexcept;
[[nodiscard]] uint64_t DebugGetAccessibilityIncrementalVerificationCountForTest() noexcept;
[[nodiscard]] uint64_t DebugGetAccessibilityIncrementalMismatchCountForTest() noexcept;
[[nodiscard]] std::wstring DebugGetAccessibilityFirstIncrementalMismatchForTest();
// The first difference on the calling thread since the program started, never reset.
[[nodiscard]] std::wstring DebugGetAccessibilityFirstProgramMismatchForTest();
void DebugResetAccessibilityIncrementalVerificationForTest() noexcept;
// W8.2: the snapshot `host` publishes now, held as an opaque owner (null when none). Any thread may describe a held snapshot,
// one line per record (name, value, range value, focus), and count the records two held snapshots share.
[[nodiscard]] std::shared_ptr<const void> DebugHoldAccessibilitySnapshotForTest(ControlHost& host) noexcept;
void DebugDescribeAccessibilitySnapshotForTest(const std::shared_ptr<const void>& held, std::vector<std::wstring>& lines);
// W8.3: a record without parts, and a read of each of its five parts as a UI Automation read makes one (Part(), noexcept),
// returning how many items they hold. Reading a part a record does not have allocates nothing, from the program's start.
// W8.3: a whole build of an embedded view's tree that is never published, held, and the bytes AccessibilitySnapshotHeapBytes
// reports for it (what RetainedAccessibilitySnapshotBytes and accessibility.snapshotBytes count for a generation).
[[nodiscard]] std::shared_ptr<const void> DebugBuildUnpublishedAccessibilitySnapshotForTest(ControlHost& host, uint64_t& reportedBytes);
[[nodiscard]] std::shared_ptr<const void> DebugMakeEmptyAccessibilityRecordForTest();
[[nodiscard]] size_t DebugReadAccessibilityRecordPartsForTest(const std::shared_ptr<const void>& record) noexcept;
[[nodiscard]] size_t DebugCountSharedAccessibilityRecordsForTest(const std::shared_ptr<const void>& first, const std::shared_ptr<const void>& second) noexcept;
// Owner-thread-only deterministic failure at the rich snapshot font query.
void DebugSetRichAccessibilityFontFailureForTest(HRESULT failure) noexcept;
[[nodiscard]] size_t DebugGetRichAccessibilityFontFailureCountForTest() noexcept;
// Owner-thread-only mutation-interest override for this exact HWND attachment. Nullopt restores automatic interest;
// null HWND with nullopt clears the calling thread's override. Other hosts, queries and UIA listeners are unchanged.
[[nodiscard]] bool DebugSetAccessibilityMutationInterestForTest(HWND hwnd, std::optional<bool> interested) noexcept;
// Process-wide: what the build gate (SystemClientsAreListening) takes UiaClientsAreListening to answer, since another process
// on a developer's desktop may listen; nullopt asks it again. A provider request still latches a host, and raising events
// still asks UiaClientsAreListening itself.
void DebugSetAccessibilitySystemListenersForTest(std::optional<bool> listening) noexcept;
[[nodiscard]] std::optional<bool> DebugGetAccessibilitySystemListenersForTest() noexcept;
namespace Detail
{
// Called on the calling thread each time a text range resolves its text, with the owner that pins the characters and the
// view the call is about to read (PinnedRangeText in DxUi.Accessibility.cpp). Lets a lifetime test replace and drop the
// published snapshot after resolution and before the read. Null outside tests; compiled into every configuration, where it
// costs one thread-local read per resolution. It is called while the range holds its target's snapshotMutex, so an observer
// must never wait for another thread that takes that mutex (any text-range call on the same window).
inline thread_local constinit void (*textRangeResolvedObserver)(const std::shared_ptr<const void>& owner, std::wstring_view text) noexcept = nullptr;
// Called with true before and false after the whole build that verifies an incremental publication (W8.2,
// DebugSetAccessibilityIncrementalVerificationForTest), so a test program can leave that build out of its allocation counts.
// Process-wide and called on the publishing thread; a program sets it once, before any test runs.
inline constinit void (*verificationScopeObserver)(bool entering) noexcept = nullptr;
} // namespace Detail
// Holds every call of `hwnd`'s fragment-root GetFocus before it counts itself or reads the snapshot, so a test decides
// when the system's focus event is answered: each call sets `enteredEvent` on arrival and waits, for at most five seconds
// and never indefinitely, for `releaseEvent`. Null events clear the gate and return once the calls it held have left it.
void DebugSetAccessibilityFocusResolutionGateForTest(HWND hwnd, HANDLE enteredEvent, HANDLE releaseEvent, bool afterSnapshot = false) noexcept;
void DebugSetAccessibilityPeerCreationGateForTest(HWND hwnd, HANDLE enteredEvent, HANDLE releaseEvent) noexcept;
// Holds one embedded property batch after extraction, outside the slot mutex and before UIA delivery, for at most
// twenty seconds. Clearing waits for its holder to leave before the test closes the events.
// afterScope holds the actual worker after its target/failure admission releases, before returning to Windows.
// In that mode enteredEvent is reset on exit, so it proves the worker remains parked when the consumer checks it.
void DebugSetAccessibilityPropertyDeliveryGateForTest(ControlHost* host, HANDLE enteredEvent, HANDLE releaseEvent, bool afterScope = false) noexcept;
[[nodiscard]] size_t DebugCountPendingTreeItemPropertiesForTest(ControlHost& host, int property) noexcept;
// Process-wide, set before attaching and cleared after completing: queued UI Automation notifications (property changes,
// structure invalidations) are queued on the owner exactly as before but not handed to the thread-pool delivery work item,
// so nothing is raised to UI Automation and the owner's queue state never depends on when that work item runs.
void DebugSetAccessibilityNotificationDeliveryManualForTest(bool manual) noexcept;
// Owner thread: empties an embedded view's queue as one delivery would (registrations released, capacity kept), without
// raising anything. Called at a fixed point, it gives the owner the queue state of a delivery that always finished first.
void DebugCompletePendingAccessibilityNotificationsForTest(ControlHost& host) noexcept;
// One owner-thread callback after the selected value capture (1/2) or sender registration (3), before queue admission.
// Simulates COM reentry; consumed before invocation so a nested publication cannot invoke it again.
using AccessibilityPropertyRegistrationHookForTest = void (*)(void* context) noexcept;
void DebugSetAccessibilityPropertyRegistrationHookForTest(ControlHost* host,
                                                          unsigned stage,
                                                          AccessibilityPropertyRegistrationHookForTest hook,
                                                          void* context) noexcept;
// Runs `hook` with `context` on the raising thread after each UI Automation selection event a host raises (an item's IsSelected
// change or selection event, or the invalidation of a tree's or grid's selection), since something else can run while they are
// raised (an outgoing call of UI Automation's in a single-threaded apartment dispatches messages): a test hides, removes or
// disconnects the control from it. A null hook clears it.
using AccessibilitySelectionEventHookForTest = void (*)(void* context) noexcept;
void DebugSetAccessibilitySelectionEventHookForTest(AccessibilitySelectionEventHookForTest hook, void* context) noexcept;
// Runs after a property notification, with its PROPERTYID. A test can retire the source during synchronous publication.
using AccessibilityPropertyEventHookForTest = void (*)(long propertyId, void* context) noexcept;
void DebugSetAccessibilityPropertyEventHookForTest(AccessibilityPropertyEventHookForTest hook, void* context) noexcept;
// Counting starts at zero when enabled and covers every record, table slot and tree node a provider call or event
// examines to resolve a control on the calling thread; it is off (and free) otherwise.
void DebugSetAccessibilityResolutionCountingForTest(bool enabled) noexcept;
void DebugResetAccessibilityResolutionVisitCountForTest() noexcept;
[[nodiscard]] uint64_t DebugGetAccessibilityResolutionVisitCountForTest() noexcept;
// The path a window host's UI Automation events use for `control`: its child indices from the root in indices, and
// their number in depth. False when the control has no element in the window. Never dereferences a control that is
// not in the tree.
[[nodiscard]] bool DebugResolveWindowHostEventPathForTest(HWND hwnd, const Control* control, std::span<uint16_t> indices, uint32_t& depth) noexcept;
// How many lookups in the published snapshot's tables disagree with a scan of its records: zero once the snapshot
// matches the tree (after RefreshAccessibilitySnapshot). The largest size_t when the window has no snapshot.
[[nodiscard]] size_t DebugCountAccessibilityIndexMismatchesForTest(HWND hwnd) noexcept;
#endif

// Scrollbar shared helpers (shared by Grid and Tree)
constexpr float kScrollbarThicknessDip         = 12.0f;
constexpr float kScrollbarMinThumbDip          = 20.0f;
constexpr float kScrollbarThumbCornerRadiusDip = 4.0f;
constexpr float kScrollbarThumbInsetDip        = 2.0f;

enum class ScrollbarOrientation : uint8_t
{
    Vertical,
    Horizontal
};

struct ResolvedScrollbarVisuals
{
    D2D1_COLOR_F track = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
    D2D1_COLOR_F thumb = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
};

struct ScrollbarAnimationTargets
{
    float track = 0.0f;
    float thumb = 0.0f;
};

[[nodiscard]] ResolvedScrollbarVisuals ResolveScrollbarVisuals(const ThemePalette& theme, bool trackHovered, bool thumbHovered, bool thumbDragging) noexcept;
[[nodiscard]] ResolvedScrollbarVisuals ResolveScrollbarVisuals(const ThemePalette& theme,
                                                               ScrollbarAnimationTargets targets,
                                                               float trackHotStrength,
                                                               float thumbHotStrength) noexcept;
[[nodiscard]] ScrollbarAnimationTargets ResolveScrollbarAnimationTargets(bool trackHovered, bool thumbHovered, bool thumbDragging) noexcept;
void UpdateScrollbarAnimation(
    ControlHost& host, const Control& owner, ScrollbarAnimationState& animation, bool trackHovered, bool thumbHovered, bool thumbDragging) noexcept;
[[nodiscard]] bool AdvanceScrollbarAnimation(const ControlHost& host, ScrollbarAnimationState& animation, uint64_t nowTickMs) noexcept;

[[nodiscard]] float ComputeScrollbarPageStepDip(const D2D1_RECT_F& trackRect,
                                                ScrollbarOrientation orientation,
                                                float viewportDip,
                                                float totalContentDip) noexcept;
[[nodiscard]] D2D1_RECT_F ComputeScrollbarThumbRect(const D2D1_RECT_F& trackRect,
                                                    ScrollbarOrientation orientation,
                                                    float viewportDip,
                                                    float totalContentDip,
                                                    float scrollOffsetDip,
                                                    float scrollExtentDip) noexcept;
[[nodiscard]] D2D1_RECT_F ComputeScrollbarThumbHitRect(const D2D1_RECT_F& trackRect,
                                                       ScrollbarOrientation orientation,
                                                       float viewportDip,
                                                       float totalContentDip,
                                                       float scrollOffsetDip,
                                                       float scrollExtentDip) noexcept;

void PaintScrollbar(ControlHost& host, const D2D1_RECT_F& trackRect, const D2D1_RECT_F& thumbRect, const ResolvedScrollbarVisuals& visuals) noexcept;

// Lines one wheel notch scrolls in Grid, Tree and TextField: the user's SPI_GETWHEELSCROLLLINES setting (3 when it cannot
// be read), read at first use, again whenever a window host receives WM_SETTINGCHANGE and before each wheel event an
// embedded host dispatches (it receives no window messages). WHEEL_PAGESCROLL scrolls
// `pageLineCount` lines (at least one); a setting of 0 scrolls nothing.
[[nodiscard]] size_t ResolveWheelScrollLineCount(size_t pageLineCount) noexcept;
void RefreshWheelScrollLines() noexcept;

// The user's font smoothing: SPI_GETFONTSMOOTHING off is Off, otherwise SPI_GETFONTSMOOTHINGTYPE selects Standard or
// ClearType (ClearType when it cannot be read). Read at first use and again whenever a window host receives
// WM_SETTINGCHANGE; RefreshFontSmoothing returns whether the value changed. Theme and typography contract, "Text
// antialiasing and rendering parameters".
enum class FontSmoothing : uint8_t
{
    Off,
    Standard,
    ClearType,
};
[[nodiscard]] FontSmoothing ResolveFontSmoothing() noexcept;
bool RefreshFontSmoothing() noexcept;
// Whether Direct2D draws text with these rendering parameters under this antialias mode. Measured on WARP: an ALIASED
// rendering mode fails (E_INVALIDARG at EndDraw) under CLEARTYPE and GRAYSCALE, OUTLINE fails under CLEARTYPE, and the
// ALIASED antialias mode accepts only DEFAULT, ALIASED and OUTLINE. DEFAULT accepts every rendering mode.
[[nodiscard]] bool TextAntialiasModeAcceptsRenderingMode(D2D1_TEXT_ANTIALIAS_MODE antialias, DWRITE_RENDERING_MODE renderingMode) noexcept;
// Parameters that keep the source's gamma, contrast, ClearType level and pixel geometry and that `antialias` accepts:
// the source itself when it is accepted, otherwise a copy with DWRITE_RENDERING_MODE_DEFAULT.
[[nodiscard]] HRESULT MakeTextRenderingParamsFor(IDWriteFactory* factory,
                                                 IDWriteRenderingParams* source,
                                                 D2D1_TEXT_ANTIALIAS_MODE antialias,
                                                 IDWriteRenderingParams** result) noexcept;

// Typeahead / case-insensitive search helpers (shared by Tree and ComboBox)
constexpr uint64_t kTypeaheadResetMs = 1000u;

[[nodiscard]] bool StartsWithInsensitive(std::wstring_view text, std::wstring_view prefix) noexcept;

// Single-line text editing helpers (shared by TextField and ComboBox)
[[nodiscard]] size_t StepToPreviousTextElement(std::wstring_view text, size_t caretIndex) noexcept;
[[nodiscard]] size_t StepToNextTextElement(std::wstring_view text, size_t caretIndex) noexcept;
[[nodiscard]] size_t CountTextElements(std::wstring_view text) noexcept;
[[nodiscard]] size_t SnapCaretIndexToTextElementBoundary(std::wstring_view text, size_t caretIndex) noexcept;
[[nodiscard]] bool IsWordCharacter(wchar_t ch) noexcept;
// Complete-scalar admission for navigation/UIA and semantic prose. BMP keeps
// the legacy Windows classification; supplementary letters/digits use Unicode 17.
[[nodiscard]] bool IsWordCharacter(std::uint32_t scalar) noexcept;
[[nodiscard]] bool IsPathSeparator(wchar_t ch) noexcept;
[[nodiscard]] size_t FindPreviousWordBoundary(std::wstring_view text, size_t caretIndex) noexcept;
[[nodiscard]] size_t FindNextWordBoundary(std::wstring_view text, size_t caretIndex) noexcept;
[[nodiscard]] bool ShouldEmitSingleLineBiDiTextMetric(std::wstring_view text, DWRITE_READING_DIRECTION readingDirection) noexcept;

void ClearSingleLineTextLayoutCache(SingleLineTextLayoutCache& cache, bool secureText = false) noexcept;
#if DXUI_ENABLE_DIAGNOSTICS
extern thread_local uint64_t testSingleLineMeasureLayoutCreations;
// Test seam (W2.2): true makes every ComboBox popup placement measure each filtered item again and never reuse the last
// placement, as before the retained widths, so a test can compare both. Thread-local; a test restores false.
extern thread_local bool testComboBoxMeasurePopupItemsEveryLayout;
#endif

[[nodiscard]] float MeasureSingleLineTextWidthDip(const ControlHost* host,
                                                  std::wstring_view text,
                                                  FontRole role,
                                                  float heightDip                           = 24.0f,
                                                  DWRITE_READING_DIRECTION readingDirection = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT,
                                                  IDWriteTextFormat* configuredFormat       = nullptr) noexcept;
[[nodiscard]] wil::com_ptr<IDWriteTextLayout> CreateSingleLineTextLayout(
    const ControlHost* host,
    std::wstring_view text,
    FontRole role,
    float widthDip,
    float heightDip,
    DWRITE_READING_DIRECTION readingDirection = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT) noexcept;
[[nodiscard]] wil::com_ptr<IDWriteTextLayout> GetOrCreateSingleLineTextLayout(
    const ControlHost* host,
    SingleLineTextLayoutCache* cache,
    std::wstring_view text,
    FontRole role,
    float minimumWidthDip,
    float heightDip,
    DWRITE_READING_DIRECTION readingDirection = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT,
    bool secureCacheText                      = false) noexcept;
[[nodiscard]] float MeasureCaretOffsetDip(const ControlHost* host,
                                          std::wstring_view text,
                                          FontRole role,
                                          size_t caretIndex,
                                          float heightDip,
                                          DWRITE_READING_DIRECTION readingDirection = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT,
                                          float layoutWidthDip                      = 0.0f) noexcept;
[[nodiscard]] float MeasureCaretOffsetDip(IDWriteTextLayout* layout, std::wstring_view text, size_t caretIndex) noexcept;
[[nodiscard]] size_t HitTestCaretIndexDip(const ControlHost* host,
                                          std::wstring_view text,
                                          FontRole role,
                                          const D2D1_RECT_F& textRect,
                                          float scrollDip,
                                          D2D1_POINT_2F point,
                                          DWRITE_READING_DIRECTION readingDirection = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT) noexcept;

void DrawSingleLineTextClipped(ControlHost& host,
                               std::wstring_view text,
                               const D2D1_RECT_F& rect,
                               FontRole role,
                               const D2D1_COLOR_F& color,
                               float scrollDip,
                               DWRITE_READING_DIRECTION readingDirection = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT) noexcept;
void DrawSingleLineTextClippedWithLayout(ControlHost& host,
                                         std::wstring_view text,
                                         const D2D1_RECT_F& rect,
                                         FontRole role,
                                         const D2D1_COLOR_F& color,
                                         float scrollDip,
                                         DWRITE_READING_DIRECTION readingDirection,
                                         IDWriteTextLayout* layout) noexcept;

[[nodiscard]] std::optional<std::pair<size_t, size_t>> GetSingleLineSelectionRange(std::optional<size_t> anchorIndex, size_t caretIndex) noexcept;
void SetSingleLineCaretIndex(size_t& caretIndex, std::optional<size_t>& anchorIndex, size_t nextCaretIndex, bool extendSelection) noexcept;
[[nodiscard]] bool DeleteSingleLineSelection(std::wstring& text, size_t& caretIndex, std::optional<size_t>& anchorIndex) noexcept;
void SelectAllSingleLineText(size_t textLength, size_t& caretIndex, std::optional<size_t>& anchorIndex) noexcept;

void ResetSingleLineSelectionClickSequence(SingleLineSelectionClickSequence& sequence) noexcept;
void ArmSingleLineSelectionClickSequence(SingleLineSelectionClickSequence& sequence, D2D1_POINT_2F pointDip) noexcept;
[[nodiscard]] bool ShouldPromoteSingleLineClickToSelectAll(const ControlHost& host,
                                                           const SingleLineSelectionClickSequence& sequence,
                                                           D2D1_POINT_2F pointDip) noexcept;

[[nodiscard]] bool IsSelectionWhitespace(wchar_t value) noexcept;
[[nodiscard]] int GetWordSelectionClass(std::wstring_view text, size_t index) noexcept;
void SelectSingleLineWordAt(std::wstring_view text, size_t hitIndex, size_t& caretIndex, std::optional<size_t>& anchorIndex) noexcept;

[[nodiscard]] std::optional<D2D1_RECT_F> ComputeSingleLineSelectionPaintRect(
    const ControlHost& host,
    std::wstring_view text,
    const D2D1_RECT_F& rect,
    FontRole role,
    float scrollDip,
    std::optional<std::pair<size_t, size_t>> selectionRange,
    DWRITE_READING_DIRECTION readingDirection = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT) noexcept;
void DrawSingleLineSelection(ControlHost& host,
                             std::wstring_view text,
                             const D2D1_RECT_F& rect,
                             FontRole role,
                             const D2D1_COLOR_F& textColor,
                             const D2D1_COLOR_F& selectionFill,
                             const D2D1_COLOR_F& selectionText,
                             float scrollDip,
                             std::optional<std::pair<size_t, size_t>> selectionRange,
                             DWRITE_READING_DIRECTION readingDirection = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT) noexcept;
void DrawSingleLineSelectionWithLayout(ControlHost& host,
                                       std::wstring_view text,
                                       const D2D1_RECT_F& rect,
                                       FontRole role,
                                       const D2D1_COLOR_F& textColor,
                                       const D2D1_COLOR_F& selectionFill,
                                       const D2D1_COLOR_F& selectionText,
                                       float scrollDip,
                                       std::optional<std::pair<size_t, size_t>> selectionRange,
                                       DWRITE_READING_DIRECTION readingDirection,
                                       IDWriteTextLayout* layout) noexcept;

[[nodiscard]] std::optional<std::pair<size_t, size_t>> ResolveNativeTextInputOptionalRange(const NativeTextInputState& state,
                                                                                           std::optional<size_t> startIndex,
                                                                                           std::optional<size_t> endIndex) noexcept;
[[nodiscard]] std::vector<D2D1_RECT_F> BuildTextInputUnderlineRects(
    const ControlHost& host, const Control& control, std::pair<size_t, size_t> range, float bottomInsetDip, float thicknessDip);
[[nodiscard]] std::vector<D2D1_RECT_F> BuildNativeCompositionUnderlineRects(const ControlHost& host, const Control& control, bool conversionTarget);
void DrawTextInputUnderlineRects(ControlHost& host, const std::vector<D2D1_RECT_F>& underlineRects, const D2D1_COLOR_F& color) noexcept;
} // namespace DxUi
