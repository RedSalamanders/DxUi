#pragma once
// In-process stand-ins for what Text Services Framework gives a text store: an advise sink that grants each lock
// synchronously, and a composition view with its range. With them a test drives the production store
// (DxUi::CreateNativeTextInputTextStore) through the calls a text service makes, without a text service, a window or
// focus. The W8.5 IME copies scenario (Embedded.Tests.ImeCopies.h) and the FX7 performance fixture, which compiles in
// both test executables, share this one definition. Depends only on Windows and the standard library.
#include <msctf.h>
#include <textstor.h>

#include <atomic>
#include <functional>

// The doubles are owned in place and never copied. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
namespace TsfDoubles
{
// The store's sink: OnLockGranted runs onLock, inside the lock the store grants.
class Sink final : public ITextStoreACPSink
{
public:
    std::function<HRESULT()> onLock;
    size_t textChanges = 0u;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) noexcept override
    {
        if (! out)
            return E_POINTER;
        *out = nullptr;
        if (iid != __uuidof(IUnknown) && iid != __uuidof(ITextStoreACPSink))
            return E_NOINTERFACE;
        *out = static_cast<ITextStoreACPSink*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return ++_references;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return --_references;
    }
    HRESULT STDMETHODCALLTYPE OnTextChange(DWORD, const TS_TEXTCHANGE*) noexcept override
    {
        ++textChanges;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnSelectionChange() noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnLayoutChange(TsLayoutCode, TsViewCookie) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnStatusChange(DWORD) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnAttrsChange(LONG, LONG, ULONG, const TS_ATTRID*) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnLockGranted(DWORD) noexcept override
    {
        return onLock ? onLock() : S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnStartEditTransaction() noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnEndEditTransaction() noexcept override
    {
        return S_OK;
    }

private:
    std::atomic<ULONG> _references{1u};
};

// The composition's range: only its ACP extent is read by the store.
class Range final : public ITfRangeACP
{
public:
    LONG start = 0, length = 0;
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
        return ++_references;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return --_references;
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

private:
    std::atomic<ULONG> _references{1u};
};

class Composition final : public ITfCompositionView
{
public:
    Range range;
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
        return ++_references;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return --_references;
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

private:
    std::atomic<ULONG> _references{1u};
};
} // namespace TsfDoubles
#pragma warning(pop)
