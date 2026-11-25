#pragma once

// A native Grid scrolling scenario: a 2,048-row, 4-column Grid filling an offscreen, non-activating WindowHost window,
// driven by messages sent to the window as the system would deliver them. Inputs arrive through the host's window
// procedure; Paint renders the pending invalidation synchronously (WM_PAINT), so each input can be followed by the frame
// that shows it. The performance and motion plan's FX3 fixture extends this scenario; nothing here takes focus or moves
// the pointer.
#include "Controls.Tests.DxUiTestHelpers.h"

class GridScrollScenario final
{
public:
    static constexpr size_t kRows        = 2048u;
    static constexpr size_t kColumns     = 4u;
    static constexpr float kColumnWidth  = 72.0f;
    static constexpr float kRowHeightDip = 24.0f;
    static constexpr float kHeaderHeight = 28.0f;

    GridScrollScenario() : _model(kRows, kColumns, kColumnWidth)
    {
        DxUi::WindowHost& host = _window.Host();
        auto root              = std::make_unique<DxUi::Panel>();
        _grid                  = root->AddChild<DxUi::Grid>();
        _grid->SetRowHeightDip(kRowHeightDip);
        _grid->SetHeaderHeightDip(kHeaderHeight);
        _grid->SetModel(&_model);
        host.SetRoot(std::move(root));
        RECT client{};
        Require(GetClientRect(_window.Hwnd(), &client) != FALSE && client.right > 0 && client.bottom > 0, "the scenario window has a client area");
        const float widthDip  = host.PixelsToDip(static_cast<float>(client.right));
        const float heightDip = host.PixelsToDip(static_cast<float>(client.bottom));
        host.GetRoot()->SetBounds(D2D1::RectF(0.0f, 0.0f, widthDip, heightDip));
        _grid->SetBounds(D2D1::RectF(0.0f, 0.0f, widthDip, heightDip));
        _pointClientPx = POINT{client.right / 2, client.bottom / 2};
        ShowWindow(_window.Hwnd(), SW_SHOWNOACTIVATE);
        RedrawWindow(_window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        _window.PumpMessages();
    }

    GridScrollScenario(const GridScrollScenario&)            = delete;
    GridScrollScenario& operator=(const GridScrollScenario&) = delete;
    GridScrollScenario(GridScrollScenario&&)                 = delete;
    GridScrollScenario& operator=(GridScrollScenario&&)      = delete;
    ~GridScrollScenario()                                    = default;

    [[nodiscard]] AttachedHostWindow& Window() noexcept
    {
        return _window;
    }
    [[nodiscard]] DxUi::Grid& Grid() noexcept
    {
        return *_grid;
    }

    // One wheel notch over the grid's centre; negative scrolls toward the end.
    void Wheel(int notches = -1)
    {
        POINT screen = _pointClientPx;
        Require(ClientToScreen(_window.Hwnd(), &screen) != FALSE, "the wheel point maps to the screen");
        static_cast<void>(
            SendMessageW(_window.Hwnd(), WM_MOUSEWHEEL, MAKEWPARAM(0u, static_cast<WORD>(notches * WHEEL_DELTA)), MAKELPARAM(screen.x, screen.y)));
    }

    // One pointer move inside the grid, offset from its centre.
    void Move(int dxPx, int dyPx)
    {
        static_cast<void>(SendMessageW(_window.Hwnd(), WM_MOUSEMOVE, 0u, MAKELPARAM(_pointClientPx.x + dxPx, _pointClientPx.y + dyPx)));
    }

    // One key press and release.
    void Key(UINT virtualKey)
    {
        static_cast<void>(SendMessageW(_window.Hwnd(), WM_KEYDOWN, virtualKey, 1));
        static_cast<void>(SendMessageW(_window.Hwnd(), WM_KEYUP, virtualKey, static_cast<LPARAM>(0xC0000001u)));
    }

    // Renders and presents whatever the inputs invalidated, now: with a frame-latency credit taken first (W3.6), the paint
    // renders at once instead of deferring to the host's next paced frame.
    void Paint() const noexcept
    {
        static_cast<void>(AwaitFrameCreditForTest(const_cast<AttachedHostWindow&>(_window).Host()));
        UpdateWindow(_window.Hwnd());
    }

private:
    LargeGridModel _model;
    AttachedHostWindow _window;
    DxUi::Grid* _grid = nullptr;
    POINT _pointClientPx{};
};
