#include "../Support/WindowMessages.h"
#include "DxUi.Internal.h"
#include "TextClipboard.h"
#include "TextStoreTarget.h"
#include "TsfFocus.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <new>
#include <utility>

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
        auto client                    = std::move(_client);
        const auto dispatchOwner       = _dispatch.owner;
        const auto dispatchContext     = _dispatch.context;
        const auto ownsClientSuccessor = _dispatch.ownsClientSuccessor;
        _hwnd                          = nullptr;
        _editing                       = false;
        _dirty                         = false;
        _composition.reset();
        _snapshot = {};
        _dispatch = {};
        if (client)
        {
            const auto successorOwnsClient = [&]() noexcept { return ownsClientSuccessor && ownsClientSuccessor(dispatchContext, client.get(), _focusId); };
            static_cast<void>(dispatchOwner);
            if (successorOwnsClient())
                return;
            EmbeddedTextInputSnapshot current;
            if (client->Read(current) == S_OK && current.focusId == _focusId && ! successorOwnsClient())
                client->Cancel();
        }
    }
    HWND GetHwnd() const noexcept override
    {
        return Available() ? _hwnd : nullptr;
    }
    bool ScheduleLock() noexcept override
    {
        const auto owner       = _dispatch.owner;
        const auto context     = _dispatch.context;
        const auto requestLock = _dispatch.requestLock;
        static_cast<void>(owner);
        return Available() && requestLock && requestLock(context);
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
            if (source->masked)
                return false;
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
        if (! Available() || ! _editing || ! _writable || _snapshot.state.masked || state.masked || state.text.size() > kMaximumTextUnits ||
            state.caretIndex > state.text.size() || (state.selectionAnchorIndex && *state.selectionAnchorIndex > state.text.size()) ||
            (_snapshot.state.readOnly && state.text != _snapshot.state.text))
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
        if (current.state.masked)
            return E_ACCESSDENIED;
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
        if (SUCCEEDED(hr) && _snapshot.state.masked)
            hr = E_ACCESSDENIED;
        else if (SUCCEEDED(hr) && (end > _snapshot.state.text.size() || _snapshot.state.readOnly))
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
    return CreateClientTextStoreForFocus(hwnd, std::move(client), snapshot.focusId, std::move(dispatch));
}

ITextStoreACP* CreateClientTextStoreForFocus(HWND hwnd, std::shared_ptr<TextInputClient> client, uint64_t focusId, TextStoreDispatch dispatch) noexcept
{
    if (! client || ! focusId)
        return nullptr;
    try
    {
        return CreateTextStore(std::make_shared<ClientTextStoreTarget>(hwnd, std::move(client), focusId, std::move(dispatch)));
    }
    catch (const std::bad_alloc&)
    {
        return nullptr;
    }
}

HRESULT DispatchDeferredTextStoreLock(ITextStoreACP* store, bool& messagePosted) noexcept
{
    messagePosted = false;
    return DispatchPendingTextStoreLock(store);
}

struct TextInputServices::State
{
    struct DispatchContext final
    {
        State* state    = nullptr;
        UINT_PTR cookie = 0;
    };

    HWND hwnd    = nullptr;
    DWORD thread = GetCurrentThreadId();
    std::shared_ptr<TextInputClient> client;
    wil::com_ptr_nothrow<ITfThreadMgr> manager;
    wil::com_ptr_nothrow<ITfDocumentMgr> document, previousDocument;
    wil::com_ptr_nothrow<ITfContext> context;
    wil::com_ptr_nothrow<ITextStoreACP> store;
    TfClientId clientId        = 0;
    bool associated            = false;
    uint64_t focusId           = 0;
    UINT_PTR dispatchCookie    = NextDispatchCookie();
    uint64_t operationRevision = 0;
    bool lockPosted            = false;
    bool detaching             = false;

    uint64_t BeginOperation() noexcept
    {
        if (++operationRevision == 0u)
            ++operationRevision;
        dispatchCookie = NextDispatchCookie();
        lockPosted     = false;
        return operationRevision;
    }
    bool IsCurrent(uint64_t revision) const noexcept
    {
        return operationRevision == revision;
    }
    bool ScheduleLock() noexcept
    {
        if (lockPosted)
            return true;
        // Without its registered message no lock is posted, as when the post fails.
        const WndMsg::RegisteredMessage lockMessage = WndMsg::TextInputServicesDeferredLock();
        if (! hwnd || ! lockMessage)
            return false;
        lockPosted = PostMessageW(hwnd, lockMessage.value, dispatchCookie, 0) != FALSE;
        return lockPosted;
    }
    bool ScheduleLock(UINT_PTR cookie) noexcept
    {
        return ! detaching && cookie == dispatchCookie && ScheduleLock();
    }

    bool OnThread() const noexcept
    {
        return thread == GetCurrentThreadId();
    }
    void Clear(uint64_t revision, HWND retiredHwnd) noexcept
    {
        // Publish the empty state before application callbacks can install a successor.
        auto oldStore            = std::move(store);
        auto oldClient           = std::move(client);
        auto oldDocument         = std::move(document);
        auto oldContext          = std::move(context);
        auto oldPreviousDocument = std::move(previousDocument);
        auto oldManager          = manager;
        const bool wasAssociated = std::exchange(associated, false);
        dispatchCookie           = NextDispatchCookie();
        lockPosted               = false;
        focusId                  = 0;
        if (oldStore)
            DisconnectNativeTextInputTextStore(oldStore.get());
        oldStore.reset();
        if (oldManager && wasAssociated)
        {
            Internal::ClearThreadManagerFocusIfOwnedBy(oldManager.get(), oldDocument.get());
            if (retiredHwnd && IsWindow(retiredHwnd))
            {
                Internal::RestoreThreadManagerFocusAssociation(
                    oldManager.get(), retiredHwnd, oldDocument.get(), oldPreviousDocument.get(), [this, revision]() noexcept { return IsCurrent(revision); });
            }
        }
        if (oldDocument)
            static_cast<void>(oldDocument->Pop(TF_POPF_ALL));
        oldContext.reset();
        oldDocument.reset();
        oldPreviousDocument.reset();
        oldClient.reset();
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
    if (_state->detaching)
        return E_UNEXPECTED;
    if (! hwnd || GetWindowThreadProcessId(hwnd, nullptr) != GetCurrentThreadId())
        return E_INVALIDARG;
    if (_state->hwnd == hwnd)
        return S_OK;
    // Without its registered message the service could never grant a deferred lock. The registration failed the first
    // time the message was asked for, perhaps long before this call, so GetLastError no longer describes that failure
    // and can even read ERROR_SUCCESS, which HRESULT_FROM_WIN32 would turn into S_OK.
    if (! WndMsg::TextInputServicesDeferredLock())
        return E_FAIL;
    Detach();
    if (_state->detaching || _state->hwnd)
        return E_UNEXPECTED;
    static_cast<void>(_state->BeginOperation());
    _state->hwnd = hwnd;
    return S_OK;
}
void TextInputServices::Detach() noexcept
{
    if (! _state->OnThread() || _state->detaching)
        return;
    const uint64_t revision = _state->BeginOperation();
    const HWND retiredHwnd  = std::exchange(_state->hwnd, nullptr);
    _state->detaching       = true;
    _state->Clear(revision, retiredHwnd);
    if (! _state->IsCurrent(revision))
    {
        _state->detaching = false;
        return;
    }
    if (_state->manager && _state->clientId)
        static_cast<void>(_state->manager->Deactivate());
    _state->clientId = 0;
    _state->manager.reset();
    _state->detaching = false;
}
void TextInputServices::ClearClient() noexcept
{
    if (! _state->OnThread() || _state->detaching)
        return;
    const uint64_t revision = _state->BeginOperation();
    _state->Clear(revision, _state->hwnd);
}
bool TextInputServices::HasClient() const noexcept
{
    return _state->OnThread() && _state->client != nullptr;
}
HRESULT TextInputServices::SetClient(std::shared_ptr<TextInputClient> client) noexcept
{
    if (! _state->OnThread())
        return RPC_E_WRONG_THREAD;
    if (_state->detaching)
        return E_UNEXPECTED;
    if (! _state->hwnd || ! IsWindow(_state->hwnd))
        return E_UNEXPECTED;
    if (client == _state->client)
        return S_OK;
    const uint64_t revision = _state->BeginOperation();
    const HWND hwnd         = _state->hwnd;
    _state->Clear(revision, hwnd);
    if (! _state->IsCurrent(revision))
        return TF_E_DISCONNECTED;
    if (! client)
        return S_OK;
    EmbeddedTextInputSnapshot snapshot;
    const HRESULT read = client->Read(snapshot);
    if (! _state->IsCurrent(revision))
        return TF_E_DISCONNECTED;
    if (read != S_OK)
        return read;
    if (! snapshot.revision || ! snapshot.focusId || ! ValidState(snapshot.state))
        return E_INVALIDARG;
    if (snapshot.state.masked)
        return E_ACCESSDENIED;
    wil::com_ptr_nothrow<ITfThreadMgr> manager = _state->manager;
    TfClientId clientId                        = _state->clientId;
    if (! manager)
    {
        const HRESULT create = CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(manager.put()));
        if (! _state->IsCurrent(revision))
            return TF_E_DISCONNECTED;
        if (FAILED(create) || ! manager)
            return FAILED(create) ? create : E_FAIL;
        clientId               = 0;
        const HRESULT activate = manager->Activate(&clientId);
        if (! _state->IsCurrent(revision))
        {
            // Balance this successful activation even when a nested request activated the same thread manager.
            // Its separate Activate call owns a separate count and remains active.
            if (SUCCEEDED(activate))
                static_cast<void>(manager->Deactivate());
            return TF_E_DISCONNECTED;
        }
        if (FAILED(activate))
            return activate;
        _state->manager  = manager;
        _state->clientId = clientId;
    }
    wil::com_ptr_nothrow<ITextStoreACP> store;
    wil::com_ptr_nothrow<ITfDocumentMgr> document, previousDocument;
    wil::com_ptr_nothrow<ITfContext> context;
    bool pushed               = false;
    bool associated           = false;
    const auto rollbackStaged = [&]() noexcept
    {
        if (store)
            DisconnectNativeTextInputTextStore(store.get());
        if (manager && associated)
        {
            Internal::ClearThreadManagerFocusIfOwnedBy(manager.get(), document.get());
            if (_state->IsCurrent(revision) && IsWindow(hwnd))
            {
                Internal::RestoreThreadManagerFocusAssociation(
                    manager.get(), hwnd, document.get(), previousDocument.get(), [this, revision]() noexcept { return _state->IsCurrent(revision); });
            }
        }
        if (pushed && document)
            static_cast<void>(document->Pop(TF_POPF_ALL));
    };
    std::shared_ptr<State::DispatchContext> dispatchContext;
    try
    {
        dispatchContext = std::make_shared<State::DispatchContext>(State::DispatchContext{_state.get(), _state->dispatchCookie});
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    TextStoreDispatch dispatch{dispatchContext.get(),
                               [](void* value) noexcept
    {
        auto* const request = static_cast<State::DispatchContext*>(value);
        return request && request->state && request->state->ScheduleLock(request->cookie);
    },
                               dispatchContext,
                               [](void* value, const TextInputClient* client, uint64_t focusId) noexcept
    {
        auto* const request = static_cast<State::DispatchContext*>(value);
        auto* const state   = request ? request->state : nullptr;
        return state && state->dispatchCookie != request->cookie && state->store && state->client.get() == client && state->focusId == focusId;
    }};
    store.attach(CreateClientTextStoreForFocus(hwnd, client, snapshot.focusId, std::move(dispatch)));
    if (! _state->IsCurrent(revision))
        return TF_E_DISCONNECTED;
    if (! store)
        return E_OUTOFMEMORY;
    HRESULT hr = manager->CreateDocumentMgr(document.put());
    if (! _state->IsCurrent(revision))
    {
        DisconnectNativeTextInputTextStore(store.get());
        return TF_E_DISCONNECTED;
    }
    if (FAILED(hr) || ! document)
    {
        DisconnectNativeTextInputTextStore(store.get());
        return FAILED(hr) ? hr : E_FAIL;
    }
    TfEditCookie cookie = 0;
    hr                  = document->CreateContext(clientId, 0, store.get(), context.put(), &cookie);
    if (! _state->IsCurrent(revision))
    {
        DisconnectNativeTextInputTextStore(store.get());
        return TF_E_DISCONNECTED;
    }
    if (FAILED(hr) || ! context)
    {
        DisconnectNativeTextInputTextStore(store.get());
        return FAILED(hr) ? hr : E_FAIL;
    }
    hr     = document->Push(context.get());
    pushed = SUCCEEDED(hr);
    if (! _state->IsCurrent(revision))
    {
        rollbackStaged();
        return TF_E_DISCONNECTED;
    }
    if (FAILED(hr))
    {
        rollbackStaged();
        return hr;
    }
    hr         = manager->AssociateFocus(hwnd, document.get(), previousDocument.put());
    associated = SUCCEEDED(hr);
    if (! _state->IsCurrent(revision))
    {
        rollbackStaged();
        return TF_E_DISCONNECTED;
    }
    if (FAILED(hr))
    {
        rollbackStaged();
        return hr;
    }
    hr = manager->SetFocus(document.get());
    if (! _state->IsCurrent(revision))
    {
        rollbackStaged();
        return TF_E_DISCONNECTED;
    }
    if (FAILED(hr))
    {
        rollbackStaged();
        return hr;
    }
    _state->client           = std::move(client);
    _state->focusId          = snapshot.focusId;
    _state->store            = std::move(store);
    _state->document         = std::move(document);
    _state->context          = std::move(context);
    _state->previousDocument = std::move(previousDocument);
    _state->associated       = associated;
    return S_OK;
}
void TextInputServices::NotifyChanged() noexcept
{
    if (! _state->OnThread() || ! _state->store)
        return;
    EmbeddedTextInputSnapshot snapshot;
    const auto client       = _state->client;
    const auto store        = _state->store;
    const uint64_t revision = _state->operationRevision;
    const uint64_t focusId  = _state->focusId;
    if (client && client->Read(snapshot) == S_OK && _state->IsCurrent(revision) && client == _state->client && store.get() == _state->store.get() &&
        snapshot.focusId == focusId && snapshot.state.masked)
    {
        // A focused editor can become a password field without changing focus identity. Disconnect the TSF store
        // immediately; its target cancels any live preview before the service drops the client.
        ClearClient();
        return;
    }
    if (_state->IsCurrent(revision) && store.get() == _state->store.get())
        NotifyTextStoreChanged(store.get());
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
    if (! WndMsg::TextInputServicesDeferredLock().Matches(message) || wParam != _state->dispatchCookie)
        return S_FALSE;
    handled    = true;
    auto store = _state->store;
    if (! store)
    {
        _state->lockPosted = false;
        return S_FALSE;
    }
    return DispatchDeferredTextStoreLock(store.get(), _state->lockPosted);
}

bool TextInputServices::CancelComposition() noexcept
{
    if (! _state->OnThread() || ! _state->client)
        return false;
    EmbeddedTextInputSnapshot snapshot;
    const auto client       = _state->client;
    const auto store        = _state->store;
    const uint64_t revision = _state->operationRevision;
    const uint64_t focusId  = _state->focusId;
    if (client->Read(snapshot) != S_OK || ! _state->IsCurrent(revision) || client != _state->client || store.get() != _state->store.get() ||
        snapshot.focusId != focusId || ! snapshot.state.compositionStartIndex)
        return false;
    // Clearing disconnects the store before the service is told to end its composition.
    ClearClient();
    return true;
}
} // namespace DxUi
