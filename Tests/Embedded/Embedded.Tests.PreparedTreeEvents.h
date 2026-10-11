#pragma once

#include "../Support/Support.Tests.PreparedTreeSemanticFixture.h"
#include "../Support/Support.Tests.UiaTestClient.h"
#include "Embedded.Tests.EmbeddedUiaBridge.h"
#include <mutex>
#if defined(DXUI_HAS_INTRINSIC_LAYOUT) && DXUI_ENABLE_DIAGNOSTICS
#include "../../src/Controls/DxUi.Internal.h"
#endif

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
    bool gateDelivery      = false; // Set before subscribing; immutable while callbacks are possible.
    bool retireRoot        = true;
    bool relationshipEvent = false;
    std::atomic<bool> heardLatestProperty{false};
    std::atomic<bool> heardFirstRelationship{false};
    std::atomic<HRESULT> firstRelationshipResult{E_PENDING};
    std::atomic<HRESULT> retainedRelationshipResult{E_PENDING};
    std::atomic<HRESULT> latestRelationshipResult{E_PENDING};
    std::atomic<HRESULT> retiredRelationshipResult{E_PENDING};
    std::atomic<HRESULT> clearedRelationshipResult{E_PENDING};
    std::atomic<bool> expectClearedRelationship{false};
    std::atomic<size_t> intermediateRelationships{0u};
    std::atomic<size_t> duplicateFirstRelationships{0u};
    std::atomic<size_t> duplicateLatestRelationships{0u};
    std::atomic<bool> latestRelationshipClaimed{false};
    std::atomic<bool> clearedRelationshipClaimed{false};
    std::atomic<size_t> callbackActivity{0u};
    wil::com_ptr_nothrow<IUIAutomationElement> firstRelationship;
    wil::com_ptr_nothrow<IUIAutomationElement> latestRelationship;
    wil::unique_event relationshipCleared{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    wil::unique_event callbacksDrained{CreateEventW(nullptr, TRUE, TRUE, nullptr)};

    void CloseCallbacks() noexcept
    {
        const std::scoped_lock lock(callbackMutex);
        acceptingCallbacks = false;
    }

    bool AdmitCallback() noexcept
    {
        const std::scoped_lock lock(callbackMutex);
        if (! acceptingCallbacks)
            return false;
        ++activeCallbacks;
        ++callbackActivity;
        ResetEvent(callbacksDrained.get());
        return true;
    }

    void LeaveCallback() noexcept
    {
        const std::scoped_lock lock(callbackMutex);
        ++callbackActivity;
        if (--activeCallbacks == 0u)
            SetEvent(callbacksDrained.get());
    }

    std::mutex callbackMutex;
    bool acceptingCallbacks = true;
    size_t activeCallbacks  = 0u;

    HRESULT STDMETHODCALLTYPE HandlePropertyChangedEvent(IUIAutomationElement* sender, PROPERTYID property, VARIANT eventValue) noexcept override
    {
        if (! AdmitCallback())
            return S_OK;
        const auto leave = wil::scope_exit([&]() noexcept { LeaveCallback(); });
        if (relationshipEvent)
        {
            if (property != UIA_LabeledByPropertyId)
                return S_OK;
            if (expectClearedRelationship.load() && (eventValue.vt == VT_EMPTY || (eventValue.vt == VT_UNKNOWN && ! eventValue.punkVal)))
            {
                if (clearedRelationshipClaimed.exchange(true, std::memory_order_acq_rel))
                    return S_OK;
                wil::com_ptr_nothrow<IUIAutomationElement> current;
                HRESULT result = sender->get_CurrentLabeledBy(current.put());
                if (SUCCEEDED(result) && current)
                    result = E_FAIL;
                wil::unique_bstr name;
                const HRESULT firstResult = firstRelationship ? firstRelationship->get_CurrentName(name.put()) : E_FAIL;
                name.reset();
                const HRESULT latestResult = latestRelationship ? latestRelationship->get_CurrentName(name.put()) : E_FAIL;
                if (SUCCEEDED(result) && (firstResult != UIA_E_ELEMENTNOTAVAILABLE || latestResult != UIA_E_ELEMENTNOTAVAILABLE))
                    result = E_FAIL;
                clearedRelationshipResult.store(result);
                SetEvent(relationshipCleared.get());
                return S_OK;
            }
            wil::com_ptr_nothrow<IUIAutomationElement> label;
            HRESULT result = eventValue.vt == VT_UNKNOWN && eventValue.punkVal ? eventValue.punkVal->QueryInterface(IID_PPV_ARGS(label.put())) : E_FAIL;
            wil::unique_bstr name;
            if (SUCCEEDED(result))
                result = label->get_CurrentName(name.put());
            const std::wstring_view text = name ? std::wstring_view(name.get(), SysStringLen(name.get())) : std::wstring_view{};
            std::cerr << "[RELATIONSHIP EVENT] type=" << eventValue.vt << " result=" << static_cast<unsigned long>(result)
                      << " first=" << (text == L"Form label first β") << " final=" << (text == L"Form label final Ω")
                      << " pending=" << (text == L"Form label pending") << " gateOpen=" << (WaitForSingleObject(continueDelivery.get(), 0) == WAIT_OBJECT_0)
                      << '\n';
            if (SUCCEEDED(result) && text == L"Form label first β")
            {
                // UIA can deliver the same literal relationship more than once through the bridge. Each
                // payload was queried above; only the first valid copy owns the reentrant action and pointer.
                if (heardFirstRelationship.exchange(true, std::memory_order_acq_rel))
                {
                    ++duplicateFirstRelationships;
                    return S_OK;
                }
                firstRelationship       = std::move(label);
                firstRelationshipResult = result;
                return HandleNotification(StructureChangeType_ChildrenInvalidated);
            }
            if (SUCCEEDED(result) && text == L"Form label final Ω")
            {
                if (latestRelationshipClaimed.exchange(true, std::memory_order_acq_rel))
                {
                    ++duplicateLatestRelationships;
                    return S_OK;
                }
                // UIA supplies the new relationship in the event. Its current property must name that same
                // live label after the queued changes, on this real client's apartment.
                wil::com_ptr_nothrow<IUIAutomationElement> current;
                result = sender->get_CurrentLabeledBy(current.put());
                wil::unique_bstr currentName;
                if (SUCCEEDED(result))
                    result = current ? current->get_CurrentName(currentName.put()) : E_FAIL;
                if (SUCCEEDED(result) && (! currentName || std::wstring_view(currentName.get(), SysStringLen(currentName.get())) != text))
                    result = E_FAIL;
                latestRelationship = std::move(label);
                latestRelationshipResult.store(result);
                heardLatestProperty.store(true);
            }
            else
                ++intermediateRelationships;
            return S_OK;
        }
        if (property == UIA_NamePropertyId && eventValue.vt == VT_BSTR && eventValue.bstrVal &&
            std::wstring_view(eventValue.bstrVal, SysStringLen(eventValue.bstrVal)) == L"Root replacement dernier Ω")
            heardLatestProperty.store(true);
        return property == UIA_NamePropertyId ? HandleNotification(StructureChangeType_ChildrenInvalidated) : S_OK;
    }

    HRESULT STDMETHODCALLTYPE HandleStructureChangedEvent(IUIAutomationElement*, StructureChangeType change, SAFEARRAY*) noexcept override
    {
        if (! AdmitCallback())
            return S_OK;
        const auto leave = wil::scope_exit([&]() noexcept { LeaveCallback(); });
        return HandleNotification(change);
    }

    HRESULT HandleNotification(StructureChangeType change) noexcept
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
        if (relationshipEvent)
        {
            wil::unique_bstr labelName;
            const HRESULT labelResult = firstRelationship ? firstRelationship->get_CurrentName(labelName.put()) : E_FAIL;
            retainedRelationshipResult.store(
                SUCCEEDED(labelResult) && labelName && std::wstring_view(labelName.get(), SysStringLen(labelName.get())) == L"Form label first β" ? S_OK
                : FAILED(labelResult)                                                                                                             ? labelResult
                                                                                                                                                  : E_FAIL);
        }
        wil::unique_bstr name;
        HRESULT result = lastRow->get_CurrentName(name.put()); // A real client calls back into the new snapshot.
        if (SUCCEEDED(result) && (! name || std::wstring_view(name.get(), SysStringLen(name.get())) != L"Omega nouveau Ω"))
            result = E_FAIL;
        readResult.store(result);
        invokeResult.store(probeOwner->Invoke());
        if (retireRoot)
        {
            replaceResult.store(replaceRoot->Invoke()); // Standard UIA action destroys the Tree on its owner.
            if (relationshipEvent)
            {
                wil::unique_bstr retiredName;
                retiredRelationshipResult.store(firstRelationship ? firstRelationship->get_CurrentName(retiredName.put()) : E_FAIL);
            }
        }
        SetEvent(completed.get());
        return S_OK;
    }
};

static void RunPreparedTreeReentrantEmbeddedClient(GraphicsFixture& gpu, bool coalescedBurst, bool propertyEvent = false, bool relationshipEvent = false)
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
#if defined(DXUI_HAS_INTRINSIC_LAYOUT)
    std::array<Label*, 34u> relationshipLabels{};
    if (relationshipEvent)
    {
        // Every label remains alive while the relationship changes. A retained event element can still be
        // queried, but must never keep the unrelated prepared Tree generation from its creation alive.
        for (size_t index = 0u; index < relationshipLabels.size(); ++index)
        {
            relationshipLabels[index] = root->AddChild<Label>(index == 0u                               ? L"Form label initial α"
                                                              : index == 1u                             ? L"Form label first β"
                                                              : index + 1u == relationshipLabels.size() ? L"Form label final Ω"
                                                                                                        : L"Form label pending");
            relationshipLabels[index]->SetBounds(D2D1::RectF(208, 128, 312, 160));
        }
        Hr(button->SetAccessibleFormProperties(relationshipLabels.front(), false, true), "set the initial embedded form relationship");
    }
#endif
    view.Controls().SetRoot(std::move(root));
    Hr(view.Prepare(320, 240), "prepare embedded event source");
    Hr(bridge.Attach(false, 9317u), "attach embedded event source");
    wil::com_ptr_nothrow<EmbeddedPreparedTreeObserver> observer;
    observer.attach(Microsoft::WRL::Make<EmbeddedPreparedTreeObserver>().Detach());
    Check(observer && observer->completed && observer->entered && observer->continueDelivery && observer->relationshipCleared && observer->callbacksDrained,
          "prepared Tree allocates the real client observer and delivery gates");
    const bool gatedPropertyBatch =
#if defined(DXUI_HAS_INTRINSIC_LAYOUT) && DXUI_ENABLE_DIAGNOSTICS
        coalescedBurst && relationshipEvent;
#else
        false;
#endif
    observer->gateDelivery      = coalescedBurst && ! gatedPropertyBatch;
    observer->retireRoot        = ! (propertyEvent && coalescedBurst);
    observer->relationshipEvent = relationshipEvent;
    wil::unique_event ready(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    wil::unique_event stop(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    wil::unique_event finished(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    wil::unique_event batchEntered(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    wil::unique_event batchRelease(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Check(ready && stop && finished && batchEntered && batchRelease, "prepared Tree client lifetime events exist");
    std::atomic<HRESULT> setup{E_PENDING};
    std::atomic<HRESULT> teardown{E_PENDING};
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
        if (SUCCEEDED(result) && relationshipEvent)
        {
            wil::com_ptr_nothrow<IUIAutomationElement> initial;
            result = buttonElement->get_CurrentLabeledBy(initial.put());
            wil::unique_bstr name;
            if (SUCCEEDED(result))
                result = initial ? initial->get_CurrentName(name.put()) : E_FAIL;
            if (SUCCEEDED(result) && (! name || std::wstring_view(name.get(), SysStringLen(name.get())) != L"Form label initial α"))
                result = E_FAIL;
        }
        if (SUCCEEDED(result))
        {
            if (propertyEvent)
            {
                PROPERTYID property = relationshipEvent ? UIA_LabeledByPropertyId : UIA_NamePropertyId;
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
            observer->CloseCallbacks();
            if (propertyEvent)
                result = automation->RemovePropertyChangedEventHandler(element.get(), observer.get());
            else
                result = automation->RemoveStructureChangedEventHandler(element.get(), observer.get());
        }
        else
            observer->CloseCallbacks();
        // Unsubscription can return while an already accepted callback is still running. The owner keeps
        // pumping until finished; only after admission is closed and accepted callbacks drain can these reset.
        if (WaitForSingleObject(observer->callbacksDrained.get(), 20000u) == WAIT_OBJECT_0)
        {
            observer->lastRow.reset();
            observer->replaceRoot.reset();
            observer->probeOwner.reset();
            observer->firstRelationship.reset();
            observer->latestRelationship.reset();
        }
        else
            result = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
        teardown.store(result);
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
        Check(SUCCEEDED(teardown.load()), "accepted prepared Tree callbacks drain before client references retire");
    });
    Check(waitFor(ready.get()) && SUCCEEDED(setup.load()), "real client subscribes to prepared Tree structure changes");
    Check(probeActions == 1, "real client owner action succeeds outside the notification callback");
#if defined(DXUI_HAS_INTRINSIC_LAYOUT) && DXUI_ENABLE_DIAGNOSTICS
    const auto releasePropertyGate = wil::scope_exit([&]() noexcept
    {
        if (gatedPropertyBatch)
        {
            SetEvent(batchRelease.get());
            DebugSetAccessibilityPropertyDeliveryGateForTest(nullptr, nullptr, nullptr);
        }
    });
    if (gatedPropertyBatch)
        DebugSetAccessibilityPropertyDeliveryGateForTest(&view.Controls(), batchEntered.get(), batchRelease.get());
#endif
    observer->armed.store(true, std::memory_order_release);
    const auto notificationWitness = relationshipEvent ? std::make_shared<PreparedTreeSourceWitness>() : witness;
    model.Replace(
        std::make_shared<ThreePreparedTreeRows>(ThreePreparedTreeRows::Names{L"Alpha α", L"Beta nouveau β", L"Omega nouveau Ω"}, notificationWitness));
#if defined(DXUI_HAS_INTRINSIC_LAYOUT)
    if (relationshipEvent)
        Hr(button->SetAccessibleFormProperties(relationshipLabels[1u], false, true), "change the embedded form relationship");
    else
#endif
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
        Check(waitFor(gatedPropertyBatch ? batchEntered.get() : observer->entered.get()),
              "the active notification batch or real callback enters its explicit gate before the publication burst");
        std::array<std::shared_ptr<PreparedTreeSourceWitness>, 32u> burstWitnesses;
        for (size_t index = 0u; index < burstWitnesses.size(); ++index)
        {
            burstWitnesses[index] = std::make_shared<PreparedTreeSourceWitness>();
            model.Replace(std::make_shared<ThreePreparedTreeRows>(
                ThreePreparedTreeRows::Names{L"Alpha α", L"Beta nouveau β", index + 1u == burstWitnesses.size() ? L"Omega nouveau Ω" : L"Omega en attente Ω"},
                burstWitnesses[index]));
            tree->NotifyDataChanged();
#if defined(DXUI_HAS_INTRINSIC_LAYOUT)
            if (relationshipEvent)
                Hr(button->SetAccessibleFormProperties(relationshipLabels[index + 2u], false, true), "queue a later embedded form relationship");
            else
#endif
                if (propertyEvent)
                button->SetAccessibleName(index + 1u == burstWitnesses.size() ? L"Root replacement dernier Ω" : L"Root replacement attente Ω");
            Hr(view.Prepare(320, 240), "prepare changed embedded source");
            Hr(bridge.Update(), "publish changed embedded source");
        }
        for (size_t index = 0u; index + 1u < burstWitnesses.size(); ++index)
            Check(burstWitnesses[index]->destructions.load() == 1u, "a held notification does not retain intermediate prepared source generations");
        if (relationshipEvent)
            Check(notificationWitness->destructions.load() == 1u, "a retained relationship notification value does not pin its prepared row source");
        Check(burstWitnesses.back()->destructions.load() == 0u && replacements == 0u,
              "the final coherent source stays alive and the client action waits behind its explicit gate");
        SetEvent(observer->continueDelivery.get());
        SetEvent(batchRelease.get());
    }
    const bool notified = waitFor(observer->completed.get());
    std::cerr << "[PREPARED TREE CLIENT] notified=" << notified << " read=" << std::hex << observer->readResult.load()
              << " invoke=" << observer->invokeResult.load() << std::dec << " replacements=" << replacements << '\n';
    Check(notified, "same-count same-id prepared source invalidates the real client");
    Check(observer->readResult.load() == S_OK, "real notification callback reads the new complete row name");
    Check(observer->invokeResult.load() == S_OK, "real notification callback invokes a surviving owner action");
    Check(probeActions == 2, "callback owner action succeeds exactly once");
    if (relationshipEvent)
    {
        Check(observer->firstRelationshipResult.load() == S_OK, "real UIA client reads the first marshalled relationship event element");
        Check(observer->retainedRelationshipResult.load() == S_OK, "retained relationship event element resolves its live label after publication");
    }
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
        if (relationshipEvent)
        {
            Check(observer->latestRelationshipResult.load() == S_OK, "latest relationship event and current LabeledBy property name the same live label");
            Check(waitFor(observer->callbacksDrained.get()), "relationship callbacks finish before the event set is reviewed");
            const auto quiet = UiaTest::WaitForQuiescence(250u,
                                                          5000u,
                                                          [] { return GetTickCount64(); },
                                                          [&] { return observer->callbackActivity.load(); },
                                                          [&]
            {
                bridge.Pump();
                static_cast<void>(MsgWaitForMultipleObjectsEx(0, nullptr, 5u, QS_ALLINPUT, MWMO_INPUTAVAILABLE));
            });
            Check(quiet.quiet && WaitForSingleObject(observer->callbacksDrained.get(), 0) == WAIT_OBJECT_0,
                  "relationship event review records bounded quiet with no admitted callback still using the labels");
            if (gatedPropertyBatch)
                Check(observer->intermediateRelationships.load() == 0u, "one pending relationship batch coalesces to the latest distinct value");
            std::cerr << "[RELATIONSHIP DELIVERY] intermediate=" << observer->intermediateRelationships.load()
                      << " duplicateFirst=" << observer->duplicateFirstRelationships.load()
                      << " duplicateFinal=" << observer->duplicateLatestRelationships.load() << " gatedBatch=" << gatedPropertyBatch << '\n';
#if defined(DXUI_HAS_INTRINSIC_LAYOUT)
            observer->expectClearedRelationship.store(true);
            auto* const panel = dynamic_cast<Panel*>(view.Controls().GetRoot());
            Check(panel != nullptr, "relationship clear preserves the current panel");
            for (auto* label : {relationshipLabels[1u], relationshipLabels.back()})
            {
                bool removed = false;
                for (size_t index = 0u; index < panel->GetLogicalChildCount(); ++index)
                    if (panel->GetLogicalChild(index) == label)
                    {
                        auto retired = panel->TakeChild(index);
                        Check(retired.get() == label, "retire the client-held relationship label through supported ownership transfer");
                        removed = true;
                        break; // The owned label dies here before the clear publication.
                    }
                Check(removed, "each client-held relationship label belonged to the current panel");
            }
            Hr(button->SetAccessibleFormProperties(nullptr, false, true), "clear the expired form relationship");
            Hr(view.Prepare(320, 240), "prepare cleared embedded relationship");
            Hr(bridge.Update(), "publish cleared embedded relationship");
            Check(waitFor(observer->relationshipCleared.get()), "real UIA client hears the empty relationship after its old label retired");
            Check(observer->clearedRelationshipResult.load() == S_OK,
                  "cleared relationship is empty and both retained event elements report unavailable after label retirement");
#endif
        }
        Check(replacements == 0, "held property delivery preserves the current source and control");
        return;
    }
    Check(observer->replaceResult.load() == UIA_E_ELEMENTNOTAVAILABLE, "retiring its control reports unavailable after one acknowledged root action");
    Check(replacements == 1 && view.Controls().GetRoot()->GetAccessibleName() == L"Acknowledged replacement",
          "the real UIA event callback replaces the root exactly once without using the removed Tree");
    if (relationshipEvent)
        Check(observer->retiredRelationshipResult.load() == UIA_E_ELEMENTNOTAVAILABLE,
              "client-retained relationship event element reports unavailable after root replacement");
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

#if defined(DXUI_HAS_INTRINSIC_LAYOUT)
static void TestPreparedTreeLabeledByEventPermitsOwnerReentry(GraphicsFixture& gpu)
{
    RunPreparedTreeReentrantEmbeddedClient(gpu, false, true, true);
}

static void TestPreparedTreeLabeledByBurstCoalescesWithoutRetainingSources(GraphicsFixture& gpu)
{
    RunPreparedTreeReentrantEmbeddedClient(gpu, true, true, true);
}
#endif
