#include "DxUi.Internal.h"
#include "TextClipboard.h"
#include "TextStoreTarget.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <new>

namespace DxUi
{
namespace
{
constexpr size_t kMaximumTextUnits = 65536;
std::atomic<UINT_PTR> nextTextDispatchCookie{1};
UINT_PTR NextDispatchCookie() noexcept
{
    UINT_PTR value = nextTextDispatchCookie.fetch_add(1, std::memory_order_relaxed);
    if (! value)
        value = nextTextDispatchCookie.fetch_add(1, std::memory_order_relaxed);
    return value;
}
UINT TextDispatchMessage() noexcept
{
    static const UINT message = RegisterWindowMessageW(L"DxUi.TextInputServices.DeferredLock.v1");
    return message;
}

bool ValidState(const NativeTextInputState& state) noexcept
{
    const size_t length = state.text.size();
    return length <= kMaximumTextUnits && state.caretIndex <= length && (! state.selectionAnchorIndex || *state.selectionAnchorIndex <= length) &&
           state.firstVisibleLine <= length;
}

void ClearComposition(NativeTextInputState& state) noexcept
{
    state.compositionStartIndex.reset();
    state.compositionEndIndex.reset();
    state.compositionCursorIndex.reset();
    state.conversionTargetStartIndex.reset();
    state.conversionTargetEndIndex.reset();
    state.compositionClauseBoundaries.clear();
}

HRESULT ReadCompositionRange(ITfCompositionView* composition, ITfRange* suppliedRange, size_t& start, size_t& end) noexcept
{
    wil::com_ptr_nothrow<ITfRange> range;
    if (suppliedRange)
        range = suppliedRange;
    else if (! composition)
        return E_POINTER;
    else
    {
        const HRESULT hr = composition->GetRange(range.put());
        if (FAILED(hr))
            return hr;
    }
    wil::com_ptr_nothrow<ITfRangeACP> acp;
    if (! range)
        return E_INVALIDARG;
    RETURN_IF_FAILED(range.query_to(acp.put()));
    LONG first = 0, count = 0;
    RETURN_IF_FAILED(acp->GetExtent(&first, &count));
    if (first < 0 || count < 0 || static_cast<size_t>(count) > kMaximumTextUnits || static_cast<size_t>(first) > kMaximumTextUnits - static_cast<size_t>(count))
        return E_INVALIDARG;
    start = static_cast<size_t>(first);
    end   = start + static_cast<size_t>(count);
    return S_OK;
}

// One adapter per focused view. TSF writes are staged for one lock: InsertTextAtSelection can precede
// OnStartComposition in that same lock. Publishing early would incorrectly commit the first IME preview.
class ClientTextStoreTarget final : public TextStoreTarget
{
public:
    ClientTextStoreTarget(HWND hwnd, std::shared_ptr<TextInputClient> client, uint64_t focusId, TextStoreDispatch dispatch) noexcept
        : _hwnd(hwnd),
          _client(std::move(client)),
          _thread(GetCurrentThreadId()),
          _focusId(focusId),
          _dispatch(dispatch)
    {
    }

    void Disconnect() noexcept override
    {
        if (_thread != GetCurrentThreadId())
            return;
        auto client = std::move(_client);
        _hwnd       = nullptr;
        _editing    = false;
        _dirty      = false;
        _composition.reset();
        _snapshot = {};
        _dispatch = {};
        if (client)
        {
            EmbeddedTextInputSnapshot current;
            if (client->Read(current) == S_OK && current.focusId == _focusId)
                client->Cancel();
        }
    }
    HWND GetHwnd() const noexcept override
    {
        return Available() ? _hwnd : nullptr;
    }
    bool ScheduleLock() noexcept override
    {
        return Available() && _dispatch.requestLock && _dispatch.requestLock(_dispatch.context);
    }
    bool NotifyDuringLock() const noexcept override
    {
        return false;
    }

    bool ReadState(TextInputState& state) const noexcept override
    {
        state = {};
        if (! Available())
            return false;
        try
        {
            EmbeddedTextInputSnapshot current;
            const NativeTextInputState* source = &_snapshot.state;
            if (! _editing)
            {
                const auto client = _client;
                if (client->Read(current) != S_OK || client != _client || ! current.revision || current.focusId != _focusId || ! ValidState(current.state))
                    return false;
                source = &current.state;
            }
            state.text                 = source->text;
            state.caretIndex           = source->caretIndex;
            state.selectionAnchorIndex = source->selectionAnchorIndex;
            state.firstVisibleLine     = source->firstVisibleLine;
            state.readOnly             = source->readOnly;
            state.masked               = source->masked;
            state.multiline            = source->multiline;
            return true;
        }
        catch (const std::bad_alloc&)
        {
            state = {};
            return false;
        }
    }
    bool ApplyState(const TextInputState& state, bool) noexcept override
    {
        if (! Available() || ! _editing || ! _writable || state.text.size() > kMaximumTextUnits || state.caretIndex > state.text.size() ||
            (state.selectionAnchorIndex && *state.selectionAnchorIndex > state.text.size()) || (_snapshot.state.readOnly && state.text != _snapshot.state.text))
            return false;
        try
        {
            _geometryDirty             = _geometryDirty || state.text != _snapshot.state.text || state.firstVisibleLine != _snapshot.state.firstVisibleLine;
            _snapshot.state.text       = state.text;
            _snapshot.state.caretIndex = state.caretIndex;
            _snapshot.state.selectionAnchorIndex = state.selectionAnchorIndex;
            _snapshot.state.firstVisibleLine     = state.firstVisibleLine;
            _dirty                               = true;
            return true;
        }
        catch (const std::bad_alloc&)
        {
            _failed = true;
            return false;
        }
    }
    HRESULT BeginEdit(bool writable) noexcept override
    {
        if (! Available())
            return TF_E_DISCONNECTED;
        if (_editing)
            return TS_E_SYNCHRONOUS;
        EmbeddedTextInputSnapshot current;
        const auto client = _client;
        const HRESULT hr  = client->Read(current);
        if (hr != S_OK || ! current.revision || current.focusId != _focusId || ! ValidState(current.state))
            return FAILED(hr) ? hr : TS_E_INVALIDPOS;
        if (client != _client)
            return TF_E_DISCONNECTED;
        _snapshot      = std::move(current);
        _editing       = true;
        _writable      = writable;
        _dirty         = false;
        _failed        = false;
        _rangeSupplied = false;
        _geometryDirty = false;
        return S_OK;
    }
    HRESULT EndEdit(bool success) noexcept override
    {
        if (! _editing)
            return TF_E_DISCONNECTED;
        if (! success || _failed)
        {
            _editing = false;
            _dirty   = false;
            return E_FAIL;
        }
        const HRESULT hr = Flush();
        _editing         = false;
        _dirty           = false;
        return hr;
    }

    HRESULT StartComposition(ITfCompositionView* composition, BOOL* accepted) noexcept override
    {
        *accepted = FALSE;
        if (! Available())
            return TF_E_DISCONNECTED;
        if (_composition || ! composition)
            return S_OK;
        const bool ownEdit = ! _editing;
        if (ownEdit)
            RETURN_IF_FAILED(BeginEdit(true));
        size_t start = 0, end = 0;
        HRESULT hr = ReadCompositionRange(composition, nullptr, start, end);
        if (SUCCEEDED(hr) && (end > _snapshot.state.text.size() || _snapshot.state.readOnly))
            hr = E_INVALIDARG;
        if (SUCCEEDED(hr))
        {
            _composition = composition;
            SetCompositionRange(start, end);
            _dirty    = true;
            *accepted = TRUE;
        }
        if (ownEdit)
        {
            const HRESULT flush = EndEdit(SUCCEEDED(hr));
            if (SUCCEEDED(hr))
                hr = flush;
        }
        if (FAILED(hr))
        {
            _composition.reset();
            *accepted = FALSE;
        }
        return hr;
    }
    HRESULT UpdateComposition(ITfCompositionView* composition, ITfRange* range) noexcept override
    {
        if (! Available())
            return TF_E_DISCONNECTED;
        if (! _composition || _composition.get() != composition)
            return E_INVALIDARG;
        // A null new range is a notification that the old range remains in force.
        const bool ownEdit = ! _editing;
        if (ownEdit)
            RETURN_IF_FAILED(BeginEdit(true));
        size_t start = 0, end = 0;
        HRESULT hr = ReadCompositionRange(composition, range, start, end);
        if (SUCCEEDED(hr) && end > _snapshot.state.text.size())
            hr = E_INVALIDARG;
        if (SUCCEEDED(hr))
        {
            SetCompositionRange(start, end);
            _dirty = true;
        }
        if (ownEdit)
        {
            const HRESULT flush = EndEdit(SUCCEEDED(hr));
            if (SUCCEEDED(hr))
                hr = flush;
        }
        return hr;
    }
    HRESULT EndComposition(ITfCompositionView* composition) noexcept override
    {
        if (! Available())
            return TF_E_DISCONNECTED;
        if (! _composition || _composition.get() != composition)
            return E_INVALIDARG;
        const bool ownEdit = ! _editing;
        if (ownEdit)
            RETURN_IF_FAILED(BeginEdit(true));
        _composition.reset();
        ClearComposition(_snapshot.state);
        _dirty = true;
        return ownEdit ? EndEdit(true) : S_OK;
    }

    D2D1_RECT_F ResolveTextViewportBounds() const noexcept override
    {
        RECT rect{};
        if (GetScreenRect(&rect) != S_OK)
            return {};
        return {static_cast<float>(rect.left), static_cast<float>(rect.top), static_cast<float>(rect.right), static_cast<float>(rect.bottom)};
    }
    HRESULT GetScreenRect(RECT* rect) const noexcept override
    {
        *rect = {};
        if (! Available())
            return TF_E_DISCONNECTED;
        const auto client = _client;
        const HRESULT hr  = client->ViewportBounds(*rect);
        if (client != _client)
        {
            *rect = {};
            return TF_E_DISCONNECTED;
        }
        if (hr == S_OK && rect->right > rect->left && rect->bottom > rect->top)
            return S_OK;
        *rect = {};
        return FAILED(hr) ? hr : TS_E_NOLAYOUT;
    }
    HRESULT GetAcpFromScreenPoint(const POINT* point, LONG* acp) const noexcept override
    {
        *acp = 0;
        if (! Available())
            return TF_E_DISCONNECTED;
        TextInputState state;
        if (! ReadState(state))
            return TS_E_INVALIDPOS;
        if (_editing && _geometryDirty)
            return TS_E_NOLAYOUT;
        size_t index      = 0;
        const auto client = _client;
        const HRESULT hr  = client->HitTest(*point, index);
        if (client != _client)
            return TF_E_DISCONNECTED;
        if (hr != S_OK || index > state.text.size())
            return FAILED(hr) ? hr : TS_E_INVALIDPOS;
        *acp = static_cast<LONG>(index);
        return S_OK;
    }
    HRESULT GetRangeScreenRect(LONG start, LONG end, RECT* rect, BOOL* clipped) const noexcept override
    {
        *rect    = {};
        *clipped = TRUE;
        if (! Available())
            return TF_E_DISCONNECTED;
        TextInputState state;
        if (! ReadState(state))
            return TS_E_INVALIDPOS;
        if (start < 0 || end < start || static_cast<size_t>(end) > state.text.size())
            return TS_E_INVALIDPOS;
        if (_editing && _geometryDirty)
            return TS_E_NOLAYOUT;
        bool wasClipped   = true;
        const auto client = _client;
        const HRESULT hr  = client->RangeBounds(static_cast<size_t>(start), static_cast<size_t>(end), *rect, wasClipped);
        if (client != _client)
        {
            *rect = {};
            return TF_E_DISCONNECTED;
        }
        // TSF specifies an empty successful rectangle when the requested range is fully clipped.
        if (hr != S_OK || rect->right < rect->left || rect->bottom < rect->top)
        {
            *rect = {};
            return FAILED(hr) ? hr : TS_E_NOLAYOUT;
        }
        *clipped = wasClipped ? TRUE : FALSE;
        return S_OK;
    }

private:
    bool Available() const noexcept
    {
        return _thread == GetCurrentThreadId() && _client != nullptr;
    }
    void SetCompositionRange(size_t start, size_t end) noexcept
    {
        _rangeSupplied = true;
        ClearComposition(_snapshot.state);
        _snapshot.state.compositionStartIndex  = start;
        _snapshot.state.compositionEndIndex    = end;
        _snapshot.state.compositionCursorIndex = std::clamp(_snapshot.state.caretIndex, start, end);
    }
    HRESULT Flush() noexcept
    {
        if (! Available())
            return TF_E_DISCONNECTED;
        if (! _dirty)
            return S_OK;
        if (_composition && ! _rangeSupplied)
        {
            size_t start = 0, end = 0;
            RETURN_IF_FAILED(ReadCompositionRange(_composition.get(), nullptr, start, end));
            if (end > _snapshot.state.text.size())
                return E_INVALIDARG;
            SetCompositionRange(start, end);
        }
        else if (! _composition)
            ClearComposition(_snapshot.state);
        const auto client = _client;
        const auto action = _composition ? EmbeddedTextInputAction::Preview : EmbeddedTextInputAction::Commit;
        HRESULT hr        = E_OUTOFMEMORY;
        try
        {
            // A client callback can disconnect the store. Its input must remain owned until the callback returns.
            const NativeTextInputState change = _snapshot.state;
            const uint64_t revision           = _snapshot.revision;
            EmbeddedTextInputSnapshot current;
            const HRESULT read = client->Read(current);
            if (client != _client || read != S_OK || current.focusId != _focusId)
                return TF_E_DISCONNECTED;
            hr = client->Apply(revision, change, action);
        }
        catch (const std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }
        if (client != _client)
            return TF_E_DISCONNECTED;
        // The client validates the opaque revision. Never retry an edit against a fresh, different document.
        if (hr != S_OK)
            return FAILED(hr) ? hr : E_FAIL;
        return S_OK;
    }

    HWND _hwnd = nullptr;
    std::shared_ptr<TextInputClient> _client;
    DWORD _thread     = 0;
    uint64_t _focusId = 0;
    TextStoreDispatch _dispatch;
    EmbeddedTextInputSnapshot _snapshot;
    wil::com_ptr_nothrow<ITfCompositionView> _composition;
    bool _editing = false, _writable = false, _dirty = false, _failed = false, _rangeSupplied = false, _geometryDirty = false;
};
} // namespace

ITextStoreACP* CreateClientTextStore(HWND hwnd, std::shared_ptr<TextInputClient> client, TextStoreDispatch dispatch) noexcept
{
    if (! client)
        return nullptr;
    EmbeddedTextInputSnapshot snapshot;
    if (client->Read(snapshot) != S_OK || ! snapshot.focusId || ! snapshot.revision || ! ValidState(snapshot.state))
        return nullptr;
    try
    {
        return CreateTextStore(std::make_shared<ClientTextStoreTarget>(hwnd, std::move(client), snapshot.focusId, dispatch));
    }
    catch (const std::bad_alloc&)
    {
        return nullptr;
    }
}

struct TextInputServices::State
{
    HWND hwnd    = nullptr;
    DWORD thread = GetCurrentThreadId();
    std::shared_ptr<TextInputClient> client;
    wil::com_ptr_nothrow<ITfThreadMgr> manager;
    wil::com_ptr_nothrow<ITfDocumentMgr> document, previousDocument;
    wil::com_ptr_nothrow<ITfContext> context;
    wil::com_ptr_nothrow<ITextStoreACP> store;
    TfClientId clientId     = 0;
    bool associated         = false;
    uint64_t focusId        = 0;
    UINT_PTR dispatchCookie = NextDispatchCookie();
    bool lockPosted         = false;
    bool ScheduleLock() noexcept
    {
        if (lockPosted)
            return true;
        if (! hwnd || ! TextDispatchMessage())
            return false;
        lockPosted = PostMessageW(hwnd, TextDispatchMessage(), dispatchCookie, 0) != FALSE;
        return lockPosted;
    }

    bool OnThread() const noexcept
    {
        return thread == GetCurrentThreadId();
    }
    void Clear() noexcept
    {
        // Move retained objects before callbacks; a repeated Clear sees no active client/store.
        auto oldStore  = std::move(store);
        auto oldClient = std::move(client);
        dispatchCookie = NextDispatchCookie();
        lockPosted     = false;
        focusId        = 0;
        if (oldStore)
            DisconnectNativeTextInputTextStore(oldStore.get());
        oldStore.reset();
        oldClient.reset();
        if (manager && associated)
        {
            wil::com_ptr_nothrow<ITfDocumentMgr> replaced;
            static_cast<void>(manager->SetFocus(nullptr));
            if (IsWindow(hwnd))
                static_cast<void>(manager->AssociateFocus(hwnd, previousDocument.get(), replaced.put()));
        }
        associated = false;
        previousDocument.reset();
        if (document)
            static_cast<void>(document->Pop(TF_POPF_ALL));
        context.reset();
        document.reset();
    }
};

TextInputServices::TextInputServices() : _state(std::make_unique<State>())
{
}
TextInputServices::~TextInputServices()
{
    Detach();
}
HRESULT TextInputServices::Attach(HWND hwnd) noexcept
{
    if (! _state->OnThread())
        return RPC_E_WRONG_THREAD;
    if (! hwnd || GetWindowThreadProcessId(hwnd, nullptr) != GetCurrentThreadId())
        return E_INVALIDARG;
    if (_state->hwnd == hwnd)
        return S_OK;
    if (! TextDispatchMessage())
        return HRESULT_FROM_WIN32(GetLastError());
    Detach();
    _state->hwnd = hwnd;
    return S_OK;
}
void TextInputServices::Detach() noexcept
{
    if (! _state->OnThread())
        return;
    _state->Clear();
    if (_state->manager && _state->clientId)
        static_cast<void>(_state->manager->Deactivate());
    _state->clientId = 0;
    _state->manager.reset();
    _state->hwnd = nullptr;
}
void TextInputServices::ClearClient() noexcept
{
    if (_state->OnThread())
        _state->Clear();
}
bool TextInputServices::HasClient() const noexcept
{
    return _state->OnThread() && _state->client != nullptr;
}
HRESULT TextInputServices::SetClient(std::shared_ptr<TextInputClient> client) noexcept
{
    if (! _state->OnThread())
        return RPC_E_WRONG_THREAD;
    if (! _state->hwnd || ! IsWindow(_state->hwnd))
        return E_UNEXPECTED;
    if (client == _state->client)
        return S_OK;
    _state->Clear();
    if (! client)
        return S_OK;
    EmbeddedTextInputSnapshot snapshot;
    const HRESULT read = client->Read(snapshot);
    if (read != S_OK)
        return read;
    if (! snapshot.revision || ! snapshot.focusId || ! ValidState(snapshot.state))
        return E_INVALIDARG;
    if (! _state->manager)
    {
        RETURN_IF_FAILED(CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(_state->manager.put())));
        const HRESULT activate = _state->manager->Activate(&_state->clientId);
        if (FAILED(activate))
        {
            _state->manager.reset();
            _state->clientId = 0;
            return activate;
        }
    }
    auto rollback   = wil::scope_exit([this]() noexcept { _state->Clear(); });
    _state->client  = std::move(client);
    _state->focusId = snapshot.focusId;
    _state->store.attach(CreateClientTextStore(
        _state->hwnd, _state->client, {_state.get(), [](void* context) noexcept { return static_cast<State*>(context)->ScheduleLock(); }}));
    if (! _state->store)
        return E_OUTOFMEMORY;
    RETURN_IF_FAILED(_state->manager->CreateDocumentMgr(_state->document.put()));
    TfEditCookie cookie = 0;
    RETURN_IF_FAILED(_state->document->CreateContext(_state->clientId, 0, _state->store.get(), _state->context.put(), &cookie));
    RETURN_IF_FAILED(_state->document->Push(_state->context.get()));
    RETURN_IF_FAILED(_state->manager->AssociateFocus(_state->hwnd, _state->document.get(), _state->previousDocument.put()));
    _state->associated = true;
    RETURN_IF_FAILED(_state->manager->SetFocus(_state->document.get()));
    rollback.release();
    return S_OK;
}
void TextInputServices::NotifyChanged() noexcept
{
    if (_state->OnThread() && _state->store)
        NotifyTextStoreChanged(_state->store.get());
}
void TextInputServices::NotifyLayoutChanged() noexcept
{
    if (_state->OnThread() && _state->store)
        NotifyTextStoreLayoutChanged(_state->store.get());
}
HRESULT TextInputServices::Clipboard(TextClipboardCommand command, TextClipboard* clipboard) noexcept
{
    if (! _state->OnThread())
        return RPC_E_WRONG_THREAD;
    if (! _state->client)
        return S_FALSE;
    if (command != TextClipboardCommand::Copy && command != TextClipboardCommand::Cut && command != TextClipboardCommand::Paste)
        return E_INVALIDARG;
    auto client = _state->client;
    EmbeddedTextInputSnapshot snapshot;
    const HRESULT read = client->Read(snapshot);
    if (read != S_OK)
        return read;
    if (client != _state->client || ! snapshot.revision || snapshot.focusId != _state->focusId || ! ValidState(snapshot.state))
        return E_UNEXPECTED;
    if (snapshot.state.compositionStartIndex)
        return S_FALSE;
    if ((command != TextClipboardCommand::Paste && snapshot.state.masked) || (command != TextClipboardCommand::Copy && snapshot.state.readOnly))
        return E_ACCESSDENIED;
    const size_t anchor = snapshot.state.selectionAnchorIndex.value_or(snapshot.state.caretIndex);
    const size_t first = (std::min)(anchor, snapshot.state.caretIndex), last = (std::max)(anchor, snapshot.state.caretIndex);
    TextClipboard& service = clipboard ? *clipboard : ActiveTextClipboard();
    try
    {
        if (command != TextClipboardCommand::Paste)
        {
            if (first == last)
                return S_FALSE;
            const HRESULT copied = service.Write(_state->hwnd, std::wstring_view(snapshot.state.text).substr(first, last - first));
            if (copied != S_OK)
                return copied;
            if (client != _state->client)
                return TF_E_DISCONNECTED;
            if (command == TextClipboardCommand::Copy)
                return S_OK;
        }
        std::wstring replacement;
        if (command == TextClipboardCommand::Paste)
        {
            const HRESULT paste = service.Read(_state->hwnd, replacement);
            if (paste != S_OK)
                return paste;
            if (replacement.size() > kMaximumTextUnits || replacement.find(L'\0') != std::wstring::npos)
                return E_INVALIDARG;
            size_t written = 0;
            for (size_t i = 0; i < replacement.size(); ++i)
            {
                wchar_t character = replacement[i];
                if (character == L'\r')
                {
                    if (i + 1 < replacement.size() && replacement[i + 1] == L'\n')
                        ++i;
                    character = L'\n';
                }
                if (! snapshot.state.multiline && (character == L'\n' || character == L'\t'))
                    character = L' ';
                replacement[written++] = character;
            }
            replacement.resize(written);
        }
        if (client != _state->client)
            return TF_E_DISCONNECTED;
        const size_t retained = snapshot.state.text.size() - (last - first);
        if (replacement.size() > kMaximumTextUnits - retained)
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        if (first == last && replacement.empty())
            return S_FALSE;
        snapshot.state.text.replace(first, last - first, replacement);
        snapshot.state.caretIndex = first + replacement.size();
        snapshot.state.selectionAnchorIndex.reset();
        const HRESULT applied = client->Apply(snapshot.revision, snapshot.state, EmbeddedTextInputAction::Commit);
        if (client != _state->client)
            return TF_E_DISCONNECTED;
        if (applied == S_OK)
            NotifyChanged();
        return applied;
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
}

HRESULT TextInputServices::PreTranslate(const MSG& message, bool& handled) noexcept
{
    handled = false;
    if (! _state->OnThread())
        return RPC_E_WRONG_THREAD;
    if (! _state->client || message.hwnd != _state->hwnd || GetFocus() != _state->hwnd)
        return S_FALSE;
    const bool down = message.message == WM_KEYDOWN || message.message == WM_SYSKEYDOWN;
    const bool up   = message.message == WM_KEYUP || message.message == WM_SYSKEYUP;
    if (! down && ! up)
        return S_FALSE;
    if (down && message.wParam == VK_ESCAPE && CancelComposition())
    {
        handled = true;
        return S_OK;
    }
    wil::com_ptr_nothrow<ITfKeystrokeMgr> keys;
    RETURN_IF_FAILED(_state->manager.query_to(keys.put()));
    BOOL eaten = FALSE;
    RETURN_IF_FAILED(down ? keys->TestKeyDown(message.wParam, message.lParam, &eaten) : keys->TestKeyUp(message.wParam, message.lParam, &eaten));
    if (eaten)
    {
        RETURN_IF_FAILED(down ? keys->KeyDown(message.wParam, message.lParam, &eaten) : keys->KeyUp(message.wParam, message.lParam, &eaten));
        handled = eaten != FALSE;
    }
    if (! handled && down && (GetKeyState(VK_MENU) & 0x8000) == 0)
    {
        const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shift   = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        std::optional<TextClipboardCommand> command;
        if (control && (message.wParam == 'C' || message.wParam == VK_INSERT))
            command = TextClipboardCommand::Copy;
        else if ((control && message.wParam == 'X') || (shift && message.wParam == VK_DELETE))
            command = TextClipboardCommand::Cut;
        else if ((control && message.wParam == 'V') || (shift && message.wParam == VK_INSERT))
            command = TextClipboardCommand::Paste;
        if (command)
        {
            const auto client = _state->client;
            EmbeddedTextInputSnapshot snapshot;
            if (client && client->Read(snapshot) == S_OK && client == _state->client && ! snapshot.state.compositionStartIndex)
            {
                handled = true;
                return Clipboard(*command);
            }
        }
    }
    return S_OK;
}
HRESULT TextInputServices::HandleMessage(UINT message, WPARAM wParam, LPARAM, bool& handled) noexcept
{
    handled = false;
    if (! _state->OnThread())
        return RPC_E_WRONG_THREAD;
    if (message != TextDispatchMessage() || wParam != _state->dispatchCookie)
        return S_FALSE;
    handled            = true;
    _state->lockPosted = false;
    auto store         = _state->store;
    if (! store)
        return S_FALSE;
    const HRESULT hr = DispatchPendingTextStoreLock(store.get());
    if (hr == TS_E_NOLOCK)
        static_cast<void>(_state->ScheduleLock());
    return hr;
}

bool TextInputServices::CancelComposition() noexcept
{
    if (! _state->OnThread() || ! _state->client)
        return false;
    EmbeddedTextInputSnapshot snapshot;
    auto client = _state->client;
    if (client->Read(snapshot) != S_OK || client != _state->client || snapshot.focusId != _state->focusId || ! snapshot.state.compositionStartIndex)
        return false;
    // Clearing disconnects the store before the service is told to end its composition.
    _state->Clear();
    return true;
}
} // namespace DxUi
