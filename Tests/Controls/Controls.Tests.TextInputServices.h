#pragma once

#include "Controls.Tests.TextStoreComposition.h"

#include <atomic>

class TestTextClient final : public DxUi::TextInputClient
{
public:
    DxUi::EmbeddedTextInputSnapshot snapshot;
    std::optional<DxUi::NativeTextInputState> base;
    size_t reads = 0, commits = 0, previews = 0, cancellations = 0;
    std::function<void()> onRead, onCancel;
    std::function<void(const DxUi::NativeTextInputState&)> onApply;
    HRESULT applyResult = S_OK;
    TestTextClient()
    {
        snapshot.focusId          = 1;
        snapshot.revision         = 1;
        snapshot.state.text       = L"ab";
        snapshot.state.caretIndex = 1;
    }
    HRESULT Read(DxUi::EmbeddedTextInputSnapshot& result) noexcept override
    {
        ++reads;
        try
        {
            result        = snapshot;
            auto callback = std::move(onRead);
            if (callback)
                callback();
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
            return applyResult;
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
        auto callback = std::move(onCancel);
        if (callback)
            callback();
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
    NotifyTextStoreChanged(store.get());
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

void TestApplicationTextStoreDefersExternalNotificationUntilLockUnwinds()
{
    using namespace DxUi;
    auto client = std::make_shared<TestTextClient>();
    struct Dispatch final
    {
        bool queued    = false;
        unsigned posts = 0u;
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
    Require(store != nullptr, "deferred external-notification fixture creates an application text store");
    RequireSucceeded(store->AdviseSink(__uuidof(ITextStoreACPSink), &sink, TS_AS_TEXT_CHANGE | TS_AS_SEL_CHANGE | TS_AS_LAYOUT_CHANGE),
                     "deferred external-notification sink is advised");

    client->applyResult = HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH);
    client->onApply     = [&](const NativeTextInputState&) noexcept
    {
        client->snapshot.state.text       = L"external";
        client->snapshot.state.caretIndex = client->snapshot.state.text.size();
        client->snapshot.state.selectionAnchorIndex.reset();
        ++client->snapshot.revision;
        NotifyTextStoreChanged(store.get());
    };
    sink.onLockGranted = [&](DWORD) noexcept
    {
        TS_TEXTCHANGE change{};
        return store->SetText(0u, 0, 2, L"after", 5u, &change);
    };

    HRESULT session = E_UNEXPECTED;
    RequireSucceeded(store->RequestLock(TS_LF_READWRITE, &session), "stale application edit returns through RequestLock");
    Require(session == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH) && client->snapshot.state.text == L"external",
            "the newer application replacement rejects the staged TSF continuation");
    Require(sink.textChangeCount == 0u && sink.selectionChangeCount == 0u && sink.layoutChangeCount == 0u,
            "a TSF self-edit is not echoed while its write lock is active");
    Require(dispatch.queued && dispatch.posts == 1u, "the callback-side external change queues one bounded out-of-lock notification");

    sink.onLockGranted     = [](DWORD) noexcept { return S_OK; };
    bool requestedReadLock = false;
    HRESULT nestedRequest  = E_UNEXPECTED;
    HRESULT nestedSession  = E_UNEXPECTED;
    sink.onTextChange      = [&](const TS_TEXTCHANGE*) noexcept
    {
        requestedReadLock = true;
        nestedRequest     = store->RequestLock(TS_LF_READ, &nestedSession);
        return S_OK;
    };
    const HRESULT dispatchResult = DispatchDeferredTextStoreLock(store.get(), dispatch.queued);
    Require(dispatchResult == S_FALSE && ! dispatch.queued, "the posted turn reports external state without inventing another lock request");
    Require(requestedReadLock && SUCCEEDED(nestedRequest) && SUCCEEDED(nestedSession),
            "the external text callback can request a read lock after the failed write lock unwinds");
    Require(sink.textChangeCount == 1u && sink.selectionChangeCount == 1u && sink.layoutChangeCount == 1u,
            "the newer external replacement is reported once after lock release");
    Require(sink.lastTextChange.acpStart == 0 && sink.lastTextChange.acpOldEnd == 2 && sink.lastTextChange.acpNewEnd == 8,
            "deferred external notification compares the committed document with the newer replacement");
    Require(sink.editTransactionStartCount == 1u && sink.editTransactionEndCount == 1u && sink.editTransactionDepth == 0u,
            "deferred notification preserves the failed edit's balanced transaction callbacks");

    client->onApply                   = {};
    client->snapshot.state.text       = L"outside-read";
    client->snapshot.state.caretIndex = client->snapshot.state.text.size();
    ++client->snapshot.revision;
    sink.onTextChange  = {};
    sink.onLockGranted = [&](DWORD) noexcept
    {
        NotifyTextStoreChanged(store.get());
        return S_OK;
    };
    RequireSucceeded(store->RequestLock(TS_LF_READ, &session), "read lock observes an external callback-side replacement");
    RequireSucceeded(session, "external replacement does not fail an otherwise successful read lock");
    Require(sink.textChangeCount == 1u && dispatch.queued, "successful lock capture does not consume an external notification observed during its callback");
    RequireSucceeded(DispatchDeferredTextStoreLock(store.get(), dispatch.queued), "posted turn reports the read-lock external replacement");
    Require(sink.textChangeCount == 2u && sink.lastTextChange.acpOldEnd == 8 && sink.lastTextChange.acpNewEnd == 12,
            "the deferred read-lock notification preserves the pre-callback document as its comparison baseline");

    client->snapshot.state.text       = L"replacement-final";
    client->snapshot.state.caretIndex = client->snapshot.state.text.size();
    ++client->snapshot.revision;
    HRESULT unadviseResult = E_UNEXPECTED;
    HRESULT readviseResult = E_UNEXPECTED;
    bool disconnected      = false;
    sink.onTextChange      = [&](const TS_TEXTCHANGE*) noexcept
    {
        unadviseResult = store->UnadviseSink(&sink);
        if (SUCCEEDED(unadviseResult))
            readviseResult = store->AdviseSink(__uuidof(ITextStoreACPSink), &sink, TS_AS_TEXT_CHANGE | TS_AS_SEL_CHANGE | TS_AS_LAYOUT_CHANGE);
        DisconnectNativeTextInputTextStore(store.get());
        disconnected = true;
        return S_OK;
    };
    sink.onLockGranted = [&](DWORD) noexcept
    {
        NotifyTextStoreChanged(store.get());
        NotifyTextStoreLayoutChanged(store.get());
        return S_OK;
    };
    RequireSucceeded(store->RequestLock(TS_LF_READ, &session), "lock queues external text and prepared-layout notifications together");
    RequireSucceeded(session, "external notification fixture completes its lock");
    Require(dispatch.queued, "coalesced external text and layout changes use one posted turn");
    RequireSucceeded(DispatchDeferredTextStoreLock(store.get(), dispatch.queued), "coalesced external notifications dispatch out of lock");
    Require(disconnected && SUCCEEDED(unadviseResult) && SUCCEEDED(readviseResult),
            "the text callback can replace its sink connection and disconnect the store reentrantly");
    Require(sink.textChangeCount == 3u && sink.selectionChangeCount == 2u && sink.layoutChangeCount == 2u,
            "disconnect stops stale selection and layout notifications from reaching the retired or replacement connection");
    Require(DispatchDeferredTextStoreLock(store.get(), dispatch.queued) == S_FALSE, "disconnect leaves no deferred lock or layout work to deliver");
}

void TestApplicationTextServiceLifecycle()
{
    using namespace DxUi;
    const auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
    AttachedHostWindow window;
    TextInputServices services;
    Require(services.Attach(nullptr) == E_INVALIDARG, "application text services require a valid application HWND");
    RequireSucceeded(services.Attach(window.Hwnd()), "application text services borrow the existing HWND");
    auto maskedClient                   = std::make_shared<TestTextClient>();
    maskedClient->snapshot.state.masked = true;
    Require(services.SetClient(maskedClient) == E_ACCESSDENIED && ! services.HasClient(),
            "application text services refuse to attach a TSF document to a masked field");
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
    client->snapshot.state.masked = true;
    services.NotifyChanged();
    Require(! services.HasClient() && client->cancellations == 1,
            "a focused field that becomes masked disconnects its TSF store and cancels its input service");
    services.ClearClient();
    services.ClearClient();
    Require(! services.HasClient() && client->cancellations == 1, "client removal cancels once and safely detaches the TSF document");
    services.Detach();
    services.Detach();
}

class TextServicesReentrantFocusSink final : public ITfThreadMgrEventSink
{
public:
    std::function<void()> onSetFocus;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) noexcept override
    {
        if (! object)
            return E_POINTER;
        *object = nullptr;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(ITfThreadMgrEventSink))
        {
            *object = static_cast<ITfThreadMgrEventSink*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return _references.fetch_add(1u, std::memory_order_relaxed) + 1u;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return _references.fetch_sub(1u, std::memory_order_acq_rel) - 1u;
    }
    HRESULT STDMETHODCALLTYPE OnInitDocumentMgr(ITfDocumentMgr*) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnUninitDocumentMgr(ITfDocumentMgr*) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnSetFocus(ITfDocumentMgr*, ITfDocumentMgr*) noexcept override
    {
        auto callback = std::move(onSetFocus);
        if (callback)
        {
            try
            {
                callback();
            }
            catch (const std::exception&)
            {
                return E_FAIL;
            }
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPushContext(ITfContext*) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPopContext(ITfContext*) noexcept override
    {
        return S_OK;
    }

private:
    std::atomic<ULONG> _references{1u};
};

bool TestTextServicesSameComIdentity(IUnknown* left, IUnknown* right) noexcept
{
    if (! left || ! right)
        return false;
    wil::com_ptr_nothrow<IUnknown> leftIdentity, rightIdentity;
    return SUCCEEDED(left->QueryInterface(IID_PPV_ARGS(leftIdentity.put()))) && leftIdentity &&
           SUCCEEDED(right->QueryInterface(IID_PPV_ARGS(rightIdentity.put()))) && rightIdentity && leftIdentity.get() == rightIdentity.get();
}

void TestApplicationTextServiceReadReentrancyPreservesNewClient()
{
    using namespace DxUi;
    const auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
    AttachedHostWindow window;
    TextInputServices services;
    RequireSucceeded(services.Attach(window.Hwnd()), "application text service attaches before reentrant Read fixture");
    auto incoming               = std::make_shared<TestTextClient>();
    auto winner                 = std::make_shared<TestTextClient>();
    winner->snapshot.focusId    = 2u;
    winner->snapshot.state.text = L"newer";
    bool callbackRan            = false;
    HRESULT nestedResult        = E_UNEXPECTED;
    incoming->onRead            = [&]() noexcept
    {
        callbackRan  = true;
        nestedResult = services.SetClient(winner);
    };

    const HRESULT incomingResult = services.SetClient(incoming);
    Require(callbackRan && SUCCEEDED(nestedResult), "client Read can synchronously install a newer focus client");
    Require(incomingResult == TF_E_DISCONNECTED, "the superseded SetClient request stops after its Read callback");
    Require(services.HasClient(), "the Read callback's newer client remains attached");
    services.ClearClient();
    Require(winner->cancellations == 1u && incoming->cancellations == 0u,
            "clearing after Read reentrancy cancels the newer client and never publishes the superseded one");
}

void TestApplicationTextServiceCancelReentrancyPreservesNewClient()
{
    using namespace DxUi;
    const auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
    AttachedHostWindow window;
    TextInputServices services;
    RequireSucceeded(services.Attach(window.Hwnd()), "application text service attaches before reentrant Cancel fixture");
    auto oldClient              = std::make_shared<TestTextClient>();
    auto incoming               = std::make_shared<TestTextClient>();
    auto winner                 = std::make_shared<TestTextClient>();
    winner->snapshot.focusId    = 3u;
    winner->snapshot.state.text = L"newer";
    RequireSucceeded(services.SetClient(oldClient), "old client owns a TSF document before Cancel reentrancy");
    bool callbackRan     = false;
    HRESULT nestedResult = E_UNEXPECTED;
    oldClient->onCancel  = [&]() noexcept
    {
        callbackRan  = true;
        nestedResult = services.SetClient(winner);
    };

    const HRESULT incomingResult = services.SetClient(incoming);
    Require(callbackRan && SUCCEEDED(nestedResult), "disconnect cancellation can synchronously install a newer focus client");
    Require(incomingResult == TF_E_DISCONNECTED, "the superseded SetClient request stops after Cancel reentrancy");
    Require(services.HasClient() && incoming->reads == 0u, "the Cancel callback winner remains attached without reading the superseded client");
    services.ClearClient();
    Require(oldClient->cancellations == 1u && winner->cancellations == 1u && incoming->cancellations == 0u,
            "retiring the old store never tears down or redirects cancellation into the newer client");
}

void TestApplicationTextServiceSameClientReattachmentSurvivesReadCallbacks()
{
    using namespace DxUi;
    const auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
    {
        AttachedHostWindow window;
        TextInputServices services;
        RequireSucceeded(services.Attach(window.Hwnd()), "application text service attaches before same-client notification reentrancy");
        auto client = std::make_shared<TestTextClient>();
        RequireSucceeded(services.SetClient(client), "same-client notification fixture installs its initial session");

        client->snapshot.state.masked = true;
        ++client->snapshot.revision;
        HRESULT replacementResult = E_UNEXPECTED;
        client->onRead            = [&]() noexcept
        {
            // The returned snapshot remains masked, while the replacement session reads this new unmasked state.
            client->snapshot.state.masked = false;
            ++client->snapshot.revision;
            services.ClearClient();
            replacementResult = services.SetClient(client);
        };
        services.NotifyChanged();

        Require(SUCCEEDED(replacementResult) && services.HasClient(),
                "a same-client replacement installed from Read survives the stale masked-field notification");
        Require(client->cancellations == 1u, "the stale notification retires only the original same-client session");
        services.ClearClient();
        Require(client->cancellations == 2u, "the reattached same client remains live until its own explicit retirement");
    }

    {
        AttachedHostWindow window;
        TextInputServices services;
        RequireSucceeded(services.Attach(window.Hwnd()), "application text service attaches before same-client Escape reentrancy");
        auto client                                  = std::make_shared<TestTextClient>();
        client->snapshot.state.compositionStartIndex = 0u;
        RequireSucceeded(services.SetClient(client), "same-client Escape fixture installs an active composition session");

        HRESULT replacementResult = E_UNEXPECTED;
        client->onRead            = [&]() noexcept
        {
            services.ClearClient();
            replacementResult = services.SetClient(client);
        };
        const bool cancelled = services.CancelComposition();

        Require(! cancelled && SUCCEEDED(replacementResult) && services.HasClient(),
                "Escape does not clear a same-client replacement installed from its Read callback");
        Require(client->cancellations == 1u, "the stale Escape continuation cancels only the original session");
        services.ClearClient();
        Require(client->cancellations == 2u, "the same-client Escape replacement remains live until explicit retirement");
    }
}

void TestApplicationTextServiceRetirementReadReentrancyPreservesSameClientSuccessor()
{
    using namespace DxUi;
    const auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
    AttachedHostWindow window;
    TextInputServices services;
    RequireSucceeded(services.Attach(window.Hwnd()), "application text service attaches before retirement Read reentrancy");
    auto client                   = std::make_shared<TestTextClient>();
    NativeTextInputState original = client->snapshot.state;
    original.text                 = L"original";
    client->base                  = original;
    client->snapshot.state.text   = L"old preview";
    ++client->snapshot.revision;
    RequireSucceeded(services.SetClient(client), "retirement Read fixture installs its original preview session");

    HRESULT replacementResult = E_UNEXPECTED;
    client->onRead            = [&]() noexcept
    {
        client->snapshot.state.text = L"replacement";
        ++client->snapshot.revision;
        replacementResult = services.SetClient(client);
    };
    services.ClearClient();

    Require(SUCCEEDED(replacementResult) && services.HasClient(),
            "a same-client successor installed during retirement Read remains the active service session");
    Require(client->cancellations == 0u && client->snapshot.state.text == L"replacement",
            "the retired store skips cancellation after Read discovers a successor owning the preview state");
    services.ClearClient();
    Require(client->cancellations == 1u, "ordinary retirement still cancels the successor when no newer session replaces it");
}

void TestApplicationTextServiceDetachRejectsReentrantReattach()
{
    using namespace DxUi;
    const auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
    AttachedHostWindow window;
    TextInputServices services;
    RequireSucceeded(services.Attach(window.Hwnd()), "application text service attaches before Detach reentrancy fixture");
    auto oldClient       = std::make_shared<TestTextClient>();
    auto attemptedClient = std::make_shared<TestTextClient>();
    auto finalClient     = std::make_shared<TestTextClient>();
    RequireSucceeded(services.SetClient(oldClient), "old client owns a TSF document before Detach");
    HRESULT reentrantSetClient = S_OK;
    HRESULT reentrantAttach    = S_OK;
    oldClient->onCancel        = [&]() noexcept
    {
        reentrantSetClient = services.SetClient(attemptedClient);
        reentrantAttach    = services.Attach(window.Hwnd());
    };

    services.Detach();
    Require(reentrantSetClient == E_UNEXPECTED && reentrantAttach == E_UNEXPECTED,
            "Detach rejects a client or HWND attachment requested from its retirement callback");
    Require(! services.HasClient(), "Detach finishes detached after rejecting callback resurrection");
    RequireSucceeded(services.Attach(window.Hwnd()), "an explicit Attach succeeds after Detach has finished");
    RequireSucceeded(services.SetClient(finalClient), "a later explicit SetClient succeeds after reattachment");
    services.ClearClient();
    Require(finalClient->cancellations == 1u && attemptedClient->cancellations == 0u,
            "post-detach reattachment owns only the explicitly installed final client");
}

void TestApplicationTextServiceRetirementPreservesForeignFocusAndReentrantWinner()
{
    using namespace DxUi;
    const auto apartment                    = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
    bool activationSucceeded                = false;
    bool attachSucceeded                    = false;
    bool oldClientInstalled                 = false;
    bool managerReady                       = false;
    bool sinkAdvised                        = false;
    bool foreignFocusEstablished            = false;
    bool foreignFocusPreserved              = false;
    bool replacementInstalled               = false;
    bool callbackRan                        = false;
    bool replacementFocusPreserved          = false;
    bool sinkUnadvised                      = false;
    bool replacementCancelled               = false;
    const ForegroundRunResult foregroundRun = RunUntilForegroundHeld([&]
    {
        activationSucceeded = attachSucceeded = oldClientInstalled = managerReady = sinkAdvised = false;
        foreignFocusEstablished = foreignFocusPreserved = replacementInstalled = callbackRan = false;
        replacementFocusPreserved = sinkUnadvised = replacementCancelled = false;
        AttachedHostWindow window;
        static_cast<void>(ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE));
        const uint32_t foregroundLossCount = window.ForegroundLossCount();
        activationSucceeded                = TryActivateDxUiTestWindow(window.Hwnd());
        if (! activationSucceeded)
        {
            std::cerr << "[TRACE] application TSF activation failed: target=" << window.Hwnd() << " foreground=" << GetForegroundWindow()
                      << " active=" << GetActiveWindow() << " focus=" << GetFocus() << " visible=" << IsWindowVisible(window.Hwnd())
                      << " exStyle=" << GetWindowLongPtrW(window.Hwnd(), GWL_EXSTYLE) << '\n';
            return ForegroundAttempt{window.ForegroundLossCount() != foregroundLossCount, window.LastForegroundThiefThreadId()};
        }

        TextInputServices services;
        attachSucceeded    = SUCCEEDED(services.Attach(window.Hwnd()));
        auto oldClient     = std::make_shared<TestTextClient>();
        oldClientInstalled = attachSucceeded && SUCCEEDED(services.SetClient(oldClient));
        if (! oldClientInstalled)
            return ForegroundAttempt{window.ForegroundLossCount() != foregroundLossCount, window.LastForegroundThiefThreadId()};

        wil::com_ptr_nothrow<ITfThreadMgr> manager;
        managerReady = SUCCEEDED(CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(manager.put()))) && manager;
        wil::com_ptr_nothrow<ITfSource> source;
        managerReady = managerReady && SUCCEEDED(manager.query_to(source.put())) && source;
        if (! managerReady)
            return ForegroundAttempt{window.ForegroundLossCount() != foregroundLossCount, window.LastForegroundThiefThreadId()};
        TextServicesReentrantFocusSink sink;
        DWORD sinkCookie = TF_INVALID_COOKIE;
        sinkAdvised      = SUCCEEDED(source->AdviseSink(__uuidof(ITfThreadMgrEventSink), &sink, &sinkCookie));
        if (! sinkAdvised)
            return ForegroundAttempt{window.ForegroundLossCount() != foregroundLossCount, window.LastForegroundThiefThreadId()};
        auto unadviseSink = wil::scope_exit([&]() noexcept
        {
            if (sinkCookie != TF_INVALID_COOKIE)
                static_cast<void>(source->UnadviseSink(sinkCookie));
        });

        wil::com_ptr_nothrow<ITfDocumentMgr> foreignDocument;
        HRESULT foreignResult = manager->CreateDocumentMgr(foreignDocument.put());
        if (SUCCEEDED(foreignResult) && foreignDocument)
            foreignResult = manager->SetFocus(foreignDocument.get());
        wil::com_ptr_nothrow<ITfDocumentMgr> beforeRetirement;
        const HRESULT beforeRead = SUCCEEDED(foreignResult) ? manager->GetFocus(beforeRetirement.put()) : foreignResult;
        foreignFocusEstablished  = SUCCEEDED(beforeRead) && TestTextServicesSameComIdentity(beforeRetirement.get(), foreignDocument.get());
        if (! foreignFocusEstablished)
            return ForegroundAttempt{window.ForegroundLossCount() != foregroundLossCount, window.LastForegroundThiefThreadId()};

        services.ClearClient();
        wil::com_ptr_nothrow<ITfDocumentMgr> afterForeignRetirement;
        const HRESULT afterForeignRead = manager->GetFocus(afterForeignRetirement.put());
        foreignFocusPreserved          = SUCCEEDED(afterForeignRead) && TestTextServicesSameComIdentity(afterForeignRetirement.get(), foreignDocument.get());

        auto replacement              = std::make_shared<TestTextClient>();
        replacement->snapshot.focusId = 4u;
        oldClientInstalled            = SUCCEEDED(services.SetClient(oldClient));
        if (! oldClientInstalled || FAILED(manager->SetFocus(foreignDocument.get())))
            return ForegroundAttempt{window.ForegroundLossCount() != foregroundLossCount, window.LastForegroundThiefThreadId()};
        wil::com_ptr_nothrow<ITfDocumentMgr> replacementDocument;
        HRESULT callbackSetResult = E_UNEXPECTED;
        sink.onSetFocus           = [&]() noexcept
        {
            if (! callbackRan)
            {
                callbackRan       = true;
                callbackSetResult = services.SetClient(replacement);
                if (SUCCEEDED(callbackSetResult))
                    static_cast<void>(manager->GetFocus(replacementDocument.put()));
            }
        };
        services.ClearClient();
        wil::com_ptr_nothrow<ITfDocumentMgr> afterReentrantRetirement;
        const HRESULT focusRead   = manager->GetFocus(afterReentrantRetirement.put());
        replacementInstalled      = SUCCEEDED(callbackSetResult) && services.HasClient() && replacement->cancellations == 0u;
        replacementFocusPreserved = SUCCEEDED(focusRead) && TestTextServicesSameComIdentity(afterReentrantRetirement.get(), replacementDocument.get());

        static_cast<void>(source->UnadviseSink(sinkCookie));
        sinkCookie    = TF_INVALID_COOKIE;
        sinkUnadvised = true;
        services.ClearClient();
        replacementCancelled = replacement->cancellations == 1u;
        return ForegroundAttempt{window.ForegroundLossCount() != foregroundLossCount, window.LastForegroundThiefThreadId()};
    });
    if (! ForegroundHeldOrSkip(foregroundRun, "application TSF focus retirement assertions"))
        return;
    Require(activationSucceeded, "the application TSF focus fixture activates when no foreground thief is observed");
    Require(attachSucceeded && oldClientInstalled && managerReady && sinkAdvised, "application TSF focus fixture establishes its host and sink");
    Require(foreignFocusEstablished && foreignFocusPreserved, "clearing a host document preserves an independently focused TSF document");
    Require(callbackRan && replacementInstalled && replacementFocusPreserved,
            "retiring the old document leaves the client installed reentrantly by a TSF focus callback active and focused");
    Require(sinkUnadvised && replacementCancelled, "the TSF sink and callback-installed client are retired before fixture teardown");
    static_cast<void>(apartment);
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
    RequireSucceeded(DecodeClipboardText(maximum, text), "clipboard decoder accepts the embedded editor boundary");
    Require(text.size() == 65536, "clipboard text at the embedded boundary is not truncated");
    maximum.back() = L'x';
    Require(DecodeClipboardText(maximum, text) == HRESULT_FROM_WIN32(ERROR_INVALID_DATA) && text.empty(),
            "clipboard decoder rejects an unterminated oversized document");
    maximum.resize(100001, L'x');
    maximum.back() = 0;
    RequireSucceeded(DecodeClipboardText(maximum, text), "native clipboard decoding is independent of the embedded edit budget");
    Require(text.size() == 100000, "native clipboard decoding retains all large-selection text");
    maximum[99999] = wchar_t(0xd800);
    Require(DecodeClipboardText(maximum, text) == HRESULT_FROM_WIN32(ERROR_INVALID_DATA) && text.empty(),
            "malformed UTF-16 above the former ceiling is rejected without partial output");
    size_t bytes         = 99;
    const size_t largest = (std::numeric_limits<size_t>::max)() / sizeof(wchar_t) - 1;
    RequireSucceeded(ClipboardAllocationSize(largest, bytes), "largest representable clipboard allocation includes its terminator");
    Require(bytes == (largest + 1) * sizeof(wchar_t), "clipboard byte calculation does not wrap");
    Require(ClipboardAllocationSize(largest + 1, bytes) == HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW) && bytes == 0,
            "clipboard multiplication overflow is rejected");
    Require(ClipboardAllocationSize((std::numeric_limits<size_t>::max)(), bytes) == HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW) && bytes == 0,
            "clipboard terminator addition cannot wrap");
    RequireSucceeded(ClipboardAllocationSize(0, bytes), "empty clipboard text still allocates a terminator");
    Require(bytes == sizeof(wchar_t), "empty clipboard allocation contains one UTF-16 terminator");
}

void TestApplicationTextStoreDeferredLocks()
{
    using namespace DxUi;
    auto client = std::make_shared<TestTextClient>();
    struct Dispatch final
    {
        bool queued            = false;
        unsigned posts         = 0;
        unsigned attempts      = 0;
        unsigned delivered     = 0;
        unsigned failOnAttempt = 0;
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
            ++queue.attempts;
            if (queue.attempts == queue.failOnAttempt)
                return false;
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

            // A nested UI message loop can receive the posted request while this lock is still held. Consume it once:
            // TS_E_NOLOCK must wait for the outer lock to unwind, not enqueue itself repeatedly into this loop.
            unsigned nestedMessages = 0;
            while (dispatch.queued && nestedMessages < 4u)
            {
                ++nestedMessages;
                const HRESULT nested = DispatchDeferredTextStoreLock(store.get(), dispatch.queued);
                ++dispatch.delivered;
                Require(nested == TS_E_NOLOCK, "a nested message sees the held lock and defers the request");
            }
            Require(nestedMessages == 1u && ! dispatch.queued, "a held-lock notification does not repost inside the nested message loop");
        }
        else
            Require((flags & TS_LF_READWRITE) == TS_LF_READWRITE, "deferred lock retains the strongest requested access");
        return S_OK;
    };
    HRESULT session = E_UNEXPECTED;
    RequireSucceeded(store->RequestLock(TS_LF_READ, &session), "initial read lock requested");
    RequireSucceeded(session, "initial read lock completes");
    Require(grants == 1 && dispatch.posts == 2 && dispatch.delivered == 1u && dispatch.queued,
            "lock release posts one coalesced retry after a nested loop consumed the early notification");
    RequireSucceeded(DispatchDeferredTextStoreLock(store.get(), dispatch.queued), "application message grants the coalesced lock");
    Require(grants == 2 && ! dispatch.queued && DispatchDeferredTextStoreLock(store.get(), dispatch.queued) == S_FALSE,
            "drained locks do not repeat on an idle message");
    Require(sink.editTransactionStartCount == 1 && sink.editTransactionEndCount == 1 && sink.editTransactionDepth == 0,
            "the deferred write lock has one balanced transaction");
    grants = 0;
    RequireSucceeded(store->RequestLock(TS_LF_READ, &session), "another lock queues before detach");
    DisconnectNativeTextInputTextStore(store.get());
    Require(DispatchDeferredTextStoreLock(store.get(), dispatch.queued) == S_FALSE && grants == 1,
            "disconnect discards pending locks before a late application message");

    // If the single re-post after unlock fails, keep the accepted request coalesced and retry on the next real lock
    // request. There is no timer, loop or hidden retry queue.
    auto retryClient = std::make_shared<TestTextClient>();
    Dispatch retryDispatch{};
    retryDispatch.failOnAttempt = 2u;
    NativeTextStoreTestSink retrySink;
    wil::com_ptr_nothrow<ITextStoreACP> retryStore;
    retryStore.attach(CreateClientTextStore(nullptr,
                                            retryClient,
                                            {&retryDispatch,
                                             [](void* context) noexcept
    {
        auto& queue = *static_cast<Dispatch*>(context);
        if (! queue.queued)
        {
            ++queue.attempts;
            if (queue.attempts == queue.failOnAttempt)
                return false;
            queue.queued = true;
            ++queue.posts;
        }
        return true;
    }}));
    Require(retryStore != nullptr, "retry fixture creates an application text store");
    RequireSucceeded(retryStore->AdviseSink(__uuidof(ITextStoreACPSink), &retrySink, TS_AS_TEXT_CHANGE), "retry sink advised");
    unsigned retryGrants    = 0u;
    retrySink.onLockGranted = [&](DWORD) noexcept
    {
        ++retryGrants;
        if (retryGrants == 1u)
        {
            HRESULT deferredSession = S_OK;
            RETURN_IF_FAILED(retryStore->RequestLock(TS_LF_READ, &deferredSession));
            Require(deferredSession == TS_S_ASYNC, "retry fixture accepts the nested asynchronous lock");
            const HRESULT earlyMessage = DispatchDeferredTextStoreLock(retryStore.get(), retryDispatch.queued);
            ++retryDispatch.delivered;
            Require(earlyMessage == TS_E_NOLOCK, "retry fixture consumes its notification while the lock is held");
        }
        return S_OK;
    };
    RequireSucceeded(retryStore->RequestLock(TS_LF_READ, &session), "retry fixture starts its outer lock");
    Require(retryGrants == 1u && retryDispatch.attempts == 2u && retryDispatch.posts == 1u && ! retryDispatch.queued,
            "failed post after unlock retains the deferred request without a busy retry");
    RequireSucceeded(retryStore->RequestLock(TS_LF_READ, &session), "a later real lock request retries the retained post once");
    Require(retryDispatch.attempts == 3u && retryDispatch.posts == 2u && retryDispatch.queued,
            "the later request creates one queued retry after the earlier post failure");
    RequireSucceeded(DispatchDeferredTextStoreLock(retryStore.get(), retryDispatch.queued), "the retained asynchronous request eventually receives its lock");
    Require(retryGrants == 3u && ! retryDispatch.queued, "the one deferred request is granted exactly once");
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
