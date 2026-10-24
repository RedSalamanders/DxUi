#pragma once
#include "../../src/Controls/TextClipboard.h"
// Exercise the real clipboard codec/transport using private HGLOBAL storage. Only
// ownership calls are replaced, so no test opens or mutates the desktop clipboard.
class ClipboardTransportFixture final
{
public:
    explicit ClipboardTransportFixture(HWND owner, void (*check)(bool, const char*)) noexcept
        : owner(owner),
          check(check),
          _previousFixture(_current),
          _previousCalls(DxUi::testClipboardSystemCalls),
          _previousClipboard(DxUi::testTextClipboard)
    {
        _calls.open                    = &Open;
        _calls.close                   = &Close;
        _calls.getData                 = &GetData;
        _calls.empty                   = &Empty;
        _calls.setData                 = &SetData;
        _calls.allocate                = &Allocate;
        _current                       = this;
        DxUi::testClipboardSystemCalls = &_calls;
        DxUi::testTextClipboard        = &DxUi::SystemTextClipboard();
    }
    ~ClipboardTransportFixture()
    {
        DxUi::testTextClipboard        = _previousClipboard;
        DxUi::testClipboardSystemCalls = _previousCalls;
        _current                       = _previousFixture;
    }
    ClipboardTransportFixture(const ClipboardTransportFixture&)            = delete;
    ClipboardTransportFixture& operator=(const ClipboardTransportFixture&) = delete;

    HWND owner;
    void (*check)(bool, const char*);
    wil::unique_hglobal data;
    wil::unique_hglobal fileData;
    bool busy              = false;
    bool failAllocation    = false;
    bool failEmpty         = false;
    bool failPublish       = false;
    size_t opens           = 0;
    size_t closes          = 0;
    size_t allocations     = 0;
    size_t publications    = 0;
    size_t peakGlobalBytes = 0;

private:
    static BOOL WINAPI Open(HWND window)
    {
        auto& state = *_current;
        ++state.opens;
        if (state.busy || window != state.owner)
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
        return TRUE;
    }
    static BOOL WINAPI Close()
    {
        ++_current->closes;
        return TRUE;
    }
    static HANDLE WINAPI GetData(UINT format)
    {
        _current->check(format == CF_UNICODETEXT || format == CF_HDROP, "transport reads Unicode text or wide file lists");
        return format == CF_HDROP ? _current->fileData.get() : _current->data.get();
    }
    static BOOL WINAPI Empty()
    {
        if (_current->failEmpty)
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
        _current->data.reset();
        return TRUE;
    }
    static HANDLE WINAPI SetData(UINT format, HANDLE value)
    {
        _current->check(format == CF_UNICODETEXT, "transport publishes Unicode clipboard data");
        if (_current->failPublish)
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return nullptr;
        }
        _current->data.reset(static_cast<HGLOBAL>(value));
        ++_current->publications;
        return value;
    }
    static HGLOBAL WINAPI Allocate(UINT flags, SIZE_T bytes)
    {
        auto& state = *_current;
        ++state.allocations;
        if (state.failAllocation)
            return nullptr;
        HGLOBAL memory = GlobalAlloc(flags, bytes);
        if (memory)
        {
            const size_t retained = state.data ? GlobalSize(state.data.get()) : 0;
            state.peakGlobalBytes = (std::max)(state.peakGlobalBytes, retained + GlobalSize(memory));
        }
        return memory;
    }
    DxUi::ClipboardSystemCalls _calls;
    ClipboardTransportFixture* _previousFixture;
    const DxUi::ClipboardSystemCalls* _previousCalls;
    DxUi::TextClipboard* _previousClipboard;
    inline static thread_local ClipboardTransportFixture* _current = nullptr;
};
