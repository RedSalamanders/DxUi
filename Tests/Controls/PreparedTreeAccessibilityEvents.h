#pragma once

#include "../Support/PreparedTreeSemanticFixture.h"

class PreparedTreeStructureObserver final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                                                                                IUIAutomationStructureChangedEventHandler,
                                                                                Microsoft::WRL::FtmBase>
{
public:
    std::atomic<bool> armed{false};
    std::atomic<HRESULT> readResult{E_PENDING};
    std::atomic<HRESULT> invokeResult{E_PENDING};
    wil::com_ptr_nothrow<IUIAutomationElement> lastRow;
    wil::com_ptr_nothrow<IUIAutomationInvokePattern> replaceRoot;
    wil::unique_event completed{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    wil::unique_event entered{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    wil::unique_event continueDelivery{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    bool gateDelivery = false; // Set before subscribing; immutable while callbacks are possible.

    HRESULT STDMETHODCALLTYPE HandleStructureChangedEvent(IUIAutomationElement*, StructureChangeType change, SAFEARRAY*) noexcept override
    {
        if (change != StructureChangeType_ChildrenInvalidated || ! armed.exchange(false, std::memory_order_acq_rel))
            return S_OK;
        SetEvent(entered.get());
        if (gateDelivery && WaitForSingleObject(continueDelivery.get(), 20000) != WAIT_OBJECT_0)
        {
            readResult.store(HRESULT_FROM_WIN32(ERROR_TIMEOUT));
            SetEvent(completed.get());
            return S_OK;
        }
        wil::unique_bstr name;
        HRESULT result = lastRow->get_CurrentName(name.put()); // A real client calls back into the new snapshot.
        if (SUCCEEDED(result) && (! name || std::wstring_view(name.get(), SysStringLen(name.get())) != L"Omega nouveau Ω"))
            result = E_FAIL;
        readResult.store(result);
        invokeResult.store(replaceRoot->Invoke()); // Standard UIA action destroys the Tree on its owner.
        SetEvent(completed.get());
        return S_OK;
    }
};

static void RunPreparedTreeReentrantNativeClient(bool coalescedBurst)
{
    using namespace DxUi;
    using namespace DxUi::Tests;
    auto witness = std::make_shared<PreparedTreeSourceWitness>();
    ThreePreparedTreeModel model(std::make_shared<ThreePreparedTreeRows>(ThreePreparedTreeRows::Names{L"Alpha α", L"Beta β", L"Omega Ω"}, witness));
    AttachedHostWindow window;
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0, 0, 300, 120));
    tree->SetAccessibleAutomationId(L"PreparedTree");
    tree->SetModel(&model);
    auto* button = root->AddChild<Button>(L"Replace root");
    button->SetAccessibleAutomationId(L"ReplaceRoot");
    button->SetBounds(D2D1::RectF(0, 128, 200, 160));
    unsigned int replacements = 0;
    unsigned int probeActions = 0;
    button->SetOnClick([&]
    {
        ++replacements;
        auto replacement = std::make_unique<Panel>();
        replacement->SetAccessibleName(L"Acknowledged replacement");
        window.Host().SetRoot(std::move(replacement));
    });
    auto* probe = root->AddChild<Button>(L"Probe owner action");
    probe->SetAccessibleAutomationId(L"ProbeOwnerAction");
    probe->SetBounds(D2D1::RectF(0, 168, 200, 200));
    probe->SetOnClick([&] { ++probeActions; });
    window.Host().SetRoot(std::move(root));
    wil::com_ptr_nothrow<PreparedTreeStructureObserver> observer;
    observer.attach(Microsoft::WRL::Make<PreparedTreeStructureObserver>().Detach());
    Require(observer && observer->completed && observer->entered && observer->continueDelivery,
            "prepared Tree allocates the real client observer and delivery gates");
    observer->gateDelivery = coalescedBurst;
    wil::unique_event ready(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    wil::unique_event stop(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    wil::unique_event finished(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(ready && stop && finished, "prepared Tree client lifetime events exist");
    std::atomic<HRESULT> setup{E_PENDING};
    const HWND hwnd = window.Hwnd();
    std::jthread client([&]
    {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const auto uninitialize   = wil::scope_exit([&]
        {
            if (SUCCEEDED(initialized))
                CoUninitialize();
        });
        wil::com_ptr_nothrow<IUIAutomation> automation;
        wil::com_ptr_nothrow<IUIAutomationElement> element;
        HRESULT result = initialized;
        if (SUCCEEDED(result))
            result = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(automation.put()));
        if (SUCCEEDED(result))
            result = automation->ElementFromHandle(hwnd, element.put());
        const auto find = [&](const wchar_t* id, wil::com_ptr_nothrow<IUIAutomationElement>& found) noexcept
        {
            wil::unique_variant value;
            value.vt      = VT_BSTR;
            value.bstrVal = SysAllocString(id);
            if (! value.bstrVal)
                return E_OUTOFMEMORY;
            wil::com_ptr_nothrow<IUIAutomationCondition> condition;
            HRESULT hr = automation->CreatePropertyCondition(UIA_AutomationIdPropertyId, value, condition.put());
            if (SUCCEEDED(hr))
                hr = element->FindFirst(TreeScope_Descendants, condition.get(), found.put());
            return SUCCEEDED(hr) && ! found ? E_FAIL : hr;
        };
        wil::com_ptr_nothrow<IUIAutomationElement> treeElement, buttonElement;
        if (SUCCEEDED(result))
            result = find(L"PreparedTree", treeElement);
        if (SUCCEEDED(result))
            result = find(L"ReplaceRoot", buttonElement);
        wil::com_ptr_nothrow<IUIAutomationTreeWalker> walker;
        if (SUCCEEDED(result))
            result = automation->get_RawViewWalker(walker.put());
        if (SUCCEEDED(result))
            result = walker->GetLastChildElement(treeElement.get(), observer->lastRow.put());
        if (SUCCEEDED(result) && ! observer->lastRow)
            result = E_FAIL;
        if (SUCCEEDED(result))
            result = buttonElement->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(observer->replaceRoot.put()));
        if (SUCCEEDED(result))
            result = automation->AddStructureChangedEventHandler(element.get(), TreeScope_Subtree, nullptr, observer.get());
        wil::com_ptr_nothrow<IUIAutomationElement> probeElement;
        wil::com_ptr_nothrow<IUIAutomationInvokePattern> probePattern;
        if (SUCCEEDED(result))
            result = find(L"ProbeOwnerAction", probeElement);
        if (SUCCEEDED(result))
            result = probeElement->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(probePattern.put()));
        if (SUCCEEDED(result))
            result = probePattern->Invoke();
        setup.store(result);
        SetEvent(ready.get());
        if (SUCCEEDED(result))
        {
            WaitForSingleObject(stop.get(), INFINITE);
            static_cast<void>(automation->RemoveStructureChangedEventHandler(element.get(), observer.get()));
        }
        observer->lastRow.reset();
        observer->replaceRoot.reset();
        SetEvent(finished.get());
    });
    const auto waitFor = [&](HANDLE event)
    {
        const auto deadline = GetTickCount64() + 20000;
        while (WaitForSingleObject(event, 0) != WAIT_OBJECT_0)
        {
            window.PumpMessages();
            if (WaitForSingleObject(event, 0) == WAIT_OBJECT_0)
                return true;
            const auto now = GetTickCount64();
            if (now >= deadline)
                return false;
            const DWORD waited = MsgWaitForMultipleObjectsEx(1, &event, static_cast<DWORD>(deadline - now), QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (waited != WAIT_OBJECT_0 && waited != WAIT_OBJECT_0 + 1)
                return false;
        }
        return true;
    };
    const auto stopClient = wil::scope_exit([&]() noexcept
    {
        SetEvent(observer->continueDelivery.get());
        SetEvent(stop.get());
        Require(waitFor(finished.get()), "prepared Tree client unsubscription finishes before join");
    });
    Require(waitFor(ready.get()) && SUCCEEDED(setup.load()), "real client subscribes to prepared Tree structure changes");
    Require(probeActions == 1, "real client owner action succeeds outside the notification callback");
    observer->armed.store(true, std::memory_order_release);
    model.Replace(std::make_shared<ThreePreparedTreeRows>(ThreePreparedTreeRows::Names{L"Alpha α", L"Beta nouveau β", L"Omega nouveau Ω"}, witness));
    const auto notifyStarted = GetTickCount64();
    tree->NotifyDataChanged();
    const auto notifyFinished = GetTickCount64();
    window.Host().RefreshAccessibilitySnapshot();
    const auto publishFinished = GetTickCount64();
    std::cerr << "[PREPARED TREE PUBLISH] notifyMs=" << notifyFinished - notifyStarted << " refreshMs=" << publishFinished - notifyFinished << '\n';
    if (coalescedBurst)
    {
        Require(waitFor(observer->entered.get()), "real notification enters its retained-client gate before the publication burst");
        std::array<std::shared_ptr<PreparedTreeSourceWitness>, 32u> burstWitnesses;
        for (size_t index = 0u; index < burstWitnesses.size(); ++index)
        {
            burstWitnesses[index] = std::make_shared<PreparedTreeSourceWitness>();
            model.Replace(std::make_shared<ThreePreparedTreeRows>(
                ThreePreparedTreeRows::Names{L"Alpha α", L"Beta nouveau β", index + 1u == burstWitnesses.size() ? L"Omega nouveau Ω" : L"Omega en attente Ω"},
                burstWitnesses[index]));
            tree->NotifyDataChanged();
            window.Host().RefreshAccessibilitySnapshot();
        }
        for (size_t index = 0u; index + 1u < burstWitnesses.size(); ++index)
            Require(burstWitnesses[index]->destructions.load() == 1u, "a blocked real notification does not retain intermediate prepared source generations");
        Require(burstWitnesses.back()->destructions.load() == 0u && replacements == 0u,
                "the final coherent source stays alive and the client action waits behind its explicit gate");
        SetEvent(observer->continueDelivery.get());
    }
    const bool notified = waitFor(observer->completed.get());
    std::cerr << "[PREPARED TREE CLIENT] notified=" << notified << " read=" << std::hex << observer->readResult.load()
              << " invoke=" << observer->invokeResult.load() << std::dec << " replacements=" << replacements << '\n';
    Require(notified, "same-count same-id prepared source invalidates the real client");
    Require(observer->readResult.load() == S_OK, "real notification callback reads the new complete row name");
    Require(observer->invokeResult.load() == S_OK, "real notification callback invokes root replacement");
    Require(replacements == 1 && window.Host().GetRoot()->GetAccessibleName() == L"Acknowledged replacement",
            "the real UIA event callback replaces the root exactly once without using the removed Tree");
}

void TestPreparedTreeSourceReplacementNotifiesReentrantNativeClient()
{
    RunPreparedTreeReentrantNativeClient(false);
}

void TestPreparedTreeSourceBurstRetiresIntermediateSourcesDuringNativeCallback()
{
    RunPreparedTreeReentrantNativeClient(true);
}
