#include <DxUi/Failure.h>

#include "DxUi.RichImage.h"
#include "DxUi.RichImagePaths.h"
#include <DxUi/RichImage.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <shlwapi.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <wincodec.h>
#include <xmllite.h>
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "xmllite.lib")

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
RichImage::RichImage(unsigned width, unsigned height) : width_(width), height_(height), pixels_(static_cast<std::size_t>(width) * height * 4u), svg_(0u)
{
}
RichImage::RichImage(unsigned width, unsigned height, std::span<const std::byte> svg)
    : width_(width),
      height_(height),
      pixels_(0u),
      svg_(svg.begin(), svg.end())
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
// Where a GIF's first frame lies on its logical screen and what fills the rest.
struct GifPlacement
{
    unsigned canvasWidth{}, canvasHeight{}, left{}, top{};
    std::array<std::byte, 4u> fill{};
    bool covers{true};
};
HRESULT PlaceGifFrame(
    IWICImagingFactory* factory, IWICBitmapDecoder* decoder, IWICBitmapFrameDecode* frame, unsigned width, unsigned height, GifPlacement& placement)
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
        fill             = {{std::byte(color & 255u), std::byte((color >> 8u) & 255u), std::byte((color >> 16u) & 255u), std::byte(255u)}};
    }
    placement = {canvasWidth, canvasHeight, left, top, fill, false};
    return S_OK;
}
// Composes in place: the frame's pixels already lie at their offset inside the canvas (CopyPixels with the canvas
// stride), so each frame pixel is blended over the fill where it stands and every other pixel becomes the fill.
HRESULT ComposeGifCanvas(std::span<std::byte> canvas, unsigned width, unsigned height, const GifPlacement& placement, const std::atomic_bool* cancellation)
{
    const auto& fill = placement.fill;
    for (unsigned y = 0u; y < placement.canvasHeight; ++y)
    {
        if (Cancelled(cancellation))
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        const bool frameRow = y >= placement.top && y - placement.top < height;
        auto* row           = canvas.data() + static_cast<std::size_t>(y) * placement.canvasWidth * 4u;
        for (unsigned x = 0u; x < placement.canvasWidth; ++x)
        {
            auto* pixel = row + static_cast<std::size_t>(x) * 4u;
            if (! frameRow || x < placement.left || x - placement.left >= width)
            {
                std::memcpy(pixel, fill.data(), 4u);
                continue;
            }
            const auto alpha = std::to_integer<unsigned>(pixel[3]);
            for (unsigned channel = 0u; channel < 4u; ++channel)
                pixel[channel] =
                    std::byte(std::to_integer<unsigned>(pixel[channel]) + (std::to_integer<unsigned>(fill[channel]) * (255u - alpha) + 127u) / 255u);
        }
    }
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
    // The same transforms applied inside the decoded pixels where the stride survives: every orientation of a square
    // image and the non-transposing orientations 2-4. A transpose followed by mirroring the columns and/or rows equals
    // the source-to-destination mapping of Orient. Returns S_FALSE, untouched, when it cannot apply in place.
    static HRESULT OrientInPlace(std::span<std::byte> pixels, unsigned width, unsigned height, unsigned orientation, const std::atomic_bool* cancellation)
    {
        if (! DimensionsAllowed(width, height) || pixels.size() != static_cast<std::size_t>(width) * height * 4u || orientation < 1u || orientation > 8u)
            return E_INVALIDARG;
        const bool transpose = orientation >= 5u;
        if (transpose && width != height)
            return S_FALSE;
        if (Cancelled(cancellation))
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        const auto swap = [&](std::size_t first, std::size_t second) noexcept
        { std::swap_ranges(pixels.data() + first * 4u, pixels.data() + first * 4u + 4u, pixels.data() + second * 4u); };
        if (transpose)
            for (unsigned y = 0u; y < height; ++y)
            {
                if (Cancelled(cancellation))
                    return HRESULT_FROM_WIN32(ERROR_CANCELLED);
                for (unsigned x = y + 1u; x < width; ++x)
                    swap(static_cast<std::size_t>(y) * width + x, static_cast<std::size_t>(x) * width + y);
            }
        const bool mirrorX = orientation == 2u || orientation == 3u || orientation == 6u || orientation == 7u;
        const bool mirrorY = orientation == 3u || orientation == 4u || orientation == 7u || orientation == 8u;
        if (mirrorX || mirrorY)
            for (unsigned y = 0u; y < height; ++y)
            {
                if (Cancelled(cancellation))
                    return HRESULT_FROM_WIN32(ERROR_CANCELLED);
                const unsigned targetY = mirrorY ? height - 1u - y : y;
                for (unsigned x = 0u; x < width; ++x)
                {
                    const std::size_t from = static_cast<std::size_t>(y) * width + x;
                    const std::size_t to   = static_cast<std::size_t>(targetY) * width + (mirrorX ? width - 1u - x : x);
                    if (from < to)
                        swap(from, to);
                }
            }
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
        // The stream reads the caller's span in place, borrowed for this call only. Lifetime invariant: every WIC
        // object (stream, decoder, frame, metadata readers, palette, format converter) is a local of this call and
        // is handed to nothing that retains it; ComposeGifCanvas and the orientation transforms work on owned pixels
        // only; and CopyPixels has materialized every published pixel before Decode returns, so no WIC object or lazy
        // source outlives the span. The built-in decoders only read their stream, which makes the const_cast safe.
        wil::com_ptr_nothrow<IWICStream> stream;
        RETURN_IF_FAILED(factory->CreateStream(stream.put()));
        RETURN_IF_FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(reinterpret_cast<const BYTE*>(encoded.data())), static_cast<DWORD>(encoded.size())));
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
                const auto orientationResult =
                    MetadataUnsigned(metadata.get(), *clsid == CLSID_WICJpegDecoder ? L"/app1/ifd/{ushort=274}" : L"/ifd/{ushort=274}", orientation);
                if (orientationResult == E_OUTOFMEMORY)
                    return orientationResult;
                // Orientation is optional. A corrupt or unsupported metadata
                // value does not invalidate otherwise decodable image pixels.
                if (FAILED(orientationResult) || orientation < 1u || orientation > 8u)
                    orientation = 1u;
            }
        }
        if (Cancelled(cancellation))
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        // A GIF's first frame is copied straight into its canvas, at its offset, when the placement is known.
        GifPlacement placement;
        const HRESULT placed = *clsid == CLSID_WICGifDecoder ? PlaceGifFrame(factory, decoder.get(), frame.get(), width, height, placement) : S_OK;
        const bool canvas    = SUCCEEDED(placed) && ! placement.covers;
        wil::com_ptr_nothrow<IWICFormatConverter> converter;
        RETURN_IF_FAILED(factory->CreateFormatConverter(converter.put()));
        RETURN_IF_FAILED(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom));
        auto staged = std::shared_ptr<RichImage>(canvas ? new RichImage(placement.canvasWidth, placement.canvasHeight) : new RichImage(width, height));
        const std::size_t offset = canvas ? (static_cast<std::size_t>(placement.top) * placement.canvasWidth + placement.left) * 4u : 0u;
        RETURN_IF_FAILED(converter->CopyPixels(nullptr,
                                               (canvas ? placement.canvasWidth : width) * 4u,
                                               static_cast<UINT>(staged->pixels_.size() - offset),
                                               reinterpret_cast<BYTE*>(staged->pixels_.data() + offset)));
        if (Cancelled(cancellation))
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        // A placement failure is reported where composition reported it before: after the frame's pixels decoded.
        if (FAILED(placed))
            return placed;
        if (canvas)
        {
            RETURN_IF_FAILED(ComposeGifCanvas(staged->pixels_, width, height, placement, cancellation));
            width  = placement.canvasWidth;
            height = placement.canvasHeight;
        }
        if (orientation != 1u)
        {
            const HRESULT oriented = OrientInPlace(staged->pixels_, width, height, orientation, cancellation);
            if (FAILED(oriented))
                return oriented;
            // A transposing orientation of a non-square image needs the one additional decoded-size buffer.
            if (oriented == S_FALSE)
                return Orient(staged->pixels_, width, height, orientation, output, cancellation);
        }
        if (Cancelled(cancellation))
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        staged->width_  = width;
        staged->height_ = height;
        output          = std::move(staged);
        return S_OK;
    }
};
} // namespace Detail
HRESULT RichImage::CreateSvg(unsigned width,
                             unsigned height,
                             std::span<const std::byte> encoded,
                             std::shared_ptr<const RichImage>& output,
                             const std::atomic_bool* cancellation) noexcept
try
{
    Failure::Scope failureScope;
    if (! Detail::DimensionsAllowed(width, height) || encoded.empty() || encoded.size() > MaximumSvgBytes)
        return E_INVALIDARG;
    if (Detail::Cancelled(cancellation))
        return HRESULT_FROM_WIN32(ERROR_CANCELLED);
    // The conservative profile does not admit entity references, external
    // resources, CSS, recursive use, embedded images, scripts or animation.
    if (std::ranges::any_of(encoded,
                            [](std::byte unit)
    {
        const auto value = std::to_integer<unsigned>(unit);
        return value == '&' || value >= 0x80u || (value < 0x20u && value != '\t' && value != '\r' && value != '\n');
    }))
        return E_INVALIDARG;
    wil::com_ptr<IStream> stream;
    stream.attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(encoded.data()), static_cast<UINT>(encoded.size())));
    if (! stream)
        return E_OUTOFMEMORY;
    wil::com_ptr<IXmlReader> reader;
    RETURN_IF_FAILED(CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(reader.put()), nullptr));
    RETURN_IF_FAILED(reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit));
    RETURN_IF_FAILED(reader->SetInput(stream.get()));
    constexpr std::wstring_view svgNamespace = L"http://www.w3.org/2000/svg";
    constexpr std::wstring_view elements[]   = {L"svg", L"g", L"path", L"rect", L"circle", L"ellipse", L"line", L"polyline", L"polygon"};
    constexpr std::wstring_view attributes[] = {L"viewBox",
                                                L"width",
                                                L"height",
                                                L"x",
                                                L"y",
                                                L"cx",
                                                L"cy",
                                                L"r",
                                                L"rx",
                                                L"ry",
                                                L"x1",
                                                L"x2",
                                                L"y1",
                                                L"y2",
                                                L"d",
                                                L"points",
                                                L"fill",
                                                L"stroke",
                                                L"stroke-width",
                                                L"opacity",
                                                L"fill-opacity",
                                                L"stroke-opacity",
                                                L"stroke-linecap",
                                                L"stroke-linejoin",
                                                L"fill-rule",
                                                L"transform"};
    std::size_t count{}, pathUnits{};
    bool root{};
    XmlNodeType node{};
    HRESULT result{};
    while ((result = reader->Read(&node)) == S_OK)
    {
        if (Detail::Cancelled(cancellation))
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        if (node == XmlNodeType_Element)
        {
            UINT depth{}, length{};
            const wchar_t *name{}, *space{};
            RETURN_IF_FAILED(reader->GetDepth(&depth));
            RETURN_IF_FAILED(reader->GetLocalName(&name, &length));
            const std::wstring_view element(name, length);
            RETURN_IF_FAILED(reader->GetNamespaceUri(&space, &length));
            if (std::wstring_view(space, length) != svgNamespace || ++count > MaximumSvgElements || depth >= MaximumSvgDepth ||
                std::ranges::find(elements, element) == std::end(elements) || (! root && (depth != 0u || element != L"svg")) || (root && element == L"svg"))
                return E_INVALIDARG;
            root = true;
            while ((result = reader->MoveToNextAttribute()) == S_OK)
            {
                RETURN_IF_FAILED(reader->GetLocalName(&name, &length));
                const std::wstring_view attribute(name, length);
                RETURN_IF_FAILED(reader->GetNamespaceUri(&space, &length));
                const std::wstring_view attributeNamespace(space, length);
                const wchar_t* value{};
                RETURN_IF_FAILED(reader->GetValue(&value, &length));
                const std::wstring_view contents(value, length);
                if (attributeNamespace == L"http://www.w3.org/2000/xmlns/")
                {
                    if (depth || attribute != L"xmlns" || contents != svgNamespace)
                        return E_INVALIDARG;
                    continue;
                }
                if (! attributeNamespace.empty() || std::ranges::find(attributes, attribute) == std::end(attributes))
                    return E_INVALIDARG;
                if (attribute == L"d" || attribute == L"points")
                {
                    if (contents.size() > MaximumSvgPathUnits - pathUnits)
                        return E_INVALIDARG;
                    pathUnits += contents.size();
                }
                for (std::size_t index = 0u; index < contents.size(); ++index)
                    if (contents[index] == L'\\' || contents[index] == L'&' ||
                        (index + 3u <= contents.size() && _wcsnicmp(contents.data() + index, L"url", 3u) == 0))
                        return E_INVALIDARG;
            }
            if (FAILED(result))
                return result;
            RETURN_IF_FAILED(reader->MoveToElement());
        }
        else if (node != XmlNodeType_EndElement && node != XmlNodeType_Whitespace && node != XmlNodeType_XmlDeclaration && node != XmlNodeType_Comment)
            return E_INVALIDARG;
    }
    if (FAILED(result) || ! root)
        return FAILED(result) ? result : E_INVALIDARG;
    if (Detail::Cancelled(cancellation))
        return HRESULT_FROM_WIN32(ERROR_CANCELLED);
    auto staged = std::shared_ptr<const RichImage>(new RichImage(width, height, encoded));
    if (Detail::Cancelled(cancellation))
        return HRESULT_FROM_WIN32(ERROR_CANCELLED);
    output.swap(staged);
    return S_OK;
}
catch (const std::bad_alloc&)
{
    Failure::FailAllocation();
}
HRESULT RichImage::CreatePbgra(
    unsigned width, unsigned height, std::span<const std::byte> pixels, std::shared_ptr<const RichImage>& output, const std::atomic_bool* cancellation) noexcept
try
{
    Failure::Scope failureScope;
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
    Failure::FailAllocation();
}
HRESULT RichImage::Decode(IWICImagingFactory* factory,
                          std::span<const std::byte> encoded,
                          std::shared_ptr<const RichImage>& output,
                          const std::atomic_bool* cancellation) noexcept
try
{
    Failure::Scope failureScope;
    return Detail::RichImageDecoder::Decode(factory, encoded, output, cancellation);
}
catch (const std::bad_alloc&)
{
    Failure::FailAllocation();
}
HRESULT RichImage::LoadFile(IWICImagingFactory* factory,
                            std::wstring_view absolutePath,
                            std::shared_ptr<const RichImage>& output,
                            const std::atomic_bool* cancellation) noexcept
try
{
    Failure::Scope failureScope;
    if (! factory)
        return E_INVALIDARG;
    if (Detail::Cancelled(cancellation))
        return HRESULT_FROM_WIN32(ERROR_CANCELLED);
    std::wstring path(0u, L' ');
    const auto admission = Detail::TryNormalizeDocumentFilePath(absolutePath, path, true);
    if (admission == SourceResult::CapacityExceeded)
        return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
    if (admission != SourceResult::Success || path.empty())
        return E_INVALIDARG;
    if (path.size() > 32767u)
        return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
    const auto& calls = Detail::FileCalls();
    wil::unique_hfile file(
        calls.create(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
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
    // The read buffer is the decode's only encoded copy: Decode streams it in place and has released every WIC object
    // before it returns, while bytes is still alive.
    return Decode(factory, bytes, output, cancellation);
}
catch (const std::bad_alloc&)
{
    Failure::FailAllocation();
}
catch (const std::exception&)
{
    Failure::FailUnexpected();
}
} // namespace DxUi::RichText
