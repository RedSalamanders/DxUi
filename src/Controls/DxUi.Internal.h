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

struct ControlModelQueryAccess
{
    [[nodiscard]] static std::wstring_view RadioGroupHeader(const RadioButtons& group) noexcept
    {
        return group._header;
    }
    template <typename Owner> [[nodiscard]] static auto InvalidateGeometry(Owner& owner) noexcept
    {
        if (auto* const host = owner.GetHost())
            ++host->_interactionRevision;
        const auto lifetime = owner.GetLifetimeToken();
        // Every exit, including a failed getter, leaves a surviving model owner scheduled for fresh preparation.
        return wil::scope_exit([&owner, lifetime]() noexcept
        {
            if (! lifetime.expired())
                owner.RequestInvalidate();
        });
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
//   one source revision for text input (made on demand, or adopted from a TSF import), which it releases on focus loss and
//   destruction; the copy is keyed by that revision's identity, so a stale copy is never mistaken for the current text.
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
    ContentOnly
};
void RefreshWindowHostAccessibilitySnapshot(HWND hwnd, ControlHost* host, AccessibilityMutationKind mutation = AccessibilityMutationKind::General) noexcept;
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
void UpdateScrollbarAnimation(ControlHost& host, ScrollbarAnimationState& animation, bool trackHovered, bool thumbHovered, bool thumbDragging) noexcept;
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
