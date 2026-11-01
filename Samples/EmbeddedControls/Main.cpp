#include "ComplexUiScene.h"
#include "EmbeddedScene.h"
#include "EmbeddedTextClient.h"
#include "GraphicsFixture.h"
#include "RichImageLoader.h"
#include <DxUi/FileDrop.h>
#include <iostream>
#include <new>
#include <ole2.h>
#include <windowsx.h>
#pragma comment(lib, "user32.lib")
namespace
{
constexpr UINT RenderMessage     = WM_APP + 1;
constexpr UINT ImageReadyMessage = WM_APP + 2;
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626)
struct App
{
    static constexpr DWORD WindowStyle = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    App() : pendingImages(0u)
    {
    }
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
    DxUi::RichFileDropAdapter dropAdapter;
    std::shared_ptr<DxUi::RichFileDropReceiver> dropReceiver;
    wil::com_ptr_nothrow<IDataObject> textDragData;
    std::optional<DxUi::RichTextSourceMove> textDragMove;
    bool textDragMoved     = false;
    using DropRegistration = wil::unique_any<HWND, decltype(&RevokeDragDrop), RevokeDragDrop>;
    DropRegistration dropRegistration;
    std::shared_ptr<EmbeddedTextClient> textClient;
    RichImageLoader imageLoader;
    std::vector<DxUi::RichText::RichImageRequest> pendingImages;
    std::array<RichImageLoader::Result, RichImageLoader::MaximumResults> imageResults{};
    bool imageLoading       = false;
    bool textInput          = false;
    bool richText           = false;
    bool trackingMouseLeave = false;
    bool quitOnDestroy      = true;
    // The embedded library owns no HWND; this sample owns its leave registration.
    void StopMouseTracking(HWND hwnd) noexcept
    {
        if (trackingMouseLeave)
        {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_CANCEL | TME_LEAVE, hwnd, 0u};
            static_cast<void>(TrackMouseEvent(&tracking));
            trackingMouseLeave = false;
        }
    }
    uint64_t textRefreshCalls        = 0u;
    uint64_t textLayoutNotifications = 0u;
    ~App()
    {
        dropAdapter.Detach();
        dropRegistration.reset();
        dropReceiver.reset();
        imageLoader.Shutdown();
        ClearText();
        textServices.Detach();
        // Window teardown can reenter Proc. Keep every referenced application
        // service alive until WM_NCDESTROY has completed.
        window.reset();
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
        ++textRefreshCalls;
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
        return DxUi::ComposeModifierMask((GetKeyState(VK_SHIFT) & 0x8000) != 0, (GetKeyState(VK_CONTROL) & 0x8000) != 0, (GetKeyState(VK_MENU) & 0x8000) != 0);
    }
    void NotifyTextLayout() noexcept
    {
        ++textLayoutNotifications;
        textServices.NotifyLayoutChanged();
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
        if (imageLoading && scene.rich && imageLoader.HasResults())
        {
            const auto count = imageLoader.Drain(std::span(imageResults));
            for (std::size_t index = 0u; index < count; ++index)
            {
                auto& result = imageResults[index];
                try
                {
                    static_cast<void>(scene.rich->CompleteImageRequest(result.request, std::move(result.image)));
                }
                catch (const std::bad_alloc&)
                {
                    static_cast<void>(scene.rich->CompleteImageRequest(result.request, {}));
                }
                result = {};
            }
        }
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
        // A superseded frame is not an error: the view already asked for the preparation that replaces it.
        if (prepared == E_ABORT)
            return S_FALSE;
        RETURN_IF_FAILED(prepared);
        if (imageLoading && scene.rich)
        {
            try
            {
                scene.rich->TakePendingImageRequests(pendingImages);
            }
            catch (const std::bad_alloc&)
            {
                pendingImages.clear();
            } // Preserve the alternative; retry on a later requested frame.
            if (! pendingImages.empty() && FAILED(imageLoader.Submit(pendingImages)))
                for (const auto& request : pendingImages)
                    static_cast<void>(scene.rich->CompleteImageRequest(request, {}));
            pendingImages.clear();
        }
        if (prepared == S_OK)
        {
            RefreshText();
            NotifyTextLayout();
        }
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
            switch (msg)
            {
                case WM_PAINT:
                {
                    [[maybe_unused]] wil::unique_hdc_paint paint = wil::BeginPaint(hwnd);
                    app->Request();
                    return 0;
                }
                case RenderMessage:
                    if (FAILED(app->Render()))
                        PostQuitMessage(1);
                    return 0;
                case ImageReadyMessage:
                    if (wp == reinterpret_cast<WPARAM>(app))
                        app->Request();
                    return 0;
                case WM_LBUTTONDOWN:
                    SetFocus(hwnd);
                    if (app->View().DispatchPointer({DxUi::PointerAction::Down,
                                                     float(GET_X_LPARAM(lp)),
                                                     float(GET_Y_LPARAM(lp)),
                                                     UINT(wp),
                                                     0.0f,
                                                     DxUi::PointerDeviceFromMessageExtraInfo(GetMessageExtraInfo())}))
                        app->RefreshText();
                    if (app->View().Controls().GetCapturedControl())
                        SetCapture(hwnd);
                    else if (GetCapture() == hwnd)
                        ReleaseCapture();
                    return 0;
                case WM_MBUTTONDOWN:
                    SetFocus(hwnd);
                    if (app->View().DispatchPointer({DxUi::PointerAction::MiddleDown, float(GET_X_LPARAM(lp)), float(GET_Y_LPARAM(lp)), UINT(wp)}))
                    {
                        if (app->View().Controls().GetCapturedControl())
                            SetCapture(hwnd);
                        else if (GetCapture() == hwnd)
                            ReleaseCapture();
                        app->RefreshText();
                    }
                    return 0;
                case WM_MBUTTONUP:
                    if (app->View().DispatchPointer({DxUi::PointerAction::MiddleUp, float(GET_X_LPARAM(lp)), float(GET_Y_LPARAM(lp)), UINT(wp)}))
                        app->RefreshText();
                    if (! app->View().Controls().GetCapturedControl() && GetCapture() == hwnd)
                        ReleaseCapture();
                    return 0;
                case WM_MOUSEMOVE:
                    if (! app->trackingMouseLeave)
                    {
                        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, hwnd, 0u};
                        app->trackingMouseLeave = TrackMouseEvent(&tracking) != FALSE;
                    }
                    app->View().DispatchPointer({DxUi::PointerAction::Move,
                                                 float(GET_X_LPARAM(lp)),
                                                 float(GET_Y_LPARAM(lp)),
                                                 UINT(wp),
                                                 0.0f,
                                                 DxUi::PointerDeviceFromMessageExtraInfo(GetMessageExtraInfo())});
                    if (app->View().Controls().GetCapturedControl())
                        app->RefreshText();
                    if (app->View().NeedsAnimation())
                        SetTimer(hwnd, 1u, 16u, nullptr);
                    return 0;
                case WM_MOUSELEAVE:
                    app->trackingMouseLeave = false;
                    app->View().DispatchPointer({DxUi::PointerAction::Leave});
                    return 0;
                case WM_TIMER:
                    if (wp == 1u)
                    {
                        if (app->View().AdvanceAnimation(GetTickCount64()) && app->View().Controls().GetCapturedControl())
                            app->RefreshText();
                        if (! app->View().NeedsAnimation())
                            KillTimer(hwnd, 1u);
                        return 0;
                    }
                    break;
                case WM_MOUSEWHEEL:
                {
                    POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                    if (ScreenToClient(hwnd, &point))
                        app->View().DispatchPointer({DxUi::PointerAction::Wheel,
                                                     float(point.x),
                                                     float(point.y),
                                                     GET_KEYSTATE_WPARAM(wp),
                                                     float(GET_WHEEL_DELTA_WPARAM(wp)),
                                                     DxUi::PointerDeviceFromMessageExtraInfo(GetMessageExtraInfo())});
                    return 0;
                }
                case WM_LBUTTONUP:
                    if (app->View().DispatchPointer({DxUi::PointerAction::Up,
                                                     float(GET_X_LPARAM(lp)),
                                                     float(GET_Y_LPARAM(lp)),
                                                     UINT(wp),
                                                     0.0f,
                                                     DxUi::PointerDeviceFromMessageExtraInfo(GetMessageExtraInfo())}))
                        app->RefreshText();
                    if (! app->View().Controls().GetCapturedControl() && GetCapture() == hwnd)
                        ReleaseCapture();
                    return 0;
                case WM_CAPTURECHANGED:
                    if (app->View().DispatchPointer({DxUi::PointerAction::Cancel}))
                        app->RefreshText();
                    return 0;
                case WM_KEYDOWN:
                    if (app->View().DispatchKey(UINT(wp), true, Modifiers()))
                        app->RefreshText();
                    if (! app->View().Controls().GetCapturedControl() && GetCapture() == hwnd)
                        ReleaseCapture();
                    return 0;
                case WM_KEYUP:
                    if (app->View().DispatchKey(UINT(wp), false, Modifiers()))
                        app->RefreshText();
                    return 0;
                case WM_CHAR:
                    if (app->View().DispatchCharacter(wchar_t(wp), Modifiers()))
                        app->RefreshText();
                    return 0;
                case WM_SETFOCUS: app->RefreshText(); return 0;
                case WM_WINDOWPOSCHANGED:
                {
                    const auto* position = reinterpret_cast<const WINDOWPOS*>(lp);
                    if (position && (position->flags & SWP_NOMOVE) == 0u)
                        app->NotifyTextLayout();
                    break; // DefWindowProc still sends WM_SIZE/WM_MOVE when appropriate.
                }
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
                case WM_SHOWWINDOW:
                    if (! wp)
                    {
                        app->StopMouseTracking(hwnd);
                        app->ClearText();
                    }
                    app->View().SetVisible(wp != 0);
                    app->imageLoader.DiscardCancelled();
                    break;
                case WM_SIZE:
                    if (wp == SIZE_MINIMIZED)
                    {
                        app->StopMouseTracking(hwnd);
                        app->ClearText();
                        app->View().SetVisible(false);
                        app->imageLoader.DiscardCancelled();
                    }
                    else
                    {
                        app->View().SetVisible(true);
                        app->Request();
                    }
                    return 0;
                case WM_CLOSE:
                    app->dropAdapter.Detach();
                    app->dropRegistration.reset();
                    app->dropReceiver.reset();
                    app->window.reset();
                    return 0;
                case WM_DESTROY:
                    if (app->quitOnDestroy)
                        PostQuitMessage(0);
                    return 0;
                case WM_NCDESTROY:
                    app->StopMouseTracking(hwnd);
                    app->ClearText();
                    app->textServices.Detach();
                    app->View().SetVisible(false);
                    app->imageLoader.Shutdown();
                    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                    break;
                default: break;
            }
        }
        catch (const std::exception&)
        {
            // Win32 callback boundary: stop the owned sample on a standard failure.
            PostQuitMessage(1);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    static HRESULT VerifyMessageRouting(ID3D11Device* device)
    {
        const HWND foreground = GetForegroundWindow();
        const HWND focus      = GetFocus();
        struct PointerState
        {
            unsigned moves{}, leaves{};
        } state;
        class PointerProbe final : public DxUi::Control
        {
        public:
            explicit PointerProbe(PointerState& observed) noexcept : _observed(&observed)
            {
            }
            PointerProbe(const PointerProbe&)            = delete;
            PointerProbe& operator=(const PointerProbe&) = delete;
            PointerProbe(PointerProbe&&)                 = delete;
            PointerProbe& operator=(PointerProbe&&)      = delete;
            bool OnMouseMove(DxUi::ControlHost&, D2D1_POINT_2F, UINT) override
            {
                ++_observed->moves;
                return true;
            }
            bool OnMouseLeave(DxUi::ControlHost&) override
            {
                ++_observed->leaves;
                return true;
            }

        private:
            PointerState* _observed;
        };
        App sample;
        sample.quitOnDestroy = false; // This hidden proof owns no GetMessage loop or WM_QUIT.
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        RETURN_IF_FAILED(DxUi::GraphicsDevice::Create(device, graphics));
        RETURN_IF_FAILED(sample.View().Attach(std::move(graphics)));
        auto root     = std::make_unique<DxUi::Panel>();
        auto* pointer = root->AddChild<PointerProbe>(state);
        pointer->SetBounds(D2D1::RectF(0.f, 0.f, 240.f, 180.f));
        sample.View().Controls().SetRoot(std::move(root));
        RETURN_IF_FAILED(sample.View().Prepare(240u, 180u));
        RETURN_IF_FAILED(sample.CreateHostWindow()); // Hidden; never activates or acquires capture.
        const auto refreshBefore = sample.textRefreshCalls;
        static_cast<void>(SendMessageW(sample.window.get(), WM_APP + 77u, 0u, 0));
        static_cast<void>(SendMessageW(sample.window.get(), WM_MOUSEMOVE, 0u, MAKELPARAM(24, 24)));
        if (! state.moves || sample.textRefreshCalls != refreshBefore)
            return E_FAIL;
        static_cast<void>(SendMessageW(sample.window.get(), WM_MOUSELEAVE, 0u, 0));
        if (state.leaves != 1u || sample.textRefreshCalls != refreshBefore)
            return E_FAIL;
        POINT before{}, after{};
        if (! ClientToScreen(sample.window.get(), &before))
            return HRESULT_FROM_WIN32(GetLastError());
        const auto notificationsBefore = sample.textLayoutNotifications;
        if (! SetWindowPos(sample.window.get(), nullptr, before.x + 37, before.y + 29, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) ||
            ! ClientToScreen(sample.window.get(), &after))
            return HRESULT_FROM_WIN32(GetLastError());
        if ((before.x == after.x && before.y == after.y) || sample.textLayoutNotifications <= notificationsBefore || sample.textRefreshCalls != refreshBefore ||
            GetForegroundWindow() != foreground || GetFocus() != focus)
            return E_FAIL;
        return S_OK;
    }
    HRESULT CreateHostWindow()
    {
        WNDCLASSW wc{};
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpfnWndProc   = Proc;
        wc.lpszClassName = L"DxUi.EmbeddedControls";
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        if (! RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return HRESULT_FROM_WIN32(GetLastError());
        RECT bounds{0, 0, static_cast<LONG>(gpu.width), static_cast<LONG>(gpu.height)};
        AdjustWindowRectEx(&bounds, WindowStyle, FALSE, 0);
        window.reset(CreateWindowExW(0,
                                     wc.lpszClassName,
                                     richText ? L"DxUi - Unicode RichTextEditor semantic preview" : L"DxUi - supplied Direct3D device",
                                     WindowStyle,
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
        return S_OK;
    }
    HRESULT Run()
    {
        const auto previousDpi = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        const auto restoreDpi  = wil::scope_exit([&]
        {
            if (previousDpi)
                SetThreadDpiAwarenessContext(previousDpi);
        });
        RETURN_IF_FAILED(CreateHostWindow());
        if (richText)
        {
            RETURN_IF_FAILED(imageLoader.Initialize(window.get(), ImageReadyMessage, reinterpret_cast<WPARAM>(this)));
            imageLoading = true;
        }
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
        if (richText)
        {
            class Receiver final : public DxUi::RichFileDropReceiver
            {
            public:
                explicit Receiver(App& owner) noexcept : app(&owner)
                {
                }
                bool Preview(POINTL screen) noexcept override
                {
                    POINT client{screen.x, screen.y};
                    if (! app->window || ! ScreenToClient(app->window.get(), &client) || app->dpi <= 0.f)
                        return false;
                    return app->View().PreviewFileDrop(D2D1::Point2F(float(client.x) * 96.f / app->dpi, float(client.y) * 96.f / app->dpi));
                }
                bool Drop(POINTL screen, IDataObject& data, DxUi::RichFileDropGuard guard) noexcept override
                {
                    POINT client{screen.x, screen.y};
                    if (! app->window || ! ScreenToClient(app->window.get(), &client) || app->dpi <= 0.f)
                        return false;
                    const bool committed = app->View().CommitFileDrop(
                        D2D1::Point2F(float(client.x) * 96.f / app->dpi, float(client.y) * 96.f / app->dpi), data, guard.current, guard.context);
                    if (committed)
                        app->RefreshText();
                    return committed;
                }
                bool PreviewText(POINTL screen) noexcept override
                {
                    if (! app->textDragMove || ! app->textDragData || ! app->textDragMove->IsOwnerAlive() || ! app->scene.rich || ! app->window ||
                        app->dpi <= 0.f)
                        return false;
                    POINT client{screen.x, screen.y};
                    if (! ScreenToClient(app->window.get(), &client))
                        return false;
                    const auto point = D2D1::Point2F(float(client.x) * 96.f / app->dpi, float(client.y) * 96.f / app->dpi);
                    if (! app->View().PreviewFileDrop(point) || ! app->textDragMove || ! app->textDragMove->IsOwnerAlive())
                        return false;
                    const auto position = app->scene.rich->HitTestSourceDropPoint(app->View().Controls(), point);
                    if (! position)
                        return false;
                    const auto selected = app->scene.rich->GetSelection();
                    return *position < selected.Start() || *position > selected.End();
                }
                bool DropText(POINTL screen, IDataObject& data, DxUi::RichFileDropGuard guard) noexcept override
                {
                    if (&data != app->textDragData.get() || ! PreviewText(screen) || ! app->textDragMove || ! app->textDragMove->IsOwnerAlive() ||
                        (guard.current && ! guard.current(guard.context)))
                        return false;
                    POINT client{screen.x, screen.y};
                    if (! ScreenToClient(app->window.get(), &client))
                        return false;
                    const auto position = app->scene.rich->HitTestSourceDropPoint(
                        app->View().Controls(), D2D1::Point2F(float(client.x) * 96.f / app->dpi, float(client.y) * 96.f / app->dpi));
                    if (! position || (guard.current && ! guard.current(guard.context)) || ! app->textDragMove || ! app->textDragMove->IsOwnerAlive())
                        return false;
                    try
                    {
                        app->textDragMoved = app->scene.rich->MoveSelectedSource(*app->textDragMove, *position);
                        return app->textDragMoved;
                    }
                    catch (const std::bad_alloc&)
                    {
                        return false; // Source staging did not commit.
                    }
                    catch (const std::exception&)
                    {
                        return false; // The source transaction is atomic on failure.
                    }
                }

            private:
                App* app; // Borrowed; the application retires the adapter before destroying its window or view.
            };
            dropReceiver = std::make_shared<Receiver>(*this);
            RETURN_IF_FAILED(dropAdapter.Attach(dropReceiver));
            RETURN_IF_FAILED(RegisterDragDrop(window.get(), dropAdapter.Target()));
            dropRegistration.reset(window.get());
            scene.rich->SetOnSourceDragRequested([this](const DxUi::RichTextSourceMove& move, std::wstring_view text)
            {
                // The embedded host has released its logical capture before this
                // callback; the application owns and must release HWND capture too.
                if (window && GetCapture() == window.get())
                    ReleaseCapture();
                if (! move.IsOwnerAlive())
                    return;
                wil::com_ptr_nothrow<IDataObject> data;
                wil::com_ptr_nothrow<IDropSource> source;
                if (FAILED(DxUi::CreateRichTextDragData(text, data.put())) || FAILED(DxUi::CreateRichTextDragSource(source.put())))
                    return;
                auto* editor  = scene.rich;
                textDragMove  = move;
                textDragData  = data;
                textDragMoved = false;
                DWORD effect{};
                const HRESULT result = DoDragDrop(data.get(), source.get(), DROPEFFECT_MOVE, &effect);
                const bool movedHere = textDragMoved;
                textDragData.reset();
                textDragMove.reset();
                if (move.IsOwnerAlive())
                {
                    if (! movedHere && result == DRAGDROP_S_DROP && effect == DROPEFFECT_MOVE)
                    {
                        try
                        {
                            static_cast<void>(editor->CompleteExternalSourceMove(move));
                        }
                        catch (const std::bad_alloc&)
                        {
                            // Leave the source intact if external-move completion cannot stage deletion.
                        }
                    }
                    else if (! movedHere)
                        editor->CancelSourceMove();
                }
            });
        }
        RECT scaled{0, 0, LONG(float(gpu.width) * dpi / 96), LONG(float(gpu.height) * dpi / 96)};
        if (! AdjustWindowRectExForDpi(&scaled, WindowStyle, FALSE, 0, UINT(dpi)))
            return HRESULT_FROM_WIN32(GetLastError());
        SetWindowPos(window.get(), nullptr, 0, 0, scaled.right - scaled.left, scaled.bottom - scaled.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(window.get(), SW_SHOWNORMAL);
        Request();
        MSG message{};
        BOOL got = 0;
        while ((got = GetMessageW(&message, nullptr, 0, 0)) > 0)
        {
            bool handled = false;
            static_cast<void>(textServices.PreTranslate(message, handled));
            if (handled)
            {
                RefreshText();
                continue;
            }
            if (message.hwnd == window.get() && message.message == WM_KEYDOWN && (message.wParam == VK_RETURN || message.wParam == VK_SPACE))
            {
                DxUi::EmbeddedKeyDispatchReceipt receipt;
                static_cast<void>(View().DispatchKey(UINT(message.wParam), true, Modifiers(), receipt));
                // Dispatch before OS translation so the focused command can suppress its activation character while
                // same-owner text input keeps the active keyboard layout, including AltGr handling.
                if (View().ShouldTranslateCharacter(receipt))
                    TranslateMessage(&message);
                RefreshText();
                Request();
                continue;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return got < 0 ? HRESULT_FROM_WIN32(GetLastError()) : (message.wParam ? E_FAIL : S_OK);
    }
};
#pragma warning(pop)
} // namespace
int wmain(int argc, wchar_t** argv)
{
    const auto hr = OleInitialize(nullptr);
    if (FAILED(hr))
        return 1;
    const auto com = wil::scope_exit([] { OleUninitialize(); });
    try
    {
        bool complex                = false;
        bool textInput              = false;
        bool richText               = false;
        const wchar_t* documentPath = nullptr;
        const wchar_t* output       = nullptr;
        for (int i = 1; i < argc; ++i)
        {
            if (std::wstring_view(argv[i]) == L"--complex-ui" && ! complex)
                complex = true;
            else if (std::wstring_view(argv[i]) == L"--text-input" && ! textInput)
                textInput = true;
            else if (std::wstring_view(argv[i]) == L"--rich-text" && ! richText)
                richText = true;
            else if (std::wstring_view(argv[i]) == L"--output" && ! output && i + 1 < argc)
                output = argv[++i];
            else if (std::wstring_view(argv[i]) == L"--document-path" && ! documentPath && i + 1 < argc)
                documentPath = argv[++i];
            else
            {
                std::wcerr << L"Usage: DxUi.EmbeddedControls [--complex-ui | --text-input | --rich-text [--document-path absolute.md]] [--output image.png]\n";
                return 2;
            }
        }
        if ((complex && (textInput || richText)) || (textInput && richText))
            return 2;
        if (documentPath && ! richText)
            return 2;
        App app;
        app.textInput = textInput || richText;
        app.richText  = richText;
        if (richText)
        {
            app.gpu.width  = 980;
            app.gpu.height = 720;
        }
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
        if (FAILED(complex ? app.complex->Initialize(app.gpu.device.get(), callbacks)
                           : app.scene.Initialize(app.gpu.device.get(), callbacks, textInput, richText)))
            return 1;
        // Location metadata only: the sample never opens/saves the Markdown
        // document or derives a base from the process working directory.
        if (documentPath)
            app.scene.rich->SetDocumentFilePath(documentPath);
        if (output)
        {
            // Headless capture has no message pump to drain cooperative preparation.
            if (richText)
            {
                for (unsigned turn = 0; turn < 1024u && app.scene.rich->GetMarkdownRenderingStatus() != DxUi::MarkdownRenderingStatus::Semantic; ++turn)
                    if (FAILED(app.Render()))
                        return 1;
                if (app.scene.rich->GetMarkdownRenderingStatus() != DxUi::MarkdownRenderingStatus::Semantic)
                    return 1;
            }
            if (FAILED(app.Render()))
                return 1;
            if (textInput)
            {
                if (FAILED(App::VerifyMessageRouting(app.gpu.device.get())))
                    return 1;
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
                        catch (const std::bad_alloc&)
                        {
                            // COM-style clipboard boundary: leave the caller's edit uncommitted.
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
