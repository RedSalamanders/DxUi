#pragma once

#include <msctf.h>
#include <textstor.h>

// Included after NativeTextStoreTestSink; these tests exercise the production COM store with an application adapter.
class ClientTextRange final : public ITfRangeACP
{
public:
    LONG start = 1, length = 4;
    ULONG references = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) noexcept override
    {
        if (! out)
            return E_POINTER;
        *out = nullptr;
        if (iid != __uuidof(IUnknown) && iid != __uuidof(ITfRange) && iid != __uuidof(ITfRangeACP))
            return E_NOINTERFACE;
        *out = static_cast<ITfRangeACP*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return ++references;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return --references;
    }
    HRESULT STDMETHODCALLTYPE GetExtent(LONG* first, LONG* count) noexcept override
    {
        *first = start;
        *count = length;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetExtent(LONG first, LONG count) noexcept override
    {
        start  = first;
        length = count;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetText(TfEditCookie, DWORD, WCHAR*, ULONG, ULONG*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetText(TfEditCookie, DWORD, const WCHAR*, LONG) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetFormattedText(TfEditCookie, IDataObject**) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetEmbedded(TfEditCookie, REFGUID, REFIID, IUnknown**) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE InsertEmbedded(TfEditCookie, DWORD, IDataObject*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE ShiftStart(TfEditCookie, LONG, LONG*, const TF_HALTCOND*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE ShiftEnd(TfEditCookie, LONG, LONG*, const TF_HALTCOND*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE ShiftStartToRange(TfEditCookie, ITfRange*, TfAnchor) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE ShiftEndToRange(TfEditCookie, ITfRange*, TfAnchor) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE ShiftStartRegion(TfEditCookie, TfShiftDir, BOOL*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE ShiftEndRegion(TfEditCookie, TfShiftDir, BOOL*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE IsEmpty(TfEditCookie, BOOL*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Collapse(TfEditCookie, TfAnchor) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE IsEqualStart(TfEditCookie, ITfRange*, TfAnchor, BOOL*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE IsEqualEnd(TfEditCookie, ITfRange*, TfAnchor, BOOL*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE CompareStart(TfEditCookie, ITfRange*, TfAnchor, LONG*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE CompareEnd(TfEditCookie, ITfRange*, TfAnchor, LONG*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE AdjustForInsert(TfEditCookie, ULONG, BOOL*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetGravity(TfGravity*, TfGravity*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetGravity(TfEditCookie, TfGravity, TfGravity) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Clone(ITfRange**) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetContext(ITfContext**) noexcept override
    {
        return E_NOTIMPL;
    }
};

class ClientComposition final : public ITfCompositionView
{
public:
    ClientTextRange range;
    ULONG references = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) noexcept override
    {
        if (! out)
            return E_POINTER;
        *out = nullptr;
        if (iid != __uuidof(IUnknown) && iid != __uuidof(ITfCompositionView))
            return E_NOINTERFACE;
        *out = static_cast<ITfCompositionView*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return ++references;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return --references;
    }
    HRESULT STDMETHODCALLTYPE GetOwnerClsid(CLSID* clsid) noexcept override
    {
        *clsid = GUID_NULL;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetRange(ITfRange** out) noexcept override
    {
        *out = &range;
        range.AddRef();
        return S_OK;
    }
};
