#pragma once
#include <DxUi/TextInputServices.h>
#include <cmath>
#include <limits>
#include <msctf.h>

// Module-local adapter using only public headers. Disconnect before destroying the view or HWND.
class EmbeddedTextClient final : public DxUi::TextInputClient
{
public:
    EmbeddedTextClient(DxUi::EmbeddedHost& view, HWND window, uint64_t focus, float dpi) noexcept : _view(&view), _window(window), _focus(focus), _dpi(dpi)
    {
    }
    uint64_t FocusId() const noexcept
    {
        return _focus;
    }
    void Disconnect() noexcept
    {
        _view   = nullptr;
        _window = nullptr;
    }
    void SetDpi(float dpi) noexcept
    {
        _dpi = dpi;
    }
    HRESULT Read(DxUi::EmbeddedTextInputSnapshot& snapshot) noexcept override
    {
        snapshot = {};
        if (! _view)
            return TF_E_DISCONNECTED;
        const HRESULT hr = _view->ReadTextInput(snapshot);
        if (hr != S_OK)
            return hr;
        if (snapshot.focusId == _focus)
            return S_OK;
        snapshot = {};
        return S_FALSE;
    }
    HRESULT Apply(uint64_t revision, const DxUi::NativeTextInputState& state, DxUi::EmbeddedTextInputAction action) noexcept override
    {
        DxUi::EmbeddedTextInputSnapshot current;
        const HRESULT read = Read(current);
        if (read != S_OK)
            return read;
        return _view->ApplyTextInput(revision, state, action);
    }
    void Cancel() noexcept override
    {
        DxUi::EmbeddedTextInputSnapshot current;
        if (Read(current) == S_OK)
            static_cast<void>(_view->ApplyTextInput(current.revision, {}, DxUi::EmbeddedTextInputAction::Cancel));
    }
    HRESULT HitTest(POINT screen, size_t& index) noexcept override
    {
        index = 0;
        DxUi::EmbeddedTextInputSnapshot current;
        const HRESULT read = Read(current);
        if (read != S_OK)
            return read;
        if (! ScreenToClient(_window, &screen))
            return HRESULT_FROM_WIN32(GetLastError());
        return _view->HitTestTextInput(current.revision, {float(screen.x) * 96 / _dpi, float(screen.y) * 96 / _dpi}, index);
    }
    HRESULT RangeBounds(size_t start, size_t end, RECT& bounds, bool& clipped) noexcept override
    {
        bounds  = {};
        clipped = true;
        DxUi::EmbeddedTextInputSnapshot current;
        const HRESULT read = Read(current);
        if (read != S_OK)
            return read;
        D2D1_RECT_F dip{};
        const HRESULT hr = _view->GetTextInputRangeBounds(current.revision, start, end, dip, clipped);
        if (hr != S_OK)
            return hr;
        if (dip.left == dip.right || dip.top == dip.bottom)
            return S_OK;
        return ToScreen(dip, bounds);
    }
    HRESULT ViewportBounds(RECT& bounds) noexcept override
    {
        bounds = {};
        DxUi::EmbeddedTextInputSnapshot current;
        const HRESULT read = Read(current);
        if (read != S_OK)
            return read;
        return current.viewportBoundsDip ? ToScreen(*current.viewportBoundsDip, bounds) : S_FALSE;
    }

private:
    HRESULT ToScreen(const D2D1_RECT_F& rect, RECT& output) noexcept
    {
        POINT origin{};
        if (! ClientToScreen(_window, &origin))
            return HRESULT_FROM_WIN32(GetLastError());
        const double scale = double(_dpi) / 96;
        const double l = std::floor(rect.left * scale + origin.x), t = std::floor(rect.top * scale + origin.y);
        const double rr = std::ceil(rect.right * scale + origin.x), b = std::ceil(rect.bottom * scale + origin.y);
        constexpr double low = std::numeric_limits<LONG>::min(), high = std::numeric_limits<LONG>::max();
        if (! std::isfinite(l) || ! std::isfinite(t) || ! std::isfinite(rr) || ! std::isfinite(b) || l < low || t < low || rr > high || b > high || l > rr ||
            t > b)
            return E_INVALIDARG;
        output = {LONG(l), LONG(t), LONG(rr), LONG(b)};
        return S_OK;
    }
    DxUi::EmbeddedHost* _view;
    HWND _window;
    uint64_t _focus;
    float _dpi;
};
