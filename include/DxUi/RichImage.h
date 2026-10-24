#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>
#include <windows.h>
struct IWICImagingFactory;
namespace DxUi
{
class RichTextEditor;
}
namespace DxUi::RichText
{
namespace Detail
{
class RichImageDecoder;
class RichImageResources;
struct RichImageRequestState;
} // namespace Detail
enum class RichImageSourceKind
{
    LocalFile,
    RemoteHttp
};
// A caller-owned worker may retain this immutable request. Its cancellation
// flag has no callbacks; retiring an editor/request never runs application code.
class RichImageRequest final
{
public:
    RichImageRequest() noexcept                             = default;
    bool operator==(const RichImageRequest&) const noexcept = default;
    std::uint64_t Id() const noexcept;
    RichImageSourceKind SourceKind() const noexcept;
    std::wstring_view Source() const noexcept;
    // Compatibility/local-only accessor. Remote requests return an empty path;
    // applications must explicitly branch on SourceKind before transport I/O.
    std::wstring_view FilePath() const noexcept;
    bool Cancelled() const noexcept;
    const std::atomic_bool* Cancellation() const noexcept;

private:
    explicit RichImageRequest(std::shared_ptr<Detail::RichImageRequestState> state) noexcept : state_(std::move(state))
    {
    }
    std::shared_ptr<Detail::RichImageRequestState> state_;
    friend class Detail::RichImageResources;
};
struct RichImageResourceStatistics
{
    std::size_t imageElements{}, requestedSources{}, readySources{}, pendingSources{}, failedSources{}, decodedBytes{};
};
// Immutable device-independent pixels. Files and WIC sources never cross
// publication; a host may retain these pixels across D2D device replacement.
class RichImage final
{
public:
    static constexpr unsigned MaximumDimension = 4096u, MaximumPixels = 4194304u, MaximumEncodedBytes = 16777216u;
    unsigned Width() const noexcept
    {
        return width_;
    }
    unsigned Height() const noexcept
    {
        return height_;
    }
    unsigned Stride() const noexcept
    {
        return width_ * 4u;
    }
    std::span<const std::byte> Pixels() const noexcept
    {
        return pixels_;
    }
    // Exact tightly packed, premultiplied BGRA, top row first. Invalid pixels,
    // capacity, cancellation or allocation failure leave output unchanged.
    static HRESULT CreatePbgra(unsigned width,
                               unsigned height,
                               std::span<const std::byte> pixels,
                               std::shared_ptr<const RichImage>& output,
                               const std::atomic_bool* cancellation = nullptr) noexcept;
    // Explicit preparation/worker utilities. The caller initializes COM and
    // supplies its WIC factory. Built-in PNG/JPEG/BMP/GIF/TIFF only, first frame;
    // JPEG/TIFF EXIF orientation is applied before immutable publication.
    // These synchronous functions MUST NOT run during paint/composition or on
    // a latency-sensitive UI path. Cancellation is borrowed for this call only.
    static HRESULT Decode(IWICImagingFactory* factory,
                          std::span<const std::byte> encoded,
                          std::shared_ptr<const RichImage>& output,
                          const std::atomic_bool* cancellation = nullptr) noexcept;
    // Absolute Unicode filesystem path only; no CWD, URI resolution or remote
    // HTTP fetching. UNC access follows caller policy. Bounded file reads;
    // source sharing denies concurrent writers.
    static HRESULT LoadFile(IWICImagingFactory* factory,
                            std::wstring_view absolutePath,
                            std::shared_ptr<const RichImage>& output,
                            const std::atomic_bool* cancellation = nullptr) noexcept;
    RichImage(const RichImage&)            = delete;
    RichImage& operator=(const RichImage&) = delete;

private:
    RichImage(unsigned width, unsigned height);
    unsigned width_, height_;
    std::vector<std::byte> pixels_;
    friend class Detail::RichImageDecoder;
};
} // namespace DxUi::RichText
