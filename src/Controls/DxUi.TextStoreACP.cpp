#include "DxUi.Internal.h"
#include "TextStoreTarget.h"

#include "../Support/Diagnostics.h"

#include <algorithm>
#include <atomic>
#include <exception>
#include <limits>

#include <msctf.h>
#include <textstor.h>

namespace DxUi
{
namespace
{
constexpr TsViewCookie kTextStoreView = 1u;

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

[[nodiscard]] bool HasSelection(const TextInputState& state) noexcept
{
    return state.selectionAnchorIndex.has_value() && state.selectionAnchorIndex.value() != state.caretIndex;
}

[[nodiscard]] AcpRange GetSelectionRange(const TextInputState& state) noexcept
{
    if (! HasSelection(state))
    {
        const size_t caretIndex = (std::min)(state.caretIndex, state.text.size());
        return AcpRange{caretIndex, caretIndex};
    }

    return ClampAcpRange(ToAcp(state.selectionAnchorIndex.value()), ToAcp(state.caretIndex), state.text.size(), false);
}

[[nodiscard]] bool IsSameSelection(const TextInputState& left, const TextInputState& right) noexcept
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
    const ControlHost& host, const Control& control, const AcpRange& range, const D2D1_RECT_F& bounds, bool& clipped) noexcept
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

[[nodiscard]] bool TextStoreControlBelongsToTree(const Control* root, const Control* target) noexcept
{
    if (! root || ! target)
    {
        return false;
    }
    if (root == target)
    {
        return true;
    }
    const auto* panel = dynamic_cast<const Panel*>(root);
    if (! panel)
    {
        return false;
    }
    for (const auto& child : panel->GetChildren())
    {
        if (child && TextStoreControlBelongsToTree(child.get(), target))
        {
            return true;
        }
    }
    return false;
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

class NativeTextStoreTarget final : public TextStoreTarget
{
public:
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
        if (_compositionBase && host && control && ! lifetime.expired() && TextStoreControlBelongsToTree(host->GetRoot(), control))
        {
            // Deactivation invalidates the dispatch cookie before Pop. Allow the retiring store to
            // roll back its own still-visible preview only when no replacement TSF session won reentry.
            if (CanRestoreOwnedComposition(control) && IsCompositionTextCurrent())
            {
                static_cast<void>(RestoreCompositionBase(_compositionBase.value()));
                if (! lifetime.expired() && CanRestoreOwnedComposition(control))
                    static_cast<void>(host->SetNativeTextInputCompositionRange(control, {}, {}, {}));
            }
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
        TextInputState state{};
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
            TextInputState base = std::move(_compositionBase.value());
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
                TextInputState base = std::move(_compositionBase.value());
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
                TextInputState current{};
                applied = ReadBackingState(current) && FinishComposition(current);
            }
            _editBase.reset();
            return applied ? S_OK : E_FAIL;
        }

        TextInputState staged = std::move(_stagedState.value());
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
                applied = RememberCompositionText(staged.text);
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
            TextInputState completedState{};
            if (! ReadState(completedState) || ! FinishComposition(completedState) || ! IsCurrentTextServiceTarget())
                return E_FAIL;
            sequentialComposition = true;
        }
        if (_compositionBase)
        {
            // A second live composition cannot borrow the first one's prestate or
            // silently replace its range.
            return S_OK;
        }

        try
        {
            TextInputState base{};
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
            if (! RememberCompositionText(_compositionBase->text))
            {
                _compositionBase.reset();
                return E_OUTOFMEMORY;
            }
        }
        catch (const std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }

        // TSF can insert into the staged document and then start its composition
        // in the same lock. The undo base is the lock-entry prestate above; range
        // validation must use the current staged document instead.
        TextInputState current{};
        if (! ReadState(current))
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
        if (TryGetCompositionAcpRange(composition, nullptr, start, end) && end <= current.text.size())
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
        TextInputState state{};
        if (! ReadState(state))
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
        if (end > state.text.size())
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
            TextInputState base = std::move(_compositionBase.value());
            _compositionBase.reset();
            _compositionEndPending = false;
            const bool restored    = RestoreCompositionBase(base);
            _compositionExpectedText.reset();
            return restored ? S_OK : E_FAIL;
        }

        TextInputState finalState{};
        if (! ReadBackingState(finalState))
        {
            _compositionBase.reset();
            _compositionExpectedText.reset();
            _compositionEndPending = false;
            return TS_E_INVALIDPOS;
        }
        if (finalState.masked || finalState.readOnly)
        {
            TextInputState base = std::move(_compositionBase.value());
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
        TextInputState state{};
        Control* const control = GetLiveControl();
        if (! ReadState(state) || ! control)
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
        if (! PointInRect(bounds, D2D1::Point2F(pointDip->x, pointDip->y)))
        {
            return TS_E_INVALIDPOS;
        }

        const D2D1_POINT_2F queryPoint = D2D1::Point2F(pointDip->x, pointDip->y);
        if (const std::optional<size_t> hitIndex = control->TryHitTestTextInputPoint(*_host, queryPoint); hitIndex.has_value())
        {
            *pacp = ToAcp((std::min)(hitIndex.value(), state.text.size()));
            return S_OK;
        }

        const DWRITE_READING_DIRECTION readingDirection = ResolveReadingDirection(control->GetFlowDirection());
        *pacp = ToAcp(HitTestCaretIndexDip(_host, state.text, FontRole::Body, bounds, 0.0f, queryPoint, readingDirection));
        return S_OK;
    }
    HRESULT GetRangeScreenRect(LONG acpStart, LONG acpEnd, RECT* prc, BOOL* pfClipped) const noexcept override
    {
        TextInputState state{};
        Control* const control = GetLiveControl();
        if (! ReadState(state) || ! control)
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
        const AcpRange range                     = ClampAcpRange(acpStart, acpEnd, state.text.size(), false);
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
    [[nodiscard]] Control* GetLiveControl() const noexcept
    {
        if (! _host || ! _control || _controlLifetime.expired() || ! TextStoreControlBelongsToTree(_host->GetRoot(), _control))
        {
            return nullptr;
        }
        return _control;
    }

    [[nodiscard]] bool ReadState(TextInputState& outState) const noexcept override
    {
        Control* const control = GetLiveControl();
        if (! control || ! IsCurrentTextServiceTarget() || (dynamic_cast<const TextField*>(control) && static_cast<const TextField*>(control)->IsMasked()))
        {
            outState = {};
            return false;
        }
        if (_stagedState)
        {
            try
            {
                outState = _stagedState.value();
                return ! outState.masked;
            }
            catch (const std::bad_alloc&)
            {
                outState = {};
                return false;
            }
        }
        return ReadBackingState(outState) && ! outState.masked;
    }

    [[nodiscard]] bool ApplyState(const TextInputState& state, bool notifyChange) noexcept override
    {
        if (state.masked || state.readOnly || ! IsCurrentTextServiceTarget())
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
                return RememberCompositionText(state.text);
            return applied;
        }
        try
        {
            TextInputState current{};
            if (! ReadBackingState(current) || current.masked || current.readOnly)
                return false;
            if (! _editBase)
            {
                TextInputState base{};
                if (! ReadBackingState(base) || base.masked || base.readOnly)
                {
                    return false;
                }
                _editBase = std::move(base);
            }
            _stagedState = state;
            return true;
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }
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

    [[nodiscard]] bool RememberCompositionText(std::wstring_view text) noexcept
    {
        try
        {
            _compositionExpectedText = std::wstring(text);
            return true;
        }
        catch (const std::bad_alloc&)
        {
            // Without an exact ownership marker, cancellation must preserve current text.
            _compositionExpectedText.reset();
            return false;
        }
    }

    [[nodiscard]] bool IsCompositionTextCurrent() const noexcept
    {
        if (! _compositionBase || ! _compositionExpectedText)
            return false;
        TextInputState current{};
        return ReadBackingState(current) && current.text == _compositionExpectedText.value();
    }

    void ClearCompositionRange() noexcept
    {
        Control* const control = GetLiveControl();
        if (_host && control && IsCurrentTextServiceTarget())
            static_cast<void>(_host->SetNativeTextInputCompositionRange(control, {}, {}, {}));
    }

    [[nodiscard]] bool ReadBackingState(TextInputState& outState) const noexcept
    {
        try
        {
            Control* const control = GetLiveControl();
            if (! control)
            {
                return false;
            }

            NativeTextInputState nativeState{};
            if (_host->TryReadNativeTextInputState(control, nativeState))
            {
                outState.text                 = nativeState.text;
                outState.selectionAnchorIndex = nativeState.selectionAnchorIndex;
                outState.caretIndex           = nativeState.caretIndex;
                outState.firstVisibleLine     = nativeState.firstVisibleLine;
                outState.readOnly             = nativeState.readOnly;
                outState.masked               = nativeState.masked;
                outState.multiline            = nativeState.multiline;
                if (const auto* textField = dynamic_cast<const TextField*>(control))
                {
                    outState.readOnly = textField->IsReadOnly();
                    outState.masked   = textField->IsMasked();
                }
                return true;
            }

            if (const auto* textField = dynamic_cast<const TextField*>(control))
            {
                outState.text       = textField->GetText();
                outState.readOnly   = textField->IsReadOnly();
                outState.masked     = textField->IsMasked();
                outState.caretIndex = outState.text.size();
                if (const std::optional<std::pair<size_t, size_t>> selection = textField->GetSelectionRange())
                {
                    outState.selectionAnchorIndex = selection->first;
                    outState.caretIndex           = selection->second;
                }
                return true;
            }

            if (const auto* comboBox = dynamic_cast<const ComboBox*>(control); comboBox && comboBox->IsEditable())
            {
                outState.text       = comboBox->GetText();
                outState.caretIndex = outState.text.size();
                if (const std::optional<std::pair<size_t, size_t>> selection = comboBox->GetEditableSelectionRange())
                {
                    outState.selectionAnchorIndex = selection->first;
                    outState.caretIndex           = selection->second;
                }
                return true;
            }

            return false;
        }
        catch (const std::bad_alloc&)
        {
            outState = {};
            return false;
        }
    }

    [[nodiscard]] bool ApplyControlState(const TextInputState& state, bool notifyChange) noexcept
    {
        Control* const control           = GetLiveControl();
        const auto* eligibilityTextField = dynamic_cast<const TextField*>(control);
        if (! control || ! IsCurrentTextServiceTarget() || state.readOnly || state.masked ||
            (eligibilityTextField && (eligibilityTextField->IsMasked() || eligibilityTextField->IsReadOnly())))
        {
            return false;
        }

        Control* const previousFocus = _host->GetFocusControl();
        try
        {
            if (auto* textField = dynamic_cast<TextField*>(control))
            {
                if (! control->ImportTextInputState(*_host, state, notifyChange))
                    return false;

                // Notifying user code may replace the tree, focus or text. Never apply stale selection afterward.
                if (GetLiveControl() != control || ! IsCurrentTextServiceTarget() || _host->GetFocusControl() != previousFocus ||
                    textField->GetText() != state.text)
                    return false;
                _host->SyncTextInput(control);
                if (GetLiveControl() != control || ! IsCurrentTextServiceTarget())
                    return false;
                _host->Invalidate();
                return true;
            }

            if (auto* comboBox = dynamic_cast<ComboBox*>(control); comboBox && comboBox->IsEditable())
            {
                if (! control->ImportTextInputState(*_host, state, notifyChange))
                    return false;

                if (GetLiveControl() != control || ! IsCurrentTextServiceTarget() || _host->GetFocusControl() != previousFocus ||
                    comboBox->GetText() != state.text)
                    return false;
                _host->SyncTextInput(control);
                if (GetLiveControl() != control || ! IsCurrentTextServiceTarget())
                    return false;
                _host->Invalidate();
                return true;
            }
        }
        catch (const std::exception&)
        {
            // Contain allocation/user callback failures at the COM boundary. SetText already invalidated its view.
            return false;
        }
        return false;
    }

    [[nodiscard]] bool FinishComposition(const TextInputState& finalState) noexcept
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

        TextInputState base = std::move(_compositionBase.value());
        _compositionBase.reset();
        _compositionEndPending = false;
        const bool restored    = RestoreCompositionBase(base);
        _compositionExpectedText.reset();
        if (! restored)
            return false;
        if (base.text == finalState.text)
        {
            return ApplyControlState(finalState, false);
        }
        return ApplyControlState(finalState, true);
    }

    [[nodiscard]] bool RestoreCompositionBase(const TextInputState& base) noexcept
    {
        Control* const control = GetLiveControl();
        if (! control)
            return false;
        try
        {
            TextInputState current{};
            if (! ReadBackingState(current))
                return false;
            if (! _compositionExpectedText || current.text != _compositionExpectedText.value())
                return true;
            if (! CanRestoreOwnedComposition(control))
                return false;

            TextInputState restore = base;
            // Cancellation rolls back preview content and selection while preserving
            // policy changes made by the application during composition.
            restore.readOnly = current.readOnly;
            restore.masked   = current.masked;
            if (auto* textField = dynamic_cast<TextField*>(control))
            {
                if (! control->ImportTextInputState(*_host, restore, false))
                    return false;
            }
            else if (auto* comboBox = dynamic_cast<ComboBox*>(control); comboBox && comboBox->IsEditable())
            {
                if (! control->ImportTextInputState(*_host, restore, false))
                    return false;
            }
            else
            {
                return false;
            }

            if (GetLiveControl() != control || ! CanRestoreOwnedComposition(control))
                return false;
            _host->SyncTextInput(control);
            if (GetLiveControl() != control || ! CanRestoreOwnedComposition(control))
                return false;
            TextInputState afterSync{};
            if (! ReadBackingState(afterSync) || afterSync.text != restore.text)
                return false;
            _host->Invalidate();
            return true;
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }
        catch (const std::exception&)
        {
            return false;
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
    std::optional<TextInputState> _editBase;
    std::optional<TextInputState> _stagedState;
    std::optional<TextInputState> _compositionBase;
    std::optional<std::wstring> _compositionExpectedText;
    bool _editActive            = false;
    bool _compositionEndPending = false;
};

namespace
{

class TextStoreACP final : public ITextStoreACP, public ITextStoreACP2, public ITfContextOwnerCompositionSink
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
        SecureWipe::SecureClear(_observedState.text);
        _observedState    = TextInputState{};
        _observedViewport = D2D1::RectF();
        _hasObservedState = false;
    }

    HRESULT STDMETHODCALLTYPE OnStartComposition(ITfCompositionView* composition, BOOL* outAccepted) noexcept override
    {
        if (! outAccepted)
        {
            return E_POINTER;
        }

        *outAccepted = FALSE;
        return _target->StartComposition(composition, outAccepted);
    }

    HRESULT STDMETHODCALLTYPE OnUpdateComposition(ITfCompositionView* composition, ITfRange* rangeNew) noexcept override
    {
        return _target->UpdateComposition(composition, rangeNew);
    }

    HRESULT STDMETHODCALLTYPE OnEndComposition(ITfCompositionView* composition) noexcept override
    {
        return _target->EndComposition(composition);
    }

    HRESULT STDMETHODCALLTYPE AdviseSink(REFIID riid, IUnknown* punk, DWORD dwMask) noexcept override
    {
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
            return E_FAIL;
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
            TextInputState currentState{};
            return target->ReadState(currentState) && ! currentState.masked && currentState.text == _observedState.text &&
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
        if (! pdcs)
        {
            return E_POINTER;
        }

        pdcs->dwDynamicFlags = 0u;
        pdcs->dwStaticFlags  = TS_SS_NOHIDDENTEXT;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE QueryInsert(LONG acpTestStart, LONG acpTestEnd, ULONG cch, LONG* pacpResultStart, LONG* pacpResultEnd) noexcept override
    {
        if (! pacpResultStart || ! pacpResultEnd)
        {
            return E_POINTER;
        }

        TextInputState state{};
        if (! ReadState(state))
        {
            return TS_E_INVALIDPOS;
        }

        const AcpRange range = ClampAcpRange(acpTestStart, acpTestEnd, state.text.size());
        *pacpResultStart     = ToAcp(range.start);
        *pacpResultEnd       = ToAcp(range.start + cch);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetSelection(ULONG ulIndex, ULONG ulCount, TS_SELECTION_ACP* pSelection, ULONG* pcFetched) noexcept override
    {
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

        TextInputState state{};
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

        TextInputState state{};
        if (! ReadState(state))
        {
            return TS_E_INVALIDPOS;
        }

        const AcpRange range                       = ClampAcpRange(pSelection[0].acpStart, pSelection[0].acpEnd, state.text.size(), false);
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
        if (! HasReadLock())
        {
            return TS_E_NOLOCK;
        }
        if (! pcchPlainRet || ! pcRunInfoRet || ! pacpNext)
        {
            return E_POINTER;
        }

        TextInputState state{};
        if (! ReadState(state))
        {
            return TS_E_INVALIDPOS;
        }

        const AcpRange range = ClampAcpRange(acpStart, acpEnd, state.text.size());
        const std::wstring_view value(state.text.data() + range.start, range.end - range.start);
        const size_t copied = (std::min)(value.size(), static_cast<size_t>(cchPlainReq));
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
        if (! HasReadWriteLock())
        {
            return TS_E_NOLOCK;
        }

        return ReplaceTextRange(dwFlags, acpStart, acpEnd, pchText, cch, pChange);
    }

    HRESULT STDMETHODCALLTYPE GetFormattedText(LONG /*acpStart*/, LONG /*acpEnd*/, IDataObject** ppDataObject) noexcept override
    {
        if (! ppDataObject)
        {
            return E_POINTER;
        }

        *ppDataObject = nullptr;
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetEmbedded(LONG /*acpPos*/, REFGUID /*rguidService*/, REFIID /*riid*/, IUnknown** ppunk) noexcept override
    {
        if (! ppunk)
        {
            return E_POINTER;
        }

        *ppunk = nullptr;
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE QueryInsertEmbedded(const GUID* /*pguidService*/, const FORMATETC* /*pFormatEtc*/, BOOL* pfInsertable) noexcept override
    {
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
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE RequestSupportedAttrs(DWORD /*dwFlags*/, ULONG /*cFilterAttrs*/, const TS_ATTRID* /*paFilterAttrs*/) noexcept override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE RequestAttrsAtPosition(LONG /*acpPos*/,
                                                     ULONG /*cFilterAttrs*/,
                                                     const TS_ATTRID* /*paFilterAttrs*/,
                                                     DWORD /*dwFlags*/) noexcept override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE RequestAttrsTransitioningAtPosition(LONG /*acpPos*/,
                                                                  ULONG /*cFilterAttrs*/,
                                                                  const TS_ATTRID* /*paFilterAttrs*/,
                                                                  DWORD /*dwFlags*/) noexcept override
    {
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
        if (! pacpNext || ! pfFound || ! plFoundOffset)
        {
            return E_POINTER;
        }

        *pacpNext      = acpStart;
        *pfFound       = FALSE;
        *plFoundOffset = 0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE RetrieveRequestedAttrs(ULONG /*ulCount*/, TS_ATTRVAL* /*paAttrVals*/, ULONG* pcFetched) noexcept override
    {
        if (! pcFetched)
        {
            return E_POINTER;
        }

        *pcFetched = 0u;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetEndACP(LONG* pacp) noexcept override
    {
        if (! HasReadLock())
        {
            return TS_E_NOLOCK;
        }
        if (! pacp)
        {
            return E_POINTER;
        }

        TextInputState state{};
        if (! ReadState(state))
        {
            return TS_E_INVALIDPOS;
        }

        *pacp = ToAcp(state.text.size());
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetActiveView(TsViewCookie* pvcView) noexcept override
    {
        if (! pvcView)
        {
            return E_POINTER;
        }

        *pvcView = kTextStoreView;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetACPFromPoint(TsViewCookie vcView, const POINT* ptScreen, DWORD /*dwFlags*/, LONG* pacp) noexcept override
    {
        if (! HasReadLock())
        {
            return TS_E_NOLOCK;
        }
        if (! ptScreen || ! pacp)
        {
            return E_POINTER;
        }
        if (vcView != kTextStoreView)
        {
            return E_INVALIDARG;
        }

        return _target->GetAcpFromScreenPoint(ptScreen, pacp);
    }

    HRESULT STDMETHODCALLTYPE GetTextExt(TsViewCookie vcView, LONG acpStart, LONG acpEnd, RECT* prc, BOOL* pfClipped) noexcept override
    {
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
        TextInputState state;
        if (! ReadState(state))
            return TS_E_INVALIDPOS;
        if (state.readOnly)
            return E_ACCESSDENIED;
        const AcpRange range     = GetSelectionRange(state);
        constexpr size_t maximum = 65536;
        const size_t retained    = state.text.size() - (range.end - range.start);
        if (retained > maximum || cch > maximum - retained)
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

    [[nodiscard]] bool ReadState(TextInputState& state) const noexcept
    {
        if (! _target->ReadState(state) || state.masked)
        {
            state = {};
            return false;
        }
        return true;
    }
    [[nodiscard]] bool ApplyState(const TextInputState& state, bool notifyChange) noexcept
    {
        return _target->ApplyState(state, notifyChange);
    }

    HRESULT ReplaceTextRange(DWORD dwFlags, LONG acpStart, LONG acpEnd, const WCHAR* pchText, ULONG cch, TS_TEXTCHANGE* pChange) noexcept
    {
        TextInputState state{};
        if (! ReadState(state))
        {
            return TS_E_INVALIDPOS;
        }
        if (state.readOnly)
        {
            return E_ACCESSDENIED;
        }
        if (cch > 0u && ! pchText)
        {
            return E_POINTER;
        }

        const AcpRange range = ClampAcpRange(acpStart, acpEnd, state.text.size());
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
        const size_t newCaret             = range.start + replacement.size();
        const bool selfEditChanged        = state.text.compare(range.start, range.end - range.start, replacement) != 0 || state.caretIndex != newCaret ||
                                            state.selectionAnchorIndex.has_value();
        constexpr size_t maximumTextUnits = 65536;
        const size_t retainedUnits        = state.text.size() - (range.end - range.start);
        if (retainedUnits > maximumTextUnits || replacement.size() > maximumTextUnits - retainedUnits)
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        try
        {
            state.text.replace(range.start, range.end - range.start, replacement);
        }
        catch (const std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
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
        TextInputState state{};
        if (! ReadState(state))
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

        TextInputState currentState{};
        if (! target->ReadState(currentState) || currentState.masked)
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

        const bool textChanged      = currentState.text != _observedState.text;
        const bool selectionChanged = ! IsSameSelection(currentState, _observedState);
        const bool layoutChanged    = textChanged || selectionChanged || currentState.firstVisibleLine != _observedState.firstVisibleLine ||
                                      currentState.masked != _observedState.masked || currentState.multiline != _observedState.multiline ||
                                      ! IsSameRect(currentViewport, _observedViewport);

        const TS_TEXTCHANGE textChange{0, ToAcp(_observedState.text.size()), ToAcp(currentState.text.size())};

        TextInputState notificationState{};
        try
        {
            notificationState = currentState;
        }
        catch (const std::bad_alloc&)
        {
            _pendingExternalNotification = true;
            // Keep the change pending for the next real external-change notification; do not repost indefinitely
            // under sustained allocation failure.
            return false;
        }
        _observedState               = std::move(currentState);
        _observedViewport            = currentViewport;
        _pendingExternalNotification = false;

        const auto continuationIsCurrent = [this, sink, sinkMask, sinkConnectionRevision, target, &notificationState, currentViewport]() noexcept
        {
            if (_target != target || _sink.get() != sink.get() || _sinkMask != sinkMask || _sinkConnectionRevision != sinkConnectionRevision ||
                _lockFlags != 0u || ! _hasObservedState || _observedState.text != notificationState.text ||
                ! IsSameSelection(_observedState, notificationState) || _observedState.firstVisibleLine != notificationState.firstVisibleLine ||
                _observedState.masked != notificationState.masked || _observedState.multiline != notificationState.multiline ||
                ! IsSameRect(_observedViewport, currentViewport))
                return false;

            TextInputState latestState{};
            return target->ReadState(latestState) && ! latestState.masked && latestState.text == notificationState.text &&
                   IsSameSelection(latestState, notificationState) && latestState.firstVisibleLine == notificationState.firstVisibleLine &&
                   latestState.masked == notificationState.masked && latestState.multiline == notificationState.multiline &&
                   IsSameRect(target->ResolveTextViewportBounds(), currentViewport);
        };

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

    std::atomic<ULONG> _referenceCount{1u};
    std::shared_ptr<TextStoreTarget> _target;
    DWORD _pendingLockFlags = 0;
    DWORD _lockFlags        = 0u;
    DWORD _sinkMask         = 0u;
    wil::com_ptr_nothrow<ITextStoreACPSink> _sink;
    uint64_t _sinkConnectionRevision = 0u;
    TextInputState _observedState;
    D2D1_RECT_F _observedViewport     = D2D1::RectF();
    bool _hasObservedState            = false;
    bool _pendingExternalNotification = false;
    bool _pendingLayoutNotification   = false;
    bool _hasSelfEditInLock           = false;
#if DXUI_ENABLE_DIAGNOSTICS
    uint64_t _debugTextChangeNotifications      = 0u;
    uint64_t _debugSelectionChangeNotifications = 0u;
    uint64_t _debugLayoutChangeNotifications    = 0u;
#endif
};
} // namespace

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
        return nullptr;
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
