#include <DxUi/Failure.h>

#include "DxUi.Internal.h"
#include "TextStoreTarget.h"

#include "Support/Diagnostics.h"

#include <algorithm>
#include <atomic>
#include <exception>
#include <limits>

#include <inputscope.h>
#include <msctf.h>
#include <olectl.h>
#include <textstor.h>

namespace DxUi
{
namespace
{
constexpr TsViewCookie kTextStoreView = 1u;
// The most UTF-16 units one TSF edit inserts; the document itself has no ceiling of the store's.
constexpr size_t kMaximumTextStoreEditUnits = 65'536u;

[[nodiscard]] LONG ToAcp(size_t value) noexcept
{
    return static_cast<LONG>((std::min)(value, static_cast<size_t>((std::numeric_limits<LONG>::max)())));
}

[[nodiscard]] bool IsTextStoreLayoutAvailable(const D2D1_RECT_F& bounds) noexcept
{
    return bounds.right > bounds.left && bounds.bottom > bounds.top;
}

[[nodiscard]] size_t ClampAcp(LONG value, size_t textLength, bool negativeMeansEnd = false) noexcept
{
    if (value < 0)
    {
        return negativeMeansEnd ? textLength : 0u;
    }

    return (std::min)(static_cast<size_t>(value), textLength);
}

struct AcpRange
{
    size_t start = 0u;
    size_t end   = 0u;
};

[[nodiscard]] AcpRange ClampAcpRange(LONG start, LONG end, size_t textLength, bool endMinusOneMeansEnd = true) noexcept
{
    AcpRange range{ClampAcp(start, textLength), ClampAcp(end, textLength, endMinusOneMeansEnd)};
    if (range.end < range.start)
    {
        std::swap(range.start, range.end);
    }
    return range;
}

// A position TSF may observe as a change edge or a character edge: never between the units of a surrogate pair or
// between the CR and LF of a CRLF pair.
[[nodiscard]] bool IsTextStoreCut(std::wstring_view text, size_t position) noexcept
{
    if (position == 0u || position >= text.size())
        return true;
    const wchar_t before = text[position - 1u];
    const wchar_t after  = text[position];
    if (IsUtf16LeadSurrogate(before) && IsUtf16TrailSurrogate(after))
        return false;
    return ! (before == L'\r' && after == L'\n');
}

[[nodiscard]] size_t NextTextStoreCut(std::wstring_view text, size_t position) noexcept
{
    if (position >= text.size())
        return text.size();
    size_t next = StepToNextCodePoint(text, position);
    if (next < text.size() && text[position] == L'\r' && text[next] == L'\n')
        ++next;
    return next;
}

[[nodiscard]] size_t PreviousTextStoreCut(std::wstring_view text, size_t position) noexcept
{
    if (position == 0u)
        return 0u;
    size_t previous = StepToPreviousCodePoint(text, (std::min)(position, text.size()));
    if (previous > 0u && text[previous] == L'\n' && text[previous - 1u] == L'\r')
        --previous;
    return previous;
}

[[nodiscard]] bool IsInputScopeRequested(ULONG count, const TS_ATTRID* attributes) noexcept
{
    for (ULONG index = 0u; attributes && index < count; ++index)
    {
        if (IsEqualGUID(attributes[index], kTextStoreInputScopeProperty))
            return true;
    }
    return false;
}

[[nodiscard]] InputScope ToTsfInputScope(TextInputScope scope) noexcept
{
    switch (scope)
    {
        case TextInputScope::Number: return IS_NUMBER;
        case TextInputScope::Url: return IS_URL;
        case TextInputScope::Default: break;
    }
    return IS_DEFAULT;
}

[[nodiscard]] bool HasSelection(const SharedTextInputState& state) noexcept
{
    return state.selectionAnchorIndex.has_value() && state.selectionAnchorIndex.value() != state.caretIndex;
}

[[nodiscard]] AcpRange GetSelectionRange(const SharedTextInputState& state) noexcept
{
    const size_t length = state.View().size();
    if (! HasSelection(state))
    {
        const size_t caretIndex = (std::min)(state.caretIndex, length);
        return AcpRange{caretIndex, caretIndex};
    }

    return ClampAcpRange(ToAcp(state.selectionAnchorIndex.value()), ToAcp(state.caretIndex), length, false);
}

[[nodiscard]] bool IsSameSelection(const SharedTextInputState& left, const SharedTextInputState& right) noexcept
{
    return left.caretIndex == right.caretIndex && left.selectionAnchorIndex == right.selectionAnchorIndex;
}

[[nodiscard]] bool IsSameRect(const D2D1_RECT_F& left, const D2D1_RECT_F& right) noexcept
{
    return left.left == right.left && left.top == right.top && left.right == right.right && left.bottom == right.bottom;
}

[[nodiscard]] RECT DipRectToScreenRect(ControlHost& host, const D2D1_RECT_F& rectDip) noexcept
{
    POINT topLeft{static_cast<LONG>(std::lround(host.DipsToPixels(rectDip.left))), static_cast<LONG>(std::lround(host.DipsToPixels(rectDip.top)))};
    POINT bottomRight{static_cast<LONG>(std::lround(host.DipsToPixels(rectDip.right))), static_cast<LONG>(std::lround(host.DipsToPixels(rectDip.bottom)))};
    ClientToScreen(host.GetHwnd(), &topLeft);
    ClientToScreen(host.GetHwnd(), &bottomRight);
    return RECT{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
}

[[nodiscard]] D2D1_RECT_F ClipTextStoreRectToBounds(D2D1_RECT_F rect, const D2D1_RECT_F& bounds) noexcept
{
    rect.left   = std::clamp(rect.left, bounds.left, bounds.right);
    rect.right  = std::clamp(rect.right, rect.left, bounds.right);
    rect.top    = std::clamp(rect.top, bounds.top, bounds.bottom);
    rect.bottom = std::clamp(rect.bottom, rect.top, bounds.bottom);
    if (rect.right <= rect.left)
    {
        rect.right = (std::min)(bounds.right, rect.left + 1.0f);
    }
    if (rect.bottom <= rect.top)
    {
        rect.bottom = (std::min)(bounds.bottom, rect.top + 1.0f);
    }
    return rect;
}

[[nodiscard]] std::optional<D2D1_RECT_F> TryResolveTextStoreRangeRect(
    const ControlHost& host, const Control& control, const AcpRange& range, const D2D1_RECT_F& bounds, bool& clipped)
{
    clipped = false;
    std::optional<D2D1_RECT_F> result;
    if (range.start < range.end)
    {
        const std::optional<std::vector<D2D1_RECT_F>> rangeRects = control.TryGetTextInputRangeRects(host, range.start, range.end);
        if (rangeRects.has_value() && ! rangeRects.value().empty())
        {
            result = rangeRects.value().front();
            for (const D2D1_RECT_F& rangeRect : rangeRects.value())
            {
                D2D1_RECT_F& rect = result.value();
                rect.left         = (std::min)(rect.left, rangeRect.left);
                rect.top          = (std::min)(rect.top, rangeRect.top);
                rect.right        = (std::max)(rect.right, rangeRect.right);
                rect.bottom       = (std::max)(rect.bottom, rangeRect.bottom);
            }
        }
    }

    if (! result.has_value())
    {
        for (size_t index = range.start;; ++index)
        {
            const std::optional<D2D1_RECT_F> caretRect = control.TryGetTextInputCaretRect(host, index);
            if (! caretRect.has_value())
            {
                return std::nullopt;
            }

            if (! result.has_value())
            {
                result = caretRect.value();
            }
            else
            {
                D2D1_RECT_F& rect = result.value();
                rect.left         = (std::min)(rect.left, caretRect->left);
                rect.top          = (std::min)(rect.top, caretRect->top);
                rect.right        = (std::max)(rect.right, caretRect->right);
                rect.bottom       = (std::max)(rect.bottom, caretRect->bottom);
            }

            if (index == range.end)
            {
                break;
            }
        }
    }

    const D2D1_RECT_F& raw = result.value();
    clipped                = raw.left < bounds.left || raw.top < bounds.top || raw.right > bounds.right || raw.bottom > bounds.bottom;
    return ClipTextStoreRectToBounds(raw, bounds);
}

[[nodiscard]] bool TryGetCompositionAcpRange(ITfCompositionView* composition, ITfRange* suppliedRange, size_t& start, size_t& end) noexcept
{
    start = 0u;
    end   = 0u;
    wil::com_ptr_nothrow<ITfRange> range;
    if (suppliedRange)
    {
        range = suppliedRange;
    }
    else if (! composition || FAILED(composition->GetRange(range.put())) || ! range)
    {
        return false;
    }

    wil::com_ptr_nothrow<ITfRangeACP> rangeAcp;
    if (FAILED(range.query_to(rangeAcp.put())) || ! rangeAcp)
    {
        return false;
    }

    LONG acpStart = 0;
    LONG length   = 0;
    if (FAILED(rangeAcp->GetExtent(&acpStart, &length)) || acpStart < 0 || length < 0)
    {
        return false;
    }
    start = static_cast<size_t>(acpStart);
    end   = start + static_cast<size_t>(length);
    return end >= start;
}

} // namespace

// Reads and staging share the document with the host's native cache and the control (SharedText); only an edit allocates.
// Lifetime: every state below holds its own SharedText, so its text stays valid across the re-entrant imports, notifications,
// SetText calls and synchronizations that each method may trigger, whatever the control or cache then holds.
class NativeTextStoreTarget final : public TextStoreTarget
{
public:
    [[nodiscard]] size_t GetMaximumDocumentTextUnits() const noexcept override
    {
        return dynamic_cast<const RichTextEditor*>(GetLiveControl()) ? RichText::Document::MaximumUnits : (std::numeric_limits<size_t>::max)();
    }
    NativeTextStoreTarget(ControlHost& host, Control& control, bool hostDeferredWorkEligible) noexcept
        : _host(&host),
          _control(&control),
          _controlLifetime(GetControlLifetimeToken(control)),
          _hostDeferredWorkCookie(host._nativeTextStoreDispatchCookie),
          _hostDeferredWorkEligible(hostDeferredWorkEligible)
    {
    }
    void Disconnect() noexcept override
    {
        ControlHost* const host = _host;
        Control* const control  = _control;
        const auto lifetime     = _controlLifetime;
        if (_compositionBase && host && control && ! lifetime.expired() && IsControlInTree(host->GetRoot(), control))
        {
            // Deactivation invalidates the dispatch cookie before Pop. Allow the retiring store to
            // roll back its own still-visible preview only when no replacement TSF session won reentry.
            if (CanRestoreOwnedComposition(control) && IsCompositionTextCurrent())
            {
                // Pin the base: the restore re-enters the control and may reach Abandon through it.
                const SharedTextInputState base = _compositionBase.value();
                static_cast<void>(RestoreCompositionBase(base));
                if (! lifetime.expired() && CanRestoreOwnedComposition(control))
                    static_cast<void>(host->SetNativeTextInputCompositionRange(control, {}, {}, {}));
            }
            // A preview left in place (restore refused or not attempted) is the text now.
            if (! lifetime.expired() && IsControlInTree(host->GetRoot(), control))
                SharedTextInputAccess::SettlePreview(*control, *host);
        }
        Abandon();
    }
    void Abandon() noexcept
    {
        // Quiet teardown must sever observers without walking the retained tree or
        // restoring a composition through callbacks on an abandoned control.
        _stagedState.reset();
        _editBase.reset();
        _compositionBase.reset();
        _compositionExpectedText.reset();
        _editActive            = false;
        _compositionEndPending = false;
        _host                  = nullptr;
        _control               = nullptr;
        _controlLifetime.reset();
    }
    HRESULT BeginEdit(bool readWrite) noexcept override
    {
        _editActive = false;
        _editBase.reset();
        _stagedState.reset();
        SharedTextInputState state{};
        if (! IsCurrentTextServiceTarget() || ! ReadBackingState(state))
        {
            return TS_E_INVALIDPOS;
        }
        if (state.masked)
        {
            return E_ACCESSDENIED;
        }
        if (! readWrite)
        {
            _editActive = true;
            return S_OK;
        }
        if (state.readOnly)
        {
            return E_ACCESSDENIED;
        }
        _editBase   = std::move(state);
        _editActive = true;
        return S_OK;
    }
    HRESULT EndEdit(bool commit) noexcept override
    {
        _editActive = false;
        if (_compositionBase && ! IsCurrentTextServiceTarget())
        {
            SharedTextInputState base = std::move(_compositionBase.value());
            _compositionBase.reset();
            _compositionEndPending = false;
            _stagedState.reset();
            _editBase.reset();
            const bool restored = RestoreCompositionBase(base);
            _compositionExpectedText.reset();
            Control* const control = GetLiveControl();
            if (control && _host && CanRestoreOwnedComposition(control))
                static_cast<void>(_host->SetNativeTextInputCompositionRange(control, {}, {}, {}));
            return restored ? S_OK : E_FAIL;
        }
        if (! IsCurrentTextServiceTarget())
        {
            _stagedState.reset();
            _editBase.reset();
            return S_OK;
        }
        if (! commit)
        {
            _stagedState.reset();
            if (_compositionEndPending && _compositionBase)
            {
                SharedTextInputState base = std::move(_compositionBase.value());
                _compositionBase.reset();
                _compositionEndPending = false;
                const bool restored    = RestoreCompositionBase(base);
                _compositionExpectedText.reset();
                _editBase.reset();
                return restored ? S_OK : E_FAIL;
            }
            _editBase.reset();
            return S_OK;
        }

        if (! _stagedState)
        {
            bool applied = true;
            if (_compositionEndPending && _compositionBase)
            {
                SharedTextInputState current{};
                applied = ReadBackingState(current) && FinishComposition(current);
            }
            _editBase.reset();
            return applied ? S_OK : E_FAIL;
        }

        // The staged state owns its text; the imports below cannot release it while it is applied.
        SharedTextInputState staged = std::move(_stagedState.value());
        _stagedState.reset();
        bool applied = false;
        if (_compositionBase && ! IsCompositionTextCurrent())
        {
            // An application replaced the document after the preview was imported.
            // Drop this stale TSF write instead of restoring or overwriting that text.
            _compositionBase.reset();
            _compositionExpectedText.reset();
            _compositionEndPending = false;
            ClearCompositionRange();
            applied = true;
        }
        else if (_compositionEndPending && _compositionBase)
        {
            applied = FinishComposition(staged);
        }
        else
        {
            applied = ApplyControlState(staged, ! _compositionBase.has_value());
            if (applied && _compositionBase)
                RememberCompositionText(staged.text);
        }
        _editBase.reset();
        return applied ? S_OK : E_FAIL;
    }
    HRESULT StartComposition(ITfCompositionView* composition, BOOL* accepted) noexcept override
    {
        if (! accepted)
        {
            return E_POINTER;
        }
        *accepted = FALSE;
        if (! IsCurrentTextServiceTarget())
            return TS_E_INVALIDPOS;
        bool sequentialComposition = false;
        if (_compositionBase && _compositionEndPending)
        {
            // EndComposition can be followed by another start inside the same TSF lock.
            // Commit the completed composition now so the next one gets its own undo base.
            SharedTextInputState completedState{};
            if (! ReadSharedState(completedState) || ! FinishComposition(completedState) || ! IsCurrentTextServiceTarget())
                return E_FAIL;
            sequentialComposition = true;
        }
        if (_compositionBase)
        {
            // A second live composition cannot borrow the first one's prestate or
            // silently replace its range.
            return S_OK;
        }

        SharedTextInputState base{};
        if (sequentialComposition)
        {
            if (! ReadBackingState(base))
                return TS_E_INVALIDPOS;
        }
        else if (_editBase)
        {
            base = _editBase.value();
        }
        else if (! ReadBackingState(base))
        {
            return TS_E_INVALIDPOS;
        }
        if (base.masked || base.readOnly)
            return E_ACCESSDENIED;
        _compositionBase = std::move(base);
        RememberCompositionText(_compositionBase->text);

        // TSF can insert into the staged document and then start its composition
        // in the same lock. The undo base is the lock-entry prestate above; range
        // validation must use the current staged document instead.
        SharedTextInputState current{};
        if (! ReadSharedState(current))
        {
            _compositionBase.reset();
            _compositionExpectedText.reset();
            return TS_E_INVALIDPOS;
        }
        if (current.masked || current.readOnly)
        {
            _compositionBase.reset();
            _compositionExpectedText.reset();
            return E_ACCESSDENIED;
        }

        size_t start                     = 0u;
        size_t end                       = 0u;
        std::optional<size_t> startValue = 0u;
        std::optional<size_t> endValue   = 0u;
        if (TryGetCompositionAcpRange(composition, nullptr, start, end) && end <= current.View().size())
        {
            startValue = start;
            endValue   = end;
        }
        if (_host && _control && ! _controlLifetime.expired() && IsCurrentTextServiceTarget())
        {
            if (! _host->SetNativeTextInputCompositionRange(_control, startValue, endValue, {}))
            {
                _compositionBase.reset();
                _compositionExpectedText.reset();
                return E_FAIL;
            }
        }
        _compositionEndPending = false;
        *accepted              = TRUE;
        return S_OK;
    }
    HRESULT UpdateComposition(ITfCompositionView* composition, ITfRange* rangeNew) noexcept override
    {
        if (! _compositionBase || ! _host || ! _control || _controlLifetime.expired() || ! IsCurrentTextServiceTarget())
        {
            return TS_E_INVALIDPOS;
        }
        SharedTextInputState state{};
        if (! ReadSharedState(state))
        {
            return TS_E_INVALIDPOS;
        }
        if (state.masked || state.readOnly)
            return E_ACCESSDENIED;
        size_t start = 0u;
        size_t end   = 0u;
        if (! TryGetCompositionAcpRange(composition, rangeNew, start, end))
        {
            return S_OK;
        }
        if (end > state.View().size())
        {
            return E_INVALIDARG;
        }
        return _host->SetNativeTextInputCompositionRange(_control, start, end, end) ? S_OK : E_FAIL;
    }
    HRESULT EndComposition(ITfCompositionView*) noexcept override
    {
        if (! _compositionBase)
        {
            return S_OK;
        }
        if (_host && _control && ! _controlLifetime.expired() && IsCurrentTextServiceTarget())
        {
            if (! _host->SetNativeTextInputCompositionRange(_control, {}, {}, {}))
            {
                return E_FAIL;
            }
        }
        _compositionEndPending = true;
        if (_editActive)
        {
            return S_OK;
        }

        if (! IsCurrentTextServiceTarget())
        {
            SharedTextInputState base = std::move(_compositionBase.value());
            _compositionBase.reset();
            _compositionEndPending = false;
            const bool restored    = RestoreCompositionBase(base);
            _compositionExpectedText.reset();
            return restored ? S_OK : E_FAIL;
        }

        SharedTextInputState finalState{};
        if (! ReadBackingState(finalState))
        {
            _compositionBase.reset();
            _compositionExpectedText.reset();
            _compositionEndPending = false;
            return TS_E_INVALIDPOS;
        }
        if (finalState.masked || finalState.readOnly)
        {
            SharedTextInputState base = std::move(_compositionBase.value());
            _compositionBase.reset();
            _compositionEndPending = false;
            const bool restored    = RestoreCompositionBase(base);
            _compositionExpectedText.reset();
            return restored ? S_OK : E_FAIL;
        }
        return FinishComposition(finalState) ? S_OK : E_FAIL;
    }
    [[nodiscard]] HWND GetHwnd() const noexcept override
    {
        return _host ? _host->GetHwnd() : nullptr;
    }
    bool ScheduleLock() noexcept override
    {
        return _hostDeferredWorkEligible && _host && _host->ScheduleNativeTextStoreDeferredWork(_control, _hostDeferredWorkCookie);
    }
    HRESULT GetAcpFromScreenPoint(const POINT* ptScreen, LONG* pacp) const noexcept override
    {
        SharedTextInputState state{};
        Control* const control = GetLiveControl();
        if (! ReadSharedState(state) || ! control)
        {
            return TS_E_INVALIDPOS;
        }

        const std::optional<PointDip> pointDip = _host->ScreenPointToDipPoint(*ptScreen);
        if (! pointDip)
        {
            return TS_E_INVALIDPOS;
        }

        const D2D1_RECT_F bounds = ResolveTextViewportBounds();
        if (! IsTextStoreLayoutAvailable(bounds))
        {
            *pacp = 0;
            return TS_E_NOLAYOUT;
        }

        // The nearest caret position for any point; the store applies the GetACPFromPoint flags and character boxes.
        const D2D1_POINT_2F queryPoint = D2D1::Point2F(pointDip->x, pointDip->y);
        if (const std::optional<size_t> hitIndex = control->TryHitTestTextInputPoint(*_host, queryPoint); hitIndex.has_value())
        {
            *pacp = ToAcp((std::min)(hitIndex.value(), state.View().size()));
            return S_OK;
        }

        const DWRITE_READING_DIRECTION readingDirection = ResolveReadingDirection(control->GetFlowDirection());
        *pacp = ToAcp(HitTestCaretIndexDip(_host, state.View(), FontRole::Body, bounds, 0.0f, queryPoint, readingDirection));
        return S_OK;
    }
    HRESULT GetRangeScreenRect(LONG acpStart, LONG acpEnd, RECT* prc, BOOL* pfClipped) const noexcept override
    try
    {
        SharedTextInputState state{};
        Control* const control = GetLiveControl();
        if (! ReadSharedState(state) || ! control)
        {
            return TS_E_INVALIDPOS;
        }

        const D2D1_RECT_F bounds = ResolveTextViewportBounds();
        if (! IsTextStoreLayoutAvailable(bounds))
        {
            *prc       = RECT{};
            *pfClipped = TRUE;
            return TS_E_NOLAYOUT;
        }
        const AcpRange range                     = ClampAcpRange(acpStart, acpEnd, state.View().size(), false);
        bool clipped                             = false;
        const std::optional<D2D1_RECT_F> rectDip = TryResolveTextStoreRangeRect(*_host, *control, range, bounds, clipped);
        if (! rectDip.has_value())
        {
            *prc       = RECT{};
            *pfClipped = TRUE;
            return TS_E_NOLAYOUT;
        }

        *prc       = DipRectToScreenRect(*_host, rectDip.value());
        *pfClipped = clipped ? TRUE : FALSE;
        return S_OK;
    }
    catch (const std::bad_alloc&)
    {
        Failure::FailAllocation();
    }
    HRESULT GetScreenRect(RECT* prc) const noexcept override
    {
        if (! GetLiveControl())
        {
            return TS_E_INVALIDPOS;
        }

        const D2D1_RECT_F bounds = ResolveTextViewportBounds();
        if (! IsTextStoreLayoutAvailable(bounds))
        {
            *prc = RECT{};
            return TS_E_NOLAYOUT;
        }
        POINT topLeft{static_cast<LONG>(std::lround(_host->DipsToPixels(bounds.left))), static_cast<LONG>(std::lround(_host->DipsToPixels(bounds.top)))};
        POINT bottomRight{static_cast<LONG>(std::lround(_host->DipsToPixels(bounds.right))),
                          static_cast<LONG>(std::lround(_host->DipsToPixels(bounds.bottom)))};
        ClientToScreen(_host->GetHwnd(), &topLeft);
        ClientToScreen(_host->GetHwnd(), &bottomRight);
        *prc = RECT{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
        return S_OK;
    }
    bool SetCompositionAttributes(const TextStoreCompositionAttributes& attributes) noexcept override
    {
        Control* const control = GetLiveControl();
        if (! control || ! _compositionBase || ! IsCurrentTextServiceTarget())
            return false;
        return _host->SetNativeTextInputCompositionAttributes(control, attributes.targetStart, attributes.targetEnd, attributes.clauseBoundaries);
    }
    [[nodiscard]] Control* GetLiveControl() const noexcept
    {
        if (! _host || ! _control || _controlLifetime.expired() || ! IsControlInTree(_host->GetRoot(), _control))
        {
            return nullptr;
        }
        return _control;
    }

    [[nodiscard]] bool ReadPolicy(Policy& policy) const noexcept override
    {
        Control* const control = GetLiveControl();
        if (! control || ! IsCurrentTextServiceTarget())
            return false;
        if (const auto* field = dynamic_cast<const TextField*>(control))
        {
            policy = {field->IsReadOnly(), field->IsMasked(), field->IsMultiline(), ToTsfInputScope(field->GetInputScope())};
            return true;
        }
        // Flags only: a rich editor's revision is not materialized for them.
        SharedTextInputState state{};
        if (! ReadSharedStateForNotification(state))
            return false;
        policy = {state.readOnly, state.masked, state.multiline};
        return true;
    }

    // The copying interface stays for completeness; the store itself reads and stages shared text.
    [[nodiscard]] bool ReadState(TextInputState& outState) const noexcept override
    {
        try
        {
            SharedTextInputState state{};
            if (! ReadSharedState(state))
            {
                outState = {};
                return false;
            }
            outState = {std::wstring(state.View()),
                        state.selectionAnchorIndex,
                        state.caretIndex,
                        state.firstVisibleLine,
                        state.readOnly,
                        state.masked,
                        state.multiline};
            return true;
        }
        catch (const std::bad_alloc&)
        {
            Failure::FailAllocation();
        }
    }

    [[nodiscard]] bool ApplyState(const TextInputState& state, bool notifyChange) noexcept override
    {
        try
        {
            const SharedTextInputState shared{MakeSharedText(state.text),
                                              state.selectionAnchorIndex,
                                              state.caretIndex,
                                              state.firstVisibleLine,
                                              state.readOnly,
                                              state.masked,
                                              state.multiline};
            return ApplySharedState(shared, notifyChange);
        }
        catch (const std::bad_alloc&)
        {
            Failure::FailAllocation();
        }
    }

    [[nodiscard]] bool ReadSharedState(SharedTextInputState& outState) const noexcept override
    {
        Control* const control = GetLiveControl();
        if (! control || ! IsCurrentTextServiceTarget() || (dynamic_cast<const TextField*>(control) && static_cast<const TextField*>(control)->IsMasked()))
        {
            outState = {};
            return false;
        }
        if (_stagedState)
        {
            outState = _stagedState.value();
            return ! outState.masked;
        }
        return ReadBackingState(outState) && ! outState.masked;
    }

    [[nodiscard]] bool ReadSharedStateForNotification(SharedTextInputState& outState) const noexcept override
    {
        Control* const control = GetLiveControl();
        if (! control || ! IsCurrentTextServiceTarget() || (dynamic_cast<const TextField*>(control) && static_cast<const TextField*>(control)->IsMasked()))
        {
            outState = {};
            return false;
        }
        if (_stagedState)
        {
            outState = _stagedState.value();
            return ! outState.masked;
        }
        return ReadBackingState(outState, false) && ! outState.masked;
    }

    [[nodiscard]] bool ApplySharedState(const SharedTextInputState& state, bool notifyChange) noexcept override
    {
        if (state.masked || state.readOnly || ! state.text || ! IsCurrentTextServiceTarget())
        {
            return false;
        }
        if (_compositionBase && ! IsCompositionTextCurrent())
        {
            _compositionBase.reset();
            _compositionExpectedText.reset();
            _compositionEndPending = false;
            _stagedState.reset();
            _editBase.reset();
            ClearCompositionRange();
            return false;
        }
        if (! _editActive)
        {
            const bool applied = ApplyControlState(state, notifyChange);
            if (applied && _compositionBase)
                RememberCompositionText(state.text);
            return applied;
        }
        SharedTextInputState current{};
        if (! ReadBackingState(current) || current.masked || current.readOnly)
            return false;
        if (! _editBase)
        {
            // The backing state just read is the lock-entry state: nothing has been applied in this lock yet.
            _editBase = std::move(current);
        }
        // Staging shares the caller's buffer: the store's edit made it, and nothing writes to a shared buffer.
        _stagedState = state;
        return true;
    }

private:
    [[nodiscard]] bool CanRestoreOwnedComposition(Control* expectedControl) const noexcept
    {
        if (! _host || ! expectedControl || GetLiveControl() != expectedControl)
            return false;
        if (! _hostDeferredWorkEligible)
            return true;
        if (_host->_nativeTextStoreDispatchCookie == _hostDeferredWorkCookie)
            return IsCurrentTextServiceTarget();
        return ! (_host->_nativeTextInputTsfActive && _host->_nativeTextInputTsfControl == expectedControl);
    }

    [[nodiscard]] bool IsCurrentTextServiceTarget() const noexcept
    {
        Control* const control = GetLiveControl();
        if (! control || ! _host)
            return false;
        if (_hostDeferredWorkEligible && _host->_nativeTextStoreDispatchCookie != _hostDeferredWorkCookie)
            return false;
        if (_host->GetFocusControl() != control || ! control->HasFocus() || ! IsControlEffectivelyInteractive(_host->GetRoot(), control))
            return false;

        const HWND hwnd = _host->GetHwnd();
        if (! hwnd)
            return true; // Headless text-store targets are used by focused unit fixtures.
        if (! IsWindow(hwnd) || ! IsWindowVisible(hwnd) || GetFocus() != hwnd)
            return false;
        RECT client{};
        return GetClientRect(hwnd, &client) != FALSE && client.right > client.left && client.bottom > client.top;
    }

    // The document the composition's preview left in the control. Holding the buffer keeps the content comparable even after
    // the control moves on to another buffer; a different buffer with equal content still compares equal.
    void RememberCompositionText(SharedText text) noexcept
    {
        _compositionExpectedText = std::move(text);
    }

    [[nodiscard]] bool IsCompositionTextCurrent() const noexcept
    {
        if (! _compositionBase || ! _compositionExpectedText)
            return false;
        SharedTextInputState current{};
        return ReadBackingState(current) && SharedTextEquals(current.text, _compositionExpectedText);
    }

    void ClearCompositionRange() noexcept
    {
        Control* const control = GetLiveControl();
        if (_host && control && IsCurrentTextServiceTarget())
            static_cast<void>(_host->SetNativeTextInputCompositionRange(control, {}, {}, {}));
    }

    // The control's state with its document shared: from the host's native cache when this control owns the session, else
    // from the control itself. On success the text is never null.
    // A rich editor's document travels as its immutable revision. Every read for a lock materializes it (at most once per
    // revision: the host cache and the editor keep the copy); only the change notification, which compares and finds the
    // changed range in place, reads without materializing.
    [[nodiscard]] bool ReadBackingState(SharedTextInputState& outState, bool materialize = true) const noexcept
    {
        try
        {
            Control* const control = GetLiveControl();
            if (! control)
            {
                return false;
            }

            if (control == _host->_nativeTextInputControl && _host->_nativeTextInputStateCacheValid &&
                (_host->_nativeTextInputCacheText || _host->_nativeTextInputCacheSource))
            {
                const NativeTextInputState& cache = _host->_nativeTextInputStateCache;
                // A notification read of a rich editor's revision carries the revision alone: holding the materialized
                // copy as well (the observed state keeps what it reads) would keep the editor from updating that copy in
                // place after the next keystroke.
                outState.source = _host->_nativeTextInputCacheSource;
                outState.text = materialize ? _host->MaterializeNativeTextInputCacheText() : outState.source ? SharedText() : _host->_nativeTextInputCacheText;
                outState.selectionAnchorIndex = cache.selectionAnchorIndex;
                outState.caretIndex           = cache.caretIndex;
                outState.firstVisibleLine     = cache.firstVisibleLine;
                outState.readOnly             = cache.readOnly;
                outState.masked               = cache.masked;
                outState.multiline            = cache.multiline;
                if (const auto* textField = dynamic_cast<const TextField*>(control))
                {
                    outState.readOnly = textField->IsReadOnly();
                    outState.masked   = textField->IsMasked();
                }
                return true;
            }

            // Without the session cache, read the control directly: its document shared, its selection as the public
            // selection range reports it.
            std::optional<std::pair<size_t, size_t>> selection;
            if (const auto* textField = dynamic_cast<const TextField*>(control))
                selection = textField->GetSelectionRange();
            else if (const auto* comboBox = dynamic_cast<const ComboBox*>(control); comboBox && comboBox->IsEditable())
                selection = comboBox->GetEditableSelectionRange();
            else if (! dynamic_cast<const RichTextEditor*>(control))
                return false;
            SharedTextInputState exported{};
            if (! SharedTextInputAccess::Export(*control, exported) || ! exported.HasDocument())
                return false;
            if (dynamic_cast<const RichTextEditor*>(control))
            {
                if (materialize)
                    SharedTextInputAccess::Materialize(control, exported);
                else if (exported.source)
                    exported.text.reset(); // As above: a notification read keeps the revision, not the editor's copy.
                outState = std::move(exported);
                return true;
            }
            SharedTextInputAccess::Materialize(control, exported);
            outState            = {};
            outState.text       = std::move(exported.text);
            outState.readOnly   = exported.readOnly;
            outState.masked     = exported.masked;
            outState.caretIndex = outState.text->size();
            if (selection)
            {
                outState.selectionAnchorIndex = selection->first;
                outState.caretIndex           = selection->second;
            }
            return true;
        }
        catch (const std::bad_alloc&)
        {
            Failure::FailAllocation();
        }
    }

    [[nodiscard]] bool ApplyControlState(const SharedTextInputState& state, bool notifyChange) noexcept
    {
        Control* const control            = GetLiveControl();
        const auto* eligibilityTextField  = dynamic_cast<const TextField*>(control);
        const auto* eligibilityRichEditor = dynamic_cast<const RichTextEditor*>(control);
        const auto* eligibilityComboBox   = dynamic_cast<const ComboBox*>(control);
        if (! control || ! IsCurrentTextServiceTarget() || state.readOnly || state.masked ||
            (eligibilityTextField && (eligibilityTextField->IsMasked() || eligibilityTextField->IsReadOnly())) ||
            (eligibilityRichEditor && eligibilityRichEditor->IsReadOnly()) ||
            (! eligibilityTextField && ! eligibilityRichEditor && ! (eligibilityComboBox && eligibilityComboBox->IsEditable())))
        {
            return false;
        }

        Control* const previousFocus = _host->GetFocusControl();
        try
        {
            if (! SharedTextInputAccess::Import(*control, *_host, state, notifyChange))
                return false;
            // Import can synchronously notify user code, retire/rebind the owner or replace the tree, focus or text. Match the
            // same source and focus before synchronizing the shared cache; never apply stale selection afterward.
            if (GetLiveControl() != control || ! IsCurrentTextServiceTarget() || _host->GetFocusControl() != previousFocus ||
                ! IsControlTextCurrent(*control, state.text))
                return false;
            _host->SyncTextInput(control);
            if (GetLiveControl() != control || ! IsCurrentTextServiceTarget())
                return false;
            _host->Invalidate();
            return true;
        }
        catch (const std::exception&)
        {
            Failure::FailUnexpected();
        }
    }

    // Whether the live control still shows `text`; a control that adopted the buffer compares without reading it.
    // A rich editor compares its source in place (or by the buffer it adopted), never through a flattened copy.
    [[nodiscard]] static bool IsControlTextCurrent(const Control& control, const SharedText& text)
    {
        return SharedTextInputAccess::ShowsText(control, text);
    }

    [[nodiscard]] bool FinishComposition(const SharedTextInputState& finalState) noexcept
    {
        if (! _compositionBase)
        {
            _compositionEndPending = false;
            return ApplyControlState(finalState, true);
        }

        if (! IsCompositionTextCurrent())
        {
            // SetText or another application edit replaced the preview. A late TSF
            // completion must not restore the old base or publish stale staged text.
            _compositionBase.reset();
            _compositionExpectedText.reset();
            _compositionEndPending = false;
            _stagedState.reset();
            _editBase.reset();
            ClearCompositionRange();
            return true;
        }

        // Commit keeps its double import: restore the lock-entry base without notifying, then import the final text as one
        // notifying edit, so undo removes the whole composition in one step.
        SharedTextInputState base = std::move(_compositionBase.value());
        _compositionBase.reset();
        _compositionEndPending = false;
        const bool restored    = RestoreCompositionBase(base);
        _compositionExpectedText.reset();
        if (! restored)
            return false;
        if (SharedTextEquals(base.text, finalState.text))
        {
            return ApplyControlState(finalState, false);
        }
        return ApplyControlState(finalState, true);
    }

    // Every path that ends a composition by restoring (or failing to restore) its base comes through here. Whatever the
    // outcome, a control that kept preview state (an editable ComboBox) then takes the text it shows as committed.
    [[nodiscard]] bool RestoreCompositionBase(const SharedTextInputState& base) noexcept
    {
        const bool restored = RestoreCompositionBaseText(base);
        SettleLeftoverPreview();
        return restored;
    }

    void SettleLeftoverPreview() noexcept
    {
        if (Control* const control = GetLiveControl(); control && _host)
            SharedTextInputAccess::SettlePreview(*control, *_host);
    }

    [[nodiscard]] bool RestoreCompositionBaseText(const SharedTextInputState& base) noexcept
    {
        Control* const control = GetLiveControl();
        if (! control)
            return false;
        try
        {
            SharedTextInputState current{};
            if (! ReadBackingState(current))
                return false;
            if (! _compositionExpectedText || ! SharedTextEquals(current.text, _compositionExpectedText))
                return true;
            if (! CanRestoreOwnedComposition(control))
                return false;

            SharedTextInputState restore = base;
            // Cancellation rolls back preview content and selection while preserving
            // policy changes made by the application during composition.
            restore.readOnly = current.readOnly;
            restore.masked   = current.masked;
            if (! dynamic_cast<RichTextEditor*>(control) && ! dynamic_cast<TextField*>(control) &&
                ! (dynamic_cast<ComboBox*>(control) && static_cast<ComboBox*>(control)->IsEditable()))
                return false;
            if (! SharedTextInputAccess::Import(*control, *_host, restore, false))
                return false;

            if (GetLiveControl() != control || ! CanRestoreOwnedComposition(control))
                return false;
            _host->SyncTextInput(control);
            if (GetLiveControl() != control || ! CanRestoreOwnedComposition(control))
                return false;
            SharedTextInputState afterSync{};
            if (! ReadBackingState(afterSync) || ! SharedTextEquals(afterSync.text, restore.text))
                return false;
            _host->Invalidate();
            return true;
        }
        catch (const std::bad_alloc&)
        {
            Failure::FailAllocation();
        }
        catch (const std::exception&)
        {
            Failure::FailUnexpected();
        }
    }

    [[nodiscard]] D2D1_RECT_F ResolveTextViewportBounds() const noexcept override
    {
        Control* const control = GetLiveControl();
        if (! control)
        {
            return D2D1::RectF();
        }
        if (const std::optional<D2D1_RECT_F> viewport = control->TryGetTextInputViewportRect(); viewport.has_value())
        {
            return viewport.value();
        }
        return control->GetHitBounds();
    }

    ControlHost* _host = nullptr;
    Control* _control  = nullptr;
    std::weak_ptr<int> _controlLifetime;
    UINT_PTR _hostDeferredWorkCookie = 0u;
    bool _hostDeferredWorkEligible   = false;
    std::optional<SharedTextInputState> _editBase;
    std::optional<SharedTextInputState> _stagedState;
    std::optional<SharedTextInputState> _compositionBase;
    SharedText _compositionExpectedText;
    bool _editActive            = false;
    bool _compositionEndPending = false;
};

namespace
{

class TextStoreACP final : public ITextStoreACP, public ITextStoreACP2, public ITfContextOwnerCompositionSink, public ITfInputScope, public ITfTextEditSink
{
public:
    explicit TextStoreACP(std::shared_ptr<TextStoreTarget> target) noexcept : _target(std::move(target))
    {
    }

    TextStoreACP(const TextStoreACP&)            = delete;
    TextStoreACP& operator=(const TextStoreACP&) = delete;
    TextStoreACP(TextStoreACP&&)                 = delete;
    TextStoreACP& operator=(TextStoreACP&&)      = delete;

    ~TextStoreACP() noexcept
    {
        DetachHost();
    }

    void DetachHost() noexcept
    {
        if (auto* nativeTarget = dynamic_cast<NativeTextStoreTarget*>(_target.get()))
            nativeTarget->Abandon();
        Disconnect();
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) noexcept override
    {
        const Failure::Scope failureScope;
        if (! ppvObject)
        {
            return E_POINTER;
        }

        *ppvObject = nullptr;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(ITextStoreACP))
        {
            *ppvObject = static_cast<ITextStoreACP*>(this);
            AddRef();
            return S_OK;
        }
        if (riid == __uuidof(ITextStoreACP2))
        {
            *ppvObject = static_cast<ITextStoreACP2*>(this);
            AddRef();
            return S_OK;
        }
        if (riid == __uuidof(ITfContextOwnerCompositionSink))
        {
            *ppvObject = static_cast<ITfContextOwnerCompositionSink*>(this);
            AddRef();
            return S_OK;
        }
        if (riid == __uuidof(ITfInputScope))
        {
            *ppvObject = static_cast<ITfInputScope*>(this);
            AddRef();
            return S_OK;
        }
        if (riid == __uuidof(ITfTextEditSink))
        {
            *ppvObject = static_cast<ITfTextEditSink*>(this);
            AddRef();
            return S_OK;
        }

        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return _referenceCount.fetch_add(1u, std::memory_order_relaxed) + 1u;
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        const ULONG remaining = _referenceCount.fetch_sub(1u, std::memory_order_acq_rel) - 1u;
        if (remaining == 0u)
        {
            delete this;
        }
        return remaining;
    }

    void Disconnect() noexcept
    {
        ++_sinkConnectionRevision;
        _sink.reset();
        _sinkMask                    = 0u;
        _lockFlags                   = 0u;
        _pendingLockFlags            = 0;
        _pendingExternalNotification = false;
        _pendingLayoutNotification   = false;
        _target->Disconnect();
        // Dropping the observed buffer wipes it once its last owner releases it.
        _observedState    = SharedTextInputState{};
        _observedViewport = D2D1::RectF();
        _hasObservedState = false;
    }

    HRESULT STDMETHODCALLTYPE OnStartComposition(ITfCompositionView* composition, BOOL* outAccepted) noexcept override
    {
        const Failure::Scope failureScope;
        if (! outAccepted)
        {
            return E_POINTER;
        }

        *outAccepted = FALSE;
        return _target->StartComposition(composition, outAccepted);
    }

    HRESULT STDMETHODCALLTYPE OnUpdateComposition(ITfCompositionView* composition, ITfRange* rangeNew) noexcept override
    {
        const Failure::Scope failureScope;
        return _target->UpdateComposition(composition, rangeNew);
    }

    HRESULT STDMETHODCALLTYPE OnEndComposition(ITfCompositionView* composition) noexcept override
    {
        const Failure::Scope failureScope;
        return _target->EndComposition(composition);
    }

    // After each edit session TSF grants a read-only cookie; GUID_PROP_ATTRIBUTE then describes the composition's
    // clauses and conversion target, which the target publishes beside the composition range.
    HRESULT STDMETHODCALLTYPE OnEndEdit(ITfContext* context, TfEditCookie readCookie, ITfEditRecord* /*editRecord*/) noexcept override
    {
        const Failure::Scope failureScope;
        if (! context)
            return E_INVALIDARG;
        try
        {
            if (ReadCompositionAttributeSpans(*context, readCookie))
            {
                ResolveTextStoreCompositionAttributes(_attributeCompositionStart, _attributeCompositionEnd, _attributeSpans, _compositionAttributes);
                static_cast<void>(_target->SetCompositionAttributes(_compositionAttributes));
            }
        }
        catch (const std::bad_alloc&)
        {
            Failure::FailAllocation();
        }
        return S_OK;
    }

    void SetTextEditSinkCookie(DWORD cookie) noexcept
    {
        _textEditSinkCookie = cookie;
    }

    [[nodiscard]] DWORD TakeTextEditSinkCookie() noexcept
    {
        return std::exchange(_textEditSinkCookie, TF_INVALID_COOKIE);
    }

    HRESULT STDMETHODCALLTYPE AdviseSink(REFIID riid, IUnknown* punk, DWORD dwMask) noexcept override
    {
        const Failure::Scope failureScope;
        if (! punk)
        {
            return E_INVALIDARG;
        }
        if (riid != __uuidof(ITextStoreACPSink))
        {
            return E_NOINTERFACE;
        }
        if (_sink)
        {
            // The advised sink advising again changes its mask; another sink is refused, as ITextStoreACP::AdviseSink says.
            wil::com_ptr_nothrow<IUnknown> requestedIdentity;
            wil::com_ptr_nothrow<IUnknown> advisedIdentity;
            if (FAILED(punk->QueryInterface(__uuidof(IUnknown), requestedIdentity.put_void())) ||
                FAILED(_sink->QueryInterface(__uuidof(IUnknown), advisedIdentity.put_void())) || ! requestedIdentity || ! advisedIdentity)
            {
                return E_FAIL;
            }
            if (requestedIdentity.get() != advisedIdentity.get())
            {
                return CONNECT_E_ADVISELIMIT;
            }
            _sinkMask = dwMask;
            return S_OK;
        }

        ITextStoreACPSink* rawSink = nullptr;
        const HRESULT hr           = punk->QueryInterface(__uuidof(ITextStoreACPSink), reinterpret_cast<void**>(&rawSink));
        if (FAILED(hr))
        {
            return hr;
        }

        _sink.attach(rawSink);
        _sinkMask = dwMask;
        ++_sinkConnectionRevision;
        CaptureObservedState();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE UnadviseSink(IUnknown* punk) noexcept override
    {
        const Failure::Scope failureScope;
        if (! punk)
        {
            return E_INVALIDARG;
        }
        if (! _sink)
        {
            return E_FAIL;
        }

        wil::com_ptr_nothrow<IUnknown> requestedIdentity;
        HRESULT hr = punk->QueryInterface(__uuidof(IUnknown), requestedIdentity.put_void());
        if (FAILED(hr) || ! requestedIdentity)
        {
            return E_FAIL;
        }

        wil::com_ptr_nothrow<IUnknown> advisedIdentity;
        hr = _sink->QueryInterface(__uuidof(IUnknown), advisedIdentity.put_void());
        if (FAILED(hr) || ! advisedIdentity || requestedIdentity.get() != advisedIdentity.get())
        {
            return E_FAIL;
        }

        _sink.reset();
        _sinkMask = 0u;
        ++_sinkConnectionRevision;
        _pendingLayoutNotification = false;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE RequestLock(DWORD dwLockFlags, HRESULT* phrSession) noexcept override
    {
        const Failure::Scope failureScope;
        if (! phrSession)
        {
            return E_POINTER;
        }

        *phrSession = S_OK;
        if (_lockFlags != 0u)
        {
            *phrSession = TS_E_SYNCHRONOUS;
            if ((dwLockFlags & TS_LF_SYNC) == 0)
            {
                const DWORD previous = _pendingLockFlags;
                _pendingLockFlags |= (dwLockFlags & TS_LF_READWRITE);
                if (_target->ScheduleLock())
                    *phrSession = TS_S_ASYNC;
                else
                    _pendingLockFlags = previous;
            }
            return S_OK;
        }

        _lockFlags         = dwLockFlags;
        _hasSelfEditInLock = false;
        // A deferred request may have been accepted while the previous lock was held, but its post could fail while
        // that lock was released. Retain the coalesced flags and make one bounded retry when TSF next requests a lock.
        // Mark this lock active first so a nested message loop can defer, never recursively grant, the queued request.
        if (_pendingLockFlags != 0u)
            static_cast<void>(_target->ScheduleLock());
        wil::com_ptr_nothrow<ITextStoreACPSink> sink = _sink;
        const bool isReadWriteLock                   = (dwLockFlags & TS_LF_READWRITE) == TS_LF_READWRITE;
        const HRESULT beginHr                        = _target->BeginEdit(isReadWriteLock);
        if (FAILED(beginHr))
        {
            ReleaseLockAndSchedulePending();
            *phrSession = beginHr;
            return S_OK;
        }
        bool editTransactionStarted = false;
        if (sink && isReadWriteLock)
        {
            const HRESULT startHr = sink->OnStartEditTransaction();
            if (FAILED(startHr))
            {
                *phrSession = startHr;
                static_cast<void>(_target->EndEdit(false));
                ReleaseLockAndSchedulePending();
                return S_OK;
            }
            editTransactionStarted = true;
        }

        if (sink)
        {
            *phrSession = sink->OnLockGranted(dwLockFlags);
        }

        const HRESULT applyHr = _target->EndEdit(SUCCEEDED(*phrSession));
        if (SUCCEEDED(*phrSession) && FAILED(applyHr))
            *phrSession = applyHr;
        if (SUCCEEDED(*phrSession) && (! _pendingExternalNotification || _hasSelfEditInLock))
            CaptureObservedState();
        if (sink && editTransactionStarted)
        {
            const HRESULT endHr = sink->OnEndEditTransaction();
            if (SUCCEEDED(*phrSession) && FAILED(endHr))
            {
                *phrSession = endHr;
            }
        }
        ReleaseLockAndSchedulePending();
        return S_OK;
    }

    HRESULT DispatchPendingLock() noexcept
    {
        if (_lockFlags != 0)
            return TS_E_NOLOCK;
        const DWORD flags = std::exchange(_pendingLockFlags, 0);
        if (! flags || ! _sink)
            return S_FALSE;
        HRESULT session  = S_OK;
        const HRESULT hr = RequestLock(flags, &session);
        return FAILED(hr) ? hr : session;
    }

    void ScheduleReconciliation() noexcept
    {
        _pendingExternalNotification = true;
        if (_lockFlags == 0u && _target)
            static_cast<void>(_target->ScheduleLock());
    }

#if DXUI_ENABLE_DIAGNOSTICS
    void DebugGetNotificationCounts(uint64_t& textChanges, uint64_t& selectionChanges, uint64_t& layoutChanges) const noexcept
    {
        textChanges      = _debugTextChangeNotifications;
        selectionChanges = _debugSelectionChangeNotifications;
        layoutChanges    = _debugLayoutChangeNotifications;
    }
#endif

    void DispatchPendingExternalChanges() noexcept
    {
        // The queued turn may be pumped by a nested loop before RequestLock returns. Leave the pending
        // notification coalesced; ReleaseLockAndSchedulePending posts it again after the lock unwinds.
        if (_lockFlags != 0u)
            return;

        const auto target                            = _target;
        wil::com_ptr_nothrow<ITextStoreACPSink> sink = _sink;
        const DWORD sinkMask                         = _sinkMask;
        const uint64_t sinkConnectionRevision        = _sinkConnectionRevision;
        const bool layoutWasPending                  = std::exchange(_pendingLayoutNotification, false);
        const bool externalLayoutNotified            = _pendingExternalNotification && NotifyExternalChangesIfNeeded();
        const auto notificationTurnIsCurrent         = [this, target, sink, sinkMask, sinkConnectionRevision]() noexcept
        {
            if (! target || _target != target || ! sink || _sink.get() != sink.get() || _sinkMask != sinkMask ||
                _sinkConnectionRevision != sinkConnectionRevision || _lockFlags != 0u || ! _hasObservedState)
                return false;
            SharedTextInputState currentState{};
            return target->ReadSharedStateForNotification(currentState) && ! currentState.masked && SameTextInputContent(currentState, _observedState) &&
                   IsSameSelection(currentState, _observedState) && currentState.firstVisibleLine == _observedState.firstVisibleLine &&
                   currentState.masked == _observedState.masked && currentState.multiline == _observedState.multiline &&
                   IsSameRect(target->ResolveTextViewportBounds(), _observedViewport);
        };
        if (layoutWasPending && ! externalLayoutNotified)
        {
            if (notificationTurnIsCurrent() && (_sinkMask & TS_AS_LAYOUT_CHANGE) != 0u)
            {
#if DXUI_ENABLE_DIAGNOSTICS
                ++_debugLayoutChangeNotifications;
#endif
                static_cast<void>(sink->OnLayoutChange(TS_LC_CHANGE, kTextStoreView));
            }
        }
        // A layout queued reentrantly belongs to a later posted turn; keep it coalesced and let the host/app scheduler
        // deliver it after this notification sequence has returned.
    }

    HRESULT STDMETHODCALLTYPE GetStatus(TS_STATUS* pdcs) noexcept override
    {
        const Failure::Scope failureScope;
        if (! pdcs)
        {
            return E_POINTER;
        }

        pdcs->dwDynamicFlags = 0u;
        pdcs->dwStaticFlags  = TS_SS_NOHIDDENTEXT;
        TextStoreTarget::Policy policy;
        if (! _target->ReadPolicy(policy))
            return TS_E_INVALIDPOS;
        if (policy.readOnly || policy.masked)
            pdcs->dwDynamicFlags |= TS_SD_READONLY;
        if (policy.multiline)
            pdcs->dwStaticFlags |= TS_SS_MULTILINE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetInputScopes(InputScope** scopes, UINT* count) noexcept override
    {
        const Failure::Scope failureScope;
        if (! scopes || ! count)
            return E_POINTER;
        *scopes = nullptr;
        *count  = 0u;
        TextStoreTarget::Policy policy;
        if (! _target->ReadPolicy(policy))
            return TF_E_DISCONNECTED;
        wil::unique_cotaskmem_ptr<InputScope> value(static_cast<InputScope*>(CoTaskMemAlloc(sizeof(InputScope))));
        if (! value)
            return E_OUTOFMEMORY;
        *value.get() = policy.masked ? IS_PASSWORD : policy.inputScope;
        *scopes      = value.release(); // The COM caller takes this CoTaskMem allocation.
        *count       = 1u;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPhrase(BSTR** phrases, UINT* count) noexcept override
    {
        const Failure::Scope failureScope;
        if (! phrases || ! count)
            return E_POINTER;
        *phrases = nullptr;
        *count   = 0u;
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetRegularExpression(BSTR* expression) noexcept override
    {
        const Failure::Scope failureScope;
        if (! expression)
            return E_POINTER;
        *expression = nullptr;
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetSRGS(BSTR* grammar) noexcept override
    {
        const Failure::Scope failureScope;
        if (! grammar)
            return E_POINTER;
        *grammar = nullptr;
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetXML(BSTR* xml) noexcept override
    {
        const Failure::Scope failureScope;
        if (! xml)
            return E_POINTER;
        *xml = nullptr;
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE QueryInsert(LONG acpTestStart, LONG acpTestEnd, ULONG cch, LONG* pacpResultStart, LONG* pacpResultEnd) noexcept override
    {
        const Failure::Scope failureScope;
        if (pacpResultStart)
            *pacpResultStart = 0;
        if (pacpResultEnd)
            *pacpResultEnd = 0;
        if (! pacpResultStart || ! pacpResultEnd)
        {
            return E_POINTER;
        }

        SharedTextInputState state{};
        if (! ReadState(state))
        {
            return TS_E_INVALIDPOS;
        }

        const AcpRange range = ClampAcpRange(acpTestStart, acpTestEnd, state.View().size());
        if (state.readOnly)
            return TS_E_READONLY;
        if (! ReplacementFitsCapacity(state.View().size(), range, cch))
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        *pacpResultStart = ToAcp(range.start);
        *pacpResultEnd   = ToAcp(range.start + cch);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetSelection(ULONG ulIndex, ULONG ulCount, TS_SELECTION_ACP* pSelection, ULONG* pcFetched) noexcept override
    {
        const Failure::Scope failureScope;
        if (! HasReadLock())
        {
            return TS_E_NOLOCK;
        }
        if (! pSelection || ! pcFetched)
        {
            return E_POINTER;
        }

        *pcFetched = 0u;
        if (ulCount == 0u || (ulIndex != TS_DEFAULT_SELECTION && ulIndex != 0u))
        {
            return S_OK;
        }

        SharedTextInputState state{};
        if (! ReadState(state))
        {
            return TS_E_INVALIDPOS;
        }

        const AcpRange range             = GetSelectionRange(state);
        pSelection[0].acpStart           = ToAcp(range.start);
        pSelection[0].acpEnd             = ToAcp(range.end);
        pSelection[0].style.ase          = (HasSelection(state) && state.caretIndex < state.selectionAnchorIndex.value()) ? TS_AE_START : TS_AE_END;
        pSelection[0].style.fInterimChar = FALSE;
        *pcFetched                       = 1u;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetSelection(ULONG ulCount, const TS_SELECTION_ACP* pSelection) noexcept override
    {
        const Failure::Scope failureScope;
        if (! HasReadWriteLock())
        {
            return TS_E_NOLOCK;
        }
        if (ulCount == 0u)
        {
            return S_OK;
        }
        if (! pSelection)
        {
            return E_POINTER;
        }

        SharedTextInputState state{};
        if (! ReadState(state))
        {
            return TS_E_INVALIDPOS;
        }

        const AcpRange range                       = ClampAcpRange(pSelection[0].acpStart, pSelection[0].acpEnd, state.View().size(), false);
        const size_t previousCaret                 = state.caretIndex;
        const std::optional<size_t> previousAnchor = state.selectionAnchorIndex;
        const bool startIsActive                   = pSelection[0].style.ase == TS_AE_START;
        state.caretIndex                           = startIsActive ? range.start : range.end;
        state.selectionAnchorIndex                 = range.start == range.end ? std::nullopt : std::optional<size_t>(startIsActive ? range.end : range.start);
        if (! ApplyState(state, false))
        {
            return E_FAIL;
        }
        _hasSelfEditInLock = _hasSelfEditInLock || previousCaret != state.caretIndex || previousAnchor != state.selectionAnchorIndex;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetText(LONG acpStart,
                                      LONG acpEnd,
                                      WCHAR* pchPlain,
                                      ULONG cchPlainReq,
                                      ULONG* pcchPlainRet,
                                      TS_RUNINFO* prgRunInfo,
                                      ULONG cRunInfoReq,
                                      ULONG* pcRunInfoRet,
                                      LONG* pacpNext) noexcept override
    {
        const Failure::Scope failureScope;
        if (! HasReadLock())
        {
            return TS_E_NOLOCK;
        }
        if (! pcchPlainRet || ! pcRunInfoRet || ! pacpNext)
        {
            return E_POINTER;
        }

        SharedTextInputState state{};
        if (! ReadState(state))
        {
            return TS_E_INVALIDPOS;
        }

        const AcpRange range          = ClampAcpRange(acpStart, acpEnd, state.View().size());
        const std::wstring_view value = state.View().substr(range.start, range.end - range.start);
        const size_t copied           = (std::min)(value.size(), static_cast<size_t>(cchPlainReq));
        if (pchPlain && copied > 0u)
        {
            std::copy_n(value.data(), copied, pchPlain);
        }

        *pcchPlainRet = static_cast<ULONG>(copied);
        *pcRunInfoRet = 0u;
        if (prgRunInfo && cRunInfoReq > 0u && copied > 0u)
        {
            prgRunInfo[0].type   = TS_RT_PLAIN;
            prgRunInfo[0].uCount = static_cast<ULONG>(copied);
            *pcRunInfoRet        = 1u;
        }
        *pacpNext = ToAcp(range.start + copied);
        return copied == value.size() ? S_OK : S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE SetText(DWORD dwFlags, LONG acpStart, LONG acpEnd, const WCHAR* pchText, ULONG cch, TS_TEXTCHANGE* pChange) noexcept override
    {
        const Failure::Scope failureScope;
        if (! HasReadWriteLock())
        {
            return TS_E_NOLOCK;
        }

        return ReplaceTextRange(dwFlags, acpStart, acpEnd, pchText, cch, pChange);
    }

    HRESULT STDMETHODCALLTYPE GetFormattedText(LONG /*acpStart*/, LONG /*acpEnd*/, IDataObject** ppDataObject) noexcept override
    {
        const Failure::Scope failureScope;
        if (! ppDataObject)
        {
            return E_POINTER;
        }

        *ppDataObject = nullptr;
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetEmbedded(LONG /*acpPos*/, REFGUID /*rguidService*/, REFIID /*riid*/, IUnknown** ppunk) noexcept override
    {
        const Failure::Scope failureScope;
        if (! ppunk)
        {
            return E_POINTER;
        }

        *ppunk = nullptr;
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE QueryInsertEmbedded(const GUID* /*pguidService*/, const FORMATETC* /*pFormatEtc*/, BOOL* pfInsertable) noexcept override
    {
        const Failure::Scope failureScope;
        if (! pfInsertable)
        {
            return E_POINTER;
        }

        *pfInsertable = FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE
    InsertEmbedded(DWORD /*dwFlags*/, LONG /*acpStart*/, LONG /*acpEnd*/, IDataObject* /*pDataObject*/, TS_TEXTCHANGE* /*pChange*/) noexcept override
    {
        const Failure::Scope failureScope;
        return E_NOTIMPL;
    }

    // The store supports one attribute, GUID_PROP_INPUTSCOPE, whose value is this store's ITfInputScope for the whole
    // document. TS_ATTR_FIND_WANT_VALUE asks for the value; otherwise the support query reports VT_EMPTY.
    HRESULT STDMETHODCALLTYPE RequestSupportedAttrs(DWORD dwFlags, ULONG cFilterAttrs, const TS_ATTRID* paFilterAttrs) noexcept override
    {
        const Failure::Scope failureScope;
        if (cFilterAttrs > 0u && ! paFilterAttrs)
            return E_INVALIDARG;
        _inputScopeRequested      = IsInputScopeRequested(cFilterAttrs, paFilterAttrs);
        _inputScopeValueRequested = (dwFlags & TS_ATTR_FIND_WANT_VALUE) != 0u;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE RequestAttrsAtPosition(LONG /*acpPos*/, ULONG cFilterAttrs, const TS_ATTRID* paFilterAttrs, DWORD /*dwFlags*/) noexcept override
    {
        const Failure::Scope failureScope;
        if (cFilterAttrs > 0u && ! paFilterAttrs)
            return E_INVALIDARG;
        _inputScopeRequested      = IsInputScopeRequested(cFilterAttrs, paFilterAttrs);
        _inputScopeValueRequested = true;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE RequestAttrsTransitioningAtPosition(LONG /*acpPos*/,
                                                                  ULONG /*cFilterAttrs*/,
                                                                  const TS_ATTRID* /*paFilterAttrs*/,
                                                                  DWORD /*dwFlags*/) noexcept override
    {
        const Failure::Scope failureScope;
        // The input scope covers the whole document, so no attribute transitions anywhere.
        _inputScopeRequested = false;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE FindNextAttrTransition(LONG acpStart,
                                                     LONG /*acpHalt*/,
                                                     ULONG /*cFilterAttrs*/,
                                                     const TS_ATTRID* /*paFilterAttrs*/,
                                                     DWORD /*dwFlags*/,
                                                     LONG* pacpNext,
                                                     BOOL* pfFound,
                                                     LONG* plFoundOffset) noexcept override
    {
        const Failure::Scope failureScope;
        if (! pacpNext || ! pfFound || ! plFoundOffset)
        {
            return E_POINTER;
        }

        *pacpNext      = acpStart;
        *pfFound       = FALSE;
        *plFoundOffset = 0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE RetrieveRequestedAttrs(ULONG ulCount, TS_ATTRVAL* paAttrVals, ULONG* pcFetched) noexcept override
    {
        const Failure::Scope failureScope;
        if (! pcFetched)
        {
            return E_POINTER;
        }

        *pcFetched = 0u;
        if (ulCount > 0u && ! paAttrVals)
            return E_INVALIDARG;
        if (! std::exchange(_inputScopeRequested, false) || ulCount == 0u)
            return S_OK;
        TS_ATTRVAL& value = paAttrVals[0];
        value.idAttr      = kTextStoreInputScopeProperty;
        value.dwOverlapId = 0u;
        VariantInit(&value.varValue);
        if (_inputScopeValueRequested)
        {
            // The caller owns the VARIANT and releases this reference with VariantClear.
            value.varValue.vt      = VT_UNKNOWN;
            value.varValue.punkVal = static_cast<ITfInputScope*>(this);
            AddRef();
        }
        *pcFetched = 1u;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetEndACP(LONG* pacp) noexcept override
    {
        const Failure::Scope failureScope;
        if (! HasReadLock())
        {
            return TS_E_NOLOCK;
        }
        if (! pacp)
        {
            return E_POINTER;
        }

        SharedTextInputState state{};
        if (! ReadState(state))
        {
            return TS_E_INVALIDPOS;
        }

        *pacp = ToAcp(state.View().size());
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetActiveView(TsViewCookie* pvcView) noexcept override
    {
        const Failure::Scope failureScope;
        if (! pvcView)
        {
            return E_POINTER;
        }

        *pvcView = kTextStoreView;
        return S_OK;
    }

    // ITextStoreACP::GetACPFromPoint: by default the character whose box contains the point, else TS_E_INVALIDPOINT;
    // GXFPF_ROUND_NEAREST returns the nearer edge of that box and GXFPF_NEAREST the closest position outside every box.
    HRESULT STDMETHODCALLTYPE GetACPFromPoint(TsViewCookie vcView, const POINT* ptScreen, DWORD dwFlags, LONG* pacp) noexcept override
    {
        const Failure::Scope failureScope;
        if (! HasReadLock())
        {
            return TS_E_NOLOCK;
        }
        if (! ptScreen || ! pacp)
        {
            return E_POINTER;
        }
        *pacp = 0;
        if (vcView != kTextStoreView)
        {
            return E_INVALIDARG;
        }

        LONG nearestAcp  = 0;
        const HRESULT hr = _target->GetAcpFromScreenPoint(ptScreen, &nearestAcp);
        if (FAILED(hr))
            return hr;
        SharedTextInputState state{};
        if (! ReadState(state))
            return TS_E_INVALIDPOS;
        const std::wstring_view text = state.View();
        size_t nearest               = nearestAcp < 0 ? 0u : (std::min)(static_cast<size_t>(nearestAcp), text.size());
        if (! IsTextStoreCut(text, nearest))
            --nearest;

        RECT view{};
        const bool inView = _target->GetScreenRect(&view) == S_OK && PtInRect(&view, *ptScreen) != FALSE;
        const auto inBox  = [this, ptScreen](size_t start, size_t end) noexcept
        {
            RECT box{};
            BOOL clipped = FALSE;
            return start < end && _target->GetRangeScreenRect(ToAcp(start), ToAcp(end), &box, &clipped) == S_OK && PtInRect(&box, *ptScreen) != FALSE;
        };
        std::optional<AcpRange> containing;
        if (inView)
        {
            const size_t next     = NextTextStoreCut(text, nearest);
            const size_t previous = PreviousTextStoreCut(text, nearest);
            if (inBox(nearest, next))
                containing = AcpRange{nearest, next};
            else if (inBox(previous, nearest))
                containing = AcpRange{previous, nearest};
        }
        if (containing)
        {
            // The hit test's nearest position is always one edge of the box that contains the point.
            *pacp = ToAcp((dwFlags & GXFPF_ROUND_NEAREST) != 0u ? nearest : containing->start);
            return S_OK;
        }
        if ((dwFlags & GXFPF_NEAREST) != 0u)
        {
            *pacp = ToAcp(nearest);
            return S_OK;
        }
        return TS_E_INVALIDPOINT;
    }

    HRESULT STDMETHODCALLTYPE GetTextExt(TsViewCookie vcView, LONG acpStart, LONG acpEnd, RECT* prc, BOOL* pfClipped) noexcept override
    {
        const Failure::Scope failureScope;
        if (! HasReadLock())
        {
            return TS_E_NOLOCK;
        }
        if (! prc || ! pfClipped)
        {
            return E_POINTER;
        }
        if (vcView != kTextStoreView)
        {
            return E_INVALIDARG;
        }

        return _target->GetRangeScreenRect(acpStart, acpEnd, prc, pfClipped);
    }

    HRESULT STDMETHODCALLTYPE GetScreenExt(TsViewCookie vcView, RECT* prc) noexcept override
    {
        const Failure::Scope failureScope;
        if (! prc)
        {
            return E_POINTER;
        }
        if (vcView != kTextStoreView)
        {
            return E_INVALIDARG;
        }
        return _target->GetScreenRect(prc);
    }

    HRESULT STDMETHODCALLTYPE GetWnd(TsViewCookie vcView, HWND* phwnd) noexcept override
    {
        const Failure::Scope failureScope;
        if (! phwnd)
        {
            return E_POINTER;
        }
        if (vcView != kTextStoreView)
        {
            return E_INVALIDARG;
        }

        *phwnd = _target->GetHwnd();
        return *phwnd ? S_OK : TS_E_INVALIDPOS;
    }

    HRESULT STDMETHODCALLTYPE
    InsertTextAtSelection(DWORD dwFlags, const WCHAR* pchText, ULONG cch, LONG* pacpStart, LONG* pacpEnd, TS_TEXTCHANGE* pChange) noexcept override
    {
        const Failure::Scope failureScope;
        if (pacpStart)
            *pacpStart = 0;
        if (pacpEnd)
            *pacpEnd = 0;
        if (pChange)
            *pChange = {};
        if (! HasReadWriteLock())
            return TS_E_NOLOCK;
        if ((dwFlags & ~(TS_IAS_NOQUERY | TS_IAS_QUERYONLY)) || (dwFlags & TS_IAS_NOQUERY && dwFlags & TS_IAS_QUERYONLY))
            return E_INVALIDARG;
        const bool queryOnly = (dwFlags & TS_IAS_QUERYONLY) != 0;
        if (! (dwFlags & TS_IAS_NOQUERY) && (! pacpStart || ! pacpEnd))
            return E_POINTER;
        if (! queryOnly && ! pChange)
            return E_POINTER;
        if (cch && ! pchText)
            return E_INVALIDARG;
        SharedTextInputState state{};
        if (! ReadState(state))
            return TS_E_INVALIDPOS;
        if (state.readOnly)
            return TS_E_READONLY;
        const AcpRange range = GetSelectionRange(state);
        // One edit inserts at most 65,536 UTF-16 units. The document is as long as its field allows: a long multiline
        // field takes IME, emoji and dictation edits like a short one.
        if (! ReplacementFitsCapacity(state.View().size(), range, cch))
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        if (! queryOnly)
        {
            const HRESULT hr = ReplaceTextRange(0, ToAcp(range.start), ToAcp(range.end), pchText, cch, pChange);
            if (FAILED(hr))
                return hr;
        }
        if (pacpStart)
            *pacpStart = ToAcp(range.start);
        if (pacpEnd)
            *pacpEnd = ToAcp(range.start + cch);
        if (pChange)
            *pChange = {ToAcp(range.start), ToAcp(range.end), ToAcp(range.start + cch)};
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE
    InsertEmbeddedAtSelection(DWORD /*dwFlags*/, IDataObject* /*pDataObject*/, LONG* pacpStart, LONG* pacpEnd, TS_TEXTCHANGE* /*pChange*/) noexcept override
    {
        const Failure::Scope failureScope;
        if (! pacpStart || ! pacpEnd)
        {
            return E_POINTER;
        }

        *pacpStart = 0;
        *pacpEnd   = 0;
        return E_NOTIMPL;
    }

private:
    [[nodiscard]] bool HasReadLock() const noexcept
    {
        return (_lockFlags & TS_LF_READ) != 0u;
    }

    [[nodiscard]] bool HasReadWriteLock() const noexcept
    {
        return (_lockFlags & TS_LF_READWRITE) == TS_LF_READWRITE;
    }

    // Reads share the target's document; a successful read always carries text.
    [[nodiscard]] bool ReadState(SharedTextInputState& state) const noexcept
    {
        if (! _target->ReadSharedState(state) || state.masked || ! state.text)
        {
            state = {};
            return false;
        }
        return true;
    }
    [[nodiscard]] bool ApplyState(const SharedTextInputState& state, bool notifyChange) noexcept
    {
        return _target->ApplySharedState(state, notifyChange);
    }

    [[nodiscard]] bool ReplacementFitsCapacity(size_t textLength, const AcpRange& range, size_t insertedUnits) const noexcept
    {
        if (insertedUnits > kMaximumTextStoreEditUnits)
            return false;
        const size_t retainedUnits = textLength - (range.end - range.start);
        const size_t maximumUnits  = _target->GetMaximumDocumentTextUnits();
        return retainedUnits <= maximumUnits && insertedUnits <= maximumUnits - retainedUnits;
    }

    HRESULT ReplaceTextRange(DWORD dwFlags, LONG acpStart, LONG acpEnd, const WCHAR* pchText, ULONG cch, TS_TEXTCHANGE* pChange) noexcept
    {
        if (pChange)
            *pChange = {};
        SharedTextInputState state{};
        if (! ReadState(state))
        {
            return TS_E_INVALIDPOS;
        }
        if (state.readOnly)
        {
            return TS_E_READONLY;
        }
        if (cch > 0u && ! pchText)
        {
            return E_POINTER;
        }

        const AcpRange range = ClampAcpRange(acpStart, acpEnd, state.View().size());
        if (! ReplacementFitsCapacity(state.View().size(), range, cch))
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        if ((dwFlags & TS_IAS_QUERYONLY) != 0u)
        {
            if (pChange)
            {
                pChange->acpStart  = ToAcp(range.start);
                pChange->acpOldEnd = ToAcp(range.end);
                pChange->acpNewEnd = ToAcp(range.start + cch);
            }
            return S_OK;
        }

        const std::wstring_view replacement(pchText ? pchText : L"", static_cast<size_t>(cch));
        const size_t newCaret           = range.start + replacement.size();
        const std::wstring_view current = state.View();
        const bool selfEditChanged =
            current.substr(range.start, range.end - range.start) != replacement || state.caretIndex != newCaret || state.selectionAnchorIndex.has_value();
        try
        {
            // The edit is the one document-sized allocation of the lock: the new text is built once, at its final size, and
            // every later read, stage and import in the lock shares it. `current` stays valid: `state` pins its buffer
            // until the assignment below, after which it is no longer read.
            std::wstring next;
            next.reserve(current.size() - (range.end - range.start) + replacement.size());
            next.append(current.substr(0u, range.start)).append(replacement).append(current.substr(range.end));
            state.text = MakeSharedText(std::move(next));
            // The new text is no longer the revision the read carried (SharedTextInputState invariant).
            state.source.reset();
        }
        catch (const std::bad_alloc&)
        {
            Failure::FailAllocation();
        }
        state.caretIndex = newCaret;
        state.selectionAnchorIndex.reset();
        if (! ApplyState(state, true))
        {
            return E_FAIL;
        }

        TS_TEXTCHANGE change{};
        change.acpStart  = ToAcp(range.start);
        change.acpOldEnd = ToAcp(range.end);
        change.acpNewEnd = ToAcp(state.caretIndex);
        if (pChange)
        {
            *pChange = change;
        }

        _hasSelfEditInLock = _hasSelfEditInLock || selfEditChanged;
        return S_OK;
    }

    void CaptureObservedState() noexcept
    {
        // The observed state serves only change notifications, so a rich editor's revision is kept without materializing it.
        SharedTextInputState state{};
        if (! _target->ReadSharedStateForNotification(state) || state.masked || ! state.HasDocument())
        {
            _hasObservedState = false;
            return;
        }

        _observedState               = std::move(state);
        _observedViewport            = ResolveTextViewportBounds();
        _hasObservedState            = true;
        _pendingExternalNotification = false;
    }

public:
    bool NotifyExternalChangesIfNeeded() noexcept
    {
        if (_lockFlags != 0u)
        {
            _pendingExternalNotification = true;
            if (_target)
                static_cast<void>(_target->ScheduleLock());
            return false;
        }

        wil::com_ptr_nothrow<ITextStoreACPSink> sink  = _sink;
        const DWORD sinkMask                          = _sinkMask;
        const uint64_t sinkConnectionRevision         = _sinkConnectionRevision;
        const std::shared_ptr<TextStoreTarget> target = _target;
        if (! sink || ! target)
            return false;

        // A rich editor's revision is compared and its changed range found in place: a notification never materializes it.
        SharedTextInputState currentState{};
        if (! target->ReadSharedStateForNotification(currentState) || currentState.masked || ! currentState.HasDocument())
        {
            _hasObservedState = false;
            return false;
        }

        const D2D1_RECT_F currentViewport = target->ResolveTextViewportBounds();
        if (_target != target || _sink.get() != sink.get() || _sinkMask != sinkMask || _sinkConnectionRevision != sinkConnectionRevision || _lockFlags != 0u)
            return false;
        if (! _hasObservedState)
        {
            _observedState    = std::move(currentState);
            _observedViewport = currentViewport;
            _hasObservedState = true;
            return false;
        }

        const bool textChanged      = ! SameTextInputContent(currentState, _observedState);
        const bool selectionChanged = ! IsSameSelection(currentState, _observedState);
        const bool readOnlyChanged  = currentState.readOnly != _observedState.readOnly;
        const bool layoutChanged    = textChanged || selectionChanged || currentState.firstVisibleLine != _observedState.firstVisibleLine ||
                                      currentState.masked != _observedState.masked || currentState.multiline != _observedState.multiline ||
                                      ! IsSameRect(currentViewport, _observedViewport);

        // Only the changed middle re-anchors TIP ranges; the common prefix and suffix keep theirs.
        const TS_TEXTCHANGE textChange = textChanged ? ComputeTextStoreTextChange(_observedState, currentState) : TS_TEXTCHANGE{};

        // Shares the document: the notification state pins the buffer it compares against for the whole sequence, even
        // when a sink callback re-enters and replaces the control text or the observed state.
        const SharedTextInputState notificationState = currentState;
        _observedState                               = std::move(currentState);
        _observedViewport                            = currentViewport;
        _pendingExternalNotification                 = false;

        const auto continuationIsCurrent = [this, sink, sinkMask, sinkConnectionRevision, target, &notificationState, currentViewport]() noexcept
        {
            if (_target != target || _sink.get() != sink.get() || _sinkMask != sinkMask || _sinkConnectionRevision != sinkConnectionRevision ||
                _lockFlags != 0u || ! _hasObservedState || ! SameTextInputContent(_observedState, notificationState) ||
                ! IsSameSelection(_observedState, notificationState) || _observedState.firstVisibleLine != notificationState.firstVisibleLine ||
                _observedState.masked != notificationState.masked || _observedState.multiline != notificationState.multiline ||
                _observedState.readOnly != notificationState.readOnly || ! IsSameRect(_observedViewport, currentViewport))
                return false;

            SharedTextInputState latestState{};
            return target->ReadSharedStateForNotification(latestState) && ! latestState.masked && SameTextInputContent(latestState, notificationState) &&
                   IsSameSelection(latestState, notificationState) && latestState.firstVisibleLine == notificationState.firstVisibleLine &&
                   latestState.masked == notificationState.masked && latestState.multiline == notificationState.multiline &&
                   latestState.readOnly == notificationState.readOnly && IsSameRect(target->ResolveTextViewportBounds(), currentViewport);
        };

        if (readOnlyChanged && (sinkMask & TS_AS_STATUS_CHANGE) != 0u)
        {
            static_cast<void>(sink->OnStatusChange(notificationState.readOnly ? TS_SD_READONLY : 0u));
            if (! continuationIsCurrent())
                return false;
        }
        if (textChanged && (sinkMask & TS_AS_TEXT_CHANGE) != 0u)
        {
#if DXUI_ENABLE_DIAGNOSTICS
            ++_debugTextChangeNotifications;
#endif
            static_cast<void>(sink->OnTextChange(0u, &textChange));
            if (! continuationIsCurrent())
                return false;
        }
        if (selectionChanged && (sinkMask & TS_AS_SEL_CHANGE) != 0u)
        {
#if DXUI_ENABLE_DIAGNOSTICS
            ++_debugSelectionChangeNotifications;
#endif
            static_cast<void>(sink->OnSelectionChange());
            if (! continuationIsCurrent())
                return false;
        }
        const bool notifyLayout = layoutChanged && (sinkMask & TS_AS_LAYOUT_CHANGE) != 0u;
        if (notifyLayout)
        {
#if DXUI_ENABLE_DIAGNOSTICS
            ++_debugLayoutChangeNotifications;
#endif
            static_cast<void>(sink->OnLayoutChange(TS_LC_CHANGE, kTextStoreView));
        }
        return notifyLayout;
    }

public:
    void NotifyPreparedLayout() noexcept
    {
        if (_lockFlags != 0u)
        {
            _pendingLayoutNotification = true;
            if (_target)
                static_cast<void>(_target->ScheduleLock());
            return;
        }
        NotifyLayoutChanged();
    }

private:
    void ReleaseLockAndSchedulePending() noexcept
    {
        _lockFlags = 0u;
        if (_pendingLockFlags != 0u || _pendingExternalNotification || _pendingLayoutNotification)
        {
            // On failure, keep the already-accepted request coalesced for the next real RequestLock call to retry.
            // ScheduleLock is a single PostMessage attempt; it never pumps or waits for the UI queue.
            static_cast<void>(_target->ScheduleLock());
        }
    }

    void NotifyLayoutChanged() noexcept
    {
        wil::com_ptr_nothrow<ITextStoreACPSink> sink = _sink;
        const DWORD sinkMask                         = _sinkMask;
        if (sink && _lockFlags == 0u && (sinkMask & TS_AS_LAYOUT_CHANGE) != 0u)
        {
#if DXUI_ENABLE_DIAGNOSTICS
            ++_debugLayoutChangeNotifications;
#endif
            static_cast<void>(sink->OnLayoutChange(TS_LC_CHANGE, kTextStoreView));
        }
    }

    [[nodiscard]] D2D1_RECT_F ResolveTextViewportBounds() const noexcept
    {
        return _target->ResolveTextViewportBounds();
    }

    [[nodiscard]] bool EnsureDisplayAttributeServices() noexcept
    {
        // Created once per store on its first composition and reused for every later edit session.
        if (! _categoryMgr)
            static_cast<void>(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(_categoryMgr.put())));
        if (! _displayAttributeMgr)
            static_cast<void>(CoCreateInstance(CLSID_TF_DisplayAttributeMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(_displayAttributeMgr.put())));
        return _categoryMgr && _displayAttributeMgr;
    }

    [[nodiscard]] std::optional<TF_DA_ATTR_INFO> ReadDisplayAttribute(ITfProperty& property, TfEditCookie readCookie, ITfRange& range) noexcept
    {
        VARIANT value;
        VariantInit(&value);
        if (FAILED(property.GetValue(readCookie, &range, &value)))
            return std::nullopt;
        const bool isAtom = value.vt == VT_I4;
        const auto atom   = static_cast<TfGuidAtom>(isAtom ? value.lVal : 0);
        static_cast<void>(VariantClear(&value));
        GUID guid{};
        if (! isAtom || FAILED(_categoryMgr->GetGUID(atom, &guid)))
            return std::nullopt;
        wil::com_ptr_nothrow<ITfDisplayAttributeInfo> info;
        TF_DISPLAYATTRIBUTE attribute{};
        if (FAILED(_displayAttributeMgr->GetDisplayAttributeInfo(guid, info.put(), nullptr)) || ! info || FAILED(info->GetAttributeInfo(&attribute)))
            return std::nullopt;
        return attribute.bAttr;
    }

    // Reads the first composition's range and its GUID_PROP_ATTRIBUTE spans. False when no composition is active.
    [[nodiscard]] bool ReadCompositionAttributeSpans(ITfContext& context, TfEditCookie readCookie)
    {
        _attributeSpans.reserve(kMaximumTextStoreClauseBoundaries);
        _compositionAttributes.clauseBoundaries.reserve(kMaximumTextStoreClauseBoundaries);
        _attributeSpans.clear();
        wil::com_ptr_nothrow<ITfContextComposition> compositions;
        wil::com_ptr_nothrow<IEnumITfCompositionView> views;
        if (FAILED(context.QueryInterface(IID_PPV_ARGS(compositions.put()))) || ! compositions || FAILED(compositions->EnumCompositions(views.put())) ||
            ! views)
            return false;
        wil::com_ptr_nothrow<ITfCompositionView> view;
        ULONG fetched = 0u;
        wil::com_ptr_nothrow<ITfRange> compositionRange;
        if (views->Next(1u, view.put(), &fetched) != S_OK || fetched != 1u || ! view || FAILED(view->GetRange(compositionRange.put())) || ! compositionRange ||
            ! TryGetCompositionAcpRange(view.get(), compositionRange.get(), _attributeCompositionStart, _attributeCompositionEnd))
            return false;

        wil::com_ptr_nothrow<ITfProperty> property;
        wil::com_ptr_nothrow<IEnumTfRanges> ranges;
        if (! EnsureDisplayAttributeServices() || FAILED(context.GetProperty(GUID_PROP_ATTRIBUTE, property.put())) || ! property ||
            FAILED(property->EnumRanges(readCookie, ranges.put(), compositionRange.get())) || ! ranges)
            return true; // A composition without readable attributes publishes no target or clauses.
        while (_attributeSpans.size() < kMaximumTextStoreClauseBoundaries)
        {
            wil::com_ptr_nothrow<ITfRange> range;
            ULONG count = 0u;
            if (ranges->Next(1u, range.put(), &count) != S_OK || count != 1u || ! range)
                break;
            size_t start                                   = 0u;
            size_t end                                     = 0u;
            const std::optional<TF_DA_ATTR_INFO> attribute = ReadDisplayAttribute(*property, readCookie, *range);
            if (attribute && TryGetCompositionAcpRange(nullptr, range.get(), start, end))
                _attributeSpans.push_back(TextStoreAttributeSpan{start, end, attribute.value()});
        }
        return true;
    }

    std::atomic<ULONG> _referenceCount{1u};
    std::shared_ptr<TextStoreTarget> _target;
    DWORD _pendingLockFlags = 0;
    DWORD _lockFlags        = 0u;
    DWORD _sinkMask         = 0u;
    wil::com_ptr_nothrow<ITextStoreACPSink> _sink;
    uint64_t _sinkConnectionRevision = 0u;
    SharedTextInputState _observedState;
    D2D1_RECT_F _observedViewport     = D2D1::RectF();
    bool _hasObservedState            = false;
    bool _pendingExternalNotification = false;
    bool _pendingLayoutNotification   = false;
    bool _hasSelfEditInLock           = false;
    bool _inputScopeRequested         = false;
    bool _inputScopeValueRequested    = false;
    DWORD _textEditSinkCookie         = TF_INVALID_COOKIE;
    wil::com_ptr_nothrow<ITfCategoryMgr> _categoryMgr;
    wil::com_ptr_nothrow<ITfDisplayAttributeMgr> _displayAttributeMgr;
    std::vector<TextStoreAttributeSpan> _attributeSpans;
    TextStoreCompositionAttributes _compositionAttributes;
    size_t _attributeCompositionStart = 0u;
    size_t _attributeCompositionEnd   = 0u;
#if DXUI_ENABLE_DIAGNOSTICS
    uint64_t _debugTextChangeNotifications      = 0u;
    uint64_t _debugSelectionChangeNotifications = 0u;
    uint64_t _debugLayoutChangeNotifications    = 0u;
#endif
};
} // namespace

namespace
{
// The common prefix and suffix rule over two texts read one unit at a time. equalPrefix and equalSuffix are lengths already
// known to be equal in both (0 when unknown); the scan starts past them, so the result is the whole-text rule's.
template <typename BeforeAt, typename AfterAt>
[[nodiscard]] TS_TEXTCHANGE ComputeTextChangeByUnits(
    size_t beforeSize, size_t afterSize, const BeforeAt& before, const AfterAt& after, size_t equalPrefix, size_t equalSuffix) noexcept
{
    const auto isCut = [](const auto& unitAt, size_t size, size_t position) noexcept
    {
        if (position == 0u || position >= size)
            return true;
        const wchar_t previous = unitAt(position - 1u);
        const wchar_t next     = unitAt(position);
        if (IsUtf16LeadSurrogate(previous) && IsUtf16TrailSurrogate(next))
            return false;
        return ! (previous == L'\r' && next == L'\n');
    };
    const size_t limit = (std::min)(beforeSize, afterSize);
    size_t prefix      = (std::min)(equalPrefix, limit);
    while (prefix < limit && before(prefix) == after(prefix))
        ++prefix;
    while (prefix > 0u && (! isCut(before, beforeSize, prefix) || ! isCut(after, afterSize, prefix)))
        --prefix;
    size_t suffix = (std::min)(equalSuffix, limit - prefix);
    while (suffix < limit - prefix && before(beforeSize - 1u - suffix) == after(afterSize - 1u - suffix))
        ++suffix;
    while (suffix > 0u && (! isCut(before, beforeSize, beforeSize - suffix) || ! isCut(after, afterSize, afterSize - suffix)))
        --suffix;
    return TS_TEXTCHANGE{ToAcp(prefix), ToAcp(beforeSize - suffix), ToAcp(afterSize - suffix)};
}
} // namespace

TS_TEXTCHANGE ComputeTextStoreTextChange(std::wstring_view before, std::wstring_view after) noexcept
{
    return ComputeTextChangeByUnits(
        before.size(), after.size(), [before](size_t at) noexcept { return before[at]; }, [after](size_t at) noexcept { return after[at]; }, 0u, 0u);
}

TS_TEXTCHANGE ComputeTextStoreTextChange(const SharedTextInputState& before, const SharedTextInputState& after) noexcept
{
    if (before.text && after.text)
        return ComputeTextStoreTextChange(before.View(), after.View());
    // At least one side is an unmaterialized rich editor revision: read units in place. Two revisions of one document are
    // equal outside their tracked changed range, so the scans start at that range.
    size_t equalPrefix = 0u;
    size_t equalSuffix = 0u;
    if (before.source && after.source)
    {
        const auto range = after.source->ChangedRangeFrom(*before.source);
        if (range.kind == RichText::SourceChangeKind::Unchanged)
        {
            equalPrefix = before.source->Length();
            equalSuffix = before.source->Length();
        }
        else if (range.kind == RichText::SourceChangeKind::TrackedEdits)
        {
            equalPrefix = range.begin;
            equalSuffix = before.source->Length() - range.previousEnd;
        }
    }
    const auto unitReader = [](const SharedTextInputState& state)
    { return [&state](size_t at) noexcept { return state.text ? (*state.text)[at] : state.source->UnitAt(at); }; };
    return ComputeTextChangeByUnits(before.Length(), after.Length(), unitReader(before), unitReader(after), equalPrefix, equalSuffix);
}

void ResolveTextStoreCompositionAttributes(size_t compositionStart,
                                           size_t compositionEnd,
                                           std::span<const TextStoreAttributeSpan> spans,
                                           TextStoreCompositionAttributes& result)
{
    result.targetStart.reset();
    result.targetEnd.reset();
    result.clauseBoundaries.clear();
    if (compositionEnd <= compositionStart)
        return;

    std::vector<size_t>& boundaries = result.clauseBoundaries;
    const auto addBoundary          = [&boundaries](size_t boundary)
    {
        // The last slot is kept for the composition end.
        if (boundaries.size() < kMaximumTextStoreClauseBoundaries - 1u && boundary > boundaries.back())
            boundaries.push_back(boundary);
    };
    size_t previousEnd = compositionStart;
    bool targetClosed  = false;
    for (const TextStoreAttributeSpan& span : spans)
    {
        const size_t start = (std::max)(span.start, compositionStart);
        const size_t end   = (std::min)(span.end, compositionEnd);
        if (end <= start || start < previousEnd)
            continue;
        if (boundaries.empty())
            boundaries.push_back(compositionStart);
        addBoundary(start);
        addBoundary(end);
        previousEnd = end;

        const bool isTarget = span.attribute == TF_ATTR_TARGET_CONVERTED || span.attribute == TF_ATTR_TARGET_NOTCONVERTED;
        if (targetClosed)
            continue;
        if (isTarget && ! result.targetStart)
        {
            result.targetStart = start;
            result.targetEnd   = end;
        }
        else if (isTarget && result.targetEnd == start)
            result.targetEnd = end;
        else if (result.targetStart)
            targetClosed = true;
    }
    if (! boundaries.empty() && boundaries.back() < compositionEnd)
        boundaries.push_back(compositionEnd);
}

HRESULT AdviseTextStoreEditSink(ITfContext* context, ITextStoreACP* store) noexcept
{
    auto* const concrete = dynamic_cast<TextStoreACP*>(store);
    wil::com_ptr_nothrow<ITfSource> source;
    if (! context || ! concrete)
        return E_INVALIDARG;
    HRESULT hr = context->QueryInterface(IID_PPV_ARGS(source.put()));
    if (FAILED(hr))
        return hr;
    DWORD cookie = TF_INVALID_COOKIE;
    hr           = source->AdviseSink(__uuidof(ITfTextEditSink), static_cast<ITfTextEditSink*>(concrete), &cookie);
    if (SUCCEEDED(hr))
        concrete->SetTextEditSinkCookie(cookie);
    return hr;
}

void UnadviseTextStoreEditSink(ITfContext* context, IUnknown* store) noexcept
{
    wil::com_ptr_nothrow<ITextStoreACP> textStore;
    if (! context || ! store || FAILED(store->QueryInterface(IID_PPV_ARGS(textStore.put()))) || ! textStore)
        return;
    auto* const concrete = dynamic_cast<TextStoreACP*>(textStore.get());
    const DWORD cookie   = concrete ? concrete->TakeTextEditSinkCookie() : TF_INVALID_COOKIE;
    wil::com_ptr_nothrow<ITfSource> source;
    if (cookie != TF_INVALID_COOKIE && SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(source.put()))))
        static_cast<void>(source->UnadviseSink(cookie));
}

ITextStoreACP* CreateTextStore(std::shared_ptr<TextStoreTarget> target) noexcept
{
    if (! target)
        return nullptr;
    auto* store = new (std::nothrow) TextStoreACP(std::move(target));
    return store ? static_cast<ITextStoreACP*>(store) : nullptr;
}

void NotifyTextStoreLayoutChanged(ITextStoreACP* store) noexcept
{
    // A sink callback can release the application's last reference.
    wil::com_ptr_nothrow<ITextStoreACP> lifetime = store;
    if (auto* concrete = dynamic_cast<TextStoreACP*>(store))
        concrete->NotifyPreparedLayout();
}
HRESULT DispatchPendingTextStoreLock(ITextStoreACP* store) noexcept
{
    // A sink callback can release the application's last reference.
    wil::com_ptr_nothrow<ITextStoreACP> lifetime = store;
    auto* concrete                               = dynamic_cast<TextStoreACP*>(store);
    if (! concrete)
        return E_INVALIDARG;
    concrete->DispatchPendingExternalChanges();
    return concrete->DispatchPendingLock();
}

void NotifyTextStoreChanged(ITextStoreACP* store) noexcept
{
    // A sink callback can release the application's last reference.
    wil::com_ptr_nothrow<ITextStoreACP> lifetime = store;
    if (auto* concrete = dynamic_cast<TextStoreACP*>(store))
        concrete->NotifyExternalChangesIfNeeded();
}

void ScheduleTextStoreReconciliation(ITextStoreACP* store) noexcept
{
    // The host posts one generation-tagged turn after activation; no sink callback runs on the TSF stack.
    wil::com_ptr_nothrow<ITextStoreACP> lifetime = store;
    if (auto* concrete = dynamic_cast<TextStoreACP*>(store))
        concrete->ScheduleReconciliation();
}

#if DXUI_ENABLE_DIAGNOSTICS
void DebugGetTextStoreNotificationCountsForTest(ITextStoreACP* store, uint64_t& textChanges, uint64_t& selectionChanges, uint64_t& layoutChanges) noexcept
{
    textChanges = selectionChanges = layoutChanges = 0u;
    auto* concrete                                 = dynamic_cast<TextStoreACP*>(store);
    if (concrete)
        concrete->DebugGetNotificationCounts(textChanges, selectionChanges, layoutChanges);
}
#endif

ITextStoreACP* CreateNativeTextInputTextStore(ControlHost& host, Control& control, bool hostDeferredWorkEligible) noexcept
{
    try
    {
        return CreateTextStore(std::make_shared<NativeTextStoreTarget>(host, control, hostDeferredWorkEligible));
    }
    catch (const std::bad_alloc&)
    {
        Failure::FailAllocation();
    }
}

void DetachNativeTextInputTextStore(IUnknown* store) noexcept
{
    if (! store)
    {
        return;
    }

    wil::com_ptr_nothrow<ITextStoreACP> textStore;
    if (FAILED(store->QueryInterface(IID_PPV_ARGS(textStore.put()))) || ! textStore)
    {
        return;
    }

    auto* concreteStore = static_cast<TextStoreACP*>(textStore.get());
    concreteStore->DetachHost();
}

void DisconnectNativeTextInputTextStore(IUnknown* textStore) noexcept
{
    auto* store = dynamic_cast<TextStoreACP*>(textStore);
    if (store)
    {
        store->Disconnect();
    }
}

#if DXUI_ENABLE_DIAGNOSTICS
ITextStoreACP* ControlHost::DebugCreateNativeTextInputTextStoreForTest() noexcept
{
    if (_textInputBackend != TextInputBackend::Native || ! _nativeTextInputControl || ! _nativeTextInputStateCacheValid)
    {
        return nullptr;
    }

    return CreateNativeTextInputTextStore(*this, *_nativeTextInputControl);
}
#endif
} // namespace DxUi
