#pragma once
#include "Controls/TextClipboard.h"
#include <DxUi/FileDrop.h>
#include <cstring>
#include <functional>
#include <vector>
// The SDK's WRL implementation uses its own interlocked target semantics.
#pragma warning(push)
#pragma warning(disable : 4746)
#include <wrl/implements.h>
#pragma warning(pop)
namespace RichFileDropTests
{
using namespace DxUi;
// COM test objects intentionally have identity and cannot be copied or moved.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class DropMediumOwner final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IUnknown>
{
public:
    DropMediumOwner(wil::unique_hglobal value, unsigned* releaseCount) noexcept : memory(std::move(value)), releases(releaseCount)
    {
    }
    ~DropMediumOwner() override
    {
        ++*releases;
    }
    wil::unique_hglobal memory;
    unsigned* releases;
};
class DropDataObject final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDataObject>
{
public:
    std::vector<std::byte> bytes;
    unsigned calls{}, releases{}, queries{};
    HRESULT getDataResult = S_OK;
    std::function<void()> duringGetData;
    std::function<void()> duringQuery;
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* format, STGMEDIUM* medium) override
    {
        ++calls;
        if (FAILED(getDataResult))
            return getDataResult;
        if (duringGetData)
            duringGetData();
        if (! format || ! medium || format->cfFormat != CF_HDROP || format->dwAspect != DVASPECT_CONTENT || format->lindex != -1 ||
            format->tymed != TYMED_HGLOBAL)
            return DV_E_FORMATETC;
        wil::unique_hglobal memory(GlobalAlloc(GMEM_MOVEABLE, bytes.size()));
        if (! memory)
            return E_OUTOFMEMORY;
        auto* destination = GlobalLock(memory.get());
        if (! destination)
            return E_OUTOFMEMORY;
        // Heap rounding must not accidentally supply a missing terminator.
        std::memset(destination, 0x41, GlobalSize(memory.get()));
        std::memcpy(destination, bytes.data(), bytes.size());
        static_cast<void>(GlobalUnlock(memory.get()));
        auto owner = Microsoft::WRL::Make<DropMediumOwner>(std::move(memory), &releases);
        if (owner.Get() == nullptr)
            return E_OUTOFMEMORY;
        medium->tymed          = TYMED_HGLOBAL;
        medium->hGlobal        = owner->memory.get();
        medium->pUnkForRelease = owner.Detach();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* format) override
    {
        ++queries;
        if (duringQuery)
            duringQuery();
        return format && format->cfFormat == CF_HDROP && format->dwAspect == DVASPECT_CONTENT && format->lindex == -1 && format->tymed == TYMED_HGLOBAL
                   ? S_OK
                   : DV_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC*) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD, IEnumFORMATETC**) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }
};
class DropReceiver final : public RichFileDropReceiver
{
public:
    unsigned previews{}, commits{};
    bool Preview(POINTL) noexcept override
    {
        ++previews;
        return true;
    }
    bool Drop(POINTL, IDataObject& data, RichFileDropGuard guard) noexcept override
    {
        TextClipboardContent content;
        if (ReadDataObjectFilePaths(&data, content) != S_OK || ! guard.current(guard.context))
            return false;
        ++commits;
        return true;
    }
};
#pragma warning(pop)

} // namespace RichFileDropTests
