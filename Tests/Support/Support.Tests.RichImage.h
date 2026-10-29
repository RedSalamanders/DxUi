#pragma once
#include "Controls/DxUi.RichImage.h"
#include <DxUi/RichImage.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <wil/com.h>
#include <wil/resource.h>
#include <wincodec.h>
namespace RichImageTests
{
using DxUi::RichText::RichImage;
using Pixels = RichImage;
inline HRESULT Decode(IWICImagingFactory* factory, std::span<const BYTE> encoded, std::shared_ptr<const RichImage>& output)
{
    return RichImage::Decode(factory, std::as_bytes(encoded), output);
}
inline std::vector<BYTE> Encode(
    IWICImagingFactory* factory, REFGUID container, std::span<BYTE> pixels, unsigned width, unsigned height, unsigned orientation = 1u)
{
    wil::com_ptr<IStream> stream;
    THROW_IF_FAILED(CreateStreamOnHGlobal(nullptr, TRUE, stream.put()));
    wil::com_ptr<IWICBitmapEncoder> encoder;
    THROW_IF_FAILED(factory->CreateEncoder(container, nullptr, encoder.put()));
    THROW_IF_FAILED(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache));
    wil::com_ptr<IWICBitmapFrameEncode> frame;
    wil::com_ptr<IPropertyBag2> options;
    THROW_IF_FAILED(encoder->CreateNewFrame(frame.put(), options.put()));
    THROW_IF_FAILED(frame->Initialize(options.get()));
    THROW_IF_FAILED(frame->SetSize(width, height));
    GUID format = GUID_WICPixelFormat32bppBGRA;
    THROW_IF_FAILED(frame->SetPixelFormat(&format));
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(factory->CreateBitmapFromMemory(
        width, height, GUID_WICPixelFormat32bppBGRA, width * 4u, static_cast<UINT>(pixels.size()), pixels.data(), bitmap.put()));
    wil::com_ptr<IWICPalette> palette;
    if (container == GUID_ContainerFormatGif)
    {
        THROW_IF_FAILED(factory->CreatePalette(palette.put()));
        THROW_IF_FAILED(palette->InitializeFromBitmap(bitmap.get(), 256u, FALSE));
        THROW_IF_FAILED(frame->SetPalette(palette.get()));
    }
    wil::com_ptr<IWICFormatConverter> converter;
    THROW_IF_FAILED(factory->CreateFormatConverter(converter.put()));
    THROW_IF_FAILED(converter->Initialize(bitmap.get(), format, WICBitmapDitherTypeNone, palette.get(), 0.0, WICBitmapPaletteTypeCustom));
    if (container == GUID_ContainerFormatTiff)
    {
        wil::com_ptr<IWICMetadataQueryWriter> writer;
        THROW_IF_FAILED(frame->GetMetadataQueryWriter(writer.put()));
        wil::unique_prop_variant value;
        value.vt    = VT_UI2;
        value.uiVal = static_cast<USHORT>(orientation);
        THROW_IF_FAILED(writer->SetMetadataByName(L"/ifd/{ushort=274}", &value));
    }
    THROW_IF_FAILED(frame->WriteSource(converter.get(), nullptr));
    THROW_IF_FAILED(frame->Commit());
    THROW_IF_FAILED(encoder->Commit());
    HGLOBAL memory{};
    THROW_IF_FAILED(GetHGlobalFromStream(stream.get(), &memory));
    auto* bytes = static_cast<const BYTE*>(GlobalLock(memory));
    if (! bytes)
        throw std::runtime_error("encoded fixture memory");
    const auto unlock = wil::scope_exit([&] noexcept { GlobalUnlock(memory); });
    STATSTG stat{};
    THROW_IF_FAILED(stream->Stat(&stat, STATFLAG_NONAME));
    return std::vector<BYTE>(bytes, bytes + stat.cbSize.QuadPart);
}
// Walk only the generated fixture's GIF blocks; never scan for a separator
// inside palette/compressed bytes. This is an independent container fixture,
// not a second image decoder.
inline std::size_t GifDescriptor(std::span<const BYTE> bytes)
{
    if (bytes.size() < 13u)
        throw std::runtime_error("short generated GIF");
    std::size_t at = 13u;
    if (bytes[10] & 0x80u)
        at += 3u * (2u << (bytes[10] & 7u));
    while (at < bytes.size() && bytes[at] == 0x21u)
    {
        at += 2u;
        while (at < bytes.size() && bytes[at])
        {
            const auto count = bytes[at];
            at += static_cast<std::size_t>(count) + 1u;
        }
        ++at;
    }
    if (at + 10u > bytes.size() || bytes[at] != 0x2cu)
        throw std::runtime_error("generated GIF image descriptor");
    return at;
}
class FileReadFixture
{
public:
    FileReadFixture() noexcept : previous_(DxUi::RichText::Detail::testRichImageFileCalls), previousFixture_(active_)
    {
        calls_.read                                    = &Read;
        active_                                        = this;
        DxUi::RichText::Detail::testRichImageFileCalls = &calls_;
    }
    ~FileReadFixture()
    {
        DxUi::RichText::Detail::testRichImageFileCalls = previous_;
        active_                                        = previousFixture_;
    }
    FileReadFixture(const FileReadFixture&)            = delete;
    FileReadFixture& operator=(const FileReadFixture&) = delete;
    std::size_t reads{}, maximumRead{};
    bool fail{}, zero{};
    std::atomic_bool* cancel{};

private:
    static BOOL WINAPI Read(HANDLE file, LPVOID bytes, DWORD count, LPDWORD read, LPOVERLAPPED overlapped)
    {
        ++active_->reads;
        active_->maximumRead = std::max(active_->maximumRead, static_cast<std::size_t>(count));
        if (active_->fail)
        {
            SetLastError(ERROR_READ_FAULT);
            return FALSE;
        }
        if (active_->zero)
        {
            *read = 0u;
            return TRUE;
        }
        const auto result = ReadFile(file, bytes, count, read, overlapped);
        if (active_->cancel)
            active_->cancel->store(true, std::memory_order_relaxed);
        return result;
    }
    DxUi::RichText::Detail::RichImageFileCalls calls_;
    const DxUi::RichText::Detail::RichImageFileCalls* previous_;
    FileReadFixture* previousFixture_;
    inline static thread_local FileReadFixture* active_{};
};
template <typename Check> inline void Files(IWICImagingFactory* factory, std::span<const BYTE> encoded, Check&& check)
{
    std::array<wchar_t, MAX_PATH> temp{};
    const auto count = GetTempPathW(static_cast<DWORD>(temp.size()), temp.data());
    check(count && count < temp.size(), "image fixture has a bounded real temporary directory");
    GUID id{};
    THROW_IF_FAILED(CoCreateGuid(&id));
    std::array<wchar_t, 40u> guid{};
    check(StringFromGUID2(id, guid.data(), static_cast<int>(guid.size())) > 0, "image fixture has unique owned file identity");
    std::wstring path(temp.data(), count);
    path.append(L"DxUi-image-\u6771\U0001f600-").append(guid.data()).append(L".png");
    bool created{};
    const auto cleanup = wil::scope_exit([&] noexcept
    {
        if (created)
            DeleteFileW(path.c_str());
    });
    wil::unique_hfile file(
        CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr));
    check(static_cast<bool>(file), "image fixture creates its own Unicode file without replacing another file");
    created = true;
    DWORD written{};
    check(WriteFile(file.get(), encoded.data(), static_cast<DWORD>(encoded.size()), &written, nullptr) && written == encoded.size(),
          "image fixture writes complete native PNG bytes");
    std::shared_ptr<const RichImage> image;
    check(RichImage::LoadFile(factory, path, image) == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION) && ! image,
          "image loading refuses an existing concurrent writer");
    file.reset();
    THROW_IF_FAILED(RichImage::LoadFile(factory, path, image));
    check(image->Width() == 2u && image->Height() == 3u, "absolute supplementary-Unicode file path decodes without a process CWD");
    const auto held = image;
    check(RichImage::LoadFile(factory, L"relative.png", image) == E_INVALIDARG && image == held, "relative file loading cannot depend on CWD");
    check(RichImage::LoadFile(factory, path + L".missing", image) == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) && image == held,
          "missing image preserves immutable resource");
    FileReadFixture transport;
    transport.fail = true;
    check(RichImage::LoadFile(factory, path, image) == HRESULT_FROM_WIN32(ERROR_READ_FAULT) && image == held,
          "real-file read failure publishes no partial pixels");
    transport.fail = false;
    transport.zero = true;
    check(RichImage::LoadFile(factory, path, image) == HRESULT_FROM_WIN32(ERROR_HANDLE_EOF) && image == held, "premature EOF cannot publish a partial source");
    transport.zero = false;
    file.reset(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY, nullptr));
    check(static_cast<bool>(file), "image fixture opens only its existing source for padding");
    LARGE_INTEGER padded{};
    padded.QuadPart = 131072;
    check(SetFilePointerEx(file.get(), padded, nullptr, FILE_BEGIN) && SetEndOfFile(file.get()), "image fixture provides multiple physical read chunks");
    file.reset();
    std::atomic_bool cancel{false};
    transport.cancel = &cancel;
    transport.reads  = 0u;
    check(RichImage::LoadFile(factory, path, image, &cancel) == HRESULT_FROM_WIN32(ERROR_CANCELLED) && image == held && transport.reads == 1u &&
              transport.maximumRead <= 65536u,
          "cancellation between bounded real reads stops before remaining I/O/decode/publication");
    transport.cancel = nullptr;
    file.reset(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY, nullptr));
    padded.QuadPart = static_cast<LONGLONG>(RichImage::MaximumEncodedBytes) + 1;
    check(file && SetFilePointerEx(file.get(), padded, nullptr, FILE_BEGIN) && SetEndOfFile(file.get()), "image fixture declares an oversized file");
    file.reset();
    transport.reads = 0u;
    check(RichImage::LoadFile(factory, path, image) == HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW) && image == held && ! transport.reads,
          "oversized image file is rejected before reading or decoding");
}
template <typename Check> inline void Core(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    check(SUCCEEDED(initialized) || initialized == RPC_E_CHANGED_MODE, "image decoder initializes or borrows its caller COM apartment");
    const auto apartment = wil::scope_exit([&] noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    wil::com_ptr<IWICImagingFactory> factory;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put())));
    std::array<BYTE, 24u> source{};
    for (unsigned i = 0u; i < 6u; ++i)
    {
        source[i * 4u]      = static_cast<BYTE>((i + 1u) * 30u);
        source[i * 4u + 3u] = 255u;
    }
    std::shared_ptr<const Pixels> output;
    for (const auto& type : {GUID_ContainerFormatBmp, GUID_ContainerFormatGif, GUID_ContainerFormatTiff})
    {
        const auto encoded = Encode(factory.get(), type, source, 2u, 3u);
        THROW_IF_FAILED(Decode(factory.get(), encoded, output));
        check(output->Width() == 2u && output->Height() == 3u &&
                  std::equal(source.begin(), source.end(), output->Pixels().begin(), [](BYTE a, std::byte b) { return a == std::to_integer<BYTE>(b); }),
              "lossless built-in bitmap/GIF/TIFF decoder fixture");
    }
    constexpr unsigned expected[8][6] = {{1, 2, 3, 4, 5, 6},
                                         {2, 1, 4, 3, 6, 5},
                                         {6, 5, 4, 3, 2, 1},
                                         {5, 6, 3, 4, 1, 2},
                                         {1, 3, 5, 2, 4, 6},
                                         {5, 3, 1, 6, 4, 2},
                                         {6, 4, 2, 5, 3, 1},
                                         {2, 4, 6, 1, 3, 5}};
    for (unsigned orientation = 1u; orientation <= 8u; ++orientation)
    {
        const auto encoded = Encode(factory.get(), GUID_ContainerFormatTiff, source, 2u, 3u, orientation);
        THROW_IF_FAILED(Decode(factory.get(), encoded, output));
        check(output->Width() == (orientation >= 5u ? 3u : 2u), "TIFF metadata changes actual decoded dimensions");
        for (unsigned i = 0u; i < 6u; ++i)
            check(std::to_integer<unsigned>(output->Pixels()[i * 4u]) == expected[orientation - 1u][i] * 30u,
                  "TIFF EXIF metadata matches independent orientation pixels");
    }
    std::array<BYTE, 256u> solid{};
    for (unsigned i = 0u; i < 64u; ++i)
    {
        solid[i * 4u]      = 224u;
        solid[i * 4u + 1u] = 160u;
        solid[i * 4u + 2u] = 80u;
        solid[i * 4u + 3u] = 255u;
    }
    const auto encoded = Encode(factory.get(), GUID_ContainerFormatJpeg, solid, 8u, 8u);
    THROW_IF_FAILED(Decode(factory.get(), encoded, output));
    check(output->Width() == 8u && output->Height() == 8u, "JPEG dimensions");
    for (unsigned i = 0u; i < 64u; ++i)
        for (unsigned c = 0u; c < 4u; ++c)
            check(std::abs(int(std::to_integer<unsigned>(output->Pixels()[i * 4u + c])) - int(solid[i * 4u + c])) <= 3,
                  "known solid JPEG pixel/color tolerance");
    const auto png = Encode(factory.get(), GUID_ContainerFormatPng, source, 2u, 3u);
    THROW_IF_FAILED(Decode(factory.get(), png, output));
    check(std::equal(source.begin(), source.end(), output->Pixels().begin(), [](BYTE a, std::byte b) { return a == std::to_integer<BYTE>(b); }),
          "PNG lossless source pixels");
    source[0]        = 255u;
    source[3]        = 128u;
    const auto alpha = Encode(factory.get(), GUID_ContainerFormatPng, source, 2u, 3u);
    THROW_IF_FAILED(Decode(factory.get(), alpha, output));
    check(output->Pixels()[0] == std::byte(128u) && output->Pixels()[3] == std::byte(128u), "PNG alpha is premultiplied before publication");
    const auto held = output;
    for (std::size_t size = 1u; size < 32u; ++size)
        check(FAILED(Decode(factory.get(), std::span(png).first(std::min(size, png.size())), output)) && output == held,
              "truncated image data never replaces immutable output");
    std::atomic_bool cancelled{true};
    check(RichImage::Decode(factory.get(), std::as_bytes(std::span(png)), output, &cancelled) == HRESULT_FROM_WIN32(ERROR_CANCELLED) && output == held,
          "canceled decode preserves its prior resource");
    check(RichImage::CreatePbgra(2u, 3u, std::as_bytes(std::span(source)), output) == E_INVALIDARG && output == held,
          "non-premultiplied caller pixels are rejected");
    check(RichImage::CreatePbgra(4096u, 1025u, {}, output) == E_INVALIDARG && output == held, "decoded pixel capacity is checked before allocation");
    std::vector<std::byte> oversized(RichImage::MaximumEncodedBytes + 1u);
    check(RichImage::Decode(factory.get(), oversized, output) == HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW) && output == held,
          "encoded byte capacity is checked before decode");
    source[0]             = 30u;
    source[3]             = 255u;
    auto gif              = Encode(factory.get(), GUID_ContainerFormatGif, source, 2u, 3u);
    const auto descriptor = GifDescriptor(gif);
    gif[6]                = 4u;
    gif[7]                = 0u;
    gif[8]                = 5u;
    gif[9]                = 0u;
    gif[descriptor + 1u] = gif[descriptor + 3u] = 1u;
    gif[descriptor + 2u] = gif[descriptor + 4u] = 0u;
    THROW_IF_FAILED(Decode(factory.get(), gif, output));
    check(output->Width() == 4u && output->Height() == 5u, "partial GIF first frame uses its logical canvas and offsets");
    std::array<std::byte, 4u> fill{};
    if (gif[10] & 0x80u)
    {
        const auto palette = 13u + static_cast<std::size_t>(gif[11]) * 3u;
        check(palette + 3u <= descriptor, "generated GIF background palette lies before its descriptor");
        fill = {{std::byte(gif[palette + 2u]), std::byte(gif[palette + 1u]), std::byte(gif[palette]), std::byte(255u)}};
    }
    for (unsigned y = 0u; y < 5u; ++y)
        for (unsigned x = 0u; x < 4u; ++x)
        {
            const auto pixel = output->Pixels().subspan((static_cast<std::size_t>(y) * 4u + x) * 4u, 4u);
            if (x >= 1u && x < 3u && y >= 1u && y < 4u)
                check(std::to_integer<unsigned>(pixel[0]) == ((y - 1u) * 2u + x) * 30u && pixel[3] == std::byte(255u),
                      "partial GIF frame is composited at the declared pixel offset");
            else
                check(std::equal(fill.begin(), fill.end(), pixel.begin()), "partial GIF background matches global-palette/transparent canvas policy");
        }
    // Turn one known palette colour transparent without using the decoder to
    // construct the oracle. Both its frame pixel and uncovered canvas must be
    // transparent black after premultiplied materialization.
    auto transparentGif     = gif;
    const auto paletteStart = (gif[10] & 0x80u) ? 13u : descriptor + 10u;
    const auto paletteCount = 2u << ((gif[10] & 0x80u) ? (gif[10] & 7u) : (gif[descriptor + 9u] & 7u));
    check((gif[10] & 0x80u) || (gif[descriptor + 9u] & 0x80u), "transparent GIF fixture has an explicit palette");
    unsigned transparentIndex = paletteCount;
    for (unsigned index = 0u; index < paletteCount; ++index)
        if (gif[paletteStart + index * 3u] == 0u && gif[paletteStart + index * 3u + 1u] == 0u && gif[paletteStart + index * 3u + 2u] == 30u)
        {
            transparentIndex = index;
            break;
        }
    check(transparentIndex < paletteCount, "transparent fixture finds its independently known first colour");
    std::size_t extension = 13u + ((gif[10] & 0x80u) ? 3u * (2u << (gif[10] & 7u)) : 0u);
    bool changedControl{};
    while (extension < descriptor)
    {
        check(gif[extension] == 0x21u, "generated GIF contains only extension blocks before its first frame");
        if (gif[extension + 1u] == 0xf9u && gif[extension + 2u] == 4u)
        {
            transparentGif[extension + 3u] |= 1u;
            transparentGif[extension + 6u] = static_cast<BYTE>(transparentIndex);
            changedControl                 = true;
        }
        extension += 2u;
        while (gif[extension])
            extension += static_cast<std::size_t>(gif[extension]) + 1u;
        ++extension;
    }
    if (! changedControl)
    {
        const std::array<BYTE, 8u> control{0x21u, 0xf9u, 4u, 1u, 0u, 0u, static_cast<BYTE>(transparentIndex), 0u};
        transparentGif.insert(transparentGif.begin() + static_cast<std::ptrdiff_t>(descriptor), control.begin(), control.end());
    }
    THROW_IF_FAILED(Decode(factory.get(), transparentGif, output));
    for (unsigned y = 0u; y < 5u; ++y)
        for (unsigned x = 0u; x < 4u; ++x)
        {
            const auto pixel   = output->Pixels().subspan((static_cast<std::size_t>(y) * 4u + x) * 4u, 4u);
            const bool visible = x >= 1u && x < 3u && y >= 1u && y < 4u && (x != 1u || y != 1u);
            if (visible)
                check(std::to_integer<unsigned>(pixel[0]) == ((y - 1u) * 2u + x) * 30u && pixel[1] == std::byte{} && pixel[2] == std::byte{} &&
                          pixel[3] == std::byte(255u),
                      "transparent GIF retains every independently known opaque frame colour");
            else
                check(std::ranges::all_of(pixel, [](auto channel) { return channel == std::byte{}; }),
                      "transparent frame colour and uncovered GIF canvas materialize transparent black");
        }
    Files(factory.get(), png, check);
}
} // namespace RichImageTests
