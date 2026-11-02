#pragma once

#include "DxUiTestHelpers.h"

#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <wrl/implements.h>

class FocusNameRecorder final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                                                                    IUIAutomationFocusChangedEventHandler,
                                                                    Microsoft::WRL::FtmBase>
{
public:
    HRESULT STDMETHODCALLTYPE HandleFocusChangedEvent(IUIAutomationElement* sender) noexcept override
    {
        if (! sender)
            return S_OK;
        int processId = 0;
        const HRESULT processResult = sender->get_CachedProcessId(&processId);
        // Both properties are cached; querying them here does not call into the provider thread.
        wil::unique_bstr name;
        const HRESULT nameResult = sender->get_CachedName(name.put());
        const bool acceptedProcess = SUCCEEDED(processResult) && static_cast<DWORD>(processId) == GetCurrentProcessId();
        try
        {
            const std::scoped_lock lock(_mutex);
            if (SUCCEEDED(nameResult) && name)
            {
                const std::wstring_view observed(name.get(), SysStringLen(name.get()));
                // Capture only invented fixture names; never retain arbitrary external-window names.
                if (observed == L"Essai de focus" || observed == L"Premier bouton" || observed == L"Second bouton")
                {
                    if (_observationCount < _observations.size())
                    {
                        const int kind = observed == L"Essai de focus" ? 0 : observed == L"Premier bouton" ? 1 : 2;
                        _observations[_observationCount++] = {kind, nameResult, processResult, processId, acceptedProcess};
                    }
                    else
                    {
                        ++_omittedObservations;
                    }
                }
            }
            else
            {
                ++_unreadableNames;
                _lastNameFailure = nameResult;
            }
            if (! acceptedProcess || FAILED(nameResult) || ! name)
                return S_OK;
            _names.emplace_back(name.get(), SysStringLen(name.get()));
        }
        catch (const std::bad_alloc&)
        {
            // A dropped name fails the test's own count; the client callback never throws.
        }
        return S_OK;
    }

    [[nodiscard]] size_t Count(std::wstring_view name) const
    {
        const std::scoped_lock lock(_mutex);
        return static_cast<size_t>(std::ranges::count(_names, name));
    }

    [[nodiscard]] std::wstring LastName() const
    {
        const std::scoped_lock lock(_mutex);
        return _names.empty() ? std::wstring{} : _names.back();
    }

    [[nodiscard]] std::vector<std::wstring> Names() const
    {
        const std::scoped_lock lock(_mutex);
        return _names;
    }

    void PrintCachedIdentityDiagnostics() const
    {
        const std::scoped_lock lock(_mutex);
        std::cerr << "    [UIA-CACHED-IDENTITY] expectedProcessId=" << GetCurrentProcessId() << " unreadableNames=" << _unreadableNames << " lastNameHr=" << static_cast<uint32_t>(_lastNameFailure) << " observations=" << _observationCount << " omitted=" << _omittedObservations << '\n';
        for (size_t i = 0; i < _observationCount; ++i)
        {
            const auto& event = _observations[i];
            std::cerr << "    [UIA-CACHED-IDENTITY] kind=" << event.kind << " nameHr=" << static_cast<uint32_t>(event.nameResult) << " processHr=" << static_cast<uint32_t>(event.processResult)
                      << " processId=" << event.processId << " accepted=" << event.acceptedProcess << '\n';
        }
    }

private:
    struct CachedIdentityObservation
    {
        int kind = 0; // 0 window, 1 first button, 2 second button.
        HRESULT nameResult = E_FAIL;
        HRESULT processResult = E_FAIL;
        int processId = 0;
        bool acceptedProcess = false;
    };
    mutable std::mutex _mutex;
    std::vector<std::wstring> _names;
    std::array<CachedIdentityObservation, 64> _observations{};
    size_t _observationCount = 0u;
    size_t _omittedObservations = 0u;
    size_t _unreadableNames = 0u;
    HRESULT _lastNameFailure = S_OK;
};

// An in-process UIA client on its own MTA thread, as a screen reader is another process: it records the names of
// focus-changed events and focuses a named element of the window on request. Providers answer on the window's thread,
// so every wait pumps it.
class FocusEventClient final
{
public:
    static constexpr ULONGLONG kSetupAllowanceMs       = 20000; // Cold client setup has exceeded 3 s on hosted runners.
    static constexpr ULONGLONG kNotificationDeadlineMs = 3000;

    FocusEventClient(const AttachedHostWindow& pump, HWND target) : _pump(pump), _target(target)
    {
        _recorder.attach(Microsoft::WRL::Make<FocusNameRecorder>().Detach());
        Require(_recorder != nullptr && _stop && _request, "allocate the UIA focus client");
        _thread = std::jthread([this] { Run(); });
        Require(WaitUntil(kSetupAllowanceMs, [this] { return _ready.load(); }) && SUCCEEDED(_setup.load()), "subscribe UIA focus changes");
    }

    FocusEventClient(const FocusEventClient&)            = delete;
    FocusEventClient& operator=(const FocusEventClient&) = delete;

    ~FocusEventClient()
    {
        SetEvent(_stop.get());
        // The thread member joins after this without pumping, and the client's teardown may need this thread's providers to
        // answer: wait for it here, pumping, as long as its setup was allowed, and fail instead of hanging in the join.
        Require(WaitUntil(kSetupAllowanceMs, [this] { return _finished.load(); }), "the UIA focus client thread ends");
    }

    template <typename Predicate> [[nodiscard]] bool WaitUntil(ULONGLONG timeoutMs, const Predicate& predicate) const
    {
        const ULONGLONG deadline = GetTickCount64() + timeoutMs;
        while (! predicate() && GetTickCount64() < deadline)
        {
            _pump.PumpMessages();
            Sleep(1);
        }
        return predicate();
    }

    // Lets events already raised reach the client before a test asserts that one never came.
    void Settle() const
    {
        static_cast<void>(WaitUntil(500u, [] { return false; }));
    }

    [[nodiscard]] size_t Count(std::wstring_view name) const
    {
        return _recorder->Count(name);
    }

    [[nodiscard]] std::wstring LastName() const
    {
        return _recorder->LastName();
    }

    // For a failure report: every name heard, in order.
    void PrintNames() const
    {
        std::cerr << "    [UIA] focus names heard:";
        for (const std::wstring& name : _recorder->Names())
        {
            std::cerr << " '";
            for (const wchar_t unit : name)
                std::cerr << (unit < 0x80 ? static_cast<char>(unit) : '?');
            std::cerr << "'";
        }
        std::cerr << '\n';
        _recorder->PrintCachedIdentityDiagnostics();
    }

    // IUIAutomationElement::SetFocus on the target window's element with this name.
    [[nodiscard]] HRESULT FocusElementNamed(std::wstring name)
    {
        _requestName = std::move(name);
        _requestResult.store(E_PENDING);
        SetEvent(_request.get());
        static_cast<void>(WaitUntil(kSetupAllowanceMs, [this] { return _requestResult.load() != E_PENDING; }));
        return _requestResult.load();
    }

private:
    void Run() noexcept
    {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const auto uninitialize   = wil::scope_exit([&]
        {
            if (SUCCEEDED(initialized))
                CoUninitialize();
        });
        wil::com_ptr_nothrow<IUIAutomation> automation;
        wil::com_ptr_nothrow<IUIAutomationCacheRequest> cache;
        HRESULT hr = initialized;
        if (SUCCEEDED(hr))
            hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(automation.put()));
        if (SUCCEEDED(hr))
            hr = automation->CreateCacheRequest(cache.put());
        if (SUCCEEDED(hr))
            hr = cache->AddProperty(UIA_NamePropertyId);
        if (SUCCEEDED(hr))
            hr = cache->AddProperty(UIA_ProcessIdPropertyId);
        if (SUCCEEDED(hr))
            hr = automation->AddFocusChangedEventHandler(cache.get(), _recorder.get());
        _setup.store(hr);
        _ready.store(true);
        if (SUCCEEDED(hr))
        {
            const std::array<HANDLE, 2> events{_stop.get(), _request.get()};
            while (WaitForMultipleObjects(static_cast<DWORD>(events.size()), events.data(), FALSE, 30000) == WAIT_OBJECT_0 + 1)
                _requestResult.store(FocusNamed(*automation.get()));
            static_cast<void>(automation->RemoveFocusChangedEventHandler(_recorder.get()));
        }
        _finished.store(true);
    }

    [[nodiscard]] HRESULT FocusNamed(IUIAutomation& automation) const noexcept
    {
        wil::com_ptr_nothrow<IUIAutomationElement> root;
        HRESULT hr = automation.ElementFromHandle(_target, root.put());
        VARIANT name{};
        VariantInit(&name);
        const auto clearName = wil::scope_exit([&] { VariantClear(&name); });
        name.vt              = VT_BSTR;
        name.bstrVal         = SysAllocStringLen(_requestName.data(), static_cast<UINT>(_requestName.size()));
        if (SUCCEEDED(hr) && ! name.bstrVal)
            hr = E_OUTOFMEMORY;
        wil::com_ptr_nothrow<IUIAutomationCondition> condition;
        if (SUCCEEDED(hr))
            hr = automation.CreatePropertyCondition(UIA_NamePropertyId, name, condition.put());
        wil::com_ptr_nothrow<IUIAutomationElement> element;
        if (SUCCEEDED(hr))
            hr = root->FindFirst(TreeScope_Descendants, condition.get(), element.put());
        if (SUCCEEDED(hr) && ! element)
            hr = UIA_E_ELEMENTNOTAVAILABLE;
        if (SUCCEEDED(hr))
            hr = element->SetFocus();
        return hr;
    }

    const AttachedHostWindow& _pump;
    HWND _target = nullptr;
    wil::com_ptr_nothrow<FocusNameRecorder> _recorder;
    wil::unique_event _stop{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    wil::unique_event _request{CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    std::wstring _requestName;
    std::atomic<HRESULT> _requestResult{S_OK};
    std::atomic<HRESULT> _setup{E_PENDING};
    std::atomic<bool> _ready{false};
    std::atomic<bool> _finished{false};
    std::jthread _thread; // Last: it joins before the state it uses is destroyed.
};
