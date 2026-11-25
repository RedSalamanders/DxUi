// Rough measurement for W2.4 (developer decision on G10): what a thread's native popup device stack holds idle and what
// creating it costs per menu session. Mirrors EnsureSharedWindowHostGraphicsResources + EnsureSharedWindowHostCompositionDevice
// and a CompositionSwapChain popup attach + first present. Hidden window, never shown or activated.
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include <d2d1_3.h>
#include <dwrite.h>
#include <dcomp.h>
#include <psapi.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "psapi.lib")
using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;

struct Stack
{
    ComPtr<ID3D11Device> d3d;
    ComPtr<ID3D11DeviceContext> ctx;
    ComPtr<IDXGIFactory2> dxgi;
    ComPtr<ID2D1Factory1> d2dFactory;
    ComPtr<ID2D1Device> d2d;
    ComPtr<IDWriteFactory> write;
    ComPtr<IDCompositionDesktopDevice> dcomp;
};

bool CreateStack(Stack& s, bool warp)
{
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1};
    if (FAILED(D3D11CreateDevice(nullptr, warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3,
                                 D3D11_SDK_VERSION, &s.d3d, nullptr, &s.ctx)))
        return false;
    ComPtr<IDXGIDevice> dx;
    s.d3d.As(&dx);
    ComPtr<IDXGIAdapter> adapter;
    dx->GetAdapter(&adapter);
    adapter->GetParent(IID_PPV_ARGS(&s.dxgi));
    D2D1_FACTORY_OPTIONS o{};
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &o, &s.d2dFactory);
    s.d2dFactory->CreateDevice(dx.Get(), &s.d2d);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &s.write);
    return SUCCEEDED(DCompositionCreateDevice2(dx.Get(), __uuidof(IDCompositionDesktopDevice), &s.dcomp));
}

// One popup: hidden window, composition swap chain, target, D2D context, first frame presented and committed.
bool Session(Stack& s, HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC1 d{};
    d.Width = 420; d.Height = 560; d.Format = DXGI_FORMAT_B8G8R8A8_UNORM; d.SampleDesc.Count = 1;
    d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; d.BufferCount = 2; d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    d.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED; d.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    ComPtr<IDXGISwapChain1> chain;
    if (FAILED(s.dxgi->CreateSwapChainForComposition(s.d3d.Get(), &d, nullptr, &chain))) return false;
    ComPtr<IDXGISwapChain2> chain2; chain.As(&chain2); chain2->SetMaximumFrameLatency(1);
    HANDLE waitable = chain2->GetFrameLatencyWaitableObject();
    ComPtr<IDCompositionTarget> target; ComPtr<IDCompositionVisual2> visual;
    s.dcomp->CreateTargetForHwnd(hwnd, TRUE, &target);
    s.dcomp->CreateVisual(&visual);
    visual->SetContent(chain.Get()); target->SetRoot(visual.Get());
    ComPtr<ID2D1DeviceContext> dc; s.d2d->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc);
    ComPtr<IDXGISurface> surface; chain->GetBuffer(0, IID_PPV_ARGS(&surface));
    auto p = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    ComPtr<ID2D1Bitmap1> bmp; dc->CreateBitmapFromDxgiSurface(surface.Get(), &p, &bmp);
    WaitForSingleObjectEx(waitable, 1000, TRUE);
    dc->SetTarget(bmp.Get()); dc->BeginDraw(); dc->Clear(D2D1::ColorF(0, 0, 0, 0));
    ComPtr<ID2D1SolidColorBrush> b; dc->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.3f, 0.4f, 0.96f), &b);
    dc->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(10, 10, 410, 550), 8, 8), b.Get());
    dc->EndDraw();
    const HRESULT hr = chain->Present(0, 0);
    s.dcomp->Commit();
    CloseHandle(waitable);
    dc->SetTarget(nullptr);
    return SUCCEEDED(hr);
}

double Median(std::vector<double> v) { std::sort(v.begin(), v.end()); return v[v.size() / 2]; }
SIZE_T PrivateBytes() { PROCESS_MEMORY_COUNTERS_EX m{sizeof(m)}; GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&m, sizeof(m)); return m.PrivateUsage; }
UINT64 Video(bool warp, DXGI_MEMORY_SEGMENT_GROUP g)
{
    ComPtr<IDXGIFactory4> f; CreateDXGIFactory1(IID_PPV_ARGS(&f));
    ComPtr<IDXGIAdapter> a;
    if (warp) f->EnumWarpAdapter(IID_PPV_ARGS(&a)); else f->EnumAdapters(0, &a);
    ComPtr<IDXGIAdapter3> a3; a.As(&a3);
    DXGI_QUERY_VIDEO_MEMORY_INFO info{}; a3->QueryVideoMemoryInfo(0, g, &info);
    return info.CurrentUsage;
}

int main()
{
    WNDCLASSW wc{}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"W24StackProbe"; RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP | WS_EX_NOACTIVATE, wc.lpszClassName, L"", WS_POPUP, 0, 0, 420, 560, nullptr, nullptr, wc.hInstance, nullptr);
    { ComPtr<IDXGIFactory1> f; CreateDXGIFactory1(IID_PPV_ARGS(&f)); ComPtr<IDXGIAdapter1> a; f->EnumAdapters1(0, &a); DXGI_ADAPTER_DESC1 desc{}; a->GetDesc1(&desc); wprintf(L"default adapter: %s (dedicated %llu MiB)\n", desc.Description, (unsigned long long)desc.DedicatedVideoMemory >> 20); }
    for (bool warp : {false, true})
    {
        // warm-up once so DLL loads do not count
        { Stack s; CreateStack(s, warp); Session(s, hwnd); }
        std::vector<double> cold, warm;
        for (int i = 0; i < 11; ++i)
        {
            auto t0 = Clock::now();
            Stack s; CreateStack(s, warp); Session(s, hwnd);
            cold.push_back(std::chrono::duration<double, std::milli>(Clock::now() - t0).count());
        }
        Stack keep; CreateStack(keep, warp); Session(keep, hwnd);
        for (int i = 0; i < 11; ++i)
        {
            auto t0 = Clock::now();
            Session(keep, hwnd);
            warm.push_back(std::chrono::duration<double, std::milli>(Clock::now() - t0).count());
        }
        // idle stack footprint: stack created, one session made and torn down
        const SIZE_T before = PrivateBytes();
        const UINT64 vBefore = Video(warp, DXGI_MEMORY_SEGMENT_GROUP_LOCAL), nvBefore = Video(warp, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL);
        SIZE_T after = 0; UINT64 vAfter = 0, nvAfter = 0;
        {
            Stack s; CreateStack(s, warp); Session(s, hwnd); s.ctx->Flush();
            after = PrivateBytes(); vAfter = Video(warp, DXGI_MEMORY_SEGMENT_GROUP_LOCAL); nvAfter = Video(warp, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL);
        }
        printf("%s: attach-to-first-present median cold %.2f ms (min %.2f max %.2f), warm %.2f ms (min %.2f max %.2f); idle stack +%lld KiB private, +%lld KiB local video, +%lld KiB non-local video\n",
               warp ? "WARP" : "hardware", Median(cold), *std::min_element(cold.begin(), cold.end()), *std::max_element(cold.begin(), cold.end()),
               Median(warm), *std::min_element(warm.begin(), warm.end()), *std::max_element(warm.begin(), warm.end()),
               (long long)(after - before) / 1024, (long long)(vAfter - vBefore) / 1024, (long long)(nvAfter - nvBefore) / 1024);
    }
    DestroyWindow(hwnd);
    return 0;
}

