#pragma once

#include "../Support/Support.Tests.PreparedTreeSemanticFixture.h"
#include "Embedded.Tests.EmbeddedUiaBridge.h"

class EmbeddedPreparedTreeObserver final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                                                                               IUIAutomationStructureChangedEventHandler,
                                                                               IUIAutomationPropertyChangedEventHandler,
                                                                               Microsoft::WRL::FtmBase>
{
public:
    std::atomic<bool> armed{false};
    std::atomic<HRESULT> readResult{E_PENDING};
    std::atomic<HRESULT> invokeResult{E_PENDING};
    std::atomic<HRESULT> replaceResult{E_PENDING};
    wil::com_ptr_nothrow<IUIAutomationElement> lastRow;
    wil::com_ptr_nothrow<IUIAutomationInvokePattern> replaceRoot;
    wil::com_ptr_nothrow<IUIAutomationInvokePattern> probeOwner;
    wil::unique_event completed{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    wil::unique_event entered{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    wil::unique_event continueDelivery{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    bool gateDelivery = false; // Set before subscribing; immutable while callbacks are possible.
    bool retireRoot   = true;
    std::atomic<bool> heardLatestProperty{false};

    HRESULT STDMETHODCALLTYPE HandlePropertyChangedEvent(IUIAutomationElement* sender, PROPERTYID property, VARIANT eventValue) noexcept override
    {
        if (property == UIA_NamePropertyId && eventValue.vt == VT_BSTR && eventValue.bstrVal &&
            std::wstring_view(eventValue.bstrVal, SysStringLen(eventValue.bstrVal)) == L"Root replacement dernier Ω")
            heardLatestProperty.store(true);
        return property == UIA_NamePropertyId ? HandleStructureChangedEvent(sender, StructureChangeType_ChildrenInvalidated, nullptr) : S_OK;
    }

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
        invokeResult.store(probeOwner->Invoke());
        if (retireRoot)
            replaceResult.store(replaceRoot->Invoke()); // Standard UIA action destroys the Tree on its owner.
        SetEvent(completed.get());
        return S_OK;
    }
};

static void RunPreparedTreeReentrantEmbeddedClient(GraphicsFixture& gpu, bool coalescedBurst, bool propertyEvent = false)
{
    using namespace DxUi;
    using namespace DxUi::Tests;
    auto witness = std::make_shared<PreparedTreeSourceWitness>();
    ThreePreparedTreeModel model(std::make_shared<ThreePreparedTreeRows>(ThreePreparedTreeRows::Names{L"Alpha α", L"Beta β", L"Omega Ω"}, witness));
    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get()), "embedded event view");
    auto& view = scene.view;
    EmbeddedUiaTest::Bridge bridge(view, 320, 240);
    Check(bridge.valid, "nonactivating application UIA bridge");
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
        view.Controls().SetRoot(std::move(replacement));
    });
    auto* probe = root->AddChild<Button>(L"Probe owner action");
    probe->SetAccessibleAutomationId(L"ProbeOwnerAction");
    probe->SetBounds(D2D1::RectF(0, 168, 200, 200));
    probe->SetOnClick([&] { ++probeActions; });
    view.Controls().SetRoot(std::move(root));
    Hr(view.Prepare(320, 240), "prepare embedded event source");
    Hr(bridge.Attach(false, 9317u), "attach embedded event source");
    wil::com_ptr_nothrow<EmbeddedPreparedTreeObserver> observer;
    observer.attach(Microsoft::WRL::Make<EmbeddedPreparedTreeObserver>().Detach());
    Check(observer && observer->completed && observer->entered && observer->continueDelivery,
          "prepared Tree allocates the real client observer and delivery gates");
    observer->gateDelivery = coalescedBurst;
    observer->retireRoot   = ! (propertyEvent && coalescedBurst);
    wil::unique_event ready(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    wil::unique_event stop(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    wil::unique_event finished(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Check(ready && stop && finished, "prepared Tree client lifetime events exist");
    std::atomic<HRESULT> setup{E_PENDING};
    const HWND hwnd = bridge.Hwnd();
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
        {
            if (propertyEvent)
            {
                PROPERTYID property = UIA_NamePropertyId;
                result = automation->AddPropertyChangedEventHandlerNativeArray(element.get(), TreeScope_Subtree, nullptr, observer.get(), &property, 1);
            }
            else
                result = automation->AddStructureChangedEventHandler(element.get(), TreeScope_Subtree, nullptr, observer.get());
        }
        wil::com_ptr_nothrow<IUIAutomationElement> probeElement;

        if (SUCCEEDED(result))
            result = find(L"ProbeOwnerAction", probeElement);
        if (SUCCEEDED(result))
            result = probeElement->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(observer->probeOwner.put()));
        if (SUCCEEDED(result))
            result = observer->probeOwner->Invoke();
        setup.store(result);
        SetEvent(ready.get());
        if (SUCCEEDED(result))
        {
            WaitForSingleObject(stop.get(), INFINITE);
            if (propertyEvent)
                static_cast<void>(automation->RemovePropertyChangedEventHandler(element.get(), observer.get()));
            else
                static_cast<void>(automation->RemoveStructureChangedEventHandler(element.get(), observer.get()));
        }
        observer->lastRow.reset();
        observer->replaceRoot.reset();
        observer->probeOwner.reset();
        SetEvent(finished.get());
    });
    const auto waitFor = [&](HANDLE event)
    {
        const auto deadline = GetTickCount64() + 20000;
        while (WaitForSingleObject(event, 0) != WAIT_OBJECT_0)
        {
            bridge.Pump();
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
        Check(waitFor(finished.get()), "prepared Tree client unsubscription finishes before join");
    });
    Check(waitFor(ready.get()) && SUCCEEDED(setup.load()), "real client subscribes to prepared Tree structure changes");
    Check(probeActions == 1, "real client owner action succeeds outside the notification callback");
    observer->armed.store(true, std::memory_order_release);
    model.Replace(std::make_shared<ThreePreparedTreeRows>(ThreePreparedTreeRows::Names{L"Alpha α", L"Beta nouveau β", L"Omega nouveau Ω"}, witness));
    if (propertyEvent)
        button->SetAccessibleName(L"Root replacement nouveau Ω");
    const auto notifyStarted = GetTickCount64();
    tree->NotifyDataChanged();
    const auto notifyFinished = GetTickCount64();
    Hr(view.Prepare(320, 240), "prepare changed embedded source");
    Hr(bridge.Update(), "publish changed embedded source");
    const auto publishFinished = GetTickCount64();
    std::cerr << "[PREPARED TREE PUBLISH] notifyMs=" << notifyFinished - notifyStarted << " refreshMs=" << publishFinished - notifyFinished << '\n';
    if (coalescedBurst)
    {
        Check(waitFor(observer->entered.get()), "real notification enters its retained-client gate before the publication burst");
        std::array<std::shared_ptr<PreparedTreeSourceWitness>, 32u> burstWitnesses;
        for (size_t index = 0u; index < burstWitnesses.size(); ++index)
        {
            burstWitnesses[index] = std::make_shared<PreparedTreeSourceWitness>();
            model.Replace(std::make_shared<ThreePreparedTreeRows>(
                ThreePreparedTreeRows::Names{L"Alpha α", L"Beta nouveau β", index + 1u == burstWitnesses.size() ? L"Omega nouveau Ω" : L"Omega en attente Ω"},
                burstWitnesses[index]));
            tree->NotifyDataChanged();
            if (propertyEvent)
                button->SetAccessibleName(index + 1u == burstWitnesses.size() ? L"Root replacement dernier Ω" : L"Root replacement attente Ω");
            Hr(view.Prepare(320, 240), "prepare changed embedded source");
            Hr(bridge.Update(), "publish changed embedded source");
        }
        for (size_t index = 0u; index + 1u < burstWitnesses.size(); ++index)
            Check(burstWitnesses[index]->destructions.load() == 1u, "a blocked real notification does not retain intermediate prepared source generations");
        Check(burstWitnesses.back()->destructions.load() == 0u && replacements == 0u,
              "the final coherent source stays alive and the client action waits behind its explicit gate");
        SetEvent(observer->continueDelivery.get());
    }
    const bool notified = waitFor(observer->completed.get());
    std::cerr << "[PREPARED TREE CLIENT] notified=" << notified << " read=" << std::hex << observer->readResult.load()
              << " invoke=" << observer->invokeResult.load() << std::dec << " replacements=" << replacements << '\n';
    Check(notified, "same-count same-id prepared source invalidates the real client");
    Check(observer->readResult.load() == S_OK, "real notification callback reads the new complete row name");
    Check(observer->invokeResult.load() == S_OK, "real notification callback invokes a surviving owner action");
    Check(probeActions == 2, "callback owner action succeeds exactly once");
    if (! observer->retireRoot)
    {
        Check(waitFor(observer->completed.get()), "property callback completes");
        const auto deadline = GetTickCount64() + 20000;
        while (! observer->heardLatestProperty.load() && GetTickCount64() < deadline)
        {
            bridge.Pump();
            Sleep(1);
        }
        Check(observer->heardLatestProperty.load(), "held property delivery coalesces to the latest literal value");
        Check(replacements == 0, "held property delivery preserves the current source and control");
        return;
    }
    Check(observer->replaceResult.load() == UIA_E_ELEMENTNOTAVAILABLE, "retiring its control reports unavailable after one acknowledged root action");
    Check(replacements == 1 && view.Controls().GetRoot()->GetAccessibleName() == L"Acknowledged replacement",
          "the real UIA event callback replaces the root exactly once without using the removed Tree");
}

static void TestPreparedTreeSourceReplacementNotifiesReentrantEmbeddedClient(GraphicsFixture& gpu)
{
    RunPreparedTreeReentrantEmbeddedClient(gpu, false);
}

static void TestPreparedTreeSourceBurstRetiresIntermediateSourcesDuringEmbeddedCallback(GraphicsFixture& gpu)
{
    RunPreparedTreeReentrantEmbeddedClient(gpu, true);
}

static void TestPreparedTreePropertyEventPermitsOwnerReentry(GraphicsFixture& gpu)
{
    RunPreparedTreeReentrantEmbeddedClient(gpu, false, true);
}

static void TestPreparedTreePropertyBurstCoalescesWithoutRetainingSources(GraphicsFixture& gpu)
{
    RunPreparedTreeReentrantEmbeddedClient(gpu, true, true);
}
