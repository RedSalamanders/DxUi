#include "EmbeddedScene.h"
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
    wil::com_ptr_nothrow<IDXGISwapChain> swapChain;
    wil::unique_hwnd window;
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
        RETURN_IF_FAILED(scene.view.Prepare(gpu.width, gpu.height, dpi));
        gpu.Bind();
        RETURN_IF_FAILED(scene.view.Composite(gpu.context.get(), gpu.Viewport()));
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
                    app->scene.view.DispatchPointer({DxUi::PointerAction::Down, float(GET_X_LPARAM(lp)), float(GET_Y_LPARAM(lp)), UINT(wp)});
                    return 0;
                case WM_MOUSEMOVE:
                    app->scene.view.DispatchPointer({DxUi::PointerAction::Move, float(GET_X_LPARAM(lp)), float(GET_Y_LPARAM(lp)), UINT(wp)});
                    return 0;
                case WM_LBUTTONUP:
                    app->scene.view.DispatchPointer({DxUi::PointerAction::Up, float(GET_X_LPARAM(lp)), float(GET_Y_LPARAM(lp)), UINT(wp)});
                    if (GetCapture() == hwnd)
                        ReleaseCapture();
                    return 0;
                case WM_CAPTURECHANGED: app->scene.view.DispatchPointer({DxUi::PointerAction::Cancel}); return 0;
                case WM_KEYDOWN: app->scene.view.DispatchKey(UINT(wp), true, (GetKeyState(VK_SHIFT) & 0x8000) ? MK_SHIFT : 0); return 0;
                case WM_KEYUP: app->scene.view.DispatchKey(UINT(wp), false); return 0;
                case WM_CHAR: app->scene.view.DispatchCharacter(wchar_t(wp)); return 0;
                case WM_SHOWWINDOW: app->scene.view.SetVisible(wp != 0); break;
                case WM_SIZE:
                    if (wp == SIZE_MINIMIZED)
                        app->scene.view.SetVisible(false);
                    else
                    {
                        app->scene.view.SetVisible(true);
                        app->Request();
                    }
                    return 0;
                case WM_CLOSE: app->window.reset(); return 0;
                case WM_DESTROY: PostQuitMessage(0); return 0;
                case WM_NCDESTROY: SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0); break;
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
        WNDCLASSW wc{};
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpfnWndProc   = Proc;
        wc.lpszClassName = L"DxUi.EmbeddedControls";
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        if (! RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return HRESULT_FROM_WIN32(GetLastError());
        constexpr DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT bounds{0, 0, 480, 240};
        AdjustWindowRectEx(&bounds, style, FALSE, 0);
        window.reset(CreateWindowExW(0,
                                     wc.lpszClassName,
                                     L"DxUI - supplied Direct3D device",
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
        desc.BufferDesc.Width  = 480;
        desc.BufferDesc.Height = 240;
        desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count  = 1;
        desc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount       = 2;
        desc.OutputWindow      = window.get();
        desc.Windowed          = TRUE;
        desc.SwapEffect        = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        RETURN_IF_FAILED(factory->CreateSwapChain(gpu.device.get(), &desc, swapChain.put()));
        factory->MakeWindowAssociation(window.get(), DXGI_MWA_NO_ALT_ENTER);
        ShowWindow(window.get(), SW_SHOWNORMAL);
        Request();
        MSG message{};
        BOOL got = 0;
        while ((got = GetMessageW(&message, nullptr, 0, 0)) > 0)
        {
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
        App app;
        if (FAILED(app.gpu.Create()))
            return 1;
        if (FAILED(app.scene.Initialize(app.gpu.device.get(), {&app, [](void* context) noexcept { static_cast<App*>(context)->Request(); }})))
            return 1;
        if (argc == 3 && std::wstring_view(argv[1]) == L"--output")
        {
            if (FAILED(app.Render()) || FAILED(app.gpu.Save(argv[2])))
                return 1;
            std::wcout << L"Rendered public Toggle and Slider to " << argv[2] << L'\n';
            return 0;
        }
        if (argc != 1)
        {
            std::wcerr << L"Usage: DxUi.EmbeddedControls [--output image.png]\n";
            return 2;
        }
        return FAILED(app.Run()) ? 1 : 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
