#pragma once

#include "DxUi.h"

#include <algorithm>
#include <bit>
#include <new>
#include <oleauto.h>
#include <span>

namespace DxUi
{
[[nodiscard]] std::weak_ptr<int> GetControlLifetimeToken(const Control& control) noexcept;

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
        try
        {
            ResizeTextLayoutTable(table, table.size() / 2u, generation);
        }
        catch (const std::bad_alloc&)
        {
            // Keeping the larger table is always correct; a later paint tries again.
        }
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

[[nodiscard]] D2D1_COLOR_F CompositeOverBackground(const D2D1_COLOR_F& overlay, const D2D1_COLOR_F& background) noexcept;
[[nodiscard]] uint32_t PackColor(const D2D1_COLOR_F& color) noexcept;
[[nodiscard]] D2D1_COLOR_F RainbowTint(std::wstring_view seed, bool dark) noexcept;
[[nodiscard]] D2D1_COLOR_F RainbowMenuSelectionTint(std::wstring_view seed, bool dark) noexcept;
[[nodiscard]] D2D1_COLOR_F RainbowFolderViewSelectionTint(uint32_t stableHash32, bool dark) noexcept;
[[nodiscard]] D2D1_COLOR_F ChooseContrastingTextColor(const D2D1_COLOR_F& background) noexcept;
[[nodiscard]] std::wstring_view GetCheckboxCheckGlyph(const ControlHost& host) noexcept;
[[nodiscard]] FontRole GetCheckboxCheckFontRole(const ControlHost& host) noexcept;
[[nodiscard]] DWRITE_READING_DIRECTION ResolveReadingDirection(FlowDirection flowDirection) noexcept;
void ResolveAdornmentColors(const ThemePalette& theme, AdornmentTone tone, D2D1_COLOR_F& fill, D2D1_COLOR_F& text) noexcept;
[[nodiscard]] bool RaiseWindowHostTextInputAutomationEvent(HWND hwnd, const Control* control, TextInputAutomationEventKind kind) noexcept;
void RaiseWindowHostDisclosureChanged(HWND hwnd, const Control* control, bool expanded) noexcept;
// A native menu popup's row focus, raised once its keyboard transition completes (ordinary window hosts announce
// focus from their snapshot changes instead).
void RaiseWindowHostFocusChanged(HWND hwnd, const Control* control) noexcept;
[[nodiscard]] ITextStoreACP* CreateNativeTextInputTextStore(ControlHost& host, Control& control) noexcept;
void DetachNativeTextInputTextStore(IUnknown* store) noexcept;
void DisconnectNativeTextInputTextStore(IUnknown* textStore) noexcept;
[[nodiscard]] bool IsRenderStageActiveForDebug() noexcept;
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
                      FlowDirection flowDirection                   = FlowDirection::LeftToRight);
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
    const ThemePalette& theme, bool enabled, bool hovered, bool pressed, bool focused, bool keyboardFocused, bool primary) noexcept;
[[nodiscard]] ButtonVisualStyle ResolveButtonVisualStyle(const ThemePalette& theme,
                                                         bool enabled,
                                                         bool hovered,
                                                         bool pressed,
                                                         bool focused,
                                                         bool keyboardFocused,
                                                         bool primary,
                                                         float hoverStrength,
                                                         float focusStrength) noexcept;
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

void RegisterWindowHostAccessibilityTarget(HWND hwnd, ControlHost* host) noexcept;
void UnregisterWindowHostAccessibilityTarget(HWND hwnd, ControlHost* host) noexcept;
void NotifyWindowHostAccessibilityDestroyed(HWND hwnd) noexcept;
// Republishes a window host's snapshot and raises what changed for clients: StructureChanged when semantic controls
// were added, removed or replaced, and the focus change when another element took focus inside a window that holds
// the foreground's keyboard focus. While the window itself gains focus, the system's focus event reports it; a move
// later in the turn of the gain is left to that event only in a window whose fragment-root GetFocus no call has ever
// begun on, because the first such call reports the moved-to element (see ReporterOfFocusMove).
void RefreshWindowHostAccessibilitySnapshot(HWND hwnd, ControlHost* host) noexcept;
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
[[nodiscard]] LRESULT ReturnWindowHostAccessibilityProvider(HWND hwnd, WPARAM wp, LPARAM lp) noexcept;
[[nodiscard]] bool TryHandleWindowHostAccessibilityMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LRESULT& outResult) noexcept;
#if DXUI_ENABLE_DIAGNOSTICS
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
// Holds every call of `hwnd`'s fragment-root GetFocus before it counts itself or reads the snapshot, so a test decides
// when the system's focus event is answered: each call sets `enteredEvent` on arrival and waits, for at most five seconds
// and never indefinitely, for `releaseEvent`. Null events clear the gate and return once the calls it held have left it.
void DebugSetAccessibilityFocusResolutionGateForTest(HWND hwnd, HANDLE enteredEvent, HANDLE releaseEvent) noexcept;
// Runs `hook` with `context` on the raising thread after each UI Automation selection event a host raises (an item's IsSelected
// change or selection event, or the invalidation of a tree's or grid's selection), since something else can run while they are
// raised (an outgoing call of UI Automation's in a single-threaded apartment dispatches messages): a test hides, removes or
// disconnects the control from it. A null hook clears it.
using AccessibilitySelectionEventHookForTest = void (*)(void* context) noexcept;
void DebugSetAccessibilitySelectionEventHookForTest(AccessibilitySelectionEventHookForTest hook, void* context) noexcept;
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

// Typeahead / case-insensitive search helpers (shared by Tree and ComboBox)
constexpr uint64_t kTypeaheadResetMs = 1000u;

[[nodiscard]] bool StartsWithInsensitive(std::wstring_view text, std::wstring_view prefix) noexcept;

// Single-line text editing helpers (shared by TextField and ComboBox)
[[nodiscard]] bool IsUtf16LeadSurrogate(wchar_t ch) noexcept;
[[nodiscard]] bool IsUtf16TrailSurrogate(wchar_t ch) noexcept;
[[nodiscard]] size_t StepToPreviousCodePoint(std::wstring_view text, size_t caretIndex) noexcept;
[[nodiscard]] size_t StepToNextCodePoint(std::wstring_view text, size_t caretIndex) noexcept;
[[nodiscard]] size_t StepToPreviousTextElement(std::wstring_view text, size_t caretIndex) noexcept;
[[nodiscard]] size_t StepToNextTextElement(std::wstring_view text, size_t caretIndex) noexcept;
[[nodiscard]] size_t CountTextElements(std::wstring_view text) noexcept;
[[nodiscard]] size_t SnapCaretIndexToTextElementBoundary(std::wstring_view text, size_t caretIndex) noexcept;
[[nodiscard]] bool IsWordCharacter(wchar_t ch) noexcept;
[[nodiscard]] bool IsPathSeparator(wchar_t ch) noexcept;
[[nodiscard]] size_t FindPreviousWordBoundary(std::wstring_view text, size_t caretIndex) noexcept;
[[nodiscard]] size_t FindNextWordBoundary(std::wstring_view text, size_t caretIndex) noexcept;
[[nodiscard]] bool ShouldEmitSingleLineBiDiTextMetric(std::wstring_view text, DWRITE_READING_DIRECTION readingDirection) noexcept;

void ClearSingleLineTextLayoutCache(SingleLineTextLayoutCache& cache, bool secureText = false) noexcept;

[[nodiscard]] float MeasureSingleLineTextWidthDip(const ControlHost* host,
                                                  std::wstring_view text,
                                                  FontRole role,
                                                  float heightDip                           = 24.0f,
                                                  DWRITE_READING_DIRECTION readingDirection = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT) noexcept;
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
[[nodiscard]] int GetWordSelectionClass(wchar_t value) noexcept;
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
