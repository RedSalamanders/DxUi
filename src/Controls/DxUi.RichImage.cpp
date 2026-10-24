#include "DxUi.RichImage.h"
#include "DxUi.RichImagePaths.h"
#include <DxUi/RichImage.h>
#include <array>
#include <cstring>
#include <wil/com.h>
#include <wil/resource.h>
#include <wincodec.h>

namespace DxUi::RichText
{
std::uint64_t RichImageRequest::Id() const noexcept
{
    return state_ ? state_->id : 0u;
}
std::wstring_view RichImageRequest::FilePath() const noexcept
{
    return state_ && state_->sourceKind == RichImageSourceKind::LocalFile ? std::wstring_view(state_->filePath) : std::wstring_view{};
}
RichImageSourceKind RichImageRequest::SourceKind() const noexcept
{
    return state_ ? state_->sourceKind : RichImageSourceKind::LocalFile;
}
std::wstring_view RichImageRequest::Source() const noexcept
{
    return state_ ? std::wstring_view(state_->filePath) : std::wstring_view{};
}
bool RichImageRequest::Cancelled() const noexcept
{
    return ! state_ || state_->cancelled.load(std::memory_order_relaxed);
}
const std::atomic_bool* RichImageRequest::Cancellation() const noexcept
{
    static const std::atomic_bool empty{true};
    return state_ ? &state_->cancelled : &empty;
}
RichImage::RichImage(unsigned width, unsigned height) : width_(width), height_(height), pixels_(static_cast<std::size_t>(width) * height * 4u)
{
}
namespace Detail
{
#if DXUI_ENABLE_DIAGNOSTICS
thread_local const RichImageFileCalls* testRichImageFileCalls = nullptr;
#endif
namespace
{
const RichImageFileCalls& FileCalls() noexcept
{
    static const RichImageFileCalls calls;
#if DXUI_ENABLE_DIAGNOSTICS
    if (testRichImageFileCalls)
        return *testRichImageFileCalls;
#endif
    return calls;
}
bool Cancelled(const std::atomic_bool* flag) noexcept
{
    return flag && flag->load(std::memory_order_relaxed);
}
bool DimensionsAllowed(unsigned width, unsigned height) noexcept
{
    return width && height && width <= RichImage::MaximumDimension && height <= RichImage::MaximumDimension && width <= RichImage::MaximumPixels / height;
}
const CLSID* BuiltinDecoder(std::span<const std::byte> bytes) noexcept
{
    const auto* data     = reinterpret_cast<const BYTE*>(bytes.data());
    constexpr BYTE png[] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (bytes.size() >= 8u && ! std::memcmp(data, png, 8u))
        return &CLSID_WICPngDecoder;
    if (bytes.size() >= 3u && data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff)
        return &CLSID_WICJpegDecoder;
    if (bytes.size() >= 6u && (! std::memcmp(data, "GIF87a", 6u) || ! std::memcmp(data, "GIF89a", 6u)))
        return &CLSID_WICGifDecoder;
    if (bytes.size() >= 2u && data[0] == 'B' && data[1] == 'M')
        return &CLSID_WICBmpDecoder;
    if (bytes.size() >= 4u && (! std::memcmp(data, "II\x2a\0", 4u) || ! std::memcmp(data, "MM\0\x2a", 4u)))
        return &CLSID_WICTiffDecoder;
    return nullptr;
}
HRESULT MetadataUnsigned(IWICMetadataQueryReader* reader, const wchar_t* path, unsigned& output, bool required = false) noexcept
{
    wil::unique_prop_variant value;
    const auto result = reader->GetMetadataByName(path, &value);
    if (result == WINCODEC_ERR_PROPERTYNOTFOUND)
        return required ? WINCODEC_ERR_BADMETADATAHEADER : S_OK;
    RETURN_IF_FAILED(result);
    if (value.vt == VT_UI1)
        output = value.bVal;
    else if (value.vt == VT_UI2)
        output = value.uiVal;
    else if (value.vt == VT_UI4)
        output = value.ulVal;
    else if (value.vt == VT_BOOL)
        output = value.boolVal != VARIANT_FALSE;
    else
        return WINCODEC_ERR_UNEXPECTEDMETADATATYPE;
    return S_OK;
}
HRESULT ComposeGifCanvas(IWICImagingFactory* factory,
                         IWICBitmapDecoder* decoder,
                         IWICBitmapFrameDecode* frame,
                         std::vector<std::byte>& pixels,
                         unsigned& width,
                         unsigned& height,
                         const std::atomic_bool* cancellation)
{
    wil::com_ptr_nothrow<IWICMetadataQueryReader> global, local;
    RETURN_IF_FAILED(decoder->GetMetadataQueryReader(global.put()));
    RETURN_IF_FAILED(frame->GetMetadataQueryReader(local.put()));
    unsigned canvasWidth{}, canvasHeight{}, left{}, top{}, transparent{}, globalTable{}, background{};
    RETURN_IF_FAILED(MetadataUnsigned(global.get(), L"/logscrdesc/Width", canvasWidth, true));
    RETURN_IF_FAILED(MetadataUnsigned(global.get(), L"/logscrdesc/Height", canvasHeight, true));
    RETURN_IF_FAILED(MetadataUnsigned(local.get(), L"/imgdesc/Left", left, true));
    RETURN_IF_FAILED(MetadataUnsigned(local.get(), L"/imgdesc/Top", top, true));
    RETURN_IF_FAILED(MetadataUnsigned(local.get(), L"/grctlext/TransparencyFlag", transparent));
    if (! DimensionsAllowed(canvasWidth, canvasHeight))
        return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
    if (left > canvasWidth || top > canvasHeight || width > canvasWidth - left || height > canvasHeight - top)
        return WINCODEC_ERR_BADIMAGE;
    if (left == 0u && top == 0u && width == canvasWidth && height == canvasHeight)
        return S_OK;
    std::array<std::byte, 4u> fill{};
    RETURN_IF_FAILED(MetadataUnsigned(global.get(), L"/logscrdesc/GlobalColorTableFlag", globalTable));
    RETURN_IF_FAILED(MetadataUnsigned(global.get(), L"/logscrdesc/BackgroundColorIndex", background));
    if (! transparent && globalTable)
    {
        wil::com_ptr_nothrow<IWICPalette> palette;
        RETURN_IF_FAILED(factory->CreatePalette(palette.put()));
        RETURN_IF_FAILED(decoder->CopyPalette(palette.get()));
        std::array<WICColor, 256u> colors{};
        UINT count{};
        RETURN_IF_FAILED(palette->GetColors(static_cast<UINT>(colors.size()), colors.data(), &count));
        if (background >= count)
            return WINCODEC_ERR_BADIMAGE;
        const auto color = colors[background];
        fill             = {std::byte(color & 255u), std::byte((color >> 8u) & 255u), std::byte((color >> 16u) & 255u), std::byte(255u)};
    }
    std::vector<std::byte> canvas(static_cast<std::size_t>(canvasWidth) * canvasHeight * 4u);
    for (std::size_t at = 0u; at < canvas.size(); at += 4u)
        std::memcpy(canvas.data() + at, fill.data(), 4u);
    for (unsigned y = 0u; y < height; ++y)
    {
        if (Cancelled(cancellation))
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        for (unsigned x = 0u; x < width; ++x)
        {
            const auto* source = pixels.data() + (static_cast<std::size_t>(y) * width + x) * 4u;
            auto* destination  = canvas.data() + (static_cast<std::size_t>(top + y) * canvasWidth + left + x) * 4u;
            const auto alpha   = std::to_integer<unsigned>(source[3]);
            for (unsigned channel = 0u; channel < 4u; ++channel)
                destination[channel] =
                    std::byte(std::to_integer<unsigned>(source[channel]) + (std::to_integer<unsigned>(destination[channel]) * (255u - alpha) + 127u) / 255u);
        }
    }
    pixels.swap(canvas);
    width  = canvasWidth;
    height = canvasHeight;
    return S_OK;
}
} // namespace
class RichImageDecoder
{
public:
    // EXIF transforms expressed as source-to-destination pixel coordinates,
    // independent of WIC's combined flip/rotation ordering. All eight cases have
    // a separate 2x3 oracle and real TIFF metadata fixtures.
    static HRESULT Orient(std::span<const std::byte> input,
                          unsigned width,
                          unsigned height,
                          unsigned orientation,
                          std::shared_ptr<const RichImage>& output,
                          const std::atomic_bool* cancellation)
    {
        if (! DimensionsAllowed(width, height) || input.size() != static_cast<std::size_t>(width) * height * 4u || orientation < 1u || orientation > 8u)
            return E_INVALIDARG;
        if (Cancelled(cancellation))
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        const bool transpose = orientation >= 5u;
        auto staged          = std::shared_ptr<RichImage>(new RichImage(transpose ? height : width, transpose ? width : height));
        for (unsigned y = 0u; y < height; ++y)
        {
            if (Cancelled(cancellation))
                return HRESULT_FROM_WIN32(ERROR_CANCELLED);
            for (unsigned x = 0u; x < width; ++x)
            {
                unsigned dx = x, dy = y;
                switch (orientation)
                {
                    case 2u: dx = width - 1u - x; break;
                    case 3u:
                        dx = width - 1u - x;
                        dy = height - 1u - y;
                        break;
                    case 4u: dy = height - 1u - y; break;
                    case 5u:
                        dx = y;
                        dy = x;
                        break;
                    case 6u:
                        dx = height - 1u - y;
                        dy = x;
                        break;
                    case 7u:
                        dx = height - 1u - y;
                        dy = width - 1u - x;
                        break;
                    case 8u:
                        dx = y;
                        dy = width - 1u - x;
                        break;
                    default: break;
                }
                std::memcpy(staged->pixels_.data() + (static_cast<std::size_t>(dy) * staged->width_ + dx) * 4u,
                            input.data() + (static_cast<std::size_t>(y) * width + x) * 4u,
                            4u);
            }
        }
        if (Cancelled(cancellation))
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        output = staged;
        return S_OK;
    }
    static HRESULT Decode(IWICImagingFactory* factory,
                          std::span<const std::byte> encoded,
                          std::shared_ptr<const RichImage>& output,
                          const std::atomic_bool* cancellation)
    {
        if (! factory || encoded.empty())
            return E_INVALIDARG;
        if (encoded.size() > RichImage::MaximumEncodedBytes)
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        const auto* clsid = BuiltinDecoder(encoded);
        if (! clsid)
            return WINCODEC_ERR_COMPONENTNOTFOUND;
        if (Cancelled(cancellation))
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        // Retain the stream's backing memory until all lazy WIC sources retire.
        std::vector<BYTE> owned(encoded.size());
        std::memcpy(owned.data(), encoded.data(), encoded.size());
        wil::com_ptr_nothrow<IWICStream> stream;
        RETURN_IF_FAILED(factory->CreateStream(stream.put()));
        RETURN_IF_FAILED(stream->InitializeFromMemory(owned.data(), static_cast<DWORD>(owned.size())));
        wil::com_ptr_nothrow<IWICBitmapDecoder> decoder;
        RETURN_IF_FAILED(CoCreateInstance(*clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(decoder.put())));
        RETURN_IF_FAILED(decoder->Initialize(stream.get(), WICDecodeMetadataCacheOnLoad));
        if (Cancelled(cancellation))
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        wil::com_ptr_nothrow<IWICBitmapFrameDecode> frame;
        RETURN_IF_FAILED(decoder->GetFrame(0u, frame.put()));
        unsigned width{}, height{};
        RETURN_IF_FAILED(frame->GetSize(&width, &height));
        if (! DimensionsAllowed(width, height))
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        unsigned orientation = 1u;
        if (*clsid == CLSID_WICJpegDecoder || *clsid == CLSID_WICTiffDecoder)
        {
            wil::com_ptr_nothrow<IWICMetadataQueryReader> metadata;
            const auto reader = frame->GetMetadataQueryReader(metadata.put());
            if (reader == E_OUTOFMEMORY)
                return reader;
            if (SUCCEEDED(reader))
            {
                RETURN_IF_FAILED(
                    MetadataUnsigned(metadata.get(), *clsid == CLSID_WICJpegDecoder ? L"/app1/ifd/{ushort=274}" : L"/ifd/{ushort=274}", orientation));
                if (orientation < 1u || orientation > 8u)
                    orientation = 1u;
            }
        }
        if (Cancelled(cancellation))
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        wil::com_ptr_nothrow<IWICFormatConverter> converter;
        RETURN_IF_FAILED(factory->CreateFormatConverter(converter.put()));
        RETURN_IF_FAILED(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom));
        std::vector<std::byte> pixels(static_cast<std::size_t>(width) * height * 4u);
        RETURN_IF_FAILED(converter->CopyPixels(nullptr, width * 4u, static_cast<UINT>(pixels.size()), reinterpret_cast<BYTE*>(pixels.data())));
        if (Cancelled(cancellation))
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        if (*clsid == CLSID_WICGifDecoder)
            RETURN_IF_FAILED(ComposeGifCanvas(factory, decoder.get(), frame.get(), pixels, width, height, cancellation));
        return Orient(pixels, width, height, orientation, output, cancellation);
    }
};
} // namespace Detail
HRESULT RichImage::CreatePbgra(
    unsigned width, unsigned height, std::span<const std::byte> pixels, std::shared_ptr<const RichImage>& output, const std::atomic_bool* cancellation) noexcept
try
{
    if (! Detail::DimensionsAllowed(width, height) || pixels.size() != static_cast<std::size_t>(width) * height * 4u)
        return E_INVALIDARG;
    for (std::size_t at = 0u; at < pixels.size(); at += 4u)
    {
        if (at % (width * 4u) == 0u && Detail::Cancelled(cancellation))
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        if (pixels[at] > pixels[at + 3u] || pixels[at + 1u] > pixels[at + 3u] || pixels[at + 2u] > pixels[at + 3u])
            return E_INVALIDARG;
    }
    return Detail::RichImageDecoder::Orient(pixels, width, height, 1u, output, cancellation);
}
catch (const std::bad_alloc&)
{
    return E_OUTOFMEMORY;
}
HRESULT RichImage::Decode(IWICImagingFactory* factory,
                          std::span<const std::byte> encoded,
                          std::shared_ptr<const RichImage>& output,
                          const std::atomic_bool* cancellation) noexcept
try
{
    return Detail::RichImageDecoder::Decode(factory, encoded, output, cancellation);
}
catch (const std::bad_alloc&)
{
    return E_OUTOFMEMORY;
}
HRESULT RichImage::LoadFile(IWICImagingFactory* factory,
                            std::wstring_view absolutePath,
                            std::shared_ptr<const RichImage>& output,
                            const std::atomic_bool* cancellation) noexcept
try
{
    if (! factory)
        return E_INVALIDARG;
    if (Detail::Cancelled(cancellation))
        return HRESULT_FROM_WIN32(ERROR_CANCELLED);
    const auto path = Detail::NormalizeDocumentFilePath(absolutePath);
    if (path.empty())
        return E_INVALIDARG;
    std::wstring native(0u, L' ');
    if (path.starts_with(L"\\\\"))
        native.append(L"\\\\?\\UNC\\").append(path, 2u);
    else
        native.append(L"\\\\?\\").append(path);
    if (native.size() > 32767u)
        return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
    const auto& calls = Detail::FileCalls();
    wil::unique_hfile file(
        calls.create(native.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (! file)
        return HRESULT_FROM_WIN32(GetLastError());
    if (calls.type(file.get()) != FILE_TYPE_DISK)
        return E_INVALIDARG;
    LARGE_INTEGER length{};
    if (! calls.size(file.get(), &length))
        return HRESULT_FROM_WIN32(GetLastError());
    if (length.QuadPart <= 0)
        return WINCODEC_ERR_BADIMAGE;
    if (length.QuadPart > MaximumEncodedBytes)
        return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length.QuadPart));
    std::size_t at{};
    while (at < bytes.size())
    {
        if (Detail::Cancelled(cancellation))
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        DWORD count{};
        if (! calls.read(file.get(), bytes.data() + at, static_cast<DWORD>(std::min<std::size_t>(65536u, bytes.size() - at)), &count, nullptr))
            return HRESULT_FROM_WIN32(GetLastError());
        if (! count)
            return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
        at += count;
    }
    file.reset();
    return Decode(factory, bytes, output, cancellation);
}
catch (const std::bad_alloc&)
{
    return E_OUTOFMEMORY;
}
catch (const std::length_error&)
{
    return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
}
catch (const std::invalid_argument&)
{
    return E_INVALIDARG;
}
} // namespace DxUi::RichText
