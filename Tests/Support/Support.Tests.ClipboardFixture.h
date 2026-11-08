#pragma once
#include "Controls/TextClipboard.h"
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
    wil::unique_hglobal htmlData;
    wil::unique_hglobal dibData, dibV5Data;
    bool busy                  = false;
    bool failAllocation        = false;
    bool failEmpty             = false;
    bool failPublish           = false;
    bool failHtmlPublish       = false;
    size_t opens               = 0;
    size_t closes              = 0;
    size_t allocations         = 0;
    size_t publications        = 0;
    size_t peakGlobalBytes     = 0;
    size_t lastAllocationBytes = 0;
    size_t unicodeReads = 0, dibReads = 0, dibV5Reads = 0;

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
        if (format == CF_UNICODETEXT)
            ++_current->unicodeReads;
        else if (format == CF_DIB)
            ++_current->dibReads;
        else if (format == CF_DIBV5)
            ++_current->dibV5Reads;
        const auto htmlFormat = RegisterClipboardFormatW(L"HTML Format");
        _current->check(format == CF_UNICODETEXT || format == CF_HDROP || format == htmlFormat || format == CF_DIB || format == CF_DIBV5,
                        "transport reads supported clipboard formats");
        return format == CF_HDROP   ? _current->fileData.get()
               : format == CF_DIB   ? _current->dibData.get()
               : format == CF_DIBV5 ? _current->dibV5Data.get()
                                    : (format == htmlFormat ? _current->htmlData.get() : _current->data.get());
    }
    static BOOL WINAPI Empty()
    {
        if (_current->failEmpty)
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
        _current->data.reset();
        _current->fileData.reset();
        _current->htmlData.reset();
        return TRUE;
    }
    static HANDLE WINAPI SetData(UINT format, HANDLE value)
    {
        const auto html = RegisterClipboardFormatW(L"HTML Format");
        _current->check(format == CF_UNICODETEXT || format == html, "transport publishes supported clipboard formats");
        if (_current->failPublish || (format == html && _current->failHtmlPublish))
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return nullptr;
        }
        (format == html ? _current->htmlData : _current->data).reset(static_cast<HGLOBAL>(value));
        ++_current->publications;
        return value;
    }
    static HGLOBAL WINAPI Allocate(UINT flags, SIZE_T bytes)
    {
        auto& state = *_current;
        ++state.allocations;
        state.lastAllocationBytes = bytes;
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
