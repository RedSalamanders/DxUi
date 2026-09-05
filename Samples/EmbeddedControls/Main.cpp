#include "../ComplexUi/ComplexUiScene.h"
#include "EmbeddedScene.h"
#include "EmbeddedTextClient.h"
#include "GraphicsFixture.h"
#include <iostream>
#include <windowsx.h>
#pragma comment(lib, "user32.lib")
namespace
{
constexpr UINT RenderMessage = WM_APP + 1;
struct App
{
    GraphicsFixture gpu;
    EmbeddedScene scene;
    std::unique_ptr<ComplexUiScene> complex;
    DxUi::EmbeddedHost& View() noexcept
    {
        return complex ? complex->view : scene.view;
    }
    wil::com_ptr_nothrow<IDXGISwapChain> swapChain;
    wil::unique_hwnd window;
    DxUi::TextInputServices textServices;
    std::shared_ptr<EmbeddedTextClient> textClient;
    bool textInput = false;
    ~App()
    {
        ClearText();
        textServices.Detach();
    }
    void ClearText() noexcept
    {
        textServices.ClearClient();
        if (textClient)
            textClient->Disconnect();
        textClient.reset();
    }
    void RefreshText() noexcept
    {
        try
        {
            if (! textInput || ! window || GetFocus() != window.get())
            {
                ClearText();
                return;
            }
            DxUi::EmbeddedTextInputSnapshot snapshot;
            if (View().ReadTextInput(snapshot) != S_OK)
            {
                ClearText();
                return;
            }
            if (! textClient || textClient->FocusId() != snapshot.focusId)
            {
                ClearText();
                textClient = std::make_shared<EmbeddedTextClient>(View(), window.get(), snapshot.focusId, dpi);
            }
            textClient->SetDpi(dpi);
            if (! textServices.HasClient() && textServices.SetClient(textClient) != S_OK)
            {
                ClearText();
                return;
            }
            textServices.NotifyChanged();
        }
        catch (const std::bad_alloc&)
        {
            ClearText();
        }
    }
    static UINT Modifiers() noexcept
    {
        return ((GetKeyState(VK_SHIFT) & 0x8000) ? MK_SHIFT : 0U) | ((GetKeyState(VK_CONTROL) & 0x8000) ? MK_CONTROL : 0U);
    }
    bool queued = false;
    float dpi   = 96;
    void Request() noexcept
    {
        if (window && ! queued)
        {
            queued = PostMessageW(window.get(), RenderMessage, 0, 0) != FALSE;
        }
    }
    HRESULT Render()
    {
        queued = false;
        if (window)
        {
            RECT client{};
            if (! GetClientRect(window.get(), &client))
                return HRESULT_FROM_WIN32(GetLastError());
            if (client.right <= 0 || client.bottom <= 0 || IsIconic(window.get()))
                return S_FALSE;
            dpi              = float(GetDpiForWindow(window.get()));
            const UINT width = UINT(client.right), height = UINT(client.bottom);
            if (width != gpu.width || height != gpu.height)
            {
                gpu.context->OMSetRenderTargets(0, nullptr, nullptr);
                RETURN_IF_FAILED(swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0));
                RETURN_IF_FAILED(gpu.Resize(width, height));
            }
        }
        const HRESULT prepared = View().Prepare(gpu.width, gpu.height, dpi);
        RETURN_IF_FAILED(prepared);
        RefreshText();
        if (prepared == S_OK)
            textServices.NotifyLayoutChanged();
        gpu.Bind();
        RETURN_IF_FAILED(View().Composite(gpu.context.get(), gpu.Viewport()));
        if (swapChain)
        {
            wil::com_ptr_nothrow<ID3D11Texture2D> back;
            RETURN_IF_FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(back.put())));
            gpu.context->CopyResource(back.get(), gpu.target.get());
            return swapChain->Present(1, 0);
        }
        return S_OK;
    }
    static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) noexcept
    {
        auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE)
        {
            app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }
        if (! app)
            return DefWindowProcW(hwnd, msg, wp, lp);
        try
        {
            bool textHandled = false;
            static_cast<void>(app->textServices.HandleMessage(msg, wp, lp, textHandled));
            if (textHandled)
            {
                app->RefreshText();
                return 0;
            }
            const auto refresh = wil::scope_exit([&] { app->RefreshText(); });
            switch (msg)
            {
                case WM_PAINT:
                {
                    PAINTSTRUCT paint{};
                    BeginPaint(hwnd, &paint);
                    EndPaint(hwnd, &paint);
                    app->Request();
                    return 0;
                }
                case RenderMessage:
                    if (FAILED(app->Render()))
                        PostQuitMessage(1);
                    return 0;
                case WM_LBUTTONDOWN:
                    SetFocus(hwnd);
                    SetCapture(hwnd);
                    app->View().DispatchPointer({DxUi::PointerAction::Down, float(GET_X_LPARAM(lp)), float(GET_Y_LPARAM(lp)), UINT(wp)});
                    return 0;
                case WM_MOUSEMOVE:
                    app->View().DispatchPointer({DxUi::PointerAction::Move, float(GET_X_LPARAM(lp)), float(GET_Y_LPARAM(lp)), UINT(wp)});
                    return 0;
                case WM_MOUSEWHEEL:
                {
                    POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                    if (ScreenToClient(hwnd, &point))
                        app->View().DispatchPointer(
                            {DxUi::PointerAction::Wheel, float(point.x), float(point.y), GET_KEYSTATE_WPARAM(wp), float(GET_WHEEL_DELTA_WPARAM(wp))});
                    return 0;
                }
                case WM_LBUTTONUP:
                    app->View().DispatchPointer({DxUi::PointerAction::Up, float(GET_X_LPARAM(lp)), float(GET_Y_LPARAM(lp)), UINT(wp)});
                    if (GetCapture() == hwnd)
                        ReleaseCapture();
                    return 0;
                case WM_CAPTURECHANGED: app->View().DispatchPointer({DxUi::PointerAction::Cancel}); return 0;
                case WM_KEYDOWN: app->View().DispatchKey(UINT(wp), true, Modifiers()); return 0;
                case WM_KEYUP: app->View().DispatchKey(UINT(wp), false, Modifiers()); return 0;
                case WM_CHAR: app->View().DispatchCharacter(wchar_t(wp), Modifiers()); return 0;
                case WM_KILLFOCUS:
                    app->ClearText();
                    app->View().CancelTextInput();
                    return 0;
                case WM_DPICHANGED:
                {
                    app->ClearText();
                    app->View().CancelTextInput();
                    const auto* bounds = reinterpret_cast<const RECT*>(lp);
                    SetWindowPos(
                        hwnd, nullptr, bounds->left, bounds->top, bounds->right - bounds->left, bounds->bottom - bounds->top, SWP_NOZORDER | SWP_NOACTIVATE);
                    app->Request();
                    return 0;
                }
                case WM_SHOWWINDOW: app->View().SetVisible(wp != 0); break;
                case WM_SIZE:
                    if (wp == SIZE_MINIMIZED)
                        app->View().SetVisible(false);
                    else
                    {
                        app->View().SetVisible(true);
                        app->Request();
                    }
                    return 0;
                case WM_CLOSE: app->window.reset(); return 0;
                case WM_DESTROY: PostQuitMessage(0); return 0;
                case WM_NCDESTROY:
                    app->ClearText();
                    app->textServices.Detach();
                    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                    break;
                default: break;
            }
        }
        catch (const std::exception&)
        {
            PostQuitMessage(1);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    HRESULT Run()
    {
        const auto previousDpi = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        const auto restoreDpi  = wil::scope_exit([&]
        {
            if (previousDpi)
                SetThreadDpiAwarenessContext(previousDpi);
        });
        WNDCLASSW wc{};
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpfnWndProc   = Proc;
        wc.lpszClassName = L"DxUi.EmbeddedControls";
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        if (! RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return HRESULT_FROM_WIN32(GetLastError());
        constexpr DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT bounds{0, 0, static_cast<LONG>(gpu.width), static_cast<LONG>(gpu.height)};
        AdjustWindowRectEx(&bounds, style, FALSE, 0);
        window.reset(CreateWindowExW(0,
                                     wc.lpszClassName,
                                     L"DxUi - supplied Direct3D device",
                                     style,
                                     CW_USEDEFAULT,
                                     CW_USEDEFAULT,
                                     bounds.right - bounds.left,
                                     bounds.bottom - bounds.top,
                                     nullptr,
                                     nullptr,
                                     wc.hInstance,
                                     this));
        if (! window)
            return HRESULT_FROM_WIN32(GetLastError());
        wil::com_ptr_nothrow<IDXGIDevice> dxgi;
        RETURN_IF_FAILED(gpu.device->QueryInterface(IID_PPV_ARGS(dxgi.put())));
        wil::com_ptr_nothrow<IDXGIAdapter> adapter;
        RETURN_IF_FAILED(dxgi->GetAdapter(adapter.put()));
        wil::com_ptr_nothrow<IDXGIFactory> factory;
        RETURN_IF_FAILED(adapter->GetParent(IID_PPV_ARGS(factory.put())));
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferDesc.Width  = gpu.width;
        desc.BufferDesc.Height = gpu.height;
        desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count  = 1;
        desc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount       = 2;
        desc.OutputWindow      = window.get();
        desc.Windowed          = TRUE;
        desc.SwapEffect        = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        RETURN_IF_FAILED(factory->CreateSwapChain(gpu.device.get(), &desc, swapChain.put()));
        factory->MakeWindowAssociation(window.get(), DXGI_MWA_NO_ALT_ENTER);
        if (textInput)
            RETURN_IF_FAILED(textServices.Attach(window.get()));
        dpi = float(GetDpiForWindow(window.get()));
        RECT scaled{0, 0, LONG(float(gpu.width) * dpi / 96), LONG(float(gpu.height) * dpi / 96)};
        if (! AdjustWindowRectExForDpi(&scaled, style, FALSE, 0, UINT(dpi)))
            return HRESULT_FROM_WIN32(GetLastError());
        SetWindowPos(window.get(), nullptr, 0, 0, scaled.right - scaled.left, scaled.bottom - scaled.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(window.get(), SW_SHOWNORMAL);
        Request();
        MSG message{};
        BOOL got = 0;
        while ((got = GetMessageW(&message, nullptr, 0, 0)) > 0)
        {
            RefreshText();
            bool handled = false;
            static_cast<void>(textServices.PreTranslate(message, handled));
            if (handled)
            {
                RefreshText();
                continue;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return got < 0 ? HRESULT_FROM_WIN32(GetLastError()) : (message.wParam ? E_FAIL : S_OK);
    }
};
} // namespace
int wmain(int argc, wchar_t** argv)
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return 1;
    const auto com = wil::scope_exit([] { CoUninitialize(); });
    try
    {
        bool complex          = false;
        bool textInput        = false;
        const wchar_t* output = nullptr;
        for (int i = 1; i < argc; ++i)
        {
            if (std::wstring_view(argv[i]) == L"--complex-ui" && ! complex)
                complex = true;
            else if (std::wstring_view(argv[i]) == L"--text-input" && ! textInput)
                textInput = true;
            else if (std::wstring_view(argv[i]) == L"--output" && ! output && i + 1 < argc)
                output = argv[++i];
            else
            {
                std::wcerr << L"Usage: DxUi.EmbeddedControls [--complex-ui | --text-input] [--output image.png]\n";
                return 2;
            }
        }
        if (complex && textInput)
            return 2;
        App app;
        app.textInput = textInput;
        if (textInput)
            app.gpu.height = 340;
        if (complex)
        {
            app.complex    = std::make_unique<ComplexUiScene>();
            app.gpu.width  = 1280;
            app.gpu.height = 720;
        }
        if (FAILED(app.gpu.Create()))
            return 1;
        const DxUi::EmbeddedCallbacks callbacks{&app, [](void* context) noexcept { static_cast<App*>(context)->Request(); }};
        if (FAILED(complex ? app.complex->Initialize(app.gpu.device.get(), callbacks) : app.scene.Initialize(app.gpu.device.get(), callbacks, textInput)))
            return 1;
        if (output)
        {
            if (FAILED(app.Render()))
                return 1;
            if (textInput)
            {
                wil::unique_hwnd hidden(
                    CreateWindowExW(0, L"STATIC", L"DxUi public text proof", WS_POPUP, 0, 0, 480, 340, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr));
                if (! hidden)
                    return 1;
                app.View().Controls().SetFocusControl(app.scene.text);
                if (FAILED(app.View().Prepare(480, 340)))
                    return 1;
                DxUi::EmbeddedTextInputSnapshot state;
                if (app.View().ReadTextInput(state) != S_OK)
                    return 1;
                auto client = std::make_shared<EmbeddedTextClient>(app.View(), hidden.get(), state.focusId, 96.0f);
                DxUi::TextInputServices service;
                const auto detach = wil::scope_exit([&]
                {
                    service.Detach();
                    client->Disconnect();
                });
                if (FAILED(service.Attach(hidden.get())) || service.SetClient(client) != S_OK)
                    return 1;
                RECT bounds{};
                bool clipped = true;
                if (client->RangeBounds(0, 4, bounds, clipped) != S_OK || bounds.right <= bounds.left)
                    return 1;
                struct PrivateClipboard final : DxUi::TextClipboard
                {
                    HRESULT Read(HWND, std::wstring& text) noexcept override
                    {
                        try
                        {
                            text = L" \u6771\u4eac";
                            return S_OK;
                        }
                        catch (...)
                        {
                            return E_OUTOFMEMORY;
                        }
                    }
                    HRESULT Write(HWND, std::wstring_view) noexcept override
                    {
                        return E_NOTIMPL;
                    }
                } clipboard;
                app.scene.text->SetSelectionRange(state.state.text.size(), state.state.text.size());
                if (service.Clipboard(DxUi::TextClipboardCommand::Paste, &clipboard) != S_OK)
                    return 1;
                if (app.scene.text->GetText() != L"Desk profile \u6771\u4eac")
                    return 1;
                if (FAILED(app.View().Prepare(480, 340)))
                    return 1;
                service.NotifyLayoutChanged();
            }
            if (FAILED(app.Render()) || FAILED(app.gpu.Save(output)))
                return 1;
            std::wcout << L"Rendered independent DxUi sample to " << output << L'\n';
            return 0;
        }
        return FAILED(app.Run()) ? 1 : 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
