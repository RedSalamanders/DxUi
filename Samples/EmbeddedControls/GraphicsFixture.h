#pragma once
#include <DxUi/Embedded.h>
#include <filesystem>
#include <vector>
#include <wil/result.h>
#include <wincodec.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowscodecs.lib")
struct GraphicsFixture
{
    wil::com_ptr_nothrow<ID3D11Device> device;
    wil::com_ptr_nothrow<ID3D11DeviceContext> context;
    wil::com_ptr_nothrow<ID3D11Texture2D> target;
    wil::com_ptr_nothrow<ID3D11RenderTargetView> rtv;
    UINT width = 480, height = 240;
    HRESULT Create(bool warp = true) noexcept
    {
        RETURN_IF_FAILED(D3D11CreateDevice(nullptr,
                                           warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE,
                                           nullptr,
                                           D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                           nullptr,
                                           0,
                                           D3D11_SDK_VERSION,
                                           device.put(),
                                           nullptr,
                                           context.put()));
        return Resize(width, height);
    }
    HRESULT Resize(UINT w, UINT h) noexcept
    {
        context->OMSetRenderTargets(0, nullptr, nullptr);
        rtv.reset();
        target.reset();
        width  = w;
        height = h;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width            = w;
        desc.Height           = h;
        desc.MipLevels        = 1;
        desc.ArraySize        = 1;
        desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BindFlags        = D3D11_BIND_RENDER_TARGET;
        RETURN_IF_FAILED(device->CreateTexture2D(&desc, nullptr, target.put()));
        return device->CreateRenderTargetView(target.get(), nullptr, rtv.put());
    }
    void Bind() noexcept
    {
        auto* targetView = rtv.get();
        context->OMSetRenderTargets(1, &targetView, nullptr);
        constexpr float background[] = {0.055f, 0.065f, 0.085f, 1};
        context->ClearRenderTargetView(targetView, background);
    }
    D3D11_VIEWPORT Viewport() const noexcept
    {
        return {0, 0, float(width), float(height), 0, 1};
    }
    HRESULT Read(std::vector<uint8_t>& pixels)
    {
        D3D11_TEXTURE2D_DESC desc{};
        target->GetDesc(&desc);
        desc.Usage          = D3D11_USAGE_STAGING;
        desc.BindFlags      = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        wil::com_ptr_nothrow<ID3D11Texture2D> staging;
        RETURN_IF_FAILED(device->CreateTexture2D(&desc, nullptr, staging.put()));
        context->CopyResource(staging.get(), target.get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        RETURN_IF_FAILED(context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped));
        const auto unmap = wil::scope_exit([&] { context->Unmap(staging.get(), 0); });
        pixels.resize(size_t(width) * height * 4);
        for (UINT y = 0; y < height; ++y)
            memcpy(pixels.data() + size_t(y) * width * 4, static_cast<uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch, size_t(width) * 4);
        return S_OK;
    }
    HRESULT Save(const std::filesystem::path& path)
    {
        std::vector<uint8_t> pixels;
        RETURN_IF_FAILED(Read(pixels));
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());
        wil::com_ptr_nothrow<IWICImagingFactory> factory;
        RETURN_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put())));
        wil::com_ptr_nothrow<IWICStream> stream;
        RETURN_IF_FAILED(factory->CreateStream(stream.put()));
        RETURN_IF_FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE));
        wil::com_ptr_nothrow<IWICBitmapEncoder> encoder;
        RETURN_IF_FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put()));
        RETURN_IF_FAILED(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache));
        wil::com_ptr_nothrow<IWICBitmapFrameEncode> frame;
        RETURN_IF_FAILED(encoder->CreateNewFrame(frame.put(), nullptr));
        RETURN_IF_FAILED(frame->Initialize(nullptr));
        RETURN_IF_FAILED(frame->SetSize(width, height));
        auto format = GUID_WICPixelFormat32bppBGRA;
        RETURN_IF_FAILED(frame->SetPixelFormat(&format));
        if (format != GUID_WICPixelFormat32bppBGRA)
            return E_NOTIMPL;
        RETURN_IF_FAILED(frame->WritePixels(height, width * 4, static_cast<UINT>(pixels.size()), pixels.data()));
        RETURN_IF_FAILED(frame->Commit());
        return encoder->Commit();
    }
};
