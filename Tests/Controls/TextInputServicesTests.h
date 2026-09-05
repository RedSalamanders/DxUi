// Included after NativeTextStoreTestSink; these tests exercise the production COM store with an application adapter.
class ClientTextRange final : public ITfRangeACP
{
public:
    LONG start = 1, length = 4;
    ULONG references = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) noexcept override
    {
        if (! out)
            return E_POINTER;
        *out = nullptr;
        if (iid != __uuidof(IUnknown) && iid != __uuidof(ITfRange) && iid != __uuidof(ITfRangeACP))
            return E_NOINTERFACE;
        *out = static_cast<ITfRangeACP*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return ++references;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return --references;
    }
    HRESULT STDMETHODCALLTYPE GetExtent(LONG* first, LONG* count) noexcept override
    {
        *first = start;
        *count = length;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetExtent(LONG first, LONG count) noexcept override
    {
        start  = first;
        length = count;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetText(TfEditCookie, DWORD, WCHAR*, ULONG, ULONG*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetText(TfEditCookie, DWORD, const WCHAR*, LONG) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetFormattedText(TfEditCookie, IDataObject**) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetEmbedded(TfEditCookie, REFGUID, REFIID, IUnknown**) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE InsertEmbedded(TfEditCookie, DWORD, IDataObject*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE ShiftStart(TfEditCookie, LONG, LONG*, const TF_HALTCOND*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE ShiftEnd(TfEditCookie, LONG, LONG*, const TF_HALTCOND*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE ShiftStartToRange(TfEditCookie, ITfRange*, TfAnchor) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE ShiftEndToRange(TfEditCookie, ITfRange*, TfAnchor) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE ShiftStartRegion(TfEditCookie, TfShiftDir, BOOL*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE ShiftEndRegion(TfEditCookie, TfShiftDir, BOOL*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE IsEmpty(TfEditCookie, BOOL*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Collapse(TfEditCookie, TfAnchor) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE IsEqualStart(TfEditCookie, ITfRange*, TfAnchor, BOOL*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE IsEqualEnd(TfEditCookie, ITfRange*, TfAnchor, BOOL*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE CompareStart(TfEditCookie, ITfRange*, TfAnchor, LONG*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE CompareEnd(TfEditCookie, ITfRange*, TfAnchor, LONG*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE AdjustForInsert(TfEditCookie, ULONG, BOOL*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetGravity(TfGravity*, TfGravity*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetGravity(TfEditCookie, TfGravity, TfGravity) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Clone(ITfRange**) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetContext(ITfContext**) noexcept override
    {
        return E_NOTIMPL;
    }
};

class ClientComposition final : public ITfCompositionView
{
public:
    ClientTextRange range;
    ULONG references = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) noexcept override
    {
        if (! out)
            return E_POINTER;
        *out = nullptr;
        if (iid != __uuidof(IUnknown) && iid != __uuidof(ITfCompositionView))
            return E_NOINTERFACE;
        *out = static_cast<ITfCompositionView*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return ++references;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return --references;
    }
    HRESULT STDMETHODCALLTYPE GetOwnerClsid(CLSID* clsid) noexcept override
    {
        *clsid = GUID_NULL;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetRange(ITfRange** out) noexcept override
    {
        *out = &range;
        range.AddRef();
        return S_OK;
    }
};

class TestTextClient final : public DxUi::TextInputClient
{
public:
    DxUi::EmbeddedTextInputSnapshot snapshot;
    std::optional<DxUi::NativeTextInputState> base;
    size_t commits = 0, previews = 0, cancellations = 0;
    std::function<void(const DxUi::NativeTextInputState&)> onApply;
    TestTextClient()
    {
        snapshot.focusId          = 1;
        snapshot.revision         = 1;
        snapshot.state.text       = L"ab";
        snapshot.state.caretIndex = 1;
    }
    HRESULT Read(DxUi::EmbeddedTextInputSnapshot& result) noexcept override
    {
        try
        {
            result = snapshot;
            return S_OK;
        }
        catch (const std::bad_alloc&)
        {
            result = {};
            return E_OUTOFMEMORY;
        }
    }
    HRESULT Apply(uint64_t revision, const DxUi::NativeTextInputState& state, DxUi::EmbeddedTextInputAction action) noexcept override
    {
        if (revision != snapshot.revision)
            return HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH);
        try
        {
            if (action == DxUi::EmbeddedTextInputAction::Preview)
            {
                if (! base)
                    base = snapshot.state;
                ++previews;
            }
            else
            {
                ++commits;
                base.reset();
            }
            snapshot.state = state;
            ++snapshot.revision;
            if (onApply)
                onApply(state);
            return S_OK;
        }
        catch (const std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }
    }
    void Cancel() noexcept override
    {
        ++cancellations;
        if (base)
        {
            snapshot.state = std::move(*base);
            base.reset();
            ++snapshot.revision;
        }
    }
    HRESULT HitTest(POINT point, size_t& index) noexcept override
    {
        index = 0;
        if (point.y < 20 || point.y >= 40 || point.x < -100 || point.x > -80)
            return S_FALSE;
        index = static_cast<size_t>((point.x + 100) / 10);
        return S_OK;
    }
    HRESULT RangeBounds(size_t start, size_t end, RECT& rect, bool& clipped) noexcept override
    {
        rect    = {-100 + static_cast<LONG>(start * 10), 20, -100 + static_cast<LONG>(end * 10 + 1), 40};
        clipped = end > 1;
        return S_OK;
    }
    HRESULT ViewportBounds(RECT& rect) noexcept override
    {
        rect = {-100, 20, -80, 40};
        return S_OK;
    }
};

void TestApplicationTextStoreTransactions()
{
    using namespace DxUi;
    auto client = std::make_shared<TestTextClient>();
    // Declare fake COM objects before the store so held references are released first.
    ClientComposition composition;
    NativeTextStoreTestSink sink;
    wil::com_ptr_nothrow<ITextStoreACP> store;
    store.attach(CreateClientTextStore(nullptr, client));
    Require(store != nullptr, "application adapter creates the shared COM text store without any renderer HWND");
    RequireSucceeded(store->AdviseSink(__uuidof(ITextStoreACPSink), &sink, TS_AS_TEXT_CHANGE | TS_AS_SEL_CHANGE | TS_AS_LAYOUT_CHANGE),
                     "application text sink advised");
    wil::com_ptr_nothrow<ITfContextOwnerCompositionSink> compositions;
    RequireSucceeded(store.query_to(compositions.put()), "application store supports TSF composition callbacks");
    auto runLock = [&](const std::function<HRESULT(DWORD)>& fn, DWORD flags = TS_LF_READWRITE)
    {
        sink.onLockGranted = fn;
        HRESULT session    = E_UNEXPECTED;
        RequireSucceeded(store->RequestLock(flags, &session), "application text store grants a bounded lock");
        return session;
    };
    RequireSucceeded(runLock(
                         [&](DWORD) noexcept
    {
        LONG first = 0, last = 0;
        TS_TEXTCHANGE change{};
        RETURN_IF_FAILED(store->InsertTextAtSelection(0, L"\u6771\u4eac\U0001f600", 4, &first, &last, &change));
        Require(client->previews == 0 && client->commits == 0 && client->snapshot.state.text == L"ab",
                "TSF insertion stays private until composition ordering is known");
        BOOL accepted = FALSE;
        RETURN_IF_FAILED(compositions->OnStartComposition(&composition, &accepted));
        Require(accepted && first == 1 && last == 5, "composition starts after initial insertion and retains UTF-16 ACP indices");
        return S_OK;
    }),
                     "initial composition transaction succeeds");
    Require(client->previews == 1 && client->commits == 0 && client->snapshot.state.text == L"a\u6771\u4eac\U0001f600b" &&
                client->snapshot.state.compositionStartIndex == 1 && client->snapshot.state.compositionEndIndex == 5,
            "initial IME text is one preview and never a model commit");
    Require(sink.textChangeCount == 0 && sink.selectionChangeCount == 0 && sink.layoutChangeCount == 0,
            "the store does not echo TSF-originated edits back to the TSF sink");

    RequireSucceeded(runLock(
                         [&](DWORD) noexcept
    {
        TS_TEXTCHANGE change{};
        RETURN_IF_FAILED(store->SetText(0, 1, 5, L"\u65e5\u672c", 2, &change));
        // TSF supplies the new range before updating the composition view's old range.
        ClientTextRange newRange;
        newRange.length = 2;
        return compositions->OnUpdateComposition(&composition, &newRange);
    }),
                     "new composition range is accepted before its view is updated");
    composition.range.length = 2;
    Require(client->previews == 2 && client->commits == 0 && client->snapshot.state.text == L"a\u65e5\u672cb" &&
                client->snapshot.state.compositionEndIndex == 3,
            "updated composition preserves its original pre-edit base and new underline range");
    RequireSucceeded(runLock([&](DWORD) noexcept { return compositions->OnEndComposition(&composition); }), "composition ends in a write transaction");
    Require(client->commits == 1 && ! client->snapshot.state.compositionStartIndex && ! client->base,
            "ending composition commits exactly once even when text equals the last preview");
    Require(sink.editTransactionStartCount == 3 && sink.editTransactionEndCount == 3 && sink.editTransactionDepth == 0,
            "application composition balances all edit transactions");

    // Callback disconnect must preserve the borrowed edit argument until its callback returns.
    bool argumentSurvived = false;
    client->onApply       = [&](const NativeTextInputState& state)
    {
        DisconnectNativeTextInputTextStore(store.get());
        argumentSurvived = state.text == L"final";
    };
    const HRESULT disconnected = runLock([&](DWORD) noexcept
    {
        TS_TEXTCHANGE change{};
        return store->SetText(0, 0, 4, L"final", 5, &change);
    });
    Require(FAILED(disconnected) && argumentSurvived && sink.editTransactionDepth == 0,
            "callback disconnection is safe and never reports a successful continuation");
    Require(FAILED(compositions->OnEndComposition(&composition)), "late TSF completion cannot access a disconnected application");
}

void TestApplicationTextStoreFailuresAndGeometry()
{
    using namespace DxUi;
    auto client = std::make_shared<TestTextClient>();
    NativeTextStoreTestSink sink;
    wil::com_ptr_nothrow<ITextStoreACP> store;
    store.attach(CreateClientTextStore(nullptr, client));
    RequireSucceeded(store->AdviseSink(__uuidof(ITextStoreACPSink), &sink, TS_AS_TEXT_CHANGE | TS_AS_SEL_CHANGE | TS_AS_LAYOUT_CHANGE),
                     "failure-test sink advised");
    HRESULT session    = E_UNEXPECTED;
    sink.onLockGranted = [&](DWORD) noexcept
    {
        TS_TEXTCHANGE change{};
        RETURN_IF_FAILED(store->SetText(0, 0, 2, L"stale", 5, &change));
        client->snapshot.state.text = L"newer";
        ++client->snapshot.revision;
        return S_OK;
    };
    RequireSucceeded(store->RequestLock(TS_LF_READWRITE, &session), "stale test obtains lock");
    Require(session == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH) && client->snapshot.state.text == L"newer" && client->commits == 0,
            "a changed application revision rejects the whole staged edit without retrying against newer text");
    NotifyTextStoreChanged(store.get());
    client->snapshot.state.text       = L"ab";
    client->snapshot.state.caretIndex = 1;
    client->snapshot.state.readOnly   = true;
    ++client->snapshot.revision;
    sink.onLockGranted = [&](DWORD) noexcept
    {
        TS_TEXTCHANGE change{};
        Require(store->SetText(0, 0, 2, L"bad", 3, &change) == E_ACCESSDENIED, "read-only application text rejects mutation");
        const TS_SELECTION_ACP selection{0, 2, {TS_AE_START, FALSE}};
        RETURN_IF_FAILED(store->SetSelection(1, &selection));
        RECT rect{};
        BOOL clipped = FALSE;
        LONG acp     = -1;
        const POINT point{-90, 30};
        RETURN_IF_FAILED(store->GetACPFromPoint(1, &point, 0, &acp));
        Require(acp == 1, "screen hit testing preserves a monitor's negative origin");
        RETURN_IF_FAILED(store->GetTextExt(1, 0, 2, &rect, &clipped));
        Require(rect.left == -100 && rect.right == -79 && clipped, "TSF geometry uses application screen pixels and clipping flag exactly once");
        Require(store->GetTextExt(1, -1, 2, &rect, &clipped) == TS_E_INVALIDPOS && rect.left == 0 && clipped, "invalid ACP geometry is rejected and cleared");
        return S_OK;
    };
    RequireSucceeded(store->RequestLock(TS_LF_READWRITE, &session), "selection test obtains lock");
    RequireSucceeded(session, "read-only text still permits selection");
    Require(client->snapshot.state.caretIndex == 0 && client->snapshot.state.selectionAnchorIndex == 2 && client->snapshot.state.readOnly,
            "backward selection keeps the active start and authoritative read-only policy");
    Require(sink.textChangeCount == 2, "only separately observed external text replacements notify the sink");

    client->snapshot.state.readOnly = false;
    sink.onLockGranted              = [&](DWORD) noexcept
    {
        TS_TEXTCHANGE change{};
        RETURN_IF_FAILED(store->SetText(0, 0, 2, L"old", 3, &change));
        ++client->snapshot.focusId;
        client->snapshot.state.text = L"fresh";
        ++client->snapshot.revision;
        return S_OK;
    };
    RequireSucceeded(store->RequestLock(TS_LF_READWRITE, &session), "focus-replacement test obtains a lock");
    Require(FAILED(session) && client->snapshot.state.text == L"fresh",
            "a focus change rejects a staged edit even when a later client read has a fresh revision");
    // Restore this test client's identity only to continue independent malformed-snapshot tests.
    --client->snapshot.focusId;
    client->snapshot.state.text.assign(65536, L'x');
    client->snapshot.state.caretIndex = 65536;
    client->snapshot.state.selectionAnchorIndex.reset();
    ++client->snapshot.revision;
    sink.onLockGranted = [&](DWORD) noexcept
    {
        LONG first = 0, last = 0;
        TS_TEXTCHANGE change{};
        Require(store->InsertTextAtSelection(0, L"x", 1, &first, &last, &change) == HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW),
                "insertion rejects a document beyond the bound before allocating");
        return S_OK;
    };
    RequireSucceeded(store->RequestLock(TS_LF_READWRITE, &session), "capacity test obtains lock");
    RequireSucceeded(session, "rejected insertion leaves a successful unchanged transaction");
    Require(client->snapshot.state.text.size() == 65536, "text is not truncated on overflow");
    client->snapshot.state.text.push_back(L'x');
    ++client->snapshot.revision;
    RequireSucceeded(store->RequestLock(TS_LF_READ, &session), "invalid application snapshot is reported as a session failure");
    Require(FAILED(session), "oversized application snapshots cannot enter the text store");
    RequireSucceeded(store->UnadviseSink(&sink), "failure-test sink unadvised");
}

void TestApplicationTextServiceLifecycle()
{
    using namespace DxUi;
    const auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
    AttachedHostWindow window;
    TextInputServices services;
    Require(services.Attach(nullptr) == E_INVALIDARG, "application text services require a valid application HWND");
    RequireSucceeded(services.Attach(window.Hwnd()), "application text services borrow the existing HWND");
    auto client = std::make_shared<TestTextClient>();
    RequireSucceeded(services.SetClient(client), "application text services associate their real TSF context");
    Require(services.HasClient(), "application text service retains one focused client");
    bool handled = true;
    MSG message{.hwnd = window.Hwnd(), .message = WM_PAINT};
    Require(SUCCEEDED(services.PreTranslate(message, handled)) && ! handled, "unrelated paint is never consumed by the text service");
    HRESULT wrongThread = S_OK;
    std::thread worker([&] { wrongThread = services.SetClient(nullptr); });
    worker.join();
    Require(wrongThread == RPC_E_WRONG_THREAD && services.HasClient(), "a wrong-thread call cannot disconnect UI text services");
    services.ClearClient();
    services.ClearClient();
    Require(! services.HasClient() && client->cancellations == 1, "client removal cancels once and safely detaches the TSF document");
    services.Detach();
    services.Detach();
}

void TestApplicationClipboardCommands()
{
    using namespace DxUi;
    const auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
    AttachedHostWindow window;
    TextInputServices services;
    RequireSucceeded(services.Attach(window.Hwnd()), "clipboard service borrows application HWND");
    auto client = std::make_shared<TestTextClient>();
    RequireSucceeded(services.SetClient(client), "clipboard client attaches");
    class Clipboard final : public TextClipboard
    {
    public:
        std::wstring text = L"sentinel";
        size_t reads = 0, writes = 0;
        HRESULT readResult = S_OK, writeResult = S_OK;
        std::function<void()> afterRead;
        HRESULT Read(HWND, std::wstring& out) noexcept override
        {
            ++reads;
            out.clear();
            if (readResult != S_OK)
                return readResult;
            try
            {
                out = text;
                if (afterRead)
                    afterRead();
                return S_OK;
            }
            catch (const std::bad_alloc&)
            {
                return E_OUTOFMEMORY;
            }
        }
        HRESULT Write(HWND, std::wstring_view value) noexcept override
        {
            ++writes;
            if (writeResult != S_OK)
                return writeResult;
            try
            {
                text = value;
                return S_OK;
            }
            catch (const std::bad_alloc&)
            {
                return E_OUTOFMEMORY;
            }
        }
    } clipboard;
    Require(services.Clipboard(TextClipboardCommand::Copy, &clipboard) == S_FALSE && clipboard.writes == 0, "copy without selection preserves clipboard");
    client->snapshot.state.selectionAnchorIndex = 0;
    client->snapshot.state.masked               = true;
    Require(services.Clipboard(TextClipboardCommand::Copy, &clipboard) == E_ACCESSDENIED && clipboard.writes == 0, "concealed text cannot reach the clipboard");
    client->snapshot.state.masked   = false;
    client->snapshot.state.readOnly = true;
    RequireSucceeded(services.Clipboard(TextClipboardCommand::Copy, &clipboard), "read-only selection can be copied");
    Require(clipboard.text == L"a" && client->commits == 0, "copy transfers exactly the logical selection without editing");
    Require(services.Clipboard(TextClipboardCommand::Cut, &clipboard) == E_ACCESSDENIED &&
                services.Clipboard(TextClipboardCommand::Paste, &clipboard) == E_ACCESSDENIED && clipboard.reads == 0 && clipboard.writes == 1,
            "read-only cut/paste fail before accessing the clipboard");
    client->snapshot.state.readOnly = false;
    clipboard.writeResult           = E_ACCESSDENIED;
    Require(services.Clipboard(TextClipboardCommand::Cut, &clipboard) == E_ACCESSDENIED && client->snapshot.state.text == L"ab" && client->commits == 0,
            "failed copy prevents cut from deleting text");
    clipboard.writeResult = S_OK;
    RequireSucceeded(services.Clipboard(TextClipboardCommand::Cut, &clipboard), "cut copies then commits once");
    Require(client->snapshot.state.text == L"b" && client->commits == 1 && clipboard.text == L"a", "cut modifies only the selected range");
    clipboard.text = L"\u6771\u4eac\U0001f600\r\nX\t";
    RequireSucceeded(services.Clipboard(TextClipboardCommand::Paste, &clipboard), "Unicode paste normalizes single-line separators");
    Require(client->snapshot.state.text == L"\u6771\u4eac\U0001f600 X b" && client->commits == 2, "Unicode paste is one edit, preserving the surrogate pair");
    client->snapshot.state.multiline            = true;
    client->snapshot.state.selectionAnchorIndex = 0;
    clipboard.text                              = L"A\r\nB\rC\n";
    RequireSucceeded(services.Clipboard(TextClipboardCommand::Paste, &clipboard), "multiline paste preserves normalized logical newlines");
    Require(client->snapshot.state.text == L"A\nB\nC\nb", "multiline paste replaces the selected UTF-16 range");
    const auto before = client->snapshot.state.text;
    clipboard.text.assign(65537, L'x');
    Require(FAILED(services.Clipboard(TextClipboardCommand::Paste, &clipboard)) && client->snapshot.state.text == before,
            "oversized clipboard input leaves the document unchanged");
    clipboard.text      = L"late";
    clipboard.afterRead = [&]
    {
        client->snapshot.state.text = L"newer";
        ++client->snapshot.revision;
    };
    Require(services.Clipboard(TextClipboardCommand::Paste, &clipboard) == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH) &&
                client->snapshot.state.text == L"newer",
            "clipboard reentrancy cannot overwrite a newer document revision");
    services.Detach();
}

void TestBoundedClipboardDecode()
{
    using namespace DxUi;
    std::wstring text = L"stale";
    const std::array<wchar_t, 2> noTerminator{L'a', L'b'};
    Require(FAILED(DecodeClipboardText(noTerminator, text)) && text.empty(), "unterminated clipboard memory is rejected without reading beyond its allocation");
    const std::array<wchar_t, 2> highSurrogate{wchar_t(0xd800), 0};
    const std::array<wchar_t, 2> lowSurrogate{wchar_t(0xdc00), 0};
    Require(FAILED(DecodeClipboardText(highSurrogate, text)) && FAILED(DecodeClipboardText(lowSurrogate, text)),
            "malformed clipboard surrogate sequences cannot enter the editor");
    const std::array<wchar_t, 5> unicode{L'\u6771', wchar_t(0xd83d), wchar_t(0xde00), 0, L'z'};
    RequireSucceeded(DecodeClipboardText(unicode, text), "bounded clipboard UTF-16 accepts a complete supplementary character");
    Require(text == L"\u6771\U0001f600", "decoding stops at the first terminator, ignoring allocation padding");
    std::vector<wchar_t> maximum(65537, L'x');
    maximum.back() = 0;
    RequireSucceeded(DecodeClipboardText(maximum, text), "clipboard decoder accepts exactly its documented text capacity");
    Require(text.size() == 65536, "maximum clipboard text is not truncated");
    maximum.back() = L'x';
    Require(DecodeClipboardText(maximum, text) == HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW) && text.empty(),
            "clipboard decoder rejects an unterminated oversized document");
}

void TestApplicationTextStoreDeferredLocks()
{
    using namespace DxUi;
    auto client = std::make_shared<TestTextClient>();
    struct Dispatch final
    {
        bool queued    = false;
        unsigned posts = 0;
    } dispatch;
    NativeTextStoreTestSink sink;
    wil::com_ptr_nothrow<ITextStoreACP> store;
    store.attach(CreateClientTextStore(nullptr,
                                       client,
                                       {&dispatch,
                                        [](void* context) noexcept
    {
        auto& queue = *static_cast<Dispatch*>(context);
        if (! queue.queued)
        {
            queue.queued = true;
            ++queue.posts;
        }
        return true;
    }}));
    Require(store != nullptr, "deferred-lock fixture creates an application text store");
    RequireSucceeded(store->AdviseSink(__uuidof(ITextStoreACPSink), &sink, TS_AS_TEXT_CHANGE), "deferred-lock sink advised");
    unsigned grants    = 0;
    sink.onLockGranted = [&](DWORD flags) noexcept
    {
        ++grants;
        if (grants == 1)
        {
            HRESULT session = S_OK;
            RETURN_IF_FAILED(store->RequestLock(TS_LF_READWRITE | TS_LF_SYNC, &session));
            Require(session == TS_E_SYNCHRONOUS, "nested synchronous locks are rejected without recursion");
            RETURN_IF_FAILED(store->RequestLock(TS_LF_READ, &session));
            Require(session == TS_S_ASYNC, "a nested asynchronous read lock is deferred");
            RETURN_IF_FAILED(store->RequestLock(TS_LF_READWRITE, &session));
            Require(session == TS_S_ASYNC && grants == 1, "a deferred write upgrade coalesces without running inside the current callback");
        }
        else
            Require((flags & TS_LF_READWRITE) == TS_LF_READWRITE, "deferred lock retains the strongest requested access");
        return S_OK;
    };
    HRESULT session = E_UNEXPECTED;
    RequireSucceeded(store->RequestLock(TS_LF_READ, &session), "initial read lock requested");
    RequireSucceeded(session, "initial read lock completes");
    Require(grants == 1 && dispatch.posts == 1 && dispatch.queued, "one event represents all deferred lock requests");
    dispatch.queued = false;
    RequireSucceeded(DispatchPendingTextStoreLock(store.get()), "application message grants the coalesced lock");
    Require(grants == 2 && DispatchPendingTextStoreLock(store.get()) == S_FALSE, "drained locks do not repeat on an idle message");
    Require(sink.editTransactionStartCount == 1 && sink.editTransactionEndCount == 1 && sink.editTransactionDepth == 0,
            "the deferred write lock has one balanced transaction");
    grants = 0;
    RequireSucceeded(store->RequestLock(TS_LF_READ, &session), "another lock queues before detach");
    DisconnectNativeTextInputTextStore(store.get());
    Require(DispatchPendingTextStoreLock(store.get()) == S_FALSE && grants == 1, "disconnect discards pending locks before a late application message");
}

void TestApplicationCompositionCancelAfterFocusReplacement()
{
    using namespace DxUi;
    auto client = std::make_shared<TestTextClient>();
    ClientComposition composition;
    NativeTextStoreTestSink sink;
    wil::com_ptr_nothrow<ITextStoreACP> store;
    store.attach(CreateClientTextStore(nullptr, client));
    wil::com_ptr_nothrow<ITfContextOwnerCompositionSink> composing;
    RequireSucceeded(store.query_to(composing.put()), "cancel fixture obtains composition sink");
    RequireSucceeded(store->AdviseSink(__uuidof(ITextStoreACPSink), &sink, 0), "cancel fixture advises lock sink");
    sink.onLockGranted = [&](DWORD) noexcept
    {
        LONG first = 0, last = 0;
        TS_TEXTCHANGE change{};
        RETURN_IF_FAILED(store->InsertTextAtSelection(0, L"ime!", 4, &first, &last, &change));
        BOOL accepted = FALSE;
        return composing->OnStartComposition(&composition, &accepted);
    };
    HRESULT session = E_UNEXPECTED;
    RequireSucceeded(store->RequestLock(TS_LF_READWRITE, &session), "cancel fixture starts composition");
    RequireSucceeded(session, "cancel fixture previews successfully");
    Require(client->previews == 1 && client->commits == 0, "cancel fixture holds only a preview");
    DisconnectNativeTextInputTextStore(store.get());
    Require(client->snapshot.state.text == L"ab" && client->commits == 0 && client->cancellations == 1,
            "disconnect restores the pre-composition text without committing");
    Require(FAILED(composing->OnEndComposition(&composition)), "late completion cannot commit after cancellation");

    // A stale client need not be well behaved: the adapter verifies focus before calling its Cancel method.
    auto changed = std::make_shared<TestTextClient>();
    wil::com_ptr_nothrow<ITextStoreACP> oldStore;
    oldStore.attach(CreateClientTextStore(nullptr, changed));
    changed->snapshot.focusId    = 2;
    changed->snapshot.state.text = L"new focus";
    DisconnectNativeTextInputTextStore(oldStore.get());
    Require(changed->cancellations == 0 && changed->snapshot.state.text == L"new focus", "disconnect never forwards cancellation to a replacement focus");
}

void TestApplicationTextStoreInsertionAndLayout()
{
    using namespace DxUi;
    auto client = std::make_shared<TestTextClient>();
    NativeTextStoreTestSink sink;
    wil::com_ptr_nothrow<ITextStoreACP> store;
    store.attach(CreateClientTextStore(nullptr, client));
    RequireSucceeded(store->AdviseSink(__uuidof(ITextStoreACPSink), &sink, TS_AS_LAYOUT_CHANGE | TS_AS_TEXT_CHANGE), "geometry sink advised");
    sink.onLockGranted = [&](DWORD) noexcept
    {
        LONG first = 99, last = 99;
        TS_TEXTCHANGE change{};
        Require(store->InsertTextAtSelection(TS_IAS_NOQUERY | TS_IAS_QUERYONLY, L"x", 1, &first, &last, &change) == E_INVALIDARG,
                "contradictory insertion flags rejected");
        RETURN_IF_FAILED(store->InsertTextAtSelection(TS_IAS_QUERYONLY, L"x", 1, &first, &last, nullptr));
        Require(first == 1 && last == 2 && client->snapshot.state.text == L"ab", "query-only predicts insertion without editing");
        RETURN_IF_FAILED(store->InsertTextAtSelection(TS_IAS_NOQUERY, L"x", 1, nullptr, nullptr, &change));
        RECT bounds{1, 2, 3, 4};
        BOOL clipped = FALSE;
        LONG index   = 99;
        POINT point{-90, 30};
        Require(store->GetTextExt(1, 0, 2, &bounds, &clipped) == TS_E_NOLAYOUT && bounds.left == 0 && clipped, "staged text cannot return old layout geometry");
        Require(store->GetACPFromPoint(1, &point, 0, &index) == TS_E_NOLAYOUT && index == 0, "point query waits for staged text layout");
        Require(change.acpStart == 1 && change.acpOldEnd == 1 && change.acpNewEnd == 2, "NOQUERY accepts absent ACP outputs and returns its edit description");
        return S_OK;
    };
    HRESULT session = E_UNEXPECTED;
    RequireSucceeded(store->RequestLock(TS_LF_READWRITE, &session), "insertion lock granted");
    RequireSucceeded(session, "insertion transaction completed");
    Require(client->snapshot.state.text == L"axb" && client->commits == 1, "NOQUERY commits once after the lock");
    Require(sink.layoutChangeCount == 0, "TSF edit does not echo layout before preparation");
    NotifyTextStoreLayoutChanged(store.get());
    Require(sink.layoutChangeCount == 1, "prepared layout explicitly notifies TSF after NOLAYOUT");
    client->snapshot.state.text.assign(65536, L'x');
    client->snapshot.state.caretIndex = 65536;
    client->snapshot.state.selectionAnchorIndex.reset();
    ++client->snapshot.revision;
    sink.onLockGranted = [&](DWORD) noexcept
    {
        LONG first = 99, last = 99;
        Require(store->InsertTextAtSelection(TS_IAS_QUERYONLY, L"x", 1, &first, &last, nullptr) == HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW) && first == 0 &&
                    last == 0,
                "query-only obeys the same bound as insertion");
        return S_OK;
    };
    RequireSucceeded(store->RequestLock(TS_LF_READWRITE, &session), "capacity-query lock granted");
    RequireSucceeded(session, "capacity rejection leaves document unchanged");
    client->snapshot.state.text       = L"External update";
    client->snapshot.state.caretIndex = 0;
    ++client->snapshot.revision;
    bool released     = false;
    sink.onTextChange = [&](const TS_TEXTCHANGE*) noexcept
    {
        static_cast<void>(store->UnadviseSink(&sink));
        store.reset();
        released = true;
        return S_OK;
    };
    NotifyTextStoreChanged(store.get());
    Require(released && ! store, "notification safely survives releasing the application's final store reference");
}
