#pragma once

// What an application does to expose an embedded view to UI Automation, as a test plays it, so that a UI Automation client (see
// ../Support/UiaTestClient.h) reaches the view's elements and hears the events its accessibility bridge raises, which is what a
// screen reader does through the application's window.
//
// The application owns a window and answers WM_GETOBJECT with a provider of its own, whose only child is the root element the
// view hands out (EmbeddedHost::GetAccessibilityProvider). It adapts the view's site interface: the parent of the view's root
// element, and the fragment root of every element in it, are that application element. The embedded providers use COM threading,
// so UI Automation calls them on the application's STA, and each wait of the test pumps that thread: the client's pump is the
// bridge's `Pump()`.

#include <DxUi/Embedded.h>
#include <UIAutomation.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <atomic>

namespace EmbeddedUiaTest
{
// The application's element for its window: a pane that hosts the window and has the view's root element for its only child.
class ApplicationElement final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                                                                     IRawElementProviderSimple,
                                                                     IRawElementProviderFragment,
                                                                     IRawElementProviderFragmentRoot>
{
public:
    void Initialize(HWND hwnd) noexcept
    {
        _hwnd = hwnd;
    }

    void SetViewRoot(IRawElementProviderFragmentRoot* root) noexcept
    {
        _viewRoot = root;
    }

    void ClearViewRoot() noexcept
    {
        _viewRoot.reset();
    }

    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* options) noexcept override
    {
        if (! options)
            return E_POINTER;
        // The view's providers answer on the STA that attached it, so the element they hang from does too.
        *options = static_cast<ProviderOptions>(ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID, IUnknown** provider) noexcept override
    {
        if (! provider)
            return E_POINTER;
        *provider = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId, VARIANT* result) noexcept override
    {
        if (! result)
            return E_POINTER;
        VariantInit(result);
        switch (propertyId)
        {
            case UIA_ControlTypePropertyId:
                result->vt   = VT_I4;
                result->lVal = UIA_PaneControlTypeId;
                break;
            case UIA_NamePropertyId:
                result->vt      = VT_BSTR;
                result->bstrVal = SysAllocString(L"Application window");
                return result->bstrVal ? S_OK : E_OUTOFMEMORY;
            case UIA_IsControlElementPropertyId:
            case UIA_IsContentElementPropertyId:
            case UIA_IsEnabledPropertyId:
                result->vt      = VT_BOOL;
                result->boolVal = VARIANT_TRUE;
                break;
            default: break;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** provider) noexcept override
    {
        if (! provider)
            return E_POINTER;
        return UiaHostProviderFromHwnd(_hwnd, provider);
    }

    HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection direction, IRawElementProviderFragment** element) noexcept override
    {
        if (! element)
            return E_POINTER;
        *element = nullptr;
        if ((direction == NavigateDirection_FirstChild || direction == NavigateDirection_LastChild) && _viewRoot)
            return _viewRoot.query_to(element);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** runtimeId) noexcept override
    {
        if (! runtimeId)
            return E_POINTER;
        *runtimeId = nullptr; // A fragment root a window hosts is identified by its window.
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* rectangle) noexcept override
    {
        if (! rectangle)
            return E_POINTER;
        RECT window{};
        if (GetWindowRect(_hwnd, &window) == FALSE)
            return HRESULT_FROM_WIN32(GetLastError());
        *rectangle = {static_cast<double>(window.left),
                      static_cast<double>(window.top),
                      static_cast<double>(window.right - window.left),
                      static_cast<double>(window.bottom - window.top)};
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** roots) noexcept override
    {
        if (! roots)
            return E_POINTER;
        *roots = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetFocus() noexcept override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot** root) noexcept override
    {
        if (! root)
            return E_POINTER;
        *root = this;
        (*root)->AddRef();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(double x, double y, IRawElementProviderFragment** element) noexcept override
    {
        if (! element)
            return E_POINTER;
        *element = nullptr;
        return _viewRoot ? _viewRoot->ElementProviderFromPoint(x, y, element) : S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetFocus(IRawElementProviderFragment** element) noexcept override
    {
        if (! element)
            return E_POINTER;
        *element = nullptr;
        return _viewRoot ? _viewRoot->GetFocus(element) : S_OK;
    }

private:
    HWND _hwnd = nullptr;
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> _viewRoot;
};

// What the view asks of its application: the application element is the view root's parent and every element's fragment root.
class ApplicationSite final : public DxUi::EmbeddedAccessibilitySite
{
public:
    explicit ApplicationSite(ApplicationElement& application) noexcept : _application(&application)
    {
    }

    HRESULT Navigate(NavigateDirection direction, IRawElementProviderFragment** result) noexcept override
    {
        *result = nullptr;
        if (direction == NavigateDirection_Parent)
        {
            *result = _application;
            (*result)->AddRef();
        }
        return S_OK;
    }

    HRESULT FragmentRoot(IRawElementProviderFragmentRoot** result) noexcept override
    {
        *result = _application;
        (*result)->AddRef();
        return S_OK;
    }

    HRESULT RequestFocus() noexcept override
    {
        focusRequests.fetch_add(1u);
        return S_OK;
    }

    void ActionCompleted() noexcept override
    {
        completions.fetch_add(1u);
    }

    std::atomic<size_t> focusRequests{0u};
    std::atomic<size_t> completions{0u};

private:
    ApplicationElement* _application; // Borrowed: the bridge outlives the attachment, which it ends first.
};

// The application window, its element and the view's attachment to them. Declare it after the view and before the client: the
// client unsubscribes while the bridge still answers, and the bridge ends the attachment before the view goes.
class Bridge final
{
public:
    Bridge(DxUi::EmbeddedHost& view, UINT widthPx, UINT heightPx) : _view(view), _widthPx(widthPx), _heightPx(heightPx)
    {
        static const ATOM atom = []() noexcept
        {
            WNDCLASSW windowClass{};
            windowClass.lpfnWndProc   = &Bridge::WndProc;
            windowClass.hInstance     = GetModuleHandleW(nullptr);
            windowClass.lpszClassName = kWindowClassName;
            return RegisterClassW(&windowClass);
        }();
        _hwnd.reset(CreateWindowExW(WS_EX_NOACTIVATE,
                                    kWindowClassName,
                                    L"Application window",
                                    WS_OVERLAPPED,
                                    -32000,
                                    -32000,
                                    static_cast<int>(widthPx),
                                    static_cast<int>(heightPx),
                                    nullptr,
                                    nullptr,
                                    GetModuleHandleW(nullptr),
                                    this));
        if (! atom || ! _hwnd)
        {
            valid = false;
            return;
        }
        _application.attach(Microsoft::WRL::Make<ApplicationElement>().Detach());
        if (! _application)
        {
            valid = false;
            return;
        }
        _application->Initialize(_hwnd.get());
        _site = std::make_shared<ApplicationSite>(*_application.get());
    }

    Bridge(const Bridge&)            = delete;
    Bridge& operator=(const Bridge&) = delete;

    ~Bridge()
    {
        _view.DisconnectAccessibility(); // The view lets go of its site and of the references it gave out.
        if (_application)
        {
            _application->ClearViewRoot();
            wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
            if (SUCCEEDED(_application.query_to(simple.put())))
                static_cast<void>(UiaDisconnectProvider(simple.get()));
        }
        if (_hwnd)
            static_cast<void>(UiaReturnRawElementProvider(_hwnd.get(), 0, 0, nullptr));
        _hwnd.reset();
    }

    // Attaches the view's accessibility to the application element (the view is prepared) and shows the application's window to
    // UI Automation. The placement is the window's: its screen rectangle, with the keyboard focus the application reports.
    [[nodiscard]] HRESULT Attach(bool keyboardFocus = true, uint64_t runtimeId = 0x5E4D0001ull) noexcept
    {
        if (! valid)
            return E_FAIL;
        _placement = {{-32000.0, -32000.0, static_cast<double>(_widthPx), static_cast<double>(_heightPx)}, keyboardFocus};
        HRESULT hr = _view.AttachAccessibility(_site, runtimeId, _placement);
        if (hr != S_OK)
            return hr;
        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
        hr = _view.GetAccessibilityProvider(root.put());
        if (hr != S_OK)
            return hr;
        _application->SetViewRoot(root.get());
        return S_OK;
    }

    // Publishes the view after a preparation, as the application does on its frame: the events of what changed are raised now.
    [[nodiscard]] HRESULT Update(bool keyboardFocus) noexcept
    {
        _placement.hasKeyboardFocus = keyboardFocus;
        return _view.UpdateAccessibility(_placement);
    }

    [[nodiscard]] HRESULT Update() noexcept
    {
        return _view.UpdateAccessibility(_placement);
    }

    [[nodiscard]] HWND Hwnd() const noexcept
    {
        return _hwnd.get();
    }

    [[nodiscard]] ApplicationSite& Site() noexcept
    {
        return *_site;
    }

    // Dispatches the pending messages of this thread, which carry UI Automation's calls into the view's providers.
    void Pump() const
    {
        MSG message{};
        for (size_t processed = 0u; processed < 4096u && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE; ++processed)
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    bool valid = true;

private:
    static constexpr PCWSTR kWindowClassName = L"DxUiTests.EmbeddedBridge";

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_NCCREATE)
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<const CREATESTRUCTW*>(lParam)->lpCreateParams));
        auto* self = reinterpret_cast<Bridge*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self && self->_application && message == WM_GETOBJECT && lParam == static_cast<LPARAM>(UiaRootObjectId))
        {
            wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
            if (SUCCEEDED(self->_application.query_to(simple.put())))
                return UiaReturnRawElementProvider(hwnd, wParam, lParam, simple.get());
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    DxUi::EmbeddedHost& _view;
    UINT _widthPx  = 0u;
    UINT _heightPx = 0u;
    DxUi::EmbeddedAccessibilityPlacement _placement{};
    wil::unique_hwnd _hwnd;
    wil::com_ptr_nothrow<ApplicationElement> _application;
    std::shared_ptr<ApplicationSite> _site;
};
} // namespace EmbeddedUiaTest
