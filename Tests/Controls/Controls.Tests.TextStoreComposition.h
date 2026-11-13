#pragma once

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls/DxUi.Internal.h"
#include "Controls/TextStoreTarget.h"

#include <array>
#include <atomic>
#include <functional>
#include <inputscope.h>
#include <msctf.h>
#include <textstor.h>

// Range and composition doubles for the production COM store with an application adapter.
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

// TSF conformance cases. They drive the production COM store through a deterministic in-process target and sink, or a
// headless host, so they take no focus and run in the non-activating TextField suite.
namespace TextStoreConformanceTests
{
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class Sink final : public ITextStoreACPSink
{
public:
    std::function<HRESULT(DWORD)> onLockGranted;
    uint32_t textChangeCount = 0u;
    TS_TEXTCHANGE lastTextChange{};

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) noexcept override
    {
        if (! object)
            return E_POINTER;
        *object = nullptr;
        if (riid != __uuidof(IUnknown) && riid != __uuidof(ITextStoreACPSink))
            return E_NOINTERFACE;
        *object = static_cast<ITextStoreACPSink*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return _references.fetch_add(1u, std::memory_order_relaxed) + 1u;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return _references.fetch_sub(1u, std::memory_order_acq_rel) - 1u;
    }
    HRESULT STDMETHODCALLTYPE OnTextChange(DWORD, const TS_TEXTCHANGE* change) noexcept override
    {
        ++textChangeCount;
        if (change)
            lastTextChange = *change;
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
    HRESULT STDMETHODCALLTYPE OnLockGranted(DWORD flags) noexcept override
    {
        return onLockGranted ? onLockGranted(flags) : S_OK;
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

// One line of text whose UTF-16 unit i occupies the screen box [left + 10i, left + 10i + 10) x [top, bottom). Its hit
// test returns the nearest code-point edge for any point, inside or outside the view, as DirectWrite does.
class GridTarget final : public DxUi::TextStoreTarget
{
public:
    DxUi::TextInputState state;
    RECT viewport{100, 20, 200, 40};

    void Disconnect() noexcept override
    {
    }
    [[nodiscard]] bool ReadState(DxUi::TextInputState& result) const noexcept override
    {
        try
        {
            result = state;
            return true;
        }
        catch (const std::bad_alloc&)
        {
            DxUi::Failure::FailAllocation();
        }
    }
    [[nodiscard]] bool ApplyState(const DxUi::TextInputState& next, bool) noexcept override
    {
        try
        {
            state = next;
            return true;
        }
        catch (const std::bad_alloc&)
        {
            DxUi::Failure::FailAllocation();
        }
    }
    [[nodiscard]] D2D1_RECT_F ResolveTextViewportBounds() const noexcept override
    {
        return D2D1::RectF(
            static_cast<float>(viewport.left), static_cast<float>(viewport.top), static_cast<float>(viewport.right), static_cast<float>(viewport.bottom));
    }
    [[nodiscard]] HWND GetHwnd() const noexcept override
    {
        return nullptr;
    }
    HRESULT GetAcpFromScreenPoint(const POINT* point, LONG* acp) const noexcept override
    {
        *acp = 0;
        if (viewport.right <= viewport.left)
            return TS_E_NOLAYOUT;
        const double units = (static_cast<double>(point->x) - static_cast<double>(viewport.left)) / 10.0;
        const size_t size  = state.text.size();
        size_t position    = units <= 0.0 ? 0u : (std::min)(static_cast<size_t>(units + 0.5), size);
        if (position > 0u && position < size && DxUi::IsUtf16TrailSurrogate(state.text[position]) && DxUi::IsUtf16LeadSurrogate(state.text[position - 1u]))
            position = units < static_cast<double>(position) ? position - 1u : position + 1u;
        *acp = static_cast<LONG>(position);
        return S_OK;
    }
    HRESULT GetRangeScreenRect(LONG start, LONG end, RECT* rect, BOOL* clipped) const noexcept override
    {
        *rect    = {};
        *clipped = FALSE;
        if (viewport.right <= viewport.left)
            return TS_E_NOLAYOUT;
        if (start < 0 || end < start || static_cast<size_t>(end) > state.text.size())
            return TS_E_INVALIDPOS;
        *rect = RECT{viewport.left + start * 10, viewport.top, viewport.left + end * 10, viewport.bottom};
        return S_OK;
    }
    HRESULT GetScreenRect(RECT* rect) const noexcept override
    {
        *rect = viewport;
        return viewport.right > viewport.left ? S_OK : TS_E_NOLAYOUT;
    }
};
#pragma warning(pop)

struct PointResult
{
    HRESULT hr = E_PENDING;
    LONG acp   = -1;
};

inline PointResult QueryPoint(ITextStoreACP& store, Sink& sink, POINT point, DWORD flags)
{
    PointResult result;
    sink.onLockGranted = [&](DWORD) noexcept
    {
        result.hr = store.GetACPFromPoint(1u, &point, flags, &result.acp);
        return S_OK;
    };
    HRESULT session = E_PENDING;
    RequireSucceeded(store.RequestLock(TS_LF_READ | TS_LF_SYNC, &session), "TSF point query obtains a read lock");
    RequireSucceeded(session, "TSF point query lock callback succeeds");
    sink.onLockGranted = nullptr;
    return result;
}

inline bool IsPoint(const PointResult& result, HRESULT hr, LONG acp) noexcept
{
    return result.hr == hr && result.acp == acp;
}

inline void TestTextStorePointQueriesHonorDocumentedFlags()
{
    using namespace DxUi;
    auto target        = std::make_shared<GridTarget>();
    target->state.text = L"abcdef";
    Sink sink;
    wil::com_ptr_nothrow<ITextStoreACP> store;
    store.attach(CreateTextStore(target));
    Require(store != nullptr, "TSF point fixture creates a store");
    RequireSucceeded(store->AdviseSink(__uuidof(ITextStoreACPSink), &sink, TS_AS_TEXT_CHANGE), "TSF point fixture advises a sink");

    constexpr DWORD roundNearest = GXFPF_ROUND_NEAREST;
    constexpr DWORD nearest      = GXFPF_NEAREST;
    // Inside the box of "b" ([110, 120)), nearer its trailing edge.
    const POINT insideB{117, 30};
    Require(IsPoint(QueryPoint(*store, sink, insideB, 0u), S_OK, 1), "a point inside a character box returns that character by default");
    Require(IsPoint(QueryPoint(*store, sink, insideB, roundNearest), S_OK, 2), "GXFPF_ROUND_NEAREST returns the nearer bounding edge");
    Require(IsPoint(QueryPoint(*store, sink, insideB, nearest), S_OK, 1), "GXFPF_NEAREST keeps the containing character inside a box");
    Require(IsPoint(QueryPoint(*store, sink, insideB, roundNearest | nearest), S_OK, 2), "combined flags round inside a box");
    // Inside the view but after the last character box ([150, 160)).
    const POINT afterText{185, 30};
    Require(IsPoint(QueryPoint(*store, sink, afterText, 0u), TS_E_INVALIDPOINT, 0), "a point outside every character box is TS_E_INVALIDPOINT");
    Require(IsPoint(QueryPoint(*store, sink, afterText, roundNearest), TS_E_INVALIDPOINT, 0),
            "GXFPF_ROUND_NEAREST alone does not resolve a point outside every box");
    Require(IsPoint(QueryPoint(*store, sink, afterText, nearest), S_OK, 6), "GXFPF_NEAREST returns the closest position outside a box");
    // Outside the view entirely.
    const POINT rightOfView{250, 30};
    const POINT aboveView{132, 5};
    Require(IsPoint(QueryPoint(*store, sink, rightOfView, 0u), TS_E_INVALIDPOINT, 0), "an out-of-view point is TS_E_INVALIDPOINT, not an invalid position");
    Require(IsPoint(QueryPoint(*store, sink, aboveView, roundNearest), TS_E_INVALIDPOINT, 0), "an out-of-view point is outside every box when rounding");
    Require(IsPoint(QueryPoint(*store, sink, rightOfView, nearest), S_OK, 6), "GXFPF_NEAREST resolves an out-of-view point to the closest position");
    Require(IsPoint(QueryPoint(*store, sink, aboveView, nearest), S_OK, 3), "GXFPF_NEAREST resolves a point above the view by its column");

    // A surrogate pair is one character box ([110, 130)); no result falls between its code units.
    target->state.text = L"a\U0001F600b";
    const POINT insidePair{123, 30};
    Require(IsPoint(QueryPoint(*store, sink, insidePair, 0u), S_OK, 1), "a point in a surrogate pair returns the pair's start");
    Require(IsPoint(QueryPoint(*store, sink, insidePair, roundNearest), S_OK, 3), "rounding inside a surrogate pair returns its nearer edge");

    target->viewport = RECT{100, 20, 100, 40};
    const std::array<DWORD, 4u> allFlags{0u, roundNearest, nearest, roundNearest | nearest};
    for (const DWORD flags : allFlags)
        Require(IsPoint(QueryPoint(*store, sink, insideB, flags), TS_E_NOLAYOUT, 0), "every flag reports TS_E_NOLAYOUT before layout");
    RequireSucceeded(store->UnadviseSink(&sink), "TSF point fixture unadvises its sink");
}

inline InputScope QueryInputScope(ITextStoreACP& store, const char* message)
{
    wil::com_ptr_nothrow<ITfInputScope> scope;
    RequireSucceeded(store.QueryInterface(IID_PPV_ARGS(scope.put())), message);
    wil::unique_cotaskmem_ptr<InputScope> values;
    UINT count = 0u;
    RequireSucceeded(scope->GetInputScopes(wil::out_param(values), &count), message);
    Require(count == 1u && values, message);
    return values.get()[0];
}

// TSF reads GUID_PROP_INPUTSCOPE as an ITfInputScope through the attribute request methods.
inline InputScope RetrieveInputScopeAttribute(ITextStoreACP& store, bool atPosition, const char* message)
{
    const TS_ATTRID requested = DxUi::kTextStoreInputScopeProperty;
    if (atPosition)
        RequireSucceeded(store.RequestAttrsAtPosition(0, 1u, &requested, 0u), message);
    else
        RequireSucceeded(store.RequestSupportedAttrs(TS_ATTR_FIND_WANT_VALUE, 1u, &requested), message);
    std::array<TS_ATTRVAL, 2u> values{};
    ULONG fetched = 0u;
    RequireSucceeded(store.RetrieveRequestedAttrs(static_cast<ULONG>(values.size()), values.data(), &fetched), message);
    Require(fetched == 1u && values[0].idAttr == DxUi::kTextStoreInputScopeProperty && values[0].varValue.vt == VT_UNKNOWN && values[0].varValue.punkVal,
            message);
    wil::com_ptr_nothrow<ITfInputScope> scope;
    const HRESULT queried = values[0].varValue.punkVal->QueryInterface(IID_PPV_ARGS(scope.put()));
    static_cast<void>(VariantClear(&values[0].varValue));
    RequireSucceeded(queried, message);
    wil::unique_cotaskmem_ptr<InputScope> scopes;
    UINT count = 0u;
    RequireSucceeded(scope->GetInputScopes(wil::out_param(scopes), &count), message);
    Require(count == 1u && scopes, message);
    return scopes.get()[0];
}

inline void TestTextStoreReportsFieldInputScopes()
{
    using namespace DxUi;
    ControlHost host;
    auto root            = std::make_unique<Panel>();
    auto* const url      = root->AddChild<TextField>(L"https://example.invalid/");
    auto* const stepper  = root->AddChild<NumericStepper>();
    auto* const plain    = root->AddChild<TextField>(L"plain");
    TextField* const num = &stepper->Field();
    url->SetInputScope(TextInputScope::Url);
    Require(url->GetInputScope() == TextInputScope::Url && num->GetInputScope() == TextInputScope::Number && plain->GetInputScope() == TextInputScope::Default,
            "a URL field and a NumericStepper field declare their input scopes");
    host.SetRoot(std::move(root));

    const auto observe = [&](TextField& field, InputScope expected, const char* message)
    {
        host.SetFocusControl(&field, false);
        wil::com_ptr_nothrow<ITextStoreACP> store;
        store.attach(CreateNativeTextInputTextStore(host, field, false));
        Require(store != nullptr, message);
        Require(QueryInputScope(*store, message) == expected, message);
        Require(RetrieveInputScopeAttribute(*store, false, message) == expected, message);
        Require(RetrieveInputScopeAttribute(*store, true, message) == expected, message);

        const TS_ATTRID requested = DxUi::kTextStoreInputScopeProperty;
        RequireSucceeded(store->RequestSupportedAttrs(0u, 1u, &requested), message);
        TS_ATTRVAL value{};
        ULONG fetched = 0u;
        RequireSucceeded(store->RetrieveRequestedAttrs(1u, &value, &fetched), message);
        Require(fetched == 1u && value.idAttr == DxUi::kTextStoreInputScopeProperty && value.varValue.vt == VT_EMPTY,
                "a support query without TS_ATTR_FIND_WANT_VALUE reports the attribute with an empty value");
        const TS_ATTRID other = GUID_PROP_ATTRIBUTE;
        RequireSucceeded(store->RequestSupportedAttrs(TS_ATTR_FIND_WANT_VALUE, 1u, &other), message);
        RequireSucceeded(store->RetrieveRequestedAttrs(1u, &value, &fetched), message);
        Require(fetched == 0u, "an unsupported attribute is not reported");
    };
    observe(*url, IS_URL, "a URL field reports IS_URL to TSF");
    observe(*num, IS_NUMBER, "a NumericStepper field reports IS_NUMBER to TSF");
    observe(*plain, IS_DEFAULT, "a plain field reports IS_DEFAULT to TSF");
}

inline bool IsChange(const TS_TEXTCHANGE& change, LONG start, LONG oldEnd, LONG newEnd) noexcept
{
    return change.acpStart == start && change.acpOldEnd == oldEnd && change.acpNewEnd == newEnd;
}

inline void TestTextStoreExternalChangeExcludesCommonPrefixAndSuffix()
{
    using namespace DxUi;
    auto target        = std::make_shared<GridTarget>();
    target->state.text = L"abcdef";
    Sink sink;
    wil::com_ptr_nothrow<ITextStoreACP> store;
    store.attach(CreateTextStore(target));
    Require(store != nullptr, "TSF change fixture creates a store");
    RequireSucceeded(store->AdviseSink(__uuidof(ITextStoreACPSink), &sink, TS_AS_TEXT_CHANGE), "TSF change fixture advises a sink");
    target->state.text = L"abXYef";
    NotifyTextStoreChanged(store.get());
    Require(sink.textChangeCount == 1u && IsChange(sink.lastTextChange, 2, 4, 4), "a middle edit of abcdef to abXYef notifies {2, 4, 4}");
    RequireSucceeded(store->UnadviseSink(&sink), "TSF change fixture unadvises its sink");

    Require(IsChange(ComputeTextStoreTextChange(L"abcdef", L"abXYef"), 2, 4, 4), "the change excludes the common prefix and suffix");
    Require(IsChange(ComputeTextStoreTextChange(L"abc", L"abXc"), 2, 2, 3), "an insertion is an empty old range");
    Require(IsChange(ComputeTextStoreTextChange(L"abcd", L"ad"), 1, 3, 1), "a deletion is an empty new range");
    Require(IsChange(ComputeTextStoreTextChange(L"aaa", L"aaaa"), 3, 3, 4), "the suffix never overlaps the prefix");
    Require(IsChange(ComputeTextStoreTextChange(L"", L"xy"), 0, 0, 2), "inserting into empty text replaces nothing");
    // U+1F600 and U+1F601 share their lead surrogate; the prefix cannot end between the units of either pair.
    Require(IsChange(ComputeTextStoreTextChange(L"x\U0001F600y", L"x\U0001F601y"), 1, 3, 3), "the prefix never splits a surrogate pair");
    // U+1F600 and U+1F200 share their trail surrogate; the suffix cannot start between the units of either pair.
    Require(IsChange(ComputeTextStoreTextChange(L"a\U0001F600", L"b\U0001F200"), 0, 3, 3), "the suffix never splits a surrogate pair");
    Require(IsChange(ComputeTextStoreTextChange(L"ab\r\ncd", L"ab\rcd"), 2, 4, 3), "the prefix never splits a CRLF pair");
    Require(IsChange(ComputeTextStoreTextChange(L"a\r\nb", L"x\nb"), 0, 3, 2), "the suffix never splits a CRLF pair");
}

inline void TestTextStoreDisplayAttributesResolveClausesAndTarget()
{
    using namespace DxUi;
    TextStoreCompositionAttributes result;
    const std::array<TextStoreAttributeSpan, 3u> clauses{{
        {10u, 13u, TF_ATTR_CONVERTED},
        {13u, 16u, TF_ATTR_TARGET_CONVERTED},
        {16u, 19u, TF_ATTR_CONVERTED},
    }};
    ResolveTextStoreCompositionAttributes(10u, 19u, clauses, result);
    Require(result.targetStart == 13u && result.targetEnd == 16u, "three clauses resolve to one conversion target range");
    Require(result.clauseBoundaries == std::vector<size_t>{10u, 13u, 16u, 19u},
            "three clauses resolve to their boundaries in the IMM convention: the start, two interior edges and the end");

    const std::array<TextStoreAttributeSpan, 4u> mixed{{
        {4u, 6u, TF_ATTR_INPUT},
        {6u, 8u, TF_ATTR_TARGET_NOTCONVERTED},
        {8u, 9u, TF_ATTR_TARGET_CONVERTED},
        {12u, 20u, TF_ATTR_TARGET_CONVERTED},
    }};
    ResolveTextStoreCompositionAttributes(5u, 14u, mixed, result);
    Require(result.targetStart == 6u && result.targetEnd == 9u, "the target is the first contiguous run of target spans");
    Require(result.clauseBoundaries == std::vector<size_t>{5u, 6u, 8u, 9u, 12u, 14u}, "spans are clipped to the composition");

    const std::array<TextStoreAttributeSpan, 3u> invalid{{
        {7u, 7u, TF_ATTR_TARGET_CONVERTED},
        {9u, 8u, TF_ATTR_TARGET_CONVERTED},
        {30u, 40u, TF_ATTR_TARGET_CONVERTED},
    }};
    ResolveTextStoreCompositionAttributes(5u, 14u, invalid, result);
    Require(! result.targetStart && ! result.targetEnd && result.clauseBoundaries.empty(), "empty, reversed and outside spans publish nothing");

    std::vector<TextStoreAttributeSpan> many;
    for (size_t index = 0u; index < 400u; ++index)
        many.push_back({index, index + 1u, TF_ATTR_CONVERTED});
    ResolveTextStoreCompositionAttributes(0u, 400u, many, result);
    Require(result.clauseBoundaries.size() == kMaximumTextStoreClauseBoundaries && result.clauseBoundaries.back() == 400u,
            "clause boundaries are bounded and always end at the composition end");
}
} // namespace TextStoreConformanceTests
