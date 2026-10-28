#pragma once
#include <DxUi/Configuration.h>
#include <DxUi/RichImage.h>
#include <string>
#include <windows.h>
namespace DxUi::RichText::Detail
{
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
struct RichImageRequestState
{
    RichImageRequestState(std::wstring_view path, std::uint64_t requestId, RichImageSourceKind kind = RichImageSourceKind::LocalFile)
        : filePath(path),
          id(requestId),
          sourceKind(kind)
    {
    }
    std::wstring filePath;
    std::uint64_t id;
    RichImageSourceKind sourceKind;
    std::atomic_bool cancelled{false};
};
#pragma warning(pop)
// Dormant, owner/worker-thread-local transport seam. Tests retain real file
// handles and memory while injecting read failure/cancellation deterministically.
struct RichImageFileCalls
{
    decltype(&CreateFileW) create = &CreateFileW;
    decltype(&GetFileType) type   = &GetFileType;
    decltype(&GetFileSizeEx) size = &GetFileSizeEx;
    decltype(&ReadFile) read      = &ReadFile;
};
#if DXUI_ENABLE_DIAGNOSTICS
extern thread_local const RichImageFileCalls* testRichImageFileCalls;
#endif
} // namespace DxUi::RichText::Detail
