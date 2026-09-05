#pragma once
#include "Embedded.h"

namespace DxUi
{
// Application-side C++ adapter. Keep it in the module linking DxUi.lib; use application-defined COM/POD
// transport to a plugin. Calls are synchronous on the attaching UI thread, outside rendering.
class TextInputClient
{
public:
    virtual ~TextInputClient() = default;
    // Each client represents one immutable focusId. Read returns S_FALSE after that focus is replaced;
    // Apply and Cancel must also verify it so old services cannot modify the replacement field.
    virtual HRESULT Read(EmbeddedTextInputSnapshot& snapshot) noexcept                                                   = 0;
    virtual HRESULT Apply(uint64_t revision, const NativeTextInputState& state, EmbeddedTextInputAction action) noexcept = 0;
    virtual void Cancel() noexcept                                                                                       = 0;
    // The application converts geometry to physical screen coordinates. No HWND is sent to an embedded view.
    virtual HRESULT HitTest(POINT screenPoint, size_t& index) noexcept                              = 0;
    virtual HRESULT RangeBounds(size_t start, size_t end, RECT& screenRect, bool& clipped) noexcept = 0;
    virtual HRESULT ViewportBounds(RECT& screenRect) noexcept                                       = 0;
};

enum class TextClipboardCommand : uint8_t
{
    Copy,
    Cut,
    Paste
};
// Borrowed only during a command. The default uses one nonblocking OpenClipboard attempt and bounded
// Unicode data. Applications and tests may supply their own clipboard implementation.
class TextClipboard
{
public:
    virtual ~TextClipboard()                                           = default;
    virtual HRESULT Read(HWND owner, std::wstring& text) noexcept      = 0;
    virtual HRESULT Write(HWND owner, std::wstring_view text) noexcept = 0;
};

// One lazy TSF service per application HWND. Borrows the HWND and retains only the active client.
// Owns no renderer, swap chain, timer or worker. The application initializes COM in its UI STA.
// ClearClient before page/view removal and on OS focus loss; a retained TSF reference becomes disconnected.
class TextInputServices final
{
public:
    TextInputServices();
    ~TextInputServices();
    TextInputServices(const TextInputServices&)            = delete;
    TextInputServices& operator=(const TextInputServices&) = delete;
    HRESULT Attach(HWND hwnd) noexcept;
    void Detach() noexcept;
    HRESULT SetClient(std::shared_ptr<TextInputClient> client) noexcept;
    void ClearClient() noexcept;
    void NotifyChanged() noexcept;
    // Call after changed layout has been prepared (including scroll/DPI/screen-position changes).
    // Required when geometry queries previously reported TS_E_NOLAYOUT. Never call from inside a client callback.
    void NotifyLayoutChanged() noexcept;
    HRESULT Clipboard(TextClipboardCommand command, TextClipboard* clipboard = nullptr) noexcept;
    // Call before TranslateMessage/DispatchMessage. S_OK + handled consumes a TSF key; unrelated messages pass.
    HRESULT PreTranslate(const MSG& message, bool& handled) noexcept;
    // Forward window messages before ordinary dispatch. Only this instance's coalesced deferred TSF lock
    // message is consumed; stale attachment/client generations are ignored. No timer or extra HWND is used.
    HRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) noexcept;
    // Escape cancels preview before ordinary editor handling. A second Escape remains available to the app.
    bool CancelComposition() noexcept;
    [[nodiscard]] bool HasClient() const noexcept;

private:
    struct State;
    std::unique_ptr<State> _state;
};
} // namespace DxUi
